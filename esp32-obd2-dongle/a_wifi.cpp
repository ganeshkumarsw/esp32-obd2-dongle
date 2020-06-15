#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "version.h"
#include "util.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "app.h"
#include "a_led.h"
#include "a_wifi.h"
#include <DNSServer.h>
#include <Update.h>

#define WIFI_AP 0
#define WIFI_AP_DNS 0
#define WIFI_STA 0

#if WIFI_AP_DNS
const byte DNS_PORT = 53;
DNSServer DNS_Server;
#endif

AsyncWebServer HttpServer(80);
AsyncWebSocket WebSocket("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource Events("/events");
AsyncWebSocketClient *p_WebSocketClient;
File FsUploadFile;
WiFiServer SocketServer(6888);

char WIFI_STA_SSID[50] = STA_WIFI_SSID;
char WIFI_STA_Password[50] = STA_WIFI_PASSWORD;
uint8_t WIFI_SeqNo;
uint8_t WIFI_RxBuff[4130];
uint8_t WIFI_TxBuff[4130];
uint16_t WIFI_TxLen;

static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    wl_status_t wifiStatus;
    bool staConnected = false;
    char apSSID[32];
    char apPass[63];
    char mac[6];
    Preferences preferences;

    preferences.begin("config", false);

    LED_SetLedState(WIFI_CONN_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_1HZ);
    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    WiFi.macAddress((uint8_t *)mac);
    sprintf(apSSID, "%s-%02X%02X", AP_WIFI_SSID, mac[4], mac[5]);
    Serial.printf("SSID: %s\r\n", apSSID);
    if (preferences.getString("apSSID").equals(apSSID) == 0)
    {
        Serial.println("apSSID is corrupted or diff updated");
        preferences.putString("apSSID", apSSID);
    }
    else
    {
        Serial.println("apSSID is same");
        preferences.putString("apSSID", apSSID);
    }

    if (preferences.getString("apPASS") == "")
    {
        Serial.println("apPASS Key is missing");
        preferences.putString("apPASS", AP_WIFI_PASSWORD);
    }
    else
    {
        Serial.println("apPASS Key is available");
    }

    if (preferences.getString("stSSID") == "")
    {
        Serial.println("stSSID Key is missing, updated SSID through command");
    }

    if (preferences.getString("stPASS") == "")
    {
        Serial.println("stPASS Key is missing, updated SSID through command");
    }

    // WiFi.mode(WIFI_AP_STA); //Both hotspot and client are enabled
    // WiFi.onEvent(WIFI_EventCb, SYSTEM_EVENT_MAX);
    // const IPAddress apIP = IPAddress(192, 168, 5, 1);

    // WiFi.scanNetworks will return the number of networks found
    WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());

    if (!MDNS.begin("obd2"))
    {
        Serial.println("Error setting up MDNS responder!");
        // while (1)
        // {
        //     delay(1000);
        // }
    }
    Serial.println("mDNS responder started");
    // Add service to MDNS-SD
    MDNS.addService("_http", "_tcp", 80);

    int n = 0; //WiFi.scanNetworks();
    Serial.println("Scan done");
    if (n == 0)
    {
        Serial.println("No networks found");
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            Serial.println("SSID: " + WiFi.SSID(i));
            // Print SSID and RSSI for each network found
            if (WiFi.SSID(i).equals(preferences.getString("stSSID")) == true)
            {
                staConnected = true;
                delay(10);
                wifiStatus = WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());
                WiFi.setSleep(false);
                Serial.println("WIFI STA begin");
                // Serial.println(wifiStatus);

                if (WiFi.getAutoConnect() == false)
                {
                    WiFi.setAutoConnect(true);
                }

                if (WiFi.getAutoReconnect() == false)
                {
                    WiFi.setAutoReconnect(true);
                }

                // if (!MDNS.begin("obd2"))
                // {
                //     Serial.println("Error setting up MDNS responder!");
                //     // while (1)
                //     // {
                //     //     delay(1000);
                //     // }
                // }
                // Serial.println("mDNS responder started");
                // // Add service to MDNS-SD
                // MDNS.addService("_http", "_tcp", 80);
                break;
            }
        }
    }

    staConnected = true;
    if (staConnected == false)
    {
        Serial.println("WIFI AP begin");
        if (!WiFi.softAP(preferences.getString("apSSID").c_str(), preferences.getString("apPASS").c_str()))
        {
            Serial.println("ESP32 SoftAP failed to start!");
        }
    }

    WiFi.setHostname("ESP32_OBD2");

