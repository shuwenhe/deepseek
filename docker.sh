#!/bin/bash
set -e

export DOCKER_BUILDKIT=1  # ✅ 开启 BuildKit

IMAGE_NAME="deepseek-proxy"
PORT=8080

echo "🐳 构建 Docker 镜像: $IMAGE_NAME ..."
docker build -t $IMAGE_NAME .

echo "🚀 启动容器，映射端口 $PORT ..."
docker run -it --rm -p $PORT:$PORT --name $IMAGE_NAME-container $IMAGE_NAME

