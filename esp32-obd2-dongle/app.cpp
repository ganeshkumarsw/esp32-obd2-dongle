#include <Arduino.h>
#include "a_ble.h"
#include "a_uart.h"
#include "a_can.h"
#include "app.h"

void APP_Init(void)
{
}

void APP_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    CAN_frame_t rx_frame;
    uint16_t frameCount = 0;

    Serial.println("APP_Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("APP uxHighWaterMark = %d\r\n", uxHighWaterMark);


    while (1)
    {
        //receive next CAN frame from queue
        if (CAN_ReadFrame(&rx_frame) == pdTRUE)
        {
            //printf("%d", frameCount++);
            //do stuff!
            if (rx_frame.FIR.B.FF == CAN_frame_std)
            {
                printf("New standard frame");
            }
            else
            {
                printf("New extended frame");
            }

            if (rx_frame.FIR.B.RTR == CAN_RTR)
            {
                printf(" RTR from 0x%08x, DLC %d\r\n", rx_frame.MsgID, rx_frame.FIR.B.DLC);
            }
            else
            {
                printf(" from 0x%08x, DLC %d\n", rx_frame.MsgID, rx_frame.FIR.B.DLC);
                // convert to upper case and respond to sender
                for (int i = 0; i < 8; i++)
                {
                    if (rx_frame.data.u8[i] >= 'a' && rx_frame.data.u8[i] <= 'z')
                    {
                        rx_frame.data.u8[i] = rx_frame.data.u8[i] - 32;
                    }
                }
            }

            //respond to sender
            CAN_WriteFrame(&rx_frame);
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
