#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "ProtocolsChar.h"

// WiFi配置
#define DEFAULT_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define DEFAULT_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

// MQTT重连次数
#define MQTT_RECONNECT_TIMES      5 

static const char *TAG = "MAIN"; // 日志标签

// 状态机当前状态
app_state_t current_state = STATE_INIT; 

// ESP32软件版本号
uint8_t ESP32_SoftVer[16] = {0};   

/***************外部函数声明***************/ 
extern void wifi_sta();
extern void mqtt_init();
extern void uart_init();
extern uint16_t crc16_reverse(uint8_t* data, size_t length);
extern uint8_t calculate_checksum(const uint8_t *data, size_t length);
extern void handle_MQTT_publish(void);

/***************外部变量声明***************/ 
extern uint8_t wifi_connect_status; // 连接状态
extern esp_mqtt_client_handle_t client; // MQTT句柄
extern uint8_t MQTT_CONNECT_STATE; // MQTT连接状态
extern wifi_config_t wifi_config; // WiFi配置信息
extern bool CONNECTED_TO_CLOUD_FLAG; // 连接到云端标志位
extern const uint8_t connected_mqtt[8];
extern const uint8_t rsp_wifi_status[7];

extern bool WiFi_status_rsp;
extern bool wifi_reset_flg;         // WiFi重置标志位
extern bool get_product_info_flg;   // 获取产品信息标志位
extern int mqtt_disconnected_count;
uint8_t Sent_Product_Info[100] = {0}; // 发送的产品信息帧

/***************函数声明**************/ 
void app_init(void);
void handle_nvs_erase(void);
void handle_wifi_disconnection(void);
void handle_cloud_connection();
void state_machine(void);
bool is_wifi_connected(void);
