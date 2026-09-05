"""啟動 board_monitor，並允許用 launch 參數覆寫 STM32 逾時秒數。"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'stm32_timeout_sec', default_value='1.0',
            description='heartbeat 超過此秒數未更新即視為 STM32 離線'),
        DeclareLaunchArgument(
            'publish_rate_hz', default_value='1.0',
            description='/boards/status 的發布頻率'),
        Node(
            package='board_monitor',
            executable='board_monitor_node',
            name='board_monitor',
            output='screen',
            parameters=[{
                'stm32_timeout_sec': LaunchConfiguration('stm32_timeout_sec'),
                'publish_rate_hz': LaunchConfiguration('publish_rate_hz'),
            }],
        ),
    ])
