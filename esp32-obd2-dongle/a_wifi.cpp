#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "version.h"
#include "util.h"
#include <WiFi.h>
#include "AsyncUDP.h"
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include "app.h"
#include "a_led.h"
#include "a_wifi.h"
#include <Update.h>

#define STA_STATIC_IP 0

AsyncUDP UDP;
AsyncWebServer HttpServer(80);
AsyncWebSocket WebSocket("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource Events("/events");
AsyncWebSocketClient *p_WebSocketClient;
WiFiClient WIFI_Client;
File FsUploadFile;
WiFiServer SocketServer(6888);
SemaphoreHandle_t WIFI_SemTCP_SocComplete;
SemaphoreHandle_t WIFI_SemWebSocTxComplete;

char WIFI_STA_SSID[50] = STA_WIFI_SSID;
char WIFI_STA_Password[50] = STA_WIFI_PASSWORD;
char WIFI_AP_SSID[50] = "";
char WIFI_AP_Password[50] = AP_WIFI_PASSWORD;
uint8_t WIFI_SeqNo;
uint8_t WIFI_RxBuff[4130];
uint8_t WIFI_TxBuff[4130];
uint16_t WIFI_TxLen;
bool UDP_Status = false;

static void WIFI_RestartTask(void *pvParameters);
static void WIFI_ScanTask(void *pvParameters);
static void WIFI_SupportTask(void *pvParameters);
static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    wl_status_t wifiStatus;
    bool staConnected = false;
    char mac[6];
    Preferences preferences;

    WIFI_SemTCP_SocComplete = xSemaphoreCreateBinary();
    WIFI_SemWebSocTxComplete = xSemaphoreCreateBinary();

    if ((WIFI_SemTCP_SocComplete == NULL) || (WIFI_SemWebSocTxComplete == NULL))
    {
        Serial.println("ERROR: Failed to create Soc Tx complete semaphore");
    }
    else
    {
        xSemaphoreGive(WIFI_SemTCP_SocComplete);
        xSemaphoreGive(WIFI_SemWebSocTxComplete);
    }

    preferences.begin("config", false);

    LED_SetLedState(WIFI_CONN_LED, LED_STATE_ON, LED_TOGGLE_RATE_1HZ);
    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    WiFi.macAddress((uint8_t *)mac);
    sprintf(WIFI_AP_SSID, "%s-%02X%02X", AP_WIFI_SSID, mac[4], mac[5]);
    Serial.printf("INFO: SSID <%s>\r\n", WIFI_AP_SSID);

    if ((preferences.getString("stSSID") != "") && (strnlen(preferences.getString("stSSID").c_str(), 50) < 50))
    {
        if ((preferences.getString("stPASS") != "") && (strnlen(preferences.getString("stPASS").c_str(), 50) < 50))
        {
            WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());
#if STA_STATIC_IP
            WiFi.config(IPAddress(192, 168, 43, 77), IPAddress(192, 168, 43, 1), IPAddress(255, 255, 255, 0), IPAddress(8, 8, 8, 8));
#endif
        }
        else
        {
            Serial.println("INFO: ST mode PASSWORD is missing. Update password");
        }
    }
    else
    {
        Serial.println("INFO: ST mode SSID Key is missing. Update SSID");
    }

    if (MDNS.begin("obd2") == false)
    {
        Serial.println("ERROR: Setting up MDNS responder failed!");
    }
    else
    {
        Serial.println("INFO: mDNS responder started");
        // Add service to MDNS-SD
        MDNS.addService("_http", "_tcp", 80);
    }

    // int n = WiFi.scanNetworks();
    // Serial.println("INFO: AP Scan completed");
    // if (n == 0)
    // {
    //     Serial.println("INFO: No networks found");
    // }
    // else
    // {
    //     for (int i = 0; i < n; ++i)
    //     {
    //         if (WiFi.SSID(i).equals(preferences.getString("stSSID")) == true)
    //         {
    //             staConnected = true;
    //             Serial.println("INFO: AP found and joined");
    //             break;
    //         }
    //     }
    // }

    if (staConnected == false)
    {
        Serial.println("INFO: WIFI AP begin");
        if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_Password) == false)
        {
            Serial.println("INFO: SoftAP failed to start!");
        }
    }

    WiFi.setHostname("ESP32_OBD2");

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
            uint8_t errorCode = APP_RESP_ACK;

            Serial.printf("INFO: Websocket data <%d> received\r\n", len);

            if (type == WS_EVT_CONNECT)
            {
                p_WebSocketClient = client;
                Serial.println("INFO: Websocket client connection received");
            }
            else if (type == WS_EVT_DISCONNECT)
            {
                p_WebSocketClient = NULL;
                Serial.println("INFO: Websocket Client disconnected");
            }
            else if (type == WS_EVT_DATA)
            {
                WIFI_SeqNo = data[0];

                if (len < sizeof(WIFI_TxBuff))
                {
                    p_WebSocketClient = client;
                    crc16Act = ((uint16_t)data[len - 2] << 8) | (uint16_t)data[len - 1];
                    crc16Calc = UTIL_CRC16_CCITT(0xFFFF, data, (len - 2));

                    if (crc16Act == crc16Calc)
                    {
                        APP_ProcessData(&data[11], (len - 13), APP_MSG_CHANNEL_WEB_SOC);
                    }
                    else
                    {
                        errorCode = APP_RESP_NACK_15;
                    }
                }
                else
                {
                    errorCode = APP_RESP_NACK_15;
                }

                if (errorCode != APP_RESP_ACK)
                {
                    len = 0;
                    buff[len++] = 0x20;
                    buff[len++] = 2 + 2;
                    buff[len++] = APP_RESP_NACK;
                    buff[len++] = errorCode;
                    crc16 = UTIL_CRC16_CCITT(0xFFFF, &buff[2], (len - 2));
                    buff[len++] = crc16 >> 8;
                    buff[len++] = crc16;
                    WIFI_WebSoc_Write(buff, len);
                }
            }
        });

        Events.onConnect([](AsyncEventSourceClient *client) {
            if (client->lastId())
            {
                Serial.printf("INFO: Client reconnected! Last message ID that it got is: %u\r\n", client->lastId());
            }
            // send event with message "hello!", id current millis
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
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Login Success!");
            response->addHeader("Connection", "close");
            request->send(response);
        });

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
                        Serial.println("INFO: Device being restarted");
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
                        Events.send(String(index + len).c_str(), "progress", millis());
                    }
                    else
                    {
                        Update.printError(Serial);
                        Events.send((String("Update Error: ") + Update.getError()).c_str(), "error", millis());
                    }
                }
            });

        HttpServer.on(
            "/version",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                String info = "{\"info\":{";
                info += "\"Firmware\":{";
                info += "\"MAJOR VER\":" xstr(MAJOR_VERSION) ",";
                info += "\"MINOR VER\":" xstr(MINOR_VERSION) ",";
                info += "\"SUB VER\":" xstr(SUB_VERSION);
                info += "},";
                info += "\"COMMIT\":"
                        "\"" SW_VERSION "\",";
                info += "\"SDK\":\"" + String(ESP.getSdkVersion()) + "\",";
                info += "\"CPU FREQ\":" + String(getCpuFrequencyMhz()) + ",";
                info += "\"APB FREQ\":" + String(getApbFrequency()) + ",";
                info += "\"FLASH SIZE\":" + String(ESP.getFlashChipSize()) + ",";
                info += "\"RAM SIZE\":" + String(ESP.getHeapSize()) + ",";
                info += "\"FREE RAM\":" + String(ESP.getFreeHeap()) + ",";
                info += "\"MAX RAM ALLOC\":" + String(ESP.getMaxAllocHeap());
                info += "}}";
                request->send(200, "application/json", info);
            });

        HttpServer.on(
            "/login",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                request->send(200);
            });

        HttpServer.on(
            "/scan",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                const char *scanStatus[] = {
                    str(WIFI_SCAN_RUNNING),
                    str(WIFI_SCAN_FAILED),
                };

                int n = WiFi.scanNetworks(true);

                if ((n == WIFI_SCAN_RUNNING) || (n == WIFI_SCAN_FAILED))
                {
                    request->send(200, "application/json", scanStatus[(n * -1) - 1]);

                    if (n == WIFI_SCAN_RUNNING)
                    {
                        if (xTaskCreate(WIFI_ScanTask, "WIFI_ScanTask", 2000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
                        {
                            configASSERT(0);
                        }
                    }
                }
                else
                {
                    request->send(200, "application/json", String(n));
                }
            });

        HttpServer.on(
            "/fsread",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                String info = "{";
                File root = SPIFFS.open("/");
                File file = root.openNextFile();

                while (file)
                {
                    info += "\"" + String(file.name()) + "\":" + String(file.size());
                    file.close();
                    file = root.openNextFile();
                    if (file)
                    {
                        info += ",";
                    }
                }
                info += "}";

                request->send(200, "application/json", info);
            });

        HttpServer.on(
            "/fsdelete",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if (request->args() > 2)
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
                }
                request->send(200);
            });

        HttpServer.on(
            "/apconnect",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if (request->args() > 1)
                {
                    if ((request->arg("ssid").equals("") == false) && (request->arg("password").equals("") == false))
                    {
                        Preferences preferences;

                        preferences.begin("config", false);
                        preferences.putString("stSSID", request->arg("ssid").c_str());
                        preferences.putString("stPASS", request->arg("password").c_str());
                        Serial.println("INFO: AP credentials saved");
                        request->send(200, "text/plain", "Restart to join into the configured AP");
                    }
                    else
                    {
                        request->send(401, "text/plain", "Wrong input Parameter values");
                    }
                }
                else
                {
                    request->send(401, "text/plain", "Wrong no.of input Parameters");
                }
            });

        HttpServer.on(
            "/restart",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if (xTaskCreate(WIFI_RestartTask, "WIFI_RestartTask", 1000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
                {
                    configASSERT(0);
                    request->send(401, "text/plain", "Restart failed, manually restart the device");
                }
                else
                {
                    request->send(200, "text/plain", "Will be restarted in 2secs");
                }
            });

        HttpServer.on(
            "/fsupload",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                if (request->args() > 2)
                {
                    if ((request->arg("username").equals("admin") == true) && (request->arg("password").equals("ADmiNPaSSworD") == true))
                    {
                        request->send(200);
                    }
                    else
                    {
                        request->send(401, "text/plain", "Authentication Failed");
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
                }
            });

        HttpServer.on(
            "/test",
            HTTP_POST,
            [](AsyncWebServerRequest *request) {
                Serial.println("INFO: /test Form Success");
                request->send(200, "text/plain", "Hello");
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                Serial.printf("INFO: /test File Success: <%u>B\r\n", index + len);
            },
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                Serial.printf("INFO: /test Body Success: <%u>B\r\n", index + len);
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

        HttpServer.on("/jquery.tabletojson.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/jquery.tabletojson.min.js.gz", "text/javascript");
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
    preferences.end();
}

void WIFI_ScanTask(void *pvParameters)
{
    Serial.println("INFO: AP Scan Task started");

    while (1)
    {
        int n = WiFi.scanComplete();

        if (n == WIFI_SCAN_FAILED)
        {
            Serial.println("INFO: AP Scan failed");
            break;
        }

        Serial.println("INFO: AP Scan running");

        if (n > 0)
        {
            const char *encryptionType[] = {"AUTH_OPEN",            /**< authenticate mode : open */
                                            "AUTH_WEP",             /**< authenticate mode : WEP */
                                            "AUTH_WPA_PSK",         /**< authenticate mode : WPA_PSK */
                                            "AUTH_WPA2_PSK",        /**< authenticate mode : WPA2_PSK */
                                            "AUTH_WPA_WPA2_PSK",    /**< authenticate mode : WPA_WPA2_PSK */
                                            "AUTH_WPA2_ENTERPRISE", /**< authenticate mode : WPA2_ENTERPRISE */
                                            "AUTH_MAX"};

            String json = "[";
            Serial.printf("INFO: AP Scan completed <%d>\r\n", n);

            for (int i = 0; i < n; ++i)
            {
                if (i)
                    json += ",";
                json += "{";
                json += "\"rssi\":" + String(WiFi.RSSI(i));
                json += ",\"ssid\":\"" + WiFi.SSID(i) + "\"";
                json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
                json += ",\"channel\":" + String(WiFi.channel(i));
                json += ",\"secure\":\"" + String(encryptionType[WiFi.encryptionType(i)]) + "\"";
                json += "}";
            }

            json += "]";
            WiFi.scanDelete();
            Events.send(json.c_str(), "scan", millis());
            break;
        }

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void WIFI_RestartTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP.restart();
    vTaskDelete(NULL);
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
                Serial.printf("INFO: WiFi in ST mode connected <%s>\r\n", WiFi.localIP().toString().c_str());
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_ON, LED_TOGGLE_RATE_NONE);

                if ((UDP_Status == false) && (UDP.listenMulticast(IPAddress(239, 1, 2, 3), 1234) == true))
                {
                    UDP_Status = true;
                    UDP.onPacket([](AsyncUDPPacket packet) {
                        //reply to the client
                        packet.println(WiFi.localIP());
                    });
                }

                // if (WiFi.softAPdisconnect(true) == ESP_OK)
                // {
                //     Serial.println("INFO: SoftAP turned OFF");
                // }
                break;

            case WL_DISCONNECTED:
                Serial.println("INFO: " str(WL_DISCONNECTED));
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_ON, LED_TOGGLE_RATE_1HZ);
                wifiConnect = true;
                break;

            case WL_IDLE_STATUS:
                Serial.println("INFO: " str(WL_IDLE_STATUS));
                wifiConnect = true;
                break;

            case WL_NO_SSID_AVAIL:
                Serial.println("INFO: " str(WL_NO_SSID_AVAIL));
                wifiConnect = true;
                break;

            case WL_SCAN_COMPLETED:
                Serial.println("INFO: " str(WL_SCAN_COMPLETED));
                break;

            case WL_CONNECT_FAILED:
                Serial.println("INFO: " str(WL_CONNECT_FAILED));
                wifiConnect = true;
                break;

            case WL_CONNECTION_LOST:
                Serial.println("INFO: " str(WL_CONNECTION_LOST));
                wifiConnect = true;
                break;

            default:
                break;
            }

            if (wifiConnect == true)
            {
                WIFI_Client.stop();
                wifiConnect = false;
                WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());

                // Serial.println("INFO: WIFI AP begin");
                // if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_Password) == false)
                // {
                //     Serial.println("INFO: SoftAP failed to start!");
                // }
            }
        }

        preferences.end();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void WIFI_Task(void *pvParameters)
{
    wl_status_t wifiStatus = WL_IDLE_STATUS;
    // UBaseType_t uxHighWaterMark;
    uint32_t socketTimeoutTmr;

    // ESP_LOGI("WIFI", "Task Started");

    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // ESP_LOGI("WIFI", "uxHighWaterMark = %d", uxHighWaterMark);

    WIFI_Init();

    if ((WIFI_SemTCP_SocComplete == NULL) || (WIFI_SemWebSocTxComplete == NULL))
    {
        vTaskDelete(NULL);
        return;
    }

    if (xTaskCreate(WIFI_SupportTask, "WIFI_SupportTask", 5000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
    {
        configASSERT(0);
    }

    while (1)
    {
        WIFI_Client = SocketServer.available();

        if (WIFI_Client.connected() == true)
        {
            int32_t len;
            int32_t idx = 0;
            WIFI_Client.setTimeout(86400);

            Serial.println("INFO: TCP Socket Client connected");

            while (WIFI_Client.connected())
            {
                StopTimer(socketTimeoutTmr);
                idx = 0;

                if (WIFI_Client.available() > 0)
                {
                    StartTimer(socketTimeoutTmr, 4);
                }

                while (IsTimerRunning(socketTimeoutTmr))
                {
                    len = WIFI_Client.available();
                    if (len > 0)
                    {
                        if ((idx + len) < sizeof(WIFI_RxBuff))
                        {
                            len = WIFI_Client.read(&WIFI_RxBuff[idx], len);
                            Serial.printf("INFO: TCP Socket data <%ld> read in this call\r\n", len);

                            if (len > 0)
                            {
                                idx += len;
                                ResetTimer(socketTimeoutTmr, 4);
                            }
                        }
                        else
                        {
                            WIFI_Client.flush();
                            Serial.printf("ERROR: TCP Socket data <%ld> is bigger than buffer can hold\r\n", (len + idx));
                            idx = 0;
                            StopTimer(socketTimeoutTmr);
                            break;
                        }
                    }
                    else
                    {
                        Serial.println("INFO: Waiting, NO TCP Socket data during this call");
                    }

                    vTaskDelay(pdMS_TO_TICKS(2));
                }

                StopTimer(socketTimeoutTmr);

                if (idx > 0)
                {
                    uint8_t errorCode = APP_RESP_ACK;
                    uint16_t crc16Act;
                    uint16_t crc16Calc;
                    uint8_t buff[30];
                    uint16_t crc16;

                    WIFI_SeqNo = WIFI_RxBuff[0];
                    Serial.printf("INFO: TCP Socket data <%ld> received\r\n", idx);

                    if (idx < sizeof(WIFI_TxBuff))
                    {
                        crc16Act = ((uint16_t)WIFI_RxBuff[idx - 2] << 8) | (uint16_t)WIFI_RxBuff[idx - 1];
                        crc16Calc = UTIL_CRC16_CCITT(0xFFFF, WIFI_RxBuff, (idx - 2));

                        if (crc16Act == crc16Calc)
                        {
                            APP_ProcessData(&WIFI_RxBuff[11], (idx - 13), APP_MSG_CHANNEL_TCP_SOC);
                        }
                        else
                        {
                            errorCode = APP_RESP_NACK_15;
                        }
                    }
                    else
                    {
                        errorCode = APP_RESP_NACK_15;
                    }

                    if (errorCode != APP_RESP_ACK)
                    {
                        len = 0;
                        buff[len++] = 0x20;
                        buff[len++] = 2 + 2;
                        buff[len++] = APP_RESP_NACK;
                        buff[len++] = errorCode;
                        crc16 = UTIL_CRC16_CCITT(0xFFFF, &buff[2], (len - 2));
                        buff[len++] = crc16 >> 8;
                        buff[len++] = crc16;
                        WIFI_TCP_Soc_Write(buff, len);
                    }

                    idx = 0;
                }

                vTaskDelay(pdMS_TO_TICKS(1));
            }

            Serial.println("INFO: TCP Socket Client disconnected");
            WIFI_Client.stop();
        }
        else
        {
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        WebSocket.cleanupClients();
    }
}

void WIFI_TCP_Soc_Write(uint8_t *payLoad, uint16_t len)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))

    uint16_t crc16;
    uint32_t idx;
    uint32_t tick;

    if (WIFI_SemTCP_SocComplete == NULL)
    {
        return;
    }

    xSemaphoreTake(WIFI_SemTCP_SocComplete, portMAX_DELAY);

    // if (WIFI_TxLen == 0)
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

        if (WIFI_Client.connected() == true)
        {
            WIFI_Client.write(WIFI_TxBuff, WIFI_TxLen);
            Serial.println("INFO: App processed TCP Socket Data");
        }
        else
        {
            Serial.println("INFO: TCP Socket Client disconnected during send");
        }

        WIFI_TxLen = 0;
    }

    xSemaphoreGive(WIFI_SemTCP_SocComplete);
}

void WIFI_WebSoc_Write(uint8_t *payLoad, uint16_t len)
{
#define byte(x, y) ((uint8_t)(x >> (y * 8)))

    uint16_t crc16;
    uint32_t idx;
    uint32_t tick;

    if (WIFI_SemWebSocTxComplete == NULL)
    {
        return;
    }

    xSemaphoreTake(WIFI_SemWebSocTxComplete, portMAX_DELAY);

    // if (WIFI_TxLen == 0)
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
        }
        else
        {
            Serial.println("INFO: TCP Socket Client disconnected during send");
        }

        WIFI_TxLen = 0;
    }

    xSemaphoreGive(WIFI_SemWebSocTxComplete);
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
        Serial.print("INFO: IP address: ");
        Serial.println(WiFi.localIP());
        break;

    case SYSTEM_EVENT_STA_CONNECTED:
        Serial.println("INFO: WiFi connected");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI("WIFI", "Wifi Connecting.....");
        WiFi.reconnect();
        break;

    case SYSTEM_EVENT_AP_START:
        Serial.print("INFO: SoftAP Ip: ");
        Serial.println(WiFi.softAPIP().toString());
        break;

    case SYSTEM_EVENT_AP_STAIPASSIGNED:

        break;
    }
}