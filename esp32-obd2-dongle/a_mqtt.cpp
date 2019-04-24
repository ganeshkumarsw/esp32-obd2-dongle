#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "a_mqtt.h"

char MQTT_Server[200] = "ec2-34-209-89-96.us-west-2.compute.amazonaws.com";

/* create an instance of PubSubClient client */
WiFiClient MQTT_WifiClient;
PubSubClient MQTT_Client(MQTT_WifiClient);


void MQTT_ReceivedCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message received: ");
    Serial.println(topic);

    Serial.print("payload: ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
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

    Serial.println("MQTT_Task Started");
    
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    printf("MQTT uxHighWaterMark = %d\r\n", uxHighWaterMark);


    while (1)
    {
        if (!MQTT_Client.connected())
        {
            Serial.print("MQTT connecting ...");
            /* client ID */
            String clientId = "ESP32Client";

            /* connect now */
            if (MQTT_Client.connect(clientId.c_str(), NULL, NULL))
            {
                Serial.println("connected");
                /* subscribe topic with default QoS 0*/
                MQTT_Client.subscribe("REQUEST", MQTTQOS2);
            }
            else
            {
                Serial.print("failed, status code =");
                Serial.print(MQTT_Client.state());
                Serial.println("try again in 5 seconds");
                /* Wait 5 seconds before retrying */
                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        }

        MQTT_Client.loop();

        vTaskDelay(5 * portTICK_PERIOD_MS);
    }
}

void MQTT_Write(uint8_t *payLoad, uint16_t len)
{
    MQTT_Client.publish("RESPONSE", payLoad, len);
}