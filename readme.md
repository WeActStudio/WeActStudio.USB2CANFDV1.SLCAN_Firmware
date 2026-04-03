# WeAct Studio USB2CANFD V1 SLCAN Firmware

## Firmware packaging
* Open [firmware_packager](https://github.com/WeActStudio/WeActStudio.USB2CANFDV1/blob/master/Tools/WeActStudio_Upgrade_Tool.zip), select the firmware, check Do not encrypt, add character watermark, select the header size, and click Pack.  
![display](/Images/firmware_packager_en.png)

## SLCAN Protocol Description
> Using the virtual serial, the command is as follows:
- `O[CR]` - Opens the CAN channel
- `C[CR]` - Close the CAN channel
- `S0[CR]` - Set the nominal bit rate to 10k
- `S1[CR]` - Set the nominal bit rate to 20k
- `S2[CR]` - Set the nominal bit rate to 50k
- `S3[CR]` - Set the nominal bit rate to 100k
- `S4[CR]` - Set the nominal bit rate to 125k (default)
- `S5[CR]` - Set the nominal bit rate to 250k
- `S6[CR]` - Set the nominal bit rate to 500k
- `S7[CR]` - Set the nominal bit rate to 800k
- `S8[CR]` - Set the nominal bit rate to 1M
- `S9[CR]` - Set the nominal bit rate to 83.3k
- `SA[CR]` - Set the nominal bit rate to 75k
- `SB[CR]` - Set the nominal bit rate to 62.5k
- `SC[CR]` - Set the nominal bit rate to 33.3k
- `SD[CR]` - Set the nominal bit rate to 5k
- `Sxxyy[CR]` - Custom nominal bit rate (60/2=30Mhz CAN clock) [xx=seg1(hex,0x02\~0xff), yy=seg2(hex,0x02\~0x80)]
- `Sddxxyy[CR]` - Custom nominal bit rate ([60/div]Mhz CAN clock) [dd=div(hex,0x01\~0xff), xx=seg1(hex,0x02\~0xff), yy=seg2(hex,0x02\~0x80)]
- `Y1[CR]` - Set the CANFD data segment bit rate to 1M
- `Y2[CR]` - Set CANFD data segment bit rate to 2M (default)
- `Y3[CR]` - Set the CANFD data segment bit rate to 3M
- `Y4[CR]` - Set the CANFD data segment bit rate to 4M
- `Y5[CR]` - Set the CANFD data segment bit rate to 5M
- `Yxxyy[CR]` - Custom CANFD data segment bit rate (60Mhz CAN clock) [xx=seg1(hex,0x01\~0x20), yy=seg2(hex,0x01\~0x10)]
- `Yddxxyy[CR]` - Custom CANFD data segment bit rate ([60/div]Mhz CAN clock) [dd=div(hex,0x01\~0x20), xx=seg1(hex,0x01\~0x20), yy=seg2(hex,0x01\~0x10)]
- `M0[CR]` - Set to normal mode, need close can channel (default)
- `M1[CR]` - Set to silent mode, need close can channel
- `A0[CR]` - Turn off automatic retransmission, need close can channel (default)
- `A1[CR]` - Enable automatic retransmission, need close can channel (not recommended, may crash)
- `tIIILDD...[CR]` - Transfer data frame (standard ID) [ID, length, data]
- `TIIIIIIIILDD...[CR]` - Transfer data frame (extended ID) [ID, length, data]
- `rIIIL[CR]` - Transfer remote frame (standard ID) [ID, length]
- `RIIIIIIIIL[CR]` - Transfer remote frame (extended ID) [ID, length]
- `dIIILDD...[CR]` - Transmit CANFD standard frames (without BRS enabled) [ID, length, data]
- `DIIIIIIIILDD...[CR]` - Transmit CANFD extended frames (without BRS enabled) [ID, length, data]
- `bIIILDD...[CR]` - Transmit CANFD standard frames (BRS enabled) [ID, length, data]
- `BIIIIIIIILDD...[CR]` - Transmit CANFD extended frames (BRS enabled) [ID, length, data]
- `0x80 + t, uint8_t length , uint16_t ID, uint8_t DLC, uint8_t *D...` - Transfer data frame (standard ID) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `0x80 + T, uint8_t length , uint32_t ID, uint8_t DLC, uint8_t *D...` - Transfer data frame (extended ID) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `0x80 + r, uint8_t length , uint16_t ID, uint8_t DLC` - Transfer remote frame (standard ID) [length, ID, DLC] Enhance mode, length=ID+DLC
- `0x80 + R, uint8_t length , uint16_t ID, uint8_t DLC` - Transfer remote frame (extended ID) [length, ID, DLC] Enhance mode, length=ID+DLC
- `0x80 + d, uint8_t length , uint16_t ID, uint8_t DLC, uint8_t *D...` - Transmit CANFD standard frames (without BRS enabled) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `0x80 + D, uint8_t length , uint32_t ID, uint8_t DLC, uint8_t *D...` - Transmit CANFD extended frames (without BRS enabled) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `0x80 + b, uint8_t length , uint16_t ID, uint8_t DLC, uint8_t *D...` - Transmit CANFD standard frames (BRS enabled) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `0x80 + B, uint8_t length , uint32_t ID, uint8_t DLC, uint8_t *D...` - Transmit CANFD extended frames (BRS enabled) [length, ID, DLC, Data] Enhance mode, length=ID+DLC+Data
- `V[CR]` - Reads the firmware version
- `E[CR]` - Read the failure state
- `X[CR]` - Enter firmware upgrade mode
- `H0[CR]` - Turn off SLCAN Enhance mode, need close can channel (default)
- `H1[CR]` - Turn on SLCAN Enhance mode, need close can channel
- `fIIIMMM[CR]` - Set standard ID filter, need close can channel [ID, mask] (default: 0,0)
- `FIIIIIIIIMMMMMMMM[CR]` - Set extended ID filter, need close can channel [ID, mask] (default: 0,0)

`[CR]` : `0x0D` (hex), `\r` (ascii)

**A status statement is returned after the command is sent**
- [CR]: transmission successful
- 0x07: transmission failed

**Note**  
The CANFD message length is as follows (in ascii, eg: `A`) (uint8_t DLC is hex, eg: `0x0A`):
- `0-8` : Same as standard CAN
- `9` : length = 12
- `A` : length = 16
- `B` : length = 20
- `C` : length = 24
- `D` : length = 32
- `E` : length = 48
- `F` : length = 64

**cangaroo is located in [Tools/cangaroo](https://github.com/WeActStudio/WeActStudio.USB2CANFDV1/tree/master/Tools)**  
**The documentation for calculating custom Bitrate Settings is located in `Doc/CAN Bitrate Calculate_波特率计算.xlsx`**
