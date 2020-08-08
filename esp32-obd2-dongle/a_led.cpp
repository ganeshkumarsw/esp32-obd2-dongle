#include <Arduino.h>
#include "config.h"
#include "a_led.h"

#if 0
// v1 Hardware
const LED_Config_t LED_OutConfig[LED_OUT_MAX] = {
    // [LED_OUT_1]
    {GPIO_NUM_12, LED_STATE_OFF},
    // [LED_OUT_2]
    {GPIO_NUM_14, LED_STATE_OFF},
    // [LED_OUT_3]
    {GPIO_NUM_32, LED_STATE_OFF},
    // [LED_OUT_4]
    {GPIO_NUM_33, LED_STATE_OFF},
    // [LED_OUT_5]
    {GPIO_NUM_26, LED_STATE_OFF},
    // [LED_OUT_6]
    {GPIO_NUM_25, LED_STATE_OFF},
    // [LED_OUT_7]
    {GPIO_NUM_27, LED_STATE_OFF},
};
#else
// v2 Hardware
const LED_Config_t LED_OutConfig[LED_OUT_MAX] = {
    // [LED_OUT_1]
    {GPIO_NUM_18, LED_STATE_OFF},
    // [LED_OUT_2]
    {GPIO_NUM_19, LED_STATE_OFF},
    // [LED_OUT_3]
    {GPIO_NUM_27, LED_STATE_OFF}, 
    // [LED_OUT_4]
    {GPIO_NUM_26, LED_STATE_OFF}, 
    // [LED_OUT_5]
    {GPIO_NUM_25, LED_STATE_OFF},
    // [LED_OUT_6]
    {GPIO_NUM_33, LED_STATE_OFF},   // NO LED Connected
    // [LED_OUT_7]
    {GPIO_NUM_32, LED_STATE_OFF},   // NO LED Connected
};
#endif
struct
{
    LED_Toggle_Rate_t ToggleRate;
    LED_State_t IO_State;
} LED_OutParams[LED_OUT_MAX] = {
    // [LED_OUT_1] [LED_RED]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_2] [LED_BLUE]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_3] [LED_AMBER]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_4] [LED_YELLOW]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_5] [LED_GREEN]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_6] [LED_COLOR_0]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
    // [LED_OUT_7] [LED_COLOR_1]
    {LED_TOGGLE_RATE_NONE, LED_STATE_OFF},
};

void LED_Init(void)
{
    uint8_t idx;
    const LED_Config_t *p_config;

    for (idx = 0; idx < LED_OUT_MAX; idx++)
    {
        p_config = &LED_OutConfig[idx];
        pinMode(p_config->PinNo, OUTPUT);
        digitalWrite(p_config->PinNo, p_config->Default);
    }
}

void LED_Task(void *pvParameters)
{
    uint8_t idx;
    UBaseType_t uxHighWaterMark;
    uint32_t togglefreqCntr;

    ESP_LOGI("LED", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("LED", "uxHighWaterMark = %d", uxHighWaterMark);

    LED_Init();

    togglefreqCntr = 0;

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        togglefreqCntr++;

        for (idx = 0; idx < LED_OUT_MAX; idx++)
        {
            if ((LED_OutParams[idx].ToggleRate != LED_TOGGLE_RATE_NONE) && ((togglefreqCntr % LED_OutParams[idx].ToggleRate) == 0))
            {
                // if the LED is off turn it on and vice-versa:
                if (LED_OutParams[idx].IO_State == LED_STATE_OFF)
                {
                    LED_OutParams[idx].IO_State = LED_STATE_ON;
                }
                else
                {
                    LED_OutParams[idx].IO_State = LED_STATE_OFF;
                }
            }

            digitalWrite(LED_OutConfig[idx].PinNo, LED_OutParams[idx].IO_State);
        }
    }
}

void LED_SetLedState(led_num_t gpio, LED_State_t state, LED_Toggle_Rate_t toggleRate)
{
    if (gpio < LED_OUT_MAX)
    {
        // Serial_printf("INFO: LED <%d>, State <%d>, Freq <%d>\r\n", gpio, state, toggleRate);
        LED_OutParams[gpio].IO_State = state;
        LED_OutParams[gpio].ToggleRate = toggleRate;
    }
}