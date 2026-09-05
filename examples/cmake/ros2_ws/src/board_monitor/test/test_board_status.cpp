// 不啟動 ROS 的單元測試：驗證自訂訊息的預設值與欄位可正常存取。
#include <gtest/gtest.h>

#include "tutorial_interfaces/msg/board_status.hpp"

TEST(BoardStatus, DefaultsAreZeroAndFalse)
{
  tutorial_interfaces::msg::BoardStatus msg;
  EXPECT_EQ(msg.uno_q_analog0, 0);
  EXPECT_FALSE(msg.pi_button);
  EXPECT_EQ(msg.stm32_heartbeat, 0);
  EXPECT_FALSE(msg.stm32_alive);
  EXPECT_TRUE(msg.header.frame_id.empty());
}

TEST(BoardStatus, FieldsRoundTrip)
{
  tutorial_interfaces::msg::BoardStatus msg;
  msg.uno_q_analog0 = 1023;
  msg.pi_button = true;
  msg.stm32_heartbeat = 42;
  msg.stm32_alive = true;
  msg.header.frame_id = "boards";

  EXPECT_EQ(msg.uno_q_analog0, 1023);
  EXPECT_TRUE(msg.pi_button);
  EXPECT_EQ(msg.stm32_heartbeat, 42);
  EXPECT_TRUE(msg.stm32_alive);
  EXPECT_EQ(msg.header.frame_id, "boards");
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
