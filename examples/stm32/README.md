# STM32 micro-ROS 範例

此資料夾只包含應用層程式碼，不含 CubeMX 產生的初始化檔案（`main.c`、`stm32f4xx_hal_conf.h` 等），因為那些檔案依板子與 CubeMX 版本而異。

使用方式：

1. 依 [第 3 章 3.4 節](../../docs/03-stm32-micro-ros.md#34-cubemx-專案設定) 建立 CubeMX 專案。
2. 依 3.5 節引入 `micro_ros_stm32cubemx_utils` 並建置靜態函式庫。
3. 把 `Core/Inc/micro_ros_app.h` 與 `Core/Src/micro_ros_app.c` 複製到專案對應資料夾。
4. 在 `main.c` 的 `StartDefaultTask()` 內呼叫 `micro_ros_app_task()`。

測試板：NUCLEO-F446RE。其他 STM32 只需改 `huart2` 與 `LD2_*` 的名稱。
