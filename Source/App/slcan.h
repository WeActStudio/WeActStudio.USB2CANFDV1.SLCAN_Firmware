#ifndef __SLCAN_H
#define __SLCAN_H

#include "main.h"

// Maximum rx buffer len
#define SLCAN_MTU (1 + 8 + 1 + 128 + 1) // canfd 64 frame plus \r plus some padding
#define SLCAN_STD_ID_LEN 3
#define SLCAN_EH_STD_ID_LEN 2
#define SLCAN_EXT_ID_LEN 8
#define SLCAN_EH_EXT_ID_LEN 4

#define SLCAN_STD_HEADER ('t')
#define SLCAN_EXT_HEADER ('T')
#define SLCAN_STD_REMOTE_HEADER ('r')
#define SLCAN_EXT_REMOTE_HEADER ('R')
#define SLCAN_STD_FD_HEADER ('d')
#define SLCAN_EXT_FD_HEADER ('D')
#define SLCAN_STD_FDBRS_HEADER ('b')
#define SLCAN_EXT_FDBRS_HEADER ('B')
#define SLCAN_EH_START (0x80)

#define SLCAN_ERROR (-1)
#define SLCAN_OK (0)
#define SLCAN_OK_NOT_RESPONSE (1)
#define SLCAN_WAIT (2)

#define SLCAN_RET_OK (uint8_t *)"\x0D"
#define SLCAN_RET_ERR (uint8_t *)"\x07"
#define SLCAN_RET_LEN 1

#pragma pack(push, 1)
typedef struct{
        uint16_t std_id;
        uint8_t dlc;
        uint8_t data[64];
} std_frame_t;

typedef struct{
        uint32_t ext_id;
        uint8_t dlc;
        uint8_t data[64];
} ext_frame_t;

typedef union {
    uint8_t data[64+4+1];
    std_frame_t std_frame ;
    ext_frame_t ext_frame ;
} can_frame_t;

typedef struct
{
  uint8_t header;
  uint8_t length;
  can_frame_t frame;
} slcan_eh_msg_t;
#pragma pack(pop)

// Prototypes
void slcan_init(void);
uint8_t slcan_is_enhance_mode(void);
int32_t slcan_parse_frame(uint8_t *buf, FDCAN_RxHeaderTypeDef *frame_header, uint8_t* frame_data);
int32_t slcan_parse_str(uint8_t *buf, uint8_t len);

int32_t slcan_eh_parse_frame(uint8_t *buf, FDCAN_RxHeaderTypeDef *frame_header, uint8_t *frame_data);
int32_t slcan_eh_parse_str(uint8_t *buf);

// TODO: move to helper c file
int8_t hal_dlc_code_to_bytes(uint32_t hal_dlc_code);

uint8_t slan_is_need_to_update(void);

#endif // _SLCAN_H
