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
  CAN_Init();
  APP_Init();
  MQTT_Init();

  if (xTaskCreate(CreateTask_Task, "CreateTask_Task", 2000, NULL, 1, NULL) != pdTRUE)
  {
    configASSERT(0);
  }
}

void loop()
{
  vTaskDelay(5 / portTICK_PERIOD_MS);
  vTaskDelete(NULL);
}

void CreateTask_Task(void *pvParameters)
{
  UBaseType_t uxHighWaterMark;

  Serial.println("CreateTask_Task Started");

  uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  printf("CreateTask uxHighWaterMark = %d\r\n", uxHighWaterMark);

  if (xTaskCreate(HeartBeat_Task, "HeartBeat_Task", 1000, NULL, 1, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  if (xTaskCreate(APP_Task, "APP_Task", 10000, NULL, 1, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  if (xTaskCreate(UART_Task, "UART_Task", 10000, NULL, 8, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  // if (xTaskCreate(BLE_Task, "BLE_Task", 30000, NULL, 5, NULL) != pdTRUE)
  // {
  //   configASSERT(0);
  // }

  if (xTaskCreate(WIFI_Task, "WIFI_Task", 30000, NULL, 5, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  if (xTaskCreate(MQTT_Task, "MQTT_Task", 20000, NULL, 8, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  vTaskDelete(NULL);
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