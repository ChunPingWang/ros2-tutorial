# 第 3 章：STM32 與 micro-ROS

## 3.1 微控制器上的 ROS 2 長什麼樣

STM32 沒有作業系統、沒有網路堆疊、RAM 通常只有幾十到幾百 KB，跑不了完整的 ROS 2。**micro-ROS** 是 ROS 2 官方的微控制器分支，它保留了 `rcl`、`rclc` 的 API，把底層換成輕量的 Micro XRCE-DDS：

```
   STM32 (micro-ROS client)                  Linux 主機 (例如第 2 章的 Pi)
 ┌──────────────────────────┐   UART/USB    ┌──────────────────────────┐
 │ rclc publisher/subscriber│ ────────────▶ │  micro-ROS Agent          │
 │ Micro XRCE-DDS client    │ ◀──────────── │  (XRCE ⇄ DDS 轉換)        │──▶ ROS 2 DDS 網路
 │ FreeRTOS                 │               │  docker: micro-ros-agent  │
 └──────────────────────────┘               └──────────────────────────┘
```

Agent 是關鍵：STM32 只跟 Agent 講一條序列通道，Agent 代表它進入 DDS 網路。從其他節點看，`/stm32/heartbeat` 就是一個普通 topic。

## 3.2 適用場景與限制

| 適合 | 不適合 |
|------|--------|
| 馬達閉迴路控制、編碼器回讀 | 影像、點雲等大資料 |
| 安全 I/O、急停、互鎖 | 需要 action server 的複雜狀態機（可做但吃資源） |
| 高頻感測器（IMU 1 kHz） | 需要動態建立多個節點 |
| 確定性延遲需求 | 需要 Python |

資源需求量級：Flash 約 150 到 250 KB，RAM 約 30 到 60 KB（含 FreeRTOS 與 micro-ROS 堆疊）。STM32F4 以上、RAM 至少 128 KB 的型號都足夠。

## 3.3 硬體與工具鏈

本章以 **NUCLEO-F446RE** 為例（Cortex-M4 180 MHz，512 KB Flash，128 KB RAM，板載 ST-LINK 提供虛擬 COM port）。其他 Nucleo 板只需調整 UART 與時脈設定。

需要的工具：

| 工具 | 用途 |
|------|------|
| STM32CubeIDE 1.16 以上 | 產生初始化程式碼、編譯、燒錄 |
| Docker | 建置 micro-ROS 靜態函式庫、執行 Agent |
| Linux 主機（Pi 或筆電） | 執行 Agent 與 ROS 2 |

三種替代路徑，各有取捨：

| 路徑 | 說明 |
|------|------|
| **CubeMX + FreeRTOS（本章）** | 官方 `micro_ros_stm32cubemx_utils`，最貼近量產專案 |
| micro_ros_arduino | 用 Arduino core for STM32，上手最快，但難整合 DMA、多工 |
| Zephyr | 官方支援，適合已在用 Zephyr 的團隊；UNO Q 的 STM32U585 走這條路 |

## 3.4 CubeMX 專案設定

建立新專案後，依序設定：

1. **時脈**：HCLK 180 MHz（F446RE）。
2. **USART2**（接 ST-LINK 虛擬 COM）：
   - Mode: Asynchronous，115200 8N1
   - **DMA Settings**：新增 USART2_RX 與 USART2_TX，RX 設 Circular 模式。這是 micro-ROS 序列傳輸的必要條件。
   - NVIC：啟用 USART2 global interrupt。
3. **FreeRTOS**：Middleware 選 FreeRTOS，Interface CMSIS_V2。
   - Config parameters → `TOTAL_HEAP_SIZE` 設 **30000** 以上。
   - Tasks and Queues → defaultTask 的 Stack Size 設 **3000 words**（12 KB）。micro-ROS 官方要求 10 KB 以上。
