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

    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());
}

void WIFI_Task(void *pvParameters)
{
    wl_status_t wifiStatus = WL_IDLE_STATUS;
    UBaseType_t uxHighWaterMark;

    ESP_LOGI("WIFI", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("WIFI", "uxHighWaterMark = %d", uxHighWaterMark);

    WIFI_Init();

    while (1)
    {
        if (wifiStatus != WiFi.status())
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                ESP_LOGI("WIFI", "WiFi connected; IP address: %s", WiFi.localIP().toString().c_str());
            }
        }

        wifiStatus = WiFi.status();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void WIFI_EventCb(system_event_id_t event)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_GOT_IP:
        // Serial.print("IP address: ");
        // Serial.println(WiFi.localIP());
        break;

    case SYSTEM_EVENT_STA_CONNECTED:
        // Serial.println("WiFi connected");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI("WIFI", "Wifi Connecting.....");
        WiFi.reconnect();
        break;

    case SYSTEM_EVENT_AP_START:
        // Serial.print("SoftAP Ip: ");
        // Serial.println(WiFi.softAPIP().toString());
        break;

    case SYSTEM_EVENT_AP_STAIPASSIGNED:

        break;
    }
}