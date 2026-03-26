//
// can: initializes and provides methods to interact with the CAN peripheral
//

#include "stm32g0xx_hal.h"
#include "slcan.h"
#include "usbd_cdc_if.h"
#include "can.h"
#include "board.h"

#include "error.h"
#include "system.h"

// Private variables
#define can_handle hfdcan1
extern FDCAN_HandleTypeDef can_handle;

// static FDCAN_FilterTypeDef filter;
enum can_bus_state bus_state;
uint32_t can_mode = FDCAN_MODE_NORMAL;
FunctionalState can_autoretransmit = DISABLE;

// Structure for CAN/FD bitrate configuration
typedef struct can_bitrate_cfg_
{
    uint16_t prescaler;
    uint8_t sjw;
    uint8_t time_seg1;
    uint8_t time_seg2;
} can_bitrate_cfg_t;

can_bitrate_cfg_t bitrate_nominal, bitrate_data = {0};

QueueHandle_t can_tx_Queue;
QueueHandle_t can_rx_Queue;

osMemoryPoolId_t can_rx_msg_MemPool;
osMemoryPoolId_t can_tx_msg_MemPool;

osThreadId_t cantxTaskHandle;
const osThreadAttr_t cantxTask_attributes = {
    .name = "cantxTask",
    .priority = (osPriority_t)osPriorityAboveNormal1,
    .stack_size = 128 * 4};
void cantxTask(void *argument);

osThreadId_t canrxTaskHandle;
const osThreadAttr_t canrxTask_attributes = {
    .name = "canrxTask",
    .priority = (osPriority_t)osPriorityAboveNormal1,
    .stack_size = 128 * 4};
void canrxTask(void *argument);

// Initialize CAN peripheral settings, but don't actually start the peripheral
void can_init(void)
{
    can_handle.Instance = FDCAN1;
    // default to 125 kbit/s
    can_set_bitrate(CAN_BITRATE_125K);
    can_set_data_bitrate(CAN_DATA_BITRATE_2M);
    can_set_silent(0);
    bus_state = OFF_BUS;

    can_tx_Queue = xQueueCreate(16, sizeof(uint32_t));
    can_rx_Queue = xQueueCreate(16, sizeof(uint32_t));

    can_tx_msg_MemPool = osMemoryPoolNew(16, sizeof(can_tx_msg_t), NULL);
    can_rx_msg_MemPool = osMemoryPoolNew(16, sizeof(can_rx_msg_t), NULL);

    cantxTaskHandle = osThreadNew(cantxTask, NULL, &cantxTask_attributes);
    canrxTaskHandle = osThreadNew(canrxTask, NULL, &canrxTask_attributes);
}

