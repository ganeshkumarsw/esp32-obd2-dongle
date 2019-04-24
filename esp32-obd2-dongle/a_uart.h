#ifndef __A_UART_H__
#define __A_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

void UART_Init(void);
void UART_Task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif