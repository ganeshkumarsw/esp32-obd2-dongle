#ifndef __A_BLE_H__
#define __A_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

void BLE_Init(void);
void BLE_Task(void *pvParameters);
void BLE_Write(uint8_t *payLoad, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif