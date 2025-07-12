# 使用 Ubuntu + 构建工具
FROM ubuntu:24.04

# 安装构建依赖
RUN apt update && apt install -y \
    git cmake g++ libssl-dev curl libasio-dev \
    libcpr-dev nlohmann-json3-dev

# 设置工作目录
WORKDIR /app

# 拷贝代码
COPY . .

# 构建
RUN mkdir build && cd build && cmake .. && make

# 运行应用
CMD ["./build/deepseek_proxy"]

