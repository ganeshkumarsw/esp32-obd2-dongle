#include <Arduino.h>
#include "config.h"
#include "a_ble.h"
#include "a_uart.h"
#include "a_can.h"
#include "a_wifi.h"
#include "a_mqtt.h"
#include "app.h"

void GPIO_Init(void)
{
  pinMode(GPIO_NUM_18, OUTPUT);
  pinMode(GPIO_NUM_19, OUTPUT);
  pinMode(GPIO_NUM_25, OUTPUT);
  pinMode(GPIO_NUM_26, OUTPUT);
  pinMode(GPIO_NUM_27, OUTPUT);
  pinMode(GPIO_NUM_32, OUTPUT);
  pinMode(GPIO_NUM_33, OUTPUT);
}

void setup()
{
  Serial.begin(115200);
  printf("OBDII USB/Wifi/BT Dongle\r\n");

  GPIO_Init();
  CAN_Init(CAN_SPEED_1000KBPS);
  BLE_Init();
  APP_Init();
  WIFI_Init();
  //MQTT_Init();

  TASK_Init();
}

void loop()
{
}

void TASK_Init(void)
{
  TaskHandle_t taskHandle = NULL;

  xTaskCreate(HeartBeat_Task, "HeartBeat_Task", 1000, NULL, 1, &taskHandle);
  configASSERT(taskHandle);

  taskHandle = NULL;
  xTaskCreate(APP_Task, "APP_Task", 10000, NULL, 1, &taskHandle);
  configASSERT(taskHandle);

  taskHandle = NULL;
  xTaskCreate(BLE_Task, "BLE_Task", 10000, NULL, 8, &taskHandle);
  configASSERT(taskHandle);

  taskHandle = NULL;
  xTaskCreate(UART_Task, "UART_Task", 10000, NULL, 8, &taskHandle);
  configASSERT(taskHandle);

  taskHandle = NULL;
  xTaskCreate(WIFI_Task, "WIFI_Task", 30000, NULL, 5, &taskHandle);
  configASSERT(taskHandle);

  taskHandle = NULL;
  //xTaskCreate(MQTT_Task, "MQTT_Task", 20000, NULL, 8, &taskHandle);
  //configASSERT(taskHandle);
}

void HeartBeat_Task(void *pvParameters)
{
  uint8_t ledState = LOW;

  while (1)
  {
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW)
    {
      ledState = HIGH;
    }
    else
    {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(HEART_BEAT_LED, ledState);
  }
}