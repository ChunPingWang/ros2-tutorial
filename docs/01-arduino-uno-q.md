# 第 1 章：Arduino UNO Q 與 ROS 2

## 1.1 這塊板子的特殊之處

Arduino UNO Q 是 Arduino 被 Qualcomm 收購後推出的第一塊「雙腦」板：

| 側 | 晶片 | 作業系統 | 角色 |
|----|------|----------|------|
| MPU（Linux 側） | Qualcomm Dragonwing QRB2210，四核 Cortex-A53 2.0 GHz，2 或 4 GB LPDDR4X，16 或 32 GB eMMC | Debian 13 | 執行 ROS 2、Python、AI 推論 |
| MCU（即時側） | STM32U585，Cortex-M33 160 MHz | 裸機 Arduino core 或 Zephyr | GPIO、PWM、ADC、I2C、SPI |

兩側之間透過 Arduino 官方的 **Bridge** RPC 機制溝通。Linux 側呼叫 `Bridge.call("函式名", 參數)`，MCU 側用 `Bridge.provide("函式名", 函式)` 註冊可被呼叫的函式，反向亦可。

**對 ROS 2 的意義**：ROS 2 節點跑在 Linux 側，不必碰 MCU；MCU 側維持傳統 Arduino Sketch 的寫法。這是與 Raspberry Pi 加獨立 Arduino 最大的差別：不用自己設計序列埠協定。

## 1.2 兩條安裝路徑

| 路徑 | 優點 | 缺點 |
|------|------|------|
| **A. Docker 容器（本章採用）** | 用官方 `ros:jazzy` 映像，10 分鐘可用；不污染 Debian 系統 | 容器內看不到 App Lab 的 Python 套件，需要一個小型橋接服務 |
| B. 原生安裝 | 節點可直接 `import arduino.app_utils` | Debian 是 ROS 2 的 Tier 3 平台，沒有 apt 二進位套件，必須從原始碼編譯，在 A53 上約需數小時 |

本章採用路徑 A，並用一個 40 行的 Python 服務把 Bridge 暴露成本機 TCP 介面，讓容器內的 ROS 2 節點可以呼叫。這種「宿主機做硬體代理、容器做 ROS 2」的分層在工業現場也是常見做法，方便日後把容器搬到別的主機上。

## 1.3 系統架構

```
                    Arduino UNO Q
 ┌────────────────────────────────────────────────────────────┐
 │  Linux (Debian)                                            │
 │                                                            │
 │   ┌─────────────────────┐    TCP 127.0.0.1:5555            │
 │   │ Docker: ros:jazzy   │ ───────────────────┐             │
 │   │  uno_q_bridge node  │                    ▼             │
 │   │  sub /uno_q/led     │        ┌──────────────────────┐  │
 │   │  pub /uno_q/analog0 │        │ bridge_server.py     │  │
 │   └─────────────────────┘        │ (App Lab Python 環境) │  │
 │          ▲ DDS (host network)    └──────────┬───────────┘  │
 │          │                                  │ Bridge RPC   │
 ├──────────┼──────────────────────────────────┼──────────────┤
 │  MCU (STM32U585)                            ▼              │
 │          │                       led_bridge.ino            │
 │          │                       provide("set_led")        │
 │          │                       provide("read_analog")    │
 └──────────┼─────────────────────────────────────────────────┘
            │
      區域網路上的其他 ROS 2 節點（例如第 2 章的 Raspberry Pi）
```

## 1.4 準備工作

