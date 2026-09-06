# 第 2 章：Raspberry Pi 與 ROS 2

## 2.1 為什麼 Raspberry Pi 是最標準的路徑

Raspberry Pi 4 與 5 都能跑 Ubuntu 24.04 arm64，而 Ubuntu 24.04 是 ROS 2 Jazzy 的 **Tier 1 平台**，可以直接用 apt 安裝官方二進位套件。三章之中，這是唯一不需要容器也不需要交叉編譯的路徑。

| 型號 | RAM | 適用範圍 |
|------|-----|----------|
| Pi 4 4 GB | 4 GB | 教學、閘道、單一感測器節點 |
| Pi 5 8 GB | 8 GB | Nav2、2D LiDAR SLAM、輕量視覺推論 |
| Pi 5 16 GB | 16 GB | 多感測器融合、RViz 遠端顯示、開發機 |

**不建議** 使用 Raspberry Pi OS 跑 ROS 2。它是 Debian 衍生版，在 ROS 2 屬 Tier 3，得從原始碼編譯。

## 2.2 系統安裝

1. 用 Raspberry Pi Imager 燒錄 **Ubuntu Server 24.04 LTS (64-bit)**，在進階選項中預設好使用者、Wi-Fi 與 SSH。
2. 開機後更新並安裝基本工具：

```bash
sudo apt update && sudo apt full-upgrade -y
sudo apt install -y curl gnupg lsb-release software-properties-common
sudo add-apt-repository -y universe
```

3. 加入 ROS 2 apt 來源（官方目前以 `ros2-apt-source` 套件管理金鑰與來源）：

```bash
export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}')
curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb"
sudo dpkg -i /tmp/ros2-apt-source.deb
sudo apt update
```

4. 安裝 ROS 2 Jazzy。無螢幕的 Pi 裝 `ros-base` 即可，RViz 放在筆電上遠端看：

```bash
sudo apt install -y ros-jazzy-ros-base ros-dev-tools python3-colcon-common-extensions
echo 'source /opt/ros/jazzy/setup.bash' >> ~/.bashrc
echo 'export ROS_DOMAIN_ID=42' >> ~/.bashrc
source ~/.bashrc
```

5. 驗證：

```bash
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_py listener
```

## 2.3 GPIO 存取

Ubuntu 上不能用 RPi.GPIO（它依賴 Raspberry Pi OS 的 `/dev/mem` 存取方式，在 Pi 5 上完全不支援）。本章使用 **gpiozero + lgpio** 後端，Pi 4 與 Pi 5 皆可：

```bash
sudo apt install -y python3-gpiozero python3-lgpio
sudo usermod -aG dialout,gpio "$USER"   # gpio 群組不存在時可忽略
```

若 `/dev/gpiochip*` 權限不足，加上 udev 規則：

```bash
sudo tee /etc/udev/rules.d/60-gpio.rules <<'RULE'
SUBSYSTEM=="gpio", KERNEL=="gpiochip*", MODE="0660", GROUP="dialout"
RULE
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## 2.4 範例：LED 與按鈕節點

檔案：[`examples/raspberry_pi/ros2_ws/src/pi_gpio/pi_gpio/gpio_node.py`](../examples/raspberry_pi/ros2_ws/src/pi_gpio/pi_gpio/gpio_node.py)

接線（BCM 編號）：

| 元件 | GPIO | 說明 |
|------|------|------|
| LED | 17 | 串 330 Ω 電阻到 GND |
| 按鈕 | 27 | 另一端接 GND，使用內建上拉 |

節點行為：

- 訂閱 `/pi/led`（`std_msgs/Bool`）控制 LED。
- 按鈕按下或放開時，發布 `/pi/button`（`std_msgs/Bool`），另外每秒重送一次目前狀態，方便晚加入的節點取得初始值。

編譯與執行：

```bash
mkdir -p ~/ros2_ws/src
cp -r examples/raspberry_pi/ros2_ws/src/pi_gpio ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 run pi_gpio gpio_node
```

驗證：

```bash
ros2 topic pub --once /pi/led std_msgs/msg/Bool "{data: true}"
ros2 topic echo /pi/button
```

## 2.5 開機自動啟動

檔案：[`examples/raspberry_pi/systemd/ros2-pi-gpio.service`](../examples/raspberry_pi/systemd/ros2-pi-gpio.service)

```bash
sudo cp examples/raspberry_pi/systemd/ros2-pi-gpio.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ros2-pi-gpio
journalctl -u ros2-pi-gpio -f
```

幾個實務細節已寫進 unit 檔：

- `After=network-online.target`，避免 DDS 在網卡起來前綁定錯誤介面。
- `Restart=on-failure` 加 `RestartSec=3`，節點異常時自動重啟。
- 以一般使用者執行，不用 root。
- 環境變數集中在 `/etc/ros2/env`，方便日後改 domain ID 或切換 RMW。

## 2.6 跨板通訊：與 UNO Q、STM32 互通

三塊板子接在同一個區域網路、`ROS_DOMAIN_ID=42` 後，在 Pi 上應可看到全部 topic：

```bash
ros2 topic list
# /pi/button
# /pi/led
# /stm32/heartbeat       <- 第 3 章的 micro-ROS 節點，經 Agent 進來
# /stm32/led
# /uno_q/analog0         <- 第 1 章
# /uno_q/led
```

一個簡單的整合示範，讓 UNO Q 的可變電阻控制 Pi 的 LED，Pi 的按鈕控制 STM32 的 LED：

```bash
# 終端 1：analog0 > 512 就點亮 Pi LED
ros2 topic echo /uno_q/analog0 --field data | while read v; do
  [[ "$v" =~ ^[0-9]+$ ]] || continue
  if [ "$v" -gt 512 ]; then s=true; else s=false; fi
  ros2 topic pub --once /pi/led std_msgs/msg/Bool "{data: $s}" >/dev/null
