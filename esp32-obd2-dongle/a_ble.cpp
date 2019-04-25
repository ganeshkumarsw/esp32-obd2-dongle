#include <Arduino.h>
#include "BluetoothSerial.h"
#include "a_ble.h"

BluetoothSerial SerialBT;
uint8_t BLE_Buff[4096];

void BLE_Init(void)
{
    SerialBT.begin("OBDII USB/Wifi/BT Dongle"); //Bluetooth device name
}

void BLE_Task(void *pvParameters)
{
    uint16_t idx;
    uint16_t len;
    UBaseType_t uxHighWaterMark;

    Serial.println("BLE_Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("BLE uxHighWaterMark = %d\r\n", uxHighWaterMark);

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
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}