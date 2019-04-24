#include <Arduino.h>
#include <WiFi.h>
#include "a_wifi.h"

char WIFI_SSID[50] = "AndroidAP\0";
char WIFI_Password[50] = "amct8022\0";

void WIFI_Init(void)
{

}

void WIFI_Task(void *pvParameters)
{
    wl_status_t wifiStatus = WL_IDLE_STATUS;
    UBaseType_t uxHighWaterMark;

    Serial.println("WIFI_Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("WIFI uxHighWaterMark = %d\r\n", uxHighWaterMark);

    WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);
    Serial.print("Mac: ");
    Serial.println(WiFi.softAPmacAddress());

    while (1)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.print(".");
        }

        if (wifiStatus != WiFi.status())
        {
            if (wifiStatus == WL_CONNECTED)
            {
                Serial.println("");
                Serial.println("WiFi connected");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
            }

            wifiStatus = WiFi.status();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}