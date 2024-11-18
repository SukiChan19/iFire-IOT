

#include "driver/uart.h"
#include "driver/gpio.h"


void uart_init(void)
{
    //Configure UART
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(1, 1024 * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(1, 2, 3, -1, -1));
}
