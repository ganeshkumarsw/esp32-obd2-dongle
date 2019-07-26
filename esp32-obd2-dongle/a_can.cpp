#include <Arduino.h>
#include "config.h"
#include <ESP32CAN.h>
#include <CAN_config.h>
#include "a_can.h"

#if 1
// Create a queue capable of containing 10 uint32_t values.
// QueueHandle_t CAN_CmdQueue;
// typedef struct CAN_Cmd
// {
//     uint8_t cmd;
//     uint32_t data;
// } CAN_Cmd_t;

CAN_device_t CAN_cfg = {
    .speed = CAN_SPEED_500KBPS,
    .tx_pin_id = GPIO_NUM_5,
    .rx_pin_id = GPIO_NUM_4,
    .rx_queue = NULL,
    .tx_queue = NULL,
}; // CAN Config
SemaphoreHandle_t CAN_SemaphoreRxTxQueue;

void CAN_Init(void)
{
    CAN_filter_t filter;

    if (CAN_cfg.rx_queue == NULL)
    {
        CAN_cfg.rx_queue = xQueueCreate(CAN_RX_QUEUE_SIZE, sizeof(CAN_frame_t));
    }

    if (CAN_cfg.tx_queue == NULL)
    {
        CAN_cfg.tx_queue = xQueueCreate(CAN_TX_QUEUE_SIZE, sizeof(CAN_frame_t));
    }

    if ((CAN_cfg.rx_queue == NULL) || (CAN_cfg.tx_queue == NULL))
    {
        Serial.println("CAN Failed to create queue message for either Rx / Tx");
    }
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

    ESP32Can.CANConfigFilter(&filter);

    // Init CAN Module
    ESP32Can.CANInit();
}

void CAN_DeInit(void)
{
    ESP32Can.CANStop();
}

void CAN_SetBaud(CAN_speed_t speed)
{
    ESP32Can.CANStop();

    CAN_cfg.speed = speed;

    // Init CAN Module
    ESP32Can.CANInit();
}

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))
    uint32_t acceptance_mask;
    CAN_filter_t filter;

    ESP32Can.CANStop();

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

    ESP32Can.CANConfigFilter(&filter);

    // Init CAN Module
    ESP32Can.CANInit();
}

esp_err_t CAN_ReadFrame(CAN_frame_t *frame, TickType_t ticks_to_wait)
{
    esp_err_t status;

    if ((CAN_cfg.rx_queue != NULL) && (xQueueReceive(CAN_cfg.rx_queue, frame, ticks_to_wait) == pdTRUE))
    {
        status = ESP_OK;
    }
    else
    {
        status = ESP_FAIL;
    }

    return status;
}

esp_err_t CAN_WriteFrame(CAN_frame_t *frame, TickType_t ticks_to_wait)
{
    esp_err_t status = ESP_OK;

    if (CAN_cfg.tx_queue == NULL)
    {
        status = ESP_FAIL; 
    }
    else
    {
        if (xQueueSend(CAN_cfg.tx_queue, (void *)frame, (TickType_t)(5 / portTICK_PERIOD_MS)) != pdPASS)
        {
            Serial.println("CAN Failed to queue Tx message");
            status = ESP_FAIL; 
        }
    }

    return status;
}

