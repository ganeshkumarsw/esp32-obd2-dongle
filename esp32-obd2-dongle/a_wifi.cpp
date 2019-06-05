#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include <WiFi.h>
#include "SPIFFS.h"
#include "ESPmDNS.h"
#include "ESPAsyncWebServer.h"
#include "a_led.h"
#include "a_wifi.h"

AsyncWebServer HttpServer(80);
WiFiServer SocketServer(6888);

char WIFI_SSID[50] = STA_WIFI_SSID;
char WIFI_Password[50] = STA_WIFI_PASSWORD;
uint8_t WIFI_RxBuff[4096];

static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    char apSSID[32];
    char apPass[63];
    char mac[6];
    Preferences preferences;

    preferences.begin("config", false);

    LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_TOGGLE, GPIO_TOGGLE_1HZ);
    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    // WiFi.macAddress((uint8_t *)mac);
    // sprintf(apSSID, "%s %x%x%x%x%x%x", AP_WIFI_SSID, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // preferences.putBytes("apSSID", apSSID, sizeof(apSSID));

    // sprintf(apPass, "%s", AP_WIFI_PASSWORD);
    // preferences.putBytes("apPASS", apPass, sizeof(apPass));

    unsigned int len = preferences.getBytes("apSSID", apSSID, sizeof(apSSID));
    if (len != sizeof(apSSID))
    {
        WiFi.macAddress((uint8_t *)mac);
        sprintf(apSSID, "%s %x%x%x%x%x%x", AP_WIFI_SSID, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        preferences.putBytes("apSSID", apSSID, sizeof(apSSID));
        preferences.getBytes("apSSID", apSSID, sizeof(apSSID));
    }
    // Serial.println(apSSID);

    len = preferences.getBytes("apPASS", apPass, sizeof(apPass));
    if (len != sizeof(apPass))
    {
        sprintf(apPass, "%s", AP_WIFI_PASSWORD);
        preferences.putBytes("apPASS", apPass, sizeof(apPass));
        preferences.getBytes("apPASS", apPass, sizeof(apPass));
    }
    // Serial.println(apPass);

    preferences.end();

    // WiFi.mode(WIFI_AP_STA);  //Both hotspot and client are enabled
    // WiFi.onEvent(WIFI_EventCb, SYSTEM_EVENT_MAX);

    // WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);
    // WiFi.waitForConnectResult();
    WiFi.softAP(apSSID, apPass);

    // if (MDNS.begin("myap"))
    // {
    //     ESP_LOGI("WIFI", "MDNS responder started");
    // }

    if (!SPIFFS.begin())
    {
        ESP_LOGI("WIFI", "An Error has occurred while mounting SPIFFS");
    }
    else
    {
        // File root = SPIFFS.open("/");
        // File file = root.openNextFile();

        // while (file)
        // {
        //     Serial.print("FILE: ");
        //     Serial.println(file.name());
        //     file.close();
        //     file = root.openNextFile();
        // }

        HttpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/index.html", "text/html");
        });

        HttpServer.on(
            "/login",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                Serial.println("Received post request");
                //List all parameters (Compatibility)
                int args = request->args();
                for (int i = 0; i < args; i++)
                {
                    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
                }

                request->send(SPIFFS, "/doc.txt", "text/plain");
            });

        HttpServer.on("/fsexplorer.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/fsexplorer.html", "text/html");
        });

        HttpServer.on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/favicon-32x32.png", "image/png");
        });

        HttpServer.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/jquery.min.js", "text/javascript");
        });

        HttpServer.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/bootstrap.min.css", "text/css");
        });

        HttpServer.on("/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/bootstrap.min.js", "text/javascript");
        });

        HttpServer.on("/autopeepal.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/autopeepal.png", "image/png");
        });

        // HttpServer.on("/doc.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        //     request->send(SPIFFS, "/doc.txt", "text/plain");
        // });

        HttpServer.begin();
    }

    ESP_LOGI("WIFI", "AP IP address: %s", WiFi.softAPIP().toString().c_str());
    SocketServer.begin();
    SocketServer.setTimeout(2);
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
        WiFiClient client = SocketServer.available();

        if (client)
        {
            while (client.connected())
            {

                if (client.available())
                {
                    // Serial.println("Client connected");
                    uint16_t len = client.read(WIFI_RxBuff, sizeof(WIFI_RxBuff));

                    if (len)
                    {
                        client.write(WIFI_RxBuff, len);
                    }

                    // client.stop();
                    Serial.println("Client disconnected");
                }
            }
            // else
            // {
            client.stop();
            Serial.println("Client disconnected");
            // }
        }
        // if (wifiStatus != WiFi.status())
        // {
        //     if (WiFi.status() == WL_CONNECTED)
        //     {
        //         ESP_LOGI("WIFI", "WiFi connected; IP address: %s", WiFi.localIP().toString().c_str());
        //         LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_HIGH, GPIO_TOGGLE_NONE);
        //     }
        //     else
        //     {
        //         LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_TOGGLE, GPIO_TOGGLE_1HZ);
        //     }
        // }

        // wifiStatus = WiFi.status();
        vTaskDelay(1 / portTICK_PERIOD_MS);
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