1. 依 [官方 User Manual](https://docs.arduino.cc/tutorials/uno-q/user-manual/) 完成首次開機、Wi-Fi 設定與 App Lab 安裝。
2. 用 SSH 登入 Linux 側（App Lab 也提供終端機）：

```bash
ssh arduino@<UNO-Q-IP>
sudo apt update && sudo apt install -y docker.io git
sudo usermod -aG docker "$USER"   # 重新登入後生效
```

3. 確認架構與記憶體：

```bash
uname -m        # 應顯示 aarch64
free -h         # 2 GB 版本建議只跑 ros-base，不要跑 desktop
```

## 1.5 MCU 側：Sketch

檔案：[`examples/uno_q/sketch/led_bridge/led_bridge.ino`](../examples/uno_q/sketch/led_bridge/led_bridge.ino)

```cpp
#include <Arduino_RouterBridge.h>

const int LED_PIN = LED_BUILTIN;
const int ANALOG_PIN = A0;

bool set_led(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  return on;
}

int read_analog(int pin) {
  return analogRead(pin);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Bridge.begin();
  Bridge.provide("set_led", set_led);
  Bridge.provide("read_analog", read_analog);
}

void loop() {
  // 所有動作都由 Linux 側透過 RPC 觸發，loop 可留空或做看門狗。
  delay(10);
}
```

重點：

- `Bridge.provide` 註冊的函式簽名要與 Linux 側呼叫時的參數型別一致。
- `loop()` 留空是刻意的。即時邏輯（例如 PID）可以放在這裡，Linux 側只負責送設定值與讀狀態。
- 用 App Lab 上傳 Sketch。標頭檔名稱請以 App Lab 內建的 Bridge 範例為準，不同版本可能略有差異。

## 1.6 Linux 側：橋接服務

檔案：[`examples/uno_q/host/bridge_server.py`](../examples/uno_q/host/bridge_server.py)

這個服務在 App Lab 的 Python 環境中執行，監聽 `127.0.0.1:5555`，接受一行一個 JSON 請求：

```json
{"fn": "set_led", "args": [true]}
{"fn": "read_analog", "args": [0]}
```

並回傳：

```json
{"ok": true, "result": 512}
```

啟動方式（在 App Lab 建立一個 App，把此檔案作為 Python 主程式，或直接在終端機執行）：

```bash
python3 examples/uno_q/host/bridge_server.py
```

為什麼不讓 ROS 2 節點直接呼叫 Bridge：容器內沒有 App Lab 的 Python 套件，而且把硬體存取集中在一個宿主機程序，可以避免多個節點同時搶 RPC 通道。

## 1.7 ROS 2 側：容器與節點

### 建立映像

檔案：[`examples/uno_q/docker/Dockerfile`](../examples/uno_q/docker/Dockerfile)

```bash
cd examples/uno_q
docker build -t uno-q-ros2 -f docker/Dockerfile .
```

映像以 `ros:jazzy-ros-base` 為基底（arm64 官方支援），並把 `ros2_ws` 複製進去用 `colcon` 編譯。

### 執行

```bash
docker run -d --name uno-q-ros2 --restart unless-stopped \
  --network host \
  -e ROS_DOMAIN_ID=42 \
  -e UNO_Q_BRIDGE_HOST=127.0.0.1 \
  uno-q-ros2
```

`--network host` 有兩個用途：讓 DDS multicast 直接使用宿主機網卡，以及讓容器用 `127.0.0.1` 連到橋接服務。

### 節點程式

檔案：[`examples/uno_q/ros2_ws/src/uno_q_bridge/uno_q_bridge/bridge_node.py`](../examples/uno_q/ros2_ws/src/uno_q_bridge/uno_q_bridge/bridge_node.py)

節點做兩件事：

- 訂閱 `/uno_q/led`（`std_msgs/Bool`），收到後呼叫 `set_led`。
- 每 100 ms 呼叫 `read_analog(0)`，發布到 `/uno_q/analog0`（`std_msgs/Int32`）。

連線失敗時節點會每秒重試，不會讓整個容器崩潰。

## 1.8 驗證

在 UNO Q 上，或在同一網路、`ROS_DOMAIN_ID=42` 的任何機器上：

```bash
ros2 topic list
# /uno_q/analog0
# /uno_q/led

ros2 topic echo /uno_q/analog0 --once
ros2 topic pub --once /uno_q/led std_msgs/msg/Bool "{data: true}"
```

板上的 LED 應該亮起。若沒有反應，依序檢查：

1. `docker logs uno-q-ros2`，看是否有 `connected to bridge` 訊息。
2. `ss -ltnp | grep 5555`，確認橋接服務在監聽。
3. App Lab 的 Sketch 是否成功上傳且 `Bridge.begin()` 有執行。

## 1.9 效能與資源評估

在 4 GB 版本上實測的量級（供規劃參考，非精確基準）：

| 項目 | 數值 |
|------|------|
| `ros:jazzy-ros-base` 容器閒置記憶體 | 約 150 MB |
| 本章節點 CPU 使用率 | 單核 3% 以下 |
| Bridge RPC 往返延遲 | 約 1 到 3 ms |
| 100 Hz 的 analog 發布 | 可穩定達成 |

不適合在 UNO Q 上跑的東西：Nav2 全套、視覺 SLAM、RViz。這些應放到第 2 章的 Raspberry Pi 5 或更強的主機，UNO Q 專心當 I/O 控制器。

## 1.10 進階方向

- **micro-ROS 跑在 MCU 側**：Zephyr 已正式支援 UNO Q 的 STM32U585，理論上可以用 micro-ROS 的 Zephyr 移植版讓 MCU 直接成為 ROS 2 節點，透過與 Linux 側的序列通道連到 Agent。目前尚無官方範例，若採用需自行驗證，第 3 章的 micro-ROS 概念可以直接沿用。
- **邊緣 AI**：QRB2210 的 ISP 支援兩顆相機。可以在 Linux 側用 App Lab 的 AI Brick 做偵測，結果透過本章的節點發布為 ROS 2 訊息。
- **多台 UNO Q**：每台各跑一個容器，用 namespace 區分（`/cell1/uno_q/led`），由 Pi 統一調度。這是產線多工作站常見的拓樸。

## 參考資料

- [UNO Q 官方文件](https://docs.arduino.cc/hardware/uno-q)
- [UNO Q User Manual](https://docs.arduino.cc/tutorials/uno-q/user-manual/)
- [ROS 2 Jazzy 在 UNO Q 容器中控制 LED 的社群範例](https://github.com/miguelgonrod/ros_arduino_uno_Q)
- [Zephyr 對 UNO Q 的支援](https://docs.zephyrproject.org/latest/boards/arduino/uno_q/doc/index.html)
