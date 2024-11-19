/****************************************************************************
*

MCU固件更新流程：

1. 下载固件到预存分区
2. 传输固件到外部MCU
3. 外部MCU校验固件，并写入MCU的Flash
4. 重启MCU

*
****************************************************************************/

// #include "esp_http_client.h"
// #include "esp_partition.h"
// #include "esp_log.h"
// //#include "spi_flash_mmap.h"
// #include "driver/uart.h"

// #define PARTITION_NAME "firmware"   //MCU固件预存分区，0x8E000 bits（568KB）
// #define CHUNK_SIZE 512  // 每个分片的大小
// static const char *TAG = "MCU_FIRMWARE";

// extern uint8_t calculate_checksum(const uint8_t *data, size_t length);


// /**
//  * @brief 下载固件到预存分区
//  * 
//  * @param url 固件下载地址
//  * @return 无
//  */
// void download_firmware_to_partition(const char *url) {
//     const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, PARTITION_NAME);
//     if (partition == NULL) {
//         ESP_LOGE(TAG, "Partition not found!");
//         return;
//     }

//     esp_http_client_config_t config = {
//         .url = url,
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     esp_err_t err = esp_http_client_open(client, 0);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
//         return;
//     }

//     int data_read;
//     char buffer[1024];
//     int offset = 0;

//     while ((data_read = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
//         if (offset + data_read > partition->size) {
//             ESP_LOGE(TAG, "Firmware size exceeds partition size!");
//             break;
//         }
//         esp_partition_write(partition, offset, buffer, data_read);
//         offset += data_read;
//     }

//     esp_http_client_close(client);
//     esp_http_client_cleanup(client);

//     ESP_LOGI(TAG, "Firmware downloaded and stored successfully.");
// }


// /**
//  * @brief 传输固件到外部MCU
//  * 
//  * @return 无
//  */
// void transfer_firmware_to_mcu() {
//     const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, PARTITION_NAME);
//     if (partition == NULL) {
//         ESP_LOGE(TAG, "Partition not found!");
//         return;
//     }

//     uint8_t buffer[CHUNK_SIZE];
//     int offset = 0;
//     int chunk_id = 0;

//     while (offset < partition->size) {
//         // 读取分片数据
//         int read_size = (partition->size - offset) > CHUNK_SIZE ? CHUNK_SIZE : (partition->size - offset);
//         esp_partition_read(partition, offset, buffer, read_size);

//         // 计算校验和
//         uint8_t checksum = calculate_checksum(buffer, read_size);

//         // 发送分片编号、数据、校验和（假设使用UART传输）
//         uart_write_bytes(UART_NUM_1, (const char *)&chunk_id, sizeof(chunk_id));  // 发送分片编号
//         uart_write_bytes(UART_NUM_1, (const char *)buffer, read_size);           // 发送分片数据
//         uart_write_bytes(UART_NUM_1, (const char *)&checksum, sizeof(checksum)); // 发送校验和

//         ESP_LOGI(TAG, "Sent chunk %d with checksum %02X", chunk_id, checksum);

//         // 等待外部MCU的ACK确认
//         uint8_t ack;
//         int len = uart_read_bytes(UART_NUM_1, &ack, 1, pdMS_TO_TICKS(1000));
//         if (len < 1 || ack != 0xA5) {  // 假设ACK值为0xA5
//             ESP_LOGE(TAG, "Failed to receive ACK for chunk %d", chunk_id);
//             break;
//         }

//         offset += read_size;
//         chunk_id++;
//     }

//     ESP_LOGI(TAG, "Firmware transfer to MCU completed.");
// }
