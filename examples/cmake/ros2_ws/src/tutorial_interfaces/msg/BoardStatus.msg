# 三塊板子的彙整狀態，由 board_monitor 以 1 Hz 發布到 /boards/status
std_msgs/Header header
int32 uno_q_analog0        # UNO Q A0 讀值，0..1023
bool  pi_button            # Raspberry Pi 按鈕是否按下
int32 stm32_heartbeat      # STM32 最後一次 heartbeat 計數
bool  stm32_alive          # heartbeat 在 stm32_timeout_sec 內有更新