// Start the CAN peripheral
void can_enable(void)
{
    if (bus_state == OFF_BUS)
    {
        can_handle.Init.ClockDivider = FDCAN_CLOCK_DIV1; // 60Mhz
        can_handle.Init.FrameFormat = FDCAN_FRAME_FD_BRS;

        can_handle.Init.Mode = can_mode;
        can_handle.Init.AutoRetransmission = can_autoretransmit;
        can_handle.Init.TransmitPause = DISABLE;     // emz
        can_handle.Init.ProtocolException = DISABLE; // emz

        can_handle.Init.NominalPrescaler = bitrate_nominal.prescaler;
        can_handle.Init.NominalSyncJumpWidth = bitrate_nominal.sjw;
        can_handle.Init.NominalTimeSeg1 = bitrate_nominal.time_seg1;
        can_handle.Init.NominalTimeSeg2 = bitrate_nominal.time_seg2;

        // FD only
        can_handle.Init.DataPrescaler = bitrate_data.prescaler;
        can_handle.Init.DataSyncJumpWidth = bitrate_data.sjw;
        can_handle.Init.DataTimeSeg1 = bitrate_data.time_seg1;
        can_handle.Init.DataTimeSeg2 = bitrate_data.time_seg2;

        can_handle.Init.StdFiltersNbr = 1;
        can_handle.Init.ExtFiltersNbr = 1;
        can_handle.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

        HAL_FDCAN_Init(&can_handle);

        // This is a must for high data bit rates, especially for isolated transceivers
        HAL_FDCAN_ConfigTxDelayCompensation(
            &can_handle,
            can_handle.Init.DataPrescaler * can_handle.Init.DataTimeSeg1,
            0);
        HAL_FDCAN_EnableTxDelayCompensation(&can_handle);

        FDCAN_FilterTypeDef FDCAN1_RXFilter;
        FDCAN1_RXFilter.IdType = FDCAN_STANDARD_ID;
        FDCAN1_RXFilter.FilterIndex = 0;
        FDCAN1_RXFilter.FilterType = FDCAN_FILTER_RANGE;
        FDCAN1_RXFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
        FDCAN1_RXFilter.FilterID1 = 0;
        FDCAN1_RXFilter.FilterID2 = 0x7ff;
        if (HAL_FDCAN_ConfigFilter(&can_handle, &FDCAN1_RXFilter) != HAL_OK)
        {
            while (1)
                ;
        }

        FDCAN1_RXFilter.IdType = FDCAN_EXTENDED_ID;
        FDCAN1_RXFilter.FilterIndex = 0;
        FDCAN1_RXFilter.FilterType = FDCAN_FILTER_RANGE;
        FDCAN1_RXFilter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
        FDCAN1_RXFilter.FilterID1 = 0;
        FDCAN1_RXFilter.FilterID2 = 0x1FFFFFFF;
        if (HAL_FDCAN_ConfigFilter(&can_handle, &FDCAN1_RXFilter) != HAL_OK)
        {
            while (1)
                ;
        }

        HAL_FDCAN_ConfigGlobalFilter(&can_handle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);

        //		HAL_FDCAN_EnableEdgeFiltering(&can_handle);

        HAL_FDCAN_ActivateNotification(&can_handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

        HAL_FDCAN_ActivateNotification(&can_handle, FDCAN_IT_DATA_PROTOCOL_ERROR | FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_ERROR_WARNING | FDCAN_IT_BUS_OFF, 0);

        HAL_FDCAN_ActivateNotification(&can_handle, FDCAN_IT_TX_COMPLETE, FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2);

        HAL_FDCAN_ActivateNotification(&can_handle, FDCAN_IT_TX_FIFO_EMPTY, 0);

        HAL_FDCAN_Start(&can_handle);

        bus_state = ON_BUS;

        osThreadFlagsSet(ledrdyTaskHandle, LED_STATE_BLINK_200MS);
    }
}

// Disable the CAN peripheral and go off-bus
void can_disable(void)
{
    if (bus_state == ON_BUS)
    {
        HAL_FDCAN_Stop(&can_handle);
        HAL_FDCAN_DeactivateNotification(&can_handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE);
        HAL_FDCAN_DeactivateNotification(&can_handle, FDCAN_IT_DATA_PROTOCOL_ERROR | FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_ERROR_WARNING | FDCAN_IT_BUS_OFF);
        HAL_FDCAN_DeactivateNotification(&can_handle, FDCAN_IT_TX_COMPLETE);
        HAL_FDCAN_DeInit(&can_handle);
        bus_state = OFF_BUS;

        xQueueReset(can_tx_Queue);
        osMemoryPoolDelete(can_tx_msg_MemPool);
        can_tx_msg_MemPool = osMemoryPoolNew(16, sizeof(can_tx_msg_t), NULL);

        xQueueReset(can_rx_Queue);
        osMemoryPoolDelete(can_rx_msg_MemPool);
        can_rx_msg_MemPool = osMemoryPoolNew(16, sizeof(can_rx_msg_t), NULL);

        osThreadFlagsSet(ledrdyTaskHandle, LED_STATE_ON);
    }
}

enum can_bus_state can_is_enable(void)
{
    return bus_state;
}

void can_set_data_bitrate(enum can_data_bitrate bitrate)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return;
    }

    switch (bitrate)
    {
    case CAN_DATA_BITRATE_1M:
        bitrate_data.prescaler = 2;
        bitrate_data.sjw = 6;
        bitrate_data.time_seg1 = 22;
        bitrate_data.time_seg2 = 7;
        break;
    case CAN_DATA_BITRATE_2M:
        bitrate_data.prescaler = 1;
        bitrate_data.sjw = 7;
        bitrate_data.time_seg1 = 21;
        bitrate_data.time_seg2 = 8;
        break;
    case CAN_DATA_BITRATE_3M:
        bitrate_data.prescaler = 1;
        bitrate_data.sjw = 4;
        bitrate_data.time_seg1 = 14;
        bitrate_data.time_seg2 = 5;
        break;
    case CAN_DATA_BITRATE_4M:
        bitrate_data.prescaler = 1;
        bitrate_data.sjw = 3;
        bitrate_data.time_seg1 = 10;
        bitrate_data.time_seg2 = 4;
        break;
    case CAN_DATA_BITRATE_5M:
    default:
        bitrate_data.prescaler = 1;
        bitrate_data.sjw = 2;
        bitrate_data.time_seg1 = 8;
        bitrate_data.time_seg2 = 3;
        break;
    }
}

