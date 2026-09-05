/*
 * micro-ROS 應用程式（STM32 + FreeRTOS + USART2 DMA）
 *
 *   發布 /stm32/heartbeat (std_msgs/Int32)  每 100 ms 遞增
 *   訂閱 /stm32/led       (std_msgs/Bool)   控制 PA5 (LD2)
 *
 * 前置條件：
 *   - CubeMX 已設定 USART2 + DMA (RX Circular)，FreeRTOS CMSIS_V2
 *   - 專案已引入 micro_ros_stm32cubemx_utils，extra_sources 只保留 dma_transport.c
 *   - defaultTask stack >= 3000 words，FreeRTOS heap >= 30 KB
 */

#include "micro_ros_app.h"

#include "main.h"
#include "cmsis_os.h"

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <uxr/client/transport.h>

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/int32.h>

/* --- 由 micro_ros_stm32cubemx_utils/extra_sources 提供 ------------------ */
extern UART_HandleTypeDef huart2;

bool   cubemx_transport_open(struct uxrCustomTransport *transport);
bool   cubemx_transport_close(struct uxrCustomTransport *transport);
size_t cubemx_transport_write(struct uxrCustomTransport *transport, const uint8_t *buf, size_t len, uint8_t *err);
size_t cubemx_transport_read(struct uxrCustomTransport *transport, uint8_t *buf, size_t len, int timeout, uint8_t *err);

void *microros_allocate(size_t size, void *state);
void  microros_deallocate(void *pointer, void *state);
void *microros_reallocate(void *pointer, size_t size, void *state);
void *microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void *state);

/* --- 狀態 --------------------------------------------------------------- */
#define NODE_NAME        "stm32_node"
#define HEARTBEAT_TOPIC  "/stm32/heartbeat"
#define LED_TOPIC        "/stm32/led"
#define HEARTBEAT_MS     100
#define EXECUTOR_HANDLES 2   /* 1 timer + 1 subscription */

static rcl_allocator_t     allocator;
static rclc_support_t      support;
static rcl_node_t          node;
static rcl_publisher_t     heartbeat_pub;
static rcl_subscription_t  led_sub;
static rcl_timer_t         heartbeat_timer;
static rclc_executor_t     executor;

static std_msgs__msg__Int32 heartbeat_msg;
static std_msgs__msg__Bool  led_msg;

typedef enum {
    STATE_WAITING_AGENT,
    STATE_AGENT_AVAILABLE,
    STATE_AGENT_CONNECTED,
    STATE_AGENT_DISCONNECTED
} app_state_t;

/* --- Callbacks ---------------------------------------------------------- */
static void heartbeat_cb(rcl_timer_t *timer, int64_t last_call_time)
{
    (void)last_call_time;
    if (timer == NULL) {
        return;
    }
    heartbeat_msg.data++;
    (void)rcl_publish(&heartbeat_pub, &heartbeat_msg, NULL);
}

static void led_cb(const void *msgin)
{
    const std_msgs__msg__Bool *msg = (const std_msgs__msg__Bool *)msgin;
    /* LD2_GPIO_Port / LD2_Pin 由 CubeMX 依 PA5 的 User Label 產生 */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, msg->data ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* --- 建立 / 銷毀 ROS 實體 ------------------------------------------------ */
static bool create_entities(void)
{
    allocator = rcl_get_default_allocator();

    if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
        return false;
    }
    if (rclc_node_init_default(&node, NODE_NAME, "", &support) != RCL_RET_OK) {
        return false;
    }
    if (rclc_publisher_init_default(
            &heartbeat_pub, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
            HEARTBEAT_TOPIC) != RCL_RET_OK) {
        return false;
    }
    if (rclc_subscription_init_default(
            &led_sub, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
            LED_TOPIC) != RCL_RET_OK) {
        return false;
    }
    if (rclc_timer_init_default2(
            &heartbeat_timer, &support,
            RCL_MS_TO_NS(HEARTBEAT_MS), heartbeat_cb, true) != RCL_RET_OK) {
        return false;
    }

    executor = rclc_executor_get_zero_initialized_executor();
    if (rclc_executor_init(&executor, &support.context, EXECUTOR_HANDLES, &allocator) != RCL_RET_OK) {
        return false;
    }
    if (rclc_executor_add_timer(&executor, &heartbeat_timer) != RCL_RET_OK) {
        return false;
    }
    if (rclc_executor_add_subscription(
            &executor, &led_sub, &led_msg, &led_cb, ON_NEW_DATA) != RCL_RET_OK) {
        return false;
    }

    heartbeat_msg.data = 0;

    /* 與 Agent 對時，之後可用 rmw_uros_epoch_millis() 取得主機時間 */
    (void)rmw_uros_sync_session(1000);

    return true;
}

static void destroy_entities(void)
{
    rmw_context_t *rmw_context = rcl_context_get_rmw_context(&support.context);
    (void)rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

    (void)rclc_executor_fini(&executor);
    (void)rcl_timer_fini(&heartbeat_timer);
    (void)rcl_subscription_fini(&led_sub, &node);
    (void)rcl_publisher_fini(&heartbeat_pub, &node);
    (void)rcl_node_fini(&node);
    (void)rclc_support_fini(&support);
}

/* --- 進入點 ------------------------------------------------------------- */
void micro_ros_app_task(void)
{
    /* 1. 傳輸層：USART2 + DMA，framing 開啟（序列傳輸必須） */
    rmw_uros_set_custom_transport(
        true,
        (void *)&huart2,
        cubemx_transport_open,
        cubemx_transport_close,
        cubemx_transport_write,
        cubemx_transport_read);

    /* 2. 記憶體配置改用 FreeRTOS heap */
    rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
    freeRTOS_allocator.allocate      = microros_allocate;
    freeRTOS_allocator.deallocate    = microros_deallocate;
    freeRTOS_allocator.reallocate    = microros_reallocate;
    freeRTOS_allocator.zero_allocate = microros_zero_allocate;
    if (!rcutils_set_default_allocator(&freeRTOS_allocator)) {
        Error_Handler();
    }

    /* 3. 狀態機：等待 Agent -> 連線 -> 斷線重連 */
    app_state_t state = STATE_WAITING_AGENT;

    for (;;) {
        switch (state) {
        case STATE_WAITING_AGENT:
            if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) {
                state = STATE_AGENT_AVAILABLE;
            } else {
                osDelay(500);
            }
            break;

        case STATE_AGENT_AVAILABLE:
            if (create_entities()) {
                state = STATE_AGENT_CONNECTED;
            } else {
                destroy_entities();
                state = STATE_WAITING_AGENT;
            }
            break;

        case STATE_AGENT_CONNECTED: {
            /* 每 200 ms 檢查一次 Agent 是否還在 */
            static uint32_t last_ping_tick = 0;
            uint32_t now = osKernelGetTickCount();
            if ((now - last_ping_tick) > pdMS_TO_TICKS(200)) {
                last_ping_tick = now;
                if (rmw_uros_ping_agent(50, 3) != RMW_RET_OK) {
                    state = STATE_AGENT_DISCONNECTED;
                    break;
                }
            }
            (void)rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
            osDelay(1);
            break;
        }

        case STATE_AGENT_DISCONNECTED:
            destroy_entities();
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); /* 安全狀態 */
            state = STATE_WAITING_AGENT;
            break;
        }
    }
}
