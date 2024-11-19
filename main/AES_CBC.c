#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aes/esp_aes.h" // 引入AES相关的头文件
#include "esp_log.h" // 引入日志相关的头文件
#include "esp_wifi_types_generic.h"
#include "AES_CBC.h"


/** 
 * @brief AES-CBC解密函数
 * 
 * 该函数用于解密获取的密文
 * 
 * @param encrypted-CBC加密后的password密文
 * 
 * @param iv：CBC加密时的偏移量  
 * 
 * @param decrypted_password: 解密后的password明文
 * 
 */
void decrypt_password(uint8_t *encrypted ,size_t encrypted_length, uint8_t *iv ,uint8_t *decrypted_password){
    esp_aes_context aes_ctx;                    // AES上下文
    esp_aes_init(&aes_ctx);                 // 初始化AES上下文
    uint8_t key[KEY_SIZE] = KEY;                // 密钥

    esp_aes_setkey(&aes_ctx, key, KEY_SIZE * 8);    // 设置密钥

    //size_t data_length = sizeof(encrypted); // 待解密的数据长度
    uint8_t decrypted_data[encrypted_length]; // 解密后的数据
    memset(decrypted_data, 0, encrypted_length);
    esp_log_buffer_hex("AES_CBC_ENCRYPT_DATA", encrypted, encrypted_length); // 打印加密数据
    esp_log_buffer_hex("AES_CBC_IV", iv, IV_SIZE); // 打印IV
    esp_log_buffer_hex("AES_CBC_KEY", key, KEY_SIZE); // 打印密钥
    esp_aes_crypt_cbc(&aes_ctx, ESP_AES_DECRYPT, encrypted_length, iv, encrypted, decrypted_data);
    esp_log_buffer_hex("AES_CBC_DECRYPT_DATA", decrypted_data, sizeof(decrypted_data)); // 打印解密后未去填充的数据
    size_t length = sizeof(decrypted_data);
    if (decrypted_data[length - 1] < BLOCK_SIZE) // 去掉填充数据
    {
        size_t padding_length = decrypted_data[length - 1];
        memcpy(decrypted_password, decrypted_data, length - padding_length);
        decrypted_password[length - padding_length] = '\0';
    }
    else
    {
        memcpy(decrypted_password, decrypted_data, length);
    }
    esp_aes_free(&aes_ctx); // 释放AES上下文

}