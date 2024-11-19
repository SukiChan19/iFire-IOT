
/****************************************************************************
*

WiFi连接模块

*
****************************************************************************/
#include <string.h>
#include "esp_bt.h"
#include "esp_err.h"
// #include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// #include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_bt_main.h"
#include "esp_coexist.h"
#include "driver/uart.h"

#include "gatts_table.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"

#include "ProtocolsChar.h"
#include "WiFi_Connect.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define DEFAULT_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define DEFAULT_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
//#define DEFAULT_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#define DEFAULT_ESP_MAXIMUM_RETRY  2 // 重连2次以测试

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;


static const char *TAG = "wifi station";

/**********************************************************/

/********** WiFi 配置结构体 *******************/
wifi_config_t wifi_config = {
    .sta = {
        .ssid = DEFAULT_ESP_WIFI_SSID,
        .password = DEFAULT_ESP_WIFI_PASS,
        .bssid_set = true,
        .bssid = {0},
        .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
        .sae_h2e_identifier = H2E_IDENTIFIER,
    },
};

/*******************************/

/** 
 * @brief WiFi事件处理函数
 * 
 * 该函数用于处理与WiFi连接相关的事件，包括连接开始、连接断开和成功获取IP地址。
 * 根据不同的事件类型执行相应的操作，如连接接入点、重试连接或获取IP时的状态更新。
 * 
 * @param arg：         传递给事件处理程序的参数（未在此段代码中使用）。
 * @param event_base：  事件的基础类型，指示事件的来源（此处为WiFi或IP事件）。
 * @param event_id：    具体事件的ID，用于进一步确定事件类型
 * @param event_data：  指向事件相关数据的指针。
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    uint8_t wrong_password_respones[] = WIFI_CONNECT_FAIL;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < DEFAULT_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {

            if (BLE_CONFIG_NET == DISABLE)
            {
                // 如果 BLE 配置网络未开启，则启用 BLE 配置网络
                ESP_LOGW(TAG, "Start BLE configuration network.");
                while (!WiFi_status_rsp)    
                {
                    //发送配置命令，等待MCU响应 {0x55,0xAA,0x00,0x03,0x00,0x01,0x01,0x04}
                    uart_write_bytes(UART_NUM_1, (const char*)"\x55\xAA\x00\x03\x00\x01\x01\x04", 8);
                    vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1s
                }
                WiFi_status_rsp = false;     // 清除WiFi连接状态响应标志位

                // 启用共存模式，设置为Wi-Fi优先级更高
                esp_err_t ret = esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Set coexistence preference failed: %s", esp_err_to_name(ret));
                }
                s_retry_num = 0;
                wifi_connect_status = CONNECT_FAILED;   // 更新连接失败状态
                BLE_CONFIG_NET = ENABLE;                // 开启 BLE 配置网络标志位
                gatts_profile_init();                   // 启动 BLE 配置网络
            }
            else
            {
                ESP_LOGW(TAG, "BLE configuration network failed, password error or timeout.");
                //notify ：WiFi连接失败
                if (ble_connected)
                {
                    esp_ble_gatts_send_indicate(G_gatts_if, conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                sizeof(wrong_password_respones), (uint8_t*)wrong_password_respones, false);
                } 
            }

            //xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
        wifi_connect_status = CONNECT_FAILED;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool create_default_wifi_sta = false;

/** 
 * @brief         保存WiFi配置到NVS
 * 
 * 该函数用于将WiFi配置保存到NVS存储空间，以便下次启动时读取。
 */
void save_wifi_config_to_nvs() {
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle)); // 打开NVS存储空间
    ESP_ERROR_CHECK(nvs_set_str(my_handle, "ssid", (char *)wifi_config.sta.ssid));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "bssid", wifi_config.sta.bssid, sizeof(wifi_config.sta.bssid)));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, "password", (char *)wifi_config.sta.password));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, "authmode", (uint8_t)wifi_config.sta.threshold.authmode));
    ESP_ERROR_CHECK(nvs_commit(my_handle)); // 提交写入
    nvs_close(my_handle);                   // 关闭存储空间
}


/**
 * @brief         从NVS读取WiFi配置
 * 
 * 该函数用于从NVS存储空间读取WiFi配置，以便连接到指定的WiFi网络。
 * 如果NVS中没有保存过WiFi配置，则使用默认的WiFi配置。
 * 读取到的WiFi配置会保存在全局变量wifi_config中。
 * 该函数返回读取到的WiFi配置是否成功的状态码。
 * 成功时返回ESP_OK，失败时返回对应的错误码。
 * 
 * @return        读取到的WiFi配置是否成功的状态码。
 * 成功时返回ESP_OK，失败时返回对应的错误码。
 */
