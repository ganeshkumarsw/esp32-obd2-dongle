#include <Arduino.h>
#include "a_can.h"

//Initialize configuration structures using macro initializers
can_general_config_t CAN_g_config = {
    .mode = CAN_MODE_NORMAL,
    .tx_io = GPIO_NUM_5,
    .rx_io = GPIO_NUM_4,
    .clkout_io = (gpio_num_t)CAN_IO_UNUSED,
    .bus_off_io = (gpio_num_t)CAN_IO_UNUSED,
    .tx_queue_len = 10,
    .rx_queue_len = 25,
    .alerts_enabled = CAN_ALERT_NONE,
    .clkout_divider = 0,
};
can_timing_config_t CAN_t_config = CAN_TIMING_CONFIG_500KBITS();
can_filter_config_t CAN_f_config = {.acceptance_code = (0x00000029 << 3), .acceptance_mask = ~(CAN_EXTD_ID_MASK << 3), .single_filter = true};


void CAN_Init(void)
{
    //Install CAN driver
    if (can_driver_install(&CAN_g_config, &CAN_t_config, &CAN_f_config) == ESP_OK)
    {
        ESP_LOGI("CAN", "Driver installed");
    }
    else
    {
        ESP_LOGE("CAN", "Failed to install driver");
    }

    //Start CAN driver
    if (can_start() == ESP_OK)
    {
        ESP_LOGI("CAN", "Driver started");
    }
    else
    {
        ESP_LOGE("CAN", "Failed to start driver");
    }
}

void CAN_DeInit(void)
{
    //Start CAN driver
    if (can_stop() == ESP_OK)
    {
        ESP_LOGI("CAN", "Driver stopped");
    }
    else
    {
        ESP_LOGE("CAN", "Failed to stop driver");
    }

    //Install CAN driver
    if (can_driver_uninstall() == ESP_OK)
    {
        ESP_LOGI("CAN", "Driver uninstalled");
    }
    else
    {
        ESP_LOGE("CAN", "Failed to uninstall driver");
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

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool stdId)
{
    CAN_DeInit();

    if(stdId == true)
    {
        CAN_f_config = {.acceptance_code = (acceptance_code << (32 - 11)), .acceptance_mask = ~(CAN_STD_ID_MASK << (32 - 11)), .single_filter = true};
    }
    else
    {
        CAN_f_config = {.acceptance_code = (acceptance_code << (32 - 29)), .acceptance_mask = ~(CAN_EXTD_ID_MASK << (32 - 29)), .single_filter = true};
    }
    
    CAN_Init();
}

esp_err_t CAN_ReadFrame(can_message_t *frame)
{
    esp_err_t status;

    status = can_receive(frame, pdMS_TO_TICKS(5));

    if (status == ESP_OK)
    {
        ESP_LOGD("CAN", "Message received; ID is %d", frame->identifier);
        //Process received message
        if (frame->flags & CAN_MSG_FLAG_EXTD)
        {
            ESP_LOGD("CAN", "Message is in Extended Format");
        }
        else
        {
            ESP_LOGD("CAN", "Message is in Standard Format");
        }
    }
    else
    {

    }

    return status;
}

esp_err_t CAN_WriteFrame(can_message_t *frame)
{
    esp_err_t status;

    status = can_transmit(frame, pdMS_TO_TICKS(10));
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

    ESP_LOGI("CAN", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("CAN", "uxHighWaterMark = %d", uxHighWaterMark);

    while (1)
    {
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
        // }
        // else
        // {

        // }

        // if (!(message.flags & CAN_MSG_FLAG_RTR))
        // {
        //     for (int i = 0; i < message.data_length_code; i++)
        //     {
        //         Serial.print("ID is "); ("Data byte %d = %d\n", i, message.data[i]);
        //         Serial.print(message.identifier);
        //     }
        // }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}