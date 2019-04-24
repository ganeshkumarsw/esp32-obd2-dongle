#include <Arduino.h>
#include "a_uart.h"

uint8_t UART_Buff[4096];

void UART_Init(void)
{
}

void UART_Task(void *pvParameters)
{
    uint16_t idx;
    uint16_t len;
    UBaseType_t uxHighWaterMark;

    Serial.println("UART_Task Started");
    
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("UART uxHighWaterMark = %d\r\n", uxHighWaterMark);

    while (1)
    {
        idx = 0;
        len = 0;

        while (len != Serial.available())
        {
            len = Serial.available();
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }

        while (Serial.available())
        {
            UART_Buff[idx++] = Serial.read();

            if (idx >= sizeof(UART_Buff))
            {
                break;
            }
        }

        if (len != 0)
        {
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}