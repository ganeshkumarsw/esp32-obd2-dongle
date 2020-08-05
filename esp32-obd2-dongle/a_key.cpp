#include <Arduino.h>
#include "config.h"
#include "a_input.h"

const uint8_t KEY_InConfig[KEY_NO_MAX] = {
    // [KEY_NO_ERASE]
    {GPIO_NUM_35},

};

uint8_t KEY_PrevState[BTN_IN_MAX];
key_event_t KeyEvent;
// Key de-bounce count is in 100ms resol
uint8_t KeyDebCnt;
uint32_t KeyTimer;

void Key_Init(void)
{
    uint8_t idx;

    KEY_PrevState[KEY_NO_UP] = 0x01;
    KeyEvent.EventType = KEY_EVENT_TYPE_NIL;
    KeyEvent.KeyNo = KEY_NO_INVALID;
    StartTimer(KeyTimer, 100);

    for (idx = 0; idx < BTN_IN_MAX; idx++)
    {
        pinMode(KEY_InConfig[idx], INPUT);
    }
}

void Key_Task(void *pvParameters)
{
    bool keyCurrState;
    uint8_t i;

    Key_Init();

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if(IsTimerElapsed(KeyTimer) == true)
        {
            ResetTimer(KeyTimer, 100);
            for(i = 0; i < KEY_NO_MAX; i++)
            {
                keyCurrState = digitalRead(KEY_InConfig[i]);

                if((keyCurrState == false) && (KEY_PrevState[i] == true))
                {
                    KeyDebCnt = 0;
                }
                else if((keyCurrState == true) && (KEY_PrevState[i] == false))
                {
                    if(KeyDebCnt < 8)
                    {
                        KeyEvent.KeyNo = i;
                        KeyEvent.EventType = KEY_EVENT_TYPE_SHORT;
                        KeyDebCnt = 0;
                    }
                }

                if(keyCurrState == false)
                {
                    KeyDebCnt++;
                    if(KeyDebCnt > 15)
                    {
                        KeyEvent.KeyNo = i;
                        KeyEvent.EventType = KEY_EVENT_TYPE_LONG;
                    }
                }

                KEY_PrevState[i] = keyCurrState;
            }
        }
    }
}

/**
 * @brief 
 * 
 * @return key_event_t 
 */
key_event_t KEY_Read(void)
{
    key_event_t temp;
    
    if(KeyEvent.KeyNo != KEY_NO_INVALID)
    {
        temp = KeyEvent;
        KeyEvent.EventType = KEY_EVENT_TYPE_NIL;
        KeyEvent.KeyNo = KEY_NO_INVALID;
    }
	else
	{
		temp.EventType = KEY_EVENT_TYPE_NIL;
		temp.KeyNo = KEY_NO_INVALID;
	}
    
    return temp;
}