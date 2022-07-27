#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

uint16_t UTIL_CRC16_CCITT(uint16_t initVal, uint8_t* p_inBuff, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif