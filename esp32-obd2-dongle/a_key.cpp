#include <Arduino.h>
#include "config.h"
#include "a_key.h"

const uint8_t KEY_InConfig[KEY_NO_MAX] ={
    // [KEY_NO_ERASE]
    GPIO_NUM_35,
};

uint8_t KEY_PrevState[KEY_NO_MAX] ={
    // [KEY_NO_ERASE]
    0x01,
};

KEY_event_t KeyEvent;
// Key de-bounce count is in 100ms resol
uint8_t KeyDebCnt;
// uint32_t KeyTimer;
SemaphoreHandle_t KEY_SemState;

void Key_Init(void)
{
    uint8_t idx;

    KeyEvent.EventType = KEY_EVENT_TYPE_NIL;
    KeyEvent.KeyNo = KEY_NO_INVALID;
    // StartTimer(KeyTimer, 100);

    for (idx = 0; idx < KEY_NO_MAX; idx++)
    {
        pinMode(KEY_InConfig[idx], INPUT);
    }

    KEY_SemState = xSemaphoreCreateBinary();
    if (KEY_SemState == NULL)
    {
        Serial_println("ERROR: Failed to create KEY State semaphore");
    }
    else
    {
        xSemaphoreGive(KEY_SemState);
    }
}

void Key_Task(void *pvParameters)
{
    int keyCurrState = 0;
    uint8_t i;

    Key_Init();

    if (KEY_SemState == NULL)
    {
        Serial_println("ERROR: KEY Task deleted due to unable to create KEY State semaphore");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        for (i = 0; i < KEY_NO_MAX; i++)
        {
            keyCurrState = digitalRead(KEY_InConfig[i]);
            // Serial_printf("INFO: KEY <%hhd>, state <%d>\r\n", i, keyCurrState);

            if ((keyCurrState == 0) && (KEY_PrevState[i] == 1))
            {
                KeyDebCnt = 0;
            }
            else if ((keyCurrState == 1) && (KEY_PrevState[i] == 0))
            {
                if (KeyDebCnt < 8)
                {
                    xSemaphoreTake(KEY_SemState, portMAX_DELAY);
                    KeyEvent.KeyNo = i;
                    KeyEvent.EventType = KEY_EVENT_TYPE_SHORT;
                    KeyDebCnt = 0;
                    xSemaphoreGive(KEY_SemState);
                }
            }

            if (keyCurrState == 0)
            {
                KeyDebCnt++;
                if (KeyDebCnt > 15)
                {
                    KeyDebCnt = 16;
                    xSemaphoreTake(KEY_SemState, portMAX_DELAY);
                    KeyEvent.KeyNo = i;
                    KeyEvent.EventType = KEY_EVENT_TYPE_LONG;
                    xSemaphoreGive(KEY_SemState);
                }
            }

            KEY_PrevState[i] = keyCurrState;
        }

        // Serial_println("INFO: KEY Task sleep");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief
 *
 * @return key_event_t
 */
KEY_event_t KEY_Read(void)
{
    KEY_event_t temp ={
        .KeyNo = KEY_NO_INVALID,
        .EventType = KEY_EVENT_TYPE_NIL,
    };

    if (KEY_SemState != NULL)
    {
        xSemaphoreTake(KEY_SemState, portMAX_DELAY);

        if (KeyEvent.KeyNo != KEY_NO_INVALID)
        {
            temp = KeyEvent;
            KeyEvent.EventType = KEY_EVENT_TYPE_NIL;
            KeyEvent.KeyNo = KEY_NO_INVALID;
        }

        xSemaphoreGive(KEY_SemState);
    }
    return temp;
}