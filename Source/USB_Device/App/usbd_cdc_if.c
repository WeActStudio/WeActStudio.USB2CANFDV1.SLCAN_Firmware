/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usbd_cdc_if.c
 * @version        : v3.0_Cube
 * @brief          : Usb device for Virtual Com Port.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include "slcan.h"
#include "error.h"
#include "system.h"
#include "config.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
uint8_t slcan_str[SLCAN_MTU];
uint8_t slcan_str_index = 0;

typedef struct
{
  uint8_t data[SLCAN_MTU]; // Data buffer
  uint8_t len;
} usb_tx_msg_buf_t;

typedef struct
{
  uint8_t *buf; // Data buffer
  uint32_t len;
} usb_rx_msg_buf_t;

QueueHandle_t usb_tx_Queue;
QueueHandle_t usb_rx_Queue;

osMemoryPoolId_t usb_rx_msg_MemPool;
osMemoryPoolId_t usb_tx_msg_MemPool;

osSemaphoreId_t usb_tx_lock_Semaphore;
osSemaphoreId_t usb_rx_full_lock_Semaphore;

osThreadId_t usbtxTaskHandle;
const osThreadAttr_t usbtxTask_attributes = {
    .name = "usbtxTask",
    .priority = (osPriority_t)osPriorityAboveNormal,
    .stack_size = 128 * 4};
void usbtxTask(void *argument);

osThreadId_t usbrxTaskHandle;
const osThreadAttr_t usbrxTask_attributes = {
    .name = "usbrxTask",
    .priority = (osPriority_t)osPriorityAboveNormal,
    .stack_size = 128 * 4};
void usbrxTask(void *argument);

