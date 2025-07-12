#include <crow.h>
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    crow::SimpleApp app;

    // POST / 路由，接收 JSON 请求
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        std::cout << "请求体: " << req.body << std::endl;

        try {
            auto j = nlohmann::json::parse(req.body);
            std::string prompt = j.value("prompt", "");
            if (prompt.empty()) {
                return crow::response(400, "Missing prompt");
            }

            // 模拟调用模型返回结果
            nlohmann::json res_json;
            res_json["response"] = "模拟返回结果，收到 prompt: " + prompt;

            crow::response res;
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.body = res_json.dump();
            return res;
        } catch (const std::exception& e) {
            return crow::response(400, std::string("Invalid JSON: ") + e.what());
        }
    });

    app.port(8080).multithreaded().run();
}