#if WIFI_AP
    if (!WiFi.softAP("OBD DONGLE", "password1"))
    {
        Serial.println("ESP32 SoftAP failed to start!");
    }
#endif

#if WIFI_AP_DNS
    // modify TTL associated  with the domain name (in seconds)
    // default is 60 seconds
    DNS_Server.setTTL(300);
    // set which return code will be used for all other domains (e.g. sending
    // ServerFailure instead of NonExistentDomain will reduce number of queries
    // sent by clients)
    // default is DNSReplyCode::NonExistentDomain
    DNS_Server.setErrorReplyCode(DNSReplyCode::ServerFailure);

    // start DNS server for a specific domain name
    DNS_Server.start(DNS_PORT, "*", WiFi.softAPIP());
#endif

#if WIFI_STA
    wifiStatus = WiFi.begin((char *)WIFI_SSID, (char *)WIFI_Password);
    Serial.print("WIFI begin status: ");
    Serial.println(wifiStatus);

    if (!MDNS.begin("obd2"))
    {
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");
#endif

    if (!SPIFFS.begin())
    {
        ESP_LOGI("WIFI", "An Error has occurred while mounting SPIFFS");
    }
    else
    {
        // attach AsyncWebSocket
        WebSocket.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            uint16_t crc16Act;
            uint16_t crc16Calc;
            uint8_t buff[30];
            uint16_t crc16;

            if (type == WS_EVT_CONNECT)
            {
                p_WebSocketClient = client;
                Serial.println("Websocket client connection received");
            }
            else if (type == WS_EVT_DISCONNECT)
            {
                p_WebSocketClient = NULL;
                Serial.println("Client disconnected");
            }
            else if (type == WS_EVT_DATA)
            {
                p_WebSocketClient = client;
                WIFI_SeqNo = data[0];
                crc16Act = ((uint16_t)data[len - 2] << 8) | (uint16_t)data[len - 1];
                crc16Calc = UTIL_CRC16_CCITT(0xFFFF, data, (len - 2));

                if (crc16Act == crc16Calc)
                {
                    APP_ProcessData(&data[11], (len - 13), APP_CHANNEL_WEB_SOC);
                }
                else
                {
                    len = 0;
                    buff[len++] = 0x20;
                    buff[len++] = 2 + 2 + 4;
                    buff[len++] = APP_RESP_NACK;
                    buff[len++] = APP_RESP_NACK_15;
                    buff[len++] = crc16Act >> 8;
                    buff[len++] = crc16Act;
                    buff[len++] = crc16Calc >> 8;
                    buff[len++] = crc16Calc;
                    crc16 = UTIL_CRC16_CCITT(0xFFFF, &buff[2], (len - 2));
                    buff[len++] = crc16 >> 8;
                    buff[len++] = crc16;
                }
                // Serial.print("Data received: ");

                // for (int i = 0; i < len; i++)
                // {
                //     Serial.print((char)data[i], HEX);
                //     Serial.print(' ');
                // }
                // Serial.println();
            }
        });

        Events.onConnect([](AsyncEventSourceClient *client) {
            if (client->lastId())
            {
                Serial.printf("Client reconnected! Last message ID that it got is: %u\r\n", client->lastId());
            }
            //send event with message "hello!", id current millis
            // and set reconnect delay to 1 second
            client->send("hello!", NULL, millis(), 1000);
        });

        HttpServer.addHandler(&WebSocket);
        HttpServer.addHandler(&Events);

        HttpServer.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404);
        });

        HttpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/index.html", "text/html");
        });

        // HTTP basic authentication
        HttpServer.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (!request->authenticate("admin", "admin"))
                return request->requestAuthentication();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Login Success!");
            response->addHeader("Connection", "close");
            request->send(response);
        });

        // Simple Firmware Update Form
        // HttpServer.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        //     AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<form method='POST' action='/flash' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
        //     response->addHeader("Connection", "close");
        //     request->send(response);
        // });

        HttpServer.on(
            "/flash",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if ((request->arg("username").equals("admin") == true) && (request->arg("password").equals("ADmiNPaSSworD") == true))
                {
                    if (Update.hasError() == false)
                    {
                        AsyncWebServerResponse *response = request->beginResponse(200);
                        response->addHeader("Connection", "close");

                        Events.send("Device being restarted", "success", millis());
                        Serial.println("Device being restarted");
                        request->send(response);
                        delay(100);
                        ESP.restart();
                    }
                    else
                    {
                        Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", String("Update Error: ") + Update.getError());
                        request->send(response);
                    }
                    
                }
                else
                {
                    AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication Failed");
                    request->send(response);
                }
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                if (!index)
                {
                    //   Serial.printf("Update Start: %s\n", filename.c_str());
                    //   Update.runAsync(true);
                    if ((request->arg("username").equals("admin") == false) || (request->arg("password").equals("ADmiNPaSSworD") == false))
                    {
                        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication Failed");
                        request->send(response);
                        return;
                    }

                    if (!Update.begin())
                    {
                        Update.printError(Serial);
                        Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                    }
                }

                if (!Update.hasError())
                {
                    if (Update.write(data, len) != len)
                    {
                        Update.printError(Serial);
                        Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                    }
                    else
                    {
                        Events.send(String(index + len).c_str(), "progress", millis());
                    }
                }
                else
                {
                    Update.printError(Serial);
                    Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                }

                if (final)
                {
                    if (Update.end(true))
                    {
                        //   Serial.printf("Update Success: %uB\n", index + len);
                        Events.send(String(index + len).c_str(), "progress", millis());
                    }
                    else
                    {
                        Update.printError(Serial);
                        Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                    }
                }

                //   request->send(200);
            });

        // attach filesystem root at URL /fs
        // HttpServer.serveStatic("/fs", SPIFFS, "/");

        HttpServer.on(
            "/version",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                AsyncJsonResponse *jsonResp = new AsyncJsonResponse();
                JsonVariant root = jsonResp->getRoot();
                JsonVariant info = root.createNestedObject("info");
                JsonVariant firmware = info.createNestedObject("Firmware");
                firmware["MAJOR VER"] = MAJOR_VERSION;
                firmware["MINOR VER"] = MINOR_VERSION;
                firmware["SUB VER"] = SUB_VERSION;
                info["COMMIT"] = SW_VERSION;
                info["SDK"] = ESP.getSdkVersion();
                info["CPU FREQ"] = getCpuFrequencyMhz();
                info["APB FREQ"] = getApbFrequency();
                info["FLASH SIZE"] = ESP.getFlashChipSize();
                info["RAM SIZE"] = ESP.getHeapSize();
                info["FREE RAM"] = ESP.getFreeHeap();
                info["MAX RAM ALLOC"] = ESP.getMaxAllocHeap();
                // Serial.println("MHz");
                // Serial.print("ESP32 FLASH SIZE: ");
                // Serial.print(ESP.getFlashChipSize() / (1024.0 * 1024), 2);
                // Serial.println("MB");
                // Serial.print("ESP32 RAM SIZE: ");
                // Serial.print(ESP.getHeapSize() / 1024.0, 2);
                // Serial.println("KB");
                // Serial.print("ESP32 FREE RAM: ");
                // Serial.print(ESP.getFreeHeap() / 1024.0, 2);
                // Serial.println("KB");
                // Serial.print("ESP32 MAX RAM ALLOC: ");
                // Serial.print(ESP.getMaxAllocHeap() / 1024.0, 2);
                // Serial.println("KB");
                // Serial.print("ESP32 FREE PSRAM: ");
                // Serial.print(ESP.getFreePsram() / 1024.0, 2);
                // Serial.println("KB");

                // String info = "{\"info\":{\"firmware\":";
                // info = info + "\"" + MAJOR_VERSION + "." + MINOR_VERSION + "." + SUB_VERSION + "_" + ESP.getSdkVersion() + "\"}}";
                jsonResp->setLength();
                // serializeJson(doc, Serial);
                request->send(jsonResp);
                // info.~String();
            });

        // HttpServer.on(
        //     "/info",
        //     HTTP_POST,
        //     [](AsyncWebServerRequest *request) {
        //         String info = "{\"info\":{\"firmware\":";
        //         info = info + "\"" + MAJOR_VERSION + "." + MINOR_VERSION + "." + SUB_VERSION + "_" + ESP.getSdkVersion() + "\"}}";
        //         request->send(200, "application/json", info);
        //         info.~String();
        //     });

        HttpServer.on(
            "/login",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                // Serial.println("Received post request");
                //List all parameters (Compatibility)
                // int args = request->args();
                // for (int i = 0; i < args; i++)
                // {
                //     Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
                // }

                request->send(200);
            });

        // HttpServer.on(
        //     "/fsread",
        //     HTTP_POST,
        //     [](AsyncWebServerRequest *request) {
        //         String json = "{\"data\":[";
        //         File root = SPIFFS.open("/");
        //         File file = root.openNextFile();

        //         while (file)
        //         {
        //             json = json + "[\"" + file.name() + "\",\"" + file.size() + "\"],";
        //             file.close();
        //             file = root.openNextFile();
        //         }
        //         root.close();

        //         json = json + "[\"end\", \"0\"]]}";
        //         request->send(200, "application/json", json);
        //         json.~String();
        //     });

        HttpServer.on(
            "/fsread",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                AsyncJsonResponse *jsonResp = new AsyncJsonResponse();
                JsonVariant rootJson = jsonResp->getRoot();

                File root = SPIFFS.open("/");
                File file = root.openNextFile();

                while (file)
                {
                    rootJson[String(file.name())] = file.size();
                    file.close();
                    file = root.openNextFile();
                }

                root.close();
                jsonResp->setLength();
                request->send(jsonResp);
            });

        HttpServer.on(
            "/fsdelete",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if (request->args() > 0)
                {
                    if ((request->arg("username").equals("admin") == true) && (request->arg("password").equals("ADmiNPaSSworD") == true))
                    {
                        SPIFFS.remove(request->arg("file"));
                        request->send(200);
                    }
                    else
                    {
                        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication Failed");
                        request->send(response);
                    }

                    // Serial.printf("%s: %s removed\n", request->argName(i).c_str(), request->arg(i).c_str());
                }
                request->send(200);
            });

        HttpServer.on(
            "/fsupload",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                //   bool shouldReboot = !Update.hasError();
                //   AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
                //   response->addHeader("Connection", "close");
                if (request->args() > 2)
                {
                    if ((request->arg("username").equals("admin") == true) && (request->arg("password").equals("ADmiNPaSSworD") == true))
                    {
                        request->send(200);
                    }
                    else
                    {
                        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication Failed");
                        request->send(response);
                    }
                }
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                if (!index)
                {
                    if ((request->arg("username").equals("admin") == false) || (request->arg("password").equals("ADmiNPaSSworD") == false))
                    {
                        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Authentication Failed");
                        request->send(response);
                        return;
                    }

                    filename = "/" + filename;
                    FsUploadFile = SPIFFS.open(filename, "w");
                }

                if (FsUploadFile)
                {
                    // Write the received bytes to the file
                    Events.send(String(index + FsUploadFile.write(data, len)).c_str(), "progress", millis());
                }

                if (final)
                {
                    if (FsUploadFile)
                    {
                        // If the file was successfully created
                        FsUploadFile.close();
                    }
                    //   Serial.printf("Update Success: %uB\n", index + len);
                }
            });

        HttpServer.on(
            "/test",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                Serial.printf("get Success");
                request->send(200, "text/plain", "Hello");
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                Serial.printf("file Success: %uB\n", index + len);
            },
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                Serial.printf("body Success: %uB\n", index + len);
            });

        HttpServer.on(
            "/tasklist",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                request->send(200);
            });

        HttpServer.on("/explorer", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/explorer.html", "text/html");
            request->send(response);
        });

        HttpServer.on("/terminal", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/terminal.html", "text/html");
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

        HttpServer.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/bootstrap.bundle.min.js.gz", "text/javascript");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/tooltip.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/tooltip.min.js.gz", "text/javascript");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        HttpServer.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/config.html", "text/html");
            request->send(response);
        });

        HttpServer.on("/autopeepal.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(SPIFFS, "/autopeepal.png", "image/png");
        });

        HttpServer.begin();
    }

    ESP_LOGI("WIFI", "AP IP address: %s", WiFi.softAPIP().toString().c_str());
    SocketServer.begin();
