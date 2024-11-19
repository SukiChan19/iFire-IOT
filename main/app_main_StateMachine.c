#include <reent.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
// #include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
// #include "lwip/sockets.h"
// #include "lwip/dns.h"
// #include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/uart.h"
//#include "ProtocolsChar.h"
#include "esp_ota_ops.h"
#include "app_main_StateMachine.h"

/**
 * @brief 主函数
 * 
 * 该函数用于实现整个程序的主循环，包括初始化、状态机、任务延时等
 */
void app_main(void)
{
    while (true) {
        state_machine(); // 调用状态机
        vTaskDelay(pdMS_TO_TICKS(200)); // 延时200ms
        // size_t free_heap = esp_get_free_heap_size();
        // ESP_LOGE(TAG, "Free heap size: %d bytes", free_heap);    // 打印空闲堆栈大小
    }
}

/**
 * @brief 状态机函数
 * 
 * 该函数用于实现状态机，根据当前状态执行相应的操作
 */
void state_machine(void) {

    switch (current_state) {
        case STATE_INIT: 
            app_init(); // 初始化
            current_state = STATE_WIFI_CONNECT; // 进入WiFi连接状态

            break;

        case STATE_WIFI_CONNECT:
            ESP_LOGI(TAG, "Entering WiFi connect state");
            wifi_sta();
            if (wifi_connect_status == CONNECT_SUCCESS) {
                current_state = STATE_MQTT_CONNECT; // 连接成功，进入MQTT连接状态
                client = NULL; // 清空MQTT句柄
                ESP_LOGI(TAG, "Entering MQTT connect state");
            } else {
                ESP_LOGW(TAG, "WiFi connect failed, retrying...");
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;

        case STATE_MQTT_CONNECT:
            ESP_LOGI(TAG, "Entering MQTT connect state");
            if (!client) {
                ESP_LOGI(TAG, "Initializing MQTT...");
                mqtt_init(); // 初始化MQTT
            }

            if (wifi_connect_status == CONNECT_FAILED) {
                handle_wifi_disconnection();
                break;
            }

            if (!MQTT_CONNECT_STATE) {
                ESP_LOGI(TAG, "Connecting to MQTT server...");
                esp_mqtt_client_reconnect(client); // 重新连接MQTT
                vTaskDelay(pdMS_TO_TICKS(200));
            } else {   
                
                if(CONNECTED_TO_CLOUD_FLAG){
                    handle_cloud_connection();
                }
            }
            break;

        case STATE_WORKING:
            handle_MQTT_publish(); // 处理MQTT发布


            if (wifi_connect_status == CONNECT_FAILED) {
                ESP_LOGI(TAG, "WiFi disconnected, trying to reconnect...");
                current_state = STATE_WIFI_CONNECT; // 重新尝试WiFi连接 wifi_disconnect = {0x55,0xAA,0x00,0x03,0x00,0x01,0x02,0x05}
                uart_write_bytes(UART_NUM_1, (const char*)WIFI_DISCONNECTED, WIFI_DISCONNECTED_len ); 
                ESP_LOGI(TAG, "Entering WiFi connect state");
            }
            else if(MQTT_CONNECT_STATE == false) {
                ESP_LOGI(TAG, "MQTT disconnected, trying to reconnect...");
                //current_state = STATE_MQTT_CONNECT; // 重新尝试MQTT连接
                uart_write_bytes(UART_NUM_1, (const char*)WIFI_DISCONNECTED, WIFI_DISCONNECTED_len);  
                ESP_LOGI(TAG, "Entering MQTT connect state");
                if(mqtt_disconnected_count < MQTT_RECONNECT_TIMES){
                    mqtt_disconnected_count++;
                    esp_mqtt_client_reconnect(client); // 重新连接MQTT
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                else if(mqtt_disconnected_count >= MQTT_RECONNECT_TIMES){    // 断开重连次数达到最大值，停止重连
                    ESP_LOGI(TAG, "MQTT disconnected, Stop MQTT...");
                    mqtt_disconnected_count = 0;
                    if(client){
                        esp_mqtt_client_stop(client); 
                        esp_mqtt_client_destroy(client); 
                        client = NULL; // 清空MQTT句柄
                    }else{
                        ESP_LOGI(TAG, "MQTT client is already NULL!");
                    }
                    vTaskDelay(pdMS_TO_TICKS(200));
                    current_state = STATE_WIFI_CONNECT; // 重新尝试WiFi连接
                }
            }

            if(!is_wifi_connected()){
                ESP_LOGI(TAG, "WiFi disconnected, Stop MQTT...");
                esp_mqtt_client_stop(client); // 停止MQTT

                uart_write_bytes(UART_NUM_1, (const char*)WIFI_DISCONNECTED, WIFI_DISCONNECTED_len); 

                current_state = STATE_WIFI_CONNECT; // 重新尝试WiFi连接
                ESP_LOGI(TAG, "Entering WiFi connect state");
            }

            if (wifi_reset_flg){
                    handle_nvs_erase(); // 调用NVS擦除函数清除ssid和password信息
                    esp_wifi_stop();
                    esp_wifi_deinit();  // 停止WiFi并释放资源
                    vTaskDelay(pdMS_TO_TICKS(100)); // 延迟 100 毫秒
                    esp_restart();      // 重启esp32
            }
            
            break;
    }
}

/**
 * @brief 处理WiFi断开连接
 * 
 * @param sent_wifi_disconnect 发送的断开连接信息到MCU
 */
void handle_wifi_disconnection() {
    ESP_LOGI(TAG, "WiFi disconnected, trying to reconnect...");
    current_state = STATE_WIFI_CONNECT; // 重新尝试WiFi连接
    uart_write_bytes(UART_NUM_1, (const char*)WIFI_DISCONNECTED, WIFI_DISCONNECTED_len); 
    ESP_LOGI(TAG, "Entering WiFi connect state");
}

/**
 * @brief 处理云连接
 */
void handle_cloud_connection() {
    while (true) {
        // 发送连接到云端信息给MCU {0x55,0xAA,0x00,0x03,0x00,0x01,0x04,0x07}
        uart_write_bytes(UART_NUM_1, (const char*)CONNECTED_TO_CLOUD, CONNECTED_TO_CLOUD_len);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1s  
        if (WiFi_status_rsp) {
            WiFi_status_rsp = false;
            current_state = STATE_WORKING; // 连接成功，进入工作状态 
                ESP_LOGI(TAG, "Entering Working state");
            break;
        }
    }
}



/**
 * @brief 初始化函数
 * 
 * 该函数用于初始化系统，包括初始化NVS、网络接口、事件循环、UART等
 */
void app_init(void){
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }


    // 获取当前应用的描述信息
    const esp_app_desc_t* app_desc = esp_app_get_description();
    const char* version = app_desc->version; // 获取版本号
    memcpy(ESP32_SoftVer, version, 16);      // 保存软件版本号
    ESP_LOGI(TAG, "ESP32 software version: %s", ESP32_SoftVer);
    ESP_LOGI(TAG, "ESP32 software version length: %d", strlen(version));
    // 将版本号写入NVS
    nvs_handle_t my_handle;
    // 打开NVS存储空间
    ret = nvs_open("otadata", NVS_READWRITE, &my_handle); 
    if (ret == ESP_OK) {
        ret = nvs_set_str(my_handle, "last_version", version); // 写入最新版本号
        if (ret == ESP_OK) {
            ret = nvs_commit(my_handle); // 提交写入
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Last Version number written to NVS: %s", version);
            } else {
                ESP_LOGE(TAG, "Failed to commit  Last version to NVS");
            }
        } else {
            ESP_LOGE(TAG, "Failed to write Lastversion to NVS");
        }
        nvs_close(my_handle); // 关闭NVS句柄
    } else {
        ESP_LOGE(TAG, "Failed to open NVS handle");
    }
    // 初始化网络接口
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // UART初始化
    uart_init(); 
    vTaskDelay(pdMS_TO_TICKS(100)); // 延时100ms
    // 获取产品信息
    while (!get_product_info_flg) {
        uart_write_bytes(UART_NUM_1, (const char*)GET_PRODUCT_INFO, GET_PRODUCT_INFO_len); 
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1s
    }
    ESP_LOGI(TAG, "Product Info Received");
    get_product_info_flg = false; // 产品信息获取成功，清除标志位
}


