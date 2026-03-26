#ifndef __BOARD_H
#define __BOARD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

#define LEDTX (0)
#define LEDRX (1)
#define LEDRDY (2)

#ifndef LEDTX_Pin
#define LEDTX_Pin GPIO_PIN_1
#endif
#ifndef LEDTX_GPIO_Port
#define LEDTX_GPIO_Port GPIOA
#endif

#ifndef LEDRX_Pin
#define LEDRX_Pin GPIO_PIN_0
#endif
#ifndef LEDRX_GPIO_Port
#define LEDRX_GPIO_Port GPIOA
#endif

#ifndef LEDRDY_Pin
#define LEDRDY_Pin GPIO_PIN_2
#endif
#ifndef LEDRDY_GPIO_Port
#define LEDRDY_GPIO_Port GPIOA
#endif

    void board_led_init(void);

#define BOARD_LEDTX_TOGGLE() HAL_GPIO_TogglePin(LEDTX_GPIO_Port,LEDTX_Pin)
#define BOARD_LEDTX_ON() HAL_GPIO_WritePin(LEDTX_GPIO_Port,LEDTX_Pin,GPIO_PIN_RESET)
#define BOARD_LEDTX_OFF() HAL_GPIO_WritePin(LEDTX_GPIO_Port,LEDTX_Pin,GPIO_PIN_SET)

#define BOARD_LEDRX_TOGGLE() HAL_GPIO_TogglePin(LEDRX_GPIO_Port,LEDRX_Pin)
#define BOARD_LEDRX_ON() HAL_GPIO_WritePin(LEDRX_GPIO_Port,LEDRX_Pin,GPIO_PIN_RESET)
#define BOARD_LEDRX_OFF() HAL_GPIO_WritePin(LEDRX_GPIO_Port,LEDRX_Pin,GPIO_PIN_SET)

#define BOARD_LEDRDY_TOGGLE() HAL_GPIO_TogglePin(LEDRDY_GPIO_Port,LEDRDY_Pin)
#define BOARD_LEDRDY_ON() HAL_GPIO_WritePin(LEDRDY_GPIO_Port,LEDRDY_Pin,GPIO_PIN_RESET)
#define BOARD_LEDRDY_OFF() HAL_GPIO_WritePin(LEDRDY_GPIO_Port,LEDRDY_Pin,GPIO_PIN_SET)


#ifdef __cplusplus
}
#endif

#endif
