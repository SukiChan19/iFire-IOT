/* 

WiFi扫描程序

*/



#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
// #include "esp_event.h"
// #include "nvs_flash.h"
// #include "regex.h"
#include "WiFi_Scan.h"

#define DEFAULT_SCAN_LIST_SIZE 15   // default number of APs to scan

#ifdef USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BTIMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/

/**************** Static Variables Declarations *******************/

static const char *TAG = "WiFi_Scan";

/*****************************************************************/



/*****************************************************************/


#ifdef USE_CHANNEL_BTIMAP
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config) {

    for(uint8_t i = 0; i < channel_list_size; i++) {
        uint8_t channel = channel_list[i];
        scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
    }
}
#endif /*USE_CHANNEL_BTIMAP*/



/* Initialize Wi-Fi as sta and set scan method */
void wifi_scan(void)
{

    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef USE_CHANNEL_BTIMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1,sizeof(wifi_scan_config_t));
    if (!scan_config) {
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);

#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BTIMAP*/

    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", found_ap_lists_number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&found_ap_lists_number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, found_ap_lists_number);
    for (int i = 0; i < found_ap_lists_number; i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        esp_log_buffer_hex("WiFi_Scan: BSSID \t\t", ap_info[i].bssid, 6);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        // 添加头部信息 0xAB:起始字节 0x01：版本号 0x02:传输ap_info指令
        uint8_t ap_lists_header[] = AP_LIST_INFO_header;
        memcpy(ap_lists_data[i], ap_lists_header, sizeof(ap_lists_header));
        /** 填充data数据长度 = 7 + BSSID长度 + SSID长度
        | bytes |     0       |      1        |      2      |       3        |      4        |       5        |      6      |      7        |     8     |
        | type  | rssi char   |rssi abs value | authmode    | pairwise_cipher| group_cipher  | primary_channel| BSSID       | SSID length   |   SSID    | 
        |length | 1 byte      | 1 byte        | 1 byte      |  1 byte        | 1 byte        |  1 byte        | 6 bytes     | 1 byte        |SSID length| 
        */
        ap_lists_data[i][3] = 13 + strlen((char*)ap_info[i].ssid);
        // 填充data数据
        ap_lists_data[i][4] = ap_info[i].rssi > 0 ? 0x01 : 0x00;      //0： 符号位，rssi值大于0，则为1，否则为0
        ap_lists_data[i][5] = abs(ap_info[i].rssi);                   //1： 绝对值rssi值
        ap_lists_data[i][6] = ap_info[i].authmode;                    //2： 认证方式
        ap_lists_data[i][7] = ap_info[i].pairwise_cipher;             //3： 对称加密算法
        ap_lists_data[i][8] = ap_info[i].group_cipher;                //4： 组加密算法
        ap_lists_data[i][9] = ap_info[i].primary;                     //5： 主信道
        memcpy(ap_lists_data[i] + 10, ap_info[i].bssid, 6);           //6： BSSID
        ap_lists_data[i][16] = strlen((char*)ap_info[i].ssid);        //7： SSID长度    SSID[32]
        memcpy(ap_lists_data[i] + 17, ap_info[i].ssid, ap_lists_data[i][16]); // SSID
        // 计算CRC校验码并填充
        uint8_t crc_input[100];
        memcpy(crc_input, ap_lists_data[i] + 2, 15 + ap_lists_data[i][16]); 
        ap_lists_data[i][17 + ap_lists_data[i][16]] = crc8(crc_input, 15 + ap_lists_data[i][16]); 
        ap_lists_data[i][18 + ap_lists_data[i][16]] = 0xAA ; // 尾部字节 0xAA
        ap_lists_data[i][19 + ap_lists_data[i][16]] = '\0' ; // 字符串结束符                  

        esp_log_buffer_hex(TAG, ap_lists_data[i], 19 + ap_lists_data[i][16]);
    }
    esp_wifi_scan_stop();
    esp_wifi_clear_ap_list();
}

/*
    The following is the list of all the possible authentication modes:
    认证方式authmode对应值：
    0 : < authenticate mode : open 
    1 : < authenticate mode : WEP 
    2 : < authenticate mode : WPA_PSK 
    3 : < authenticate mode : WPA2_PSK 
    4 : < authenticate mode : WPA_WPA2_PSK 
    5 : < authenticate mode : WiFi EAP security 
    6 : < authenticate mode : WPA3_PSK 
    7 : < authenticate mode : WPA2_WPA3_PSK 
    8 : < authenticate mode : WAPI_PSK 
    9 : < authenticate mode : OWE 
    10 : < authenticate mode : WPA3_ENT_SUITE_B_192_BIT 
    11 : < this authentication mode will yield same result as WIFI_AUTH_WPA3_PSK and not recommended to be used.
            It will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead. 
    12 : < this authentication mode will yield same result as WIFI_AUTH_WPA3_PSK and not recommended to be used. 
            It will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead.
    13 : < authenticate mode : DPP 
    14 : < maximum number of authentication modes 
 */


/*
    The following is the list of all the possible cipher types:
    加密算法cipher_type对应值：
    0 : < cipher type : none 
    1 : < cipher type : WEP40 
    2 : < cipher type : WEP104 
    3 : < cipher type : TKIP 
    4 : < cipher type : CCMP 
    5 : < cipher type : TKIP and CCMP 
    6 : < cipher type : AES-CMAC-128 
    7 : < cipher type : SMS4 
    8 : < cipher type : GCMP 
    9 : < cipher type : GCMP-256 
    10 : < cipher type : AES-GMAC-128 
    11 : < cipher type : AES-GMAC-256 
    12 : < cipher type : unknown
*/
