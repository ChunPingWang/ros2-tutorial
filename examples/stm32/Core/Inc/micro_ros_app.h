#ifndef MICRO_ROS_APP_H
#define MICRO_ROS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * micro-ROS 應用程式進入點。
 * 在 CubeMX 產生的 StartDefaultTask() 內呼叫，此函式不會返回。
 */
void micro_ros_app_task(void);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_ROS_APP_H */
