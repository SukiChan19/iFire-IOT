#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
//#include "nvs_flash.h"
#include "freertos/portable.h"
#include "freertos/projdefs.h"
// #include "nvs.h"
//#include "mqtt_client.h"

// #include "esp_wifi.h"
//#include "esp_timer.h"
#include "freertos/timers.h"
#include "dealwith_MCU_data.h"

/**
 * @brief 心跳检测定时器回调函数
 * 心跳检测定时器1 1s触发一次：下达心跳检测和查询指令到MCU
 * 响应检测定时器3 200ms触发一次：MCU响应MQTT指令超时处理
 */
static void vTimerCallback1(TimerHandle_t xTimer) {  
    static int Heartbeat_time_cout = IDLE_HEARTBEAT_PERIOD_S;       
    static int status_check_time_cout = IDLE_STAUTS_CHECK_PERIOD_S;      

    Heartbeat_time_cout++;
    status_check_time_cout++;

    // 设备是否处于工作状态上报周期不同
    int heartbeat_period = (Dev_Status != RUNNING)? IDLE_HEARTBEAT_PERIOD_S : RUNNING_HEARTBEAT_PERIOD_S;
    int status_check_period = (Dev_Status != RUNNING)? IDLE_STAUTS_CHECK_PERIOD_S : RUNNING_STATUS_CHECK_PERIOD_S;

      //设备处于非工作状态
    if (Heartbeat_time_cout > heartbeat_period){    
        Heartbeat_time_cout = 0;
        Hearbeat_flg = true;
        // 心跳时状态查询计数器延后一秒，避免同时写入触发
        if(status_check_time_cout > status_check_period) {status_check_time_cout = status_check_period - 1; }
        uart_write_bytes(UART_NUM_1, (const char*)HEARTBEAT_DETECT, HEARTBEAT_DETECT_len);  // 心跳检测指令
        // 打印心跳检测指令
        esp_log_buffer_hex("SENT Heartbeat detect", (const char*)HEARTBEAT_DETECT, HEARTBEAT_DETECT_len); 

    }
    if (status_check_time_cout > status_check_period){ 
        status_check_time_cout = 0;
        status_check_flg = true;

        uart_write_bytes(UART_NUM_1, (const char*)DevStatus_report, DevStatus_report_len); // 设备状态上报指令
        // 打印设备状态上报指令
        esp_log_buffer_hex("SENT DevStatus_report", (const char*)DevStatus_report, DevStatus_report_len); 

    }

    if(current_state != STATE_WORKING){ // 非工作状态，停止定时器
        xTimer1_flg = false;
        need_rsp_flg = false;
        xTimerStop(xTimer, 0);
        ESP_LOGI(TAG, "Stop Timer1");
    }
}

static void vTimerCallback3(TimerHandle_t xTimer) {
    // 定时器回调函数，处理超时逻辑
    if (need_rsp_flg == true) {
        need_rsp_flg = false;
        ESP_LOGI(TAG, "Wait for response timeout,continue to send command");
    }
    // 停止定时器3
    xTimer3_flg = false;
    xTimerDelete(xTimer, 0);
    xTimer = NULL;

    //重新启动定时器1
    if (xTimer1_flg == false){
        xTimer1_flg = true;
        xTimerStart(xTimer1, 0);        // 启动定时器1
        ESP_LOGI(TAG, "Restart Timer1");
    }
}

/**
 * @brief 处理MQTT发布指令数据
 * 计时器1 1s触发一次：下达心跳检测和查询指令到MCU
 * 计时器3 200ms触发一次：MCU响应MQTT指令超时处理
 */