/**
 * @brief 检查WiFi连接状态WiFi
 * 
 * 该函数用于检查WiFi连接状态，返回true表示已连接，false表示未连接
 * 
 * @return true 表示已连接，false 表示未连接
 * 
 */
bool is_wifi_connected(void) {
    wifi_ap_record_t ap_info;
    // 获取当前连接的AP信息
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    return (ret == ESP_OK); // 如果返回ESP_OK，则WiFi连接正常
}

/**
 * @brief 校验和对比验证
 * 
 * 该函数计算校验和并与接收到的校验和进行比较
 * 
 * @param mcu_data 接收到的MCU数据
 * @param length 数据长度
 * @return true 如果校验和有效
 * @return false 如果校验和无效
 */
bool validate_checksum(const uint8_t *mcu_data, size_t length) {
    uint8_t checksum = calculate_checksum(mcu_data, length - 1); // 计算校验和 
    if (checksum != mcu_data[length - 1]) {
        ESP_LOGW(TAG, "Checksum validation failed, received: 0x%02x, calculated: 0x%02x", mcu_data[length - 1], checksum);
        return false; // 校验和无效
    }
    return true; // 校验和有效
}


/**
 * @brief 处理NVS擦除指令
 * 
 * 该函数用于擦除NVS存储区中的ssid和password信息
 */
void handle_nvs_erase(void) {
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    err |= nvs_erase_key(my_handle, "ssid");
    err |= nvs_erase_key(my_handle, "bssid");
    err |= nvs_erase_key(my_handle, "password");
    err |= nvs_erase_key(my_handle, "authmode");
    err |= nvs_commit(my_handle);
    nvs_close(my_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Erase ssid, password and authmode from NVS success");
    } else {
        ESP_LOGE(TAG, "Erase NVS failed");
    }
}

/**
 * EXAMPLE 产品信息回复：
 * {0x55,0xAA,0x03,0x01,0x00,0x19,
 * 0x77,0x74,0x71,0x6F,0x66,0x6C,0x7A,0x62,0x63,0x7A,0x69,0x63,0x6E,0x35,0x68,0x76,
 * 0x30,0x31,0x32,0x33,
 * 0x01，
 * 0x01，
 * 0x01,0x00,0x00,
 * 0x88}
 * 
 * {0x55,0xAA,0x03,0x01,0x00,0x19,
 * ----------帧头-----------，数据长度=16 + 4 + 1 + 1 + 3  = 25 = 0x19，
 *  0x77,0x74,0x71,0x6F,0x66,0x6C,0x7A,0x62,0x63,0x7A,0x69,0x63,0x6E,0x35,0x68,0x76,
 *  产品ID:w  t  q  o  f  i  z  b  c  z  i  c  n  5  h  v，
 * 
 * 0x30,0x31,0x32,0x33,
 * DevName_len = 4，设备名称:0123，
 * 
 * 0x01
 * 设备型号:0x01,
 * 
 * 0x01
 * 设备程序号:0x01,
 * 
 * 0x01,0x00,0x00
 * 软件版本号:1.0.0，
 * 0x88
 * 校验和}    
*/
