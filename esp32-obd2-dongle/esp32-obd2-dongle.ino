#include "config.h"
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
  APP_Init();

  xTaskCreate(HeartBeat_Task, "HeartBeat_Task", 1000, NULL, 1, NULL);
  xTaskCreate(APP_Task, "APP_Task", 10000, NULL, 1, NULL);
}

void loop()
{

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
    digitalWrite(GPIO_NUM_33, ledState);
  }
}