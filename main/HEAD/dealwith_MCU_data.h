#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "stdbool.h"
#include "mqtt_client.h"
#include "ProtocolsChar.h"

// 心跳周期和状态查询周期配置
#define RUNNING_HEARTBEAT_PERIOD_S      60      // 工作中心跳周期（秒）
#define IDLE_HEARTBEAT_PERIOD_S         30      // 待机心跳周期（秒）
#define RUNNING_STATUS_CHECK_PERIOD_S   5       // 工作中状态查询周期（秒）
#define IDLE_STAUTS_CHECK_PERIOD_S      5       // 待机状态查询周期（秒）

const char* TAG = "Data_Trans";
const uint8_t rsp_wifi_status[7] = {0x55, 0xAA, 0x03, 0x03, 0x00, 0x00, 0x05}; // WiFi连接状态响应

/*********外部变量 *************/
extern esp_mqtt_client_handle_t client;     // MQTT句柄
extern uint8_t MCU_RESP_TOPIC[];            // MCU发布响应主题
extern uint8_t last_trace_id[4];            // MQTT传输流水号
extern uint8_t command[2];                  // 指令
extern bool need_rsp_flg;                   // 是否需要响应
extern app_state_t current_state;           // 状态机当前状态

// 设备信息
uint8_t DevPID[16] = {0};               // 设备PID（SN）
uint8_t DevName[4] = {0};               // 设备名称
uint8_t DevModel = 0;                   // 设备型号
uint8_t DevProgram = 0;                 // 软件版本号
uint8_t MCU_SoftVer[3] = {0};           // 设备固件版本号

/*********全局变量 *************/
uint8_t Dev_Status = SHUTDOWN;       // 设备状态
bool WiFi_status_rsp = false;        // WiFi连接状态响应标志位
bool wifi_reset_flg = false;         // WiFi重置标志位
bool get_product_info_flg = false;   // 接收到产品信息标志位
bool publish_data_flg = false;       // 发布数据标志位

static uint8_t publish_data[256] = {0};  // 发布数据缓存区
static bool Polling_Flag = false;        // 轮询标志位
int publish_data_len = 0;                // 发布数据长度

typedef struct {
    uint8_t data[256];
    size_t length;
} publish_data_t;

#define BUFFER_SIZE 20
publish_data_t publish_data_buf[BUFFER_SIZE];   // 发布数据缓存区

volatile size_t fifo_head = 0;      // 头索引
volatile size_t fifo_tail = 0;      // 尾索引

/*********外部函数 *************/
extern uint16_t crc16_reverse(uint8_t* data, size_t length);
extern uint8_t calculate_checksum(const uint8_t *data, size_t length);
extern bool validate_checksum(const uint8_t *mcu_data, size_t length);
extern QueueHandle_t uart0_queue;

/*********函数声明 *************/
void handle_MCU_data(uint8_t *mcu_sent_data, size_t len);
void publish_to_mqtt(uint8_t *data, size_t size);
void enqueue_uart_data(uint8_t *data, size_t length);
void dequeue_uart_data(publish_data_t *data);
bool is_fifo_empty(void);
bool is_fifo_full(void);
void publish_cached_data(void);

// 定时器标志位
static bool xTimer1_flg = false;        // 心跳检测，定时器1标志位
static bool Hearbeat_flg = false;       // 心跳检测标志位
static bool status_check_flg = false;   // 状态查询标志位
static bool xTimer3_flg = false;        // 超时响应，定时器3标志位
TimerHandle_t xTimer1 = NULL;           // 心跳检测定时器
TimerHandle_t xTimer3 = NULL;           // 超时响应定时器
