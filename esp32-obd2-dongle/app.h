#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

void APP_Init(void);
void APP_Task(void *pvParameters);
void APP_ProcessData(uint8_t *p_buff, uint16_t len, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif