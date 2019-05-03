#include <Arduino.h>
#include "config.h"
#include "mqtt_client.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "app.h"
#include "a_mqtt.h"


static esp_err_t MQTT_EventHandler(esp_mqtt_event_handle_t event);

esp_mqtt_client_config_t MQTT_Config;
esp_mqtt_client_handle_t MQTT_Client;

static esp_err_t MQTT_EventHandler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client;
    int msg_id;

    client = event->client;

    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI("MQTT", "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI("MQTT", "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI("MQTT", "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI("MQTT", "sent unsubscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT", "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI("MQTT", "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI("MQTT", "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI("MQTT", "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI("MQTT", "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI("MQTT", "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI("MQTT", "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGI("MQTT", "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

void MQTT_Init(void)
{
    MQTT_Config.event_handle = MQTT_EventHandler;
    MQTT_Config.uri = "mqtt://iot.eclipse.org";
    MQTT_Config.port = 1883;
    MQTT_Config.task_stack = 8000;
    MQTT_Config.buffer_size = 4096;
    MQTT_Config.refresh_connection_after_ms = 500;

    MQTT_Client = esp_mqtt_client_init(&MQTT_Config);
    esp_mqtt_client_start(MQTT_Client);
}

void MQTT_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;

    ESP_LOGI("MQTT", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("MQTT", "uxHighWaterMark = %d", uxHighWaterMark);

    while (1)
    {
        vTaskDelay(10 * portTICK_PERIOD_MS);
    }
}

void MQTT_Write(uint8_t *payLoad, uint16_t len)
{
    esp_mqtt_client_publish(MQTT_Client, "RESPONSE", (char*)payLoad, len, 0, 0);
}

#if 0
char MQTT_Server[200] = MQTT_URL;
//char MQTT_Server[200] = {13,126,50,237};

/* create an instance of PubSubClient client */
WiFiClient MQTT_WifiClient;
PubSubClient MQTT_Client(MQTT_WifiClient);


void MQTT_ReceivedCallback(char *topic, byte *payload, unsigned int length)
{
    ESP_LOGI("MQTT", "Message received: %s", topic);

    // ESP_LOGI("MQTT", "payload: ");
    // for (int i = 0; i < length; i++)
    // {
    //     Serial.print((char)payload[i]);
    // }
    // Serial.println();

    APP_ProcessData(payload, length, APP_CHANNEL_MQTT);
}

void MQTT_Init(void)
{
    /* configure the MQTT server with IPaddress and port */
    MQTT_Client.setServer(MQTT_Server, 1883);
    /* this receivedCallback function will be invoked 
  when client received subscribed topic */
    MQTT_Client.setCallback(MQTT_ReceivedCallback);
}

void MQTT_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;

    ESP_LOGI("MQTT", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("MQTT", "uxHighWaterMark = %d", uxHighWaterMark);


    while (1)
    {
        if (!MQTT_Client.connected())
        {
            ESP_LOGI("MQTT", "connecting ...");
            /* client ID */
            String clientId = "ESP32Client";

            /* connect now */
            if (MQTT_Client.connect(clientId.c_str(), NULL, NULL))
            {
                ESP_LOGI("MQTT", "connected");
                /* subscribe topic with default QoS 0*/
                MQTT_Client.subscribe("REQUEST");
            }
            else
            {
                ESP_LOGI("MQTT", "failed, status code = %d try again in 5 seconds", MQTT_Client.state());
                /* Wait 5 seconds before retrying */
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        }

        MQTT_Client.loop();

        vTaskDelay(10 * portTICK_PERIOD_MS);
    }
}

void MQTT_Write(uint8_t *payLoad, uint16_t len)
{
    MQTT_Client.publish("RESPONSE", payLoad, len);
}
#endif