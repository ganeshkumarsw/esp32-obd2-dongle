#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#define APP_ISO_FC_WAIT_TIME 10000

#define RSTVC 1
#define SPRCOL 2
#define GPRCOL 3
#define STXHDR 4
#define GTXHDR 5
#define SRXHDRMSK 6
#define GRXHDRMSK 7
#define SFCBLKL 8
#define GFCBLKL 9
#define SFCST 10
#define GFCST 11
#define SETP1MIN 12
#define GETP1MIN 13
#define SETP2MAX 14
#define GETP2MAX 15
#define TXTP 16
#define STPTXTP 17
#define TXPAD 18
#define STPTXPAD 19
#define GETFWVER 20

//typedef void (*cb_APP_FrameType)(uint8_t *, uint16_t);

#define ACK 0       // Positive Response
#define NACK 1      // Negative Response
#define NACK10 0x10 // Command Not Supported
#define NACK12 0x12 // Input Not supported
#define NACK13 0x13 // Invalid format or incorrect message length of input
#define NACK14 0x14 // Invalid operation
#define NACK15 0x15 // CRC failure
#define NACK22 0x22 // Conditions not correct
#define NACK31 0x31 // Request out of range
#define NACK33 0x33 // security access denied
#define NACK78 0x78 // response pending
#define NACK24 0x24 // request sequence error
#define NACK35 0x35 // Invalid Key
#define NACK36 0x36 // exceeded number of attempts
#define NACK37 0x37 // required time delay not expired
#define NACK72 0x72 // General programming failure
#define NACK7E 0x7E // sub fn not supported in this diag session

typedef enum
{
    APP_CAN_PROTOCOL_NONE = 0,
    APP_CAN_PROTOCOL_ISO15765,
    APP_CAN_PROTOCOL_NORMAL,
    APP_CAN_PROTOCOL_OE_IVN,
} CAN_PROTOCOL_t;

typedef enum
{
    APP_ISO_STATE_SINGLE = 0,
    APP_ISO_STATE_FIRST,
    APP_ISO_STATE_CONSECUTIVE,
    APP_ISO_STATE_SEP_TIME,
    APP_ISO_STATE_FC_WAIT_TIME,
    APP_ISO_STATE_SEND_TO_APP,
    APP_ISO_STATE_IDLE,
} APP_ISO_STATE_t;

typedef enum
{
    APP_ISO_TYPE_SINGLE = 0,
    APP_ISO_TYPE_FIRST,
    APP_ISO_TYPE_CONSECUTIVE,
    APP_ISO_TYPE_FLOWCONTROL,
} APP_ISO_TYPE_t;

typedef enum
{
    APP_BUFF_LOCKED_BY_NONE = 0,
    APP_BUFF_LOCKED_BY_FRAME0,
    APP_BUFF_LOCKED_BY_FRAME1,
    APP_BUFF_LOCKED_BY_FRAME4,
    APP_BUFF_LOCKED_BY_ISO_TP_RX_FF,
    APP_BUFF_LOCKED_BY_ISO_TP_RX_CF,
} APP_BUFF_LOCKED_BY_t;

typedef enum
{
    APP_CHANNEL_NONE = -1,
    APP_CHANNEL_UART = 0,
    APP_CHANNEL_MQTT,
    APP_CHANNEL_BLE,
    APP_CHANNEL_TCP_SOC,
    APP_CHANNEL_WEB_SOC,
    APP_CHANNEL_MAX,
}APP_CHANNEL_t;

void APP_Init(void);
void APP_Task(void *pvParameters);
void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel);

#ifdef __cplusplus
}
#endif

#endif