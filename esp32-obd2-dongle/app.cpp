#include <Arduino.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include "a_ble.h"
#include "app.h"

CAN_device_t CAN_cfg;

void APP_Init(void)
{
    CAN_cfg.speed = CAN_SPEED_1000KBPS;
    CAN_cfg.tx_pin_id = GPIO_NUM_5;
    CAN_cfg.rx_pin_id = GPIO_NUM_4;
    CAN_cfg.rx_queue = xQueueCreate(20, sizeof(CAN_frame_t));

    //initialize CAN Module
    ESP32Can.CANInit();
    BLE_Init();

    xTaskCreate(BLE_Task, "BLE_Task", 10000, NULL, 1, NULL);
}

void APP_Task(void *pvParameters)
{
    //UBaseType_t uxHighWaterMark;
    CAN_frame_t rx_frame;
    uint16_t frameCount = 0;

    //uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    //printf("uxHighWaterMark = %d\r\n", uxHighWaterMark);

    while (1)
    {
        //receive next CAN frame from queue
        if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 10 * portTICK_PERIOD_MS) == pdTRUE)
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
            ESP32Can.CANWriteFrame(&rx_frame);
        }
    }
}