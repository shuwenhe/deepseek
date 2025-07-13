#!/bin/bash

SERVICE_NAME=deepseek
EXEC_PATH="/home/shuwen1/deepseek/deepseek"
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"
USER_NAME="shuwen1"

# 检查执行文件是否存在
if [ ! -f "$EXEC_PATH" ]; then
    echo "[ERROR] 未找到可执行文件：$EXEC_PATH"
    exit 1
fi

# 写入 systemd 服务文件
echo "[INFO] 创建 systemd 服务文件：$SERVICE_FILE"

sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=DeepSeek Proxy Service
After=network.target

[Service]
Type=simple
User=$USER_NAME
WorkingDirectory=/home/$USER_NAME/deepseek
ExecStart=$EXEC_PATH
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

# 重新加载 systemd 并启用服务
echo "[INFO] 重新加载 systemd..."
sudo systemctl daemon-reexec
sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl restart "$SERVICE_NAME"

echo "[INFO] 服务状态："
sudo systemctl status "$SERVICE_NAME" --no-pager

