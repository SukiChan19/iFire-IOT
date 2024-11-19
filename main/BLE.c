/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
*

创建BLE蓝牙通讯，实现设备间通信，包括广播、扫描、连接、读写、通知等功能。

主要实现GATT服务的定义和初始化，包括服务、特征、特征值等。

连接蓝牙传输MCU数据，并配网

*
****************************************************************************/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// #include "esp_system.h"
#include "esp_log.h"
// #include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_mac.h"

#include "stdlib.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
// #include "esp_bt_device.h"
#include "gatts_table.h"
#include "esp_gatt_common_api.h"
#include "ProtocolsChar.h"

#include "esp_wifi.h"

#include "driver/uart.h"

#define GATTS_TABLE_TAG "GATTS_TABLE"

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define DEVICE_NAME                 "iFireTech"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
*/
#define GATTS_CHAR_VAL_LEN_MAX      500
#define PREPARE_BUF_MAX_SIZE        1024
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

static uint8_t adv_config_done       = 0;

#define DEFAULT_SCAN_LIST_SIZE      15  // number of adv data in the scan response

/********外部变量声明******** */
extern wifi_config_t wifi_config;
// extern wifi_ap_record_t ap_info[];
extern uint16_t found_ap_lists_number;
extern uint8_t ap_lists_data[DEFAULT_SCAN_LIST_SIZE][70];
extern uint8_t ConfigNetworkFailed;
extern uint8_t DevPID[16];
extern uint8_t MCU_SoftVer[2];


/********全局变量声明******** */
esp_gatt_if_t G_gatts_if = ESP_GATT_IF_NONE; // 记录 GATT 接口
uint16_t conn_id; // 记录连接 ID
bool ble_connected = false; // 记录蓝牙连接状态
//uint8_t encrypted_password[16]; // 记录加密后的密文
uint8_t get_iv[16];
uint8_t UUID[4] = {0x32, 0x32, 0x33, 0x33}; // 记录MCU UUID  4 bytes 由MCU生成，暂定为0xAA 0xBB 0xCC 0xDD



/********外部函数声明******** */
//extern void wifi_sta(void);
extern void wifi_scan();
extern void save_wifi_config_to_nvs();
extern uint8_t crc8(uint8_t* data, size_t length);
extern void decrypt_password(uint8_t *encrypted ,size_t encrypted_length, uint8_t *iv ,uint8_t *decrypted_password);


uint16_t heart_rate_handle_table[HRS_IDX_NB];

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

//static prepare_type_env_t prepare_write_env;

static uint8_t raw_adv_data[31] = {
        /* flags */
        0x02, 0x01, 0x06,   // flags length, flags data type, flags data
        /* tx power*/
        0x02, 0x0a, 0xeb,
        /* service uuid */
        0x03, 0x03, 0xFF, 0x00,
        /* device name */
        0x0e, 0x09, 'i','F','i','r','e','T','e','c','h',
        //后接MCU UUID 4 bytes 应由MCU生成
};
static uint8_t raw_scan_rsp_data[26] = {
        0x07, 0x11
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/******************** GATT服务变量声明 *******************/



/* Service    定义GATT字符UUID */
static const uint16_t GATTS_SERVICE_UUID      = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_A       = 0xFF01;

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
/* Characteristic Value 定义GATT特征属性 */

static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
/* heartrate measurement characteristic value   心率测量特征值 */
static const uint8_t heart_measurement_ccc[2]      = {0x00, 0x00};
static const int8_t char_value[4]                  = "0000";


// GATT数据库定义
/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
{
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID), (uint8_t *)&GATTS_SERVICE_UUID}},

    /* Characteristic Declaration */
    [IDX_CHAR_A]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_A] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_A, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_A]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}}

};

