# 第 4 章：CMake 與 ROS 2 建置系統

## 4.1 CMake 在 ROS 2 裡的位置

前三章的 Python 節點用 `setup.py` 建置，看不到 CMake。但只要寫 C++ 節點、自訂訊息、或把 STM32 專案納入同一套建置流程，CMake 就躲不掉。ROS 2 的建置堆疊有三層：

```
colcon            工作區層級：找出所有套件、算相依順序、逐一建置
  └─ ament_cmake  套件層級：在 CMake 之上加 ROS 2 慣例（安裝路徑、套件索引、匯出相依）
       └─ CMake   實際產生 Makefile 或 Ninja 檔並編譯
```

| 你寫的東西 | 建置類型 | 章節 |
|------------|----------|------|
| Python 節點 | `ament_python`，走 setuptools | 第 1、2 章 |
| C++ 節點 | `ament_cmake` | 本章 |
| 自訂訊息 `.msg` `.srv` `.action` | `ament_cmake` 加 `rosidl` | 本章 |
| micro-ROS 靜態函式庫 | colcon 加 CMake toolchain file | 第 3 章、本章 4.8 |
| STM32 韌體本身 | CubeMX 匯出的 CMake 專案 | 本章 4.8 |

## 4.2 一個最小的 C++ 套件

一個 `ament_cmake` 套件至少有三個檔案：

```
board_monitor/
├── package.xml          給 colcon 與 rosdep 看的相依宣告
├── CMakeLists.txt       給 CMake 看的建置規則
└── src/board_monitor_node.cpp
```

兩份相依清單必須一致：`package.xml` 決定 colcon 的建置順序與 `rosdep install` 要裝什麼，`CMakeLists.txt` 決定實際連結什麼。漏寫其中一邊是新手最常見的錯誤，症狀是「在自己機器上能編，在乾淨的 CI 上找不到套件」。

### CMakeLists.txt 逐段說明

檔案：[`examples/cmake/ros2_ws/src/board_monitor/CMakeLists.txt`](../examples/cmake/ros2_ws/src/board_monitor/CMakeLists.txt)

```cmake
cmake_minimum_required(VERSION 3.8)
project(board_monitor)
```
Jazzy 的最低需求是 3.8，Ubuntu 24.04 內建 3.28。`project()` 的名稱必須與 `package.xml` 的 `<name>` 相同。

```cmake
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
```
ROS 2 官方範本的預設警告等級。企業專案建議再加 `-Werror`，把警告當錯誤。

```cmake
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tutorial_interfaces REQUIRED)
```
每個 ROS 2 相依都是一個 CMake package。`find_package` 會讀取 `/opt/ros/jazzy/share/<pkg>/cmake/` 或工作區 `install/` 下的設定檔，取得 include 路徑、函式庫與匯出的 target。

```cmake
add_executable(board_monitor_node src/board_monitor_node.cpp)
target_compile_features(board_monitor_node PUBLIC cxx_std_17)
target_link_libraries(board_monitor_node PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${tutorial_interfaces_TARGETS})
```
這是 **Jazzy 建議的現代寫法**：直接連結 CMake target，而不是舊教材裡的 `ament_target_dependencies()`。`rclcpp::rclcpp` 是 rclcpp 匯出的 target；訊息套件則提供 `<pkg>_TARGETS` 變數，內含 C++ 型別支援的所有 target。兩種寫法在 Jazzy 上都能用；`ament_target_dependencies` 自 Kilted 起正式標記為 deprecated（Jazzy 尚未出警告，但官方教學已全面改用 `target_link_libraries`），新專案不要再用。

```cmake
install(TARGETS board_monitor_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
```
`ros2 run <pkg> <exe>` 只會到 `lib/<pkg>/` 找執行檔，`ros2 launch` 只會到 `share/<pkg>/` 找 launch 檔。路徑寫錯的症狀是 `colcon build` 成功但 `ros2 run` 說找不到。

```cmake
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
```
`ament_package()` 必須是最後一行。它產生套件索引、匯出相依，讓下游套件的 `find_package` 能找到本套件。

### package.xml 對應