done

# 終端 2：Pi 按鈕直接轉發給 STM32
# 注意：echo --field 印出的是 Python 風格的 True / False（字首大寫）
ros2 topic echo /pi/button --field data | while read v; do
  case "$v" in
    True)  s=true ;;
    False) s=false ;;
    *)     continue ;;
  esac
  ros2 topic pub --once /stm32/led std_msgs/msg/Bool "{data: $s}" >/dev/null
done
```

正式系統應把這類邏輯寫成節點，而不是 shell 迴圈——每次 `ros2 topic pub --once` 都要重新建立節點與 discovery，延遲以秒計，跟不上高頻輸入。這裡只是驗證三板互通。

## 2.7 Wi-Fi 上 discovery 不穩定的解法：Zenoh

DDS 預設靠 UDP multicast 做 discovery。企業或工廠的 Wi-Fi AP 常會限制 multicast，症狀是 `ros2 topic list` 時有時無。Jazzy 起可以改用 **rmw_zenoh_cpp**，它用 TCP 加一個輕量 router，不依賴 multicast。

安裝與設定（所有板子都要做，容器內同樣）：

```bash
sudo apt install -y ros-jazzy-rmw-zenoh-cpp
```

在 Pi 上啟動 router（一個網路只需要一個，建議放在 Pi 上並以 systemd 常駐）：

```bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 run rmw_zenoh_cpp rmw_zenohd
```

其他板子的節點指向 Pi 的 router。範例設定檔在 [`examples/raspberry_pi/config/zenoh_session_config.json5`](../examples/raspberry_pi/config/zenoh_session_config.json5)，把 `<PI-IP>` 換成 Pi 的 IP：

```bash
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_SESSION_CONFIG_URI=/path/to/zenoh_session_config.json5
ros2 run pi_gpio gpio_node
```

注意事項：

- 同一個系統內所有節點必須用同一種 RMW。DDS 與 Zenoh 節點彼此看不到。
- micro-ROS Agent 目前只支援 DDS。若切到 Zenoh，STM32 需透過 Pi 上的 `zenoh-bridge-ros2dds` 轉發，或改用 Zenoh-pico。這部分在第 3 章 3.9 節說明。

## 2.8 相機與 LiDAR

常見周邊在 Jazzy 上的套件：

| 周邊 | 套件 | 備註 |
|------|------|------|
| Pi Camera Module 3 | `camera_ros`（libcamera 後端） | `sudo apt install ros-jazzy-camera-ros` |
| USB 相機 | `usb_cam` 或 `v4l2_camera` | 隨插即用 |
| RPLIDAR A1/A2 | `rplidar_ros` | `sudo apt install ros-jazzy-rplidar-ros` |
| LD19 / LD06 | `ldlidar_stl_ros2` | 從原始碼編譯 |

在 Pi 5 上，`camera_ros` 以 640x480 30 fps 發布 raw image 約用一顆核心的 40%。若要傳到筆電看，改用 `image_transport` 的 compressed 格式。

## 2.9 常見問題

**`ros2 topic list` 看得到別台的 topic，但 `echo` 沒資料。**
通常是防火牆。Ubuntu Server 預設 ufw 關閉，但若有開，需放行 UDP 7400 到 7500：

```bash
sudo ufw allow 7400:7500/udp
```

**多張網卡（有線加 Wi-Fi）時 discovery 混亂。**
指定 CycloneDDS 只用一張網卡：

```bash
sudo apt install -y ros-jazzy-rmw-cyclonedds-cpp
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI='<CycloneDDS><Domain><General><Interfaces><NetworkInterface name="wlan0"/></Interfaces></General></Domain></CycloneDDS>'
```

**SD 卡壽命。**
長期運作的 Pi 建議用 NVMe（Pi 5）或 USB SSD，並把 `journald` 的 `Storage` 設成 `volatile`，避免日誌把卡寫壞。

## 參考資料

- [ROS 2 Jazzy 官方安裝文件（Ubuntu deb）](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html)
- [rmw_zenoh](https://github.com/ros2/rmw_zenoh)
- [gpiozero 文件](https://gpiozero.readthedocs.io/)
- [camera_ros](https://github.com/christianrauch/camera_ros)
