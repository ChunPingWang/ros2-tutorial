"""
Raspberry Pi GPIO 節點。

  訂閱 /pi/led     (std_msgs/Bool) -> GPIO17 LED
  發布 /pi/button  (std_msgs/Bool) <- GPIO27 按鈕（內建上拉，按下為 true）

使用 gpiozero + lgpio 後端，Pi 4 與 Pi 5 皆可在 Ubuntu 24.04 上執行。
"""
import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool

try:
    from gpiozero import LED, Button
    from gpiozero.pins.lgpio import LGPIOFactory
    _GPIO_AVAILABLE = True
except ImportError:
    _GPIO_AVAILABLE = False


class PiGpioNode(Node):
    def __init__(self):
        super().__init__('pi_gpio')

        self.declare_parameter('led_pin', 17)
        self.declare_parameter('button_pin', 27)
        self.declare_parameter('button_republish_hz', 1.0)

        led_pin = self.get_parameter('led_pin').value
        button_pin = self.get_parameter('button_pin').value
        republish_hz = self.get_parameter('button_republish_hz').value

        if _GPIO_AVAILABLE:
            factory = LGPIOFactory()
            self._led = LED(led_pin, pin_factory=factory)
            self._button = Button(button_pin, pull_up=True, bounce_time=0.02, pin_factory=factory)
            self._button.when_pressed = lambda: self._publish_button(True)
            self._button.when_released = lambda: self._publish_button(False)
            self.get_logger().info(f'GPIO ready: LED={led_pin}, BUTTON={button_pin}')
        else:
            self._led = None
            self._button = None
            self.get_logger().warn('gpiozero not available, running in simulation mode')

        self._button_pub = self.create_publisher(Bool, '/pi/button', 10)
        self._led_sub = self.create_subscription(Bool, '/pi/led', self._on_led, 10)
        self._republish_timer = self.create_timer(1.0 / republish_hz, self._republish_button)

    def _on_led(self, msg: Bool):
        if self._led is None:
            self.get_logger().info(f'[sim] LED -> {msg.data}')
            return
        if msg.data:
            self._led.on()
        else:
            self._led.off()

    def _publish_button(self, pressed: bool):
        out = Bool()
        out.data = pressed
        self._button_pub.publish(out)

    def _republish_button(self):
        pressed = bool(self._button.is_pressed) if self._button is not None else False
        self._publish_button(pressed)

    def destroy_node(self):
        if self._led is not None:
            self._led.close()
        if self._button is not None:
            self._button.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = PiGpioNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
