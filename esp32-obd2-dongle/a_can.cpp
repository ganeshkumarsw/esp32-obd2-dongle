#include <Arduino.h>
#include "a_can.h"

CAN_device_t CAN_cfg = {
    .speed = CAN_SPEED_1000KBPS,
    .tx_pin_id = GPIO_NUM_5,
    .rx_pin_id = GPIO_NUM_4,
    .rx_queue = NULL,
};

void CAN_Init(void)
{
    CAN_DeInit();

    if (CAN_cfg.rx_queue == NULL)
    {
        CAN_cfg.rx_queue = xQueueCreate(20, sizeof(CAN_frame_t));
        configASSERT(CAN_cfg.rx_queue);
    }

    ESP32Can.CANInit();
}

void CAN_DeInit(void)
{
    ESP32Can.CANStop();
}

void CAN_SetBaud(CAN_speed_t speed)
{
    CAN_DeInit();
    CAN_cfg.speed = speed;
    ESP32Can.CANInit();
}

void CAN_SetFilterMask(uint32_t mask)
{
    ESP32Can.CANSetFilter(mask);
}

BaseType_t CAN_ReadFrame(CAN_frame_t *frame)
{
    if (xQueueReceive(CAN_cfg.rx_queue, frame, 10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        return pdTRUE;
    }
    else
    {
        return pdFALSE;
    }
}

void CAN_WriteFrame(CAN_frame_t *frame)
{
    ESP32Can.CANWriteFrame(frame);
}

void CAN_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;

    Serial.println("CAN_Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("CAN uxHighWaterMark = %d\r\n", uxHighWaterMark);

    while(1)
    {
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}