```xml
<buildtool_depend>ament_cmake</buildtool_depend>
<depend>rclcpp</depend>
<depend>std_msgs</depend>
<depend>tutorial_interfaces</depend>
<test_depend>ament_lint_auto</test_depend>
<test_depend>ament_lint_common</test_depend>
<export>
  <build_type>ament_cmake</build_type>
</export>
```

`<depend>` 同時代表 build、exec、export 三種相依。只在編譯期需要的用 `<build_depend>`，只在執行期需要的用 `<exec_depend>`。

## 4.3 自訂訊息套件

前三章都用 `std_msgs/Bool` 與 `Int32`。實際系統很快會需要自己的訊息型別。慣例是把訊息獨立成一個只含介面的套件，本章命名為 `tutorial_interfaces`。

檔案：[`examples/cmake/ros2_ws/src/tutorial_interfaces/`](../examples/cmake/ros2_ws/src/tutorial_interfaces/)

```
tutorial_interfaces/
├── package.xml
├── CMakeLists.txt
└── msg/BoardStatus.msg
```

`BoardStatus.msg` 把三塊板子的狀態合成一筆：

```
# 三塊板子的彙整狀態，由 board_monitor 以 1 Hz 發布
std_msgs/Header header
int32 uno_q_analog0        # UNO Q A0 讀值，0..1023
bool  pi_button            # Raspberry Pi 按鈕是否按下
int32 stm32_heartbeat      # STM32 最後一次 heartbeat 計數
bool  stm32_alive          # heartbeat 在 stm32_timeout_sec 內有更新
```

CMakeLists.txt 的核心只有一行：

```cmake
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/BoardStatus.msg"
  DEPENDENCIES std_msgs
)
ament_export_dependencies(rosidl_default_runtime)
```

`rosidl_generate_interfaces` 會同時產生 C、C++、Python 三種語言的型別，所以第 1、2 章的 Python 節點與本章的 C++ 節點可以共用同一個訊息定義。`package.xml` 要加三行，少一行就會建置失敗：

```xml
<buildtool_depend>rosidl_default_generators</buildtool_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>
```

為什麼要獨立成套件：訊息定義是跨團隊的介面契約。獨立後，Python 團隊、C++ 團隊、韌體團隊只需相依這一個套件，改訊息只重建一處。

## 4.4 範例：board_monitor 節點

檔案：[`examples/cmake/ros2_ws/src/board_monitor/src/board_monitor_node.cpp`](../examples/cmake/ros2_ws/src/board_monitor/src/board_monitor_node.cpp)

節點訂閱前三章的三個輸出 topic，每秒發布一筆 `BoardStatus` 到 `/boards/status`：

```
/uno_q/analog0   ─┐
/pi/button       ─┼─▶  board_monitor  ─▶  /boards/status (tutorial_interfaces/BoardStatus)
/stm32/heartbeat ─┘
```

程式重點：

- 用 `rclcpp::Node` 的 lambda 訂閱，三個 callback 只更新成員變數。
- `stm32_alive` 由「最後一次收到 heartbeat 的時間」與參數 `stm32_timeout_sec` 決定，STM32 斷線後 1 秒內會反映出來。
- 訂閱使用 `rclcpp::SensorDataQoS()`（best-effort）。micro-ROS 的 `rclc_*_init_default` 其實是 reliable，但許多 micro-ROS 專案為了節省序列頻寬會改用 `rclc_publisher_init_best_effort`；best-effort 訂閱端與兩種發布端都相容，而 reliable 訂閱端配 best-effort 發布端會完全收不到資料——這是跨 micro-ROS 最常踩的坑。

### 建置與執行

```bash
mkdir -p ~/ros2_ws/src
cp -r examples/cmake/ros2_ws/src/* ~/ros2_ws/src/
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -y
colcon build --symlink-install
source install/setup.bash
ros2 launch board_monitor board_monitor.launch.py
```

另開終端驗證：

```bash
ros2 topic echo /boards/status
ros2 interface show tutorial_interfaces/msg/BoardStatus
```

## 4.5 colcon 常用指令

| 指令 | 用途 |
|------|------|
| `colcon build --symlink-install` | Python 與 launch 檔用符號連結，改完不用重建 |
| `colcon build --packages-select board_monitor` | 只建一個套件 |
| `colcon build --packages-up-to board_monitor` | 建它與它相依的全部套件 |
| `colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release` | 最佳化建置，部署到 Pi 時必加 |
| `colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` | 產生 `compile_commands.json` 給 clangd 或 VS Code |
| `colcon build --parallel-workers 2` | Pi 或 UNO Q 上限制平行數，避免記憶體耗盡 |
| `colcon test && colcon test-result --verbose` | 跑測試並看結果 |
| `colcon list --topological-order` | 看建置順序，檢查相依有沒有宣告對 |

在 Raspberry Pi 5 上，Debug 建置比 Release 慢約三倍且吃更多記憶體。UNO Q 的 2 GB 版本編 rclcpp 專案時，`--parallel-workers 1` 加上 swap 才不會被 OOM killer 殺掉。

## 4.6 引入第三方函式庫

三種常見情況：

**有 CMake config 的函式庫**（Eigen、OpenCV、PCL、yaml-cpp）：

```cmake
find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)
target_link_libraries(board_monitor_node PUBLIC Eigen3::Eigen ${OpenCV_LIBS})
```

**只有 pkg-config 的函式庫**（libgpiod、libserialport）：

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiodcxx)
target_link_libraries(board_monitor_node PUBLIC PkgConfig::GPIOD)
```

**自己的函式庫要給下游套件用**：在本套件建 library target 後，用 `ament_export_targets` 與 `ament_export_dependencies` 匯出，下游才能 `find_package` 到。範例見 [`board_monitor/CMakeLists.txt`](../examples/cmake/ros2_ws/src/board_monitor/CMakeLists.txt) 底部的註解區塊。

不論哪一種，`package.xml` 都要加對應的 `<depend>`，rosdep 才會在乾淨機器上把系統套件裝好。rosdep key 與 apt 套件名的對照在 [rosdistro](https://github.com/ros/rosdistro/blob/master/rosdep/base.yaml)。

## 4.7 測試

`ament_lint_auto` 會依 `package.xml` 的 `<test_depend>` 自動掛上 cpplint、uncrustify、xmllint 等檢查。單元測試用 gtest：

```cmake
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_board_status test/test_board_status.cpp)
  target_link_libraries(test_board_status ${tutorial_interfaces_TARGETS})
