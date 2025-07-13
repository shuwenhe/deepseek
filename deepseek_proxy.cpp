#include <crow.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <atomic>
#include <thread>
#include <mutex>

// 用于存储响应数据的缓冲区和互斥锁
struct ResponseContext {
    std::mutex mutex;
    std::string buffer;
    bool completed = false;
};

// Curl 写数据回调 - 直接处理JSON流
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    ResponseContext* ctx = static_cast<ResponseContext*>(userp);
    
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->buffer.append(static_cast<char*>(contents), total);
    
    return total;
}

// 处理接收到的JSON数据并发送到客户端
void processJsonStream(ResponseContext* ctx, crow::response& res) {
    std::string remaining;
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            
            // 查找完整的JSON行
            size_t pos = 0;
            while ((pos = ctx->buffer.find('\n', pos)) != std::string::npos) {
                std::string line = ctx->buffer.substr(0, pos);
                ctx->buffer.erase(0, pos + 1);
                pos = 0;
                
                if (!line.empty()) {
                    try {
                        auto j = nlohmann::json::parse(line);
                        if (j.contains("response")) {
                            std::string response = j["response"].get<std::string>();
                            nlohmann::json sseData;
                            sseData["response"] = response;
                            
                            std::string sseMessage = "data: " + sseData.dump() + "\n\n";
                            res.write(sseMessage);
                            res.end(); // 刷新输出
                        }
                        
                        if (j.contains("done") && j["done"].get<bool>()) {
                            res.write("data: [DONE]\n\n");
                            res.end();
                            return;
                        }
                    } catch (...) {
                        // 忽略解析错误
                    }
                }
            }
        }
        
        // 如果请求已完成，检查是否还有剩余数据
        if (ctx->completed) {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if (ctx->buffer.empty()) {
                res.write("data: [DONE]\n\n");
                res.end();
                return;
            }
        }
        
        // 短暂休眠避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 调用本地 Ollama API 获取响应文本 - 改为返回流
void call_ollama_api(const std::string& prompt, ResponseContext* ctx) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        ctx->buffer = "Curl init failed";
        ctx->completed = true;
        return;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Expect:");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 设置超时时间为5分钟

    nlohmann::json post_json = {
        {"model", "deepseek-r1:latest"},
        {"prompt", prompt},
        {"stream", true}
    };
    std::string post_data = post_json.dump();

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.size());

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->buffer += "Curl request failed: " + std::string(curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    // 标记请求已完成
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->completed = true;
    }
}

int main() {
    crow::SimpleApp app;

    // GET /page 返回HTML
    CROW_ROUTE(app, "/page").methods(crow::HTTPMethod::GET)([]() {
        // 使用原始字符串字面量正确包含HTML和JavaScript
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>DeepSeek Proxy</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
        textarea { width: 100%; height: 150px; margin-bottom: 10px; }
        #response { background-color: #f5f5f5; padding: 10px; min-height: 100px; white-space: pre-wrap; }
    </style>
</head>
<body>
    <h2>DeepSeek Proxy - Input prompt</h2>
    <textarea id="prompt" rows="8" cols="60" placeholder="Enter prompt here..."></textarea><br/>
    <button onclick="sendPrompt()">Send</button>
    <h3>Response:</h3>
    <div id="response"></div>
    
    <script>
        let eventSource = null;
        
        async function sendPrompt() {
            const prompt = document.getElementById('prompt').value;
            if (!prompt) {
                alert('Please enter a prompt!');
                return;
            }
            
            const resElem = document.getElementById('response');
            resElem.textContent = 'Waiting for response...';
            
            // 关闭之前的连接
            if (eventSource) {
                eventSource.close();
            }
            
            // 创建EventSource连接
            eventSource = new EventSource('/stream?prompt=' + encodeURIComponent(prompt));
            
            // 处理接收到的数据
            eventSource.onmessage = function(e) {
                if (e.data === '[DONE]') {
                    eventSource.close();
                    return;
                }
                
                try {
                    const data = JSON.parse(e.data);
                    if (data.response) {
                        // 将新的响应追加到显示区域
                        resElem.textContent += data.response;
                        // 滚动到底部
                        resElem.scrollTop = resElem.scrollHeight;
                    }
                } catch (err) {
                    console.error('Error parsing SSE data:', err);
                }
            };
            
            // 处理错误
            eventSource.onerror = function(err) {
                console.error('EventSource failed:', err);
                resElem.textContent += '\n\n[Connection Error]';
                eventSource.close();
            };
        }
        
        // 页面卸载时关闭连接
        window.addEventListener('beforeunload', function() {
            if (eventSource) {
                eventSource.close();
            }
        });
    </script>
</body>
</html>
        )";
        
        return crow::response(html);
    });

    // GET /stream 处理流式请求
    CROW_ROUTE(app, "/stream").methods(crow::HTTPMethod::GET)([](const crow::request& req) {
        auto prompt = req.url_params.get("prompt");
        if (!prompt || !*prompt) {
            return crow::response(400, "No prompt provided.");
        }
        
        // 创建响应上下文
        auto ctx = std::make_shared<ResponseContext>();
        
        // 创建响应对象
        crow::response res;
        
        // 设置SSE头
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");
        
        // 在新线程中调用Ollama API
        std::thread([ctx, prompt = *prompt]() {
            call_ollama_api(prompt, ctx.get());
        }).detach();
        
        // 在当前线程中处理响应流
        processJsonStream(ctx.get(), res);
        
        return res;
    });

    app.port(8080).multithreaded().run();

    return 0;
}
