/* MQTT Mutual Authentication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "mqtt_client.h"
#include "ProtocolsChar.h"

/********** 全局变量 ***************/
esp_mqtt_client_handle_t client = NULL;       // MQTT 客户端
uint8_t last_trace_id[4] = {0};               // MQTT 传输流水号
uint8_t command[2] = {0};                     // 命令字
bool need_rsp_flg = false;                    // 是否需要响应
bool CONNECTED_TO_CLOUD_FLAG = false;         // 是否连接到云端
int mqtt_disconnected_count = 0;              // MQTT 断开连接次数
uint8_t MQTT_CONNECT_STATE;                   // MQTT 连接状态

/********* 外部函数 ***************/
extern uint16_t crc16_reverse(uint8_t* data, size_t length);
extern void handle_nvs_erase();
// extern esp_err_t https_ota_update(const char *url);
// extern void download_firmware_to_partition(const char *url);

static const char *TAG = "mqtts_massage";     // 日志标签

/********** 内部函数声明 ************/
void handle_mqtt_msg(esp_mqtt_event_handle_t event_msg);
void rsp_ctrl_msg(uint8_t* ctrl_data, size_t data_len, size_t len);

/********** 外部变量 ***************/
extern uint8_t DevPID[16];                    // 设备 PID 
extern uint8_t MCU_SoftVer[3];                // 设备软件版本号
extern uint8_t Dev_Status;                     // 设备状态
extern app_state_t current_state;              // 当前状态

// SSL/TLS 证书相关
extern const uint8_t client_cert_pem_start[] asm("_binary_db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_certificate_pem_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_certificate_pem_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_private_pem_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_db611f9b28db2b68141d0945315bed82723ecb3bf02f8b45fcaf488f879169f3_private_pem_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_AmazonRootCA1_pem_end");