endif()
```

範例測試檔：[`examples/cmake/ros2_ws/src/board_monitor/test/test_board_status.cpp`](../examples/cmake/ros2_ws/src/board_monitor/test/test_board_status.cpp)。它不啟動 ROS，只驗證訊息型別的預設值與欄位，是最快的一種測試。

## 4.8 CMake 與 STM32

第 3 章用 STM32CubeIDE 的圖形介面設定專案。要納入 CI 或與 ROS 2 工作區統一管理時，改用 CMake 有兩個切入點。

### CubeMX 匯出 CMake 專案

CubeMX 6.11 以上，Project Manager → Toolchain/IDE 選 **CMake**，會產生：

```
<專案>/
├── CMakeLists.txt                  頂層，加入自己的原始檔在這裡
├── CMakePresets.json               Debug / Release preset
├── cmake/
│   ├── gcc-arm-none-eabi.cmake     toolchain file
│   └── stm32cubemx/CMakeLists.txt  HAL 與 FreeRTOS 的原始檔清單
└── Core/ Drivers/ Middlewares/
```

在頂層 `CMakeLists.txt` 加入 micro-ROS：

```cmake
# 第 3 章的應用程式碼
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  Core/Src/micro_ros_app.c
  micro_ros_stm32cubemx_utils/extra_sources/custom_memory_manager.c
  micro_ros_stm32cubemx_utils/extra_sources/microros_allocators.c
  micro_ros_stm32cubemx_utils/extra_sources/microros_time.c
  micro_ros_stm32cubemx_utils/extra_sources/microros_transports/dma_transport.c
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  micro_ros_stm32cubemx_utils/microros_static_library_ide/libmicroros/include
)
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
  micro_ros_stm32cubemx_utils/microros_static_library_ide/libmicroros
)
target_link_libraries(${CMAKE_PROJECT_NAME} microros)
```

命令列建置：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

### toolchain file 的角色

交叉編譯的核心是 toolchain file：它告訴 CMake「編譯器不是本機的 g++，而是 arm-none-eabi-gcc，目標是 Cortex-M4，不要嘗試執行編出來的程式」。範例：[`examples/stm32/cmake/arm-none-eabi-toolchain.cmake`](../examples/stm32/cmake/arm-none-eabi-toolchain.cmake)。CubeMX 產生的版本內容相近，可以直接用。

第 3 章用 Docker 建置 micro-ROS 靜態函式庫時，容器內做的事就是用 colcon 加一個類似的 toolchain file，把 rcl、rclc、Micro XRCE-DDS 等幾十個套件交叉編譯成一個 `libmicroros.a`。編譯旗標（`-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`）必須與韌體專案一致，否則連結時會出現 ABI 不相容的錯誤。

### 自訂訊息給 micro-ROS 用

4.3 節的 `tutorial_interfaces` 也能給 STM32 用。把整個套件目錄複製到：

```
micro_ros_stm32cubemx_utils/microros_static_library_ide/library_generation/extra_packages/tutorial_interfaces/
```

重新執行第 3 章的 Docker 建置指令，`libmicroros.a` 就會包含 `tutorial_interfaces/msg/board_status.h`。這是同一份 `.msg` 檔第三次被編譯：Python、C++、C 三種語言各一次，介面契約只維護一處。

## 4.9 交叉編譯到 Raspberry Pi 與 UNO Q

兩塊板子都是 arm64 Linux，理論上可以在 x86 筆電上交叉編譯後複製過去，但實務上 **不建議**：ROS 2 的相依鏈很深，交叉編譯環境要把整套 sysroot 同步過來，維護成本高。

建議做法依序為：

1. **直接在板上編譯。** Pi 5 編一個中型 C++ 套件約數分鐘，可以接受。
2. **在 x86 上用 arm64 Docker 編譯。** 透過 QEMU 執行 `arm64v8/ros:jazzy` 映像，速度慢但環境與板上一致。

```bash
docker run --rm --platform linux/arm64 -v "$PWD":/ws -w /ws ros:jazzy \
  bash -c "source /opt/ros/jazzy/setup.bash && colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release"
```

3. **CI 產出 Docker 映像後推到板上。** 第 1 章 UNO Q 的容器化方案天然適合這條路。

## 4.10 常見錯誤

| 錯誤訊息或症狀 | 原因 |
|----------------|------|
| `Could not find a package configuration file provided by "xxx"` | 沒 `source` ROS 環境，或 `package.xml` 漏寫相依導致 colcon 建置順序錯 |
| `ros2 run` 找不到執行檔 | `install(TARGETS ...)` 的 DESTINATION 不是 `lib/${PROJECT_NAME}` |
| 改了 `.msg` 但 C++ 看不到新欄位 | 訊息套件與使用它的套件在同一次 `colcon build` 被平行建置，重跑一次或用 `--packages-up-to` |
| 訂閱 STM32 的 topic 收不到 | QoS 不相容，改用 `SensorDataQoS()` 或明確設 best-effort |
| Pi 上 `colcon build` 中途被殺 | 記憶體不足，加 `--parallel-workers 1` 與 `MAKEFLAGS=-j1` |
| STM32 連結時 `error: ... uses VFP register arguments` | micro-ROS 函式庫與韌體的 `-mfloat-abi` 不一致 |

## 參考資料

- [ROS 2 Jazzy：Writing a simple publisher and subscriber (C++)](https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Writing-A-Simple-Cpp-Publisher-And-Subscriber.html)
- [ROS 2 Jazzy：Creating custom msg and srv files](https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Custom-ROS2-Interfaces.html)
- [ament_cmake 使用者文件](https://docs.ros.org/en/jazzy/How-To-Guides/Ament-CMake-Documentation.html)
- [colcon 文件](https://colcon.readthedocs.io/)
- [STM32CubeMX CMake 專案（ST 官方 wiki）](https://wiki.st.com/stm32mcu/wiki/Category:CMake)
