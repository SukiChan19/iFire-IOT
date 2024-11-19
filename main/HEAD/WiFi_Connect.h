/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#include <string.h>
#include "esp_bt.h"
#include "esp_err.h"

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
#include "esp_wifi.h"
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
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

enum{
    DISABLE,
    ENABLE,
};

/********************** BLE 配置相关函数 *******************/

extern void gatts_profile_init(void);
extern bool ble_connected;

/**********************************************************/

/**********外部变量声明**********/ 
extern esp_gatt_if_t G_gatts_if; // 记录 GATT 接口
extern uint16_t conn_id; // 记录连接 ID
extern uint16_t heart_rate_handle_table[HRS_IDX_NB];
extern uint8_t MQTT_CONNECT_STATE; //MQTT连接状态
extern bool WiFi_status_rsp;


/**********全局变量声明**********/ 

uint8_t wifi_connect_status = CONNECT_FAILED;   //wifi_connect_status 0: CONNECT_SUCCESS, 1: CONNECT_FAILED
uint8_t ConfigNetworkFailed = true;             // BLE 配置网络失败标志位，0: BLE 配置网络成功，1: BLE 配置网络失败

/*******************************/ 

/**********局部变量声明**********/ 

static int s_retry_num = 0;                     // 重连次数 
static bool BLE_CONFIG_NET = DISABLE;           // BLE 配置网络标志位，0: BLE 配置网络未开启，1: BLE 配置网络已开启
