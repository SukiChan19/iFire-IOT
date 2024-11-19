
/****************************************************************************
*

传输协议字符定义

*
****************************************************************************/
#ifndef PROTOCOLSCHAR_H_
#define PROTOCOLSCHAR_H_

#include <stdint.h>

#define PROTOCOL_CHAR_MAX_LEN 10
/*协议字符定义
 *协议字符格式：起始字节(1 byte = 0xAB) + 版本号(1 byte = 0x01) +
 *
 *             指令类型(1 byte) + 数据长度(1byte) + 数据(n bytes) + CRC8校验和(1 byte) + 
 *
 *             结束字节(1 byte = 0xAA)
 */

/*  app->esp :开始扫描AP指令  指令类型：0x01  数据长度：0x01  数据：0x01（扫描开始）    CRC8校验和：0x79    结束字节：0xAA */
#define START_SCAN_AP   {0xAB,0x01,0x01,0x01,0x01,0x79,0xAA} 

/*  esp->app :扫描AP完成通知  指令类型：0x02  数据长度：0x01  数据：0x01（扫描完成）    CRC8校验和：0xC4    结束字节：0xAA */
#define FINISH_SCAN_AP  {0xAB,0x01,0x02,0x01,0x01,0xC4,0xAA}   
/*  app->esp :获取wifi信息指令 header 指令类型：0x03   */
#define GET_WIFI_INFO_header   {0xAB,0x01,0x03}   

/*  esp->app :wifi连接成功通知  指令类型：0x04 数据长度：0x02  数据：0x01（连接成功） 0x00  CRC8校验和：0x9B    结束字节：0xAA */
#define WIFI_CONNECTED   {0xAB,0x01,0x04,0x02,0x01,0x00,0x9B,0xAA} 

/*  esp->app :wifi连接失败通知  指令类型：0x04 数据长度：0x02  数据：0x02（连接失败） 0x00  CRC8校验和：0xA4    结束字节：0xAA */
#define WIFI_CONNECT_FAIL   {0xAB,0x01,0x04,0x02,0x02,0x00,0xA4,0xAA}  //esp->app :wifi连接断开通知  指令类型 0x04

/*  esp->app :校验码错误        指令类型：0x04 数据长度：0x02  数据：0x03（校验错误） 0x00  CRC8校验和：0xB1    结束字节：0xAA */
#define CRC_ERROR   {0xAB,0x01,0x04,0x02,0x03,0x00,0xB1,0xAA} 

/*  esp->app :传输AP列表信息 header 指令类型：0x02 */
#define AP_LIST_INFO_header    {0xAB,0x01,0x02}    

/*  ESP->MCU :获取产品信息指令  */
#define GET_PRODUCT_INFO        "\x55\xAA\x00\x01\x00"
#define GET_PRODUCT_INFO_len    5

/*  ESP->MCU :未连接到WIFI指令  */
#define WIFI_DISCONNECTED       "\x55\xAA\x00\x03\x00\x01\x02\x05"
#define WIFI_DISCONNECTED_len   8

/*  ESP->MCU :连接到WIFI指令  */
#define CONNECT_TO_WIFI         "\x55\xAA\x00\x03\x00\x01\x03\x06"
#define CONNECT_TO_WIFI_len     8

/*  ESP->MCU :已连接云端通知  */
#define CONNECTED_TO_CLOUD      "\x55\xAA\x00\x03\x00\x01\x04\x07"
#define CONNECTED_TO_CLOUD_len  8

/*  ESP->MCU :心跳检测指令    */
#define HEARTBEAT_DETECT        "\x55\xAA\x00\x00\xFF"
#define HEARTBEAT_DETECT_len    5

/*  ESP->MCU :查询设备状态指令  */
#define DevStatus_report        "\x55\xAA\x00\x08\x07"
#define DevStatus_report_len    5

enum DevStatus {
    SHUTDOWN,
    STANDBY,
    RUNNING
};

enum WiFi_Connect_Status
{
     CONNECT_SUCCESS,
     CONNECT_FAILED,
};

// 状态机枚举
typedef enum {
    STATE_INIT,
    STATE_WIFI_CONNECT,
    STATE_MQTT_CONNECT,
    STATE_WORKING,
} app_state_t;

#endif /* PROTOCOLSCHAR_H_ */