#if WIFI_STA
    // Add service to MDNS-SD
    MDNS.addService("_http", "_tcp", 80);
#endif
    preferences.end();
}

void WIFI_SupportTask(void *pvParameters)
{
    wl_status_t wifiStatus = WL_NO_SHIELD;
    wl_status_t wifiStatusTmp;
    bool wifiConnect;
    Preferences preferences;

    while (1)
    {
        preferences.begin("config", false);
        wifiStatusTmp = WiFi.status();
        wifiConnect = false;

        if (wifiStatus != wifiStatusTmp)
        {
            wifiStatus = wifiStatusTmp;
            switch (wifiStatus)
            {
            case WL_CONNECTED:
                Serial.printf("WiFi connected, IP address: %s\r\n", WiFi.localIP().toString().c_str());
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_HIGH, LED_TOGGLE_RATE_NONE);
                break;

            case WL_DISCONNECTED:
                // WiFi.reconnect();
                Serial.println(str(WL_DISCONNECTED));
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_1HZ);
                wifiConnect = true;
                break;

            case WL_IDLE_STATUS:
                Serial.println(str(WL_IDLE_STATUS));
                wifiConnect = true;
                break;

            case WL_NO_SSID_AVAIL:
                Serial.println(str(WL_NO_SSID_AVAIL));
                wifiConnect = true;
                break;

            case WL_SCAN_COMPLETED:
                Serial.println(str(WL_SCAN_COMPLETED));
                break;

            case WL_CONNECT_FAILED:
                Serial.println(str(WL_CONNECT_FAILED));
                wifiConnect = true;
                break;

            case WL_CONNECTION_LOST:
                Serial.println(str(WL_CONNECTION_LOST));
                wifiConnect = true;
                break;

            default:
                break;
            }

            if (wifiConnect == true)
            {
                wifiConnect = false;
                WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());
            }
        }

        preferences.end();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void WIFI_Task(void *pvParameters)
{
    wl_status_t wifiStatus = WL_IDLE_STATUS;
    UBaseType_t uxHighWaterMark;

    ESP_LOGI("WIFI", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("WIFI", "uxHighWaterMark = %d", uxHighWaterMark);

    WIFI_Init();

    if (xTaskCreate(WIFI_SupportTask, "WIFI_SupportTask", 5000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
    {
        configASSERT(0);
    }

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
                    WIFI_SeqNo = WIFI_RxBuff[0];
                    APP_ProcessData(&WIFI_RxBuff[11], (len - 13), APP_CHANNEL_TCP_SOC);
                    // client.write(WIFI_RxBuff, len);
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
#if WIFI_AP_DNS
        DNS_Server.processNextRequest();
#endif
        vTaskDelay(100 / portTICK_PERIOD_MS);
        WebSocket.cleanupClients();
    }
}

void WIFI_Soc_Write(uint8_t *payLoad, uint16_t len)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))

    uint16_t crc16;
    uint16_t idx;
    uint32_t tick;

    Serial.println("App processed");
    WiFiClient client = SocketServer.available();

    if (!WIFI_TxLen)
    {
        idx = 0;
        if ((payLoad[0] & 0xF0) == 0x20)
        {
            WIFI_TxBuff[idx++] = WIFI_SeqNo;
        }
        else
        {
            WIFI_TxBuff[idx++] = 0xFF;
        }

        WIFI_TxBuff[idx++] = byte(len, 1);
        WIFI_TxBuff[idx++] = byte(len, 0);
        tick = xTaskGetTickCount();
        WIFI_TxBuff[idx++] = byte(tick, 3);
        WIFI_TxBuff[idx++] = byte(tick, 2);
        WIFI_TxBuff[idx++] = byte(tick, 1);
        WIFI_TxBuff[idx++] = byte(tick, 0);
        memset(&WIFI_TxBuff[idx], 0, 4);
        idx = idx + 4;
        memcpy(&WIFI_TxBuff[idx], payLoad, len);
        idx = idx + len;
        crc16 = UTIL_CRC16_CCITT(0xFFFF, WIFI_TxBuff, len + 11);
        WIFI_TxBuff[idx++] = byte(crc16, 1);
        WIFI_TxBuff[idx++] = byte(crc16, 0);
        WIFI_TxLen = idx;
        WIFI_SeqNo = 0xFE;
    }
}

