/*---------------------------------------
- WeAct Studio Official Link
- taobao: weactstudio.taobao.com
- aliexpress: weactstudio.aliexpress.com
- github: github.com/WeActStudio
- gitee: gitee.com/WeAct-TC
- blog: www.weact-tc.cn
---------------------------------------*/

#include "board.h"

void board_led_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LEDTX_GPIO_Port, LEDTX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LEDRX_GPIO_Port, LEDRX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LEDRDY_GPIO_Port, LEDRDY_Pin, GPIO_PIN_SET);
  
  /*Configure GPIO pin : LEDTX_Pin */
  GPIO_InitStruct.Pin = LEDTX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LEDTX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LEDRX_Pin */
  GPIO_InitStruct.Pin = LEDRX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LEDRX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LEDRDY_Pin */
  GPIO_InitStruct.Pin = LEDRDY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LEDRDY_GPIO_Port, &GPIO_InitStruct);

}
