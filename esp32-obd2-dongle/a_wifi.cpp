#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include <WiFi.h>
#include "SPIFFS.h"
#include "ESPmDNS.h"
#include <ArduinoJson.h>
#include "ESPAsyncWebServer.h"
#include "app.h"
#include "a_led.h"
#include "a_wifi.h"

AsyncWebServer HttpServer(80);
AsyncWebSocket WebSocket("/ws"); // access at ws://[esp ip]/ws
WiFiServer SocketServer(6888);

char WIFI_SSID[50] = STA_WIFI_SSID;
char WIFI_Password[50] = STA_WIFI_PASSWORD;
uint8_t WIFI_RxBuff[4130];
uint8_t WIFI_TxBuff[4130];
uint16_t WIFI_TxLen;

static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    // char apSSID[32];
    // char apPass[63];
    // char mac[6];
    // Preferences preferences;

    // preferences.begin("config", false);

    LED_SetLedState(WIFI_CONN_LED, GPIO_STATE_TOGGLE, GPIO_TOGGLE_1HZ);
    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    // // WiFi.macAddress((uint8_t *)mac);
    // // sprintf(apSSID, "%s %x%x%x%x%x%x", AP_WIFI_SSID, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // // preferences.putBytes("apSSID", apSSID, sizeof(apSSID));

    // // sprintf(apPass, "%s", AP_WIFI_PASSWORD);
    // // preferences.putBytes("apPASS", apPass, sizeof(apPass));

    // unsigned int len = preferences.getBytes("apSSID", apSSID, sizeof(apSSID));
    // if (len != sizeof(apSSID))
    // {
    // WiFi.macAddress((uint8_t *)mac);
    // sprintf(apSSID, "%s %x%x%x%x%x%x", AP_WIFI_SSID, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    //     preferences.putBytes("apSSID", apSSID, sizeof(apSSID));
    //     preferences.getBytes("apSSID", apSSID, sizeof(apSSID));
    // }
    // // Serial.println(apSSID);

    // len = preferences.getBytes("apPASS", apPass, sizeof(apPass));
    // if (len != sizeof(apPass))
    // {
    // sprintf(apPass, "%s", AP_WIFI_PASSWORD);
    //     preferences.putBytes("apPASS", apPass, sizeof(apPass));
    //     preferences.getBytes("apPASS", apPass, sizeof(apPass));
    // }
    // // Serial.println(apPass);

    // preferences.end();

    // WiFi.mode(WIFI_AP_STA);  //Both hotspot and client are enabled
    // WiFi.onEvent(WIFI_EventCb, SYSTEM_EVENT_MAX);
    // const IPAddress apIP = IPAddress(192, 168, 5, 1);

    // WiFi.waitForConnectResult();
    WiFi.mode(WIFI_AP);
    // WiFi.softAPConfig(IPAddress(192, 168, 5, 1), IPAddress(192, 168, 5, 1), IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP("MyAp_Test", "password1"))
    {
        Serial.println("ESP32 SoftAP failed to start!");
    }

    if (!WiFi.softAPsetHostname("esp32"))
    {
        Serial.println("ESP32 SoftAP failed to set host name!");
    }

    // if (!WiFi.softAPenableIpV6())
    // {
    //     Serial.println("ESP32 SoftAP IpV6 failed to start!");
    // }

    // Serial.println(WiFi.softAPIPv6().toString());

    vTaskDelay(500 / portTICK_PERIOD_MS);
    // WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);

    // if (!MDNS.begin("esp32"))
    // {
    //     Serial.println("Error setting up MDNS responder!");
    //     while (1)
    //     {
    //         delay(1000);
    //     }
    // }
    // Serial.println("mDNS responder started");

    if (!SPIFFS.begin())
    {
        ESP_LOGI("WIFI", "An Error has occurred while mounting SPIFFS");
    }
    else
    {
        // attach AsyncWebSocket
        WebSocket.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            if (type == WS_EVT_CONNECT)
            {
                Serial.println("Websocket client connection received");
            }
            else if (type == WS_EVT_DISCONNECT)
            {
                Serial.println("Client disconnected");
            }
            else if (type == WS_EVT_DATA)
            {
                Serial.println("Data received: ");

                for (int i = 0; i < len; i++)
                {
                    Serial.print((char)data[i]);
                }

                Serial.println();
            }
        });

        HttpServer.addHandler(&WebSocket);

        HttpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/index.html", "text/html");
        });

        HttpServer.on(
            "/login",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                // Serial.println("Received post request");
                //List all parameters (Compatibility)
                int args = request->args();
                for (int i = 0; i < args; i++)
                {
                    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
                }

                request->send(200);
            });

        HttpServer.on(
            "/fsread",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                // DynamicJsonDocument doc(8000);
                String json;
                // Add an array.
                //
                // JsonArray data = doc.createNestedArray("data");

                File root = SPIFFS.open("/");
                File file = root.openNextFile();
                json = "{\"data\":[";
                while (file)
                {
                    json = json + "\"" + file.name() + "\",";
                    file.close();
                    file = root.openNextFile();
                }

                json = json + "\"end\"]}";
                request->send(200, "text/plain", json);
            });

        HttpServer.on(
            "/fsdelete",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                request->send(200);
            });

        HttpServer.on("/fsexplorer", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/fsexplorer.html.gz", "text/html");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/terminal", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/terminal.html", "text/html");
            // response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/favicon-32x32.png", "image/png");
        });

        HttpServer.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/jquery.min.js.gz", "text/javascript");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/bootstrap.min.css.gz", "text/css");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/bootstrap.min.js.gz", "text/javascript");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
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
            uint16_t len = 0;
            Serial.println("Client connected");

            while (client.connected())
            {
                while (client.available())
                {
                    len = client.read(WIFI_RxBuff, sizeof(WIFI_RxBuff));
                }

                if (len)
                {
                    APP_ProcessData(&WIFI_RxBuff[11], (len - 13), APP_CHANNEL_TCP_SOC);
                    client.write(WIFI_RxBuff, len);
                    len = 0;
                }

                if (WIFI_TxLen)
                {
                    client.write(WIFI_TxBuff, WIFI_TxLen);
                    WIFI_TxLen = 0;
                }

                vTaskDelay(1 / portTICK_PERIOD_MS);
            }

            // client.stop();
            Serial.println("Client disconnected");
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

void WIFI_Write(uint8_t *payLoad, uint16_t len)
{
    Serial.println("App processed");
    // WiFiClient client = SocketServer.available();

    if (!WIFI_TxLen)
    {
        // if (client.connected())
        {
            memcpy(WIFI_TxBuff, payLoad, len);
            WIFI_TxLen = len;
        }
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

static void WIFI_WebSocketOnEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    //Handle WebSocket event
}