#ifndef ESP32CAN_H
#define ESP32CAN_H

#include "CAN_config.h"
#include "CAN.h"

class ESP32CAN
{
    public: 
        int CANInit();
		int CANConfigFilter(const CAN_filter_t* p_filter);
        int CANWriteFrame_Task(void);
        int CANStop();
};

extern ESP32CAN ESP32Can;
#endif
