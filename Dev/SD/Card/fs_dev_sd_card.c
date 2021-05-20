/*
*********************************************************************************************************
*                                                uC/FS
*                                      The Embedded File System
*
*                    Copyright 2008-2021 Silicon Laboratories Inc. www.silabs.com
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
*
*                                             SD/MMC CARD
*                                              CARD MODE
*
* Filename     : fs_dev_sd_card.c
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) SD Card Association.  "Physical Layer Simplified Specification Version 2.00".
*                     July 26, 2006.
*
*                 (2) JEDEC Solid State Technology Association.  "MultiMediaCard (MMC) Electrical
*                     Standard, High Capacity".  JESD84-B42.  July 2007.
*********************************************************************************************************
* Note(s)      : (1) This driver has been tested with MOST SD/MMC media types, including :
*
*                     (a) Standard capacity SD cards, v1.x & v2.0.
*                     (b) SDmicro cards.
*                     (c) High capacity SD cards (SDHC)
*                     (d) MMC
*                     (e) MMCplus
*
*                     It should also work with devices conformant to the relevant SD or MMC specifications.
*
*                (2) High capacity MMC cards (>2Gig) requires an additional file system buffer for
*                    its identification sequence.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_SD_CARD_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  "../../../Source/fs.h"
#include  "../../../Source/fs_buf.h"
#include  "fs_dev_sd_card.h"


/*
*********************************************************************************************************
*                                           LINT INHIBITION
*
* Note(s) : (1) Certain macro's within this file describe commands, command values or register fields
*               that are currently unused.  lint error option #750 is disabled to prevent error messages
*               because of unused macro's :
*
*                   "Info 750: local macro '...' (line ..., file ...) not referenced"
*********************************************************************************************************
*/

/*lint -e750*/


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_SD_CARD_RETRIES_CMD                      256u
#define  FS_DEV_SD_CARD_RETRIES_CMD_BUSY               10000u
#define  FS_DEV_SD_CARD_RETRIES_DATA                       5u

#define  FS_DEV_SD_CARD_TIMEOUT_CMD_ms                  1000u

#define  FS_DEV_SD_CARD_RCA_DFLT                      0x0001u

                                                                /* ------------- CARD STATES (Table 4-35) ------------- */
#define  FS_DEV_SD_CARD_STATE_IDLE                         0u
#define  FS_DEV_SD_CARD_STATE_READY                        1u
#define  FS_DEV_SD_CARD_STATE_IDENT                        2u
#define  FS_DEV_SD_CARD_STATE_STBY                         3u
#define  FS_DEV_SD_CARD_STATE_TRAN                         4u
#define  FS_DEV_SD_CARD_STATE_DATA                         5u
#define  FS_DEV_SD_CARD_STATE_RCV                          6u
#define  FS_DEV_SD_CARD_STATE_PRG                          7u
#define  FS_DEV_SD_CARD_STATE_DIS                          8u
#define  FS_DEV_SD_CARD_STATE_IO                          15u

                                                                /* -------------- CMD FLAGS PER RESP TYPE ------------- */
#define  FS_DEV_SD_CARD_CMD_FLAG_UNKNOWN         (FS_DEV_SD_CARD_CMD_FLAG_NONE)
#define  FS_DEV_SD_CARD_CMD_FLAG_R1              (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID                                | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R1B             (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID | FS_DEV_SD_CARD_CMD_FLAG_BUSY | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R2              (                                   FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID                                | FS_DEV_SD_CARD_CMD_FLAG_RESP | FS_DEV_SD_CARD_CMD_FLAG_RESP_LONG)
#define  FS_DEV_SD_CARD_CMD_FLAG_R3              (FS_DEV_SD_CARD_CMD_FLAG_NONE                                                                        | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R4              (FS_DEV_SD_CARD_CMD_FLAG_NONE                                                                        | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R5              (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID                                | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R5B             (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID | FS_DEV_SD_CARD_CMD_FLAG_BUSY | FS_DEV_SD_CARD_CMD_FLAG_RESP | FS_DEV_SD_CARD_CMD_FLAG_RESP_LONG)
#define  FS_DEV_SD_CARD_CMD_FLAG_R6              (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID                                | FS_DEV_SD_CARD_CMD_FLAG_RESP)
#define  FS_DEV_SD_CARD_CMD_FLAG_R7              (FS_DEV_SD_CARD_CMD_FLAG_IX_VALID | FS_DEV_SD_CARD_CMD_FLAG_CRC_VALID                                | FS_DEV_SD_CARD_CMD_FLAG_RESP)

/*
*********************************************************************************************************
*                                CARD STATUS (R1) RESPONSE BIT DEFINES
*
* Note(s) : (1) See the [Ref 1], Table 4-35.
*********************************************************************************************************
*/

#define  FS_DEV_SD_CARD_R1_AKE_SEQ_ERROR         DEF_BIT_03     /* Error in the sequence of the authentication process. */
#define  FS_DEV_SD_CARD_R1_APP_CMD               DEF_BIT_05     /* Card will expect ACMD, or command interpreted as.    */
#define  FS_DEV_SD_CARD_R1_READY_FOR_DATA        DEF_BIT_08     /* Corresponds to buffer empty signaling on the bus.    */
#define  FS_DEV_SD_CARD_R1_ERASE_RESET           DEF_BIT_13     /* An erase seq was clr'd before executing.             */
#define  FS_DEV_SD_CARD_R1_CARD_ECC_DISABLED     DEF_BIT_14     /* COmmand has been executed without using internal ECC.*/
#define  FS_DEV_SD_CARD_R1_WP_ERASE_SKIP         DEF_BIT_15     /* Only partial addr space  erased due to WP'd blks.    */
#define  FS_DEV_SD_CARD_R1_CSD_OVERWRITE         DEF_BIT_16     /* Read-only section of CSD does not match card.        */
#define  FS_DEV_SD_CARD_R1_ERROR                 DEF_BIT_19     /* General or unknown error occurred.                   */
#define  FS_DEV_SD_CARD_R1_CC_ERROR              DEF_BIT_20     /* Internal card controller error.                      */
#define  FS_DEV_SD_CARD_R1_CARD_ECC_FAILED       DEF_BIT_21     /* Card internal ECC applied but failed to correct data.*/
#define  FS_DEV_SD_CARD_R1_ILLEGAL_COMMAND       DEF_BIT_22     /* Command not legal for the card state.                */
#define  FS_DEV_SD_CARD_R1_COM_CRC_ERROR         DEF_BIT_23     /* CRC check of previous command failed.                */
#define  FS_DEV_SD_CARD_R1_LOCK_UNLOCK_FAILED    DEF_BIT_24     /* Sequence of password error has been detected.        */
#define  FS_DEV_SD_CARD_R1_CARD_IS_LOCKED        DEF_BIT_25     /* Signals that card is locked by host.                 */
#define  FS_DEV_SD_CARD_R1_WP_VIOLATION          DEF_BIT_26     /* Set when the host attempts to write to protected blk.*/
#define  FS_DEV_SD_CARD_R1_ERASE_PARAM           DEF_BIT_27     /* Invalid selection of write-blocks for erase occurred.*/
#define  FS_DEV_SD_CARD_R1_ERASE_SEQ_ERROR       DEF_BIT_28     /* Error in the sequence of erase commands occurred.    */
#define  FS_DEV_SD_CARD_R1_BLOCK_LEN_ERROR       DEF_BIT_29     /* Transferred block length is not allowed for card.    */
#define  FS_DEV_SD_CARD_R1_ADDRESS_ERROR         DEF_BIT_30     /* Misaligned address used in the command.              */
#define  FS_DEV_SD_CARD_R1_OUT_OF_RANGE          DEF_BIT_31     /* Command's argument was out of the allowed range.     */

                                                                /* Mask of card's state when receiving cmd.             */
#define  FS_DEV_SD_CARD_R1_CUR_STATE_MASK        DEF_BIT_FIELD(4,9)
                                                                /* Msk that covers any err that might be reported in R1.*/
#define  FS_DEV_SD_CARD_R1_ERR_ANY               DEF_BIT_FIELD(13,19)


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       SD CARD DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_sd_card_data  FS_DEV_SD_CARD_DATA;
struct  fs_dev_sd_card_data {
    FS_QTY                UnitNbr;
    CPU_BOOLEAN           Init;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR                StatRdCtr;
    FS_CTR                StatWrCtr;
#endif

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
    FS_CTR                ErrRdCtr;
    FS_CTR                ErrWrCtr;

    FS_CTR                ErrCardBusyCtr;
    FS_CTR                ErrCardUnknownCtr;
    FS_CTR                ErrCardWaitTimeoutCtr;
    FS_CTR                ErrCardRespTimeoutCtr;
    FS_CTR                ErrCardRespChksumCtr;
    FS_CTR                ErrCardRespCmdIxCtr;
    FS_CTR                ErrCardRespEndBitCtr;
    FS_CTR                ErrCardRespCtr;
    FS_CTR                ErrCardDataUnderrunCtr;
    FS_CTR                ErrCardDataOverrunCtr;
    FS_CTR                ErrCardDataTimeoutCtr;
    FS_CTR                ErrCardDataChksumCtr;
    FS_CTR                ErrCardDataStartBitCtr;
    FS_CTR                ErrCardDataCtr;
#endif

    FS_DEV_SD_INFO        Info;
    CPU_INT32U            RCA;
    CPU_INT32U            BlkCntMax;
    CPU_INT08U            BusWidth;

    FS_DEV_SD_CARD_DATA  *NextPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  FS_DEV_SD_CARD_NAME_LEN            6u

static  const  CPU_CHAR  FSDev_SD_Card_Name[] = "sdcard";


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       COMMAND RESPONSE TYPES
*
* Note(s) : (1) Detailed command descriptions are given in [Ref 1], Section 4.7.5, and [Ref 2], Section 7.9.4.
*
*           (2) MMC command names are enclosed in parenthesis when different from SD.
*********************************************************************************************************
*/

static  const  CPU_INT08U  FSDev_SD_Card_CmdRespType[] = {
    FS_DEV_SD_CARD_RESP_TYPE_NONE     |  FS_DEV_MMC_RESP_TYPE_NONE,      /* CMD0     GO_IDLE_STATE                */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R3       |  FS_DEV_MMC_RESP_TYPE_R3,        /* CMD1    (SEND_OP_COND)                */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R2       |  FS_DEV_MMC_RESP_TYPE_R2,        /* CMD2     ALL_SEND_CID                 */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R6       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD3     SEND_RELATIVE_ADDR           */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_NONE     |  FS_DEV_MMC_RESP_TYPE_NONE,      /* CMD4     SET_DSR                      */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD5     IO_SEND_OP_CMD               */  /*        SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD6    (SWITCH)                      */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD7     SELECT/DESELECT_CARD         */  /* SD MMC SDIO  */ /****/
    FS_DEV_SD_CARD_RESP_TYPE_R7       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD8     SEND_IF_COND  (SEND_EXT_CSD) */  /* SD           */ /****/
    FS_DEV_SD_CARD_RESP_TYPE_R2       |  FS_DEV_MMC_RESP_TYPE_R2,        /* CMD9     SEND_CSD                     */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R2       |  FS_DEV_MMC_RESP_TYPE_R2,        /* CMD10    SEND_CID                     */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD11   (READ_DAT_UNTIL_STOP)         */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD12    STOP_TRANSMISSION            */  /* SD MMC SDIO  */ /****/
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD13    SEND_STATUS                  */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD14   (BUSTEST_R)                   */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_NONE     |  FS_DEV_MMC_RESP_TYPE_NONE,      /* CMD15    GO_INACTIVE_STATE            */  /* SD MMC SDIO  */

    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD16    SET_BLOCKLEN                 */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD17    READ_SINGLE_BLOCK            */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD18    READ_MULTIPLE_BLOCK          */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD19   (BUSTEST_W)                   */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD20   (WRITE_DAT_UNTIL_STOP)        */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD23    SET_BLOCK_COUNT              */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD24    WRITE_BLOCK                  */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD25    WRITE_MULTIPLE_BLOCK         */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD26   (PROGRAM_CID)                 */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD27    PROGRAM_CSD                  */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD28    SET_WRITE_PROT               */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD29    CLR_WRITE_PROT               */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD30    SEND_WRITE_PROT              */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD31    (SEND_WRITE_PROT_TYPE)       */  /*    MMC       */

    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD32    ERASE_WR_BLK_START           */  /* SD     SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD33    ERASE_WR_BLK_END             */  /* SD     SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD35   (ERASE_GROUP_START)            */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD36   (ERASE_GROUP_END)              */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1B      |  FS_DEV_MMC_RESP_TYPE_R1B,       /* CMD38    ERASE                        */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R4       |  FS_DEV_MMC_RESP_TYPE_R4,        /* CMD39   (FAST_IO)                      */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_R5       |  FS_DEV_MMC_RESP_TYPE_R5,        /* CMD40   (GO_IRQ_STATE)                 */  /*    MMC       */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD42    LOCK_UNLOCK                  */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,

    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* CMD52    IO_RW_DIRECT            */  /*        SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* CMD53    IO_RW_EXTENDED          */  /*        SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD55    APP_CMD                 */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_R1,        /* CMD56    GEN_CMD                 */  /* SD MMC SDIO  */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,

    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD6    SET_BUS_WIDTH           */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD13  SD_STATUS               */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,

    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD22  SEND_NUM_WR_BLOCKS      */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD23  SEND_WR_BLK_ERASE_CNT   */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,

    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R3       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD41  SD_SEND_OP_COND         */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD42  SET_CLR_CARD_DETECT     */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,

    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_R1       |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,   /* ACMD51  SEND_SCR                */  /* SD           */
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
    FS_DEV_SD_CARD_RESP_TYPE_UNKNOWN  |  FS_DEV_MMC_RESP_TYPE_UNKNOWN,
};

static  const  FS_FLAGS  FSDev_SD_Card_CmdFlag[] = {
    FS_DEV_SD_CARD_CMD_FLAG_UNKNOWN,
    FS_DEV_SD_CARD_CMD_FLAG_NONE,
    FS_DEV_SD_CARD_CMD_FLAG_R1,
    FS_DEV_SD_CARD_CMD_FLAG_R1B,
    FS_DEV_SD_CARD_CMD_FLAG_R2,
    FS_DEV_SD_CARD_CMD_FLAG_R3,
    FS_DEV_SD_CARD_CMD_FLAG_R4,
    FS_DEV_SD_CARD_CMD_FLAG_R5,
    FS_DEV_SD_CARD_CMD_FLAG_R5B,
    FS_DEV_SD_CARD_CMD_FLAG_R6,
    FS_DEV_SD_CARD_CMD_FLAG_R7
};


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

static  FS_DEV_SD_CARD_DATA  *FSDev_SD_Card_ListFreePtr;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           COUNTER MACRO'S
*********************************************************************************************************
*/

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* ----------------- STAT CTR MACRO'S ----------------- */

#define  FS_DEV_SD_CARD_STAT_RD_CTR_INC(p_sd_card_data)          {  FS_CTR_STAT_INC((p_sd_card_data)->StatRdCtr);                }
#define  FS_DEV_SD_CARD_STAT_WR_CTR_INC(p_sd_card_data)          {  FS_CTR_STAT_INC((p_sd_card_data)->StatWrCtr);                }

#define  FS_DEV_SD_CARD_STAT_RD_CTR_ADD(p_sd_card_data, val)     {  FS_CTR_STAT_ADD((p_sd_card_data)->StatRdCtr, (FS_CTR)(val)); }
#define  FS_DEV_SD_CARD_STAT_WR_CTR_ADD(p_sd_card_data, val)     {  FS_CTR_STAT_ADD((p_sd_card_data)->StatWrCtr, (FS_CTR)(val)); }

#else

#define  FS_DEV_SD_CARD_STAT_RD_CTR_INC(p_sd_card_data)
#define  FS_DEV_SD_CARD_STAT_WR_CTR_INC(p_sd_card_data)

#define  FS_DEV_SD_CARD_STAT_RD_CTR_ADD(p_sd_card_data, val)
#define  FS_DEV_SD_CARD_STAT_WR_CTR_ADD(p_sd_card_data, val)

#endif


#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* ------------------ ERR CTR MACRO'S ----------------- */

#define  FS_DEV_SD_CARD_ERR_RD_CTR_INC(p_sd_card_data)           {  FS_CTR_ERR_INC((p_sd_card_data)->ErrRdCtr);                  }
#define  FS_DEV_SD_CARD_ERR_WR_CTR_INC(p_sd_card_data)           {  FS_CTR_ERR_INC((p_sd_card_data)->ErrWrCtr);                  }

#define  FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err)         FSDev_SD_Card_HandleErrBSP((p_sd_card_data), (err))

#else

#define  FS_DEV_SD_CARD_ERR_RD_CTR_INC(p_sd_card_data)
#define  FS_DEV_SD_CARD_ERR_WR_CTR_INC(p_sd_card_data)

#define  FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err)

#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* --------------- DEV INTERFACE FNCTS ---------------- */
                                                                /* Get name of driver.                                  */
static  const  CPU_CHAR      *FSDev_SD_Card_NameGet          (void);

                                                                /* Init driver.                                         */
static  void                  FSDev_SD_Card_Init             (FS_ERR                       *p_err);

                                                                /* Open device.                                         */
static  void                  FSDev_SD_Card_Open             (FS_DEV                       *p_dev,
                                                              void                         *p_dev_cfg,
                                                              FS_ERR                       *p_err);

                                                                /* Close device.                                        */
static  void                  FSDev_SD_Card_Close            (FS_DEV                       *p_dev);

                                                                /* Read from device.                                    */
static  void                  FSDev_SD_Card_Rd               (FS_DEV                       *p_dev,
                                                              void                         *p_dest,
                                                              FS_SEC_NBR                    start,
                                                              FS_SEC_QTY                    cnt,
                                                              FS_ERR                       *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Write to device.                                     */
static  void                  FSDev_SD_Card_Wr               (FS_DEV                       *p_dev,
                                                              void                         *p_src,
                                                              FS_SEC_NBR                    start,
                                                              FS_SEC_QTY                    cnt,
                                                              FS_ERR                       *p_err);
#endif

                                                                /* Get device info.                                     */
static  void                  FSDev_SD_Card_Query            (FS_DEV                       *p_dev,
                                                              FS_DEV_INFO                  *p_info,
                                                              FS_ERR                       *p_err);

                                                                /* Perform device I/O control.                          */
static  void                  FSDev_SD_Card_IO_Ctrl          (FS_DEV                       *p_dev,
                                                              CPU_INT08U                    opt,
                                                              void                         *p_data,
                                                              FS_ERR                       *p_err);


                                                                /* ------------------- LOCAL FNCTS -------------------- */
                                                                /* Refresh dev.                                         */
static  CPU_BOOLEAN           FSDev_SD_Card_Refresh          (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              FS_ERR                       *p_err);

                                                                /* Make card rdy & get card type                        */
static  CPU_INT08U            FSDev_SD_Card_MakeRdy          (FS_DEV_SD_CARD_DATA          *p_sd_card_data);



                                                                /* Perform cmd w/no resp.                               */
static  CPU_BOOLEAN           FSDev_SD_Card_Cmd              (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg);

                                                                /* Perform cmd w/short resp.                            */
static  CPU_BOOLEAN           FSDev_SD_Card_CmdRShort        (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT32U                   *p_resp);

                                                                /* Perform cmd w/long resp.                             */
static  CPU_BOOLEAN           FSDev_SD_Card_CmdRLong         (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT32U                   *p_resp);

                                                                /* Perform app cmd w/short resp.                        */
static  CPU_BOOLEAN           FSDev_SD_Card_AppCmdRShort     (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    acmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT32U                   *p_resp);

                                                                /* Exec cmd & rd data blk.                              */
static  CPU_BOOLEAN           FSDev_SD_Card_RdData           (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT08U                   *p_dest,
                                                              CPU_INT32U                    size,
                                                              CPU_INT32U                    cnt);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Exec cmd & wr data blk.                              */
static  CPU_BOOLEAN           FSDev_SD_Card_WrData           (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT08U                   *p_src,
                                                              CPU_INT32U                    size,
                                                              CPU_INT32U                    cnt);
#endif

                                                                /* Get CID reg.                                         */
static  CPU_BOOLEAN           FSDev_SD_Card_SendCID          (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                   *p_dest);

                                                                /* Get CSD reg.                                         */
static  CPU_BOOLEAN           FSDev_SD_Card_SendCSD          (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                   *p_dest);

                                                                /* Get EXT_CSD reg.                                     */
static  CPU_BOOLEAN           FSDev_SD_Card_SendEXT_CSD      (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                   *p_dest);

                                                                /* Get SCR reg.                                         */
static  CPU_BOOLEAN           FSDev_SD_Card_SendSCR          (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                   *p_dest);

                                                                /* Get SD status reg.                                   */
static  CPU_BOOLEAN           FSDev_SD_Card_SendSDStatus     (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                   *p_dest);

                                                                /* Set bus width.                                       */
static  CPU_INT08U            FSDev_SD_Card_SetBusWidth      (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_INT08U                    width);

                                                                /* Stop transmission.                                   */
static  CPU_BOOLEAN           FSDev_SD_Card_Stop             (FS_DEV_SD_CARD_DATA          *p_sd_card_data);

                                                                /* Sel card.                                            */
static  CPU_BOOLEAN           FSDev_SD_Card_Sel              (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              CPU_BOOLEAN                   sel);

                                                                /* Wait until card is rdy.                              */
static  CPU_BOOLEAN           FSDev_SD_Card_WaitWhileBusy    (FS_DEV_SD_CARD_DATA          *p_sd_card_data);

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
                                                                /* Handle BSP err.                                      */
static  void                  FSDev_SD_Card_HandleErrBSP     (FS_DEV_SD_CARD_DATA          *p_sd_card_data,
                                                              FS_DEV_SD_CARD_ERR            err);
#endif

                                                                /* Init SD card command.                                */
static  void                  FSDev_SD_Card_InitCmd          (FS_DEV_SD_CARD_CMD           *p_cmd,
                                                              CPU_INT08U                    cmd_nbr,
                                                              CPU_INT32U                    arg,
                                                              CPU_INT08U                    card_type);


                                                                /* Free SD card data.                                   */
static  void                  FSDev_SD_Card_DataFree         (FS_DEV_SD_CARD_DATA          *p_sd_card_data);

                                                                /* Allocate & init SD card data.                        */
static  FS_DEV_SD_CARD_DATA  *FSDev_SD_Card_DataGet          (void);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/


const  FS_DEV_API  FSDev_SD_Card = {
    FSDev_SD_Card_NameGet,
    FSDev_SD_Card_Init,
    FSDev_SD_Card_Open,
    FSDev_SD_Card_Close,
    FSDev_SD_Card_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_SD_Card_Wr,
#endif
    FSDev_SD_Card_Query,
    FSDev_SD_Card_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_QuerySD()
*
* Description : Get low-level information about SD/MMC card.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_info      Pointer to structure that will receive SD/MMC card information.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               SD/MMC info obtained.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_info' passed a NULL pointer.
*                               FS_ERR_DEV_INVALID        Argument 'name_dev' specifies an invalid device.
*
*                                                         --------- RETURNED BY FSDev_IO_Ctrl() ---------
*                               FS_ERR_DEV_NOT_OPEN       Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT    Device is not present.
*                               FS_ERR_DEV_IO             Device I/O error.
*                               FS_ERR_DEV_TIMEOUT        Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sdcard:0:").
*********************************************************************************************************
*/

void  FSDev_SD_Card_QuerySD (CPU_CHAR        *name_dev,
                             FS_DEV_SD_INFO  *p_info,
                             FS_ERR          *p_err)
{
    CPU_INT16S  cmp_val;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }

    if (p_info == (FS_DEV_SD_INFO *)0) {                        /* Validate info ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_Card_Name, FS_DEV_SD_CARD_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_CARD_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* -------------------- GET SD INFO ------------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_SD_QUERY,
                  (void *)p_info,
                          p_err);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_RdCID()
*
* Description : Read SD/MMC Card ID (CID) register.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_dest      Pointer to 16-byte buffer that will receive SD/MMC Card ID register.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               SD/MMC Card ID register read.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_dest' passed a NULL pointer.
*                               FS_ERR_DEV_INVALID        Argument 'name_dev' specifies an invalid device.
*
*                                                         --------- RETURNED BY FSDev_IO_Ctrl() ---------
*                               FS_ERR_DEV_NOT_OPEN       Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT    Device is not present.
*                               FS_ERR_DEV_IO             Device I/O error.
*                               FS_ERR_DEV_TIMEOUT        Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sdcard:0:").
*
*               (2) (a) For SD cards, the structure of the CID is defined in [Ref 1] Section 5.1.
*
*                   (b) For MMC cards, the structure of the CID is defined in [Ref 2] Section 8.2.
*********************************************************************************************************
*/

void  FSDev_SD_Card_RdCID (CPU_CHAR    *name_dev,
                           CPU_INT08U  *p_dest,
                           FS_ERR      *p_err)
{
    CPU_INT16S  cmp_val;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }

    if (p_dest == (CPU_INT08U *)0) {                            /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_Card_Name, FS_DEV_SD_CARD_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_CARD_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* -------------------- GET SD INFO ------------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_SD_RD_CID,
                  (void *)p_dest,
                          p_err);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_RdCSD()
*
* Description : Read SD/MMC Card-Specific Data (CSD) register.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_dest      Pointer to 16-byte buffer that will receive SD/MMC Card ID register.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               SD/MMC Card-Specific Data register read.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_dest' passed a NULL pointer.
*                               FS_ERR_DEV_INVALID        Argument 'name_dev' specifies an invalid device.
*
*                                                         --------- RETURNED BY FSDev_IO_Ctrl() ---------
*                               FS_ERR_DEV_NOT_OPEN       Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT    Device is not present.
*                               FS_ERR_DEV_IO             Device I/O error.
*                               FS_ERR_DEV_TIMEOUT        Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sdcard:0:").
*
*               (2) (a) For SD v1.x & SD v2.0 standard capacity, the structure of the CSD is defined in
*                       [Ref 1], Section 5.3.2.
*
*                   (b) For MMC cards, the structure of the CSD is defined in [Ref 2], Section 8.3.
*
*                   (c) For SD v2.0 high capacity cards, the structure of the CSD is defined in [Ref 1],
*                       Section 5.3.3.
*********************************************************************************************************
*/

void  FSDev_SD_Card_RdCSD (CPU_CHAR    *name_dev,
                           CPU_INT08U  *p_dest,
                           FS_ERR      *p_err)
{
    CPU_INT16S  cmp_val;


                                                                /* ------------------- VALIDATE ARGS ------------------ */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }

    if (p_dest == (CPU_INT08U *)0) {                            /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_Card_Name, FS_DEV_SD_CARD_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_CARD_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* -------------------- GET SD INFO ------------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_SD_RD_CSD,
                  (void *)p_dest,
                          p_err);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FSDev_SD_Card_NameGet()
*
* Description : Return name of device driver.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to string which holds device driver name.
*
* Note(s)     : (1) The name MUST NOT include the ':' character.
*
*               (2) The name MUST be constant; each time this function is called, the same name MUST be
*                   returned.
*********************************************************************************************************
*/

static  const  CPU_CHAR  *FSDev_SD_Card_NameGet (void)
{
    return (FSDev_SD_Card_Name);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_Card_Init()
*
* Description : Initialize the driver.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    Device driver initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should initialize any structures, tables or variables that are common
*                   to all devices or are used to manage devices accessed with the driver.  This function
*                   SHOULD NOT initialize any devices; that will be done individually for each with
*                   device driver's 'Open()' function.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_Init (FS_ERR  *p_err)
{
    FSDev_SD_Card_UnitCtr     =  0u;
    FSDev_SD_Card_ListFreePtr = (FS_DEV_SD_CARD_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_Open()
*
* Description : Open (initialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to open.
*
*               p_dev_cfg   Pointer to device configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Device opened successfully.
*                               FS_ERR_DEV_ALREADY_OPEN        Device is already opened.
*                               FS_ERR_DEV_INVALID_CFG         Device configuration specified invalid.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_INVALID_LOW_PARAMS  Device low-level device parameters invalid.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*                               FS_ERR_MEM_ALLOC               Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should NEVER be
*                   called when a device is already open.
*
*               (2) Some drivers may need to track whether a device has been previously opened
*                   (indicating that the hardware has previously been initialized).
*
*               (3) This function will be called EVERY time the device is opened.
*
*               (4) See 'FSDev_Open() Notes #3'.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_Open (FS_DEV  *p_dev,
                                  void    *p_dev_cfg,
                                  FS_ERR  *p_err)
{
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;


   (void)p_dev_cfg;                                             /*lint --e{550} Suppress "Symbol not accessed".         */

    p_sd_card_data = FSDev_SD_Card_DataGet();
    if (p_sd_card_data == (FS_DEV_SD_CARD_DATA *)0) {
      *p_err = FS_ERR_MEM_ALLOC;
       return;
    }

    p_dev->DataPtr          = (void *)p_sd_card_data;
    p_sd_card_data->UnitNbr =  p_dev->UnitNbr;

    (void)FSDev_SD_Card_Refresh(p_sd_card_data, p_err);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_Close()
*
* Description : Close (uninitialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to close.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) This function will be called EVERY time the device is closed.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_Close (FS_DEV  *p_dev)
{
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;


    p_sd_card_data = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
    if (p_sd_card_data->Init == DEF_YES) {
        FSDev_SD_Card_BSP_Close(p_sd_card_data->UnitNbr);
    }
    FSDev_SD_Card_DataFree(p_sd_card_data);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_Card_Rd()
*
* Description : Read from a device & store data in buffer.
*
* Argument(s) : p_dev       Pointer to device to read from.
*
*               p_dest      Pointer to destination buffer.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) read.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) Standard-capacity devices receive the start byte address as the argument of the read
*                   command, limiting device access to 4-GB (the range of a 32-bit variable).  To solve
*                   that problem, high-capacity devices (like SDHC cards) receive the block number as the
*                   argument of the read command.
*
*               (3) A multiple block transfer will be performed if more than one sector is to be read and
*                   the maximum block count, returned by FSDev_SD_Card_BSP_GetMaxBlkCnt(),  is greater
*                   than 1.  If the controller is not capable of multiple block reads or writes, that
*                   function should return 1.
*
*               (4) The card is always re-selected after the lock is acquired, in case multiple cards
*                   are located on the same bus.  Since only one card may be in use (or selected) at any
*                   time, the lock can be held by only one device.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_Rd (FS_DEV      *p_dev,
                                void        *p_dest,
                                FS_SEC_NBR   start,
                                FS_SEC_QTY   cnt,
                                FS_ERR      *p_err)
{
    CPU_INT32U            blk_cnt;
    CPU_INT08U            cmd;
    CPU_INT32U            addr_inc;
    CPU_BOOLEAN           ok;
    CPU_INT32U            start_addr;
    CPU_INT32U            retries;
    CPU_INT08U           *p_dest_08;
    FS_DEV_SD_INFO       *p_sd_info;
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;


    p_dest_08      = (CPU_INT08U          *)p_dest;
    p_sd_card_data = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
    p_sd_info      = &p_sd_card_data->Info;
                                                                /* See Note #2.                                         */
    start_addr     = (p_sd_info->HighCapacity == DEF_YES) ? start : (start * FS_DEV_SD_BLK_SIZE);
    addr_inc       = (p_sd_info->HighCapacity == DEF_YES) ? 1u    : FS_DEV_SD_BLK_SIZE;

    retries        =  FS_DEV_SD_CARD_RETRIES_DATA;



                                                                /* ----------------- ACQUIRE BUS LOCK ----------------- */
    FSDev_SD_Card_BSP_Lock(p_sd_card_data->UnitNbr);
    ok = FSDev_SD_Card_Sel(p_sd_card_data, DEF_YES);            /* Sel card (see Note #4).                              */



    while (cnt > 0u) {
                                                                /* -------------- WAIT UNTIL CARD IS RDY -------------- */
        ok = FSDev_SD_Card_WaitWhileBusy(p_sd_card_data);
        if (ok != DEF_OK) {
            FS_OS_Dly_ms(2u);
            ok = FSDev_SD_Card_WaitWhileBusy(p_sd_card_data);
            if (ok != DEF_OK) {
                FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                FS_DEV_SD_CARD_ERR_RD_CTR_INC(p_sd_card_data);
                FS_TRACE_DBG(("FSDev_SD_Card_Rd(): Failed to rd card: card did not become rdy.\r\n"));
               *p_err = FS_ERR_DEV_IO;
                return;
            }
        }



                                                                /* ---------------- PERFORM MULTIPLE RD --------------- */
        blk_cnt = DEF_MIN(p_sd_card_data->BlkCntMax, cnt);      /* See Note #3.                                         */
        cmd     = (blk_cnt > 1u) ? FS_DEV_SD_CMD_READ_MULTIPLE_BLOCK : FS_DEV_SD_CMD_READ_SINGLE_BLOCK;

        ok = FSDev_SD_Card_RdData(p_sd_card_data,
                                  cmd,
                                  start_addr,
                                  p_dest_08,
                                  FS_DEV_SD_BLK_SIZE,
                                  blk_cnt);

        if (ok != DEF_OK) {
            if (retries == 0u) {
                FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                FS_DEV_SD_CARD_ERR_RD_CTR_INC(p_sd_card_data);
                FS_TRACE_DBG(("FSDev_SD_Card_Rd(): Failed to rd card.\r\n"));
               *p_err = FS_ERR_DEV_IO;
                return;
            }

            FS_TRACE_DBG(("FSDev_SD_Card_Rd(): Failed to rd card ... retrying.\r\n"));
            retries--;



        } else {
            if (blk_cnt > 1u) {                                 /* ---------------- STOP TRANSMISSION ----------------- */
                ok = FSDev_SD_Card_Stop(p_sd_card_data);

                if (ok != DEF_OK) {
                    FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                    FS_DEV_SD_CARD_ERR_RD_CTR_INC(p_sd_card_data);
                    FS_TRACE_DBG(("FSDev_SD_Card_Rd(): Failed to rd card: could not stop transmission.\r\n"));
                   *p_err = FS_ERR_DEV_IO;
                    return;
                }
            }

            FS_DEV_SD_CARD_STAT_RD_CTR_ADD(p_sd_card_data, blk_cnt);

            p_dest_08  += blk_cnt * FS_DEV_SD_BLK_SIZE;
            start_addr += blk_cnt * addr_inc;
            cnt        -= blk_cnt;

            retries     = FS_DEV_SD_CARD_RETRIES_DATA;
        }
    }



                                                                /* ----------------- RELEASE BUS LOCK ----------------- */
    FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_SD_Card_Wr()
*
* Description : Write data to a device from a buffer.
*
* Argument(s) : p_dev       Pointer to device to write to.
*
*               p_src       Pointer to source buffer.
*
*               start       Start sector of write.
*
*               cnt         Number of sectors to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) Standard-capacity devices receive the start byte address as the argument of the write
*                   command, limiting device access to 4-GB (the range of a 32-bit variable).  To solve
*                   that problem, high-capacity devices (like SDHC cards) receive the block number as the
*                   argument of the write command.
*
*               (3) A multiple block transfer will be performed if more than one sector is to be read and
*                   the maximum block count, returned by FSDev_SD_Card_BSP_GetMaxBlkCnt(),  is greater
*                   than 1.  If the controller is not capable of multiple block reads or writes, that
*                   function should return 1.
*
*               (4) The card is always re-selected after the lock is acquired, in case multiple cards
*                   are located on the same bus.  Since only one card may be in use (or selected) at any
*                   time, the lock can be held by only one device.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_SD_Card_Wr (FS_DEV      *p_dev,
                                void        *p_src,
                                FS_SEC_NBR   start,
                                FS_SEC_QTY   cnt,
                                FS_ERR      *p_err)
{
    CPU_INT32U            blk_cnt;
    CPU_INT08U            cmd;
    CPU_INT32U            addr_inc;
    CPU_BOOLEAN           ok;
    CPU_INT32U            start_addr;
    CPU_INT32U            retries;
    CPU_INT08U           *p_src_08;
    FS_DEV_SD_INFO       *p_sd_info;
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;


    p_src_08       = (CPU_INT08U          *)p_src;
    p_sd_card_data = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
    p_sd_info      = &p_sd_card_data->Info;
                                                                /* See Note #2.                                         */
    start_addr     = (p_sd_info->HighCapacity == DEF_YES) ? start : (start * FS_DEV_SD_BLK_SIZE);
    addr_inc       = (p_sd_info->HighCapacity == DEF_YES) ? 1u    : FS_DEV_SD_BLK_SIZE;

    retries        =  FS_DEV_SD_CARD_RETRIES_DATA;


                                                                /* ----------------- ACQUIRE BUS LOCK ----------------- */
    FSDev_SD_Card_BSP_Lock(p_sd_card_data->UnitNbr);
    ok = FSDev_SD_Card_Sel(p_sd_card_data, DEF_YES);            /* Sel card (see Note #4).                              */



    while (cnt > 0u) {
                                                                /* -------------- WAIT UNTIL CARD IS RDY -------------- */
        ok = FSDev_SD_Card_WaitWhileBusy(p_sd_card_data);
        if (ok != DEF_OK) {
            FS_OS_Dly_ms(2u);
            ok = FSDev_SD_Card_WaitWhileBusy(p_sd_card_data);
            if (ok != DEF_OK) {
                FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                FS_DEV_SD_CARD_ERR_WR_CTR_INC(p_sd_card_data);
                FS_TRACE_DBG(("FSDev_SD_Card_Wr(): Failed to wr card: card did not become rdy.\r\n"));
               *p_err = FS_ERR_DEV_IO;
                return;
            }
        }



                                                                /* ---------------- PERFORM MULTIPLE WR --------------- */
        blk_cnt = DEF_MIN(p_sd_card_data->BlkCntMax, cnt);      /* See Note #3.                                         */
        cmd     = (blk_cnt > 1u) ? FS_DEV_SD_CMD_WRITE_MULTIPLE_BLOCK : FS_DEV_SD_CMD_WRITE_BLOCK;

        ok = FSDev_SD_Card_WrData(p_sd_card_data,
                                  cmd,
                                  start_addr,
                                  p_src_08,
                                  FS_DEV_SD_BLK_SIZE,
                                  blk_cnt);

        if (ok != DEF_OK) {
            if (retries == 0u) {
                FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                FS_DEV_SD_CARD_ERR_WR_CTR_INC(p_sd_card_data);
                FS_TRACE_DBG(("FSDev_SD_Card_Wr(): Failed to wr card.\r\n"));
               *p_err = FS_ERR_DEV_IO;
                return;
            }

            FS_TRACE_DBG(("FSDev_SD_Card_Wr(): Failed to wr card ... retrying.\r\n"));
            retries--;



        } else {
            if (blk_cnt > 1u) {                                 /* ---------------- STOP TRANSMISSION ----------------- */
                ok = FSDev_SD_Card_Stop(p_sd_card_data);

                if (ok != DEF_OK) {
                    FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
                    FS_DEV_SD_CARD_ERR_WR_CTR_INC(p_sd_card_data);
                    FS_TRACE_DBG(("FSDev_SD_Card_Wr(): Failed to wr card: could not stop transmission.\r\n"));
                   *p_err = FS_ERR_DEV_IO;
                    return;
                }
            }

            FS_DEV_SD_CARD_STAT_WR_CTR_ADD(p_sd_card_data, blk_cnt);

            p_src_08   += blk_cnt * FS_DEV_SD_BLK_SIZE;
            start_addr += blk_cnt * addr_inc;
            cnt        -= blk_cnt;

            retries     = FS_DEV_SD_CARD_RETRIES_DATA;
        }
    }



                                                                /* ----------------- RELEASE BUS LOCK ----------------- */
    FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_Query()
*
* Description : Get information about a device.
*
* Argument(s) : p_dev       Pointer to device to query.
*
*               p_info      Pointer to structure that will receive device information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Device information obtained.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_Query (FS_DEV       *p_dev,
                                   FS_DEV_INFO  *p_info,
                                   FS_ERR       *p_err)
{
    FS_DEV_SD_INFO       *p_sd_info;
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;


    p_sd_card_data  = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
    p_sd_info       = &p_sd_card_data->Info;

    p_info->SecSize =  FS_DEV_SD_BLK_SIZE;
    p_info->Fixed   =  DEF_NO;

    switch (p_sd_info->BlkSize) {
        case 512u:
             p_info->Size = p_sd_info->NbrBlks;
             break;

        case 1024u:
             p_info->Size = p_sd_info->NbrBlks * 2u;
             break;

        case 2048u:
             p_info->Size = p_sd_info->NbrBlks * 4u;
             break;

        default:
             p_info->Size = 0u;
             break;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_IO_Ctrl()
*
* Description : Perform device I/O control operation.
*
* Argument(s) : p_dev       Pointer to device to control.
*
*               opt         Control command.
*
*               p_data      Buffer which holds data to be used for operation.
*                              OR
*                           Buffer in which data will be stored as a result of operation.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE                    Control operation performed successfully.
*                               FS_ERR_DEV_INVALID_IO_CTRL     I/O control operation unknown to driver.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) Defined I/O control operations are :
*
*                   (a) FS_DEV_IO_CTRL_REFRESH           Refresh device.
*                   (b) FS_DEV_IO_CTRL_LOW_FMT           Low-level format device.      [*]
*                   (c) FS_DEV_IO_CTRL_LOW_MOUNT         Low-level mount device.       [*]
*                   (d) FS_DEV_IO_CTRL_LOW_UNMOUNT       Low-level unmount device.     [*]
*                   (e) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level compact device.     [*]
*                   (f) FS_DEV_IO_CTRL_LOW_DEFRAG        Low-level defragment device.  [*]
*                   (g) FS_DEV_IO_CTRL_SEC_RELEASE       Release data in sector.       [*]
*                   (h) FS_DEV_IO_CTRL_PHY_RD            Read  physical device.        [*]
*                   (i) FS_DEV_IO_CTRL_PHY_WR            Write physical device.        [*]
*                   (j) FS_DEV_IO_CTRL_PHY_RD_PAGE       Read  physical device page.   [*]
*                   (k) FS_DEV_IO_CTRL_PHY_WR_PAGE       Write physical device page.   [*]
*                   (l) FS_DEV_IO_CTRL_PHY_ERASE_BLK     Erase physical device block.  [*]
*                   (m) FS_DEV_IO_CTRL_PHY_ERASE_CHIP    Erase physical device.        [*]
*
*                   (n) FS_DEV_IO_CTRL_SD_QUERY          Get info about SD/MMC card.
*                   (o) FS_DEV_IO_CTRL_SD_RD_CID         Read SD/MMC card Card ID register.
*                   (p) FS_DEV_IO_CTRL_SD_RD_CSD         Read SD/MMC card Card-Specific Data register.
*
*                           [*] NOT SUPPORTED
*
*               (3) Card selection does NOT need to occur prior to reading the CID or CSD registers;
*                   indeed, the card must NOT be selected (e.g., it must be in the 'stby' state).  See
*                   also 'FSDev_SD_Card_SendCID()  Note #3a' & 'FSDev_SD_Card_SendCSD()  Note #3'.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_IO_Ctrl (FS_DEV      *p_dev,
                                     CPU_INT08U   opt,
                                     void        *p_data,
                                     FS_ERR      *p_err)
{
    FS_DEV_SD_INFO       *p_sd_info;
    FS_DEV_SD_INFO       *p_sd_info_ctrl;
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;
    CPU_BOOLEAN           ok;
    CPU_BOOLEAN           chngd;
    CPU_BOOLEAN          *p_chngd;


    switch (opt) {
        case FS_DEV_IO_CTRL_REFRESH:                            /* -------------------- RE-OPEN DEV ------------------- */
             p_chngd        = (CPU_BOOLEAN *)p_data;
             p_sd_card_data = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
             chngd          = FSDev_SD_Card_Refresh(p_sd_card_data, p_err);
            *p_chngd        = chngd;
             break;


        case FS_DEV_IO_CTRL_SD_QUERY:                           /* ------------ GET INFO ABOUT SD/MMC CARD ------------ */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
            if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif
                                                                /* Validate dev init.                                   */
             p_sd_card_data  = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
             if (p_sd_card_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }
                                                                /* Copy info.                                           */
             p_sd_info                    = &p_sd_card_data->Info;
             p_sd_info_ctrl               = (FS_DEV_SD_INFO *)p_data;

             p_sd_info_ctrl->BlkSize      = p_sd_info->BlkSize;
             p_sd_info_ctrl->NbrBlks      = p_sd_info->NbrBlks;
             p_sd_info_ctrl->ClkFreq      = p_sd_info->ClkFreq;
             p_sd_info_ctrl->Timeout      = p_sd_info->Timeout;
             p_sd_info_ctrl->CardType     = p_sd_info->CardType;
             p_sd_info_ctrl->HighCapacity = p_sd_info->HighCapacity;
             p_sd_info_ctrl->ManufID      = p_sd_info->ManufID;
             p_sd_info_ctrl->OEM_ID       = p_sd_info->OEM_ID;
             p_sd_info_ctrl->ProdSN       = p_sd_info->ProdSN;
             Mem_Copy(p_sd_info_ctrl->ProdName, p_sd_info->ProdName, 7u);
             p_sd_info_ctrl->ProdRev      = p_sd_info->ProdRev;
             p_sd_info_ctrl->Date         = p_sd_info->Date;
            *p_err = FS_ERR_NONE;
             break;


        case FS_DEV_IO_CTRL_SD_RD_CID:                          /* --------------- READ CARD ID REGISTER -------------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                                 /* Validate data ptr.                           */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif
                                                                        /* Validate dev init.                           */
             p_sd_card_data  = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
             if (p_sd_card_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }

             FSDev_SD_Card_BSP_Lock(p_sd_card_data->UnitNbr);           /* Acquire bus lock.                            */
             ok = FSDev_SD_Card_SendCID(              p_sd_card_data,   /* Get CID reg from card (see Note #3).         */
                                        (CPU_INT08U *)p_data);
             FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);         /* Release bus lock.                            */

             if (ok != DEF_OK) {
                *p_err = FS_ERR_DEV_IO;
                 return;
             }

            *p_err = FS_ERR_NONE;
             break;


        case FS_DEV_IO_CTRL_SD_RD_CSD:                          /* --------- READ CARD-SPECIFIC DATA REGISTER --------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                                 /* Validate data ptr.                           */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif

                                                                        /* Validate dev init.                           */
             p_sd_card_data  = (FS_DEV_SD_CARD_DATA *)p_dev->DataPtr;
             if (p_sd_card_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }

             FSDev_SD_Card_BSP_Lock(p_sd_card_data->UnitNbr);           /* Acquire bus lock.                            */
             ok = FSDev_SD_Card_SendCSD(              p_sd_card_data,   /* Get CSD reg from card (see Note #3).         */
                                        (CPU_INT08U *)p_data);
             FSDev_SD_Card_BSP_Unlock(p_sd_card_data->UnitNbr);         /* Release bus lock.                            */

             if (ok != DEF_OK) {
                *p_err = FS_ERR_DEV_IO;
                 return;
             }

            *p_err = FS_ERR_NONE;
             break;


                                                                /* --------------- UNSUPPORTED I/O CTRL --------------- */
        case FS_DEV_IO_CTRL_LOW_FMT:
        case FS_DEV_IO_CTRL_LOW_UNMOUNT:
        case FS_DEV_IO_CTRL_LOW_MOUNT:
        case FS_DEV_IO_CTRL_LOW_COMPACT:
        case FS_DEV_IO_CTRL_LOW_DEFRAG:
        case FS_DEV_IO_CTRL_SEC_RELEASE:
        case FS_DEV_IO_CTRL_PHY_RD:
        case FS_DEV_IO_CTRL_PHY_WR:
        case FS_DEV_IO_CTRL_PHY_WR_PAGE:
        case FS_DEV_IO_CTRL_PHY_RD_PAGE:
        case FS_DEV_IO_CTRL_PHY_ERASE_BLK:
        case FS_DEV_IO_CTRL_PHY_ERASE_CHIP:
        default:
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
             break;
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FSDev_SD_Card_Refresh()
*
* Description : Refresh a device.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                    Device opened successfully.
*                                   FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                                   FS_ERR_DEV_IO                  Device I/O error.
*                                   FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                                   FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : DEF_YES, if device has     changed.
*               DEF_NO,  if device has NOT changed.
*
* Note(s)     : (1) The initialization flow for a SD card is charted in [Ref 1] Figure 4-2.  Similarly,
*                   the power-up sequence for a MMC card is given in Section 1.11 of [Ref 2].
*
*
*                                                    POWER ON
*                                                        |
*                                                        |
*                                                        v
*                                                      CMD0
*                                                        |
*                                                        |
*                               NO RESPONSE              v             CARD RETURNS RESPONSE
*                                      +---------------CMD8---------------+
*                                      |                                  |
*                                      |                                  |
*                                      v                                  v             NO
*                 NO RESPONSE       ACMD41                            RESPONSE------------------>UNUSABLE
*            +--------------------w/ HCS = 0<---------+                VALID?                      CARD
*            |                         |              |                   |
*            |                         |              | BUSY              |
*            |                         v              |                   v
*            |                   CARD IS READY?-------+                 ACMD41
*            |                         |                     +------->w/ HCS = 1
*            |                         | YES                 |             |          TIMEOUT
*            |                         |                BUSY |             |          OR NO
*            |                         |                     |             v          RESPONSE
*            |                         |                     +-------CARD IS READY?------------->UNUSABLE
*          CMD1<------------+          |                                   |                       CARD
*            |              |          |                                   |
* NO         |              | BUSY     |                                   |
* RESP       v              |          |                                   | YES
*  +---CARD IS READY?-------+          |                                   |
*  |         |                         |                                   |
*  |         | YES                     |                                   |
*  v         |                         |                     NO            v             YES
* UNUSABLE   |                         |               +----------- CCS IN RESPONSE? -----------+
*   CARD     |                         |               |                                        |
*            v                         v               v                                        v
*           MMC                  VER 1.x STD      VER 2.0+ STD                            VER 2.0+ HIGH
*            |                   CAPACITY SD      CAPACITY SD                              CAPACITY SD
*            |                         |               |                                        |
*            |                         |               |                                        |
*            +-------------------------+---------------+----------------------------------------+
*                                                      |
*                                                      |
*                                                      v
*                                                    CMD2
*                                                      |
*                                                      |
*                                                      v
*                                                    CMD3
*
*               (2) After the type of card is determined, the card's Relative Card Address (RCA) must be
*                   identified or assigned.
*
*                   (a) For SD cards, CMD2 is issued; in response, "[c]ard that is unidentified ... sends
*                       its CID number as the response" and "goes into the Identification state".  CMD3
*                       is then issued to get that card's RCA, which is "used to address the card in the
*                       future data transfer state".
*
*                   (b) For MMC cards, CMD2 is issued; "there should be only one card which successfully
*                       sends its full CID-number to the host".  This card "goes into Identification state".
*                       CMD3 is then issued "to assign to this card a relative card address (RCA) ...
*                       which will be used to address the card in the future data transfer mode".
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_Refresh (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                            FS_ERR               *p_err)
{
    CPU_INT08U       card_type;
    CPU_INT16U       retries;
    CPU_INT32U       rca;
    CPU_INT08U       csd[FS_DEV_SD_CSD_REG_LEN];
    CPU_INT08U       cid[FS_DEV_SD_CID_REG_LEN];
    CPU_INT32U       resp1;
    CPU_INT32U       resp6;
    CPU_INT08U       width;
    CPU_BOOLEAN      ok;
    CPU_BOOLEAN      init_prev;
    FS_QTY           unit_nbr;
    FS_DEV_SD_INFO  *p_sd_info;
    FS_DEV_SD_INFO   sd_info;
    FS_BUF          *p_buf;


                                                                /* ------------------- CHK DEV CHNGD ------------------ */
    p_sd_info = &(p_sd_card_data->Info);
    unit_nbr  = p_sd_card_data->UnitNbr;
    if (p_sd_card_data->Init == DEF_YES) {                      /* If dev already init'd ...                            */
        FSDev_SD_ClrInfo(&sd_info);

        FSDev_SD_Card_BSP_Lock(unit_nbr);
        ok = FSDev_SD_Card_SendCID( p_sd_card_data,             /* ... rd CID reg        ...                            */
                                   &cid[0]);
        FSDev_SD_Card_BSP_Unlock(unit_nbr);

        if (ok == DEF_YES) {                                    /* ... parse CID info    ...                            */
            (void)FSDev_SD_ParseCID( cid,
                                    &sd_info,
                                     p_sd_info->CardType);


            if ((sd_info.ManufID == p_sd_info->ManufID) &&      /* ... & if info is same ...                            */
                (sd_info.OEM_ID  == p_sd_info->OEM_ID)  &&
                (sd_info.ProdSN  == p_sd_info->ProdSN)) {
               *p_err = FS_ERR_NONE;                            /* ... dev has not chngd.                               */
                return (DEF_NO);
            }
        }

        FSDev_SD_Card_BSP_Close(unit_nbr);                      /* Uninit HW.                                           */
    }



                                                                /* ---------------------- INIT HW --------------------- */
    init_prev                 = p_sd_card_data->Init;           /* If init'd prev'ly, dev chngd.                        */
    p_sd_card_data->Init      = DEF_NO;                         /* No longer init'd.                                    */
    FSDev_SD_ClrInfo(p_sd_info);                                /* Clr old info.                                        */
    p_sd_card_data->RCA       = 0u;
    p_sd_card_data->BlkCntMax = 0u;
    p_sd_card_data->BusWidth  = 1u;

    ok = FSDev_SD_Card_BSP_Open(unit_nbr);                      /* Init HW.                                             */
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return (init_prev);
    }

    FSDev_SD_Card_BSP_Lock(unit_nbr);                           /* Acquire bus lock.                                    */

                                                                /* Set clk to dflt clk.                                 */
    FSDev_SD_Card_BSP_SetClkFreq(unit_nbr, (CPU_INT32U)FS_DEV_SD_DFLT_CLK_SPD);
    FSDev_SD_Card_BSP_SetTimeoutData(unit_nbr, 0xFFFFFFFFuL);
    FSDev_SD_Card_BSP_SetTimeoutResp(unit_nbr, 1000u);
    FSDev_SD_Card_BSP_SetBusWidth(unit_nbr, 1u);



                                                                 /* -------------- SEND CMD0 TO RESET BUS -------------- */
    retries = 0u;
    ok      = DEF_FAIL;
    while ((retries <  FS_DEV_SD_CARD_RETRIES_CMD) &&
           (ok      != DEF_OK)) {
        ok = FSDev_SD_Card_Cmd(p_sd_card_data,
                               FS_DEV_SD_CMD_GO_IDLE_STATE,
                               0u);

        if (ok != DEF_OK) {
            FS_OS_Dly_ms(2u);
            retries++;
        }
    }

    if (ok != DEF_OK) {                                         /* If card never responded ...                          */
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;                                  /* ... rtn err.                                         */
        return (init_prev);
    }



                                                                /* ---------------- DETERMINE CARD TYPE --------------- */
    card_type = FSDev_SD_Card_MakeRdy(p_sd_card_data);          /* Make card rd & determine card type.                  */
    if (card_type == FS_DEV_SD_CARDTYPE_NONE) {                 /* If card type NOT determined ...                      */
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;                                   /* ... rtn err.                                         */
        return (init_prev);
    }

    p_sd_info->CardType = card_type;

    if ((card_type == FS_DEV_SD_CARDTYPE_SD_V2_0_HC) ||
        (card_type == FS_DEV_SD_CARDTYPE_MMC_HC)) {
        p_sd_info->HighCapacity = DEF_YES;
    }


                                                                /* ----------- SET OR GET RCA (see Note #2) ----------- */
    ok = FSDev_SD_Card_SendCID( p_sd_card_data,                 /* Get CID reg from card.                               */
                               &cid[0]);
    if (ok != DEF_OK) {
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    (void)FSDev_SD_ParseCID(cid,                                /* Parse CID info.                                      */
                            p_sd_info,
                            card_type);

                                                                /* Perform CMD3 (R6 resp).                              */
    switch (card_type) {
        case FS_DEV_SD_CARDTYPE_MMC:
        case FS_DEV_SD_CARDTYPE_MMC_HC:
             rca = (CPU_INT32U)FS_DEV_SD_CARD_RCA_DFLT << 16;   /* For MMC cards, RCA assigned by host.                 */
             ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,
                                           FS_DEV_SD_CMD_SEND_RELATIVE_ADDR,
                                           rca,
                                          &resp6);
             break;


        case FS_DEV_SD_CARDTYPE_SD_V1_X:
        case FS_DEV_SD_CARDTYPE_SD_V2_0:
        case FS_DEV_SD_CARDTYPE_SD_V2_0_HC:
        default:
             ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,
                                           FS_DEV_SD_CMD_SEND_RELATIVE_ADDR,
                                           0u,
                                          &resp6);
             rca = resp6 & 0xFFFF0000uL;                        /* For SD cards, RCA returned by card.                  */
             break;
    }

    if (ok != DEF_OK) {
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    p_sd_card_data->RCA = rca;                                  /* Save RCA.                                            */



                                                                /* -------------------- RD CSD REG -------------------- */
    ok = FSDev_SD_Card_SendCSD( p_sd_card_data,                 /* Get CSD reg from card.                               */
                               &csd[0]);
    if (ok != DEF_OK) {
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err  = FS_ERR_DEV_IO;
        return (init_prev);
    }

    ok = FSDev_SD_ParseCSD(csd,                                 /* Parse CSD info.                                      */
                           p_sd_info,
                           card_type);
    if (ok != DEF_OK) {
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    FSDev_SD_Card_BSP_SetClkFreq(unit_nbr, p_sd_info->ClkFreq); /* Set clk freq.                                        */

    ok = FSDev_SD_Card_Sel(p_sd_card_data, DEF_YES);
    if (ok != DEF_OK) {
        FSDev_SD_Card_BSP_Unlock(unit_nbr);
        FSDev_SD_Card_BSP_Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    p_sd_card_data->Init = DEF_YES;



                                                                /* -------------------- SET BLK LEN ------------------- */
    ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,               /* Perform CMD16 (R1 resp).                             */
                                  FS_DEV_SD_CMD_SET_BLOCKLEN,
                                  FS_DEV_SD_BLK_SIZE,
                                 &resp1);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (ok != DEF_OK) {
        FS_TRACE_INFO(("FSDev_SD_Card_Refresh(): Could not set block length.\r\n"));
    }
#endif

    p_sd_card_data->BlkCntMax = FSDev_SD_Card_BSP_GetBlkCntMax(unit_nbr, FS_DEV_SD_BLK_SIZE);

    if (p_sd_card_data->BlkCntMax < 1u) {
        p_sd_card_data->BlkCntMax = 1u;
    }



                                                                /* ------------------- SET BUS WIDTH ------------------ */
    FSDev_SD_Card_BSP_SetBusWidth(unit_nbr, 1u);
    p_sd_card_data->BusWidth = 1u;
    width = FSDev_SD_Card_BSP_GetBusWidthMax(p_sd_card_data->UnitNbr);
    if (width < 1u) {
        width = 1u;
    }
    width = FSDev_SD_Card_SetBusWidth(p_sd_card_data, width);
    FS_TRACE_INFO(("FSDev_SD_Card_Refresh(): Bus width set to %d.\r\n", width));
    p_sd_card_data->BusWidth = width;


                                                                /* --------------- READ EXT_CSD (MMC-HC) -------------- */
    if(card_type == FS_DEV_SD_CARDTYPE_MMC_HC) {
        p_buf = FSBuf_Get(DEF_NULL);
        if(p_buf == DEF_NULL) {
            FSDev_SD_Card_BSP_Unlock(unit_nbr);
            FSDev_SD_Card_BSP_Close(unit_nbr);
           *p_err = FS_ERR_BUF_NONE_AVAIL;
            return (init_prev);
        }

        ok = FSDev_SD_Card_SendEXT_CSD(p_sd_card_data, (CPU_INT08U *)p_buf->DataPtr);
        if (ok != DEF_OK) {
            FSBuf_Free(p_buf);
            FSDev_SD_Card_BSP_Unlock(unit_nbr);
            FSDev_SD_Card_BSP_Close(unit_nbr);
           *p_err = FS_ERR_DEV_IO;
            return (init_prev);
        }

        ok = FSDev_SD_ParseEXT_CSD((CPU_INT08U *)p_buf->DataPtr,              /* Parse EXT_CSD info.                                  */
                                   p_sd_info);
        if (ok != DEF_OK) {
            FSBuf_Free(p_buf);
            FSDev_SD_Card_BSP_Unlock(unit_nbr);
            FSDev_SD_Card_BSP_Close(unit_nbr);
           *p_err = FS_ERR_DEV_IO;
            return (init_prev);
        }

        FSBuf_Free(p_buf);
    }


    FSDev_SD_Card_BSP_Unlock(unit_nbr);



                                                                /* ----------------- OUTPUT TRACE INFO ---------------- */
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    FSDev_SD_TraceInfo(p_sd_info);
#endif

   *p_err = FS_ERR_NONE;
    return (DEF_YES);                                           /* Always chngd if opened.                              */
}


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_MakeRdy()
*
* Description : Move card into 'ready' state (& get card type).
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
* Return(s)   : Card type :
*
*                   FS_DEV_SD_SPI_CARDTYPE_NONE          Card type could not be determined.
*                   FS_DEV_SD_SPI_CARDTYPE_SD_V1_X       SD card v1.x.
*                   FS_DEV_SD_SPI_CARDTYPE_SD_V2_0       SD card v2.0, standard capacity.
*                   FS_DEV_SD_SPI_CARDTYPE_SD_V2_0_HC    SD card v2.0, high capacity.
*                   FS_DEV_SD_SPI_CARDTYPE_MMC           MMC, standard capacity.
*                   FS_DEV_SD_SPI_CARDTYPE_MMC_HC        MMC, high capacity.
*
* Note(s)     : (1) #### Add voltage range check.
*
*               (2) (a) A SD card responds to ACMD55 with R3, the OCR register.  The power up status bit,
*                       bit 31, will be set when the card has finished its power up routine.
*
*                   (b) A MMC card responds to CMD1  with R3, the OCR register.  The power up status bit,
*                       bit 31, will be set when the card has finished its power up routine.
*
*                   (c) Proper power-up sequence for an SD-Card allows 1 second time-out value during
*                       device initialization. We set a value of 4ms to allow for 256 retries.
*                       Section 6.4 of SD Full Specs.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_Card_MakeRdy (FS_DEV_SD_CARD_DATA  *p_sd_card_data)
{
    CPU_INT08U   card_type;
    CPU_INT32U   resp_short;
    CPU_INT32U   cmd8_mask;
    CPU_INT16U   retries;
    CPU_BOOLEAN  responded;
    CPU_BOOLEAN  ok;


    resp_short = 0u;                                            /* Initialize short response before use                 */
    cmd8_mask  = 0x0u;                                          /* CMD8 Mask value for v1.x or v2.x usage               */

                                                                /* ------------- SEND CMD8 TO GET CARD OP ------------- */
    ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,               /* Perform CMD8 (R7 resp).                              */
                                  FS_DEV_SD_CMD_SEND_IF_COND,
                                 (FS_DEV_SD_CMD8_VHS_27_36_V | FS_DEV_SD_CMD8_CHK_PATTERN),
                                 &resp_short);

                                                                /* If card does resp, v2.0 card.                        */
    card_type = FS_DEV_SD_CARDTYPE_SD_V1_X;
    if (ok == DEF_OK) {
        if ((resp_short & DEF_OCTET_MASK) == FS_DEV_SD_CMD8_CHK_PATTERN) {
            card_type = FS_DEV_SD_CARDTYPE_SD_V2_0;
            cmd8_mask = FS_DEV_SD_ACMD41_HCS | FS_DEV_SD_OCR_VOLTAGE_MASK;
        }
    } else if (ok == DEF_FAIL) {                                /* No response indicates card is v1.x                   */
                                                                /* check if bits 15-23 are set                          */
            ok = FSDev_SD_Card_AppCmdRShort( p_sd_card_data,    /* Perform ACMD41 (R3 resp).                            */
                                             FS_DEV_SD_ACMD_SD_SEND_OP_COND,
                                             0u,
                                            &resp_short);
            cmd8_mask = resp_short;                             /* Set mask according to voltage bit settings           */
    }

                                                                /* ---------- SEND ACMD41 TO GET CARD STATUS ---------- */
    retries   = 0u;
    responded = DEF_NO;
    while ((retries   <  FS_DEV_SD_CARD_RETRIES_CMD) &&
           (responded == DEF_NO)) {
        ok = FSDev_SD_Card_AppCmdRShort( p_sd_card_data,        /* Perform ACMD41 (R3 resp).                            */
                                         FS_DEV_SD_ACMD_SD_SEND_OP_COND,
                                         cmd8_mask,
                                        &resp_short);

        if (ok == DEF_OK) {                                     /* Chk for busy bit (see Note #1a).                     */
            if (DEF_BIT_IS_SET(resp_short, FS_DEV_SD_OCR_BUSY) == DEF_YES) {
                responded = DEF_YES;
            }
        }

        if (responded == DEF_NO) {
            FS_OS_Dly_ms(4u);                                   /* Proper init timer (See Note #2c)                        */
        }

        retries++;
    }
                                                                /* ------------- SEND CMD1 (FOR MMC CARD) ------------- */
    if (responded == DEF_NO) {
        retries   = 0u;
        responded = DEF_NO;
        while ((retries   <  FS_DEV_SD_CARD_RETRIES_CMD) &&
               (responded == DEF_NO)) {
            ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,       /* Perform CMD1 (R3 resp).                              */
                                          FS_DEV_SD_CMD_SEND_OP_COND,
                                         (FS_DEV_SD_OCR_ACCESS_MODE_SEC | FS_DEV_SD_OCR_VOLTAGE_MASK),
                                         &resp_short);

            if (ok == DEF_OK) {                                 /* Chk for busy bit (see Note #1b).                     */
                if (DEF_BIT_IS_SET(resp_short, FS_DEV_SD_OCR_BUSY) == DEF_YES) {
                    responded = DEF_YES;
                }
            }

            if (responded == DEF_NO) {
                FS_OS_Dly_ms(2u);
            }

            retries++;
        }
                                                                /* No response during initialization                    */
        if (responded == DEF_NO) {
            return (FS_DEV_SD_CARDTYPE_NONE);
        }

        card_type = FS_DEV_SD_CARDTYPE_MMC;
    }
                                                                /* --------------------- CHECK CCS -------------------- */
                                                                /* Chk for HC card.                                     */
    if (DEF_BIT_IS_SET(resp_short, FS_DEV_SD_OCR_CCS) == DEF_YES) {
        if (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0) {
            card_type =  FS_DEV_SD_CARDTYPE_SD_V2_0_HC;
        } else {
            if (card_type == FS_DEV_SD_CARDTYPE_MMC) {
                card_type =  FS_DEV_SD_CARDTYPE_MMC_HC;
            }
        }
    }

    return (card_type);
}


/*
*********************************************************************************************************
*                                          FSDev_SD_Card_Cmd()
*
* Description : Send command requiring no response to device.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               cmd_nbr         Control command index.
*
*               arg             Command argument.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_Cmd (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                        CPU_INT08U            cmd_nbr,
                                        CPU_INT32U            arg)
{
    FS_DEV_SD_CARD_CMD  cmd;
    FS_DEV_SD_CARD_ERR  err;


    FSDev_SD_Card_InitCmd(&cmd,                                 /* Init cmd info.                                       */
                           cmd_nbr,
                           arg,
                           p_sd_card_data->Info.CardType);
                                                                /* Start cmd exec.                                      */
    FSDev_SD_Card_BSP_CmdStart( p_sd_card_data->UnitNbr,
                               &cmd,
                                0u,
                               &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wait for cmd exec end.                               */
    FSDev_SD_Card_BSP_CmdWaitEnd( p_sd_card_data->UnitNbr,
                                 &cmd,
                                  0u,
                                 &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_SPI_CmdRShort()
*
* Description : Send command with R1, R1b, R3, R4, R5, R5b, R6 or R7 response to device.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               cmd_nbr         Command index.
*
*               arg             Command argument.
*
*               p_resp          Pointer to buffer that will receive 4-byte command response.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_CmdRShort (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                              CPU_INT08U            cmd_nbr,
                                              CPU_INT32U            arg,
                                              CPU_INT32U           *p_resp)
{
    FS_DEV_SD_CARD_CMD  cmd;
    FS_DEV_SD_CARD_ERR  err;


    FSDev_SD_Card_InitCmd(&cmd,                                 /* Init cmd info.                                       */
                           cmd_nbr,
                           arg,
                           p_sd_card_data->Info.CardType);
                                                                /* Start cmd exec.                                      */
    FSDev_SD_Card_BSP_CmdStart(         p_sd_card_data->UnitNbr,
                                       &cmd,
                               (void *) 0,
                                       &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wait for cmd exec end & rd 4-byte resp.              */
    FSDev_SD_Card_BSP_CmdWaitEnd( p_sd_card_data->UnitNbr,
                                 &cmd,
                                  p_resp,
                                 &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_CmdRLong()
*
* Description : Perform command with R2 response.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               cmd_nbr         Command index.
*
*               arg             Command argument.
*
*               p_resp          Pointer to buffer that will receive 16-byte command response.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_CmdRLong (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                             CPU_INT08U            cmd_nbr,
                                             CPU_INT32U            arg,
                                             CPU_INT32U           *p_resp)
{
    FS_DEV_SD_CARD_CMD  cmd;
    FS_DEV_SD_CARD_ERR  err;


    FSDev_SD_Card_InitCmd(&cmd,                                 /* Init cmd info.                                       */
                           cmd_nbr,
                           arg,
                           p_sd_card_data->Info.CardType);
                                                                /* Start cmd exec.                                      */
    FSDev_SD_Card_BSP_CmdStart(         p_sd_card_data->UnitNbr,
                                       &cmd,
                               (void *) 0,
                                       &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wait for cmd exec end & rd 16-byte resp.             */
    FSDev_SD_Card_BSP_CmdWaitEnd( p_sd_card_data->UnitNbr,
                                 &cmd,
                                  p_resp,
                                 &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                     FSDev_SD_SPI_AppCmdRShort()
*
* Description : Perform app cmd with R1, R1b, R3, R4, R5, R5b, R6 or R7 response.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               acmd_nbr        Application-specific command index.
*
*               arg             Command argument.
*
*               p_resp          Pointer to buffer that will receive 4-byte command response.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : (1) CMD55, which indicates to the card that the following command will be an app command,
*                   should receive a R1 response.  The correct R1 response should have the APP_CMD bit
*                   (bit 5) set.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_AppCmdRShort (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                                 CPU_INT08U            acmd_nbr,
                                                 CPU_INT32U            arg,
                                                 CPU_INT32U           *p_resp)
{
    CPU_INT32U   cmd55_resp;
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca = p_sd_card_data->RCA;

    ok  = FSDev_SD_Card_CmdRShort( p_sd_card_data,              /* Perform CMD55 (R1 resp).                             */
                                   FS_DEV_SD_CMD_APP_CMD,
                                   rca,
                                  &cmd55_resp);
    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
                                                                /* Chk resp (see Note #1).                              */
    if (DEF_BIT_IS_CLR(cmd55_resp, FS_DEV_SD_CARD_R1_APP_CMD) == DEF_YES) {
        return (DEF_FAIL);
    }

    ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,               /* Perform ACMD.                                        */
                                 (acmd_nbr + 64u),
                                  arg,
                                  p_resp);

    return (ok);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_Card_RdData()
*
* Description : Execute command & read data blocks from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               cmd_nbr         Command index.
*
*               arg             Command argument.
*
*               p_dest          Pointer to destination buffer.
*
*               size            Size of each data block to be read, in octets.
*
*               cnt             Number of data blocks to be read.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) The response from the command MUST be a short response.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_RdData (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                           CPU_INT08U            cmd_nbr,
                                           CPU_INT32U            arg,
                                           CPU_INT08U           *p_dest,
                                           CPU_INT32U            size,
                                           CPU_INT32U            cnt)
{
    FS_DEV_SD_CARD_CMD  cmd;
    FS_DEV_SD_CARD_ERR  err;
    CPU_INT32U          resp;


    FSDev_SD_Card_InitCmd(&cmd,                                 /* Init cmd info.                                       */
                           cmd_nbr,
                           arg,
                           p_sd_card_data->Info.CardType);

    cmd.BlkSize = size;
    cmd.BlkCnt  = cnt;
                                                                /* Start cmd exec.                                      */
    FSDev_SD_Card_BSP_CmdStart( p_sd_card_data->UnitNbr,
                               &cmd,
                                p_dest,
                               &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wait for cmd exec end & rd 4-byte resp.              */
    FSDev_SD_Card_BSP_CmdWaitEnd( p_sd_card_data->UnitNbr,
                                 &cmd,
                                 &resp,
                                 &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Rd data.                                             */
    FSDev_SD_Card_BSP_CmdDataRd( p_sd_card_data->UnitNbr,
                                &cmd,
                                 p_dest,
                                &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_WrData()
*
* Description : Execute command & write data block to card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               cmd_nbr         Command index.
*
*               arg             Command argument.
*
*               p_src           Pointer to source buffer.
*
*               size            Size of each data block to be written, in octets.
*
*               cnt             Number of data blocks to be written.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) The response from the command MUST be a short response.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSDev_SD_Card_WrData (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                           CPU_INT08U            cmd_nbr,
                                           CPU_INT32U            arg,
                                           CPU_INT08U           *p_src,
                                           CPU_INT32U            size,
                                           CPU_INT32U            cnt)
{
    FS_DEV_SD_CARD_CMD  cmd;
    FS_DEV_SD_CARD_ERR  err;
    CPU_INT32U          resp;


    FSDev_SD_Card_InitCmd(&cmd,                                 /* Init cmd info.                                       */
                           cmd_nbr,
                           arg,
                           p_sd_card_data->Info.CardType);

    cmd.BlkSize = size;
    cmd.BlkCnt  = cnt;
                                                                /* Start cmd exec.                                      */
    FSDev_SD_Card_BSP_CmdStart( p_sd_card_data->UnitNbr,
                               &cmd,
                                p_src,
                               &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wait for cmd exec end & rd 4-byte resp.              */
    FSDev_SD_Card_BSP_CmdWaitEnd( p_sd_card_data->UnitNbr,
                                 &cmd,
                                 &resp,
                                 &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }
                                                                /* Wr data.                                             */
    FSDev_SD_Card_BSP_CmdDataWr( p_sd_card_data->UnitNbr,
                                &cmd,
                                 p_src,
                                &err);
    if (err != FS_DEV_SD_CARD_ERR_NONE) {
        FS_DEV_SD_CARD_HANDLE_ERR_BSP(p_sd_card_data, err);
        return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_SendCID()
*
* Description : Get Card ID (CID) register from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_dest          Pointer to 16-byte buffer that will receive SD/MMC Card ID register.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) (a) For SD cards, the structure of the CID is defined in [Ref 1] Section 5.1.
*
*                   (b) For MMC cards, the structure of the CID is defined in [Ref 2] Section 8.2.
*
*               (2) On little-endian processors, the long card response is read in 32-bit words.  The
*                   byte-order of the words MUST be reversed to obtain the bit-order shown in [Ref 1],
*                   Table 5-2, or [Ref 2], Table 30.
*
*                   (a) The highest bit of the first byte is bit 127 of the CID as defined in [Ref 1],
*                       Table 5-2, or [Ref 2], Table 30.
*
*               (3) ALL_SEND_CID is performed during startup to activate the card that will later be
*                   addressed.  Once the card is addressed (whereupon the RCA is assigned/obtained),
*                   SEND_CID should be performed instead.
*
*                   (a) SEND_CID can only be performed when the card is in the 'stby' state.  The card
*                       is deselected before this command (thereby moving the card to the 'stby' state)
*                       & re-selected after the command (moving the card back to the 'tran' state).
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_SendCID (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                            CPU_INT08U           *p_dest)
{
    CPU_INT32U   resp[4];
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca = p_sd_card_data->RCA;

    if (rca == 0u) {                                            /* Chk if addr'd (see Note #3).                         */

        ok = FSDev_SD_Card_CmdRLong( p_sd_card_data,            /* Perform CMD2 (R2 resp).                              */
                                     FS_DEV_SD_CMD_ALL_SEND_CID,
                                     0u,
                                    &resp[0]);
        if (ok != DEF_OK) {
            return (DEF_FAIL);
        }

    } else {
        ok = FSDev_SD_Card_Sel(p_sd_card_data, DEF_NO);         /* Desel card (see Note #3a).                           */
        if (ok != DEF_OK) {
            return (DEF_FAIL);
        }

        ok = FSDev_SD_Card_CmdRLong( p_sd_card_data,            /* Perform CMD10 (R2 resp).                             */
                                     FS_DEV_SD_CMD_SEND_CID,
                                     rca,
                                    &resp[0]);
        if (ok != DEF_OK) {
            return (DEF_FAIL);
        }
    }

                                                                /* Copy CID into buf (see Note #2).                     */
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[0],  (void *)&resp[0]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[4],  (void *)&resp[1]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[8],  (void *)&resp[2]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[12], (void *)&resp[3]);

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_SendCSD()
*
* Description : Get Card-Specific Data (CSD) register from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_dest          Pointer to 16-byte buffer that will receive SD/MMC Card-Specific Data register.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) (a) For SD v1.x & SD v2.0 standard capacity, the structure of the CSD is defined in
*                       [Ref 1], Section 5.3.2.
*
*                   (b) For MMC cards, the structure of the CSD is defined in [Ref 2], Section 8.3.
*
*                   (c) For SD v2.0 high capacity cards, the structure of the CSD is defined in [Ref 1],
*                       Section 5.3.3.
*
*               (2) On little-endian processors, the long card response is read in 32-bit words.  The
*                   byte-order of the words MUST be reversed to obtain the bit-order shown in [Ref 1],
*                   Table 5-4 or Table 5-16, or [Ref 2], Table 31.
*
*                   (a) The highest bit of the first byte is bit 127 of the CSD as defined in [Ref 1],
*                       Table 5-4 or Table 5-16, or [Ref 2], Table 31.
*
*               (3) SEND_CSD can only be performed when the card is in the 'stby' state.  The card
*                   is deselected before this command (thereby moving the card to the 'stby' state)
*                   & re-selected after the command (moving the card back to the 'tran' state).
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_SendCSD (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                            CPU_INT08U           *p_dest)
{
    CPU_INT32U   resp[4];
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca = p_sd_card_data->RCA;

    ok  = FSDev_SD_Card_Sel(p_sd_card_data, DEF_NO);            /* Desel card (see Note #3).                            */
    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }

    ok = FSDev_SD_Card_CmdRLong( p_sd_card_data,                /* Perform CMD9 (R2 resp).                              */
                                 FS_DEV_SD_CMD_SEND_CSD,
                                 rca,
                                &resp[0]);
    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }

                                                                /* Copy CSD into buf (see Note #2).                     */
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[0],  (void *)&resp[0]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[4],  (void *)&resp[1]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[8],  (void *)&resp[2]);
    MEM_VAL_COPY_GET_INT32U_BIG((void *)&p_dest[12], (void *)&resp[3]);

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_SendEXT_CSD()
*
* Description : Get Extended Card-Specific Data (EXT_CSD) register from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_dest          Pointer to 512-byte buffer that will receive MMC Extended Card-Specific Data register.
*
* Return(s)   : DEF_OK,   if the data was read.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) Only valid for High Capacity MMC cards.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_SendEXT_CSD (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                                CPU_INT08U           *p_dest)
{
    CPU_BOOLEAN  ok;


    ok = FSDev_SD_Card_Sel(p_sd_card_data, DEF_YES);

    ok = FSDev_SD_Card_RdData(p_sd_card_data, FS_DEV_MMC_CMD_SEND_EXT_CSD, 0u, p_dest, FS_DEV_SD_CID_EXT_CSD_LEN, 1u);

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_SendSCR()
*
* Description : Get SD CARD Configuration Register (SCR) from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_dest          Pointer to 8-byte buffer that will receive SD CARD Configuration Register.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) The SCR register is present ONLY on SD cards.  The register is defined in [Ref 1],
*                   Section 5.6.
*
*               (1) CMD55, which indicates to the card that the following command will be an app command,
*                   should receive a R1 response.  The correct R1 response should have the APP_CMD bit
*                   (bit 5) set.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_SendSCR (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                            CPU_INT08U           *p_dest)
{
    CPU_INT32U   cmd55_resp;
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca = p_sd_card_data->RCA;

    ok  = FSDev_SD_Card_CmdRShort( p_sd_card_data,              /* Perform CMD55 (R1 resp).                             */
                                   FS_DEV_SD_CMD_APP_CMD,
                                   rca,
                                  &cmd55_resp);
    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
                                                                /* Chk resp (see Note #2).                              */
    if (DEF_BIT_IS_CLR(cmd55_resp, FS_DEV_SD_CARD_R1_APP_CMD) == DEF_YES) {
        return (DEF_FAIL);
    }

    ok = FSDev_SD_Card_RdData( p_sd_card_data,                  /* Perform ACMD13 & rd data.                            */
                              (FS_DEV_SD_ACMD_SEND_SCR + 64u),
                               0u,
                               p_dest,
                               FS_DEV_SD_CID_SCR_LEN,
                               1u);
    return (ok);
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_SendSDStatus()
*
* Description : Get SD status from card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               p_dest          Pointer to 64-byte buffer that will receive the SD status.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) The SD status is present ONLY on SD cards.  The information format is defined in
*                   [Ref 1], Section 4.10.2.
*
*                   (a) The card MUST have been previously selected, since the SD status can only be
*                       read in the 'tran' state.
*
*               (1) CMD55, which indicates to the card that the following command will be an app command,
*                   should receive a R1 response.  The correct R1 response should have the APP_CMD bit
*                   (bit 5) set.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_SendSDStatus (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                                 CPU_INT08U           *p_dest)
{
    CPU_INT32U   cmd55_resp;
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca = p_sd_card_data->RCA;

    ok  = FSDev_SD_Card_CmdRShort( p_sd_card_data,              /* Perform CMD55 (R1 resp).                             */
                                   FS_DEV_SD_CMD_APP_CMD,
                                   rca,
                                  &cmd55_resp);
    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
                                                                /* Chk resp (see Note #2).                              */
    if (DEF_BIT_IS_CLR(cmd55_resp, FS_DEV_SD_CARD_R1_APP_CMD) == DEF_YES) {
        return (DEF_FAIL);
    }

    ok = FSDev_SD_Card_RdData( p_sd_card_data,                  /* Perform ACMD51 & rd data.                            */
                              (FS_DEV_SD_ACMD_SD_STATUS + 64u),
                               0u,
                               p_dest,
                               64u,
                               1u);
    return (ok);
}


/*
*********************************************************************************************************
*                                     FSDev_SD_Card_SetBusWidth()
*
* Description : Set bus width.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               width           Maximum bus width, in bits.
*
* Return(s)   : The configured bus width.
*
* Note(s)     : (1) The card MUST have been already selected.
*
*               (2) (a) Current SD cards always support 1- & 4-bit bus widths according to [Ref 1],
*                       Section 5-6.
*
*                       (1) The 4-bit field [51..48] of the SCR register specifies allowed bus widths.
*                           Bit 0 and bit 2 indicate support for 1- and 4-bit bus widths, respectively.
*
*                       (2) The actual bus width can be determined from bits 511:510 of the SD status.
*
*                   (b) MMC cards may support 4- or 8-bit bus widths.
*
*                       (1) A bus-test procedure is defined in [Ref 2], Section 7.4.4.   Currently, the
*                           bus-test procedure is NOT used.
*
*                       (2) The bus width is set using CMD_SWITCH_FUNC with the appropriate argument to
*                           assign byte 183 of the EXT_CSD register.   The assigned value is 1 for a
*                           4-bit bus or 2 for a 8-bit bus, as listed in [Ref 2], Table 59.
*
*                       (3) According to [Ref 2], Section 7.4.1, the "host should read the card status,
*                           using the SEND_STATUS command, after the busy signal is de-asserted, to check
*                           of the SWITCH operation".
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_Card_SetBusWidth (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                               CPU_INT08U            width)
{
    FS_DEV_SD_INFO  *p_sd_info;
    CPU_INT32U       cmd_arg;
    CPU_INT32U       resp1;
    CPU_BOOLEAN      ok;
    CPU_INT08U       state;
    CPU_INT08U       width_cfgd;
    CPU_INT08U       widths_sd;
    CPU_INT08U       scr[8];
    CPU_INT08U       sd_status[64];


    p_sd_info  = &p_sd_card_data->Info;
    width_cfgd =  1u;

    switch (p_sd_info->CardType) {
        case FS_DEV_SD_CARDTYPE_SD_V1_X:                        /* ---------------------- SD CARD --------------------- */
        case FS_DEV_SD_CARDTYPE_SD_V2_0:                                /* See Note #2a.                                */
        case FS_DEV_SD_CARDTYPE_SD_V2_0_HC:
             if (width >= 4u) {
                 ok = FSDev_SD_Card_SendSCR(p_sd_card_data, &scr[0]);   /* Rd SCR reg (see Note #2a1) ...               */
                 if (ok == DEF_OK) {
                     widths_sd = scr[1] & 0x0Fu;                        /* ... chk supported widths.                    */
                     width     = (DEF_BIT_IS_CLR(widths_sd, DEF_BIT_02) == DEF_YES) ? 1u : 4u;
                 } else {
                     FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to read SCR.\r\n"));
                     width     =  1u;
                 }
             }

             if (width == 4u) {                                         /* Cfg 4-bit width.                             */
                 ok = FSDev_SD_Card_AppCmdRShort( p_sd_card_data,
                                                  FS_DEV_SD_ACMD_BUS_WIDTH,
                                                  0x00000002u,
                                                 &resp1);

                 if (ok == DEF_OK) {
                     state = DEF_BIT_FIELD_RD(resp1, FS_DEV_SD_CARD_R1_CUR_STATE_MASK);
                     if (state == FS_DEV_SD_CARD_STATE_TRAN) {
                                                                        /* If cmd performed ...                         */
                                                                        /* ... chk actual width (see Note #2a2).        */
                         FSDev_SD_Card_BSP_SetBusWidth(p_sd_card_data->UnitNbr, 4u);
                         ok = FSDev_SD_Card_SendSDStatus(p_sd_card_data, &sd_status[0]);
                         if (ok == DEF_OK) {
                             widths_sd = (sd_status[0] & 0xC0u) >> 6;
                             if (widths_sd == 2u) {
                                 width_cfgd = 4u;
                             } else {
                                 FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to set 4-bit width: DAT_BUS_WIDTH = %d.\r\n", widths_sd));
                             }
                         } else {
                             FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to read SD status; bus width unknown?\r\n"));
                         }
                     } else {
                         FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to set 4-bit width.\r\n"));
                     }
                 } else {
                     FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to set 4-bit width.\r\n"));
                 }
             }
             break;



                                                                /* ------------------------ MMC ----------------------- */
        case FS_DEV_SD_CARDTYPE_MMC:                                    /* See Note #2b.                                */
        case FS_DEV_SD_CARDTYPE_MMC_HC:
        default:
             if (width >= 8u) {
                 width = 8u;
             } else if (width >= 4u) {
                 width = 4u;
             } else {
                 width = 1u;
             }

             if (width != 1u) {                                         /* Perform CMD6 (sw fnct).                      */
                                                                        /* See Note #2b2.                               */
                 cmd_arg  = (width == 4u) ? (1u << 8) : (2u << 8);
                 cmd_arg |= 0x03000000u | (183u << 16) | 0x00000001u;

                 ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,
                                               FS_DEV_SD_CMD_SWITCH_FUNC,
                                               cmd_arg,
                                              &resp1);

                 if (ok == DEF_YES) {
                     ok =  FSDev_SD_Card_WaitWhileBusy(p_sd_card_data); /* See Note #2b3.                               */
                 }

                 if (ok == DEF_NO) {
                     FS_TRACE_DBG(("FSDev_SD_Card_SetBusWidth(): Failed to set %d-bit width.\r\n", width));
                     width = 1u;
                 }
             }
             width_cfgd = width;
             break;
    }

    FSDev_SD_Card_BSP_SetBusWidth(p_sd_card_data->UnitNbr, width_cfgd);

    return (width_cfgd);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_Card_Stop()
*
* Description : Stop transmission.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : (1) When this function is invoked, the card should be in either the 'data' state (after a
*                   multiple block read) or 'rcv' state (after a multiple block write).  CMD12 ("stop
*                   transmission") should move the card into either 'tran' or 'prg' state, respectively.
*
*                   (a) If the stop transmission fails, then the card may no longer be in either the
*                       'data' or 'rcv' state, the only states when CMD12 is valid.  In that case, CMD13
*                       ("send status") is performed to check the current card state.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_Stop (FS_DEV_SD_CARD_DATA  *p_sd_card_data)
{
    CPU_INT32U   resp1b;
    CPU_INT16U   retries;
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;
    CPU_INT08U   state;


    rca     = p_sd_card_data->RCA;

    ok = FSDev_SD_Card_CmdRShort(p_sd_card_data,                /* Stop transmission ... perform CMD12 (R1b resp).      */
                                 FS_DEV_SD_CMD_STOP_TRANSMISSION,
                                 0u,
                                &resp1b);

                                                                /* Check if card is in state 'rcv' or 'data', as expected.  */
    if (ok == DEF_OK) {
        state = DEF_BIT_FIELD_RD(resp1b, FS_DEV_SD_CARD_R1_CUR_STATE_MASK);

        if ((state == FS_DEV_SD_CARD_STATE_DATA) ||
            (state == FS_DEV_SD_CARD_STATE_RCV )) {
            return (DEF_OK);
        }
    }

                                                                /* If stop transmission failed (see Note #1a) ...       */
    retries = 0u;
    while (retries < FS_DEV_SD_CARD_RETRIES_CMD_BUSY) {
            ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,       /* ... get status ... perform CMD13 (R1 resp).          */
                                          FS_DEV_SD_CMD_SEND_STATUS,
                                          rca,
                                         &resp1b);
        if (ok == DEF_OK) {
            if (DEF_BIT_IS_SET(resp1b, FS_DEV_SD_CARD_R1_READY_FOR_DATA) == DEF_YES) {
                return (DEF_OK);
            }
        }

        retries++;
    }

    return (DEF_FAIL);                                          /* Fail if neither approaches yield an avail status.    */
}


/*
*********************************************************************************************************
*                                         FSDev_SD_Card_Sel()
*
* Description : Select card.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               sel             Indicates whether card should be selected or deselected :
*
*                                   DEF_YES    Card should be selected.
*                                   DEF_NO     Card should be deselected.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : (1) A card state transition table is provided in [Ref 1], Table 4-28.
*
*                   (a) Before the card is selected (or after the card is deselected), it should be in
*                       'stby' state.  After the card is selected, it should be in the 'tran' state and be
*                       ready for data.
*
*                   (b) The transition between the 'prg' & 'dis' states are not handled; the card should
*                       NEVER be in either of these states when this function is called.
*
*               (2) The state returned in the response to the SEL_DESEL_CARD command is the card state
*                   when that command was received, not the state of the device after the transition
*                   is effected.  The active state of the card must be checked with the SEND_STATUS
*                   command.
*
*               (3) In certain cases, the SEL_DESEL_CARD command may fail & the state should be checked
*                   directly.
*
*                   (a) If the card is already selected, the SEL_DESEL_CARD command will fail if
*                       performed with the card's RCA.  The state of the card must be checked with the
*                       SEND_STATUS command; it should be in the 'tran' state & ready for data.
*
*                   (b) The SEL_DESEL_CARD command may fail if performed with a RCA of 0, whether or not
*                       the card is already selected.  The state of the card must be checked with the
*                       SEND_STATUS command; it should be in the 'stby' state.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_Sel (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                        CPU_BOOLEAN           sel)
{
    CPU_INT32U   resp1;
    CPU_INT16U   retries;
    CPU_INT32U   state;
    CPU_BOOLEAN  ok;
    CPU_BOOLEAN  rtn;
    CPU_INT32U   rca;


    if (sel == DEF_YES) {
        rca =  p_sd_card_data->RCA;
    } else {
        rca = 0u;
    }

    retries = 0u;
    rtn     = DEF_NO;
    while ((retries <  FS_DEV_SD_CARD_RETRIES_CMD) &&
           (rtn     == DEF_NO)) {
        ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,           /* Perform CMD7 (R1b resp).                              */
                                      FS_DEV_SD_CMD_SEL_DESEL_CARD,
                                      rca,
                                     &resp1);
        if (ok != DEF_OK) {
            rtn = DEF_YES;
        }

        ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,           /* Chk current card state (see Note #2).                */
                                      FS_DEV_SD_CMD_SEND_STATUS,
                                      p_sd_card_data->RCA,
                                     &resp1);

        if (ok == DEF_OK) {
            state = DEF_BIT_FIELD_RD(resp1, FS_DEV_SD_CARD_R1_CUR_STATE_MASK);
            if (sel == DEF_YES) {                               /* Chk if card sel'd (see Note #3a).                    */
                if (state == FS_DEV_SD_CARD_STATE_TRAN) {
                    if (DEF_BIT_IS_SET(resp1, FS_DEV_SD_CARD_R1_READY_FOR_DATA) == DEF_YES) {
                        return (DEF_OK);
                    }
                }
            } else {                                            /* Chk if card desel'd (see Note #3b).                  */
                if (state == FS_DEV_SD_CARD_STATE_STBY) {
                    return (DEF_OK);
                }
            }
        }

        retries++;
    }

    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_WaitWhileBusy()
*
* Description : Wait while card is busy.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
* Return(s)   : DEF_OK,   if command succeeds.
*               DEF_FAIL, if command fails.
*
* Note(s)     : (1) When the card is no longer busy, it should be in 'tran' state and be ready for data.
*
*               (2) If the card is in either the 'rcv' or 'data' states, then CMD12 (STOP_TRANSMISSION)
*                   must be performed.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_Card_WaitWhileBusy (FS_DEV_SD_CARD_DATA  *p_sd_card_data)
{
    CPU_INT32U   resp1;
    CPU_INT16U   retries;
    CPU_INT32U   state;
    CPU_BOOLEAN  ok;
    CPU_INT32U   rca;


    rca     = p_sd_card_data->RCA;

    retries = 0u;
    while (retries < FS_DEV_SD_CARD_RETRIES_CMD_BUSY) {
        ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,           /* Perform CMD13 (R1 resp).                             */
                                      FS_DEV_SD_CMD_SEND_STATUS,
                                      rca,
                                     &resp1);

        if (ok != DEF_OK) {
            return (DEF_FAIL);
        }

        state = DEF_BIT_FIELD_RD(resp1, FS_DEV_SD_CARD_R1_CUR_STATE_MASK);
        if (state == FS_DEV_SD_CARD_STATE_TRAN) {
            if (DEF_BIT_IS_SET(resp1, FS_DEV_SD_CARD_R1_READY_FOR_DATA) == DEF_YES) {
                return (DEF_OK);
            }

        } else {
            if ((state == FS_DEV_SD_CARD_STATE_RCV) ||
                (state == FS_DEV_SD_CARD_STATE_DATA)) {
                ok = FSDev_SD_Card_Stop(p_sd_card_data);        /* Move card to 'tran' state (see Note #2).             */

                if (ok != DEF_OK) {
                    return (DEF_FAIL);
                }

                                                                /* Perform CMD13 (R1 resp).                             */
                ok = FSDev_SD_Card_CmdRShort( p_sd_card_data,
                                              FS_DEV_SD_CMD_SEND_STATUS,
                                              rca,
                                             &resp1);

                if (ok != DEF_OK) {
                    return (DEF_FAIL);
                }

                state = DEF_BIT_FIELD_RD(resp1, FS_DEV_SD_CARD_R1_CUR_STATE_MASK);
                if (state == FS_DEV_SD_CARD_STATE_TRAN) {
                    if (DEF_BIT_IS_SET(resp1, FS_DEV_SD_CARD_R1_READY_FOR_DATA) == DEF_YES) {
                        return (DEF_OK);
                    }
                }

                return (DEF_FAIL);
            }
        }

        retries++;
    }

    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                    FSDev_SD_Card_HandleErrBSP()
*
* Description : Handle BSP error.
*
* Argument(s) : p_sd_card_data  Pointer to SD card data.
*
*               err             BSP error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
static  void  FSDev_SD_Card_HandleErrBSP (FS_DEV_SD_CARD_DATA  *p_sd_card_data,
                                          FS_DEV_SD_CARD_ERR    err)
{
    switch (err) {
        case FS_DEV_SD_CARD_ERR_BUSY:                           /* Controller is busy.                                  */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardBusyCtr);
             break;
        case FS_DEV_SD_CARD_ERR_UNKNOWN:                        /* Unknown or other error.                              */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardUnknownCtr);
             break;

        case FS_DEV_SD_CARD_ERR_WAIT_TIMEOUT:                   /* Timeout in waiting for cmd resp/data.                */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardWaitTimeoutCtr);
             break;

        case FS_DEV_SD_CARD_ERR_RESP_TIMEOUT:                   /* Timeout in rx'ing cmd resp.                          */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardRespTimeoutCtr);
             break;

        case FS_DEV_SD_CARD_ERR_RESP_CHKSUM:                    /* Err in resp checksum.                                */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardRespChksumCtr);
             break;

        case FS_DEV_SD_CARD_ERR_RESP_CMD_IX:                    /* Resp cmd ix err.                                     */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardRespCmdIxCtr);
             break;

        case FS_DEV_SD_CARD_ERR_RESP_END_BIT:                   /* Resp end bit err.                                    */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardRespEndBitCtr);
             break;

        case FS_DEV_SD_CARD_ERR_RESP:                           /* Other resp err.                                      */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardRespCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA_UNDERRUN:                  /* Data underrun.                                       */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataUnderrunCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA_OVERRUN:                   /* Data overrun.                                        */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataOverrunCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA_TIMEOUT:                   /* Timeout in rx'ing data.                              */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataTimeoutCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA_CHKSUM:                    /* Err in data checksum.                                */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataChksumCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA_START_BIT:                 /* Data start bit err.                                  */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataStartBitCtr);
             break;

        case FS_DEV_SD_CARD_ERR_DATA:                           /* Other data err.                                      */
             FS_CTR_ERR_INC(p_sd_card_data->ErrCardDataCtr);
             break;

        case FS_DEV_SD_CARD_ERR_NONE:                           /* No error.                                            */
        case FS_DEV_SD_CARD_ERR_NO_CARD:                        /* No card present.                                     */
        default:
            break;
    }
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_InitCmd()
*
* Description : Initialize SD card command information.
*
* Argument(s) : p_cmd       Pointer to SD card data.
*
*               cmd_nbr     Control index.
*
*               arg         Command argument.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_InitCmd (FS_DEV_SD_CARD_CMD  *p_cmd,
                                     CPU_INT08U           cmd_nbr,
                                     CPU_INT32U           arg,
                                     CPU_INT08U           card_type)
{
    CPU_INT08U  resp_type;
    FS_FLAGS    flags;
    CPU_INT08U  data_dir;
    CPU_INT08U  data_type;


    p_cmd->Cmd = (cmd_nbr >= 64u) ? (cmd_nbr - 64u) : cmd_nbr;
    p_cmd->Arg =  arg;

    resp_type  = FSDev_SD_Card_CmdRespType[cmd_nbr];

    switch (card_type) {
        case FS_DEV_SD_CARDTYPE_SD_V1_X:
        case FS_DEV_SD_CARDTYPE_SD_V2_0:
        case FS_DEV_SD_CARDTYPE_SD_V2_0_HC:
            resp_type = (resp_type & FS_DEV_SD_CARD_RESP_TYPE_MASK) >> FS_DEV_SD_CARD_RESP_TYPE_OFFSET;
            break;

        case FS_DEV_SD_CARDTYPE_MMC:
        case FS_DEV_SD_CARDTYPE_MMC_HC:
            resp_type = (resp_type & FS_DEV_MMC_RESP_TYPE_MASK) >> FS_DEV_MMC_RESP_TYPE_OFFSET;
            break;

        case FS_DEV_SD_CARDTYPE_NONE:
        default:
            resp_type = (resp_type & FS_DEV_SD_CARD_RESP_TYPE_MASK) >> FS_DEV_SD_CARD_RESP_TYPE_OFFSET;
            break;
    }

    flags      = FSDev_SD_Card_CmdFlag[resp_type];

    if (cmd_nbr == FS_DEV_SD_CMD_GO_IDLE_STATE) {
        flags |= FS_DEV_SD_CARD_CMD_FLAG_INIT;
    }


    if((card_type == FS_DEV_SD_CARDTYPE_NONE)    ||
       (card_type == FS_DEV_SD_CARDTYPE_SD_V1_X) ||
       (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0) ||
       (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0_HC)) {
        switch (cmd_nbr) {                                      /* ------------------- GET DATA DIR ------------------- */
            case FS_DEV_SD_CMD_READ_DAT_UNTIL_STOP:
            case FS_DEV_SD_CMD_READ_SINGLE_BLOCK:
            case FS_DEV_SD_CMD_READ_MULTIPLE_BLOCK:
            case FS_DEV_SD_CMD_BUSTEST_R:
            case (FS_DEV_SD_ACMD_SD_STATUS + 64u):
            case (FS_DEV_SD_ACMD_SEND_SCR + 64u):
                 data_dir   = FS_DEV_SD_CARD_DATA_DIR_CARD_TO_HOST;
                 flags     |= FS_DEV_SD_CARD_CMD_FLAG_START_DATA_TX;
                 break;

            case FS_DEV_SD_CMD_STOP_TRANSMISSION:
                 data_dir  = FS_DEV_SD_CARD_DATA_DIR_NONE;
                 flags    |= FS_DEV_SD_CARD_CMD_FLAG_STOP_DATA_TX;
                 break;

            case FS_DEV_SD_CMD_WRITE_DAT_UNTIL_STOP:
            case FS_DEV_SD_CMD_WRITE_BLOCK:
            case FS_DEV_SD_CMD_WRITE_MULTIPLE_BLOCK:
            case FS_DEV_SD_CMD_BUSTEST_W:
                 data_dir  = FS_DEV_SD_CARD_DATA_DIR_HOST_TO_CARD;
                 flags    |= FS_DEV_SD_CARD_CMD_FLAG_START_DATA_TX;
                 break;

            default:
                 data_dir = FS_DEV_SD_CARD_DATA_DIR_NONE;
                 break;
        }



        switch (cmd_nbr) {                                      /* ------------------- GET DATA TYPE ------------------ */
            case FS_DEV_SD_CMD_READ_DAT_UNTIL_STOP:
            case FS_DEV_SD_CMD_WRITE_DAT_UNTIL_STOP:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_STREAM;
                 break;

            case FS_DEV_SD_CMD_READ_SINGLE_BLOCK:
            case FS_DEV_SD_CMD_WRITE_BLOCK:
            case (FS_DEV_SD_ACMD_SD_STATUS + 64u):
            case (FS_DEV_SD_ACMD_SEND_SCR + 64u):
            case FS_DEV_SD_CMD_BUSTEST_R:
            case FS_DEV_SD_CMD_BUSTEST_W:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_SINGLE_BLOCK;
                 break;

            case FS_DEV_SD_CMD_READ_MULTIPLE_BLOCK:
            case FS_DEV_SD_CMD_WRITE_MULTIPLE_BLOCK:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_MULTI_BLOCK;
                 break;

            default:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_NONE;
                 break;
        }
    } else {
        switch (cmd_nbr) {                                      /* ------------------- GET DATA DIR ------------------- */
            case FS_DEV_MMC_CMD_READ_DAT_UNTIL_STOP:
            case FS_DEV_MMC_CMD_READ_SINGLE_BLOCK:
            case FS_DEV_MMC_CMD_READ_MULTIPLE_BLOCK:
            case FS_DEV_MMC_CMD_BUSTEST_R:
            case FS_DEV_MMC_CMD_SEND_EXT_CSD:
                 data_dir   = FS_DEV_SD_CARD_DATA_DIR_CARD_TO_HOST;
                 flags     |= FS_DEV_SD_CARD_CMD_FLAG_START_DATA_TX;
                 break;

            case FS_DEV_MMC_CMD_STOP_TRANSMISSION:
                 data_dir  = FS_DEV_SD_CARD_DATA_DIR_NONE;
                 flags    |= FS_DEV_SD_CARD_CMD_FLAG_STOP_DATA_TX;
                 break;

            case FS_DEV_MMC_CMD_WRITE_DAT_UNTIL_STOP:
            case FS_DEV_MMC_CMD_WRITE_BLOCK:
            case FS_DEV_MMC_CMD_WRITE_MULTIPLE_BLOCK:
            case FS_DEV_MMC_CMD_BUSTEST_W:
                 data_dir  = FS_DEV_SD_CARD_DATA_DIR_HOST_TO_CARD;
                 flags    |= FS_DEV_SD_CARD_CMD_FLAG_START_DATA_TX;
                 break;

            default:
                 data_dir = FS_DEV_SD_CARD_DATA_DIR_NONE;
                 break;
        }



        switch (cmd_nbr) {                                      /* ------------------- GET DATA TYPE ------------------ */
            case FS_DEV_MMC_CMD_READ_DAT_UNTIL_STOP:
            case FS_DEV_MMC_CMD_WRITE_DAT_UNTIL_STOP:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_STREAM;
                 break;

            case FS_DEV_MMC_CMD_READ_SINGLE_BLOCK:
            case FS_DEV_MMC_CMD_WRITE_BLOCK:
            case FS_DEV_MMC_CMD_BUSTEST_R:
            case FS_DEV_MMC_CMD_BUSTEST_W:
            case FS_DEV_MMC_CMD_SEND_EXT_CSD:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_SINGLE_BLOCK;
                 break;

            case FS_DEV_MMC_CMD_READ_MULTIPLE_BLOCK:
            case FS_DEV_MMC_CMD_WRITE_MULTIPLE_BLOCK:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_MULTI_BLOCK;
                 break;

            default:
                 data_type = FS_DEV_SD_CARD_DATA_TYPE_NONE;
                 break;
        }
    }



                                                                /* ----------------- ASSIGN CMD PARAMS ---------------- */
    p_cmd->RespType = resp_type;
    p_cmd->Flags    = flags;
    p_cmd->DataType = data_type;
    p_cmd->DataDir  = data_dir;

    p_cmd->BlkSize  = 0u;
    p_cmd->BlkCnt   = 0u;
}


/*
*********************************************************************************************************
*                                      FSDev_SD_Card_DataFree()
*
* Description : Free a SD card data object.
*
* Argument(s) : p_sd_card_data      Pointer to a SD card data object.
*               --------------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_SD_Card_DataFree (FS_DEV_SD_CARD_DATA  *p_sd_card_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_sd_card_data->NextPtr   = FSDev_SD_Card_ListFreePtr;      /* Add to free pool.                                    */
    FSDev_SD_Card_ListFreePtr = p_sd_card_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       FSDev_SD_Card_DataGet()
*
* Description : Allocate & initialize a SD card data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a SD card data object, if NO errors.
*               Pointer to NULL,                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_SD_CARD_DATA  *FSDev_SD_Card_DataGet (void)
{
    LIB_ERR               alloc_err;
    CPU_SIZE_T            octets_reqd;
    FS_DEV_SD_CARD_DATA  *p_sd_card_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FSDev_SD_Card_ListFreePtr == (FS_DEV_SD_CARD_DATA *)0) {
        p_sd_card_data = (FS_DEV_SD_CARD_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_SD_CARD_DATA),
                                                               sizeof(CPU_DATA),
                                                              &octets_reqd,
                                                              &alloc_err);
        if (p_sd_card_data == (FS_DEV_SD_CARD_DATA *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_SD_Card_DataGet(): Could not alloc mem for SD card data: %d octets required.\r\n", octets_reqd));
            return ((FS_DEV_SD_CARD_DATA *)0);
        }
        (void)alloc_err;

        FSDev_SD_Card_UnitCtr++;


    } else {
        p_sd_card_data            = FSDev_SD_Card_ListFreePtr;
        FSDev_SD_Card_ListFreePtr = FSDev_SD_Card_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    FSDev_SD_ClrInfo(&p_sd_card_data->Info);                    /* Clr SD info.                                         */

    p_sd_card_data->Init                    =  DEF_NO;

#if (FS_CFG_CTR_STAT_EN                     == DEF_ENABLED)     /* Clr stat ctrs.                                       */
    p_sd_card_data->StatRdCtr               =  0u;
    p_sd_card_data->StatWrCtr               =  0u;
#endif

#if (FS_CFG_CTR_ERR_EN                      == DEF_ENABLED)     /* Clr err ctrs.                                        */
    p_sd_card_data->ErrRdCtr                =  0u;
    p_sd_card_data->ErrWrCtr                =  0u;

    p_sd_card_data->ErrCardBusyCtr          =  0u;
    p_sd_card_data->ErrCardUnknownCtr       =  0u;
    p_sd_card_data->ErrCardWaitTimeoutCtr   =  0u;
    p_sd_card_data->ErrCardRespTimeoutCtr   =  0u;
    p_sd_card_data->ErrCardRespChksumCtr    =  0u;
    p_sd_card_data->ErrCardRespCmdIxCtr     =  0u;
    p_sd_card_data->ErrCardRespEndBitCtr    =  0u;
    p_sd_card_data->ErrCardRespCtr          =  0u;
    p_sd_card_data->ErrCardDataUnderrunCtr  =  0u;
    p_sd_card_data->ErrCardDataOverrunCtr   =  0u;
    p_sd_card_data->ErrCardDataTimeoutCtr   =  0u;
    p_sd_card_data->ErrCardDataChksumCtr    =  0u;
    p_sd_card_data->ErrCardDataStartBitCtr  =  0u;
    p_sd_card_data->ErrCardDataCtr          =  0u;
#endif

    p_sd_card_data->RCA                     =  0u;
    p_sd_card_data->BlkCntMax               =  0u;
    p_sd_card_data->BusWidth                =  0u;
    p_sd_card_data->NextPtr                 = (FS_DEV_SD_CARD_DATA *)0;

    return (p_sd_card_data);
}




