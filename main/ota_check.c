// /****************************************************************************
// *
// ESP32 版本号检查及OTA升级
// *
// ****************************************************************************/
// #include "esp_log.h"
// #include "esp_system.h"
// #include "esp_ota_ops.h"
// #include "nvs_flash.h"
// #include <string.h>
// //#include "esp_https_ota.h"


// #define TAG "OTA_CHECK"

// /**
//  * @brief 检查版本号，如果最新版本号和当前版本号不同，则进行OTA升级
//  * 
//  */
// void check_version() {
//     nvs_handle_t my_handle;
//     esp_err_t err = nvs_open("otadata", NVS_READONLY, &my_handle); // 打开NVS存储空间
//     if (err == ESP_OK) {
//         char last_version[32] = {0};
//         size_t len = 2;
//         err = nvs_get_str(my_handle, "last_version", last_version, &len); // 读取最新版本号
//         if (err == ESP_OK) {
//             const esp_app_desc_t* app_desc = esp_app_get_description();
//             const char* version = app_desc->version; // 获取当前版本号
//             if (strcmp(last_version, version) != 0) { // 如果最新版本号和当前版本号不同，则进行OTA升级
//                 ESP_LOGW(TAG, "New version found: %s", version);
//                 const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
//                 if (next_partition != NULL) {
//                     ESP_ERROR_CHECK(esp_ota_set_boot_partition(next_partition));
//                     ESP_LOGI(TAG, "Set boot partition to partition type: %d, subtype: %d", next_partition->type, next_partition->subtype);
//                 }
//             } else {
//                 ESP_LOGI(TAG, "Current version is the latest: %s", version);
//             }
//         } else {
//             ESP_LOGE(TAG, "Failed to read Last version from NVS");
//         }
//         nvs_close(my_handle); // 关闭NVS句柄
//     } else {
//         ESP_LOGE(TAG, "Failed to open NVS handle");
//     }
// }

// esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
//     switch (evt->event_id) {
//         case HTTP_EVENT_ERROR:
//             ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
//             break;
//         case HTTP_EVENT_ON_CONNECTED:
//             ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
//             break;
//         case HTTP_EVENT_ON_DATA:
//             // 这里可以处理用于固件接收的数据
//             break;
//         case HTTP_EVENT_HEADERS_SENT:
//             ESP_LOGI(TAG, "HTTP_EVENT_HEADERS_SENT");
//             break;
//         case HTTP_EVENT_ON_HEADER:
//             ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
//             break;
//         case HTTP_EVENT_REDIRECT:
//             ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
//             break;
//         default:
//             ESP_LOGW(TAG, "Unhandled event: %d", evt->event_id);
//             break;
//     }
// }

// // 示例OTA升级函数
// /**
//  * @brief OTA升级函数
//  * 
//  * @param url 固件URL    
//  * @param cert_pem 证书pem文件路径
//  * @return ESP_OK 升级成功,others 升级失败,错误码
//  */
// esp_err_t https_ota_update(const char *url, const char *cert_pem) {
//     esp_http_client_config_t config = {
//         .url = url,
//         .event_handler = _http_event_handler,
//         .cert_pem = cert_pem,  // 如果使用HTTPS且使用了证书，则在这里指定证书
//         .disable_auto_redirect = false,
//     };

//     esp_https_ota_config_t ota_config = {
//         .http_config = &config,
        
//     };

//     esp_err_t err = esp_https_ota(&ota_config);
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG, "OTA Succeed. Rebooting...");
//         esp_restart();
//     } else {
//         ESP_LOGE(TAG, "OTA Failed");
//     }
// }
