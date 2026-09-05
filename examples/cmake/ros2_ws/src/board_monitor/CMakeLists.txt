cmake_minimum_required(VERSION 3.8)
project(board_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- 相依 ------------------------------------------------------------------
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tutorial_interfaces REQUIRED)

# ---- 執行檔 ----------------------------------------------------------------
add_executable(board_monitor_node src/board_monitor_node.cpp)
target_compile_features(board_monitor_node PUBLIC cxx_std_17)

# Jazzy 建議直接連結 CMake target，不再使用 ament_target_dependencies()
target_link_libraries(board_monitor_node PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${tutorial_interfaces_TARGETS}
)

# ---- 安裝 ------------------------------------------------------------------
# ros2 run 只會到 lib/<pkg>/ 找執行檔
install(TARGETS board_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

# ros2 launch 只會到 share/<pkg>/ 找 launch 檔
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

# ---- 測試 ------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_board_status test/test_board_status.cpp)
  target_link_libraries(test_board_status ${tutorial_interfaces_TARGETS})
endif()

# ---- 若要把本套件的函式庫匯出給下游使用，改成下面的樣式 ---------------------
# add_library(board_monitor_lib src/board_monitor_lib.cpp)
# target_include_directories(board_monitor_lib PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#   $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
# target_link_libraries(board_monitor_lib PUBLIC rclcpp::rclcpp)
# install(TARGETS board_monitor_lib
#   EXPORT export_${PROJECT_NAME}
#   ARCHIVE DESTINATION lib
#   LIBRARY DESTINATION lib
#   RUNTIME DESTINATION bin)
# install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
# ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
# ament_export_dependencies(rclcpp)

# ament_package() 必須是最後一行
ament_package()
