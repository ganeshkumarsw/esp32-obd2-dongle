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

AsyncUDP UDP;
AsyncWebServer HttpServer(80);
AsyncWebSocket WebSocket("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource Events("/events");
AsyncWebSocketClient *p_WebSocketClient;
File FsUploadFile;
WiFiServer SocketServer(6888);
SemaphoreHandle_t WIFI_SemTCP_SocComplete;
SemaphoreHandle_t WIFI_SemWebSocTxComplete;

char WIFI_STA_SSID[50] = STA_WIFI_SSID;
char WIFI_STA_Password[50] = STA_WIFI_PASSWORD;
uint8_t WIFI_SeqNo;
uint8_t WIFI_RxBuff[4130];
uint8_t WIFI_TxBuff[4130];
uint16_t WIFI_TxLen;
bool UDP_Status = false;

static void WIFI_ScanTask(void *pvParameters);
static void WIFI_SupportTask(void *pvParameters);
static void WIFI_EventCb(system_event_id_t event);

void WIFI_Init(void)
{
    wl_status_t wifiStatus;
    bool staConnected = false;
    char apSSID[32];
    char apPass[63];
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

    LED_SetLedState(WIFI_CONN_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_1HZ);
    ESP_LOGI("WIFI", "MAC: %s", WiFi.macAddress().c_str());

    WiFi.macAddress((uint8_t *)mac);
    sprintf(apSSID, "%s-%02X%02X", AP_WIFI_SSID, mac[4], mac[5]);
    Serial.printf("INFO: SSID <%s>\r\n", apSSID);

    if (preferences.getString("stSSID") == "")
    {
        Serial.println("INFO: ST mode SSID Key is missing. Update SSID");
    }

    if (preferences.getString("stPASS") == "")
    {
        Serial.println("INFO: ST mode PASSWORD is missing. Update password");
    }

    WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());

    if (!MDNS.begin("obd2"))
    {
        Serial.println("ERROR: Setting up MDNS responder failed!");
    }
    else
    {
        Serial.println("INFO: mDNS responder started");
        // Add service to MDNS-SD
        MDNS.addService("_http", "_tcp", 80);
    }

    int n = 0; //WiFi.scanNetworks();
    Serial.println("Scan done");
    if (n == 0)
    {
        Serial.println("INFO: No networks found");
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            Serial.println("INFO: SSID: " + WiFi.SSID(i));
            // Print SSID and RSSI for each network found
            if (WiFi.SSID(i).equals(preferences.getString("stSSID")) == true)
            {
                staConnected = true;
                delay(10);
                wifiStatus = WiFi.begin((char *)preferences.getString("stSSID").c_str(), (char *)preferences.getString("stPASS").c_str());
                WiFi.setSleep(false);
                Serial.println("INFO: WIFI STA begin");
                // Serial.println(wifiStatus);

                if (WiFi.getAutoConnect() == false)
                {
                    WiFi.setAutoConnect(true);
                }

                if (WiFi.getAutoReconnect() == false)
                {
                    WiFi.setAutoReconnect(true);
                }
                break;
            }
        }
    }

    if (staConnected == false)
    {
        Serial.println("INFO: WIFI AP begin");
        if (!WiFi.softAP(apSSID, AP_WIFI_PASSWORD))
        {
            Serial.println("INFO: ESP32 SoftAP failed to start!");
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
                p_WebSocketClient = client;
                WIFI_SeqNo = data[0];
                crc16Act = ((uint16_t)data[len - 2] << 8) | (uint16_t)data[len - 1];
                crc16Calc = UTIL_CRC16_CCITT(0xFFFF, data, (len - 2));

                if (crc16Act == crc16Calc)
                {
                    APP_ProcessData(&data[11], (len - 13), APP_MSG_CHANNEL_WEB_SOC);
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
                info += "\"COMMIT\":" "\"" SW_VERSION "\",";
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
                String status;

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
                    info += "\"" + String(file.name()) + "\":"  + String(file.size());
                    file.close();
                    file = root.openNextFile();
                    if(file)
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
                }
                request->send(200);
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
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_HIGH, LED_TOGGLE_RATE_NONE);

                if ((UDP_Status == false) && (UDP.listenMulticast(IPAddress(239, 1, 2, 3), 1234) == true))
                {
                    UDP_Status = true;
                    UDP.onPacket([](AsyncUDPPacket packet) {
                        //reply to the client
                        packet.println(WiFi.localIP());
                    });
                }
                break;

            case WL_DISCONNECTED:
                Serial.println("INFO: " str(WL_DISCONNECTED));
                LED_SetLedState(WIFI_CONN_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_1HZ);
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
    uint32_t socketTimeoutTmr;

    ESP_LOGI("WIFI", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("WIFI", "uxHighWaterMark = %d", uxHighWaterMark);

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
        WiFiClient client = SocketServer.available();

        if (client)
        {
            uint32_t len;
            uint32_t idx = 0;

            Serial.println("INFO: TCP Socket Client connected");

            while (client.connected())
            {
                StopTimer(socketTimeoutTmr);

                if (client.available() > 0)
                {
                    idx = 0;
                    StartTimer(socketTimeoutTmr, 20);
                }

                while (IsTimerRunning(socketTimeoutTmr))
                {
                    len = client.available();
                    if (len > 0)
                    {
                        if ((idx + len) < sizeof(WIFI_RxBuff))
                        {
                            len = client.read(&WIFI_RxBuff[idx], len);
                            Serial.printf("INFO: TCP Socket data <%ld> read in this call\r\n", len);

                            if (len > 0)
                            {
                                idx += len;
                                ResetTimer(socketTimeoutTmr, 20);
                            }
                        }
                        else
                        {
                            client.flush();
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

                    vTaskDelay(5 / portTICK_PERIOD_MS);
                }

                StopTimer(socketTimeoutTmr);

                if (idx)
                {
                    WIFI_SeqNo = WIFI_RxBuff[0];
                    APP_ProcessData(&WIFI_RxBuff[11], (len - 13), APP_MSG_CHANNEL_TCP_SOC);
                    Serial.printf("INFO: TCP Socket data <%ld> received\r\n", idx);
                    idx = 0;
                }

                if (WIFI_TxLen)
                {
                    client.write(WIFI_TxBuff, WIFI_TxLen);
                    Serial.println("INFO: App processed TCP Socket Data");
                    WIFI_TxLen = 0;
                    xSemaphoreGive(WIFI_SemTCP_SocComplete);
                }

                vTaskDelay(1 / portTICK_PERIOD_MS);
            }

            Serial.println("INFO: TCP Socket Client disconnected");
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
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

    if (WIFI_TxLen == 0)
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
    uint32_t idx;
    uint32_t tick;

    if (WIFI_SemWebSocTxComplete == NULL)
    {
        return;
    }

    xSemaphoreTake(WIFI_SemWebSocTxComplete, portMAX_DELAY);

    if (WIFI_TxLen == 0)
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
            xSemaphoreGive(WIFI_SemWebSocTxComplete);
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