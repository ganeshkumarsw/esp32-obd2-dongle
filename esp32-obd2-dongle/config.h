#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif


#define BLE_CONN_LED    GPIO_NUM_33     // Blue LED
#define HEART_BEAT_LED  SECURITY_LED
#define COMM_LED        GPIO_NUM_25     // Yellow LED
#define WIFI_CONN_LED   GPIO_NUM_27     // Green LED
#define SECURITY_LED    GPIO_NUM_26     // Amber LED
#define ERROR_LED       GPIO_NUM_32     // Red LED

#ifdef __cplusplus
}
#endif

#endif