//
// slcan: Parse incoming and generate outgoing slcan messages
//

#include "slcan.h"

#include <string.h>
#include "can.h"
#include "error.h"
#include "usbd_cdc_if.h"

#include "bootloader.h"

// Private variables
//char *fw_id = "WeAct Studio 2024.01.10 V1.0.0.0\r";

// Private methods
static uint32_t __std_dlc_code_to_hal_dlc_code(uint8_t dlc_code);
static uint8_t __hal_dlc_code_to_std_dlc_code(uint32_t hal_dlc_code);

static uint8_t is_need_to_update = 0;

// FIXME: Pressing enter repeats the previous TX
void slcan_init(void)
{
	is_need_to_update = 0;
	can_disable();
}

// Parse an incoming CAN frame into an outgoing slcan message
int32_t slcan_parse_frame(uint8_t *buf, FDCAN_RxHeaderTypeDef *frame_header, uint8_t *frame_data)
{
	// Clear buffer
	for (uint8_t j = 0; j < SLCAN_MTU; j++)
		buf[j] = '\0';

	// Start building the slcan message string at idx 0 in buf[]
	int32_t msg_idx = 0;

	// Handle classic CAN frames
	if (frame_header->FDFormat == FDCAN_CLASSIC_CAN)
	{
		// Add character for frame type
		if (frame_header->RxFrameType == FDCAN_DATA_FRAME)
		{
			buf[msg_idx] = 't';
		}
		else if (frame_header->RxFrameType == FDCAN_REMOTE_FRAME)
		{
			buf[msg_idx] = 'r';
		}
	}
	// Handle FD CAN frames
	else
	{
		// FD doesn't support remote frames so this must be a data frame

		// Frame with BRS enabled
		if (frame_header->BitRateSwitch == FDCAN_BRS_ON)
		{
			buf[msg_idx] = 'b';
		}
		// Frame with BRS disabled
		else
		{
			buf[msg_idx] = 'd';
		}
	}

	// Assume standard identifier
	uint8_t id_len = SLCAN_STD_ID_LEN;
	uint32_t tmp = frame_header->Identifier;

	// Check if extended
	if (frame_header->IdType == FDCAN_EXTENDED_ID)
	{
		// Convert first char to upper case for extended frame
		buf[msg_idx] -= 32;
		id_len = SLCAN_EXT_ID_LEN;
		tmp = frame_header->Identifier;
	}
	msg_idx++;

	// Add identifier to buffer
	for (uint8_t j = id_len; j > 0; j--)
	{
		// Add nibble to buffer
		buf[j] = (tmp & 0xF);
		tmp = tmp >> 4;
		msg_idx++;
	}

	// Add DLC to buffer
	buf[msg_idx++] = __hal_dlc_code_to_std_dlc_code(frame_header->DataLength);
	int8_t bytes = hal_dlc_code_to_bytes(frame_header->DataLength);

	// Check bytes value
	if (bytes < 0)
		return -1;
	if (bytes > 64)
		return -1;

	// Add data bytes
	for (uint8_t j = 0; j < bytes; j++)
	{
		buf[msg_idx++] = (frame_data[j] >> 4);
		buf[msg_idx++] = (frame_data[j] & 0x0F);
	}

	// Convert to ASCII (2nd character to end)
	for (uint8_t j = 1; j < msg_idx; j++)
	{
		if (buf[j] < 0xA)
		{
			buf[j] += 0x30;
		}
		else
		{
			buf[j] += 0x37;
		}
	}

	// Add CR for slcan EOL
	buf[msg_idx++] = '\r';

	// Return string length
	return msg_idx;
}

