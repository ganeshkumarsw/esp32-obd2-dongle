#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include "SPIFFS.h"
#include "ESPmDNS.h"
#include "ESPAsyncWebServer.h"
#include "a_led.h"
#include "a_wifi.h"

AsyncWebServer server(80);

char WIFI_SSID[50] = STA_WIFI_SSID;
char WIFI_Password[50] = STA_WIFI_PASSWORD;

static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    WiFi.mode(WIFI_AP_STA);  //Both hotspot and client are enabled
    WiFi.onEvent(WIFI_EventCb, SYSTEM_EVENT_MAX);
    WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);
    WiFi.waitForConnectResult();

    WiFi.softAP("MyAP", "MasterSecond");

    if (MDNS.begin("myap"))
    {
        ESP_LOGI("WIFI", "MDNS responder started");
    }

    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    if (!SPIFFS.begin())
    {
        ESP_LOGI("WIFI", "An Error has occurred while mounting SPIFFS");
    }
    else
    {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/index.html", "text/html");
        });

        server.on("/jquery-3.4.0.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/jquery-3.4.0.min.js", "text/javascript");
        });

        server.on("/doc.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/doc.txt", "text/plain");
        });

        server.begin();
    }

    ESP_LOGI("WIFI", "AP IP address: %s", WiFi.softAPIP().toString().c_str());
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
                LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_HIGH);
            }
            else
            {
                LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_TOGGLE);
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