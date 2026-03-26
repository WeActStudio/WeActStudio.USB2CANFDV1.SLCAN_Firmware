#ifndef _CAN_H
#define _CAN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"
    // Classic CAN / CANFD nominal bitrates
    enum can_bitrate
    {
        CAN_BITRATE_10K = 0,
        CAN_BITRATE_20K,
        CAN_BITRATE_50K,
        CAN_BITRATE_100K,
        CAN_BITRATE_125K,
        CAN_BITRATE_250K,
        CAN_BITRATE_500K,
        CAN_BITRATE_800K,
        CAN_BITRATE_1000K,
        CAN_BITRATE_83_3K,
        CAN_BITRATE_75K,
        CAN_BITRATE_62_5K,
        CAN_BITRATE_33_3K,
        CAN_BITRATE_5K,
        CAN_BITRATE_INVALID,
    };

    // CANFD bitrates
    enum can_data_bitrate
    {
        CAN_DATA_BITRATE_1M = 1,
        CAN_DATA_BITRATE_2M = 2,
        CAN_DATA_BITRATE_3M = 3,
        CAN_DATA_BITRATE_4M = 4,
        CAN_DATA_BITRATE_5M = 5,

        CAN_DATA_BITRATE_INVALID,
    };

    // Bus state
    enum can_bus_state
    {
        OFF_BUS,
        ON_BUS
    };

// CAN transmit buffering
#define TXQUEUE_DATALEN 64 // CAN DLC length of data buffers. Must be 64 for canfd.

    // structure for CAN frames
    typedef struct
    {
        uint8_t data[TXQUEUE_DATALEN]; // Data buffer
        FDCAN_TxHeaderTypeDef header;  // Header buffer
    } can_tx_msg_t;

    typedef struct
    {
        uint8_t data[TXQUEUE_DATALEN]; // Data buffer
        FDCAN_RxHeaderTypeDef header;  // Header buffer
    } can_rx_msg_t;

    extern osMemoryPoolId_t can_tx_msg_MemPool;

    // Prototypes
    void can_init(void);
    void can_enable(void);
    void can_disable(void);
		enum can_bus_state can_is_enable(void);
    void can_set_bitrate(enum can_bitrate bitrate);
		int32_t can_set_bitrate2(uint8_t seg1,uint8_t seg2);
    int32_t can_set_bitrate3(uint8_t div, uint8_t seg1, uint8_t seg2);
    void can_set_data_bitrate(enum can_data_bitrate bitrate);
		int32_t can_set_data_bitrate2(uint8_t seg1, uint8_t seg2);
    int32_t can_set_data_bitrate3(uint8_t div, uint8_t seg1, uint8_t seg2);
    void can_set_silent(uint8_t silent);
    void can_set_autoretransmit(uint8_t autoretransmit);
    int32_t can_tx(can_tx_msg_t *tx_msg);
    void can_tx_process(void);
		void can_rx_process(void);
    FDCAN_HandleTypeDef *can_gethandle(void);
		
		void can_task_monitor(void);

#ifdef __cplusplus
}
#endif

#endif // _CAN_H
