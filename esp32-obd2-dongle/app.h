#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    APP_CHANNEL_UART = 0,
    APP_CHANNEL_BLE,
    APP_CHANNEL_MQTT,
}APP_CHANNEL_t;

void APP_Init(void);
void APP_Task(void *pvParameters);
void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel);

#ifdef __cplusplus
}
#endif

#endif