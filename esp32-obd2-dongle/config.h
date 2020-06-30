#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define xstr(s) str(s)
#define str(s) #s

#define MAJOR_VERSION             0
#define MINOR_VERSION             0
#define SUB_VERSION               25

#define STA_WIFI_SSID       "Vodafone-6046"
#define STA_WIFI_PASSWORD   "AksharAa9003755682"
// #define STA_WIFI_SSID       "AndroidAP"
// #define STA_WIFI_PASSWORD   "9003755682"
#define AP_WIFI_SSID        "OBD2"
#define AP_WIFI_PASSWORD    "password1"
#define MQTT_URL            "ec2-13-126-50-237.ap-south-1.compute.amazonaws.com"

#define CAN_RX_QUEUE_SIZE   100
#define CAN_TX_QUEUE_SIZE   50

typedef enum
{
    LED_OUT_1 = 0,
    LED_OUT_2,
    LED_OUT_3,
    LED_OUT_4,
    LED_OUT_5,
    LED_OUT_6,
    LED_OUT_7,
    LED_OUT_MAX,
}led_num_t;

#define LED_RED         LED_OUT_1
#define LED_BLUE        LED_OUT_2
#define LED_COLOR_0     LED_OUT_3
#define LED_COLOR_1     LED_OUT_4
#define LED_YELLOW      LED_OUT_5
#define LED_GREEN       LED_OUT_6
#define LED_AMBER       LED_OUT_7


#define HEART_BEAT_LED  LED_BLUE
#define COMM_LED        LED_YELLOW
#define WIFI_CONN_LED   LED_GREEN
#define SECURITY_LED    LED_AMBER
#define ERROR_LED       LED_RED


#ifdef __cplusplus
}
#endif

#endif