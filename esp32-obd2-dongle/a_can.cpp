#include <Arduino.h>
#include "config.h"
#include "CAN_config.h"
#include "CAN.h"
#include "a_can.h"

CAN_device_t CAN_cfg = {
    .speed = CAN_SPEED_500KBPS,
    .tx_pin_id = GPIO_NUM_5,
    .rx_pin_id = GPIO_NUM_4,
    .rx_queue = NULL,
    .tx_queue = NULL,
};
SemaphoreHandle_t CAN_SemaphoreRxTxQueue;

void CAN_Init(void)
{
    CAN_filter_t filter;

    if (CAN_cfg.err_queue == NULL)
    {
        CAN_cfg.err_queue = xQueueCreate(3, sizeof(CAN_error_t));
    }

    if (CAN_cfg.rx_queue == NULL)
    {
        CAN_cfg.rx_queue = xQueueCreate(CAN_RX_QUEUE_SIZE, sizeof(CAN_frame_t));
    }

    if (CAN_cfg.tx_queue == NULL)
    {
        CAN_cfg.tx_queue = xQueueCreate(CAN_TX_QUEUE_SIZE, sizeof(CAN_frame_t));

        // Set CAN Filter
        // See in the SJA1000 Datasheet chapter "6.4.15 Acceptance filter"
        // and the APPLICATION NOTE AN97076 chapter "4.1.2 Acceptance Filter"
        // for PeliCAN Mode
        filter = {
            .FM = Single_Mode,
            .ACR0 = 0,
            .ACR1 = 0,
            .ACR2 = 0,
            .ACR3 = 0,
            .AMR0 = 0xFF,
            .AMR1 = 0xFF,
            .AMR2 = 0xFF,
            .AMR3 = 0xFF,
        };

        CAN_Drv_ConfigFilter(&filter);
    }

    if ((CAN_cfg.rx_queue == NULL) || (CAN_cfg.tx_queue == NULL) || (CAN_cfg.err_queue == NULL))
    {
        Serial.println("ERROR: CAN Failed to create queue message for either Rx / Tx / err");
    }

    // Init CAN Module
    // ESP32Can.CANInit();
}

void CAN_DeInit(void)
{
    CAN_Drv_Stop();
}

void CAN_SetBaud(CAN_speed_t speed)
{
    CAN_Drv_Stop();

    CAN_cfg.speed = speed;

    // Init CAN Module
    CAN_Drv_Init(&CAN_cfg);
}

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))
    uint32_t acceptance_mask;
    CAN_filter_t filter;

    CAN_Drv_Stop();

    if (acceptance_code == 0xFFFFFFFF)
    {
        // No filter
        acceptance_mask = 0xFFFFFFFF;
        acceptance_code = 0;
    }
    else
    {
        if (extId == true)
        {
            acceptance_code = (acceptance_code << (32 - 29));
            acceptance_mask = ~(CAN_EXTD_ID_MASK << (32 - 29));
        }
        else
        {
            acceptance_code = (acceptance_code << (32 - 11));
            acceptance_mask = ~(CAN_STD_ID_MASK << (32 - 11));
        }
    }

    filter = {
        .FM = Single_Mode,
        .ACR0 = byte(acceptance_code, 3),
        .ACR1 = byte(acceptance_code, 2),
        .ACR2 = byte(acceptance_code, 1),
        .ACR3 = byte(acceptance_code, 0),
        .AMR0 = byte(acceptance_mask, 3),
        .AMR1 = byte(acceptance_mask, 2),
        .AMR2 = byte(acceptance_mask, 1),
        .AMR3 = byte(acceptance_mask, 0),
    };

    CAN_Drv_ConfigFilter(&filter);

    // Init CAN Module
    CAN_Drv_Init(&CAN_cfg);
}

esp_err_t CAN_ReadFrame(CAN_frame_t *pframe, TickType_t ticks_to_wait)
{
    esp_err_t status;

    if ((CAN_cfg.rx_queue != NULL) && (xQueueReceive(CAN_cfg.rx_queue, pframe, ticks_to_wait) == pdTRUE))
    {
        status = ESP_OK;
        Serial.println("INFO: CAN Read Rx queue success");
    }
    else
    {
        status = ESP_FAIL;
    }

    return status;
}

esp_err_t CAN_WriteFrame(const CAN_frame_t *pframe, TickType_t ticks_to_wait)
{
    esp_err_t status = ESP_OK;

    if (CAN_cfg.tx_queue == NULL)
    {
        status = ESP_FAIL;
    }
    else
    {
        // Serial.printf("DEBUG MSGID <0x%X>, DLC <0x%X>, D <0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>\r\n",
        //               pframe->MsgID,
        //               pframe->FIR.B.DLC,
        //               pframe->data.u8[0],
        //               pframe->data.u8[1],
        //               pframe->data.u8[2],
        //               pframe->data.u8[3],
        //               pframe->data.u8[4],
        //               pframe->data.u8[5],
        //               pframe->data.u8[6],
        //               pframe->data.u8[7]);
        if (xQueueSendToBack(CAN_cfg.tx_queue, (void *)pframe, ticks_to_wait) != pdPASS)
        {
            Serial.println("ERROR: CAN Failed to queue Tx message");
            status = ESP_FAIL;
        }
    }

    return status;
}

void CAN_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    CAN_frame_t frame;
    CAN_error_t error;

    ESP_LOGI("CAN", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("CAN", "uxHighWaterMark = %d", uxHighWaterMark);

    CAN_Init();

    if (CAN_cfg.tx_queue == NULL)
    {
        Serial.println("ERROR: CAN Failed to create Tx Queue");
        vTaskDelete(NULL);
    }

    while (1)
    {
        if (xQueueReceive(CAN_cfg.tx_queue, (void *)&frame, portMAX_DELAY) == pdPASS)
        {
            // Serial.printf("DEBUG CAN MSG Pushed, MSGID <0x%X>, DLC <0x%X>, D <0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>,<0x%X>\r\n",
            //               frame.MsgID,
            //               frame.FIR.B.DLC,
            //               frame.data.u8[0],
            //               frame.data.u8[1],
            //               frame.data.u8[2],
            //               frame.data.u8[3],
            //               frame.data.u8[4],
            //               frame.data.u8[5],
            //               frame.data.u8[6],
            //               frame.data.u8[7]);
            CAN_Drv_WriteFrame(&frame);
        }

        if (xQueueReceive(CAN_cfg.err_queue, (void *)&error, (TickType_t)0) == pdPASS)
        {
            Serial.printf("ERROR: CAN RXERR <%d>, TXERR <%d>, IR <%X>\r\n", error.RXERR, error.TXERR, error.IR);
        }
    }

    Serial.println("ERROR: CAN task exit");
    vTaskDelete(NULL);
}