esp_err_t read_wifi_config_from_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle); // 打开NVS存储空间
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not found, create default wifi config.");
        return err;
    }
    else {
        size_t ssid_len = 32;           // 假设SSID最大长度为32
        char ssid[32] = {0};            // 存储读取的SSID
        size_t bssid_len = 6;          // 假设BSSID最大长度为18
        char bssid[6] = {0};           // 存储读取的BSSID

        size_t password_len = 64;       // 假设密码最大长度为64
        char password[64] = {0};        // 存储读取的密码


        err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SSID read from NVS: %s", ssid);
            //将读取的SSID赋值到wifi_config中
            memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "SSID not found in NVS.");
            return err;
        } else {
            ESP_LOGE(TAG, "Error reading SSID from NVS: %s", esp_err_to_name(err));
        }

        err = nvs_get_blob(my_handle, "bssid", bssid, &bssid_len);
        if (err == ESP_OK) {
            esp_log_buffer_hex("wifi station: bssid", (void*)bssid, sizeof(bssid));
            //将读取的SSID赋值到wifi_config中
            memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "BSSID not found in NVS.");
            return err;
        } else {
            ESP_LOGE(TAG, "Error reading BSSID from NVS: %s", esp_err_to_name(err));
        }

        err = nvs_get_str(my_handle, "password", password, &password_len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Password read from NVS.");            
            //将读取的密码赋值到wifi_config中
            memcpy(wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Password not found in NVS.");
            return err;
        } else {
            ESP_LOGE(TAG, "Error reading password from NVS: %s", esp_err_to_name(err));
        }

        err = nvs_get_u8(my_handle, "authmode", (uint8_t*)&wifi_config.sta.threshold.authmode);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Authmode read from NVS: %d", wifi_config.sta.threshold.authmode);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Authmode not found in NVS.");
            return err;
        } else {
            ESP_LOGE(TAG, "Error reading Authmode from NVS: %s", esp_err_to_name(err));
        }

        nvs_close(my_handle); // 关闭存储空间

        return err;
    }
}


/**
 * @brief WiFi 连接管理函数
 * 
 * 该函数负责初始化和管理 WiFi STA 模式的连接，包括事件处理、状态监测以及重连机制。
 */
void wifi_sta(void){
    uint8_t connect_respones[] = WIFI_CONNECTED;

    // 创建 FreeRTOS 事件组，用于跟踪 WiFi 连接状态
    s_wifi_event_group = xEventGroupCreate();   

    // 初始化网络接口，如果尚未创建默认 WiFi STA 接口
    if (!create_default_wifi_sta){
        esp_netif_create_default_wifi_sta();
        create_default_wifi_sta = true;
    }
    
    // 初始化 WiFi 配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册 WiFi 事件和 IP 事件的处理程序
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    read_wifi_config_from_nvs();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    // 等待连接状态变化，直到成功连接或达到最大重试次数
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    // 检查连接状态
    if (bits & WIFI_CONNECTED_BIT) {
        s_retry_num = 0;     // 重连次数清零
        ESP_LOGI(TAG, "connected to ap SSID:%s",wifi_config.sta.ssid);
        esp_log_buffer_hex("wifi station: bssid", (void*)wifi_config.sta.bssid, sizeof(wifi_config.sta.bssid));
        if(BLE_CONFIG_NET == ENABLE){
            //notyfy：WiFi连接成功
            esp_ble_gatts_send_indicate(G_gatts_if, conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                            sizeof(connect_respones), (uint8_t*)connect_respones, false);
            vTaskDelay(pdMS_TO_TICKS(500)); //延时100ms
            // 如果 BLE 配置网络已启用，则禁用 BLE
            ESP_ERROR_CHECK(esp_bluedroid_disable());
            ESP_ERROR_CHECK(esp_bluedroid_deinit());
            ESP_ERROR_CHECK(esp_bt_controller_disable());
            ESP_ERROR_CHECK(esp_bt_controller_deinit());
            BLE_CONFIG_NET = DISABLE;
            ESP_LOGI(TAG, "BLE deinit and disable successfully.");
        }
        
        save_wifi_config_to_nvs();
        ESP_LOGI(TAG, "Save wifi config to NVS successfully.");
        wifi_connect_status = CONNECT_SUCCESS;  // 更新连接状态

        while (true) {                    //等待MCU响应
            // 发送连接成功消息到MCU    { 0x55, 0xAA, 0x00, 0x03, 0x00, 0x01, 0x03, 0x06 }
            uart_write_bytes(UART_NUM_1, (const char*)CONNECT_TO_WIFI, CONNECT_TO_WIFI_len);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1s
            if (WiFi_status_rsp){
                WiFi_status_rsp = false;
                break;
            }
            
        }
    } else {
        // 处理未知事件类型
        ESP_LOGE(TAG, "UNEXPECTED EVENT.");
    }
}