void WIFI_WebSoc_Write(uint8_t *payLoad, uint16_t len)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))

    uint16_t crc16;
    uint16_t idx;
    uint32_t tick;
    // Serial.println("App processed");

    if (!WIFI_TxLen)
    {
        idx = 0;
        if ((payLoad[0] & 0xF0) == 0x20)
        {
            WIFI_TxBuff[idx++] = WIFI_SeqNo;
        }
        else
        {
            WIFI_TxBuff[idx++] = 0xFF;
        }

        WIFI_TxBuff[idx++] = byte(len, 1);
        WIFI_TxBuff[idx++] = byte(len, 0);
        tick = xTaskGetTickCount();
        WIFI_TxBuff[idx++] = byte(tick, 3);
        WIFI_TxBuff[idx++] = byte(tick, 2);
        WIFI_TxBuff[idx++] = byte(tick, 1);
        WIFI_TxBuff[idx++] = byte(tick, 0);
        memset(&WIFI_TxBuff[idx], 0, 4);
        idx = idx + 4;
        memcpy(&WIFI_TxBuff[idx], payLoad, len);
        idx = idx + len;
        crc16 = UTIL_CRC16_CCITT(0xFFFF, WIFI_TxBuff, idx);
        WIFI_TxBuff[idx++] = byte(crc16, 1);
        WIFI_TxBuff[idx++] = byte(crc16, 0);
        WIFI_TxLen = idx;

        if (p_WebSocketClient != NULL)
        {
            p_WebSocketClient->binary(WIFI_TxBuff, WIFI_TxLen);
            WIFI_TxLen = 0;
            WIFI_SeqNo = 0xFE;
        }
    }
}

void WIFI_Set_STA_SSID(char *p_str)
{
    Preferences preferences;

    preferences.begin("config", false);
    preferences.putString("stSSID", p_str);

    preferences.end();
}

void WIFI_Set_STA_Pass(char *p_str)
{
    Preferences preferences;

    preferences.begin("config", false);
    preferences.putString("stPASS", p_str);

    preferences.end();
}

void WIFI_EventCb(system_event_id_t event)
{
    Serial.print("system_event_id_t: ");
    Serial.println(event);

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
        ESP_LOGI("WIFI", "Wifi Connecting.....");
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