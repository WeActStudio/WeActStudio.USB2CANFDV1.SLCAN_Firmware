#ifndef _SYSTEM_H
#define _SYSTEM_H

#include "main.h"

void system_irq_enable(void);
void system_irq_disable(void);
void system_hex32(char *out, uint32_t val);

#endif
