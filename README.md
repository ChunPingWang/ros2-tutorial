# ROS 2 教學：從單板電腦到微控制器

本教學以 **ROS 2 Jazzy Jalisco（LTS，支援至 2029 年 5 月）** 為基準，示範如何把 ROS 2 部署到三種常見的嵌入式硬體上，並讓它們透過同一個 ROS 2 網路協同工作。

| 章節 | 硬體 | ROS 2 執行方式 | 定位 |
|------|------|----------------|------|
| [第 1 章](docs/01-arduino-uno-q.md) | Arduino UNO Q | Linux 側以 Docker 執行 ROS 2，MCU 側用 Bridge RPC 接手即時 I/O | 帶 Linux 的邊緣控制器 |
| [第 2 章](docs/02-raspberry-pi.md) | Raspberry Pi 4 / 5 | Ubuntu 24.04 原生安裝 ROS 2 Jazzy | 機器人主腦、閘道、感知節點 |
| [第 3 章](docs/03-stm32-micro-ros.md) | STM32（Nucleo 系列） | micro-ROS + FreeRTOS，透過 micro-ROS Agent 接入 | 硬即時的馬達與感測器節點 |
| [第 4 章](docs/04-cmake.md) | 全部 | ament_cmake、自訂訊息、C++ 節點、STM32 的 CMake 專案 | 建置系統與跨板介面契約 |

## 為什麼是這三種板子

從系統架構的角度，一套機器人或工業邊緣系統通常會分成三層：

```
┌──────────────────────────────────────────────────────────────┐
│  決策 / 感知層          Raspberry Pi 5、Jetson、工控機         │
│  Nav2、SLAM、視覺推論、與雲端或 MES 整合                        │
├──────────────────────────────────────────────────────────────┤
│  邊緣控制層             Arduino UNO Q                          │
│  Linux 側跑 ROS 2 節點，MCU 側處理 GPIO / PWM / 感測器          │
├──────────────────────────────────────────────────────────────┤
│  硬即時層               STM32 + micro-ROS                       │
│  馬達閉迴路、編碼器、安全 I/O，毫秒級確定性                       │
└──────────────────────────────────────────────────────────────┘
```

三層之間全部使用 ROS 2 的 topic、service、action 溝通。micro-ROS 節點透過 Agent 進入 DDS 網路後，在 `ros2 topic list` 裡看起來與一般節點沒有差別。

## 三塊板子如何一起工作

三章的範例刻意使用一致的 topic 命名，接好網路後可以直接互通：

| Topic | 型別 | 發布者 | 訂閱者 |
|-------|------|--------|--------|
| `/uno_q/led` | `std_msgs/Bool` | Pi 或任何節點 | UNO Q |
| `/uno_q/analog0` | `std_msgs/Int32` | UNO Q | Pi |
| `/pi/led` | `std_msgs/Bool` | 任何節點 | Pi |
| `/pi/button` | `std_msgs/Bool` | Pi | 任何節點 |
| `/stm32/led` | `std_msgs/Bool` | 任何節點 | STM32 |
| `/stm32/heartbeat` | `std_msgs/Int32` | STM32 | 任何節點 |
| `/boards/status` | `tutorial_interfaces/BoardStatus` | 第 4 章的 C++ 節點 | 上層系統 |

跨板通訊的共同前提：

1. 所有節點使用相同的 `ROS_DOMAIN_ID`（範例統一用 `42`）。
2. 同一個區域網路，且允許 UDP multicast。Wi-Fi 環境下若 discovery 不穩定，第 2 章提供 Zenoh 中介層的替代設定。
3. STM32 本身不上網路，由第 3 章的 micro-ROS Agent 代理進入 DDS 網路。

## 建議閱讀順序

- 只想快速看到 ROS 2 跑起來：先讀第 2 章（Raspberry Pi），它是最標準的安裝路徑。
- 手上有 UNO Q：直接讀第 1 章，它同時涵蓋 Linux 側與 MCU 側。
- 需要硬即時控制：讀第 3 章，micro-ROS 的建置流程與前兩章差異最大。
- 要寫 C++ 節點、自訂訊息，或把韌體納入 CI：讀第 4 章。

## 目錄結構

```
docs/                        教學章節
examples/uno_q/              第 1 章範例：Sketch、Host 端橋接服務、ROS 2 套件、Docker
examples/raspberry_pi/       第 2 章範例：ROS 2 套件、systemd 服務、Zenoh 設定
examples/stm32/              第 3 章範例：micro-ROS 應用程式碼、FreeRTOS 整合、CMake toolchain file
examples/cmake/              第 4 章範例：自訂訊息套件與彙整三塊板子狀態的 C++ 節點
```

## 版本假設

| 元件 | 版本 |
|------|------|
| ROS 2 | Jazzy Jalisco |
| Ubuntu（Raspberry Pi） | 24.04 LTS arm64 |
| Arduino App Lab | 2025 年 10 月後的正式版 |
| micro-ROS | jazzy 分支 |
| STM32CubeIDE | 1.16 以上 |
| CMake | 3.28（Ubuntu 24.04 內建） |

若改用 Humble，套件名稱大多只需把 `jazzy` 換成 `humble`，但 `rmw_zenoh_cpp` 在 Humble 上仍屬實驗性質。
