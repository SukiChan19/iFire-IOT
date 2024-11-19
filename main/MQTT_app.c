
/* MQTT Mutual Authentication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
// #include "nvs_flash.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "protocol_examples_common.h"

// #include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// #include "lwip/sockets.h"
// #include "lwip/dns.h"
// #include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "ProtocolsChar.h"
#include "driver/uart.h"
#include "MQTT_app.h"

/* MQTT 发布控制指令主题*/
uint8_t CLOUD_CTRL_TOPIC[] = {'M','C','U','/',        //替换为16位设备序列号
                            'M','C','U','S','N','x','x','x','x','x','x','x','x','x','x','x','/',    //替换为16位设备序列号
                            'C','o','n','t','r','o','l','_','t','e','s','t','\0'
                            };

/* MQTT 发布 OTA 升级主题*/
uint8_t OTA_UPDATE_TOPIC[] = {'M','C','U','/',        //替换为16位设备序列号
                            'M','C','U','S','N','x','x','x','x','x','x','x','x','x','x','x','/',      //替换为16位设备序列号
                            'O','t','a','_','t','e','s','t','\0'
                            };

/* MCU 发布状态响应主题*/                          
uint8_t MCU_RESP_TOPIC[]   = {'M','C','U','/',        //替换为16位设备序列号
                            'M','C','U','S','N','x','x','x','x','x','x','x','x','x','x','x','/',    //替换为16位设备序列号
                            'S','t','a','t','u','s','_','t','e','s','t','\0'
                            };

/**
 * @brief 打印错误信息
 *
 *  如果错误代码不为零，则打印错误信息。
 *
 * @param message 错误信息的描述。
 * @param error_code 错误代码。
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/** 
 * @brief 注册的事件处理函数，用于接收 MQTT 事件
 *
 *  此函数由 MQTT 客户端事件循环调用。
 *
 * @param handler_args 注册到事件的用户数据。
 * @param base 事件处理程序的基础（在此示例中始终为 MQTT Base）。
 * @param event_id 接收到的事件的 ID。
 * @param event_data 事件的数据，类型为 esp_mqtt_event_handle_t。
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;

    //static int msg_id;
    static int ctrl_msg_id = 0;    //发布控制指令的msg_id
    static int ota_msg_id = 0;     //发布ota升级指令的msg_id
    static bool connect_to_CTRL_flg = false;    //是否连接到控制主题
    static bool connect_to_OTA_flg = false;     //是否连接到OTA主题

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:           //连接成功并订阅主题
        
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        MQTT_CONNECT_STATE = true;
        mqtt_disconnected_count = 0;

        memcpy(CLOUD_CTRL_TOPIC + 4,DevPID, 16);
        memcpy(OTA_UPDATE_TOPIC + 4,DevPID, 16);
        memcpy(MCU_RESP_TOPIC + 4,DevPID, 16);

        ctrl_msg_id = esp_mqtt_client_subscribe_single(client,(const char*)CLOUD_CTRL_TOPIC,1);
        ESP_LOGI(TAG, "sent subscribe  %s successful, ctrl_msg_id=%d", (const char*)CLOUD_CTRL_TOPIC, ctrl_msg_id);

        ota_msg_id = esp_mqtt_client_subscribe_single(client,(const char*)OTA_UPDATE_TOPIC,1);
        ESP_LOGI(TAG, "sent subscribe  %s successful, msg_id=%d", (const char*)OTA_UPDATE_TOPIC, ota_msg_id);
        //printf("MCU_RESP_TOPIC:%s\n", (const char*)MCU_RESP_TOPIC);

        break;
    case MQTT_EVENT_DISCONNECTED:        //断开连接
        ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
        CONNECTED_TO_CLOUD_FLAG = false;
        MQTT_CONNECT_STATE = false;
        mqtt_disconnected_count++;
        break;

    case MQTT_EVENT_SUBSCRIBED:         //订阅成功并发送消息
        if (event->msg_id == ctrl_msg_id){
            connect_to_CTRL_flg = true;
            ESP_LOGI(TAG, "SUBSCRIBED CTRL_TOPIC successful, msg_id=%d", event->msg_id);
        }
        if (event->msg_id == ota_msg_id){
            connect_to_OTA_flg = true;
            ESP_LOGI(TAG, "SUBSCRIBED OTA_TOPIC successful, msg_id=%d", event->msg_id);
        }
        
        if (connect_to_CTRL_flg && connect_to_OTA_flg){
            CONNECTED_TO_CLOUD_FLAG = true;
            ESP_LOGI(TAG, "CONNECTED_TO_CLOUD_FLAG = true");
            //发送连接到云端信号到MCU   {0x55,0xAA,0x00,0x03,0x00,0x01,0x04,0x07}
            uart_write_bytes(UART_NUM_1, (const char*)CONNECTED_TO_CLOUD, CONNECTED_TO_CLOUD_len);

        }
        
        break;

    case MQTT_EVENT_UNSUBSCRIBED:       //取消订阅成功
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);       
        break;

    case MQTT_EVENT_PUBLISHED:          //发布成功
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:               //接收到mqtt消息    
        ESP_LOGI(TAG, "MQTT_EVENT_DATA: topic=%s, data_len=%d", event->topic, event->data_len);
        esp_log_buffer_hex(TAG, event->data, event->data_len);
        //处理接收到的MQTT操作指令
        if(0 == memcmp(event->topic, (const char*)CLOUD_CTRL_TOPIC, strlen((const char*)CLOUD_CTRL_TOPIC))){
            handle_mqtt_msg(event);
        }else if(0 == memcmp(event->topic, (const char*)OTA_UPDATE_TOPIC, strlen((const char*)OTA_UPDATE_TOPIC))){
            // ESP_LOGI(TAG, "OTA_UPDATE_TOPIC");
        }
        break;

    case MQTT_EVENT_ERROR:           //错误
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        current_state = STATE_MQTT_CONNECT;
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @brief 处理接收到的 CLOUD_CTRL_TOPIC 消息
 *
 *  此函数处理 MQTT 消息，并将控制指令发送到 MCU。
 *
 * @param event 接收到的 MQTT 事件。
 */
