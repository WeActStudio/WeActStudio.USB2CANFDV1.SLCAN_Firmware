//
// system: initialize system clocks and other core peripherals
//

#include "system.h"

// Disable all interrupts
void system_irq_disable(void)
{
	__disable_irq();
	__DSB();
	__ISB();
}

// Enable all interrupts
void system_irq_enable(void)
{
	__enable_irq();
}

// Convert a 32-bit value to an ascii hex value
void system_hex32(char *out, uint32_t val)
{
	char *p = out + 8;
	*p-- = 0;
	while (p >= out) {
		uint8_t nybble = val & 0x0F;
		if (nybble < 10)
			*p = '0' + nybble;
		else
			*p = 'A' + nybble - 10;
		val >>= 4;
		p--;
	}
} 
