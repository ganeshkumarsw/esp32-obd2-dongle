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
    CAN_filter_t p_filter;

    CAN_DeInit();

    // Set CAN Filter
    // See in the SJA1000 Datasheet chapter "6.4.15 Acceptance filter"
    // and the APPLICATION NOTE AN97076 chapter "4.1.2 Acceptance Filter"
    // for PeliCAN Mode
    
    p_filter.FM = Single_Mode;

    p_filter.ACR0 = 0;
    p_filter.ACR1 = 0;
    p_filter.ACR2 = 0;
    p_filter.ACR3 = 0;

    p_filter.AMR0 = 0xFF;
    p_filter.AMR1 = 0xFF;
    p_filter.AMR2 = 0xFF;
    p_filter.AMR3 = 0xFF;
    ESP32Can.CANConfigFilter(&p_filter);

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

void CAN_ConfigFilterterMask(CAN_filter_t *p_filter)
{
    ESP32Can.CANConfigFilter(p_filter);
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

    while (1)
    {
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}