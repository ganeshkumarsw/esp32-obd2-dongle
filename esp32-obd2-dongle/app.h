#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define IsTimerElapsed(x) ((x > 0) && (x <= (xTaskGetTickCount() / portTICK_PERIOD_MS)))
#define IsTimerRunning(x) ((x > 0) && (x > (xTaskGetTickCount() / portTICK_PERIOD_MS)))
#define IsTimerEnabled(x) ((x) > 0)
#define StartTimer(x, y)                                    \
    {                                                       \
        x = (xTaskGetTickCount() / portTICK_PERIOD_MS) + y; \
    }
#define ResetTimer(x, y)                                    \
    {                                                       \
        x = (xTaskGetTickCount() / portTICK_PERIOD_MS) + y; \
    }
#define StopTimer(x) \
    {                \
        x = 0;       \
    }

#define APP_ISO_FC_WAIT_TIME 10000

    typedef enum
    {
        APP_REQ_CMD_RESET = 1,
        APP_REQ_CMD_SET_PROTOCOL = 2,
        APP_REQ_CMD_GET_PROTOCOL = 3,
        APP_REQ_CMD_SET_TX_CAN_ID = 4,
        APP_REQ_CMD_GET_TX_CAN_ID = 5,
        APP_REQ_CMD_SET_RX_CAN_ID = 6,
        APP_REQ_CMD_GET_RX_CAN_ID = 7,
        APP_REQ_CMD_SFCBLKL = 8,
        APP_REQ_CMD_GFCBLKL = 9,
        APP_REQ_CMD_SFCST = 10,
        APP_REQ_CMD_GFCST = 11,
        APP_REQ_CMD_SETP1MIN = 12,
        APP_REQ_CMD_GETP1MIN = 13,
        APP_REQ_CMD_SETP2MAX = 14,
        APP_REQ_CMD_GETP2MAX = 15,
        APP_REQ_CMD_TXTP = 16,
        APP_REQ_CMD_STPTXTP = 17,
        APP_REQ_CMD_TXPAD = 18,
        APP_REQ_CMD_STPTXPAD = 19,
        APP_REQ_CMD_GET_FIRMWARE_VER = 20,
        APP_REQ_CMD_GETSFR = 21,
        APP_REQ_CMD_SET_STA_SSID = 22,
        APP_REQ_CMD_SET_STA_PASSWORD = 23,
    } APP_REQ_CMD_t;

    typedef enum
    {
        APP_RESP_ACK = 0x00,     // Positive Response
        APP_RESP_NACK = 0x01,    // Negative Response
        APP_RESP_NACK_10 = 0x10, // Command Not Supported
        APP_RESP_NACK_12 = 0x12, // Input Not supported
        APP_RESP_NACK_13 = 0x13, // Invalid format or incorrect message length of input
        APP_RESP_NACK_14 = 0x14, // Invalid operation
        APP_RESP_NACK_15 = 0x15, // CRC failure
        APP_RESP_NACK_16 = 0x16, // Protocol not set
        APP_RESP_NACK_22 = 0x22, // Conditions not correct
        APP_RESP_NACK_31 = 0x31, // Request out of range
        APP_RESP_NACK_33 = 0x33, // security access denied
        APP_RESP_NACK_78 = 0x78, // response pending
        APP_RESP_NACK_24 = 0x24, // request sequence error
        APP_RESP_NACK_35 = 0x35, // Invalid Key
        APP_RESP_NACK_36 = 0x36, // exceeded number of attempts
        APP_RESP_NACK_37 = 0x37, // required time delay not expired
        APP_RESP_NACK_72 = 0x72, // General programming failure
        APP_RESP_NACK_7E = 0x7E, // sub fn not supported in this diag session
    } APP_RESP_t;

    typedef enum
    {
        APP_CAN_PROTOCOL_NONE = 0,
        APP_CAN_PROTOCOL_ISO15765,
        APP_CAN_PROTOCOL_NORMAL,
        APP_CAN_PROTOCOL_OE_IVN,
    } CAN_PROTOCOL_t;

    typedef enum
    {
        APP_STATE_CAN_ISO_SINGLE = 0,
        APP_STATE_CAN_ISO_FIRST,
        APP_STATE_CAN_ISO_CONSECUTIVE,
        APP_STATE_CAN_ISO_SEP_TIME,
        APP_STATE_CAN_ISO_FC_WAIT_TIME,
        APP_STATE_SEND_TO_APP,
        APP_STATE_IDLE,
    } APP_STATE_t;

    typedef enum
    {
        APP_CAN_ISO_TYPE_SINGLE = 0,
        APP_CAN_ISO_TYPE_FIRST,
        APP_CAN_ISO_TYPE_CONSECUTIVE,
        APP_CAN_ISO_TYPE_FLOWCONTROL,
    } APP_CAN_ISO_TYPE_t;

    // Flow control frame transfer allowed
    typedef enum
    {
        APP_CAN_ISO_FC_TM_CONT = 0,
        APP_CAN_ISO_FC_TM_WAIT,
        APP_CAN_ISO_FC_TM_ABORT,
    } APP_CAN_ISO_FC_TM_t;

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
        APP_MSG_CHANNEL_NONE = -1,
        APP_MSG_CHANNEL_UART = 0,
        APP_MSG_CHANNEL_MQTT,
        APP_MSG_CHANNEL_BLE,
        APP_MSG_CHANNEL_TCP_SOC,
        APP_MSG_CHANNEL_WEB_SOC,
        APP_MSG_CHANNEL_MAX,
    } APP_CHANNEL_t;

    void APP_Init(void);
    void APP_Task(void *pvParameters);
    void APP_ProcessData(uint8_t *p_buff, uint16_t len, APP_CHANNEL_t channel);

#ifdef __cplusplus
}
#endif

#endif