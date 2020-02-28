/*
*********************************************************************************************************
*                                                uC/FS
*                                      The Embedded File System
*
*                    Copyright 2008-2020 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                      FILE SYSTEM DEVICE DRIVER
*                                       SD/MMC CARD - CARD MODE
*                                BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                              TEMPLATE
*
* Filename : bsp_fs_dev_sd_card.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  FS_DEV_SD_CARD_BSP_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <fs_cfg.h>
#include  <Source/fs_type.h>
#include  <Source/fs_err.h>
#include  <Dev/SD/Card/fs_dev_sd_card.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          REGISTER DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    FILE SYSTEM SD CARD FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      FSDev_SD_Card_BSP_Open()
*
* Description : Open (initialize) SD/MMC card interface.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : DEF_OK,   if interface was opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function will be called every time the device is opened.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_SD_Card_BSP_Open (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_BSP_Close()
*
* Description : Close (unitialize) SD/MMC card interface.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called every time the device is closed.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_Close (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_BSP_Lock()
*
* Description : Acquire SD/MMC card bus lock.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called before the SD/MMC driver begins to access the SD/MMC
*                   card bus.  The application should NOT use the same bus to access another device until
*                   the matching call to 'FSDev_SD_Card_BSP_Unlock()' has been made.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_Lock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                     FSDev_SD_Card_BSP_Unlock()
*
* Description : Release SD/MMC card bus lock.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'FSDev_SD_Card_BSP_Lock()' will be called before the SD/MMC driver begins to access
*                   the SD/MMC card bus.  The application should NOT use the same bus to access another
*                   device until the matching call to this function has been made.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_Unlock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_BSP_CmdStart()
*
* Description : Start a command.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_cmd     Pointer to command to transmit (see Note #2).
*
*               p_data    Pointer to buffer address for DMA transfer (see Note #3).
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             FS_DEV_SD_CARD_ERR_NONE     No error.
*                             FS_DEV_SD_CARD_ERR_NO_CARD  No card present.
*                             FS_DEV_SD_CARD_ERR_BUSY     Controller is busy.
*                             FS_DEV_SD_CARD_ERR_UNKNOWN  Unknown or other error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The command start will be followed by zero, one or two additional BSP function calls,
*                   depending on whether data should be transferred and on whether any errors occur.
*
*                   (a) 'FSDev_SD_Card_BSP_CmdStart()' starts execution of the command.  It may also set
*                       up the DMA transfer (if necessary).
*
*                   (b) 'FSDev_SD_Card_BSP_CmdWaitEnd()' waits for the execution of the command to end,
*                       getting the command response (if any).
*
*                   (c) If data should be transferred from the card to the host, 'FSDev_SD_Card_BSP_CmdDataRd()'
*                       will read that data; if data should be transferred from the host to the card,
*                       'FSDev_SD_Card_BSP_CmdDataWr()' will write that data.
*
*                   (d) If an error is returned at any point, the sequence will be aborted.
*
*               (2) The command 'p_cmd' has the following parameters :
*
*                   (a) 'p_cmd->Cmd' is the command index.
*
*                   (b) 'p_cmd->Arg' is the 32-bit argument (or 0 if there is no argument).
*
*                   (c) 'p_cmd->Flags' is a bit-mapped variable with zero or more command flags :
*
*                           FS_DEV_SD_CARD_CMD_FLAG_INIT        Initialization sequence before command.
*                           FS_DEV_SD_CARD_CMD_FLAG_BUSY        Busy signal expected after command.
*                           FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID   CRC valid after command.
*                           FS_DEV_SD_CARD_CMD_FLAG_IX_VALID    Index valid after command.
*                           FS_DEV_SD_CARD_CMD_FLAG_OPEN_DRAIN  Command line is open drain.
*                           FS_DEV_SD_CARD_CMD_FLAG_DATA_START  Data start command.
*                           FS_DEV_SD_CARD_CMD_FLAG_DATA_STOP   Data stop command.
*                           FS_DEV_SD_CARD_CMD_FLAG_RESP        Response expected.
*                           FS_DEV_SD_CARD_CMD_FLAG_RESP_LONG   Long response expected.
*
*                   (d) 'p_cmd->DataDir' indicates the direction of any data transfer that should follow
*                       this command, if any :
*
*                           FS_DEV_SD_CARD_DATA_DIR_NONE          No data transfer.
*                           FS_DEV_SD_CARD_DATA_DIR_HOST_TO_CARD  Transfer host-to-card (write).
*                           FS_DEV_SD_CARD_DATA_DIR_CARD_TO_HOST  Transfer card-to-host (read).
*
*                   (e) 'p_cmd->DataType' indicates the type of the data transfer that should follow this
*                       command, if any :
*
*                           FS_DEV_SD_CARD_DATA_TYPE_NONE          No data transfer.
*                           FS_DEV_SD_CARD_DATA_TYPE_SINGLE_BLOCK  Single data block.
*                           FS_DEV_SD_CARD_DATA_TYPE_MULTI_BLOCK   Multiple data blocks.
*                           FS_DEV_SD_CARD_DATA_TYPE_STREAM        Stream data.
*
*                   (f) 'p_cmd->RespType' indicates the type of the response that should be expected from
*                       this command :
*
*                           FS_DEV_SD_CARD_RESP_TYPE_NONE  No  response.
*                           FS_DEV_SD_CARD_RESP_TYPE_R1    R1  response: Normal Response Command.
*                           FS_DEV_SD_CARD_RESP_TYPE_R1B   R1b response.
*                           FS_DEV_SD_CARD_RESP_TYPE_R2    R2  response: CID, CSD Register.
*                           FS_DEV_SD_CARD_RESP_TYPE_R3    R3  response: OCR Register.
*                           FS_DEV_SD_CARD_RESP_TYPE_R4    R4  response: Fast I/O Response (MMC).
*                           FS_DEV_SD_CARD_RESP_TYPE_R5    R5  response: Interrupt Request Response (MMC).
*                           FS_DEV_SD_CARD_RESP_TYPE_R5B   R5B response.
*                           FS_DEV_SD_CARD_RESP_TYPE_R6    R6  response: Published RCA Response.
*                           FS_DEV_SD_CARD_RESP_TYPE_R7    R7  response: Card Interface Condition.
*
*                   (g) 'p_cmd->BlkSize' and 'p_cmd->BlkCnt' are the block size and block count of the
*                       data transfer that should follow this command, if any.
*
*               (3) The pointer to the data buffer that will receive the data transfer that should follow
*                   this command is given so that a DMA transfer can be set up.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_CmdStart (FS_QTY               unit_nbr,
                                  FS_DEV_SD_CARD_CMD  *p_cmd,
                                  void                *p_data,
                                  FS_DEV_SD_CARD_ERR  *p_err)
{
    (void)unit_nbr;
    (void)p_cmd;
    (void)p_data;
    (void)p_err;
}


/*
*********************************************************************************************************
*                                   FSDev_SD_Card_BSP_CmdWaitEnd()
*
* Description : Wait for command to end & get command response.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_cmd     Pointer to command that is ending.
*
*               p_resp    Pointer to buffer that will receive command response, if any.
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             FS_DEV_SD_CARD_ERR_NONE          No error.
*                             FS_DEV_SD_CARD_ERR_NO_CARD       No card present.
*                             FS_DEV_SD_CARD_ERR_UNKNOWN       Unknown or other error.
*                             FS_DEV_SD_CARD_ERR_WAIT_TIMEOUT  Timeout in waiting for command response.
*                             FS_DEV_SD_CARD_ERR_RESP_TIMEOUT  Timeout in receiving command response.
*                             FS_DEV_SD_CARD_ERR_RESP_CHKSUM   Error in response checksum.
*                             FS_DEV_SD_CARD_ERR_RESP_CMD_IX   Response command index error.
*                             FS_DEV_SD_CARD_ERR_RESP_END_BIT  Response end bit error.
*                             FS_DEV_SD_CARD_ERR_RESP          Other response error.
*                             FS_DEV_SD_CARD_ERR_DATA          Other data err.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called even if no response is expected from the command.
*
*               (2) This function will NOT be called if 'FSDev_SD_Card_BSP_CmdStart()' returned an error.
*
*               (3) (a) For a command with a normal response, a  4-byte response should be stored in
*                       'p_resp'.
*
*                   (b) For a command with a long   response, a 16-byte response should be stored in
*                       'p_resp'.  The first  4-byte word should hold bits 127..96 of the response.
*                                  The second 4-byte word should hold bits  95..64 of the response.
*                                  The third  4-byte word should hold bits  63..32 of the response.
*                                  The fourth 4-byte word should hold bits  31.. 0 of the reponse.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_CmdWaitEnd (FS_QTY               unit_nbr,
                                    FS_DEV_SD_CARD_CMD  *p_cmd,
                                    CPU_INT32U          *p_resp,
                                    FS_DEV_SD_CARD_ERR  *p_err)
{
    (void)unit_nbr;
    (void)p_cmd;
    (void)p_resp;
    (void)p_err;
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_BSP_CmdDataRd()
*
* Description : Read data following command.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_cmd     Pointer to command that was started.
*
*               p_dest    Pointer to destination buffer.
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             FS_DEV_SD_CARD_ERR_NONE            No error.
*                             FS_DEV_SD_CARD_ERR_NO_CARD         No card present.
*                             FS_DEV_SD_CARD_ERR_UNKNOWN         Unknown or other error.
*                             FS_DEV_SD_CARD_ERR_WAIT_TIMEOUT    Timeout in waiting for data.
*                             FS_DEV_SD_CARD_ERR_DATA_OVERRUN    Data overrun.
*                             FS_DEV_SD_CARD_ERR_DATA_TIMEOUT    Timeout in receiving data.
*                             FS_DEV_SD_CARD_ERR_DATA_CHKSUM     Error in data checksum.
*                             FS_DEV_SD_CARD_ERR_DATA_START_BIT  Data start bit error.
*                             FS_DEV_SD_CARD_ERR_DATA            Other data error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_CmdDataRd (FS_QTY               unit_nbr,
                                   FS_DEV_SD_CARD_CMD  *p_cmd,
                                   void                *p_dest,
                                   FS_DEV_SD_CARD_ERR  *p_err)
{
    (void)unit_nbr;
    (void)p_cmd;
    (void)p_dest;
    (void)p_err;
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_BSP_CmdDataWr()
*
* Description : Write data following command.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_cmd     Pointer to command that was started.
*
*               p_src     Pointer to source buffer.
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             FS_DEV_SD_CARD_ERR_NONE            No error.
*                             FS_DEV_SD_CARD_ERR_NO_CARD         No card present.
*                             FS_DEV_SD_CARD_ERR_UNKNOWN         Unknown or other error.
*                             FS_DEV_SD_CARD_ERR_WAIT_TIMEOUT    Timeout in waiting for data.
*                             FS_DEV_SD_CARD_ERR_DATA_UNDERRUN   Data underrun.
*                             FS_DEV_SD_CARD_ERR_DATA_CHKSUM     Err in data checksum.
*                             FS_DEV_SD_CARD_ERR_DATA_START_BIT  Data start bit error.
*                             FS_DEV_SD_CARD_ERR_DATA            Other data error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_CmdDataWr (FS_QTY               unit_nbr,
                                   FS_DEV_SD_CARD_CMD  *p_cmd,
                                   void                *p_src,
                                   FS_DEV_SD_CARD_ERR  *p_err)
{
    (void)unit_nbr;
    (void)p_cmd;
    (void)p_src;
    (void)p_err;
}


/*
*********************************************************************************************************
*                                  FSDev_SD_Card_BSP_GetBlkCntMax()
*
* Description : Get maximum number of blocks that can be transferred with a multiple read or multiple
*               write command.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               blk_size  Block size, in octets.
*
* Return(s)   : Maximum number of blocks.
*
* Note(s)     : (1) The DMA region from which data is read or written may be a limited size.  The count
*                   returned by this function should be the number of blocks of size 'blk_size' that can
*                   fit into this region.
*
*               (2) If the controller is not capable of multiple block reads or writes, 1 should be
*                   returned.
*
*               (3) If the controller has no limit on the number of blocks in a multiple block read or
*                   write, 'DEF_INT_32U_MAX_VAL' should be returned.
*
*               (4) This function SHOULD always return the same value.  If hardware constraints change
*                   at run-time, the device MUST be closed & re-opened for any changes to be effective.
*********************************************************************************************************
*/

CPU_INT32U  FSDev_SD_Card_BSP_GetBlkCntMax (FS_QTY      unit_nbr,
                                            CPU_INT32U  blk_size)
{
    (void)unit_nbr;
    (void)blk_size;

    return (DEF_INT_32U_MAX_VAL);
}


/*
*********************************************************************************************************
*                                 FSDev_SD_Card_BSP_GetBusWidthMax()
*
* Description : Get maximum bus width, in bits.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : Maximum bus width.
*
* Note(s)     : (1) Allowed values are typically 1, 4 & 8.
*
*               (2) This function SHOULD always return the same value.  If hardware constraints change
*                   at run-time, the device MUST be closed & re-opened for any changes to be effective.
*********************************************************************************************************
*/

CPU_INT08U  FSDev_SD_Card_BSP_GetBusWidthMax (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    return (4u);
}


/*
*********************************************************************************************************
*                                   FSDev_SD_Card_BSP_SetBusWidth()
*
* Description : Set bus width.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               width     Bus width, in bits.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_SetBusWidth (FS_QTY      unit_nbr,
                                     CPU_INT08U  width)
{
    (void)unit_nbr;
    (void)width;
}


/*
*********************************************************************************************************
*                                   FSDev_SD_Card_BSP_SetClkFreq()
*
* Description : Set clock frequency.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               freq      Clock frequency, in Hz.
*
* Return(s)   : none.
*
* Note(s)     : (1) The effective clock frequency MUST be no more than 'freq'.  If the frequency cannot be
*                   configured equal to 'freq', it should be configured less than 'freq'.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_SetClkFreq (FS_QTY      unit_nbr,
                                    CPU_INT32U  freq)
{
    (void)unit_nbr;
    (void)freq;
}


/*
*********************************************************************************************************
*                                 FSDev_SD_Card_BSP_SetTimeoutData()
*
* Description : Set data timeout.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               to_clks   Timeout, in clocks.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_SetTimeoutData (FS_QTY      unit_nbr,
                                        CPU_INT32U  to_clks)
{
    (void)unit_nbr;
    (void)to_clks;
}


/*
*********************************************************************************************************
*                                 FSDev_SD_Card_BSP_SetTimeoutResp()
*
* Description : Set response timeout.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               to_ms     Timeout, in milliseconds.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_Card_BSP_SetTimeoutResp (FS_QTY      unit_nbr,
                                        CPU_INT32U  to_ms)
{
    (void)unit_nbr;
    (void)to_ms;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/
