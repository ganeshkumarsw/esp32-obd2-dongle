#ifndef __A_CAN_H__
#define __A_CAN_H__

#include <ESP32CAN.h>
#include <CAN_config.h>

#ifdef __cplusplus
extern "C" {
#endif

void CAN_Init(void);
void CAN_SetBaud(CAN_speed_t speed);
void CAN_ConfigFilterterMask(CAN_filter_t *p_filter);
void CAN_DeInit(void);
BaseType_t CAN_ReadFrame(CAN_frame_t *frame);
void CAN_WriteFrame(CAN_frame_t *frame);
void CAN_Task(void *pvParameters);


#ifdef __cplusplus
}
#endif

#endif