void handle_MQTT_publish(void) {
    // 定时器1 1s触发一次
    if(xTimer1 == NULL){
        xTimer1 = xTimerCreate("Timer1",
                        pdMS_TO_TICKS(1000), 
                        pdTRUE, 
                        (void*)0, 
                        vTimerCallback1);
    }

    if (xTimer3 == NULL){
        xTimer3 = xTimerCreate("Timer3",
                        pdMS_TO_TICKS(200), 
                        pdFALSE, 
                        (void*)0, 
                        vTimerCallback3);
    }
    
    if(Hearbeat_flg){
        if(xTimer1 != NULL){
            xTimerStop(xTimer1, 0);         // 暂停定时器1
            xTimerDelete(xTimer1, 0);       // 定期删除定时器1
            xTimer1 = NULL;   
            ESP_LOGI(TAG, "Timer1 Deleted");
        }        
        xTimer1_flg = false;
        Hearbeat_flg = false;
    }

    // 启动定时器1
    if (!xTimer1_flg && !xTimer3_flg && xTimer1 != NULL) {
        xTimer1_flg = true;
        vTimerCallback1(0);     // 先执行一次回调函数
        xTimerStart(xTimer1, 0);        // 启动定时器1
        ESP_LOGI(TAG, "Start Timer1");
    }

    // 如果需要响应并且定时器3未运行，转换到定时器3
    if (need_rsp_flg && !xTimer3_flg) {
        xTimer3_flg = true;
        xTimer1_flg = false;
        xTimerStop(xTimer1, 0);         // 停止定时器1
        xTimerStart(xTimer3, 0);        // 启动定时器3
    }

    publish_cached_data();
    if(publish_data_flg == true){
        
        int msg_id = esp_mqtt_client_publish(client, (const char*)MCU_RESP_TOPIC, (const char*)publish_data, publish_data_len, 1, 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Publish failed, msg_id=%d", msg_id);
        } else {
            esp_log_buffer_hex("Publish data", (const char*)publish_data, publish_data_len);
        }
        publish_data_flg = false;
        publish_data_len = 0;
        memset(publish_data, 0, sizeof(publish_data));
    }
}

// UART 事件处理
void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    //uint8_t publish_data[267] = {0}; // 初始化发布数据缓冲区    8 + mcu_sent_data[256] + 3
    while (1) {
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY)) {

            if (event.type == UART_DATA) {
                // 接收到UART数据
                uint8_t *dtmp = (uint8_t*) malloc(event.size);
                //ESP_LOGW(TAG,"Memory allocated :%p", dtmp);
                //长度 = 接收到的数据长度 + 8：发布帧头 + 3：发布CRC16校验码&帧尾 -7：去掉接收到的数据帧头、lenght和校验和
             
                uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY);
                esp_log_buffer_hex("Received data", dtmp, event.size); // 打印接收到的数据
                // 校验和验证
                if (!validate_checksum((const uint8_t*)dtmp, event.size)) {
                    free(dtmp);
                    //ESP_LOGW(TAG, "Memory freed: %p", dtmp);
                    continue;
                } 
                else{
                    handle_MCU_data(dtmp, event.size);
                    //publish_to_mqtt(dtmp, event.size);
                    enqueue_uart_data(dtmp, event.size);
                    free(dtmp);
                    //ESP_LOGW(TAG, "Memory freed: %p", dtmp);
                }
            }
        }
    }
}

/**
 * @brief 分析MCU数据
 * 
 * @param mcu_sent_data 接收到的数据
 * @param len 接收到的数据长度
 * 
 * @note 接受MCU数据，对相应指令标志位进行处理
 */