void CAN_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    CAN_frame_t frame;

    //Configure message to transmit
    // can_message_t message;
    // CAN_Cmd_t canCmd;
    // CAN_speed_t baud;
    // bool extId;
    // uint32_t acceptanceCode;

    ESP_LOGI("CAN", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("CAN", "uxHighWaterMark = %d", uxHighWaterMark);

    // CAN_CmdQueue = xQueueCreate(10, sizeof(CAN_Cmd_t));
    // if (CAN_CmdQueue == NULL)
    // {
    //     ESP_LOGE("CAN", "Failed to create queue message for command");
    // }

    // CAN_Init();

    while (1)
    {
        if (CAN_TxQueue != NULL)
        {
            if (xQueueReceive(CAN_cfg.tx_queue, (void *)&frame, portMAX_DELAY) == pdPASS)
            {
                ESP32Can.CANWriteFrame(&frame);
            }
        }

        // if (CAN_CmdQueue != NULL)
        // {
        //     if (xQueueReceive(CAN_CmdQueue, (void *)&canCmd, portMAX_DELAY) == pdPASS)
        //     {
        //         switch (canCmd.cmd)
        //         {
        //         case 0:
        //             CAN_Init();
        //             break;

        //         case 1:
        //             // CAN_DeInit();
        //             break;

        //         case 2:
        //             extId = canCmd.data & 0x80000000 ? true : false;
        //             acceptanceCode = canCmd.data & 0x40000000 ? 0xFFFFFFFF : canCmd.data & 0x1FFFFFFF;
        //             CAN_ConfigFilterterMask(acceptanceCode, extId);
        //             break;

        //         case 3:
        //             baud = (CAN_speed_t)canCmd.data;
        //             CAN_SetBaud(baud);
        //             break;
        //         }
        //     }
        // }

        // vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void CAN_SendCmd(uint8_t cmd, uint32_t data)
{
    // CAN_Cmd_t canCmd = {.cmd = cmd, .data = data};

    // if (xQueueSend(CAN_CmdQueue, (void *)&canCmd, (TickType_t)(10 / portTICK_PERIOD_MS)) != pdPASS)
    // {
    //     ESP_LOGE("CAN", "Failed to queue message for command");
    // }
}
#else

// Create a queue capable of containing 10 uint32_t values.
QueueHandle_t CAN_CmdQueue;
typedef struct CAN_Cmd
{
    uint8_t cmd;
    uint32_t data;
} CAN_Cmd_t;

//Initialize configuration structures using macro initializers
can_general_config_t CAN_g_config = {
    .mode = CAN_MODE_NORMAL,
    .tx_io = GPIO_NUM_5,
    .rx_io = GPIO_NUM_4,
    .clkout_io = (gpio_num_t)CAN_IO_UNUSED,
    .bus_off_io = (gpio_num_t)CAN_IO_UNUSED,
    .tx_queue_len = 25,
    .rx_queue_len = 25,
    .alerts_enabled = CAN_ALERT_NONE,
    .clkout_divider = 0,
};
can_timing_config_t CAN_t_config = CAN_TIMING_CONFIG_500KBITS();
can_filter_config_t CAN_f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();

void CAN_Init(void)
{
    //Install CAN driver
    if (can_driver_install(&CAN_g_config, &CAN_t_config, &CAN_f_config) == ESP_OK)
    {
        Serial.println("CAN Driver installed");
    }
    else
    {
        Serial.println("CAN driver Failed to install");
    }

    //Start CAN driver
    if (can_start() == ESP_OK)
    {
        Serial.println("CAN Driver started");
    }
    else
    {
        Serial.println("CAN driver failed to start");
    }
}

void CAN_DeInit(void)
{
    can_message_t message;
    can_status_info_t status_info;

    // esp_task_wdt_init(100, 1);
    // esp_task_wdt_reset();

    //Start CAN driver
    if (can_stop() == ESP_OK)
    {
        Serial.println("CAN Driver stopped");
    }
    else
    {
        Serial.println("CAN driver Failed to stop");
    }

    // while (can_receive(&message, 0) == ESP_OK);
    // while ((can_get_status_info(&status_info) == ESP_OK) && ((status_info.msgs_to_tx != 0) || (status_info.msgs_to_rx != 0)))
    // {
    // vTaskDelay(10 / portTICK_PERIOD_MS);
    // }

    //Uninstall CAN driver
    if (can_driver_uninstall() == ESP_OK)
    {
        Serial.println("CAN Driver uninstalled");
    }
    else
    {
        Serial.println("CAN driver failed to uninstall");
    }
}

void CAN_SetBaud(CAN_speed_t speed)
{
    CAN_DeInit();

    switch (speed)
    {
    case CAN_SPEED_250KBPS:
        CAN_t_config = CAN_TIMING_CONFIG_250KBITS();
        break;

    case CAN_SPEED_500KBPS:
        CAN_t_config = CAN_TIMING_CONFIG_500KBITS();
        break;

    case CAN_SPEED_1000KBPS:
        CAN_t_config = CAN_TIMING_CONFIG_1MBITS();
        break;
    }

    CAN_Init();
}

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId)
{
    CAN_DeInit();

    if (acceptance_code == 0xFFFFFFFF)
    {
        // No filter
        CAN_f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
    }
    else
    {
        if (extId == true)
        {
            CAN_f_config = {.acceptance_code = (acceptance_code << (32 - 29)), .acceptance_mask = ~(CAN_EXTD_ID_MASK << (32 - 29)), .single_filter = true};
        }
        else
        {
            CAN_f_config = {.acceptance_code = (acceptance_code << (32 - 11)), .acceptance_mask = ~(CAN_STD_ID_MASK << (32 - 11)), .single_filter = true};
        }
    }

    CAN_Init();
}

esp_err_t CAN_ReadFrame(can_message_t *frame, TickType_t ticks_to_wait)
{
    esp_err_t status;

    status = can_receive(frame, ticks_to_wait);

    if (status == ESP_OK)
    {
        // ESP_LOGD("CAN", "Message received; ID is %d", frame->identifier);
        // //Process received message
        // if (frame->flags & CAN_MSG_FLAG_EXTD)
        // {
        //     ESP_LOGD("CAN", "Message is in Extended Format");
        // }
        // else
        // {
        //     ESP_LOGD("CAN", "Message is in Standard Format");
        // }
    }
    else
    {
    }

    return status;
}

esp_err_t CAN_WriteFrame(can_message_t *frame, TickType_t ticks_to_wait)
{
    esp_err_t status;

    status = can_transmit(frame, ticks_to_wait);
    //Queue message for transmission
    if (status != ESP_OK)
    {
        ESP_LOGE("CAN", "Failed to queue message for transmission");
    }
    else
    {
    }

    return status;
}

void CAN_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    //Configure message to transmit
    can_message_t message;
    CAN_Cmd_t canCmd;
    CAN_speed_t baud;
    bool extId;
    uint32_t acceptanceCode;

    ESP_LOGI("CAN", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("CAN", "uxHighWaterMark = %d", uxHighWaterMark);

    CAN_CmdQueue = xQueueCreate(10, sizeof(CAN_Cmd_t));
    if (CAN_CmdQueue == NULL)
    {
        ESP_LOGE("CAN", "Failed to create queue message for command");
    }

    CAN_Init();

    while (1)
    {
        if (CAN_CmdQueue != NULL)
        {
            if (xQueueReceive(CAN_CmdQueue, (void *)&canCmd, portMAX_DELAY) == pdPASS)
            {
                switch (canCmd.cmd)
                {
                case 0:
                    CAN_Init();
                    break;

                case 1:
                    CAN_DeInit();
                    break;

                case 2:
                    extId = canCmd.data & 0x80000000 ? true : false;
                    acceptanceCode = canCmd.data & 0x40000000 ? 0xFFFFFFFF : canCmd.data & 0x1FFFFFFF;
                    CAN_ConfigFilterterMask(acceptanceCode, extId);
                    break;

                case 3:
                    baud = (CAN_speed_t)canCmd.data;
                    CAN_SetBaud(baud);
                    break;
                }
            }
        }
        // message.identifier = 0xAAAA;
        // message.flags = CAN_MSG_FLAG_EXTD;
        // message.data_length_code = 4;
        // for (int i = 0; i < 4; i++)
        // {
        //     message.data[i] = 0;
        // }

        // //Queue message for transmission
        // if (can_transmit(&message, pdMS_TO_TICKS(10)) != ESP_OK)
        // {
        //     ESP_LOGE("CAN", "Failed to queue message for transmission");
        // }
        // else
        // {
        // }

        // //Wait for message to be received
        // can_message_t message;
        // if (can_receive(&message, pdMS_TO_TICKS(5)) == ESP_OK)
        // {
        //     ESP_LOGD("CAN", "Message received; ID is %d", message.identifier);
        //     //Process received message
        //     if (message.flags & CAN_MSG_FLAG_EXTD)
        //     {
        //         ESP_LOGD("CAN", "Message is in Extended Format");
        //     }
        //     else
        //     {
        //         ESP_LOGD("CAN", "Message is in Standard Format");
        //     }

        //     if (!(message.flags & CAN_MSG_FLAG_RTR))
        //     {
        //         for (int i = 0; i < message.data_length_code; i++)
        //         {
        //             Serial.print("ID is ");
        //             ("Data byte %d = %d\n", i, message.data[i]);
        //             Serial.print(message.identifier);
        //         }
        //     }
        // }
        // else
        // {
        // }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void CAN_SendCmd(uint8_t cmd, uint32_t data)
{
    CAN_Cmd_t canCmd = {.cmd = cmd, .data = data};

    if (xQueueSend(CAN_CmdQueue, (void *)&canCmd, (TickType_t)(10 / portTICK_PERIOD_MS)) != pdPASS)
    {
        ESP_LOGE("CAN", "Failed to queue message for command");
    }
}

#endif