int32_t can_set_data_bitrate2(uint8_t seg1, uint8_t seg2)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return -1;
    }

    if (seg1 == 0 || seg2 == 0)
    {
        return -1;
    }

    if (seg1 > 32 || seg2 > 16)
    {
        return -1;
    }

    bitrate_data.prescaler = 1;
    bitrate_data.sjw = seg2 > 1 ? seg2 - 1 : 1;
    bitrate_data.time_seg1 = seg1;
    bitrate_data.time_seg2 = seg2;

    return 0;
}

int32_t can_set_data_bitrate3(uint8_t div, uint8_t seg1, uint8_t seg2)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return -1;
    }

    if (div == 0 || seg1 == 0 || seg2 == 0)
    {
        return -1;
    }

    if (div > 32 || seg1 > 32 || seg2 > 16)
    {
        return -1;
    }

    bitrate_data.prescaler = div;
    bitrate_data.sjw = seg2 > 1 ? seg2 - 1 : 1;
    bitrate_data.time_seg1 = seg1;
    bitrate_data.time_seg2 = seg2;

    return 0;
}

// Set the bitrate of the CAN peripheral
void can_set_bitrate(enum can_bitrate bitrate)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return;
    }

    switch (bitrate)
    {
    case CAN_BITRATE_5K:
    bitrate_nominal.prescaler = 50;
    bitrate_nominal.sjw = 29;
    bitrate_nominal.time_seg1 = 209;
    bitrate_nominal.time_seg2 = 30;
    break;
    case CAN_BITRATE_10K:
        bitrate_nominal.prescaler = 25;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_20K:
        bitrate_nominal.prescaler = 25;
        bitrate_nominal.sjw = 14;
        bitrate_nominal.time_seg1 = 104;
        bitrate_nominal.time_seg2 = 15;
        break;
    case CAN_BITRATE_33_3K:
        bitrate_nominal.prescaler = 8;
        bitrate_nominal.sjw = 27;
        bitrate_nominal.time_seg1 = 196;
        bitrate_nominal.time_seg2 = 28;
        break;
    case CAN_BITRATE_50K:
        bitrate_nominal.prescaler = 5;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_62_5K:
        bitrate_nominal.prescaler = 4;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_75K:
        bitrate_nominal.prescaler = 5;
        bitrate_nominal.sjw = 19;
        bitrate_nominal.time_seg1 = 139;
        bitrate_nominal.time_seg2 = 20;
        break;
    case CAN_BITRATE_83_3K:
        bitrate_nominal.prescaler = 3;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_100K:
        bitrate_nominal.prescaler = 5;
        bitrate_nominal.sjw = 14;
        bitrate_nominal.time_seg1 = 104;
        bitrate_nominal.time_seg2 = 15;
        break;
    case CAN_BITRATE_125K:
        bitrate_nominal.prescaler = 2;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_250K:
        bitrate_nominal.prescaler = 1;
        bitrate_nominal.sjw = 29;
        bitrate_nominal.time_seg1 = 209;
        bitrate_nominal.time_seg2 = 30;
        break;
    case CAN_BITRATE_500K:
        bitrate_nominal.prescaler = 1;
        bitrate_nominal.sjw = 14;
        bitrate_nominal.time_seg1 = 104;
        bitrate_nominal.time_seg2 = 15;
        break;
    case CAN_BITRATE_800K:
        bitrate_nominal.prescaler = 1;
        bitrate_nominal.sjw = 9;
        bitrate_nominal.time_seg1 = 65;
        bitrate_nominal.time_seg2 = 9;
        break;
    case CAN_BITRATE_1000K:
        bitrate_nominal.prescaler = 1;
        bitrate_nominal.sjw = 6;
        bitrate_nominal.time_seg1 = 52;
        bitrate_nominal.time_seg2 = 7;
        break;
    default:
        break;
    }
}

int32_t can_set_bitrate2(uint8_t seg1, uint8_t seg2)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return -1;
    }

    if (seg1 < 2 || seg2 < 2)
    {
        return -1;
    }

    if (seg2 > 128)
    {
        return -1;
    }

    bitrate_nominal.prescaler = 2;
    bitrate_nominal.sjw = seg2 > 1 ? seg2 - 1 : 1;
    bitrate_nominal.time_seg1 = seg1;
    bitrate_nominal.time_seg2 = seg2;

    return 0;
}

int32_t can_set_bitrate3(uint8_t div, uint8_t seg1, uint8_t seg2)
{
    if (bus_state == ON_BUS)
    {
        // cannot set bitrate while on bus
        return -1;
    }

    if (div == 0 || seg1 < 2 || seg2 < 2)
    {
        return -1;
    }

    if (seg2 > 128)
    {
        return -1;
    }

    bitrate_nominal.prescaler = div;
    bitrate_nominal.sjw = seg2 > 1 ? seg2 - 1 : 1;
    bitrate_nominal.time_seg1 = seg1;
    bitrate_nominal.time_seg2 = seg2;

    return 0;
}

// Set CAN peripheral to silent mode
void can_set_silent(uint8_t silent)
{
    if (bus_state == ON_BUS)
    {
        // cannot set silent mode while on bus
        return;
    }
    if (silent)
    {
        can_mode = FDCAN_MODE_BUS_MONITORING; // !!!?!?!
    }
    else
    {
        can_mode = FDCAN_MODE_NORMAL;
    }
}

// Set CAN peripheral to autoretransmit mode
void can_set_autoretransmit(uint8_t autoretransmit)
{
    if (bus_state == ON_BUS)
    {
        // Cannot set autoretransmission while on bus
        return;
    }
    if (autoretransmit)
    {
        can_autoretransmit = ENABLE;
    }
    else
    {
        can_autoretransmit = DISABLE;
    }
}

// Send a message on the CAN bus.
int32_t can_tx(can_tx_msg_t *tx_msg)
{
    // If when we increment the head we're going to hit the tail
    // (if we're filling the last spot in the queue)
    if (uxQueueSpacesAvailable(can_tx_Queue) == 0)
    {
        error_assert(ERR_FULLBUF_CANTX);
        //osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
        return SLCAN_ERROR;
    }
    else
    {
        if (__get_IPSR() != 0U)
            xQueueSendToBackFromISR(can_tx_Queue, &tx_msg, NULL);
        else
            xQueueSendToBack(can_tx_Queue, &tx_msg, 0);
    }

    return SLCAN_OK_NOT_RESPONSE;
}

void cantxTask(void *argument)
{
    for (;;)
    {
        can_tx_process();
    }
}

// Process data from CAN tx/rx circular buffers
void can_tx_process(void)
{
    uint32_t status;
    can_tx_msg_t *tx_msg;

    FDCAN_ProtocolStatusTypeDef ProtocolStatus;
    for (;;)
    {
        if (xQueueReceive(can_tx_Queue, &tx_msg, 50) == pdPASS)
        {
            HAL_FDCAN_GetProtocolStatus(&can_handle, &ProtocolStatus);
            if (HAL_FDCAN_GetTxFifoFreeLevel(&can_handle) > 0 && (ProtocolStatus.BusOff == 0))
            {
                // Transmit can frame
                status = HAL_FDCAN_AddMessageToTxFifoQ(&can_handle, &tx_msg->header, tx_msg->data);
                // This drops the packet if it fails (no retry). Failure is unlikely
                // since we check if there is a TX mailbox free.
                if (status != HAL_OK)
                {
                    osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
                    // xQueueSendToFront(can_tx_Queue, &tx_msg, 0);
                }
                else
                {
                    osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
					BOARD_LEDTX_ON();
                }
            }
            else
            {
                // xQueueSendToFront(can_tx_Queue, &tx_msg, 0);
                // osThreadFlagsSet(ledtxTaskHandle, LED_STATE_ON);
                // osDelay(2);
                if (ProtocolStatus.BusOff == 1 || uxQueueSpacesAvailable(can_tx_Queue) == 0)
                {
                    cdc_transmit(SLCAN_RET_ERR, SLCAN_RET_LEN);
                    osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
                }
                else
                {
                    xQueueSendToFront(can_tx_Queue, &tx_msg, 0);
                }
                
                BOARD_LEDTX_ON();
            }
        }
        else
        {
            BOARD_LEDTX_OFF();
        }
    }
}

