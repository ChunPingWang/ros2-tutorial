# ROS 2 Jazzy 容器，供 Arduino UNO Q（arm64，Debian）使用
# build:  docker build -t uno-q-ros2 -f docker/Dockerfile .
FROM ros:jazzy-ros-base

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ros2_ws
COPY ros2_ws/src ./src

RUN source /opt/ros/jazzy/setup.bash \
    && colcon build --symlink-install

COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENV ROS_DOMAIN_ID=42
ENV UNO_Q_BRIDGE_HOST=127.0.0.1
ENV UNO_Q_BRIDGE_PORT=5555

ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "run", "uno_q_bridge", "bridge_node"]