/**
 * @brief      gap服务
 * @param      event 回调事件
 * @param      param 回调参数指针
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "advertising start failed");
            }else{
                ESP_LOGI(GATTS_TABLE_TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}


/**
 * @brief      注册GATT服务
 * @param      event    回调事件类型
 * @param      gatts_if  GATT服务接口
 * @param      param    回调事件指针
 */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);   // 设置设备名称
            if (set_dev_name_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }
            uint8_t mac[6];                         // 存储本机的蓝牙地址
            esp_read_mac(mac, ESP_MAC_BT);     // 读取本机的蓝牙地址
            ESP_LOGI(GATTS_TABLE_TAG, "device MAC:" MACSTR, MAC2STR(mac));

            memcpy(raw_adv_data + 21, UUID, 4);     // 填充UUID

            esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
            if (raw_adv_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
            }

            adv_config_done |= ADV_CONFIG_FLAG;
            memcpy(raw_scan_rsp_data + 2, mac, 6);     // 填充MAC地址
            raw_scan_rsp_data[8] = 0x11;     // 填充厂商ID
            raw_scan_rsp_data[9] = 0xFF;
            memcpy(raw_scan_rsp_data + 10, DevPID, 16);     // 填充设备名称
            esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
            if (raw_scan_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_READ_EVT,handle = %d", param->read.handle);           
       	    break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                ESP_LOGI(GATTS_TABLE_TAG, "GATT_WRITE_EVT, handle = %d, value len = %d", param->write.handle, param->write.len);
                esp_log_buffer_hex(GATTS_TABLE_TAG, param->write.value, param->write.len);   
                if (heart_rate_handle_table[IDX_CHAR_VAL_A] == param->write.handle)
                {

                    uint8_t crc_check = param->write.value[param->write.len - 2];
                    ESP_LOGI(GATTS_TABLE_TAG, "CRC8 check:%d", crc_check);
                    uint8_t crc_input[param->write.len - 4];
                    memcpy(crc_input, param->write.value + 2 , param->write.len - 3);        // 去掉前两个字节（帧头）和最后一个字节（帧尾）
                    if(crc8(crc_input, param->write.len - 3) == 0x00){          //校验CRC8 

                        //接收到扫描wifi指令后，开始扫描附近的wifi
                        uint8_t start_scan[] = START_SCAN_AP;
                        uint8_t scan_done[] = FINISH_SCAN_AP;
                        uint8_t get_wifi_info_header[] = GET_WIFI_INFO_header;
                        

                        if(0 == memcmp(param->write.value,start_scan, sizeof(start_scan))){         // 开始扫描
                            ESP_LOGI(GATTS_TABLE_TAG, "Start Scan AP and send data to client");

                            wifi_scan();    // 扫描附近的wifi

                            for (int i = 0; i < found_ap_lists_number; i++) { 
                                esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                                            19 + ap_lists_data[i][16], ap_lists_data[i], false);
                            }
                            esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                                            sizeof(scan_done), scan_done, false);
                            ESP_LOGI(GATTS_TABLE_TAG, "scan done");
                            memset(ap_lists_data, 0, sizeof(ap_lists_data));
                        }




                        //接收到ssid和password后，保存到wifi_config中，并尝试连接到指定的路由器
                        
                        else if(0 == memcmp(param->write.value,get_wifi_info_header, sizeof(get_wifi_info_header)) &&
                             param->write.value[2] == 0x03 && param->write.value[3] > 0){


                            //获取ssid
                            uint8_t ssid_len = param->write.value[4];


                            memcpy(wifi_config.sta.ssid, param->write.value + 5, ssid_len);
                            wifi_config.sta.ssid[ssid_len] = '\0';
                            ESP_LOGI(GATTS_TABLE_TAG, "SSID:%s", wifi_config.sta.ssid);
                            esp_log_buffer_hex(GATTS_TABLE_TAG, wifi_config.sta.ssid, strlen((char*)wifi_config.sta.ssid));

                            //获取BSSID
                            memcpy(wifi_config.sta.bssid, param->write.value + 4 + ssid_len + 1, 6);
                            esp_log_buffer_hex(GATTS_TABLE_TAG, wifi_config.sta.bssid, 6);

                            //获取认证模式
                            wifi_config.sta.threshold.authmode = param -> write.value[11 + ssid_len];
                            ESP_LOGI(GATTS_TABLE_TAG, "认证模式:%d", wifi_config.sta.threshold.authmode);


                            if (wifi_config.sta.threshold.authmode != WIFI_AUTH_OPEN)   // WiFi加密模式不为open（value=0）,则获取密文以解密
                            {
                                //获取密文长度
                                

                                uint8_t encrypted_password_len = param->write.value[12 + ssid_len];
                                //获取密文以及IV
                                uint8_t encrypted_password[encrypted_password_len];
                                memset(encrypted_password, 0, sizeof(encrypted_password));
                                memcpy(encrypted_password, param->write.value + 13 + ssid_len, encrypted_password_len);
                                ESP_LOGI(GATTS_TABLE_TAG, "密文 GET");

                                memcpy(get_iv, param->write.value + 13 + ssid_len + encrypted_password_len, 16);
                                ESP_LOGI(GATTS_TABLE_TAG, "偏移量IV GET");


                                // 解密密码
                                decrypt_password(encrypted_password, encrypted_password_len, get_iv, wifi_config.sta.password);

                                ESP_LOGI(GATTS_TABLE_TAG, "明文 GET：%s", wifi_config.sta.password);

                            }
                            else    // WiFi加密模式为open（value=0）,则密码为空
                            {
                                memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
                            }

                            save_wifi_config_to_nvs();   // 保存wifi配置到nvs
                            ESP_LOGI(GATTS_TABLE_TAG, "Save wifi config to nvs");
                            //ESP_LOGI(GATTS_TABLE_TAG, "Get SSID:%s \t\t password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
                            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
                            ESP_ERROR_CHECK(esp_wifi_start() );
                            esp_wifi_connect();         // 连接到指定的路由器
                        }
                        else{
                            ESP_LOGI(GATTS_TABLE_TAG, "unknown command");
                            uint8_t error_msg[] = CRC_ERROR;
                            //notify:未知命令
                            esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                                            sizeof(error_msg), error_msg, false);
                        }

                            
                    }
                    else{
                    uint8_t error_msg[] = CRC_ERROR;
                    ESP_LOGI(GATTS_TABLE_TAG, "CRC8 check failed");
                    //notify:检验错误
                    esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                                sizeof(error_msg), error_msg, false);
                    }
                }

                if (heart_rate_handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2){
                    uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                    if (descr_value == 0x0001){
                        ESP_LOGI(GATTS_TABLE_TAG, "notify enable");  
                    }else if (descr_value == 0x0002){
                        ESP_LOGI(GATTS_TABLE_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i % 0xff;
                        }

                        // //if want to change the value in server database, call:
                        esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_A], sizeof(indicate_data), indicate_data);
                        

                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                                            sizeof(indicate_data), indicate_data, true);
                    }
                    else if (descr_value == 0x0000){
                        ESP_LOGI(GATTS_TABLE_TAG, "notify/indicate disable ");
                    }else{
                        ESP_LOGE(GATTS_TABLE_TAG, "unknown descr value");
                        esp_log_buffer_hex(GATTS_TABLE_TAG, param->write.value, param->write.len);
                    }

                }
                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
            // else{
            //     /* handle prepare write */
            //     prepare_write_event_env(gatts_if, &prepare_write_env, param);
            // }
      	    break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            // exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ble_connected = true;
            conn_id = param->connect.conn_id;
            G_gatts_if = gatts_if;
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            esp_log_buffer_hex(GATTS_TABLE_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;                                        
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms  
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms  
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms    
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ble_connected = false;
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
                memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
                esp_ble_gatts_start_service(heart_rate_handle_table[IDX_SVC]);
            } 
            break;
        }
        case ESP_GATTS_STOP_EVT:        //以下事件均为废弃事件，不用处理
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATTS_TABLE_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if) {
                if (heart_rate_profile_tab[idx].gatts_cb) {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

/**
 * @brief      初始化蓝牙服务
 */
void gatts_profile_init(void)
{
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(ESP_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(300);  // MTU size used by the application MAX:517
    if (local_mtu_ret){
        ESP_LOGE(GATTS_TABLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

}