void canrxTask(void *argument)
{
    for (;;)
    {
        can_rx_process();
    }
}

void can_rx_process(void)
{
    can_rx_msg_t *rx_msg;
    uint8_t msg_buf[SLCAN_MTU];
    int32_t msg_len;
    for (;;)
    {
        if (xQueueReceive(can_rx_Queue, &rx_msg, 50) == pdPASS)
        {
            msg_len = slcan_parse_frame((uint8_t *)&msg_buf, &rx_msg->header, rx_msg->data);
            // Transmit message via USB-CDC
            if (msg_len > 0)
            {
                cdc_transmit(msg_buf, msg_len);
            }
            osMemoryPoolFree(can_rx_msg_MemPool, rx_msg);
			BOARD_LEDRX_ON();
        }
        else
        {
			BOARD_LEDRX_OFF();
        }
    }
}

// Return reference to CAN handle
FDCAN_HandleTypeDef *can_gethandle(void)
{
    return &can_handle;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == FDCAN_IT_RX_FIFO0_NEW_MESSAGE)
    {
        can_rx_msg_t *rx_msg;
        rx_msg = (can_rx_msg_t *)osMemoryPoolAlloc(can_rx_msg_MemPool, 0U);
        if(rx_msg == NULL)
        {
            can_rx_msg_t rx;
            HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx.header, rx.data);
        }
        else
        {
            HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &(rx_msg->header), rx_msg->data);
            if (!osMemoryPoolGetSpace(can_rx_msg_MemPool))
            {
                osMemoryPoolFree(can_rx_msg_MemPool, rx_msg);
            }
            else
            {
                xQueueSendToBackFromISR(can_rx_Queue, &rx_msg, NULL);
            }
        }
    }
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    if (ErrorStatusITs == FDCAN_IT_BUS_OFF)
    {
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
    }
}

static uint32_t error_tick = 1;
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    uint32_t tick;
    static uint32_t tick_last = 0;
    uint32_t can_err_status = hfdcan->Instance->PSR;
    if (can_err_status & FDCAN_PSR_BO)
    {
        error_tick = 1;
    }
    else
    {
        if (hfdcan->ErrorCode == HAL_FDCAN_ERROR_PROTOCOL_ARBT || hfdcan->ErrorCode == HAL_FDCAN_ERROR_PROTOCOL_DATA)
        {
            tick = HAL_GetTick();
            if (tick >= tick_last)
            {
                cdc_transmit(SLCAN_RET_ERR, SLCAN_RET_LEN);
                tick_last = tick + error_tick;
                error_tick = 0;
            }
        }
    }
    hfdcan->ErrorCode = 0;
}

void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
    error_tick = 0;
    cdc_transmit(SLCAN_RET_OK, SLCAN_RET_LEN);
}

uint32_t can_tx_Queue_space;
uint32_t can_rx_Queue_space;
uint32_t can_tx_msg_MemPool_space;
uint32_t can_rx_msg_MemPool_space;
uint32_t canrxTask_space;
uint32_t cantxTask_space;

void can_task_monitor(void)
{
    can_tx_Queue_space = uxQueueSpacesAvailable(can_tx_Queue);
    can_rx_Queue_space = uxQueueSpacesAvailable(can_rx_Queue);
    can_tx_msg_MemPool_space = osMemoryPoolGetSpace(can_tx_msg_MemPool);
    can_rx_msg_MemPool_space = osMemoryPoolGetSpace(can_rx_msg_MemPool);
    canrxTask_space = osThreadGetStackSpace(canrxTaskHandle);
    cantxTask_space = osThreadGetStackSpace(cantxTaskHandle);
}