// Parse an incoming slcan command from the USB CDC port
int32_t slcan_parse_str(uint8_t *buf, uint8_t len)
{
	int32_t return_value = 2;

	can_tx_msg_t *tx_msg;
	tx_msg = (can_tx_msg_t *)osMemoryPoolAlloc(can_tx_msg_MemPool, 0U);
	if(tx_msg == NULL)
		return SLCAN_ERROR;

	tx_msg->header.TxFrameType = FDCAN_DATA_FRAME;
	tx_msg->header.FDFormat = FDCAN_CLASSIC_CAN;
	tx_msg->header.IdType = FDCAN_STANDARD_ID;
	tx_msg->header.BitRateSwitch = FDCAN_BRS_OFF;
	tx_msg->header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	tx_msg->header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	tx_msg->header.MessageMarker = 0;
	memset(tx_msg->data, 0x00, sizeof(tx_msg->data));

	// Convert from ASCII (2nd character to end)
	for (uint8_t i = 1; i < len; i++)
	{
		// Lowercase letters
		if (buf[i] >= 'a')
		{
			if (buf[i] <= 'f')
			{
				buf[i] = buf[i] - ('a' - 10);
			}
			else
			{
				return_value = SLCAN_ERROR;
				goto return_now;
			}
		}
		// Uppercase letters
		else if (buf[i] >= 'A')
		{
			if (buf[i] <= 'F')
			{
				buf[i] = buf[i] - ('A' - 10);
			}
			else
			{
				return_value = SLCAN_ERROR;
				goto return_now;
			}
		}
		// Numbers
		else
		{
			if (buf[i] <= '9')
			{
				buf[i] = buf[i] - '0';
			}
			else
			{
				return_value = SLCAN_ERROR;
				goto return_now;
			}
		}
	}

	// Handle each incoming command
	switch (buf[0])
	{
	// Open channel
	case 'O':
	{
		if (len == 1)
		{
			can_enable();
			return_value = SLCAN_OK;
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// Close channel
	case 'C':
	{
		if (len == 1)
		{
			can_disable();
			return_value = SLCAN_OK;
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// Set nominal bitrate
	case 'S':
	{
		if (len == 2)
		{
			// Check for valid bitrate
			if (buf[1] >= CAN_BITRATE_INVALID)
			{
				return_value = SLCAN_ERROR;
			}
			else
			{
				can_set_bitrate((enum can_bitrate)buf[1]);
				return_value = SLCAN_OK;
			}
		}
		else if (len == 5)
		{
			uint8_t seg1, seg2;
			seg1 = buf[1] << 4 | buf[2];
			seg2 = buf[3] << 4 | buf[4];
			return_value = can_set_bitrate2(seg1, seg2);
		}
		else if (len == 7)
		{
			uint8_t div, seg1, seg2;
			div = buf[1] << 4 | buf[2];
			seg1 = buf[3] << 4 | buf[4];
			seg2 = buf[5] << 4 | buf[6];
			return_value = can_set_bitrate3(div, seg1, seg2);
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// Set data bitrate
	case 'Y':
	{
		if (len == 2)
		{
			// Check for valid bitrate
			if (buf[1] >= CAN_DATA_BITRATE_INVALID)
			{
				return_value = SLCAN_ERROR;
			}
			else
			{
				can_set_data_bitrate((enum can_data_bitrate)buf[1]);
				return_value = SLCAN_OK;
			}
		}
		else if (len == 5)
		{
			uint8_t seg1, seg2;
			seg1 = buf[1] << 4 | buf[2];
			seg2 = buf[3] << 4 | buf[4];
			return_value = can_set_data_bitrate2(seg1, seg2);
		}
		else if (len == 7)
		{
			uint8_t div, seg1, seg2;
			div = buf[1] << 4 | buf[2];
			seg1 = buf[3] << 4 | buf[4];
			seg2 = buf[5] << 4 | buf[6];
			return_value = can_set_data_bitrate3(div, seg1, seg2);
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// FIXME: Nonstandard!
	case 'M':
	{
		if (len == 2 && (can_is_enable() == OFF_BUS))
		{
			return_value = SLCAN_OK;
			// Set mode command
			if (buf[1] == 1)
			{
				// Mode 1: silent
				can_set_silent(1);
			}
			else if (buf[1] == 0)
			{
				// Default to normal mode
				can_set_silent(0);
			}
			else
			{
				return_value = SLCAN_ERROR;
			}
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// FIXME: Nonstandard!
	case 'A':
	{
		if (len == 2 && (can_is_enable() == OFF_BUS))
		{
			return_value = SLCAN_OK;
			// Set autoretry command
			if (buf[1] == 1)
			{
				// Mode 1: autoretry enabled (default)
				can_set_autoretransmit(ENABLE);
			}
			else if (buf[1] == 0)
			{
				// Mode 0: autoretry disabled
				can_set_autoretransmit(DISABLE);
			}
			else
			{
				return_value = SLCAN_ERROR;
			}
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// FIXME: Nonstandard!
	case 'V':
	{
		if (len == 1)
		{
			// Report firmware version and remote
			cdc_transmit((uint8_t *)bootloader_get_version(), strlen(bootloader_get_version()));
			return_value = SLCAN_OK_NOT_RESPONSE;
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// FIXME: Nonstandard!
	case 'E':
	{
		if (len == 1)
		{
			// Report error register
			char errstr[64] = {0};
			snprintf(errstr, 64, "CANable Error Register: %X", (unsigned int)error_reg());
			cdc_transmit((uint8_t *)errstr, strlen(errstr));
			return_value = SLCAN_OK_NOT_RESPONSE;
		}
		else
		{
			return_value = SLCAN_ERROR;
		}
	}
	break;

	// Firmware update
	case 'X':
	{
	  //#warning "TODO: Implement firmware update via command"
		if (len == 1 && (can_is_enable() == OFF_BUS))
		{
			return_value = SLCAN_OK;
			is_need_to_update = 1;
		}
		else
		{
			return_value = SLCAN_ERROR;
			is_need_to_update = 0;
		}
	}
	break;

	// Transmit data frame (Standard ID)
	case 't':
	{
		tx_msg->header.IdType = FDCAN_STANDARD_ID;
	}
	break;

	// Transmit data frame (Extended ID)
	case 'T':
	{
		tx_msg->header.IdType = FDCAN_EXTENDED_ID;
	}
	break;

	// Transmit remote frame (Standard ID)
	case 'r':
	{
		tx_msg->header.IdType = FDCAN_STANDARD_ID;
		tx_msg->header.TxFrameType = FDCAN_REMOTE_FRAME;
	}
	break;

	// Transmit remote frame (Extended ID)
	case 'R':
	{
		tx_msg->header.IdType = FDCAN_EXTENDED_ID;
		tx_msg->header.TxFrameType = FDCAN_REMOTE_FRAME;
	}
	break;

	// Transmit CAN FD standard ID (no BRS)
	case 'd':
	{
		tx_msg->header.FDFormat = FDCAN_FD_CAN;
		tx_msg->header.IdType = FDCAN_STANDARD_ID;
		tx_msg->header.BitRateSwitch = FDCAN_BRS_OFF;
	}
	break;

	// Transmit CAN FD extended ID (no BRS)
	case 'D':
	{
		tx_msg->header.FDFormat = FDCAN_FD_CAN;
		tx_msg->header.IdType = FDCAN_EXTENDED_ID;
		tx_msg->header.BitRateSwitch = FDCAN_BRS_OFF;
	}
	break;

	// CANFD standard id transmit - with BRS
	case 'b':
	{
		tx_msg->header.FDFormat = FDCAN_FD_CAN;
		tx_msg->header.IdType = FDCAN_STANDARD_ID;
		tx_msg->header.BitRateSwitch = FDCAN_BRS_ON;
	}
	break;

	// CANFD extended id transmit - with BRS
	case 'B':
	{
		tx_msg->header.FDFormat = FDCAN_FD_CAN;
		tx_msg->header.IdType = FDCAN_EXTENDED_ID;
		tx_msg->header.BitRateSwitch = FDCAN_BRS_ON;
	}
	break;

	// Invalid command
	default:
	{
		return_value = SLCAN_ERROR;
	}
	break;
	}

	// if need return
	if (return_value != 2)
	{
	return_now:
		osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
		return return_value;
	}
	// if can disable
	else if (can_is_enable() == OFF_BUS)
	{
		osMemoryPoolFree(can_tx_msg_MemPool, tx_msg);
		return SLCAN_ERROR;
	}

	// Start parsing at second byte (skip command byte)
	uint8_t parse_loc = 1;

	// Zero out identifier
	tx_msg->header.Identifier = 0;

	uint8_t id_len;

	// Update length if message is extended ID
	if (tx_msg->header.IdType == FDCAN_EXTENDED_ID)
		id_len = SLCAN_EXT_ID_LEN;
	else
		// standard ID
		id_len = SLCAN_STD_ID_LEN;

	// Iterate through ID bytes
	while (parse_loc <= id_len)
	{
		tx_msg->header.Identifier <<= 4;
		tx_msg->header.Identifier |= buf[parse_loc++];
	}

	// Attempt to parse DLC and check sanity
	uint8_t dlc_code_raw = buf[parse_loc++];

	// If dlc is too long for an FD frame
	if (tx_msg->header.FDFormat == FDCAN_FD_CAN && dlc_code_raw > 0xF)
	{
		return_value = SLCAN_ERROR;
		goto return_now;
	}
	if (tx_msg->header.FDFormat == FDCAN_CLASSIC_CAN && dlc_code_raw > 0x8)
	{
		return_value = SLCAN_ERROR;
		goto return_now;
	}

	// Set TX frame DLC according to HAL
	tx_msg->header.DataLength = __std_dlc_code_to_hal_dlc_code(dlc_code_raw);

	// Calculate number of bytes we expect in the message
	int8_t bytes_in_msg = hal_dlc_code_to_bytes(tx_msg->header.DataLength);

	if ((bytes_in_msg < 0) || (bytes_in_msg > 64))
	{
		return_value = SLCAN_ERROR;
		goto return_now;
	}

	// Check if len != command + id + length + bytes_in_msg
	uint8_t tx_msg_len = 1 + id_len + 1 + (bytes_in_msg << 1);

	if (tx_msg_len != len)
	{
		return_value = SLCAN_ERROR;
		goto return_now;
	}

	// Parse data
	// TODO: Guard against walking off the end of the string!
	for (uint8_t i = 0; i < bytes_in_msg; i++)
	{
		tx_msg->data[i] = (buf[parse_loc] << 4) + buf[parse_loc + 1];
		parse_loc += 2;
	}

	// Transmit the message
	return_value = can_tx(tx_msg);

	return return_value;
}

// Convert a FDCAN_data_length_code to number of bytes in a message
int8_t hal_dlc_code_to_bytes(uint32_t hal_dlc_code)
{
	switch (hal_dlc_code)
	{
	case FDCAN_DLC_BYTES_0:
		return 0;
	case FDCAN_DLC_BYTES_1:
		return 1;
	case FDCAN_DLC_BYTES_2:
		return 2;
	case FDCAN_DLC_BYTES_3:
		return 3;
	case FDCAN_DLC_BYTES_4:
		return 4;
	case FDCAN_DLC_BYTES_5:
		return 5;
	case FDCAN_DLC_BYTES_6:
		return 6;
	case FDCAN_DLC_BYTES_7:
		return 7;
	case FDCAN_DLC_BYTES_8:
		return 8;
	case FDCAN_DLC_BYTES_12:
		return 12;
	case FDCAN_DLC_BYTES_16:
		return 16;
	case FDCAN_DLC_BYTES_20:
		return 20;
	case FDCAN_DLC_BYTES_24:
		return 24;
	case FDCAN_DLC_BYTES_32:
		return 32;
	case FDCAN_DLC_BYTES_48:
		return 48;
	case FDCAN_DLC_BYTES_64:
		return 64;
	default:
		return -1;
	}
}

// Convert a standard 0-F CANFD length code to a FDCAN_data_length_code
// TODO: make this a macro
static uint32_t __std_dlc_code_to_hal_dlc_code(uint8_t dlc_code)
{
	return (uint32_t)dlc_code;
}

// Convert a FDCAN_data_length_code to a standard 0-F CANFD length code
// TODO: make this a macro
static uint8_t __hal_dlc_code_to_std_dlc_code(uint32_t hal_dlc_code)
{
	return (uint8_t)hal_dlc_code;
}

uint8_t slan_is_need_to_update(void)
{
	return is_need_to_update;
}
