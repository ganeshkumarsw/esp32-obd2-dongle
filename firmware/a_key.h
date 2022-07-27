#ifndef __A_KEY_H__
#define __A_KEY_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"
#include "driver/gpio.h"

/**
    Section: Device Pin Macros
*/

typedef enum
{
    KEY_EVENT_TYPE_NIL       = 0x00,
    KEY_EVENT_TYPE_SHORT     = 0x01,
    KEY_EVENT_TYPE_LONG      = 0x02,
}KEY_event_type_t;

typedef struct 
{
    int8_t KeyNo;
    uint8_t EventType;
}KEY_event_t;

/**
    Section: Function Prototypes
*/
void KEY_Init(void);
void Key_Task(void *pvParameters);
KEY_event_t KEY_Read(void);

#ifdef __cplusplus
}
#endif

#endif