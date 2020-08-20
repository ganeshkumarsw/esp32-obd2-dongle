#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "util.h"
#include "a_ble.h"
#include "a_led.h"
#include "a_key.h"
#include "a_uart.h"
#include "a_can.h"
#include "a_mqtt.h"
#include "a_wifi.h"
#include "app.h"

#define CAN_SIMULATE_FC_FRAME 0

static void APP_Frame0(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame1(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame2(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame3(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame4(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame5(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_SendRespToFrame(uint8_t respType, uint8_t APP_RESP_nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel, bool cache);

void (*cb_APP_FrameType[])(uint8_t *, uint16_t, uint8_t) =
    {
        APP_Frame0,
        APP_Frame1,
        APP_Frame2,
        APP_Frame3,
        APP_Frame4,
        APP_Frame5};

void (*cb_APP_Send[])(uint8_t *, uint16_t) =
    {
        UART_Write,
        MQTT_Write,
        BLE_Write,
        WIFI_TCP_Soc_Write,
        WIFI_WebSoc_Write};

uint8_t APP_CAN_RxBuff[4130] = {0};
uint8_t APP_CAN_TxBuff[4130] = {0};
uint16_t APP_BuffRxIndex;
uint16_t APP_CAN_TxIndex;
uint16_t APP_CAN_RxIndex;
uint16_t APP_DataLen;
bool APP_ProcDataBusyFlag;
uint8_t APP_BuffLockedBy;
uint8_t APP_Channel;
uint8_t APP_CAN_ISO_State;

uint8_t APP_SPRCOL;

CAN_speed_t APP_CAN_Baud;
uint32_t APP_CAN_TxId;
uint8_t APP_CAN_TxIdType;
uint32_t APP_CAN_RxFilterId;
uint8_t APP_CAN_RxFilterIdType;

uint8_t APP_CAN_TxMinTime;
uint32_t APP_CAN_TxMinTmr;
uint16_t APP_CAN_RqRspMaxTime;
bool APP_CAN_TransmitTstrMsg;
uint16_t APP_CAN_PaddingByte;

CAN_PROTOCOL_t APP_CAN_Protocol;
uint16_t APP_CAN_TxDataLen;
uint16_t APP_CAN_RxDataLen;

uint8_t APP_ISO_FC_RxBlockSize;
uint8_t APP_ISO_FC_RxFlag;
uint8_t APP_ISO_FC_RxSepTime;
uint8_t APP_ISO_FC_TxBlockSize;
uint8_t APP_ISO_FC_TxFlag;
uint8_t APP_ISO_TxSepTime;
uint32_t APP_ISO_TxSepTmr;
uint8_t APP_ISO_TxFrameCounter;
uint8_t APP_ISO_TxBlockCounter;
uint8_t APP_ISO_RxBlockCounter;
uint32_t APP_ISO_FC_WaitTmr;
uint32_t APP_RxResp_TimeOutTmr;

uint32_t APP_Frame01_TimeOutTmr;
uint32_t APP_SendToAppWaitTmr;

bool APP_SecurityPassStatus;
uint8_t APP_SecuityCode[] = {0x47, 0x56, 0x8A, 0xFE, 0x56, 0x21, 0x4E, 0x23, 0x80, 0x00};

uint32_t APP_AmberLedTmr;
uint32_t APP_YellowLedTmr;
uint32_t APP_GreenLedTmr;
uint8_t APP_YellowFlashCntr;
uint8_t APP_GreenFlashCntr;
bool APP_CAN_CommStatus;
bool APP_Client_CommStatus;

static void APP_SupportTask(void *pvParameters);

void APP_Init(void)
{
    APP_CAN_Baud = CAN_SPEED_500KBPS;
    APP_CAN_TxId = 0x7E0;
    APP_CAN_TxIdType = CAN_MSG_FLAG_NONE;
    APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
    APP_CAN_Protocol = APP_CAN_PROTOCOL_ISO15765;
    APP_CAN_RqRspMaxTime = 500;
    APP_Channel = APP_MSG_CHANNEL_NONE;

    LED_SetLedState(SECURITY_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_1HZ);
}

void APP_SupportTask(void *pvParameters)
{
    KEY_event_t keyEvt;

    while (1)
    {
        if (((APP_Client_CommStatus == true) || (APP_YellowFlashCntr > 0)) &&
            IsTimerElapsed(APP_YellowLedTmr))
        {
            if (APP_Client_CommStatus == true)
            {
                APP_Client_CommStatus = 0;
                APP_YellowFlashCntr = 4;
            }

            APP_YellowFlashCntr--;

            LED_SetLedState(COMM_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_5HZ);
            StartTimer(APP_YellowLedTmr, 100);
        }
        else if (APP_YellowFlashCntr == 0)
        {
            LED_SetLedState(COMM_LED, LED_STATE_OFF, LED_TOGGLE_RATE_NONE);
        }

        if (((APP_CAN_CommStatus == true) || (APP_GreenFlashCntr > 0)) &&
            IsTimerElapsed(APP_GreenLedTmr))
        {
            if (APP_CAN_CommStatus == true)
            {
                APP_CAN_CommStatus = 0;
                APP_GreenFlashCntr = 4;
            }

            APP_GreenFlashCntr--;

            LED_SetLedState(CAN_COMM_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_5HZ);
            StartTimer(APP_GreenLedTmr, 100);
        }
        else if (APP_GreenFlashCntr == 0)
        {
            LED_SetLedState(CAN_COMM_LED, LED_STATE_OFF, LED_TOGGLE_RATE_NONE);
        }

        keyEvt = KEY_Read();

        if (keyEvt.EventType != KEY_EVENT_TYPE_NIL)
        {
            switch (keyEvt.KeyNo)
            {
            case KEY_NO_ERASE:
                if (keyEvt.EventType == KEY_EVENT_TYPE_LONG)
                {
                    Preferences preferences;
                    preferences.begin("config", false);
                    preferences.putString("stSSID", String(""));
                    preferences.putString("stPASS", String(""));
                    preferences.end();
                    ESP.restart();
                }
                break;

            default:
                break;
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void APP_Task(void *pvParameters)
{
    // UBaseType_t uxHighWaterMark;
    CAN_frame_t rx_frame;
    CAN_frame_t tx_frame;
    uint16_t frameCount = 0;
    uint8_t respType;
    uint8_t respNo;
    uint16_t respLen;
    uint8_t respBuff[30];
    uint16_t len;
    uint8_t isoFrameType;
    uint8_t delay;
    uint16_t crc16;
    uint8_t lastSent_CAN_TxBytes;

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;
    respLen = 0;

    // ESP_LOGI("APP", "Task Started");

    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // ESP_LOGI("APP", "uxHighWaterMark = %d", uxHighWaterMark);

    APP_Init();
    if (xTaskCreate(APP_SupportTask, "APP_SupportTask", 2000, NULL, tskIDLE_PRIORITY, NULL) != pdTRUE)
    {
        configASSERT(0);
    }

    while (1)
    {
        vTaskDelay(1 / portTICK_PERIOD_MS);

        if (IsTimerElapsed(APP_Frame01_TimeOutTmr))
        {
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
            APP_BuffRxIndex = 0;
            StopTimer(APP_Frame01_TimeOutTmr);
        }

        if (IsTimerElapsed(APP_RxResp_TimeOutTmr))
        {
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
            APP_CAN_RxIndex = 0;

            StopTimer(APP_RxResp_TimeOutTmr);
            respLen = 0;
            respBuff[respLen++] = 0x40;
            respBuff[respLen++] = 2;

            if (((APP_Channel > APP_MSG_CHANNEL_NONE) && (APP_Channel < APP_MSG_CHANNEL_MAX)) &&
                (cb_APP_Send[APP_Channel] != NULL))
            {
                crc16 = UTIL_CRC16_CCITT(0xFFFF, respBuff, 0);
                respBuff[respLen++] = crc16 >> 8;
                respBuff[respLen++] = crc16;

                cb_APP_Send[APP_Channel](respBuff, respLen);
            }
        }

        if (APP_SecurityPassStatus == true)
        {
            tx_frame.MsgID = APP_CAN_TxId;
            tx_frame.FIR.B.FF = (CAN_frame_format_t)APP_CAN_TxIdType;

            switch (APP_CAN_Protocol)
            {
            case APP_CAN_PROTOCOL_NONE:

                break;

            case APP_CAN_PROTOCOL_ISO15765:
                if (APP_CAN_Baud != 0)
                {
                    switch (APP_CAN_ISO_State)
                    {
                    case APP_STATE_CAN_ISO_SINGLE:
                        if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_ISO_TP_TX_SF)
                        {
                            tx_frame.FIR.B.DLC = APP_CAN_TxDataLen + 1;
                            tx_frame.data.u8[0] = 0x0F & (tx_frame.FIR.B.DLC - 1);
                            memcpy((uint8_t *)&tx_frame.data.u8[1], &APP_CAN_TxBuff[0], (tx_frame.FIR.B.DLC - 1));

                            if ((tx_frame.FIR.B.DLC < 8) && (APP_CAN_PaddingByte & 0x0100))
                            {
                                memset(((uint8_t *)&tx_frame.data.u8[0] + tx_frame.FIR.B.DLC),
                                       (uint8_t)APP_CAN_PaddingByte,
                                       (8 - tx_frame.FIR.B.DLC));
                                tx_frame.FIR.B.DLC = 8;
                            }

                            APP_CAN_TxIndex = 0;
                            APP_CAN_TxDataLen = 0;
                            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;

                            APP_CAN_CommStatus = true;
                            CAN_WriteFrame(&tx_frame, portMAX_DELAY);
                        }
                        break;

                    case APP_STATE_CAN_ISO_FIRST:
                        if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_ISO_TP_TX_FF)
                        {
                            Serial_println("INFO: " str(APP_STATE_CAN_ISO_FIRST));
                            tx_frame.FIR.B.DLC = 8;
                            tx_frame.data.u8[0] = (0x0F & (uint8_t)(APP_CAN_TxDataLen >> 8)) | 0x10;
                            tx_frame.data.u8[1] = (uint8_t)APP_CAN_TxDataLen;
                            memcpy((uint8_t *)&tx_frame.data.u8[2], &APP_CAN_TxBuff[APP_CAN_TxIndex], (tx_frame.FIR.B.DLC - 2));
                            APP_CAN_TxIndex = APP_CAN_TxIndex + (tx_frame.FIR.B.DLC - 2);
                            APP_CAN_TxDataLen = APP_CAN_TxDataLen - (tx_frame.FIR.B.DLC - 2);
                            StartTimer(APP_ISO_FC_WaitTmr, APP_ISO_FC_WAIT_TIME);
                            lastSent_CAN_TxBytes = (tx_frame.FIR.B.DLC - 2);
                            APP_ISO_TxBlockCounter = 0;
                            APP_ISO_TxFrameCounter = 1;
                            APP_ISO_FC_TxFlag = APP_CAN_ISO_FC_TM_CONT;
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_FC_WAIT_TIME;
                            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_ISO_TP_TX_CF;
#if CAN_SIMULATE_FC_FRAME
                            APP_ISO_FC_TxBlockSize = 0;
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_CONSECUTIVE;
#endif
                            APP_CAN_CommStatus = true;
                            CAN_WriteFrame(&tx_frame, portMAX_DELAY);
                        }
                        break;

                    case APP_STATE_CAN_ISO_CONSECUTIVE:
                        switch (APP_ISO_FC_TxFlag)
                        {
                        case APP_CAN_ISO_FC_TM_CONT:
                            while (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_ISO_TP_TX_CF)
                            {
                                Serial_println("INFO: " str(APP_STATE_CAN_ISO_CONSECUTIVE));

                                if (APP_CAN_TxDataLen >= 7)
                                {
                                    tx_frame.FIR.B.DLC = 8;
                                }
                                else
                                {
                                    tx_frame.FIR.B.DLC = APP_CAN_TxDataLen + 1;
                                }

                                tx_frame.data.u8[0] = (0x0F & APP_ISO_TxFrameCounter) | 0x20;
                                memcpy((uint8_t *)&tx_frame.data.u8[1], &APP_CAN_TxBuff[APP_CAN_TxIndex], (tx_frame.FIR.B.DLC - 1));
                                APP_CAN_TxIndex = APP_CAN_TxIndex + (tx_frame.FIR.B.DLC - 1);
                                APP_CAN_TxDataLen = APP_CAN_TxDataLen - (tx_frame.FIR.B.DLC - 1);
                                APP_ISO_TxFrameCounter++;
                                APP_ISO_TxBlockCounter++;
                                lastSent_CAN_TxBytes = (tx_frame.FIR.B.DLC - 1);

                                if ((tx_frame.FIR.B.DLC < 8) && (APP_CAN_PaddingByte & 0x0100))
                                {
                                    memset(((uint8_t *)&tx_frame.data.u8[0] + tx_frame.FIR.B.DLC),
                                           (uint8_t)APP_CAN_PaddingByte,
                                           (8 - tx_frame.FIR.B.DLC));
                                    tx_frame.FIR.B.DLC = 8;
                                }

                                APP_CAN_CommStatus = true;
                                // Serial_printf("DEBUG: MSGID <0x%X>, DLC <0x%02X>, D <0x%02X>,<0x%02X>,<0x%02X>,<0x%02X>,<0x%02X>,<0x%02X>,<0x%02X>,<0x%02X>\r\n",
                                //               tx_frame.MsgID,
                                //               tx_frame.FIR.B.DLC,
                                //               tx_frame.data.u8[0],
                                //               tx_frame.data.u8[1],
                                //               tx_frame.data.u8[2],
                                //               tx_frame.data.u8[3],
                                //               tx_frame.data.u8[4],
                                //               tx_frame.data.u8[5],
                                //               tx_frame.data.u8[6],
                                //               tx_frame.data.u8[7]);
                                CAN_WriteFrame(&tx_frame, portMAX_DELAY);

                                if ((APP_ISO_FC_TxBlockSize > 0) && (APP_ISO_TxBlockCounter == APP_ISO_FC_TxBlockSize))
                                {
                                    StartTimer(APP_ISO_FC_WaitTmr, APP_ISO_FC_WAIT_TIME);
                                    APP_CAN_ISO_State = APP_STATE_CAN_ISO_FC_WAIT_TIME;
                                    APP_ISO_TxBlockCounter = 0;
                                }
                                else if (APP_ISO_TxSepTime)
                                {
                                    StartTimer(APP_ISO_TxSepTmr, APP_ISO_TxSepTime);
                                    APP_CAN_ISO_State = APP_STATE_CAN_ISO_SEP_TIME;
                                }

                                if (APP_CAN_TxDataLen == 0)
                                {
                                    APP_CAN_TxIndex = 0;
                                    APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                                    StopTimer(APP_ISO_FC_WaitTmr);
                                    StopTimer(APP_ISO_TxSepTmr);
                                    APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
                                }

                                if (APP_CAN_ISO_State != APP_STATE_CAN_ISO_CONSECUTIVE)
                                {
                                    break;
                                }
                            }
                            break;

                        case APP_CAN_ISO_FC_TM_WAIT:
                            Serial_println("INFO: CAN FC WAIT");
                            StartTimer(APP_ISO_TxSepTmr, 100);
                            // APP_CAN_TxIndex = APP_CAN_TxIndex - lastSent_CAN_TxBytes;
                            // APP_CAN_TxDataLen = APP_CAN_TxDataLen + lastSent_CAN_TxBytes;
                            // if (APP_ISO_TxBlockCounter > 0)
                            // {
                            //     APP_ISO_TxBlockCounter--;
                            // }

                            // if (APP_ISO_TxFrameCounter > 1)
                            // {
                            //     APP_ISO_TxFrameCounter--;
                            // }
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_SEP_TIME;
                            break;

                        case APP_CAN_ISO_FC_TM_ABORT:
                            Serial_println("ERROR: CAN FC ABORT");
                            APP_CAN_TxIndex = 0;
                            APP_CAN_TxDataLen = 0;
                            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                            StopTimer(APP_ISO_FC_WaitTmr);
                            StopTimer(APP_ISO_TxSepTmr);
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
                            break;

                        default:
                            Serial_printf("ERROR: CAN FC <%d> not supported\r\n", APP_ISO_FC_TxFlag);
                            APP_CAN_TxIndex = 0;
                            APP_CAN_TxDataLen = 0;
                            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                            StopTimer(APP_ISO_FC_WaitTmr);
                            StopTimer(APP_ISO_TxSepTmr);
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
                            break;
                        }
                        break;

                    case APP_STATE_CAN_ISO_SEP_TIME:
                        if (IsTimerElapsed(APP_ISO_TxSepTmr))
                        {
                            StopTimer(APP_ISO_TxSepTmr);
                            APP_ISO_FC_TxFlag = APP_CAN_ISO_FC_TM_CONT;
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_CONSECUTIVE;
                        }
                        break;

                    case APP_STATE_CAN_ISO_FC_WAIT_TIME:
                        if (IsTimerElapsed(APP_ISO_FC_WaitTmr))
                        {
                            StopTimer(APP_ISO_FC_WaitTmr);
                            StopTimer(APP_ISO_TxSepTmr);
                            APP_CAN_TxIndex = 0;
                            APP_CAN_TxDataLen = 0;
                            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
                        }
                        break;

                    case APP_STATE_CAN_ISO_SEND_TO_APP:
                        APP_Client_CommStatus = true;

                        respLen = 0;

                        if (((APP_Channel > APP_MSG_CHANNEL_NONE) && (APP_Channel < APP_MSG_CHANNEL_MAX)) && (cb_APP_Send[APP_Channel] != NULL))
                        {
                            crc16 = UTIL_CRC16_CCITT(0xFFFF, APP_CAN_RxBuff, APP_CAN_RxDataLen);
                            for (uint32_t i = APP_CAN_RxDataLen; i > 0; i--)
                            {
                                APP_CAN_RxBuff[i + 1] = APP_CAN_RxBuff[i - 1];
                            }
                            APP_CAN_RxBuff[respLen++] = 0x40 | (((APP_CAN_RxDataLen + 2) >> 8) & 0x0F);
                            APP_CAN_RxBuff[respLen++] = (APP_CAN_RxDataLen + 2);
                            respLen = respLen + APP_CAN_RxDataLen;
                            APP_CAN_RxBuff[respLen++] = crc16 >> 8;
                            APP_CAN_RxBuff[respLen++] = crc16;

                            cb_APP_Send[APP_Channel](APP_CAN_RxBuff, respLen);
                        }

                        APP_CAN_RxDataLen = 0;
                        APP_CAN_ISO_State = APP_STATE_CAN_ISO_IDLE;
                        APP_CAN_RxIndex = 0;
                        APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                        StopTimer(APP_SendToAppWaitTmr);
                        break;

                    case APP_STATE_CAN_ISO_IDLE:

                        break;

                    default:
                        Serial_println("ERROR Invalid APP ISO state");
                        break;
                    }

                    if ((((APP_Channel > APP_MSG_CHANNEL_NONE) && (APP_Channel < APP_MSG_CHANNEL_MAX)) || (APP_CAN_ISO_State == APP_STATE_CAN_ISO_FC_WAIT_TIME)) &&
                        (CAN_ReadFrame(&rx_frame, pdMS_TO_TICKS(0)) == ESP_OK) &&
                        (rx_frame.FIR.B.RTR == CAN_no_RTR) &&
                        (rx_frame.MsgID == APP_CAN_RxFilterId) &&
                        (rx_frame.FIR.B.FF == APP_CAN_RxFilterIdType))
                    {
                        APP_CAN_CommStatus = true;
                        isoFrameType = rx_frame.data.u8[0] >> 4;

                        switch (isoFrameType)
                        {
                        case APP_CAN_ISO_TYPE_SINGLE:
                            respLen = 0;
                            respBuff[respLen++] = 0x40;
                            respBuff[respLen++] = ((rx_frame.data.u8[0] & 0x0F) + 2);

                            memcpy(&respBuff[respLen], &rx_frame.data.u8[1], (rx_frame.data.u8[0] & 0x0F));
                            respLen += (rx_frame.data.u8[0] & 0x0F);

                            if (cb_APP_Send[APP_Channel] != NULL)
                            {
                                crc16 = UTIL_CRC16_CCITT(0xFFFF, &respBuff[2], (respLen - 2));
                                respBuff[respLen++] = crc16 >> 8;
                                respBuff[respLen++] = crc16;

                                cb_APP_Send[APP_Channel](respBuff, respLen);
                            }

                            StopTimer(APP_RxResp_TimeOutTmr);
                            break;

                        case APP_CAN_ISO_TYPE_FIRST:
                            APP_CAN_RxDataLen = ((uint16_t)(0x0F & rx_frame.data.u8[0]) << 8) | (uint16_t)rx_frame.data.u8[1];
                            APP_CAN_RxIndex = 0;
                            memcpy(&APP_CAN_RxBuff[APP_CAN_RxIndex], &rx_frame.data.u8[2], 6);
                            APP_CAN_RxIndex = APP_CAN_RxIndex + 6;
                            StartTimer(APP_RxResp_TimeOutTmr, APP_CAN_RqRspMaxTime);

                            tx_frame.data.u8[0] = 0x30;
                            tx_frame.MsgID = APP_CAN_TxId;
                            tx_frame.FIR.B.FF = (CAN_frame_format_t)APP_CAN_TxIdType;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[1] = APP_ISO_FC_RxBlockSize;
                            tx_frame.data.u8[2] = APP_ISO_FC_RxSepTime;

                            if ((tx_frame.FIR.B.DLC < 8) && (APP_CAN_PaddingByte & 0x0100))
                            {
                                memset(((uint8_t *)&tx_frame.data.u8[0] + tx_frame.FIR.B.DLC),
                                       (uint8_t)APP_CAN_PaddingByte,
                                       (8 - tx_frame.FIR.B.DLC));
                                tx_frame.FIR.B.DLC = 8;
                            }

                            APP_CAN_CommStatus = true;
                            CAN_WriteFrame(&tx_frame, portMAX_DELAY);
                            break;

                        case APP_CAN_ISO_TYPE_CONSECUTIVE:
                            if (IsTimerRunning(APP_RxResp_TimeOutTmr))
                            {
                                StartTimer(APP_RxResp_TimeOutTmr, APP_CAN_RqRspMaxTime);
                            }
                            memcpy(&APP_CAN_RxBuff[APP_CAN_RxIndex], &rx_frame.data.u8[1], rx_frame.FIR.B.DLC - 1);
                            APP_CAN_RxIndex = APP_CAN_RxIndex + (rx_frame.FIR.B.DLC - 1);
                            APP_ISO_RxBlockCounter++;

                            if (APP_CAN_RxIndex >= APP_CAN_RxDataLen)
                            {
                                StopTimer(APP_RxResp_TimeOutTmr);
                                APP_CAN_ISO_State = APP_STATE_CAN_ISO_SEND_TO_APP;
                            }
                            else if ((APP_ISO_FC_RxBlockSize > 0) && (APP_ISO_RxBlockCounter == APP_ISO_FC_RxBlockSize))
                            {
                                APP_ISO_RxBlockCounter = 0;
                                tx_frame.MsgID = APP_CAN_TxId;
                                tx_frame.FIR.B.FF = (CAN_frame_format_t)APP_CAN_TxIdType;
                                tx_frame.FIR.B.DLC = 3;
                                tx_frame.data.u8[0] = 0x30;
                                tx_frame.data.u8[1] = APP_ISO_FC_RxBlockSize;
                                tx_frame.data.u8[2] = APP_ISO_FC_RxSepTime;

                                APP_CAN_CommStatus = true;
                                CAN_WriteFrame(&tx_frame, portMAX_DELAY);
                            }
                            break;

                        case APP_CAN_ISO_TYPE_FLOWCONTROL:
                            APP_ISO_FC_TxFlag = rx_frame.data.u8[0] & 0x0F;
                            APP_ISO_FC_TxBlockSize = rx_frame.data.u8[1];

                            if (rx_frame.data.u8[2] <= 127)
                            {
                                APP_ISO_TxSepTime = rx_frame.data.u8[2];
                            }
                            else
                            {
                                APP_ISO_TxSepTime = 0;
                            }

                            APP_CAN_ISO_State = APP_STATE_CAN_ISO_CONSECUTIVE;
                            StopTimer(APP_ISO_FC_WaitTmr);
                            break;

                        default:
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel)
{
    uint8_t frameType;
    uint16_t frameLen;
    uint8_t respType;
    uint8_t respNo;
    uint16_t crc16Act;
    uint16_t crc16Calc;
    uint16_t respLen;
    uint8_t respBuff[50];

    APP_Client_CommStatus = true;

    respLen = 0;
    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;

    // Serial_println(String("Data Received from channel ") + channel);

    if (APP_ProcDataBusyFlag == false)
    {
        APP_Channel = channel;
        APP_ProcDataBusyFlag = true;

        frameType = p_buff[0] >> 4;
        frameLen = ((uint16_t)(p_buff[0] & 0x0F) << 8) | (uint16_t)p_buff[1];

        if (frameLen == (len - 2))
        {
            crc16Act = ((uint16_t)p_buff[len - 2] << 8) | (uint16_t)p_buff[len - 1];
            crc16Calc = UTIL_CRC16_CCITT(0xFFFF, &p_buff[2], (frameLen - 2));

            if (crc16Act == crc16Calc)
            {
                if (frameType < (sizeof(cb_APP_FrameType) / sizeof(cb_APP_FrameType[0])))
                {
                    if (APP_SecurityPassStatus == true)
                    {
                        if (cb_APP_FrameType[frameType] != NULL)
                        {
                            cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), (uint8_t)channel);
                        }
                    }
                    else
                    {
                        if (frameType == 5)
                        {
                            if (cb_APP_FrameType[frameType] != NULL)
                            {
                                cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), (uint8_t)channel);
                            }
                        }
                        else
                        {
                            respType = APP_RESP_NACK;
                            respNo = APP_RESP_NACK_33;
                        }
                    }
                }
                else
                {
                    respType = APP_RESP_NACK;
                    respNo = APP_RESP_NACK_10;
                }
            }
            else
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_15;

                respBuff[respLen++] = crc16Act >> 8;
                respBuff[respLen++] = crc16Act;
                respBuff[respLen++] = crc16Calc >> 8;
                respBuff[respLen++] = crc16Calc;
            }
        }
        else
        {
            respType = APP_RESP_NACK;
            // Invalid format or incorrect message length of input
            respNo = APP_RESP_NACK_13;
        }

        APP_ProcDataBusyFlag = false;
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_14;
    }

    if (respType != APP_RESP_ACK)
    {
        APP_SendRespToFrame(respType, respNo, respBuff, respLen, (uint8_t)channel, false);
    }
}

/**
 * First Frame
 * @param p_buff
 * @param len
 */
void APP_Frame0(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;
    uint16_t respLen;
    uint8_t respBuff[50];

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;
    respLen = 0;

    if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_NONE)
    {
        APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME0;
        APP_CAN_TxDataLen = ((uint16_t)p_buff[0] << 8) | (uint16_t)p_buff[1];
        APP_BuffRxIndex = 0;

        if (APP_CAN_TxDataLen > 4095)
        {
            respType = APP_RESP_NACK;
            respNo = APP_RESP_NACK_13;
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
        }
        else
        {
            memcpy(&APP_CAN_TxBuff[APP_BuffRxIndex], &p_buff[2], (len - 2));
            APP_BuffRxIndex += (len - 2);
            StartTimer(APP_Frame01_TimeOutTmr, 10000);
        }
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_14;
    }

    respBuff[respLen++] = APP_BuffRxIndex >> 8;
    respBuff[respLen++] = APP_BuffRxIndex;

    APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
}

/**
 * Consecutive Frame
 * @param p_buff
 * @param len
 */
void APP_Frame1(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;
    uint16_t respLen;
    uint8_t respBuff[50];

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;
    respLen = 0;

    if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_FRAME0)
    {
        if (APP_CAN_TxDataLen && ((APP_BuffRxIndex + len) <= 4095))
        {
            StartTimer(APP_Frame01_TimeOutTmr, 10000);
            memcpy(&APP_CAN_TxBuff[APP_BuffRxIndex], p_buff, len);
            APP_BuffRxIndex += len;

            if (APP_BuffRxIndex >= APP_CAN_TxDataLen)
            {
                APP_BuffRxIndex = 0;
                APP_CAN_TxIndex = 0;
                APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME1;
                StopTimer(APP_Frame01_TimeOutTmr);

                if (APP_CAN_Protocol == APP_CAN_PROTOCOL_ISO15765)
                {
                    if (APP_CAN_TxDataLen < 8)
                    {
                        APP_CAN_ISO_State = APP_STATE_CAN_ISO_SINGLE;
                    }
                    else
                    {
                        APP_CAN_ISO_State = APP_STATE_CAN_ISO_FIRST;
                    }
                }
                else if (APP_CAN_Protocol == APP_CAN_PROTOCOL_NORMAL)
                {
                    StartTimer(APP_CAN_TxMinTmr, APP_CAN_TxMinTime);
                }

                StartTimer(APP_RxResp_TimeOutTmr, APP_CAN_RqRspMaxTime);
            }
        }
        else
        {
            respType = APP_RESP_NACK;
            respNo = APP_RESP_NACK_13;
            APP_BuffRxIndex = 0;
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
        }
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_14;
        APP_BuffRxIndex = 0;
        APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
    }

    respBuff[respLen++] = APP_BuffRxIndex >> 8;
    respBuff[respLen++] = APP_BuffRxIndex;

    APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
}

/**
 * Command Frame
 * @param p_buff
 * @param len
 */
void APP_Frame2(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t offset;
    uint8_t respType;
    uint8_t respNo;
    uint16_t respLen;
    uint8_t respBuff[20];
    bool canFilterMask;

    canFilterMask = true;
    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;
    respLen = 0;
    offset = 0;

    if (len > 0)
    {
        switch (p_buff[0])
        {
        case APP_REQ_CMD_RESET:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_Baud = CAN_SPEED_500KBPS;
            APP_CAN_Protocol = APP_CAN_PROTOCOL_NONE;
            // Reset or disable CAN
            CAN_DeInit();
            break;

        case APP_REQ_CMD_SET_PROTOCOL:
            if (len != 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_SPRCOL = p_buff[1];

            if (p_buff[1] < 6)
            {
                offset = 0;
                APP_CAN_Protocol = APP_CAN_PROTOCOL_ISO15765;
            }
            else if (p_buff[1] < 0x0C)
            {
                offset = 6;
                APP_CAN_Protocol = APP_CAN_PROTOCOL_NORMAL;
            }
            else if (p_buff[1] < 0x12)
            {
                offset = 0x0C;
                APP_CAN_Protocol = APP_CAN_PROTOCOL_OE_IVN;
                // Accept all messages from any CAN ID
                canFilterMask = false;
            }
            else
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_12;
            }

            if ((p_buff[1] - offset) < 2)
            {
                APP_CAN_Baud = CAN_SPEED_250KBPS;
            }
            else if ((p_buff[1] - offset) < 4)
            {
                APP_CAN_Baud = CAN_SPEED_500KBPS;
            }
            else if ((p_buff[1] - offset) < 6)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_12;
            }
            else
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_12;
            }

            if ((p_buff[1] - offset) % 2)
            {
                APP_CAN_TxIdType = CAN_MSG_FLAG_EXTD;
            }
            else
            {
                APP_CAN_TxIdType = CAN_MSG_FLAG_NONE; // Standard frame;
            }

            if (respType == APP_RESP_ACK)
            {
                APP_CAN_RqRspMaxTime = 500;
                APP_CAN_TxMinTime = 10;
                StopTimer(APP_ISO_TxSepTmr);
                StopTimer(APP_ISO_FC_WaitTmr);
                StopTimer(APP_Frame01_TimeOutTmr);
                StopTimer(APP_RxResp_TimeOutTmr);
                APP_CAN_TxIndex = 0;
                APP_CAN_RxIndex = 0;
                APP_BuffRxIndex = 0;
                APP_CAN_TxDataLen = 0;
                APP_CAN_RxDataLen = 0;
                APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                CAN_SetBaud(APP_CAN_Baud);
                CAN_ConfigFilterterMask(0x00000000, CAN_frame_ext);
            }
            break;

        case APP_REQ_CMD_GET_PROTOCOL:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = APP_SPRCOL;
            respLen = 1;
            break;

        case APP_REQ_CMD_SET_TX_CAN_ID:
            if (len == 3)
            {
                APP_CAN_TxIdType = (uint8_t)CAN_frame_std; // Standard frame
                APP_CAN_TxId = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
            }
            else if (len == 5)
            {
                APP_CAN_TxIdType = (uint8_t)CAN_frame_ext;
                APP_CAN_TxId = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
            }
            else
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
            }
            break;

        case APP_REQ_CMD_GET_TX_CAN_ID:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            // Standard frame
            if (APP_CAN_TxIdType == CAN_MSG_FLAG_NONE)
            {
                respBuff[0] = (uint8_t)(APP_CAN_TxId >> 8);
                respBuff[1] = (uint8_t)APP_CAN_TxId;
                respLen = 2;
            }
            else
            {
                respBuff[0] = (uint8_t)(APP_CAN_TxId >> 24);
                respBuff[1] = (uint8_t)(APP_CAN_TxId >> 16);
                respBuff[2] = (uint8_t)(APP_CAN_TxId >> 8);
                respBuff[3] = (uint8_t)APP_CAN_TxId;
                respLen = 4;
            }
            break;

        case APP_REQ_CMD_SET_RX_CAN_ID:
            if (len == 3)
            {
                APP_CAN_RxFilterIdType = CAN_frame_std; // Standard frame
                APP_CAN_RxFilterId = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
            }
            else if (len == 5)
            {
                APP_CAN_RxFilterIdType = CAN_frame_ext;
                APP_CAN_RxFilterId = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
            }
            else
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            CAN_ConfigFilterterMask(APP_CAN_RxFilterId, (bool)APP_CAN_RxFilterIdType);
            break;

        case APP_REQ_CMD_GET_RX_CAN_ID:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            // Standard frame
            if (APP_CAN_RxFilterIdType == CAN_MSG_FLAG_NONE)
            {
                respBuff[0] = (uint8_t)(APP_CAN_RxFilterId >> 8);
                respBuff[1] = (uint8_t)APP_CAN_RxFilterId;
                respLen = 2;
            }
            else
            {
                respBuff[0] = (uint8_t)(APP_CAN_RxFilterId >> 24);
                respBuff[1] = (uint8_t)(APP_CAN_RxFilterId >> 16);
                respBuff[2] = (uint8_t)(APP_CAN_RxFilterId >> 8);
                respBuff[3] = (uint8_t)APP_CAN_RxFilterId;
                respLen = 4;
            }
            break;

        case APP_REQ_CMD_SFCBLKL:
            if (len != 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_ISO_FC_RxBlockSize = p_buff[1];
            break;

        case APP_REQ_CMD_GFCBLKL:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = APP_ISO_FC_RxBlockSize;
            respLen = 1;
            break;

        case APP_REQ_CMD_SFCST:
            if (len != 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_ISO_FC_RxSepTime = p_buff[1];
            break;

        case APP_REQ_CMD_GFCST:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = APP_ISO_FC_RxSepTime;
            respLen = 1;
            break;

        case APP_REQ_CMD_SETP1MIN:
            if (len != 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_TxMinTime = p_buff[1];
            break;

        case APP_REQ_CMD_GETP1MIN:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = APP_CAN_TxMinTime;
            respLen = 1;
            break;

        case APP_REQ_CMD_SETP2MAX:
            if (len != 3)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_RqRspMaxTime = ((uint16_t)p_buff[1] << 8) | (uint16_t)p_buff[2];
            break;

        case APP_REQ_CMD_GETP2MAX:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = APP_CAN_RqRspMaxTime >> 8;
            respBuff[1] = APP_CAN_RqRspMaxTime;
            respLen = 2;
            break;

        case APP_REQ_CMD_TXTP:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_TransmitTstrMsg = true;
            break;

        case APP_REQ_CMD_STPTXTP:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_TransmitTstrMsg = false;
            break;

        case APP_REQ_CMD_TXPAD:
            if (len != 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_PaddingByte = 0x0100 | (uint16_t)p_buff[1];
            break;

        case APP_REQ_CMD_STPTXPAD:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            APP_CAN_PaddingByte = 0x0000;
            break;

        case APP_REQ_CMD_GET_FIRMWARE_VER:
            if (len != 1)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            respBuff[0] = MAJOR_VERSION;
            respBuff[1] = MINOR_VERSION;
            respBuff[2] = SUB_VERSION;
            respLen = 3;
            break;

        case APP_REQ_CMD_SET_STA_SSID:
            if (len < 5)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }
            p_buff[len - 1] = 0;

            WIFI_Set_STA_SSID((char *)&p_buff[1]);
            break;

        case APP_REQ_CMD_SET_STA_PASSWORD:
            if (len < 5)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            p_buff[len - 1] = 0;
            WIFI_Set_STA_Pass((char *)&p_buff[1]);
            break;

        case APP_REQ_CMD_ENABLE_CAN_TX_DELAY:
            if (len < 2)
            {
                respType = APP_RESP_NACK;
                respNo = APP_RESP_NACK_13;
                break;
            }

            if (p_buff[1] == 0)
            {
                CAN_EnableInterframeDelay(0);
            }
            else if (p_buff[1] == 1)
            {
                CAN_EnableInterframeDelay(800);
            }
            else
            {
                CAN_EnableInterframeDelay(((uint32_t)p_buff[1] * 100));
            }
            break;

        default:
            respType = APP_RESP_NACK;
            respNo = APP_RESP_NACK_10;
            break;
        }
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_13;
    }

    APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
}

/**
 * Reset Frame
 * @param p_buff
 * @param len
 */
void APP_Frame3(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;

    if (p_buff[0] == 1)
    {
        ESP.restart();
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_22;
    }

    APP_SendRespToFrame(respType, respNo, NULL, 0, channel, false);
}

/**
 * Consecutive Frame
 * @param p_buff
 * @param len
 */
void APP_Frame4(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;
    uint16_t respLen;
    uint8_t respBuff[50];

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;
    respLen = 0;

    if (APP_BuffLockedBy == APP_BUFF_LOCKED_BY_NONE)
    {
        if (len <= 4096)
        {
            memcpy(APP_CAN_TxBuff, p_buff, len);
            APP_CAN_TxDataLen = len;

            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_ISO_TP_TX_FF;

            if (APP_CAN_Protocol == APP_CAN_PROTOCOL_ISO15765)
            {
                APP_CAN_ISO_State = APP_STATE_CAN_ISO_FIRST;

                if (APP_CAN_TxDataLen < 8)
                {
                    APP_CAN_ISO_State = APP_STATE_CAN_ISO_SINGLE;
                    APP_BuffLockedBy = APP_BUFF_LOCKED_BY_ISO_TP_TX_SF;
                }
            }
            else if (APP_CAN_Protocol == APP_CAN_PROTOCOL_NORMAL)
            {
                StartTimer(APP_CAN_TxMinTmr, 1);
                APP_BuffLockedBy = APP_BUFF_LOCKED_BY_CAN_TX;
            }

            APP_CAN_TxIndex = 0;
            StartTimer(APP_RxResp_TimeOutTmr, APP_CAN_RqRspMaxTime);
        }
        else
        {
            respType = APP_RESP_NACK;
            respNo = APP_RESP_NACK_13;
        }
    }
    else
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_14;
    }

    APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
}

void APP_Frame5(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;

    respType = APP_RESP_ACK;
    respNo = APP_RESP_ACK;

    if (len != (uint16_t)sizeof(APP_SecuityCode))
    {
        APP_SecurityPassStatus = false;
    }
    else
    {
        if (memcmp(p_buff, APP_SecuityCode, sizeof(APP_SecuityCode)) == 0)
        {
            APP_SecurityPassStatus = true;
            LED_SetLedState(SECURITY_LED, LED_STATE_ON, LED_TOGGLE_RATE_NONE);
        }
        else
        {
            APP_SecurityPassStatus = false;
            LED_SetLedState(SECURITY_LED, LED_STATE_TOGGLE, LED_TOGGLE_RATE_5HZ);
        }
    }

    if (APP_SecurityPassStatus == false)
    {
        respType = APP_RESP_NACK;
        respNo = APP_RESP_NACK_35;
    }

    APP_SendRespToFrame(respType, respNo, NULL, 0, channel, false);
}

void APP_SendRespToFrame(uint8_t respType, uint8_t APP_RESP_nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel, bool cache)
{
    uint8_t len;
    uint8_t buff[30];
    uint16_t crc16;

    len = 0;
    buff[len++] = 0x20;

    if (respType > 0)
    {
        buff[len++] = 2 + 2;
        buff[len++] = respType;
        buff[len++] = APP_RESP_nackNo;
    }
    else
    {
        buff[len++] = 1 + 2;
        buff[len++] = respType;
    }

    if (((channel > APP_MSG_CHANNEL_NONE) && (channel < APP_MSG_CHANNEL_MAX)) &&
        (cb_APP_Send[channel] != NULL))
    {
        if (dataLen && (p_buff != NULL))
        {
            memcpy(&buff[len], p_buff, dataLen);
            buff[1] += dataLen;
            len += dataLen;
        }

        // Exclude header 2Bytes and CRC 2 Bytes
        crc16 = UTIL_CRC16_CCITT(0xFFFF, &buff[2], (len - 2));
        buff[len++] = crc16 >> 8;
        buff[len++] = crc16;

        cb_APP_Send[channel](buff, len);
    }
}
