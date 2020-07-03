#include <Arduino.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/can.h"
#include "a_can.h"

//Initialize configuration structures using macro initializers
can_general_config_t CAN_GeneralConfig = {
    .mode = CAN_MODE_NORMAL,
    .tx_io = GPIO_NUM_5,
    .rx_io = GPIO_NUM_4,
    .clkout_io = (gpio_num_t)CAN_IO_UNUSED,
    .bus_off_io = (gpio_num_t)CAN_IO_UNUSED,
    .tx_queue_len = 100,
    .rx_queue_len = 100,
    .alerts_enabled = CAN_ALERT_NONE,
    .clkout_divider = 0
};
can_timing_config_t CAN_TimingConfig = CAN_TIMING_CONFIG_500KBITS();
can_filter_config_t CAN_FilterConfig = CAN_FILTER_CONFIG_ACCEPT_ALL();

void CAN_Init(void)
{
    //Install CAN driver
    if (can_driver_install(&CAN_GeneralConfig, &CAN_TimingConfig, &CAN_FilterConfig) == ESP_OK)
    {
        Serial.println("INFO: CAN Driver installed\n");
    }
    else
    {
        Serial.println("INFO: Failed to install CAN driver\n");
        return;
    }

    //Start CAN driver
    if (can_start() == ESP_OK)
    {
        Serial.println("INFO: CAN Driver started\n");
    }
    else
    {
        Serial.println("INFO: Failed to start CAN driver\n");
        return;
    }
}

void CAN_DeInit(void)
{
    //Stop the CAN driver
    if (can_stop() == ESP_OK)
    {
        Serial.println("INFO: CAN Driver stopped\n");
    }
    else
    {
        Serial.println("INFO: Failed to stop CAN driver\n");
        return;
    }

    //Uninstall the CAN driver
    if (can_driver_uninstall() == ESP_OK)
    {
        Serial.println("INFO: CAN Driver uninstalled\n");
    }
    else
    {
        Serial.println("INFO: Failed to uninstall CAN driver\n");
        return;
    }
}

void CAN_SetBaud(CAN_speed_t speed)
{
    CAN_DeInit();

    switch (speed)
    {
    case CAN_SPEED_100KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_100KBITS();
        break;

    case CAN_SPEED_125KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_125KBITS();
        break;

    case CAN_SPEED_250KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_250KBITS();
        break;

    case CAN_SPEED_500KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_500KBITS();
        break;

    case CAN_SPEED_800KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_800KBITS();
        break;

    case CAN_SPEED_1000KBPS:
        CAN_TimingConfig = CAN_TIMING_CONFIG_1MBITS();
        break;
    }

    // Init CAN Module
    CAN_Init();
}

void CAN_ConfigFilterterMask(uint32_t acceptance_code, bool extId)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))
    uint32_t acceptance_mask;

    CAN_DeInit();

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

    CAN_FilterConfig.acceptance_code = acceptance_code;
    CAN_FilterConfig.acceptance_mask = acceptance_mask;
    CAN_FilterConfig.single_filter = true;

    // Init CAN Module
    CAN_Init();
}

esp_err_t CAN_ReadFrame(can_message_t *pframe, TickType_t ticks_to_wait)
{
    esp_err_t status;

    status = can_receive(pframe, ticks_to_wait);

    return status;
}

esp_err_t CAN_WriteFrame(const can_message_t *pframe, TickType_t ticks_to_wait)
{
    esp_err_t status = can_transmit(pframe, ticks_to_wait);

    return status;
}

void CAN_Task(void *pvParameters)
{
    CAN_Init();

    vTaskDelete(NULL);
}