4. **SYS**：Timebase Source 改為 TIM6 或其他 timer（FreeRTOS 需要 SysTick）。
5. **GPIO**：PA5 設為 Output（板載 LD2）。
6. Project Manager → Toolchain 選 STM32CubeIDE，產生程式碼。

## 3.5 引入 micro-ROS 函式庫

在專案根目錄加入官方 utils：

```bash
cd <CubeIDE 專案目錄>
git clone -b jazzy https://github.com/micro-ROS/micro_ros_stm32cubemx_utils.git
```

用 Docker 建置靜態函式庫（會依專案的編譯旗標客製，第一次約 10 到 20 分鐘）：

```bash
docker pull microros/micro_ros_static_library_builder:jazzy
docker run -it --rm -v "$(pwd)":/project --env MICROROS_LIBRARY_FOLDER=micro_ros_stm32cubemx_utils/microros_static_library_ide \
  microros/micro_ros_static_library_builder:jazzy
```

建置完成後在 CubeIDE 專案設定裡加入：

| 設定 | 值 |
|------|----|
| C/C++ Build → Settings → MCU GCC Compiler → Include paths | `micro_ros_stm32cubemx_utils/microros_static_library_ide/libmicroros/include` |
| MCU GCC Linker → Libraries → Library search path | `micro_ros_stm32cubemx_utils/microros_static_library_ide/libmicroros` |
| MCU GCC Linker → Libraries → Libraries | `microros` |
| C/C++ General → Paths and Symbols → Source Location | 加入 `micro_ros_stm32cubemx_utils/extra_sources` |

`extra_sources` 內含序列傳輸與 FreeRTOS 記憶體配置的實作，其中 `dma_transport.c` 是本章使用的傳輸層。同資料夾內有其他傳輸（如 `it_transport.c`、`usb_cdc_transport.c`），只保留一個，其餘從 build 中排除。

## 3.6 應用程式

檔案：[`examples/stm32/Core/Src/micro_ros_app.c`](../examples/stm32/Core/Src/micro_ros_app.c)

節點行為：

- 發布 `/stm32/heartbeat`（`std_msgs/Int32`），每 100 ms 遞增一次。
- 訂閱 `/stm32/led`（`std_msgs/Bool`），控制 PA5 的 LD2。
- 開機後等待 Agent 出現，斷線後自動重連，不會卡死。

在 CubeMX 產生的 `main.c` 的 `StartDefaultTask` 裡呼叫：

```c
/* USER CODE BEGIN 5 */
micro_ros_app_task();   // 不會返回
/* USER CODE END 5 */
```

並在 `main.c` 的 include 區加入 `#include "micro_ros_app.h"`。

核心流程摘要（完整程式碼見範例檔）：

```c
rmw_uros_set_custom_transport(true, (void *)&huart2,
    cubemx_transport_open, cubemx_transport_close,
    cubemx_transport_write, cubemx_transport_read);

rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
freeRTOS_allocator.allocate      = microros_allocate;
freeRTOS_allocator.deallocate    = microros_deallocate;
freeRTOS_allocator.reallocate    = microros_reallocate;
freeRTOS_allocator.zero_allocate = microros_zero_allocate;
rcutils_set_default_allocator(&freeRTOS_allocator);

// 等待 Agent
while (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) { osDelay(500); }

rclc_support_init(&support, 0, NULL, &allocator);
rclc_node_init_default(&node, "stm32_node", "", &support);
rclc_publisher_init_default(&heartbeat_pub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "/stm32/heartbeat");
rclc_subscription_init_default(&led_sub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), "/stm32/led");
rclc_timer_init_default2(&heartbeat_timer, &support, RCL_MS_TO_NS(100), heartbeat_cb, true);

rclc_executor_init(&executor, &support.context, 2, &allocator);
rclc_executor_add_timer(&executor, &heartbeat_timer);
rclc_executor_add_subscription(&executor, &led_sub, &led_msg, &led_cb, ON_NEW_DATA);

for (;;) {
    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
    osDelay(1);
}
```

`rclc_executor_init` 的第三個參數是 handle 數量，等於 timer 數加 subscription 數，寫少了會初始化失敗。