void handle_MCU_data(uint8_t *mcu_sent_data, size_t  len) {
    const uint8_t reset_wifi_cmd[7] = {0x55, 0xAA, 0x03, 0x04, 0x00, 0x00, 0x06}; // 响应复位wifi指令

    switch (mcu_sent_data[3]){
        case 0x00:
            //心跳响应
            //ESP_LOGI(TAG, "Received heartbeat response");
            esp_log_buffer_hex("Received heartbeat response", mcu_sent_data, len); // 打印接收到的心跳响应

            break;
        case 0x01:
            //设备信息上报
            //ESP_LOGI(TAG, "Received product information");
            esp_log_buffer_hex("Received product information", mcu_sent_data, len); // 打印接收到的产品信息
            memcpy(DevPID, mcu_sent_data + 6,16);        // 复制产品ID到全局变量DevPID
            ESP_LOGI(TAG, "SN: %s", DevPID);
            memcpy(DevName, mcu_sent_data + 22, 4);      // 复制设备名称到全局变量DevName
            ESP_LOGI(TAG, "DevID: %s", DevName);
            DevModel = mcu_sent_data[26];
            ESP_LOGI(TAG, "DevModel: %d", DevModel);
            DevProgram = mcu_sent_data[27];
            ESP_LOGI(TAG, "DevProgram: %d", DevProgram);
            memcpy(MCU_SoftVer, mcu_sent_data + 28, 3); // 复制软件版本号到全局变量MCU_SoftVer
            ESP_LOGI(TAG, "MCU_SoftVer: %d.%d.%d",MCU_SoftVer[0], MCU_SoftVer[1], MCU_SoftVer[2]);
            get_product_info_flg = true; // 产品信息获取成功标志位
            break;
        case 0x02:
            //查询RUNMODE

            break;
        case 0x03:
            //查询WIFI状态
            if (0 == memcmp(mcu_sent_data, rsp_wifi_status, sizeof(rsp_wifi_status))){
                //ESP_LOGI(TAG, "Received WiFi status response");
                esp_log_buffer_hex("Received WiFi status response", mcu_sent_data, len); // 打印接收到的WiFi状态响应
                WiFi_status_rsp = true;
            }
            break;
        case 0x04:
            //重置WiFi
            if (0 ==memcmp(mcu_sent_data,reset_wifi_cmd,sizeof(reset_wifi_cmd))) {   
                ESP_LOGW("RESTART_WIFI", "MCU Received reset wifi command, restarting...");
                wifi_reset_flg = true;
            }
            break;
        case 0x06:
            //功能设定
            break;
        case 0x07:
            //ESP_LOGI(TAG, "Received device status report");
            esp_log_buffer_hex("Received device status report", mcu_sent_data, len); // 打印接收到的设备状态上报
            //设备状态上报
            if (mcu_sent_data[6] == 0x70){
                switch (mcu_sent_data[10]){
                    case 0x00:
                        if(Dev_Status != SHUTDOWN){
                            ESP_LOGI(TAG, "Device is Shutdown.");
                            Dev_Status = SHUTDOWN;
                        }
                        break;
                    case 0x01:
                        if(Dev_Status != STANDBY){
                            ESP_LOGI(TAG, "Device is Standby.");
                            Dev_Status = STANDBY;
                        }
                        break;
                    case 0x02:
                        if(Dev_Status != RUNNING){
                            ESP_LOGI(TAG, "Device is Running.");
                            Dev_Status = RUNNING;
                        }
                        break;
                    default:
                        ESP_LOGI(TAG, "Device status is unknown.");
                        break;
                }
            }
            break;
        case 0x08:
            //查询工作状态
            break;
        default:
            break;
    }
}


/**
 * @brief 将 UART 数据添加到 FIFO 队列
 */
void enqueue_uart_data(uint8_t *data, size_t length) {
    if (!is_fifo_full()) {
        memcpy(publish_data_buf[fifo_head].data, data, length);
        publish_data_buf[fifo_head].length = length;
        fifo_head = (fifo_head + 1) % BUFFER_SIZE; // 更新头索引
    } else {
        ESP_LOGW("UART FIFO", "FIFO is full!");
    }
}

/**
 * @brief 从 FIFO 队列中取出数据并移除数据
 * 
 * @param data 待读取的数据
 */
void dequeue_uart_data(publish_data_t *data) {
    if (!is_fifo_empty()) {
        memcpy(data->data, publish_data_buf[fifo_tail].data, publish_data_buf[fifo_tail].length);
        data->length = publish_data_buf[fifo_tail].length; // 获取数据长度
        fifo_tail = (fifo_tail + 1) % BUFFER_SIZE; // 更新尾索引
    }
}

/**
 * @brief 判断 FIFO 是否为空
 */
