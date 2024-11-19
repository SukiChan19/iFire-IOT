#include "ProtocolsChar.h"
#include "esp_wifi.h"
#define DEFAULT_SCAN_LIST_SIZE 15   // default number of APs to scan

/**************** Global Variables Declarations ******************/

uint16_t found_ap_lists_number = DEFAULT_SCAN_LIST_SIZE;     //Wi-Fi Aps查找数量
uint8_t ap_lists_data[DEFAULT_SCAN_LIST_SIZE][70];           //Wi-Fi Aps数据列表,信息将传递给GATTS 
extern uint8_t crc8(uint8_t* data, size_t length);