void handle_mqtt_msg(esp_mqtt_event_handle_t event_msg)
{
    need_rsp_flg = true;
    if (event_msg->data_len > 7 && 
        0 == memcmp(event_msg->topic, (const char*)CLOUD_CTRL_TOPIC, strlen((const char*)CLOUD_CTRL_TOPIC)))  
    {   //接收到控制指令
    
        esp_log_buffer_hex("MQTT_DATA", event_msg->data, event_msg->data_len);
        if(crc16_reverse((uint8_t*)event_msg->data + 6, event_msg->data_len - 7) == 0
            && event_msg->data[0] == 0xAB && event_msg->data[event_msg->data_len - 1] == 0xAA){    
            //CRC16校验及帧头帧尾校验

            ESP_LOGI(TAG, "CRC16校验成功");

            uint8_t current_trace_id[4];                         //当前流水号
            memcpy(current_trace_id, event_msg->data + 2, sizeof(current_trace_id));
            
            if (0 == memcmp(current_trace_id,last_trace_id,sizeof(current_trace_id)))
            {
                ESP_LOGI(TAG, "重复的指令");
                return;
            }
            memcpy(last_trace_id, current_trace_id, sizeof(last_trace_id));        //更新流水号
            esp_log_buffer_hex("流水号",last_trace_id , sizeof(last_trace_id));

            //解析控制数据
            memcpy(command, event_msg->data + 6, sizeof(command));
            size_t ctrl_data_len = event_msg->data_len - 11;
            uint8_t ctrl_data[ctrl_data_len];         //控制数据
            memcpy(ctrl_data, event_msg->data + 8, ctrl_data_len);
            esp_log_buffer_hex("命令字", command, sizeof(command));
            esp_log_buffer_hex("控制数据", ctrl_data, ctrl_data_len);

            if((ctrl_data[3] == 0x04 || ctrl_data[3] == 0x06) && Dev_Status == SHUTDOWN ){
                //重置WiFi或控制指令，但设备处于关机状态
                ESP_LOGI(TAG, "设备处于关机状态，不执行该指令");

            }
            else{
                size_t len = uart_write_bytes(UART_NUM_1, (const char*)ctrl_data, ctrl_data_len);
                rsp_ctrl_msg(ctrl_data,ctrl_data_len, len);
            }
        }
    }
    else if(0 == memcmp(event_msg->topic, (const char*)OTA_UPDATE_TOPIC, strlen((const char*)OTA_UPDATE_TOPIC))){   
        //接收到OTA升级指令  //包含版本号、下载地址等信息
        ESP_LOGI(TAG, "OTA_UPDATE_TOPIC");
    }
}

