#ifndef __ERROR_H_
#define __ERROR_H_

#include "main.h"

// Error flags, value is bit position in the error register
typedef enum _error_t
{
	ERR_PERIPHINIT = 0,
	ERR_USBTX_BUSY,
	ERR_CAN_TXFAIL,
	ERR_CANRXFIFO_OVERFLOW,
	ERR_FULLBUF_CANTX,
	ERR_FULLBUF_USBRX,
	ERR_FULLBUF_USBTX,
	ERR_NULLBUF_USBTX,
	ERR_MAX
} error_t;


// Prototypes
void error_assert(error_t err);
uint32_t error_timestamp(error_t err);
uint32_t error_last_timestamp(void);
uint8_t error_occurred(error_t err);
uint32_t error_reg(void);


#endif /* INC_ERROR_H_ */