static uint8_t usb_rx_response_en;
/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
  * @brief Private variables.
  * @{
  */
/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
void cdc_task_init(void)
{
  usb_rx_response_en = 1;

  usb_tx_Queue = xQueueCreate(16, sizeof(uint32_t));
  usb_rx_Queue = xQueueCreate(16, sizeof(usb_rx_msg_buf_t));

  usb_tx_msg_MemPool = osMemoryPoolNew(16, sizeof(usb_tx_msg_buf_t), NULL);
  usb_rx_msg_MemPool = osMemoryPoolNew(16, CDC_DATA_FS_MAX_PACKET_SIZE, NULL);

  usb_tx_lock_Semaphore = osSemaphoreNew(1U, 1U, NULL);
  usb_rx_full_lock_Semaphore = osSemaphoreNew(1U, 0U, NULL);

  usbtxTaskHandle = osThreadNew(usbtxTask, NULL, &usbtxTask_attributes);
  usbrxTaskHandle = osThreadNew(usbrxTask, NULL, &usbrxTask_attributes);

  slcan_init();
}
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  slcan_str_index = 0;

  // USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_linbuf, 0);

  uint8_t *usb_rx_buf;
  usb_rx_buf = (uint8_t *)osMemoryPoolAlloc(usb_rx_msg_MemPool, 0U);

  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_rx_buf);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[hUsbDeviceFS.classId];
  osMemoryPoolFree(usb_rx_msg_MemPool, hcdc->RxBuffer);
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */

  switch (cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

  case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

  case CDC_SET_COMM_FEATURE:

    break;

  case CDC_GET_COMM_FEATURE:

    break;

  case CDC_CLEAR_COMM_FEATURE:

    break;

    /*******************************************************************************/
    /* Line Coding Structure                                                       */
    /*-----------------------------------------------------------------------------*/
    /* Offset | Field       | Size | Value  | Description                          */
    /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
    /* 4      | bCharFormat |   1  | Number | Stop bits                            */
    /*                                        0 - 1 Stop bit                       */
    /*                                        1 - 1.5 Stop bits                    */
    /*                                        2 - 2 Stop bits                      */
    /* 5      | bParityType |  1   | Number | Parity                               */
    /*                                        0 - None                             */
    /*                                        1 - Odd                              */
    /*                                        2 - Even                             */
    /*                                        3 - Mark                             */
    /*                                        4 - Space                            */
    /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
    /*******************************************************************************/
  case CDC_SET_LINE_CODING:

    break;

  case CDC_GET_LINE_CODING:

    break;

  case CDC_SET_CONTROL_LINE_STATE:

    break;

  case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  usb_rx_msg_buf_t usb_rx_msg_buf;
  uint8_t *usb_rx_buf;

  usb_rx_msg_buf.buf = Buf;
  usb_rx_msg_buf.len = Len[0];
  if (__get_IPSR() != 0U)
    xQueueSendToBackFromISR(usb_rx_Queue, &usb_rx_msg_buf, NULL);
  else
    xQueueSendToBack(usb_rx_Queue, &usb_rx_msg_buf, 0);

  if ((osMemoryPoolGetSpace(usb_rx_msg_MemPool) > 0))
  {
    usb_rx_buf = (uint8_t *)osMemoryPoolAlloc(usb_rx_msg_MemPool, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_rx_buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  }
  else
  {
    osSemaphoreRelease(usb_rx_full_lock_Semaphore);
  }

  return (USBD_OK);
  
  /* USER CODE END 6 */
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *         @note
  *
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0)
  {
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  while (hcdc->TxState)
    ;
  /* USER CODE END 7 */
  return result;
}

/**
  * @brief  CDC_TransmitCplt_FS
  *         Data transmitted callback
  *
  *         @note
  *         This function is IN transfer complete callback used to inform user that
  *         the submitted Data is successfully sent over USB.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 13 */
  UNUSED(epnum);

  //osMemoryPoolFree(usb_tx_msg_MemPool, Buf);
  osSemaphoreRelease(usb_tx_lock_Semaphore);
  /* USER CODE END 13 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
#if 0
void usbtxTask(void *argument)
{
	osDelay(2000);
	#include "can.h"
	can_set_bitrate(8);
	can_set_data_bitrate(5);
	can_enable();
	
	can_tx_msg_t test;
	test.data[0] = 0xaa;
	test.header.DataLength = 8;
	test.header.BitRateSwitch = FDCAN_BRS_ON;
	test.header.FDFormat = FDCAN_FD_CAN;
	test.header.Identifier = 0x00;
	test.header.IdType = FDCAN_STANDARD_ID;
	
	test.header.TxFrameType = FDCAN_DATA_FRAME;
	test.header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	test.header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	test.header.MessageMarker = 0;
  for (;;)
  {
		can_tx(&test);
		can_tx(&test);
		can_tx(&test);
		can_tx(&test);
//		can_tx(&test);
//		can_tx(&test);
		osDelay(1);
  }
}
#else
void usbtxTask(void *argument)
{
  usb_tx_msg_buf_t *usb_tx_msg_buf;
	usb_tx_msg_buf_t *usb_tx_msg_buf1;
  osStatus_t status;
	
//	uint8_t tx_buf[SLCAN_MTU*10];
	uint8_t *tx_buf = pvPortMalloc(SLCAN_MTU*10);

	uint32_t tx_len;

  for (;;)
  {
    status = osSemaphoreAcquire(usb_tx_lock_Semaphore, 100);
    if (status == osOK)
    {
      if (xQueueReceive(usb_tx_Queue, &usb_tx_msg_buf, osWaitForever) == pdPASS)
      {
        memcpy(tx_buf,usb_tx_msg_buf->data,usb_tx_msg_buf->len);
        tx_len = usb_tx_msg_buf->len;
        osMemoryPoolFree(usb_tx_msg_MemPool, usb_tx_msg_buf);

        while (xQueueReceive(usb_tx_Queue, &usb_tx_msg_buf1, 0) == pdPASS)
        {
          memcpy(&tx_buf[tx_len],usb_tx_msg_buf1->data,usb_tx_msg_buf1->len);
          tx_len += usb_tx_msg_buf1->len;
          osMemoryPoolFree(usb_tx_msg_MemPool, usb_tx_msg_buf1);

          if (tx_len > SLCAN_MTU * 9)
          {
            break;
          }
        }
        
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_buf, tx_len);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
      }
      else
      {
        osSemaphoreRelease(usb_tx_lock_Semaphore);
      }
    }
    else if (status == osErrorTimeout)
    {
//      slcan_init();
//      USBD_Stop(&hUsbDeviceFS);

      xQueueReset(usb_tx_Queue);
      osMemoryPoolDelete(usb_tx_msg_MemPool);
      usb_tx_msg_MemPool = osMemoryPoolNew(16, sizeof(usb_tx_msg_buf_t), NULL);

//      USBD_Start(&hUsbDeviceFS);

      osSemaphoreRelease(usb_tx_lock_Semaphore);
    }
  }
}
#endif

void usbrxTask(void *argument)
{
  usb_rx_msg_buf_t usb_rx_msg;
  int32_t result = 0;
  slcan_str_index = 0;
  osStatus_t status;
  for (;;)
  {
    if (xQueueReceive(usb_rx_Queue, &usb_rx_msg, 10) == pdPASS)
    {
      //  Process one whole buffer
      for (uint32_t i = 0; i < usb_rx_msg.len; i++)
      {
        if (usb_rx_msg.buf[i] == '\r')
        {
          result = slcan_parse_str(slcan_str, slcan_str_index);
          if (usb_get_rx_response_en())
          {
            if (result == SLCAN_OK)
            {
              cdc_transmit(SLCAN_RET_OK, SLCAN_RET_LEN);
            }
            else if (result == SLCAN_ERROR)
            {
              cdc_transmit(SLCAN_RET_ERR, SLCAN_RET_LEN);
            }
          }
          slcan_str_index = 0;

          //          break;
        }
        else
        {
          // Check for overflow of buffer
          if (slcan_str_index >= SLCAN_MTU)
          {
            // TODO: Return here and discard this CDC buffer?
            slcan_str_index = 0;
          }

          slcan_str[slcan_str_index] = usb_rx_msg.buf[i];
          slcan_str_index++;
        }
      }
      osMemoryPoolFree(usb_rx_msg_MemPool, usb_rx_msg.buf);

      status = osSemaphoreAcquire(usb_rx_full_lock_Semaphore, 0);
      if (status == osOK)
      {
        uint8_t *usb_rx_buf;
        usb_rx_buf = (uint8_t *)osMemoryPoolAlloc(usb_rx_msg_MemPool, 0U);
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_rx_buf);
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);
      }
    }
    else
    {
      slcan_str_index = 0;
    }
  }
}

uint32_t usb_tx_Queue_space;
uint32_t usb_rx_Queue_space;
uint32_t usb_tx_msg_MemPool_space;
uint32_t usb_rx_msg_MemPool_space;
uint32_t usbrxTask_space;
uint32_t usbtxTask_space;
// Process incoming and outgoing USB-CDC data
void cdc_task_monitor(void)
{
  usb_tx_Queue_space = uxQueueSpacesAvailable(usb_tx_Queue);
  usb_rx_Queue_space = uxQueueSpacesAvailable(usb_rx_Queue);
  usb_tx_msg_MemPool_space = osMemoryPoolGetSpace(usb_tx_msg_MemPool);
  usb_rx_msg_MemPool_space = osMemoryPoolGetSpace(usb_rx_msg_MemPool);
  usbrxTask_space = osThreadGetStackSpace(usbrxTaskHandle);
  usbtxTask_space = osThreadGetStackSpace(usbtxTaskHandle);
}

// Enqueue data for transmission over USB CDC to host
void cdc_transmit(uint8_t *buf, uint16_t len)
{
  if (osMemoryPoolGetSpace(usb_tx_msg_MemPool) != 0)
  {
    usb_tx_msg_buf_t *usb_tx_msg_buf;
    usb_tx_msg_buf = (usb_tx_msg_buf_t *)osMemoryPoolAlloc(usb_tx_msg_MemPool, 0U);

    if (usb_tx_msg_buf != NULL)
    {
      memcpy(usb_tx_msg_buf->data, buf, len);
      usb_tx_msg_buf->len = len;

      if (__get_IPSR() != 0U)
        xQueueSendToBackFromISR(usb_tx_Queue, &usb_tx_msg_buf, NULL);
      else
        xQueueSendToBack(usb_tx_Queue, &usb_tx_msg_buf, 0);
    }
    else
    {
      error_assert(ERR_FULLBUF_USBTX);
      return;
    }
  }
  else
  {
    error_assert(ERR_FULLBUF_USBTX);
    return;
  }
}

void usb_set_rx_response_en(uint8_t en)
{
  usb_rx_response_en = en;
}

uint8_t usb_get_rx_response_en(void)
{
  return usb_rx_response_en;
}
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */

