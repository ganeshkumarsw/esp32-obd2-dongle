#include <Arduino.h>
#include "util.h"
#include "a_ble.h"
#include "a_uart.h"
#include "a_can.h"
#include "a_mqtt.h"
#include "app.h"

#define APP_ISO_FC_WAIT_TIME    10000

#define RSTVC       1
#define SPRCOL      2
#define GPRCOL      3
#define STXHDR      4
#define GTXHDR      5
#define SRXHDRMSK   6
#define GRXHDRMSK   7
#define SFCBLKL     8
#define GFCBLKL     9
#define SFCST       10
#define GFCST       11
#define SETP1MIN    12
#define GETP1MIN    13
#define SETP2MAX    14
#define GETP2MAX    15
#define TXTP        16
#define STPTXTP     17
#define TXPAD       18
#define STPTXPAD    19
#define GETFWVER    20

//typedef void (*cb_APP_FrameType)(uint8_t *, uint16_t);

#define ACK     0	    // Positive Response
#define NACK    1	    // Negative Response
#define NACK10  0x10	// Command Not Supported
#define NACK12  0x12	// Input Not supported
#define NACK13  0x13	// Invalid format or incorrect message length of input
#define NACK14  0x14	// Invalid operation
#define NACK15  0x15	// CRC failure
#define NACK22  0x22	// Conditions not correct
#define NACK31  0x31	// Request out of range
#define NACK33  0x33	// security access denied
#define NACK78  0x78	// response pending
#define NACK24  0x24	// request sequence error
#define NACK35  0x35	// Invalid Key
#define NACK36  0x36	// exceeded number of attempts
#define NACK37  0x37	// required time delay not expired
#define NACK72  0x72	// General programming failure
#define NACK7E  0x7E	// sub fn not supported in this diag session

typedef enum
{
    APP_CAN_PROTOCOL_NONE = 0,
    APP_CAN_PROTOCOL_ISO15765,
    APP_CAN_PROTOCOL_NORMAL,
    APP_CAN_PROTOCOL_OE_IVN,
}CAN_PROTOCOL_t;

typedef enum
{
    APP_ISO_STATE_SINGLE = 0,
    APP_ISO_STATE_FIRST,
    APP_ISO_STATE_CONSECUTIVE,
    APP_ISO_STATE_SEP_TIME,
    APP_ISO_STATE_FC_WAIT_TIME,
    APP_ISO_STATE_SEND_TO_APP,
    APP_ISO_STATE_IDLE,
}APP_ISO_STATE_t;

typedef enum
{
    APP_ISO_TYPE_SINGLE = 0,
    APP_ISO_TYPE_FIRST,
    APP_ISO_TYPE_CONSECUTIVE,
    APP_ISO_TYPE_FLOWCONTROL,
}APP_ISO_TYPE_t;

typedef enum
{
    APP_BUFF_LOCKED_BY_NONE = 0,
    APP_BUFF_LOCKED_BY_FRAME0,
    APP_BUFF_LOCKED_BY_FRAME1,
    APP_BUFF_LOCKED_BY_FRAME4,
    APP_BUFF_LOCKED_BY_ISO_TP_RX_FF,
    APP_BUFF_LOCKED_BY_ISO_TP_RX_CF,       
}APP_BUFF_LOCKED_BY_t;

