#ifndef __A_LED_H__
#define __A_LED_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"
#include "driver/gpio.h"

    typedef struct
    {
        uint8_t PinNo;
        uint8_t Default;
    } LED_Config_t;

    typedef struct
    {
        uint8_t Led;
    } LED_Map_t;

    typedef enum
    {
        LED_OP_MODE_TOGGLE = 0,
        LED_OP_MODE_FIXED,
    } LED_Op_Mode_t;

    typedef enum
    {
        LED_STATE_HIGH = 0,
        LED_STATE_LOW,
        LED_STATE_TOGGLE,
    } LED_State_t;

    typedef enum
    {
        LED_TOGGLE_RATE_NONE = 0,
        LED_TOGGLE_RATE_5HZ = 10,
        LED_TOGGLE_RATE_1HZ = 50,
    } LED_Toggle_Rate_t;

    void LED_Init(void);
    void LED_Task(void *pvParameters);
    void LED_SetLedState(led_num_t gpio, LED_State_t state, LED_Toggle_Rate_t toggleRate);

#ifdef __cplusplus
}
#endif

#endif