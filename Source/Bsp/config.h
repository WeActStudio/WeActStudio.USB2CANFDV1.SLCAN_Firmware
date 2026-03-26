#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <string.h>

#define LED_STATE_BLINK_500MS (0x01)
#define LED_STATE_BLINK_200MS (0x02)
#define LED_STATE_BLINK_100MS (0x04)
#define LED_STATE_BLINK_50MS (0x08)
#define LED_STATE_ON (0x10)
#define LED_STATE_OFF (0x20)

#define struct_size(ptr, field, num) \
	(offsetof(__typeof(*(ptr)), field) + sizeof((ptr)->field[0]) * (num))
    
extern osThreadId_t ledrdyTaskHandle;
#ifdef __cplusplus
}
#endif

#endif