## 3.7 執行 Agent

把 Nucleo 用 USB 接到 Linux 主機（Pi 或筆電），確認裝置名稱：

```bash
ls /dev/ttyACM*
```

用 Docker 啟動 Agent（`--net=host` 讓它與其他 ROS 2 節點在同一 DDS 網路）：

```bash
docker run -it --rm --privileged --net=host -v /dev:/dev \
  -e ROS_DOMAIN_ID=42 \
  microros/micro-ros-agent:jazzy serial --dev /dev/ttyACM0 -b 115200 -v4
```

看到 `session established` 表示 STM32 已連上。若在 Pi 上想常駐，把它寫成 systemd 服務，作法與第 2 章相同，只是 `ExecStart` 換成上面的 `docker run` 指令並拿掉 `-it`。

## 3.8 驗證

在任何同網路、`ROS_DOMAIN_ID=42` 的機器上：

```bash
ros2 node list
# /stm32_node

ros2 topic echo /stm32/heartbeat
ros2 topic pub --once /stm32/led std_msgs/msg/Bool "{data: true}"
```

板上 LD2 應亮起。常見錯誤：

| 症狀 | 原因 |
|------|------|
| Agent 一直沒有 `session established` | DMA 未設 Circular、鮑率不一致，或 `extra_sources` 同時編了兩個 transport |
| 初始化後 HardFault | Task stack 不足，調到 3000 words 以上 |
| `rclc_executor_init` 失敗 | handle 數量少於 timer 加 subscription |
| 跑一陣子後 subscription 不再觸發 | 已知的 DMA 接收環形緩衝區問題，更新到 utils 最新 commit，或在 `led_cb` 裡避免阻塞 |

## 3.9 與前兩章整合時的注意事項

- **Agent 放哪裡**：建議放在第 2 章的 Pi 上。UNO Q 的 Linux 側也可以跑 Agent，但 UNO Q 的 USB 埠數量有限，且它自己已有一顆 MCU。
- **Domain ID**：由 Agent 的環境變數決定，STM32 端不用設。
- **Zenoh**：Agent 只支援 DDS。若整個系統改用 rmw_zenoh_cpp，有兩個選擇：一是 Agent 主機同時跑一個 `domain_bridge` 節點在 DDS 與 Zenoh 之間轉發指定 topic；二是 STM32 改用 Zenoh-pico，直接連 Pi 上的 zenoh router，但那樣就不是 micro-ROS，失去 rclc API。目前生產環境多數仍用 DDS 加 Agent。
- **時間同步**：micro-ROS 節點的時間戳來自 STM32 自身時鐘。需要與 Pi 對時的話，用 `rmw_uros_sync_session()` 與 `rmw_uros_epoch_millis()`，範例程式碼已示範。

## 3.10 進階方向

- **自訂訊息**：把 `.msg` 放進 `micro_ros_stm32cubemx_utils/microros_static_library_ide/library_generation/extra_packages`，重建靜態函式庫即可。
- **多執行緒**：每個 executor 綁一個 FreeRTOS task，高頻控制與低頻狀態回報分開。
- **UNO Q 的 STM32U585**：Zephyr 已支援該板，micro-ROS 有 Zephyr 模組。若要讓 UNO Q 的 MCU 側直接成為 micro-ROS 節點，Agent 跑在 UNO Q 的 Linux 側，傳輸走內部序列通道。本章的 rclc 程式碼可原封不動搬過去，只換傳輸層。

## 參考資料

- [micro-ROS 官方網站](https://micro.ros.org/)
- [micro_ros_stm32cubemx_utils（jazzy 分支）](https://github.com/micro-ROS/micro_ros_stm32cubemx_utils)
- [任意 STM32 移植 micro-ROS 的社群指南](https://github.com/lFatality/stm32_micro_ros_setup)
- [rclc 範例集](https://github.com/ros2/rclc/tree/master/rclc_examples)
