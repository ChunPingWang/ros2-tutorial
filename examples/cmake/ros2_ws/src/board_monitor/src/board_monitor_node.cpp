// board_monitor：把三塊板子的 topic 彙整成一筆 tutorial_interfaces/BoardStatus
//
//   訂閱 /uno_q/analog0    std_msgs/Int32   (第 1 章)
//   訂閱 /pi/button        std_msgs/Bool    (第 2 章)
//   訂閱 /stm32/heartbeat  std_msgs/Int32   (第 3 章，micro-ROS)
//   發布 /boards/status    tutorial_interfaces/BoardStatus  1 Hz

#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/int32.hpp"
#include "tutorial_interfaces/msg/board_status.hpp"

using namespace std::chrono_literals;

class BoardMonitor : public rclcpp::Node
{
public:
  BoardMonitor()
  : Node("board_monitor")
  {
    stm32_timeout_sec_ = declare_parameter<double>("stm32_timeout_sec", 1.0);
    const double publish_rate_hz = declare_parameter<double>("publish_rate_hz", 1.0);

    // micro-ROS 預設是 best-effort，用 SensorDataQoS 才收得到 STM32 的資料。
    // 三個訂閱統一用同一個 QoS，避免日後有人把 Pi 節點也改成 best-effort 時收不到。
    const auto qos = rclcpp::SensorDataQoS();

    analog_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/uno_q/analog0", qos,
      [this](const std_msgs::msg::Int32 & msg) {uno_q_analog0_ = msg.data;});

    button_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/pi/button", qos,
      [this](const std_msgs::msg::Bool & msg) {pi_button_ = msg.data;});

    heartbeat_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/stm32/heartbeat", qos,
      [this](const std_msgs::msg::Int32 & msg) {
        stm32_heartbeat_ = msg.data;
        last_heartbeat_time_ = now();
      });

    status_pub_ = create_publisher<tutorial_interfaces::msg::BoardStatus>("/boards/status", 10);

    const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::milliseconds>(period),
      [this]() {publish_status();});

    RCLCPP_INFO(
      get_logger(), "board_monitor started, stm32 timeout %.1fs, publishing at %.1f Hz",
      stm32_timeout_sec_, publish_rate_hz);
  }

private:
  void publish_status()
  {
    tutorial_interfaces::msg::BoardStatus msg;
    msg.header.stamp = now();
    msg.header.frame_id = "boards";
    msg.uno_q_analog0 = uno_q_analog0_;
    msg.pi_button = pi_button_;
    msg.stm32_heartbeat = stm32_heartbeat_;

    const bool has_heartbeat = last_heartbeat_time_.nanoseconds() > 0;
    const double age_sec = has_heartbeat ? (now() - last_heartbeat_time_).seconds() : 0.0;
    msg.stm32_alive = has_heartbeat && age_sec < stm32_timeout_sec_;

    status_pub_->publish(msg);
  }

  double stm32_timeout_sec_{1.0};

  int32_t uno_q_analog0_{0};
  bool pi_button_{false};
  int32_t stm32_heartbeat_{0};
  rclcpp::Time last_heartbeat_time_{0, 0, RCL_ROS_TIME};

  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr analog_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr button_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr heartbeat_sub_;
  rclcpp::Publisher<tutorial_interfaces::msg::BoardStatus>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BoardMonitor>());
  rclcpp::shutdown();
  return 0;
}
