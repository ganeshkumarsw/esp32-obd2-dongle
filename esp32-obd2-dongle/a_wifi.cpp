#include <Arduino.h>
#include <WiFi.h>
#include "a_wifi.h"

char WIFI_SSID[50] = "AndroidAP";
char WIFI_Password[50] = "amct8022";

static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    WiFi.onEvent(WIFI_EventCb, SYSTEM_EVENT_MAX);
    WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);
    WiFi.waitForConnectResult();

    // WiFi.softAP("MyAP", "MasterSecond");
    // WiFi.softAPsetHostname("MyAP");

    Serial.print("Mac: ");
    Serial.println(WiFi.macAddress());

    
}

void WIFI_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;

    Serial.println("WIFI_Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("WIFI uxHighWaterMark = %d\r\n", uxHighWaterMark);

    WIFI_Init();

    while (1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void WIFI_EventCb(system_event_id_t event)
{
    switch (event)
    {
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("WiFi connected");
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Wifi Connecting.....");
            WiFi.reconnect();
            break;    

        case SYSTEM_EVENT_AP_START:
            Serial.print("SoftAP Ip: ");
            Serial.println(WiFi.softAPIP().toString());
            break;

        case SYSTEM_EVENT_AP_STAIPASSIGNED:

            break;
    }
}