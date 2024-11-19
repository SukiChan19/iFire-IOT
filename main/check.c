/****************************************************************************
*

包含CRC-8、CRC-16、校验和计算函数的源文件

CRC-8 用于蓝牙通信协议校验

CRC-16 用于mqtt协议校验

校验和用于MCU数据传输校验

*
****************************************************************************/

#include <string.h>
#include <stdint.h>
#include "check.h"

/**
 * @brief CRC-8 计算函数
 * 
 * @param data 待计算的数据
 * @param length 待计算的数据长度
 * @return uint8_t CRC-8 校验值
 */
uint8_t crc8(uint8_t* data, size_t length) {
    uint8_t crc = 0x00; 
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i]; // 异或当前字节
        for (int j = 0; j < 8; j++) { // 对每个比特进行处理
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC8_POLY; // 如果最高位为 1，左移并异或多项式
            } else {
                crc <<= 1; // 否则仅左移
            }
        }
    }
    return crc & 0xFF; // 返回最终的 CRC8 值
}

/**
 * @brief 反转字节
 * 
 * @param byte 待反转的字节
 * @return uint8_t 反转后的字节
 */
uint8_t reverse_byte(uint8_t byte) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; i++) {
        if (byte & (1 << i)) {
            reversed |= (1 << (7 - i));
        }
    }
    return reversed;
}

/**
 * @brief CRC-16 计算函数
 * 
 * @param data 待计算的数据
 * @param length 待计算的数据长度
 * @return uint16_t CRC-16 校验值
 */
uint16_t crc16(uint8_t* data, size_t length) {
    uint16_t crc = 0x0000; // 初始值
    for (size_t i = 0; i < length; i++) {
        crc ^= (reverse_byte(data[i]) << 8); // 反转数据并与 CRC 进行 XOR
        for (int j = 0; j < 8; j++) { // 处理每个比特
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY; // 如果最高位为 1，左移并异或多项式
            } else {
                crc <<= 1; // 否则仅左移
            }
        }
    }
    return crc ^ 0x0000; // 最终结果异或值
}

/**
 * @brief CRC-16 反转函数
 * 
 * @param crc 待反转的 CRC-16 校验值
 * @return uint16_t 反转后的 CRC-16 校验值
 */
uint16_t reverse_crc(uint16_t crc) {
    uint16_t reversed = 0;
    for (int i = 0; i < 16; i++) {
        if (crc & (1 << i)) {
            reversed |= (1 << (15 - i));
        }
    }
    return reversed;
}
/**
 * @brief CRC-16 反转函数
 * 
 * @param data 待计算的数据
 * @param length 待计算的数据长度
 * @return uint16_t 反转后的 CRC-16 校验值
 */
uint16_t crc16_reverse(uint8_t* data, size_t length) {
    uint16_t crc = crc16(data, length);
    return reverse_crc(crc);
}


/**
 * @brief 计算校验和
 * 
 * @param data 待计算的数据
 * @param length 待计算的数据长度
 * @return 校验和
 */
uint8_t calculate_checksum(const uint8_t *data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    checksum = checksum & 0xFF;     // 取低 8 位
    return checksum;
}