static void APP_Frame0(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame1(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame2(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame3(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame4(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_Frame5(uint8_t *p_buff, uint16_t len, uint8_t channel);
static void APP_SendRespToFrame(uint8_t respType, uint8_t nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel, bool cache);

void (*cb_APP_FrameType[])(uint8_t *, uint16_t, uint8_t) = 
{
    APP_Frame0,
    APP_Frame1,
    APP_Frame2,
    APP_Frame3,
    APP_Frame4,
    APP_Frame5
};

void (*cb_APP_Send[])(uint8_t *, uint16_t) = 
{
    UART_Write,
    MQTT_Write,
};

uint8_t APP_Buff[4130] = {0};
uint16_t APP_BuffRxIndex;
uint16_t APP_BuffTxIndex;
uint16_t APP_DataLen;
bool APP_ProcDataBusyFlag;
bool APP_BuffDataRdyFlag;
uint8_t APP_BuffLockedBy;
uint8_t APP_Channel;
uint8_t APP_ISO_State;

uint8_t APP_SPRCOL;

CAN_speed_t APP_CAN_Baud;
uint32_t APP_CAN_TxId;
uint8_t APP_CAN_TxIdType;
uint32_t APP_CAN_FilterId;
uint8_t APP_CAN_FilterIdType;

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
uint32_t APP_RxResp_tmeOutTmr;

uint32_t APP_Frame01_TmeOutTmr;
uint32_t APP_SendToAppWaitTmr;

bool APP_ISO_SendToApp_FF_Flag;
bool APP_SecurityChk;
uint8_t APP_SecuityCode[] = {0x47, 0x56, 0x8A, 0xFE, 0x56, 0x21, 0x4E, 0x23, 0x80, 0x00};

uint32_t APP_AmberLedTmr;
uint32_t APP_YellowLedTmr;
uint32_t APP_GreenLedTmr;
uint8_t APP_YellowFlashCntr;
uint8_t APP_GreenFlashCntr;
bool APP_CAN_COMM_Flag;
bool APP_Client_COMM_Flag;

void APP_Init(void)
{

}

void APP_Task(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    can_message_t rx_frame;
    uint16_t frameCount = 0;

    ESP_LOGI("APP", "Task Started");

    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("APP", "uxHighWaterMark = %d", uxHighWaterMark);


    // CAN_ConfigFilterterMask(0x00000031, true);

    while (1)
    {
        //receive next CAN frame from queue
        if (CAN_ReadFrame(&rx_frame, pdMS_TO_TICKS(5)) == ESP_OK)
        {
            //printf("%d", frameCount++);
            //do stuff!
            if (rx_frame.flags & CAN_MSG_FLAG_EXTD)
            {
                printf("New extended frame");
            }
            else
            {
                printf("New standard frame");
            }

            if (rx_frame.flags & CAN_MSG_FLAG_RTR)
            {
                printf(" RTR from 0x%08x, DLC %d\r\n", rx_frame.identifier, rx_frame.data_length_code);
            }
            else
            {
                printf(" from 0x%08x, DLC %d\n", rx_frame.identifier, rx_frame.data_length_code);
                // convert to upper case and respond to sender
                for (int i = 0; i < 8; i++)
                {
                    if (rx_frame.data[i] >= 'a' && rx_frame.data[i] <= 'z')
                    {
                        rx_frame.data[i] = rx_frame.data[i] - 32;
                    }
                }
            }

            //respond to sender
            CAN_WriteFrame(&rx_frame, pdMS_TO_TICKS(10));
        }
    }
}

#if 1
void APP_ProcessData(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t frameType;
    uint16_t frameLen;
    uint8_t respType;
    uint8_t respNo;
    uint16_t crc16Act;
    uint16_t crc16Calc;
    uint16_t respLen;
    uint8_t respBuff[50];
    
    APP_Client_COMM_Flag = true;
    
    respLen = 0;
    respType = ACK;
    respNo = ACK;
    
    if(APP_ProcDataBusyFlag == false)
    {
        APP_Channel = channel;
        APP_ProcDataBusyFlag = true;
        
        frameType = p_buff[0] >> 4;
        frameLen = ((uint16_t)(p_buff[0] & 0x0F) << 8) | (uint16_t)p_buff[1];
        
        if(frameLen == (len - 2))
        {
            crc16Act = ((uint16_t)p_buff[len - 2] << 8) | (uint16_t)p_buff[len - 1];
            
            crc16Calc = UTIL_CRC16_CCITT(0xFFFF, &p_buff[2], (frameLen - 2));
            
            if(crc16Act == crc16Calc)
            {        
                if(frameType < (sizeof(cb_APP_FrameType) / sizeof(cb_APP_FrameType[0])))
                {
                    if(APP_SecurityChk == true)
                    {
                        if(cb_APP_FrameType[frameType] != NULL)
                        {
                            cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), channel);
                        }
                    }
                    else
                    {
                        if(frameType == 5)
                        {
                            if(cb_APP_FrameType[frameType] != NULL)
                            {
                                cb_APP_FrameType[frameType](&p_buff[2], (frameLen - 2), channel);
                            }
                        }
                        else
                        {
                            respType = NACK;
                            respNo = NACK33;
                        }
                    }
                }
                else
                {
                    respType = NACK;
                    respNo = NACK10;
                }
            }
            else
            {
                respType = NACK;
                respNo = NACK15;
                
                respBuff[respLen++] = crc16Act >> 8;
                respBuff[respLen++] = crc16Act;
                respBuff[respLen++] = crc16Calc >> 8;
                respBuff[respLen++] = crc16Calc;
            }
        }
        else
        {
            respType = NACK;
            // Invalid format or incorrect message length of input
            respNo = NACK13;
        }

        APP_ProcDataBusyFlag = false;
    }
    else
    {
        respType = NACK;
        respNo = NACK14;
    }
    
    if(respType != ACK)
    {
        APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
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

    respType = ACK;
    respNo = ACK;
    respLen = 0;
    
    if((APP_BuffLockedBy == APP_BUFF_LOCKED_BY_NONE) && (APP_BuffDataRdyFlag == false))
    {
        APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME0;
        APP_CAN_TxDataLen = ((uint16_t)p_buff[0] << 8) | (uint16_t)p_buff[1];
        APP_BuffRxIndex = 0;
        
        if(APP_CAN_TxDataLen > 4095)
        {
            respType = NACK;
            respNo = NACK13;
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
        }
        else
        {
            memcpy(&APP_Buff[APP_BuffRxIndex], &p_buff[2], (len - 2));
            APP_BuffRxIndex += (len - 2);
            APP_Frame01_TmeOutTmr = xTaskGetTickCount() + 10000;
        }
    }
    else
    {
        respType = NACK;
        respNo = NACK14;
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

    respType = ACK;
    respNo = ACK;
    respLen = 0;
    
    if((APP_BuffLockedBy == APP_BUFF_LOCKED_BY_FRAME0) && (APP_BuffDataRdyFlag == false))
    {
        if(APP_CAN_TxDataLen && ((APP_BuffRxIndex + len) <= 4095))
        {
            APP_Frame01_TmeOutTmr = xTaskGetTickCount() + 10000;
            memcpy(&APP_Buff[APP_BuffRxIndex], p_buff, len);
            APP_BuffRxIndex += len;
        
            if(APP_BuffRxIndex >= APP_CAN_TxDataLen)
            {
                APP_BuffRxIndex = 0;
                APP_BuffTxIndex = 0;
                APP_BuffDataRdyFlag = true;
                APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME1;
                APP_Frame01_TmeOutTmr = 0;

                if(APP_CAN_Protocol == APP_CAN_PROTOCOL_ISO15765)
                {
                    if(APP_CAN_TxDataLen < 8)
                    {
                        APP_ISO_State = APP_ISO_STATE_SINGLE;
                    }
                    else
                    {
                        APP_ISO_State = APP_ISO_STATE_FIRST;
                    }
                }
                else if(APP_CAN_Protocol == APP_CAN_PROTOCOL_NORMAL)
                {
                    APP_CAN_TxMinTmr = xTaskGetTickCount() + APP_CAN_TxMinTime;
                }
                
                APP_RxResp_tmeOutTmr = xTaskGetTickCount() + APP_CAN_RqRspMaxTime;
            }
        }
        else
        {
            respType = NACK;
            respNo = NACK13;
            APP_BuffRxIndex = 0;
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
        }
    }
    else
    {
        respType = NACK;
        respNo = NACK14;
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
    APP_Channel = channel;
    respType = ACK;
    respNo = ACK;
    respLen = 0;
    offset = 0;
    
    if(len)
    {
        switch(p_buff[0])
        {
            case RSTVC:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_Baud = CAN_SPEED_1000KBPS;
                APP_CAN_Protocol = APP_CAN_PROTOCOL_NONE;
                // Reset or disable CAN
                CAN_DeInit();
                break;

            case SPRCOL:
                if(len != 2)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_SPRCOL = p_buff[1];

                if(p_buff[1] < 6)
                {
                    offset = 0;
                    APP_CAN_Protocol = APP_CAN_PROTOCOL_ISO15765;
                }
                else if(p_buff[1] < 0x0C)
                {
                    offset = 6;
                    APP_CAN_Protocol = APP_CAN_PROTOCOL_NORMAL;
                }
                else if(p_buff[1] < 0x12)
                {
                    offset = 0x0C;
                    APP_CAN_Protocol = APP_CAN_PROTOCOL_OE_IVN;
                    // Accept all messages from any CAN ID
                    canFilterMask = false;
                }
                else
                {
                    respType = NACK;
                    respNo = NACK12;
                }

                if((p_buff[1] - offset) < 2)
                {
                    APP_CAN_Baud = CAN_SPEED_250KBPS;
                }
                else if((p_buff[1] - offset) < 4)
                {
                    APP_CAN_Baud = CAN_SPEED_500KBPS;
                }
                else if((p_buff[1] - offset) < 6)
                {
                    respType = NACK;
                    respNo = NACK12;
                }
                else
                {
                    respType = NACK;
                    respNo = NACK12;
                }

                if((p_buff[1] - offset) % 2)
                {
                    APP_CAN_TxIdType = CAN_MSG_FLAG_EXTD;
                }
                else
                {
                    APP_CAN_TxIdType = CAN_MSG_FLAG_NONE;   // Standard frame;
                }

                if(respType == ACK)
                {
                    APP_CAN_RqRspMaxTime = 500;
                    APP_CAN_TxMinTime = 10;
                    APP_ISO_TxSepTmr = 0;
                    APP_ISO_FC_WaitTmr = 0;
                    APP_Frame01_TmeOutTmr = 0;
                    APP_RxResp_tmeOutTmr = 0;
                    APP_BuffTxIndex = 0;
                    APP_BuffRxIndex = 0;
                    APP_CAN_TxDataLen = 0;
                    APP_CAN_RxDataLen = 0;
                    APP_BuffDataRdyFlag = false;
                    APP_BuffLockedBy = APP_BUFF_LOCKED_BY_NONE;
                    CAN_SetBaud(APP_CAN_Baud);
                    CAN_ConfigFilterterMask(0xFFFFFFFF, true);
                }
                break;

            case GPRCOL:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                respBuff[0] = APP_SPRCOL;
                respLen = 1;
                break;

            case STXHDR:
                if(len == 3)
                {
                    APP_CAN_TxIdType = CAN_MSG_FLAG_NONE;   // Standard frame
                    APP_CAN_TxId = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
                }
                else if(len == 5)
                {
                    APP_CAN_TxIdType = CAN_MSG_FLAG_EXTD;
                    APP_CAN_TxId = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
                }
                else
                {
                    respType = NACK;
                    respNo = NACK13;
                }
                break;

            case GTXHDR:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                // Standard frame
                if(APP_CAN_TxIdType == CAN_MSG_FLAG_NONE)   
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

            case SRXHDRMSK:
                if(len == 3)
                {
                    APP_CAN_FilterIdType = CAN_MSG_FLAG_NONE;   // Standard frame
                    APP_CAN_FilterId = ((uint32_t)p_buff[1] << 8) | (uint32_t)p_buff[2];
                }
                else if(len == 5)
                {
                    APP_CAN_FilterIdType = CAN_MSG_FLAG_EXTD;
                    APP_CAN_FilterId = ((uint32_t)p_buff[1] << 24) | ((uint32_t)p_buff[2] << 16) | ((uint32_t)p_buff[3] << 8) | (uint32_t)p_buff[4];
                }
                else
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }

                CAN_ConfigFilterterMask(APP_CAN_FilterId, (bool)APP_CAN_FilterIdType);
                break;

            case GRXHDRMSK:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                // Standard frame
                if(APP_CAN_FilterIdType == CAN_MSG_FLAG_NONE)
                {
                    respBuff[0] = (uint8_t)(APP_CAN_FilterId >> 8);
                    respBuff[1] = (uint8_t)APP_CAN_FilterId;
                    respLen = 2;
                }
                else
                {
                    respBuff[0] = (uint8_t)(APP_CAN_FilterId >> 24);
                    respBuff[1] = (uint8_t)(APP_CAN_FilterId >> 16);
                    respBuff[2] = (uint8_t)(APP_CAN_FilterId >> 8);
                    respBuff[3] = (uint8_t)APP_CAN_FilterId;
                    respLen = 4;
                }
                break;

            case SFCBLKL:
                if(len != 2)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_ISO_FC_RxBlockSize = p_buff[1];
                break;

            case GFCBLKL:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                respBuff[0] = APP_ISO_FC_RxBlockSize;
                respLen = 1;
                break;    

            case SFCST:
                if(len != 2)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_ISO_FC_RxSepTime = p_buff[1];
                break;

            case GFCST:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                respBuff[0] = APP_ISO_FC_RxSepTime;
                respLen = 1;
                break;

            case SETP1MIN:
                if(len != 2)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_TxMinTime = p_buff[1];
                break;

            case GETP1MIN:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                respBuff[0] = APP_CAN_TxMinTime;
                respLen = 1;
                break;

            case SETP2MAX:
                if(len != 3)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_RqRspMaxTime = ((uint16_t)p_buff[1] << 8) | (uint16_t)p_buff[2];
                break;

            case GETP2MAX:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                respBuff[0] = APP_CAN_RqRspMaxTime >> 8;
                respBuff[1] = APP_CAN_RqRspMaxTime;
                respLen = 2;
                break;

            case TXTP:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_TransmitTstrMsg = true;
                break;

            case STPTXTP:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_TransmitTstrMsg = false;
                break;

            case TXPAD:
                if(len != 2)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_PaddingByte = 0x0100 | (uint16_t)p_buff[1];
                break;

            case STPTXPAD:
                if(len != 1)
                {
                    respType = NACK;
                    respNo = NACK13;
                    break;
                }
                
                APP_CAN_PaddingByte = 0x0000;
                break;

            case GETFWVER: 
                if(len != 1) 
                { 
                    respType = NACK; 
                    respNo = NACK13; 
                    break; 
                } 
                 
                respBuff[0] = 0; 
                respBuff[1] = 0; 
                respBuff[2] = 1; 
                respLen = 3; 
                break;
                
            default:
                respType = NACK;
                respNo = NACK10;
                break;
        }
    }
    else
    {
        respType = NACK;
        respNo = NACK13;
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

    respType = ACK;
    respNo = ACK;
    respLen = 0;
    
    if((APP_BuffLockedBy == APP_BUFF_LOCKED_BY_NONE) && (APP_BuffDataRdyFlag == false))
    {
        if(len <= 1000)
        {
            memcpy(APP_Buff, p_buff, len);
            APP_CAN_TxDataLen = len;
        
            if(APP_CAN_Protocol == APP_CAN_PROTOCOL_ISO15765)
            {
                APP_ISO_State = APP_ISO_STATE_FIRST;
          
                if(APP_CAN_TxDataLen < 8)
                {
                    APP_ISO_State = APP_ISO_STATE_SINGLE;
                }
            }
            else if(APP_CAN_Protocol == APP_CAN_PROTOCOL_NORMAL)
            {
                APP_CAN_TxMinTmr = 1;
            }
            
            APP_BuffRxIndex = 0;
            APP_BuffTxIndex = 0;
            APP_BuffDataRdyFlag = true;
            APP_BuffLockedBy = APP_BUFF_LOCKED_BY_FRAME4;
            APP_RxResp_tmeOutTmr = xTaskGetTickCount() + APP_CAN_RqRspMaxTime;
        }
        else
        {
            respType = NACK;
            respNo = NACK13;
            APP_BuffRxIndex = 0;
        }
    }
    else
    {
        respType = NACK;
        respNo = NACK14;
        APP_BuffRxIndex = 0;
    }
    
    APP_SendRespToFrame(respType, respNo, respBuff, respLen, channel, false);
}

void APP_Frame5(uint8_t *p_buff, uint16_t len, uint8_t channel)
{
    uint8_t respType;
    uint8_t respNo;
    
    respType = ACK;
    respNo = ACK;
    
    if(len != sizeof(APP_SecuityCode))
    {
        APP_SecurityChk = false;
    }
    else
    {
        if(memcmp(p_buff, APP_SecuityCode, sizeof(APP_SecuityCode)) == 0)
        {
            APP_SecurityChk = true;
        }
        else
        {
            APP_SecurityChk = false;
        }
    }
    
    if(APP_SecurityChk == false)
    {
        respType = NACK;
        respNo = NACK35;
    }
    
    APP_SendRespToFrame(respType, respNo, NULL, 0, channel, false);
}

void APP_SendRespToFrame(uint8_t respType, uint8_t nackNo, uint8_t *p_buff, uint16_t dataLen, uint8_t channel, bool cache)
{
    uint8_t len;
    uint8_t buff[30];
    uint16_t crc16;
    
    len = 0;
    buff[len++] = 0x20;
    
    if(respType)
    {    
        buff[len++] = 2 + 2;
        buff[len++] = respType;
        buff[len++] = nackNo;
    }
    else
    {
        buff[len++] = 1 + 2;
        buff[len++] = respType;
    }
    
    if((channel < 2) && (cb_APP_Send[channel] != NULL))
    {
        if(dataLen && (p_buff != NULL))
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
#endif
