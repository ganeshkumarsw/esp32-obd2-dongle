#include <Arduino.h>
#include "BluetoothSerial.h"
#include "a_ble.h"

BluetoothSerial SerialBT;
uint8_t BLE_Buff[4096];

void BLE_Init(void)
{

}

void BLE_Task(void *pvParameters)
{

    while (1)
    {
        if (SerialBT.available())
        {
            BLE_Buff[0] = SerialBT.read();
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}