#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define VERSION             "00.00.01"
#define STA_WIFI_SSID       "FRITZ!Box 7560 UU"
#define STA_WIFI_PASSWORD   "aksharaa9003755682"
#define MQTT_URL            "ec2-13-232-102-99.ap-south-1.compute.amazonaws.com"

#define BLE_CONN_LED    GPIO_NUM_33     // Blue LED
#define HEART_BEAT_LED  BLE_CONN_LED
#define COMM_LED        GPIO_NUM_25     // Yellow LED
#define WIFI_CONN_LED   GPIO_NUM_27     // Green LED
#define SECURITY_LED    GPIO_NUM_26     // Amber LED
#define ERROR_LED       GPIO_NUM_32     // Red LED

typedef enum {
    GPIO_STATE_LOW = 0,
    GPIO_STATE_HIGH,
    GPIO_STATE_TOGGLE,
}gpio_state_t;

#ifdef __cplusplus
}
#endif

#endif