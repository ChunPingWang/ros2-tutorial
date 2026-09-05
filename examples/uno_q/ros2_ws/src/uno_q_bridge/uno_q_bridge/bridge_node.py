"""
UNO Q ROS 2 橋接節點。

  訂閱 /uno_q/led      (std_msgs/Bool)  -> MCU set_led
  發布 /uno_q/analog0  (std_msgs/Int32) <- MCU read_analog(0)，預設 10 Hz
"""
import os

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Int32

from uno_q_bridge.bridge_client import BridgeClient


class UnoQBridgeNode(Node):
    def __init__(self):
        super().__init__('uno_q_bridge')

        self.declare_parameter('bridge_host', os.environ.get('UNO_Q_BRIDGE_HOST', '127.0.0.1'))
        self.declare_parameter('bridge_port', int(os.environ.get('UNO_Q_BRIDGE_PORT', '5555')))
        self.declare_parameter('analog_pin', 0)
        self.declare_parameter('publish_rate_hz', 10.0)

        host = self.get_parameter('bridge_host').value
        port = self.get_parameter('bridge_port').value
        self._analog_pin = self.get_parameter('analog_pin').value
        rate = self.get_parameter('publish_rate_hz').value

        self._client = BridgeClient(host, port)

        self._led_sub = self.create_subscription(Bool, '/uno_q/led', self._on_led, 10)
        self._analog_pub = self.create_publisher(Int32, '/uno_q/analog0', 10)

        self._poll_timer = self.create_timer(1.0 / rate, self._poll_analog)
        self._reconnect_timer = self.create_timer(1.0, self._ensure_connected)

        self.get_logger().info(f'bridge target {host}:{port}, analog pin {self._analog_pin} @ {rate} Hz')
        self._ensure_connected()

    # ---- 連線管理 -----------------------------------------------------
    def _ensure_connected(self):
        if self._client.connected:
            return
        try:
            self._client.connect()
            self.get_logger().info('connected to bridge')
        except OSError as exc:
            self.get_logger().warn(f'bridge unavailable, retrying: {exc}', throttle_duration_sec=5.0)

    # ---- ROS callbacks -----------------------------------------------
    def _on_led(self, msg: Bool):
        try:
            self._client.call('set_led', bool(msg.data))
        except (ConnectionError, RuntimeError) as exc:
            self.get_logger().error(f'set_led failed: {exc}')

    def _poll_analog(self):
        if not self._client.connected:
            return
        try:
            value = self._client.call('read_analog', self._analog_pin)
        except (ConnectionError, RuntimeError) as exc:
            self.get_logger().error(f'read_analog failed: {exc}', throttle_duration_sec=5.0)
            return
        out = Int32()
        out.data = int(value)
        self._analog_pub.publish(out)

    def destroy_node(self):
        self._client.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = UnoQBridgeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
