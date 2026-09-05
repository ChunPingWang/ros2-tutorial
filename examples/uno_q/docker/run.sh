#!/bin/bash
# 在 UNO Q 的 Linux 側啟動 ROS 2 容器
set -e
docker run -d --name uno-q-ros2 --restart unless-stopped \
  --network host \
  -e ROS_DOMAIN_ID=42 \
  -e UNO_Q_BRIDGE_HOST=127.0.0.1 \
  -e UNO_Q_BRIDGE_PORT=5555 \
  uno-q-ros2
docker logs -f uno-q-ros2