/**
 * @brief 处理控制指令
 *
 *  此函数处理接收到的控制指令，并将响应消息发送到 MQTT 服务器。
 *
 * @param ctrl_data 接收到的控制数据。
 * @param data_len 控制数据长度。
 * @param len 发送到串口的控制数据长度。
 */
void rsp_ctrl_msg(uint8_t* ctrl_data, size_t data_len ,size_t len){
    if(len != data_len){
        ESP_LOGI(TAG, "发送指令失败");
        uint8_t rsp_false[12] = {0xAB,0x01,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x01,0x60,0xAA};
        memcpy(rsp_false + 2, last_trace_id, 4);
        int msg_id = esp_mqtt_client_publish(client, (const char*)MCU_RESP_TOPIC , 
                                             (const char*)rsp_false, sizeof(rsp_false), 1, 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Publish failed, msg_id=%d", msg_id);
        } else {
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        }

        need_rsp_flg = false;
        
    }else if (ctrl_data[3] == 0x04 || ctrl_data[3] == 0x06 || ctrl_data[3] == 0x08){    
        //WIFI重置指令、控制指令、查询工作状态指令
        ESP_LOGI(TAG, "发送控制指令成功");
        //发送响应消息
        uint8_t rsp_success[12] = {0xAB,0x01,0x00,0x00,0x00,0x00,0x00,0x02,0x01,0xC0,0xA0,0xAA};
        memcpy(rsp_success + 2, last_trace_id, 4);
        int msg_id = esp_mqtt_client_publish(client, (const char*)MCU_RESP_TOPIC , 
                                             (const char*)rsp_success, sizeof(rsp_success), 1, 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Publish failed, msg_id=%d", msg_id);
        } else {
            ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
        }

        need_rsp_flg = false;
    }else{
        ESP_LOGI(TAG, "发送产品查询指令成功");
    }
}


/**
 * @brief 初始化并启动 MQTT 客户端程序
 * 
 * 该函数配置并启动 MQTT 客户端以连接到指定的 MQTT 代理（Broker）。
 * 使用 TLS 加密安全连接，并提供必要的证书进行身份验证。
 *
 * @note 该函数会打印当前的可用内存，并初始化 MQTT 客户端，注册事件处理程序。
 */
void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://a1jlfhl8mb6ifw-ats.iot.us-east-1.amazonaws.com:8883",    // MQTT 服务器地址
        .broker.verification.certificate = (const char *)server_cert_pem_start,                 // 服务器证书
        .credentials = {
            .authentication = {
            .certificate = (const char *)client_cert_pem_start,                                 // 客户端证书
            .key = (const char *)client_key_pem_start,                                          // 客户端私钥
            },
        },
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);  //初始化 MQTT 客户端
    /* 最后一个参数可用于传递数据给事件处理程序，此程序中为 mqtt_event_handler */
    // 注册 MQTT 事件处理程序
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);  // 启动 MQTT 客户端

}

