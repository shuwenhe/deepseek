#include <crow.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

int main() {
    crow::SimpleApp app;

    // 根路径处理 POST 请求
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            nlohmann::json request_body = {
                {"model", body.value("model", "deepseek-coder:latest")},
                {"prompt", body.value("prompt", "你好")},
                {"stream", false}
            };

            auto response = cpr::Post(
                cpr::Url{"http://localhost:11434/api/generate"},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{request_body.dump()}
            );

            if (response.status_code == 200) {
                auto reply = nlohmann::json::parse(response.text);
                return crow::response(200, reply["response"].get<std::string>());
            } else {
                return crow::response(response.status_code, "DeepSeek 请求失败: " + response.text);
            }
        } catch (const std::exception& e) {
            return crow::response(500, std::string("服务异常: ") + e.what());
        }
    });

    std::cout << "🚀 监听地址: http://localhost:8080" << std::endl;
    app.port(8080).multithreaded().run();
}

