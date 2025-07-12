#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

int main() {
    // 构造请求 JSON 数据
    nlohmann::json request_body = {
        {"model", "deepseek-coder:latest"},   // 或其他你本地加载的模型名
        {"prompt", "用C++写一个冒泡排序"},
        {"stream", false}
    };

    // 发送 POST 请求到本地 DeepSeek 接口
    cpr::Response response = cpr::Post(
        cpr::Url{"http://localhost:11434/api/generate"},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{request_body.dump()}
    );

    // 检查返回状态
    if (response.status_code == 200) {
        // 解析返回 JSON
        auto result = nlohmann::json::parse(response.text);
        std::cout << "DeepSeek 回复:\n" << result["response"] << std::endl;
    } else {
        std::cerr << "请求失败，状态码: " << response.status_code << std::endl;
        std::cerr << response.text << std::endl;
    }

    return 0;
}

