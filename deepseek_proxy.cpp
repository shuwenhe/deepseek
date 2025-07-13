#include <crow.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

// Curl 写数据回调
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = (std::string*)userp;
    s->append((char*)contents, total);
    return total;
}

// 调用本地 Ollama API 获取响应文本
std::string call_ollama_api(const std::string& prompt) {
    CURL* curl = curl_easy_init();
    if (!curl) return "Curl init failed";

    std::string readBuffer;

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    nlohmann::json post_json = {
        {"model", "deepseek-r1:latest"},
        {"prompt", prompt}
    };
    std::string post_data = post_json.dump();

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.size());

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Curl request failed: " + std::string(curl_easy_strerror(res));
    }

    // 解析多行JSON流，拼接"response"字段
    std::istringstream stream(readBuffer);
    std::string line;
    std::string full_response;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            if (j.contains("response")) {
                full_response += j["response"].get<std::string>();
            }
            if (j.value("done", false) == true) {
                break; // 完成
            }
        } catch (...) {
            // 忽略解析错误
        }
    }
    return full_response.empty() ? "No response from model." : full_response;
}

int main() {
    crow::SimpleApp app;

    // GET /page 返回HTML
    CROW_ROUTE(app, "/page").methods(crow::HTTPMethod::GET)([]() {
        std::ostringstream html;
        html <<
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>DeepSeek Proxy</title></head>\n"
            "<body>\n"
            "  <h2>DeepSeek Proxy - Input prompt</h2>\n"
            "  <textarea id=\"prompt\" rows=\"8\" cols=\"60\" placeholder=\"Enter prompt here...\"></textarea><br/>\n"
            "  <button onclick=\"sendPrompt()\">Send</button>\n"
            "  <h3>Response:</h3>\n"
            "  <pre id=\"response\"></pre>\n"
            "  <script>\n"
            "    async function sendPrompt() {\n"
            "      const prompt = document.getElementById('prompt').value;\n"
            "      if (!prompt) {\n"
            "        alert('Please enter a prompt!');\n"
            "        return;\n"
            "      }\n"
            "      const resElem = document.getElementById('response');\n"
            "      resElem.textContent = 'Waiting for response...';\n"
            "      try {\n"
            "        const resp = await fetch('/page', {\n"
            "          method: 'POST',\n"
            "          headers: {'Content-Type': 'application/json'},\n"
            "          body: JSON.stringify({prompt: prompt})\n"
            "        });\n"
            "        if (!resp.ok) {\n"
            "          const text = await resp.text();\n"
            "          resElem.textContent = 'Error: ' + text;\n"
            "          return;\n"
            "        }\n"
            "        const json = await resp.json();\n"
            "        resElem.textContent = json.response || 'No response';\n"
            "      } catch (e) {\n"
            "        resElem.textContent = 'Fetch error: ' + e.message;\n"
            "      }\n"
            "    }\n"
            "  </script>\n"
            "</body>\n"
            "</html>\n";
        return crow::response(html.str());
    });

    // POST /page 处理请求，返回JSON
    CROW_ROUTE(app, "/page").methods(crow::HTTPMethod::POST)(
        [](const crow::request& req) -> crow::response {
            auto body = crow::json::load(req.body);
            if (!body) {
                return crow::response(400, "Invalid JSON");
            }
            if (!body.has("prompt") || body["prompt"].s() == "") {
                return crow::response(400, "No prompt provided.");
            }
            std::string prompt = body["prompt"].s();

            std::string result = call_ollama_api(prompt);

            nlohmann::json res_json;
            res_json["response"] = result;

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = res_json.dump();

            return res;
        });

    app.port(8080).multithreaded().run();

    return 0;
}

