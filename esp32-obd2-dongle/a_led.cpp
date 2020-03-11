#include <Arduino.h>
#include "config.h"
#include "a_led.h"

#if 0
const LED_Config_t LED_OutConfig[LED_OUT_MAX] = {
    // [LED_OUT_1]
    {GPIO_NUM_12, LED_STATE_LOW},
    // [LED_OUT_2]
    {GPIO_NUM_14, LED_STATE_LOW},
    // [LED_OUT_3]
    {GPIO_NUM_32, LED_STATE_LOW},
    // [LED_OUT_4]
    {GPIO_NUM_33, LED_STATE_LOW},
    // [LED_OUT_5]
    {GPIO_NUM_26, LED_STATE_LOW},
    // [LED_OUT_6]
    {GPIO_NUM_25, LED_STATE_LOW},
    // [LED_OUT_7]
    {GPIO_NUM_27, LED_STATE_LOW},
};
#else
const LED_Config_t LED_OutConfig[LED_OUT_MAX] = {
    // [LED_OUT_1]
    {GPIO_NUM_18, LED_STATE_LOW},
    // [LED_OUT_2]
    {GPIO_NUM_19, LED_STATE_LOW},
    // [LED_OUT_3]
    {GPIO_NUM_32, LED_STATE_LOW},
    // [LED_OUT_4]
    {GPIO_NUM_33, LED_STATE_LOW},
    // [LED_OUT_5]
    {GPIO_NUM_26, LED_STATE_LOW},
    // [LED_OUT_6]
    {GPIO_NUM_25, LED_STATE_LOW},
    // [LED_OUT_7]
    {GPIO_NUM_27, LED_STATE_LOW},
};
#endif
struct
{
    LED_Toggle_Rate_t ToggleRate;
    const LED_Op_Mode_t OpMode;
    LED_State_t IO_State;
}LED_OutParams[LED_OUT_MAX] = {
    // [LED_OUT_1]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_2]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_3]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_4]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_5]
    {LED_TOGGLE_RATE_5HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_6]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
    // [LED_OUT_7]
    {LED_TOGGLE_RATE_1HZ, LED_OP_MODE_TOGGLE, LED_STATE_LOW},
};

void LED_Init(void)
{
    uint8_t idx;
    const LED_Config_t *p_config;

    for(idx = 0; idx < LED_OUT_MAX; idx++)
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

        for(idx = 0; idx < LED_OUT_MAX; idx++)
        {
            if (LED_OutParams[idx].OpMode == LED_OP_MODE_TOGGLE)
            {
                if (LED_OutParams[idx].ToggleRate && ((togglefreqCntr % LED_OutParams[idx].ToggleRate) == 0))
                {
                    // if the LED is off turn it on and vice-versa:
                    if (LED_OutParams[idx].IO_State == LED_STATE_LOW)
                    {
                        LED_OutParams[idx].IO_State = LED_STATE_HIGH;
                    }
                    else
                    {
                        LED_OutParams[idx].IO_State = LED_STATE_LOW;
                    }
                }
            }

            digitalWrite(LED_OutConfig[idx].PinNo, LED_OutParams[idx].IO_State);
        }
    }
}

void LED_SetLedState(led_num_t gpio, LED_State_t state, LED_Toggle_Rate_t toggleRate)
{
    if(gpio < LED_OUT_MAX)
    {
        // Serial.println(String("PIN: ") + state + ", Freq: " + toggleRate);
        if(LED_OutParams[gpio].OpMode == LED_OP_MODE_FIXED)
        {
            LED_OutParams[gpio].IO_State = state;
            LED_OutParams[gpio].ToggleRate = LED_TOGGLE_RATE_NONE;
        }
        else
        {
            
            LED_OutParams[gpio].ToggleRate = toggleRate;
        }
    }
}