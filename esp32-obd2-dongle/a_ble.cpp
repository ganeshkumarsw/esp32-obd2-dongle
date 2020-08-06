#include <Arduino.h>
#include "config.h"
#include "app.h"
#include "a_led.h"
#include "BluetoothSerial.h"
#include "a_ble.h"

BluetoothSerial SerialBT;
uint8_t BLE_Buff[4096];
SemaphoreHandle_t BLE_SemTxComplete;



void BLE_Init(void)
{
    SerialBT.begin("OBDII BT Dongle"); //Bluetooth device name
    BLE_SemTxComplete = xSemaphoreCreateBinary();

    if (BLE_SemTxComplete == NULL)
    {
        Serial_println("ERROR: Failed to create BLE Tx complete semaphore");
    }
    else
    {
        xSemaphoreGive(BLE_SemTxComplete);
    }
}

void BLE_Task(void *pvParameters)
{
    uint16_t idx;
    uint16_t len;
    // UBaseType_t uxHighWaterMark;

    // ESP_LOGI("BLE", "Task Started");

    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // ESP_LOGI("BLE", "uxHighWaterMark = %d", uxHighWaterMark);

    BLE_Init();

    while (1)
    {
        idx = 0;
        len = 0;

        while (len != SerialBT.available())
        {
            len = SerialBT.available();
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }

        while (SerialBT.available())
        {
            BLE_Buff[idx++] = SerialBT.read();

            if (idx >= sizeof(BLE_Buff))
            {
                break;
            }
        }

        if (len != 0)
        {
            APP_ProcessData(BLE_Buff, idx, APP_MSG_CHANNEL_BLE);
        }

        if (SerialBT.hasClient() == true)
        {
            LED_SetLedState(WIFI_CONN_LED, LED_STATE_ON, LED_TOGGLE_RATE_NONE);
        }
        else
        {
            LED_SetLedState(WIFI_CONN_LED, LED_STATE_ON, LED_TOGGLE_RATE_1HZ);
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void BLE_Write(uint8_t *payLoad, uint16_t len)
{
    if (BLE_SemTxComplete == NULL)
    {
        return;
    }

    xSemaphoreTake(BLE_SemTxComplete, portMAX_DELAY);

    if (SerialBT.hasClient() == true)
    {
        SerialBT.write(payLoad, len);
    }

    xSemaphoreGive(BLE_SemTxComplete);
}