bool is_fifo_empty(void) {
    return fifo_head == fifo_tail;
}

/**
 * @brief 判断 FIFO 是否满
 */
bool is_fifo_full(void) {
    return ((fifo_head + 1) % BUFFER_SIZE) == fifo_tail;
}

/**
 * @brief 发布 FIFO 中的数据到发布数据缓冲区
 */
void publish_cached_data() {
    while (!is_fifo_empty()) {
        publish_data_t data;
        dequeue_uart_data(&data); // 从 FIFO 中获取数据
        // 发布到 MQTT
        publish_to_mqtt(data.data, data.length);
    }
}

/**
 * @brief 缓存待发布数据到发布数据缓冲区
 * 
 * @param data MCU响应数据
 * @param size MCU响应数据长度
 * 
 * @note 缓存待发布数据到 publish_data 数据缓冲区，并更新待响应指令标志位
 */
void publish_to_mqtt(uint8_t *data, size_t  size) {
    if(current_state == STATE_WORKING && size > 7){    
        // 工作状态，接收到数据长度大于7字节为有效数据，需发布MQTT消息

        // const uint8_t command = {0x00,0x01};       //控制指令
        // const uint8_t command = {0x00,0x02};       //控制指令响应
        // const uint8_t command = {0x02,0x02};       //主动上报指令
        // const uint8_t command = {0x02,0x04};       //错误上报指令
        // const uint8_t command = {0x02,0x06};       //紧急上报指令
        memcpy(publish_data, (uint8_t[]){0xAB,0x01}, 2);      // 添加起始符到发布数据缓冲区
        if(need_rsp_flg == true && memcmp(command, (uint8_t[]){0x00,0x01}, 2) == 0){
        //需要响应的指令，复制流水号到发布数据缓冲区，发布指令0x0002
            memcpy(publish_data + 2, &last_trace_id, sizeof(last_trace_id));
            memcpy(publish_data + 6, (uint8_t[]){0x00,0x02}, 2);                         // 添加响应指令到发布数据缓冲区
            need_rsp_flg = false;
            // 停止响应计时器
            if (xTimer3 != NULL) {
                xTimer3_flg = false;
                xTimerStop(xTimer3, 0);
            }
        }
        else{
        //不需要响应的指令，流水号填充0
            memset(publish_data + 2, 0, 4);  // 指令响应为空，填充0
            // 处理主动上报指令和错误上报指令
            uint8_t command_to_copy[2] = {0x02,0x00};
            if(data[3] == 0x00) {           //心跳上报 命令字 0x02 0x00
                command_to_copy[1] = 0x00;
            }
            else{
                if (data[6] == 0x0D) {      //错误上报 命令字 0x02 0x04
                    command_to_copy[1] = 0x04;
                } else {                    //其它上报 命令字 0x02 0x02
                    command_to_copy[1] = 0x02;
                }
            }
            memcpy(publish_data + 6, command_to_copy, 2); // 复制指令到发布数据缓冲区
        }
        uint8_t data_len = size - 7; // 去掉帧头、lenght和校验和
        memcpy(publish_data + 8,data + 6 , data_len);            // 复制UART数据到发布数据缓冲区,去掉帧头和校验和

        uint8_t check_data[data_len + 2];
        memcpy(check_data, publish_data + 6, 2 + data_len);           //CRC16校验码计算需要的数据，包括指令、UART数据
        uint16_t crc16_check = crc16_reverse(check_data,sizeof(check_data));  // 计算CRC16校验码

        memcpy(publish_data + 8 + data_len, (uint8_t*)&crc16_check, sizeof(crc16_check));  // 复制CRC16校验码到发布数据缓冲区
        publish_data[8 + data_len + 2] = 0xAA;                           // 添加结束符
        publish_data[8 + data_len + 3] = '\0';
        publish_data_len = 8 + data_len + 3;
        //esp_log_buffer_hex("UART_PUBLISH_DATA", publish_data, publish_data_len); // 打印发布数据
        publish_data_flg = true;
    } 
}
