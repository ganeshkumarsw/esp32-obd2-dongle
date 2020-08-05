#ifndef __A_KEY_H__
#define __A_KEY_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"
#include "driver/gpio.h"

typedef struct 
{
    PORT_t *p_Port;
    uint8_t Pin;
}key_map_t;

/**
    Section: Device Pin Macros
*/

typedef enum
{
    KEY_EVENT_TYPE_NIL       = 0x00,
    KEY_EVENT_TYPE_SHORT     = 0x01,
    KEY_EVENT_TYPE_LONG      = 0x02,
}key_event_type_t;

typedef struct 
{
    int8_t KeyNo;
    uint8_t EventType;
}key_event_t;

/**
    Section: Function Prototypes
*/
void KEY_Init(void);
void KEY_Task(void);
key_event_t KEY_Read(void);

#ifdef __cplusplus
}
#endif

#endif