/*
 * @Description: This file is used to configure and initialize the UART interface.
 * 
 * @Note: 在使用该函数之前，请确保相关驱动已包含并正确配置。
 * 
 */

#include "driver/uart.h"
#include "freertos/task.h"

QueueHandle_t uart0_queue;

extern void uart_event_task(void *arg);
/**
 * @brief 初始化UART接口
 * 
 * 该函数配置并初始化UART，包括设置波特率、数据位、奇偶校验、停止位、流控制和时钟源等。
 * 
 * @note 在调用此函数之前，请确保相关驱动已包含并正确配置。
 *
 * @return 无
 */
void uart_init(void)
{
    //Configure UART
    const uart_config_t uart_config = {
        .baud_rate = 115200,                    //波特率（baud_rate）：设置为115200。
        .data_bits = UART_DATA_8_BITS,          //数据位：设置为8位。
        .parity = UART_PARITY_DISABLE,          //奇偶校验：禁用。
        .stop_bits = UART_STOP_BITS_1,          //停止位：设置为1位。
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  //流控制：禁用。
        .source_clk = UART_SCLK_APB             //时钟源：设置为APB时钟。
    };
    // int intr_alloc_flags = 0;                    //中断分配标志,设置为0，不使用中断。
    //安装UART驱动，并配置参数：    端口号：uart_num = 1   缓冲区分配大小为1024*2字节  使能接收中断
    uart_driver_install(UART_NUM_1, 1024 * 2, 0, 20, &uart0_queue, 0);
    uart_param_config(UART_NUM_1, &uart_config);                                                                                                                                                               
    //设置UART引脚：    
    //端口号：uart_num = 1       TXD引脚GPIO：GPIO2    RXD引脚GPIO：GPIO3    RTS引脚GPIO：-1 （不使用）   CTS引脚GPIO：-1 （不使用）
    uart_set_pin(UART_NUM_1, 2, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //创建UART事件处理任务
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
    //开启接收中断
    uart_enable_rx_intr(UART_NUM_1); 
    
}
