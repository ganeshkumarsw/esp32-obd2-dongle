#include <Arduino.h>
#include "config.h"
#include "a_led.h"
#include "a_ble.h"
#include "a_uart.h"
#include "a_can.h"
#include "a_wifi.h"
#include "a_mqtt.h"
#include "app.h"


void setup()
{
  Serial.begin(115200);
  Serial.setRxBufferSize(4096);
  printf("OBDII USB/Wifi/BT Dongle v%s\r\n", VERSION);

  LED_Init();
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

  ESP_LOGI("CREATE", "Task Started");

  uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGI("CREATE", "uxHighWaterMark = %d", uxHighWaterMark);

  if (xTaskCreate(LED_Task, "LED_Task", 8000, NULL, 1, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  if (xTaskCreate(APP_Task, "APP_Task", 15000, NULL, 9, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  if (xTaskCreate(UART_Task, "UART_Task", 15000, NULL, 8, NULL) != pdTRUE)
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

  if (xTaskCreate(CAN_Task, "CAN_Task", 10000, NULL, 8, NULL) != pdTRUE)
  {
    configASSERT(0);
  }

  vTaskDelete(NULL);
}