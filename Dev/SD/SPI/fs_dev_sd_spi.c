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
*                                               SPI MODE
*
* Filename     : fs_dev_sd_spi.c
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) SD Card Association.  "Physical Layer Simplified Specification Version 2.00".
*                    July 26, 2006.
*
*                (2) JEDEC Solid State Technology Association.  "MultiMediaCard (MMC) Electrical
*                    Standard, High Capacity".  JESD84-B42.  July 2007.
*********************************************************************************************************
* Note(s)      : (1) This driver has been tested with MOST SD/MMC media types, including :
*
*                    (a) Standard capacity SD cards, v1.x & v2.0.
*                    (b) SDmicro cards.
*                    (c) High capacity SD cards (SDHC)
*                    (d) MMC
*                    (e) MMCplus
*
*                    It should also work with devices conformant to the relevant SD or MMC specifications.
*
*                (2) High capacity MMCPlus cards (>2GB) are not supported by the SPI driver.
*
*                (3) MMC (eMMC) v4.3 and superior dropped support for the SPI interface and as such
*                    cannot be used with this driver. SPI support is optional for MMCplus and MMCmobile
*                    cards.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_SD_SPI_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  "../../../Source/fs.h"
#include  "../../../Source/fs_dev.h"
#include  "fs_dev_sd_spi.h"


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

#define  FS_DEV_SD_SPI_CMD_TIMEOUT                       128u

#define  FS_DEV_SD_SPI_RESP_EMPTY                       0xFFu

                                                                /* ---------- DATA RESPONSE TOKENS (7.3.3.1) ---------- */
#define  FS_DEV_SD_SPI_TOKEN_RESP_ACCEPTED              0x05u
#define  FS_DEV_SD_SPI_TOKEN_RESP_CRC_REJECTED          0x0Bu
#define  FS_DEV_SD_SPI_TOKEN_RESP_WR_REJECTED           0x0Du
#define  FS_DEV_SD_SPI_TOKEN_RESP_MASK                  0x0Fu


                                                                /* -------------- BLOCK TOKENS (7.3.3.2) -------------- */
#define  FS_DEV_SD_SPI_TOKEN_START_BLK_MULT             0xFCu
#define  FS_DEV_SD_SPI_TOKEN_STOP_TRAN                  0xFDu
#define  FS_DEV_SD_SPI_TOKEN_START_BLK                  0xFEu


                                                                /* ------------ DATA ERROR TOKENS (7.3.3.3) ----------- */
#define  FS_DEV_SD_SPI_TOKEN_ERR_ERROR                  0x01u
#define  FS_DEV_SD_SPI_TOKEN_ERR_CC_ERROR               0x02u
#define  FS_DEV_SD_SPI_TOKEN_ERR_CARD_ECC_ERROR         0x04u
#define  FS_DEV_SD_SPI_TOKEN_ERR_OUT_OF_RANGE           0x08u

/*
*********************************************************************************************************
*                                       R1 RESPONSE BIT DEFINES
*
* Note(s) : (1) See [Ref 1], Figure 7-9.
*********************************************************************************************************
*/

#define  FS_DEV_SD_SPI_R1_NONE                   DEF_BIT_NONE   /* In ready state.                                      */
#define  FS_DEV_SD_SPI_R1_IN_IDLE_STATE          DEF_BIT_00     /* In idle state.                                       */
#define  FS_DEV_SD_SPI_R1_ERASE_RESET            DEF_BIT_01     /* Erase sequence clr'd before executing.               */
#define  FS_DEV_SD_SPI_R1_ILLEGAL_COMMAND        DEF_BIT_02     /* Illegal command code detected.                       */
#define  FS_DEV_SD_SPI_R1_COM_CRC_ERROR          DEF_BIT_03     /* CRC check of last command failed.                    */
#define  FS_DEV_SD_SPI_R1_ERASE_SEQUENCE_ERROR   DEF_BIT_04     /* Error in sequence of erase commands occurred.        */
#define  FS_DEV_SD_SPI_R1_ADDRESS_ERROR          DEF_BIT_05     /* Misaligned address that did not match block length.  */
#define  FS_DEV_SD_SPI_R1_PARAMETER_ERROR        DEF_BIT_06     /* Command's argument outside allowed range for card.   */

/*
*********************************************************************************************************
*                                       R2 RESPONSE BIT DEFINES
*
* Note(s) : (1) See [Ref 1], Figure 7-10.
*********************************************************************************************************
*/

#define  FS_DEV_SD_SPI_R2_CARD_IS_LOCKED         DEF_BIT_00     /* Card is locked.                                      */
#define  FS_DEV_SD_SPI_R2_WP_ERASE_SKIP          DEF_BIT_01     /* Attempt to erase a write protected sector.           */
#define  FS_DEV_SD_SPI_R2_LOCK_CMD_FAILED        DEF_BIT_01     /* Password error during lock/unlock operation.         */
#define  FS_DEV_SD_SPI_R2_ERROR                  DEF_BIT_02     /* General or unknown error during the operation.       */
#define  FS_DEV_SD_SPI_R2_CC_ERROR               DEF_BIT_03     /* Internal card controller error.                      */
#define  FS_DEV_SD_SPI_R2_CARD_ECC_FAILED        DEF_BIT_04     /* Card internal ECC applied, failed to correct data.   */
#define  FS_DEV_SD_SPI_R2_WP_VIOLATION           DEF_BIT_05     /* Tried to write a write-protected block.              */
#define  FS_DEV_SD_SPI_R2_ERASE_PARAM            DEF_BIT_06     /* Invalid selection for erase, sectors or groups.      */
#define  FS_DEV_SD_SPI_R2_OUT_OF_RANGE           DEF_BIT_07     /* Parameter out of range.                              */
#define  FS_DEV_SD_SPI_R2_CSD_OVERWRITE          DEF_BIT_07     /* CSD overwritten.                                     */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        SD SPI DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_sd_spi_data  FS_DEV_SD_SPI_DATA;
struct  fs_dev_sd_spi_data {
    FS_TYPE              Type;
    FS_QTY               UnitNbr;
    CPU_BOOLEAN          Init;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR               StatRdCtr;
    FS_CTR               StatWrCtr;
#endif

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
    FS_CTR               ErrRdCtr;
    FS_CTR               ErrWrCtr;

    FS_CTR               ErrCmdRespCtr;
    FS_CTR               ErrCmdRespTimeoutCtr;
    FS_CTR               ErrCmdRespEraseResetCtr;
    FS_CTR               ErrCmdRespIllegalCmdCtr;
    FS_CTR               ErrCmdRespCommCRC_Ctr;
    FS_CTR               ErrCmdRespEraseSeqCtr;
    FS_CTR               ErrCmdRespAddrCtr;
    FS_CTR               ErrCmdRespParamCtr;
#endif

    FS_DEV_SD_INFO       Info;

    FS_DEV_SD_SPI_DATA  *NextPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  FS_DEV_SD_SPI_NAME_LEN             2u

static  const  CPU_CHAR  FSDev_SD_SPI_Name[] = "sd";


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

static  FS_DEV_SD_SPI_DATA  *FSDev_SD_SPI_ListFreePtr;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      SPI ENTER & EXIT MACRO'S
*
* Note(s) : (1) According to [Ref 2], Section 9.49, eight clocks (i.e., one byte 'clocked') must be
*               provided after the reception of a response, reading the final byte of a data block
*               during a read data transaction or receiving the status at the end of a write data
*               transaction.
*********************************************************************************************************
*/

#define  FS_DEV_SD_SPI_ENTER(unit_nbr)      FSDev_SD_SPI_BSP_SPI.ChipSelEn((unit_nbr));                 \
                                            FSDev_SD_SPI_WaitClks((unit_nbr), 1u);

#define  FS_DEV_SD_SPI_EXIT(unit_nbr)       FSDev_SD_SPI_WaitClks((unit_nbr), 1u);                      \
                                            FSDev_SD_SPI_BSP_SPI.ChipSelDis((unit_nbr));

/*
*********************************************************************************************************
*                                           COUNTER MACRO'S
*********************************************************************************************************
*/

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* ----------------- STAT CTR MACRO'S ----------------- */

#define  FS_DEV_SD_SPI_STAT_RD_CTR_INC(p_sd_spi_data)                    {  FS_CTR_STAT_INC((p_sd_spi_data)->StatRdCtr);                }
#define  FS_DEV_SD_SPI_STAT_WR_CTR_INC(p_sd_spi_data)                    {  FS_CTR_STAT_INC((p_sd_spi_data)->StatWrCtr);                }

#define  FS_DEV_SD_SPI_STAT_RD_CTR_ADD(p_sd_spi_data, val)               {  FS_CTR_STAT_ADD((p_sd_spi_data)->StatRdCtr, (FS_CTR)(val)); }
#define  FS_DEV_SD_SPI_STAT_WR_CTR_ADD(p_sd_spi_data, val)               {  FS_CTR_STAT_ADD((p_sd_spi_data)->StatWrCtr, (FS_CTR)(val)); }

#else

#define  FS_DEV_SD_SPI_STAT_RD_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_STAT_WR_CTR_INC(p_sd_spi_data)

#define  FS_DEV_SD_SPI_STAT_RD_CTR_ADD(p_sd_spi_data, val)
#define  FS_DEV_SD_SPI_STAT_WR_CTR_ADD(p_sd_spi_data, val)

#endif


#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* ------------------ ERR CTR MACRO'S ----------------- */

#define  FS_DEV_SD_SPI_ERR_RD_CTR_INC(p_sd_spi_data)                     {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrRdCtr);                  }
#define  FS_DEV_SD_SPI_ERR_WR_CTR_INC(p_sd_spi_data)                     {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrWrCtr);                  }

#define  FS_DEV_SD_SPI_ERR_CMD_RESP_TIMEOUT_CTR_INC(p_sd_spi_data)       {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespTimeoutCtr);      }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_RESET_ERASE_CTR_INC(p_sd_spi_data)   {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespEraseResetCtr);   }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_ILLEGAL_CMD_CTR_INC(p_sd_spi_data)   {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespIllegalCmdCtr);   }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_COMM_CRC_INC(p_sd_spi_data)          {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespCommCRC_Ctr);     }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_SEQ_ERASE_CTR_INC(p_sd_spi_data)     {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespEraseSeqCtr);     }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_ADDR_CTR_INC(p_sd_spi_data)          {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespAddrCtr);         }
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_PARAM_CTR_INC(p_sd_spi_data)         {  FS_CTR_ERR_INC((p_sd_spi_data)->ErrCmdRespParamCtr);        }

#else

#define  FS_DEV_SD_SPI_ERR_RD_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_WR_CTR_INC(p_sd_spi_data)

#define  FS_DEV_SD_SPI_ERR_CMD_RESP_TIMEOUT_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_RESET_ERASE_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_ILLEGAL_CMD_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_COMM_CRC_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_SEQ_ERASE_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_ADDR_CTR_INC(p_sd_spi_data)
#define  FS_DEV_SD_SPI_ERR_CMD_RESP_PARAM_CTR_INC(p_sd_spi_data)

#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* --------------- DEV INTERFACE FNCTS ---------------- */
                                                                /* Get name of driver.                                  */
static  const  CPU_CHAR     *FSDev_SD_SPI_NameGet          (void);

                                                                /* Init driver.                                         */
static  void                 FSDev_SD_SPI_Init             (FS_ERR                       *p_err);

                                                                /* Open device.                                         */
static  void                 FSDev_SD_SPI_Open             (FS_DEV                       *p_dev,
                                                            void                         *p_dev_cfg,
                                                            FS_ERR                       *p_err);

                                                                /* Close device.                                        */
static  void                 FSDev_SD_SPI_Close            (FS_DEV                       *p_dev);

                                                                /* Read from device.                                    */
static  void                 FSDev_SD_SPI_Rd               (FS_DEV                       *p_dev,
                                                            void                         *p_dest,
                                                            FS_SEC_NBR                    start,
                                                            FS_SEC_QTY                    cnt,
                                                            FS_ERR                       *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Write to device.                                     */
static  void                 FSDev_SD_SPI_Wr               (FS_DEV                       *p_dev,
                                                            void                         *p_src,
                                                            FS_SEC_NBR                    start,
                                                            FS_SEC_QTY                    cnt,
                                                            FS_ERR                       *p_err);
#endif

                                                                /* Get device info.                                     */
static  void                 FSDev_SD_SPI_Query            (FS_DEV                       *p_dev,
                                                            FS_DEV_INFO                  *p_info,
                                                            FS_ERR                       *p_err);

                                                                /* Perform device I/O control.                          */
static  void                 FSDev_SD_SPI_IO_Ctrl          (FS_DEV                       *p_dev,
                                                            CPU_INT08U                    opt,
                                                            void                         *p_data,
                                                            FS_ERR                       *p_err);

                                                                /* ------------------- LOCAL FNCTS -------------------- */
                                                                /* Refresh dev.                                         */
static  CPU_BOOLEAN          FSDev_SD_SPI_Refresh          (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            FS_ERR                       *p_err);

                                                                /* Make card ready & get card type.                     */
static  CPU_INT08U           FSDev_SD_SPI_MakeRdy          (FS_DEV_SD_SPI_DATA           *p_sd_spi_data);



                                                                /* Perform command & read resp byte.                    */
static  CPU_INT08U           FSDev_SD_SPI_Cmd              (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg);

                                                                /* Perform command & get R1 resp.                       */
static  CPU_INT08U           FSDev_SD_SPI_CmdR1            (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg);

                                                                /* Perform command & get R3/R7 resp.                    */
static  CPU_INT08U           FSDev_SD_SPI_CmdR3_7          (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg,
                                                            CPU_INT08U                    resp[]);

                                                                /* Perform ACMD & get R1 resp.                          */
static  CPU_INT08U           FSDev_SD_SPI_AppCmdR1         (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    acmd,
                                                            CPU_INT32U                    arg);

                                                                /* Exec cmd & rd data blk.                              */
static  CPU_BOOLEAN          FSDev_SD_SPI_RdData           (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg,
                                                            CPU_INT08U                   *p_dest,
                                                            CPU_INT32U                    size);

                                                                /* Exec cmd & rd mult data blks.                        */
static  CPU_BOOLEAN          FSDev_SD_SPI_RdDataMulti      (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg,
                                                            CPU_INT08U                   *p_dest,
                                                            CPU_INT32U                    size,
                                                            CPU_INT32U                    cnt);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Exec cmd & wr data blk.                              */
static  CPU_BOOLEAN          FSDev_SD_SPI_WrData           (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg,
                                                            CPU_INT08U                   *p_src,
                                                            CPU_INT32U                    size);

                                                                /* Exec cmd & wr mult data blks.                        */
static  CPU_BOOLEAN          FSDev_SD_SPI_WrDataMulti      (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                    cmd,
                                                            CPU_INT32U                    arg,
                                                            CPU_INT08U                   *p_src,
                                                            CPU_INT32U                    size,
                                                            CPU_INT32U                    cnt);
#endif



                                                                /* Get CID reg.                                         */
static  CPU_BOOLEAN          FSDev_SD_SPI_SendCID          (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                   *p_dest);

                                                                /* Get CSD reg.                                         */
static  CPU_BOOLEAN          FSDev_SD_SPI_SendCSD          (FS_DEV_SD_SPI_DATA           *p_sd_spi_data,
                                                            CPU_INT08U                   *p_dest);


                                                                /* Rd one byte from card.                               */
static  CPU_INT08U           FSDev_SD_SPI_RdByte           (FS_QTY                        unit_nbr);

                                                                /* Wait certain number of clocks.                       */
static  void                 FSDev_SD_SPI_WaitClks         (FS_QTY                        unit_nbr,
                                                            CPU_INT08U                    nbr_clks);

                                                                /* Wait for card to rtn start token.                    */
static  CPU_INT08U           FSDev_SD_SPI_WaitForStart     (FS_QTY                        unit_nbr);

                                                                /* Wait while card rtns busy token.                     */
static  CPU_INT08U           FSDev_SD_SPI_WaitWhileBusy    (FS_QTY                        unit_nbr);


                                                                /* Free SD SPI data.                                    */
static  void                 FSDev_SD_SPI_DataFree         (FS_DEV_SD_SPI_DATA           *p_sd_spi_data);

                                                                /* Allocate & init SD SPI data.                         */
static  FS_DEV_SD_SPI_DATA  *FSDev_SD_SPI_DataGet          (void);

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_API  FSDev_SD_SPI = {
    FSDev_SD_SPI_NameGet,
    FSDev_SD_SPI_Init,
    FSDev_SD_SPI_Open,
    FSDev_SD_SPI_Close,
    FSDev_SD_SPI_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_SD_SPI_Wr,
#endif
    FSDev_SD_SPI_Query,
    FSDev_SD_SPI_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_QuerySD()
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
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sd:0:").
*********************************************************************************************************
*/

void  FSDev_SD_SPI_QuerySD (CPU_CHAR        *name_dev,
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
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_SPI_Name, FS_DEV_SD_SPI_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_SPI_NAME_LEN] != FS_CHAR_DEV_SEP) {
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
*                                        FSDev_SD_SPI_RdCID()
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
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sd:0:").
*
*               (2) (a) For SD cards, the structure of the CID is defined in [Ref 1] Section 5.1.
*
*                   (b) For MMC cards, the structure of the CID is defined in [Ref 2] Section 8.2.
*********************************************************************************************************
*/

void  FSDev_SD_SPI_RdCID (CPU_CHAR    *name_dev,
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
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_SPI_Name, FS_DEV_SD_SPI_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_SPI_NAME_LEN] != FS_CHAR_DEV_SEP) {
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
*                                        FSDev_SD_SPI_RdCSD()
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
* Note(s)     : (1) The device MUST be a SD/MMC device (e.g., "sd:0:").
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

void  FSDev_SD_SPI_RdCSD (CPU_CHAR    *name_dev,
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
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_SD_SPI_Name, FS_DEV_SD_SPI_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_SD_SPI_NAME_LEN] != FS_CHAR_DEV_SEP) {
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
*                                       FSDev_SD_SPI_NameGet()
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

static  const  CPU_CHAR  *FSDev_SD_SPI_NameGet (void)
{
    return (FSDev_SD_SPI_Name);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_SPI_Init()
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

static  void  FSDev_SD_SPI_Init (FS_ERR  *p_err)
{
    FSDev_SD_SPI_UnitCtr     =  0u;
    FSDev_SD_SPI_ListFreePtr = (FS_DEV_SD_SPI_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_SD_SPI_Open()
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

static  void  FSDev_SD_SPI_Open (FS_DEV  *p_dev,
                                 void    *p_dev_cfg,
                                 FS_ERR  *p_err)
{
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;


   (void)p_dev_cfg;                                             /*lint --e{550} Suppress "Symbol not accessed".         */

    p_sd_spi_data = FSDev_SD_SPI_DataGet();
    if (p_sd_spi_data == (FS_DEV_SD_SPI_DATA *)0) {
      *p_err = FS_ERR_MEM_ALLOC;
       return;
    }

    p_dev->DataPtr         = (void *)p_sd_spi_data;
    p_sd_spi_data->UnitNbr =  p_dev->UnitNbr;

   (void)FSDev_SD_SPI_Refresh(p_sd_spi_data, p_err);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_Close()
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

static  void  FSDev_SD_SPI_Close (FS_DEV  *p_dev)
{
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;


    p_sd_spi_data = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
    if (p_sd_spi_data->Init == DEF_YES) {
        FSDev_SD_SPI_BSP_SPI.Close(p_sd_spi_data->UnitNbr);
    }
    FSDev_SD_SPI_DataFree(p_sd_spi_data);
}


/*
*********************************************************************************************************
*                                          FSDev_SD_SPI_Rd()
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
*********************************************************************************************************
*/

static  void  FSDev_SD_SPI_Rd (FS_DEV      *p_dev,
                               void        *p_dest,
                               FS_SEC_NBR   start,
                               FS_SEC_QTY   cnt,
                               FS_ERR      *p_err)
{
    CPU_BOOLEAN          ok;
    CPU_INT32U           start_addr;
    CPU_INT08U          *p_dest_08;
    FS_DEV_SD_INFO      *p_sd_info;
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;
    FS_QTY               unit_nbr;


                                                                /* ------------------ PREPARE FOR RD ------------------ */
    unit_nbr      =  p_dev->UnitNbr;
    p_dest_08     = (CPU_INT08U         *)p_dest;
    p_sd_spi_data = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
    p_sd_info     = &p_sd_spi_data->Info;
                                                                /* See Note #2.                                         */
    start_addr    = (p_sd_info->HighCapacity == DEF_YES) ? start : (start * FS_DEV_SD_BLK_SIZE);

    FSDev_SD_SPI_BSP_SPI.Lock(unit_nbr);


    if (cnt > 1u) {                                             /* ---------------- PERFORM MULTIPLE RD --------------- */
        ok = FSDev_SD_SPI_RdDataMulti(p_sd_spi_data,            /* Rd data.                                             */
                                      FS_DEV_SD_CMD_READ_MULTIPLE_BLOCK,
                                      start_addr,
                                      p_dest_08,
                                      FS_DEV_SD_BLK_SIZE,
                                      cnt);



    } else {                                                    /* ----------------- PERFORM SINGLE RD ---------------- */
        ok = FSDev_SD_SPI_RdData(p_sd_spi_data,                 /* Rd data.                                             */
                                 FS_DEV_SD_CMD_READ_SINGLE_BLOCK,
                                 start_addr,
                                 p_dest_08,
                                 FS_DEV_SD_BLK_SIZE);
    }



    if (ok != DEF_OK) {                                         /* ---------------------- CHK ERR --------------------- */
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FS_DEV_SD_SPI_ERR_RD_CTR_INC(p_sd_spi_data);
        FS_TRACE_DBG(("FSDev_SD_SPI_Rd(): Failed to read card.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    FS_DEV_SD_SPI_STAT_RD_CTR_ADD(p_sd_spi_data, cnt);



                                                                /* ----------------- RELEASE BUS LOCK ----------------- */
    FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_SD_SPI_Wr()
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
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_SD_SPI_Wr (FS_DEV      *p_dev,
                               void        *p_src,
                               FS_SEC_NBR   start,
                               FS_SEC_QTY   cnt,
                               FS_ERR      *p_err)
{
    CPU_BOOLEAN          ok;
    CPU_INT32U           start_addr;
    FS_DEV_SD_INFO      *p_sd_info;
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;
    CPU_INT08U          *p_src_08;
    FS_QTY               unit_nbr;


                                                                /* ------------------ PREPARE FOR WR ------------------ */
    unit_nbr      =  p_dev->UnitNbr;
    p_src_08      = (CPU_INT08U         *)p_src;
    p_sd_spi_data = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
    p_sd_info     = &p_sd_spi_data->Info;
                                                                /* See Note #2.                                         */
    start_addr    = (p_sd_info->HighCapacity == DEF_YES) ? start : (start * FS_DEV_SD_BLK_SIZE);

    FSDev_SD_SPI_BSP_SPI.Lock(unit_nbr);


    if (cnt > 1u) {                                             /* ---------------- PERFORM MULTIPLE WR --------------- */
        ok = FSDev_SD_SPI_WrDataMulti(p_sd_spi_data,            /* Wr data.                                             */
                                      FS_DEV_SD_CMD_WRITE_MULTIPLE_BLOCK,
                                      start_addr,
                                      p_src_08,
                                      FS_DEV_SD_BLK_SIZE,
                                      cnt);



    } else {                                                    /* ----------------- PERFORM SINGLE WR ---------------- */
        ok = FSDev_SD_SPI_WrData(p_sd_spi_data,                 /* Wr data.                                             */
                                 FS_DEV_SD_CMD_WRITE_BLOCK,
                                 start_addr,
                                 p_src_08,
                                 FS_DEV_SD_BLK_SIZE);
    }



    if (ok != DEF_OK) {                                         /* ---------------------- CHK ERR --------------------- */
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FS_DEV_SD_SPI_ERR_WR_CTR_INC(p_sd_spi_data);
        FS_TRACE_DBG(("FSDev_SD_SPI_Wr(): Failed to write card.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    FS_DEV_SD_SPI_STAT_WR_CTR_ADD(p_sd_spi_data, cnt);



                                                                /* ----------------- RELEASE BUS LOCK ----------------- */
    FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_Query()
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

static  void  FSDev_SD_SPI_Query (FS_DEV       *p_dev,
                                  FS_DEV_INFO  *p_info,
                                  FS_ERR       *p_err)
{
    FS_DEV_SD_INFO      *p_sd_info;
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;


    p_sd_spi_data   = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
    p_sd_info       = &p_sd_spi_data->Info;

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
*                                       FSDev_SD_SPI_IO_Ctrl()
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
*********************************************************************************************************
*/

static  void  FSDev_SD_SPI_IO_Ctrl (FS_DEV      *p_dev,
                                    CPU_INT08U   opt,
                                    void        *p_data,
                                    FS_ERR      *p_err)
{
    FS_DEV_SD_INFO      *p_sd_info;
    FS_DEV_SD_INFO      *p_sd_info_ctrl;
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;
    CPU_BOOLEAN          ok;
    CPU_BOOLEAN          chngd;
    CPU_BOOLEAN         *p_chngd;


    switch (opt) {
        case FS_DEV_IO_CTRL_REFRESH:                            /* -------------------- RE-OPEN DEV ------------------- */
             p_chngd       = (CPU_BOOLEAN *)p_data;
             p_sd_spi_data = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
             chngd         = FSDev_SD_SPI_Refresh(p_sd_spi_data, p_err);
            *p_chngd       = chngd;
             break;


        case FS_DEV_IO_CTRL_SD_QUERY:                           /* ------------ GET INFO ABOUT SD/MMC CARD ------------ */
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
                                                                /* Validate dev init.                                   */
             p_sd_spi_data  = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
             if (p_sd_spi_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }
                                                                /* Copy info.                                           */
             p_sd_info                    = &p_sd_spi_data->Info;
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
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
                                                                /* Validate dev init.                                   */
             p_sd_spi_data  = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
             if (p_sd_spi_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }

                                                                /* Rd CID reg.                                          */
             FSDev_SD_SPI_BSP_SPI.Lock(p_sd_spi_data->UnitNbr);
		     ok = FSDev_SD_SPI_SendCID(              p_sd_spi_data,
                                       (CPU_INT08U *)p_data);
             FSDev_SD_SPI_BSP_SPI.Unlock(p_sd_spi_data->UnitNbr);

             if (ok == DEF_NO) {
                *p_err = FS_ERR_DEV_IO;
                 return;
             }

            *p_err = FS_ERR_NONE;
             break;


        case FS_DEV_IO_CTRL_SD_RD_CSD:                          /* --------- READ CARD-SPECIFIC DATA REGISTER --------- */
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
                                                                /* Validate dev init.                                   */
             p_sd_spi_data  = (FS_DEV_SD_SPI_DATA *)p_dev->DataPtr;
             if (p_sd_spi_data->Init == DEF_NO) {
                 *p_err = FS_ERR_DEV_NOT_PRESENT;
                  return;
             }

                                                                /* Rd CSD reg.                                          */
             FSDev_SD_SPI_BSP_SPI.Lock(p_sd_spi_data->UnitNbr);
             ok = FSDev_SD_SPI_SendCSD(              p_sd_spi_data,
                                       (CPU_INT08U *)p_data);
             FSDev_SD_SPI_BSP_SPI.Unlock(p_sd_spi_data->UnitNbr);

             if (ok == DEF_NO) {
                *p_err = FS_ERR_DEV_IO;
                 return;
             }

            *p_err = FS_ERR_NONE;
             break;


        case FS_DEV_IO_CTRL_LOW_FMT:                            /* --------------- UNSUPPORTED I/O CTRL --------------- */
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
*                                       FSDev_SD_SPI_Refresh()
*
* Description : Refresh a device.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
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
*            |              |          |                                   | YES
* NO         |              | BUSY     |                                   |
* RESP       v              |          |                                   v
*  +---CARD IS READY?-------+          |                                CMD58
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
*
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_SPI_Refresh (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                           FS_ERR              *p_err)
{
    CPU_INT08U       card_type;
    CPU_INT08U       resp_r1;
    CPU_INT08U       resp_reg[16];
    CPU_INT08U       retries;
    CPU_BOOLEAN      ok;
    CPU_BOOLEAN      init_prev;
    FS_QTY           unit_nbr;
    FS_DEV_SD_INFO  *p_sd_info;
    FS_DEV_SD_INFO   sd_info;


                                                                /* ------------------- CHK DEV CHNGD ------------------ */
    p_sd_info = &(p_sd_spi_data->Info);
    unit_nbr  = p_sd_spi_data->UnitNbr;
    if (p_sd_spi_data->Init == DEF_YES) {                       /* If dev already init'd ...                            */
        FSDev_SD_ClrInfo(&sd_info);

        FSDev_SD_SPI_BSP_SPI.Lock(unit_nbr);
        ok = FSDev_SD_SPI_SendCID( p_sd_spi_data,               /* ... rd CID reg        ...                            */
                                  &resp_reg[0]);
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);

        if (ok == DEF_YES) {                                    /* ... parse CID info    ...                            */
            (void)FSDev_SD_ParseCID( resp_reg,
                                    &sd_info,
                                     p_sd_info->CardType);


            if ((sd_info.ManufID == p_sd_info->ManufID) &&      /* ... & if info is same ...                            */
                (sd_info.OEM_ID  == p_sd_info->OEM_ID)  &&
                (sd_info.ProdSN  == p_sd_info->ProdSN)) {
               *p_err = FS_ERR_NONE;                            /* ... dev has not chngd.                               */
                return (DEF_NO);
            }
        }

        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);                   /* Uninit HW.                                           */
    }



                                                                /* ---------------------- INIT HW --------------------- */
    init_prev                = p_sd_spi_data->Init;             /* If init'd prev'ly, dev chngd.                        */
    p_sd_spi_data->Init      = DEF_NO;                          /* No longer init'd.                                    */
    FSDev_SD_ClrInfo(p_sd_info);                                /* Clr old info.                                        */

    ok = FSDev_SD_SPI_BSP_SPI.Open(unit_nbr);                   /* Init HW.                                             */
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return (init_prev);
    }

    FSDev_SD_SPI_BSP_SPI.Lock(unit_nbr);
                                                                /* Set clk to dflt clk.                                 */
    FSDev_SD_SPI_BSP_SPI.SetClkFreq(unit_nbr, (CPU_INT32U)FS_DEV_SD_DFLT_CLK_SPD);



                                                                /* -------------- SEND CMD0 TO RESET BUS -------------- */
    retries = 0u;
    resp_r1 = FS_DEV_SD_SPI_RESP_EMPTY;
    while ((retries  < FS_DEV_SD_SPI_CMD_TIMEOUT) &&
           (resp_r1 != FS_DEV_SD_SPI_R1_IN_IDLE_STATE)) {
        FS_DEV_SD_SPI_ENTER(unit_nbr);                          /* 'Enter' SPI access.                                  */
        FSDev_SD_SPI_WaitClks(unit_nbr, (CPU_INT08U)20);        /* Send empty clks.                                     */

        resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,               /* Perform SD CMD0.                                     */
                                   FS_DEV_SD_CMD_GO_IDLE_STATE,
                                   0u);

        FS_DEV_SD_SPI_EXIT(unit_nbr);                           /* 'Exit' SPI access.                                   */

        if (resp_r1 != FS_DEV_SD_SPI_R1_IN_IDLE_STATE) {
            FS_OS_Dly_ms(2u);
            retries++;
        }
    }

    if (resp_r1 != FS_DEV_SD_SPI_R1_IN_IDLE_STATE) {            /* If card never responded ...                          */
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;                                  /* ... rtn err.                                         */
        return (init_prev);
    }

    FS_OS_Dly_ms(100u);



                                                                /* ---------------- DETERMINE CARD TYPE --------------- */
    card_type = FSDev_SD_SPI_MakeRdy(p_sd_spi_data);            /* Make card rdy & determine card type.                 */
    if (card_type == FS_DEV_SD_CARDTYPE_NONE) {                 /* If card type NOT determined ...                      */
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;                                  /* ... rtn err.                                         */
        return (init_prev);
    }

    p_sd_info->CardType = card_type;

    if ((card_type == FS_DEV_SD_CARDTYPE_SD_V2_0_HC) ||
        (card_type == FS_DEV_SD_CARDTYPE_MMC_HC)) {
        p_sd_info->HighCapacity = DEF_YES;
    }



                                                                /* -------------------- RD CSD REG -------------------- */
    ok = FSDev_SD_SPI_SendCSD( p_sd_spi_data,                   /* Get CSD reg from card.                               */
                              &resp_reg[0]);
    if (ok != DEF_OK) {
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    ok = FSDev_SD_ParseCSD(resp_reg,                            /* Parse CSD info.                                      */
                           p_sd_info,
                           card_type);
    if (ok != DEF_OK) {
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }


                                                                /* Set clk freq.                                        */
    FSDev_SD_SPI_BSP_SPI.SetClkFreq(unit_nbr, p_sd_info->ClkFreq);



                                                                /* -------------------- RD CID REG -------------------- */
    ok = FSDev_SD_SPI_SendCID( p_sd_spi_data,                   /* Get CID reg from card.                               */
                              &resp_reg[0]);

    if (ok != DEF_OK) {
        FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);
        FSDev_SD_SPI_BSP_SPI.Close(unit_nbr);
       *p_err = FS_ERR_DEV_IO;
        return (init_prev);
    }

    p_sd_spi_data->Init = DEF_YES;

    (void)FSDev_SD_ParseCID(resp_reg,                           /* Parse CID info.                                      */
                            p_sd_info,
                            card_type);



                                                                /* -------------------- SET BLK LEN ------------------- */
    resp_r1 = FSDev_SD_SPI_CmdR1(p_sd_spi_data,                 /* Perform CMD16.                                       */
                                 FS_DEV_SD_CMD_SET_BLOCKLEN,
                                 FS_DEV_SD_BLK_SIZE);

    if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
        FS_TRACE_INFO(("FSDev_SD_SPI_Refresh(): Could not set block length: error: %02X.\r\n", resp_r1));
    }




#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)                   /* -------------- ENABLE CRC VERIFICATION ------------- */
    resp_r1 = FSDev_SD_SPI_CmdR1(p_sd_spi_data,
                                 FS_DEV_SD_CMD_CRC_ON_OFF,
                                 FS_DEV_SD_CMD59_CRC_OPT);

    if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
        FS_TRACE_INFO(("FSDev_SD_SPI_Refresh(): Could not enable CRC: error: %02X.\r\n", resp_r1));
    }
#endif

    FSDev_SD_SPI_BSP_SPI.Unlock(unit_nbr);



                                                                /* ----------------- OUTPUT TRACE INFO ---------------- */
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    FSDev_SD_TraceInfo(p_sd_info);
#endif

   *p_err = FS_ERR_NONE;
    return (DEF_YES);                                           /* Always chngd if opened.                              */
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_MakeRdy()
*
* Description : Move card into 'ready' state (& get card type).
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
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
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_MakeRdy (FS_DEV_SD_SPI_DATA  *p_sd_spi_data)
{
    CPU_INT08U  card_type;
    CPU_INT08U  resp_r1;
    CPU_INT08U  resp_r3_7[4];
    CPU_INT08U  retries;


                                                                /* ------------- SEND CMD8 TO GET CARD OP ------------- */
    resp_r1 = FSDev_SD_SPI_CmdR3_7(p_sd_spi_data,               /* Perform CMD8.                                        */
                                   FS_DEV_SD_CMD_SEND_IF_COND,
                                   FS_DEV_SD_CMD8_VHS_27_36_V | FS_DEV_SD_CMD8_CHK_PATTERN,
                                   resp_r3_7);

                                                                /* If card does resp, v2.0 card.                        */
    card_type = FS_DEV_SD_CARDTYPE_SD_V1_X;
    if (resp_r1 == FS_DEV_SD_SPI_R1_IN_IDLE_STATE) {
        if (resp_r3_7[3] == FS_DEV_SD_CMD8_CHK_PATTERN) {
            card_type = FS_DEV_SD_CARDTYPE_SD_V2_0;
        }
    }



                                                                /* ---------- SEND ACMD41 TO GET CARD STATUS ---------- */
    retries = 0u;
    resp_r1 = FS_DEV_SD_SPI_RESP_EMPTY;
    while (retries < FS_DEV_SD_SPI_CMD_TIMEOUT) {
        resp_r1 = FSDev_SD_SPI_AppCmdR1(p_sd_spi_data,          /* Perform ACMD41.                                      */
                                        FS_DEV_SD_ACMD_SD_SEND_OP_COND,
                                        FS_DEV_SD_ACMD41_HCS);

        if (resp_r1 == FS_DEV_SD_SPI_R1_NONE) {
            break;
        }

        FS_OS_Dly_ms(2u);

        retries++;
    }



                                                                /* ------------- SEND CMD1 (FOR MMC CARD) ------------- */
    if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
        retries = 0u;
        while (retries < FS_DEV_SD_SPI_CMD_TIMEOUT) {
            resp_r1 = FSDev_SD_SPI_CmdR1(p_sd_spi_data,
                                         FS_DEV_SD_CMD_SEND_OP_COND,
                                         0u);

            if (resp_r1 == FS_DEV_SD_SPI_R1_NONE) {
                break;
            }

            FS_OS_Dly_ms(2u);
            retries++;
        }

        if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
            return (FS_DEV_SD_CARDTYPE_NONE);
        }

        card_type = FS_DEV_SD_CARDTYPE_MMC;
    }


                                                                /* ------------ SEND CMD58 TO GET CARD OCR ------------ */
    retries = 0u;
    while (retries < FS_DEV_SD_SPI_CMD_TIMEOUT) {
        resp_r1 = FSDev_SD_SPI_CmdR3_7(p_sd_spi_data,           /* Perform CMD58.                                       */
                                       FS_DEV_SD_CMD_READ_OCR,
                                       DEF_BIT_30,
                                       resp_r3_7);

        if (resp_r1 == FS_DEV_SD_SPI_R1_NONE) {
            if (DEF_BIT_IS_SET(resp_r3_7[0], (FS_DEV_SD_OCR_BUSY >> (3u * DEF_OCTET_NBR_BITS))) == DEF_YES) {
                                                                /* Chk for HC card.                                     */
                if (DEF_BIT_IS_SET(resp_r3_7[0], (FS_DEV_SD_OCR_CCS >> (3u * DEF_OCTET_NBR_BITS))) == DEF_YES) {
                    if (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0) {
                        card_type =  FS_DEV_SD_CARDTYPE_SD_V2_0_HC;
                    } else {
                        if (card_type == FS_DEV_SD_CARDTYPE_MMC) {
                            card_type =  FS_DEV_SD_CARDTYPE_MMC_HC;
                        }
                    }
                }
                break;
            }
        }

        retries++;
    }


    return (card_type);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_SPI_Cmd()
*
* Description : Perform command & get response byte.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command index.
*
*               arg             Command argument.
*
* Return(s)   : Command response.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_Cmd (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                      CPU_INT08U           cmd,
                                      CPU_INT32U           arg)
{
    CPU_INT08U  cmd_pkt[6];
    CPU_INT08U  chk_sum;
    CPU_INT16U  timeout;
    CPU_INT08U  resp;



                                                                /* --------------------- FORM CMD --------------------- */
    cmd_pkt[0] =  DEF_BIT_06 | cmd;
    cmd_pkt[1] = (CPU_INT08U)(arg >> (3u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
    cmd_pkt[2] = (CPU_INT08U)(arg >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
    cmd_pkt[3] = (CPU_INT08U)(arg >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
    cmd_pkt[4] = (CPU_INT08U)(arg >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;

    chk_sum    = FSDev_SD_ChkSumCalc_7Bit(&cmd_pkt[0], 5u);

    cmd_pkt[5] = (CPU_INT08U)(chk_sum << 1) | DEF_BIT_00;


                                                                /* ---------------------- WR CMD ---------------------- */
    FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,
                            &cmd_pkt[0],
                             6u);


                                                                /* ---------------------- RD RESP --------------------- */
    timeout = 0u;
    while (timeout < FS_DEV_SD_SPI_CMD_TIMEOUT) {
        resp = FSDev_SD_SPI_RdByte(p_sd_spi_data->UnitNbr);     /* Rd resp byte.                                        */

        if (DEF_BIT_IS_CLR(resp, DEF_BIT_07) == DEF_YES) {      /* Top bit of resp MUST be clr.                         */
            break;
        }

        timeout++;
    }

    if (timeout == FS_DEV_SD_SPI_CMD_TIMEOUT) {                 /* If timeout, rtn err.                                 */
        FS_DEV_SD_SPI_ERR_CMD_RESP_TIMEOUT_CTR_INC(p_sd_spi_data);
        return (FS_DEV_SD_SPI_RESP_EMPTY);
    }

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
    if (cmd != FS_DEV_SD_CMD_STOP_TRANSMISSION) {
        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_ERASE_RESET) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_RESET_ERASE_CTR_INC(p_sd_spi_data);
        }

        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_ILLEGAL_COMMAND) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_ILLEGAL_CMD_CTR_INC(p_sd_spi_data);
        }

        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_COM_CRC_ERROR) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_COMM_CRC_INC(p_sd_spi_data);
        }

        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_ERASE_SEQUENCE_ERROR) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_SEQ_ERASE_CTR_INC(p_sd_spi_data);
        }

        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_ADDRESS_ERROR) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_ADDR_CTR_INC(p_sd_spi_data);
        }

        if (DEF_BIT_IS_SET(resp, FS_DEV_SD_SPI_R1_PARAMETER_ERROR) == DEF_YES) {
            FS_DEV_SD_SPI_ERR_CMD_RESP_PARAM_CTR_INC(p_sd_spi_data);
        }
    }
#endif

    return (resp);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_CmdR1()
*
* Description : Perform command & get R1 response.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command index.
*
*               arg             Command argument.
*
* Return(s)   : R1 response.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_CmdR1 (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                        CPU_INT08U           cmd,
                                        CPU_INT32U           arg)
{
    CPU_INT08U  resp_r1;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);
    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Get 1st resp byte.                                   */
                               cmd,
                               arg);
    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);


    return (resp_r1);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_CmdR3_7()
*
* Description : Perform command & get R3 or R7 response.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command index.
*
*               arg             Command argument.
*
*               resp            Buffer in which response will be stored.
*
* Return(s)   : R1 response.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_CmdR3_7 (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                          CPU_INT08U           cmd,
                                          CPU_INT32U           arg,
                                          CPU_INT08U           resp[])
{
    CPU_INT08U  resp_r1;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Get 1st resp byte.                                   */
                               cmd,
                               arg);

    if ((resp_r1 != FS_DEV_SD_SPI_R1_NONE) &&
        (resp_r1 != FS_DEV_SD_SPI_R1_IN_IDLE_STATE)) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        return (resp_r1);
    }

    FSDev_SD_SPI_BSP_SPI.Rd(p_sd_spi_data->UnitNbr,             /* Rd rest of resp.                                     */
                            resp,
                            4u);

    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */
    return (resp_r1);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_AppCmdR1()
*
* Description : Perform application-specific command & get R1 response.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               acmd            Application-specific command index.
*
*               arg             Command argument.
*
* Return(s)   : Command response.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_AppCmdR1 (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                           CPU_INT08U           acmd,
                                           CPU_INT32U           arg)
{
    CPU_INT08U  resp_r1;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Next cmd is app cmd.                                 */
                               FS_DEV_SD_CMD_APP_CMD,
                               0u);

    if ((resp_r1 != FS_DEV_SD_SPI_R1_NONE) &&
        (resp_r1 != FS_DEV_SD_SPI_R1_IN_IDLE_STATE)) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        return (resp_r1);
    }

    FSDev_SD_SPI_WaitClks(p_sd_spi_data->UnitNbr, 1u);          /* Send empty clk.                                      */

    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Send app cmd.                                        */
                               acmd,
                               arg);

    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */
    return (resp_r1);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_RdData()
*
* Description : Execute command & read data block from card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command index.
*
*               arg             Command argument.
*
*               p_dest          Pointer to destination buffer.
*
*               size            Size of data block to be read, in octets.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_SPI_RdData (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                          CPU_INT08U           cmd,
                                          CPU_INT32U           arg,
                                          CPU_INT08U          *p_dest,
                                          CPU_INT32U           size)
{
    CPU_INT08U  crc_buf[2];
#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    CPU_INT16U  crc;
    CPU_INT16U  crc_chk;
#endif
    CPU_INT08U  resp_r1;
    CPU_INT08U  token;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Perform send CSD command.                            */
                               cmd,
                               arg);

    if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_RdData(): Failed to rd card: cmd err: %02X.\r\n", resp_r1));
        return (DEF_FAIL);
    }

    token = FSDev_SD_SPI_WaitForStart(p_sd_spi_data->UnitNbr);  /* Wait for start token of data block.                  */

    if (token != FS_DEV_SD_SPI_TOKEN_START_BLK) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_RdData(): Failed to rd card: start token not received.\r\n"));
        return (DEF_FAIL);
    }

    FSDev_SD_SPI_BSP_SPI.Rd(p_sd_spi_data->UnitNbr,             /* Rd rest of resp.                                     */
                            p_dest,
                            size);

    FSDev_SD_SPI_BSP_SPI.Rd( p_sd_spi_data->UnitNbr,            /* Rd CRC ...                                           */
                            &crc_buf[0],
                             2u);

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    crc     = MEM_VAL_GET_INT16U_BIG((void *)&crc_buf[0]);      /* ... & chk CRC.                                       */
    crc_chk = FSDev_SD_ChkSumCalc_16Bit(p_dest, size);

    if (crc != crc_chk) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_RdData(): CRC chk failed: %02X != %02X.\r\n", crc, crc_chk));
        return (DEF_NO);
    }
#endif

    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                     FSDev_SD_SPI_RdDataMulti()
*
* Description : Execute command & read multiple data blocks from card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command index.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_SPI_RdDataMulti (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                               CPU_INT08U           cmd,
                                               CPU_INT32U           arg,
                                               CPU_INT08U          *p_dest,
                                               CPU_INT32U           size,
                                               CPU_INT32U           cnt)
{
    CPU_INT08U  crc_buf[2];
#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    CPU_INT16U  crc;
    CPU_INT16U  crc_chk;
#endif
    CPU_INT08U  resp_r1;
    CPU_INT08U  token;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp_r1 = FSDev_SD_SPI_Cmd(p_sd_spi_data,                   /* Perform send CSD command.                            */
                               cmd,
                               arg);

    if (resp_r1 != FS_DEV_SD_SPI_R1_NONE) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_RdDataMulti(): Failed to rd card: cmd err: %02X.\r\n", resp_r1));
        return (DEF_FAIL);
    }

    while (cnt > 0u) {
                                                                /* Wait for start token of data block.                  */
        token = FSDev_SD_SPI_WaitForStart(p_sd_spi_data->UnitNbr);

        if (token != FS_DEV_SD_SPI_TOKEN_START_BLK) {
            FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);         /* 'Exit' SPI access.                                   */
            FS_TRACE_DBG(("FSDev_SD_SPI_RdDataMulti(): Failed to rd card: start token not received.\r\n"));
            return (DEF_FAIL);
        }

        FSDev_SD_SPI_BSP_SPI.Rd(p_sd_spi_data->UnitNbr,         /* Rd rest of resp.                                     */
                                p_dest,
                                size);

        FSDev_SD_SPI_BSP_SPI.Rd( p_sd_spi_data->UnitNbr,        /* Rd CRC ...                                           */
                                &crc_buf[0],
                                 2u);

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
        crc     = MEM_VAL_GET_INT16U_BIG((void *)&crc_buf[0]);
        crc_chk = FSDev_SD_ChkSumCalc_16Bit(p_dest, size);      /* ... & chk CRC.                                       */

        if (crc != crc_chk) {
            FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);         /* 'Exit' SPI access.                                   */
            FS_TRACE_DBG(("FSDev_SD_SPI_RdDataMult(): CRC chk failed: %02X != %02X.\r\n", crc, crc_chk));
            return (DEF_NO);
        }
#endif

        p_dest += size;
        cnt--;
    }

    (void)FSDev_SD_SPI_Cmd(p_sd_spi_data,                       /* Perform stop transmission command.                   */
                           FS_DEV_SD_CMD_STOP_TRANSMISSION,
                           0u);

    (void)FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr);   /* Wait while busy token rx'd.                          */
    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_WrData()
*
* Description : Execute command & write data block to card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command to execute.
*
*               arg             Command argument.
*
*               p_src           Pointer to source buffer.
*
*               size            Size of data block to be written, in octets.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSDev_SD_SPI_WrData (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                          CPU_INT08U           cmd,
                                          CPU_INT32U           arg,
                                          CPU_INT08U          *p_src,
                                          CPU_INT32U           size)
{
#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    CPU_INT16U  crc;
#endif
    CPU_INT08U  crc_buf[2];
    CPU_INT08U  resp;
    CPU_INT08U  token;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp  = FSDev_SD_SPI_Cmd(p_sd_spi_data,                     /* Wr cmd & get resp byte.                              */
                             cmd,
                             arg);

    if (resp != FS_DEV_SD_SPI_R1_NONE) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_WrData(): Failed to wr card: cmd err: %02X.\r\n", resp));
        return (DEF_FAIL);
    }

    token = FS_DEV_SD_SPI_TOKEN_START_BLK;
    FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,            /* Wr token.                                            */
                            &token,
                             1u);

    FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,            /* Wr data.                                             */
                             p_src,
                             size);

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    crc = FSDev_SD_ChkSumCalc_16Bit(p_src, size);
    MEM_VAL_SET_INT16U_BIG((void *)&crc_buf[0], crc);
#endif

    FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,            /* Wr CRC.                                              */
                            &crc_buf[0],
                             2u);

    FSDev_SD_SPI_BSP_SPI.Rd( p_sd_spi_data->UnitNbr,            /* Rd data resp token.                                  */
                            &resp,
                             1u);


                                                                /* Chk data resp token.                                 */
    if ((resp & FS_DEV_SD_SPI_TOKEN_RESP_MASK) != FS_DEV_SD_SPI_TOKEN_RESP_ACCEPTED) {
                                                                /* Wait while busy token rx'd.                          */
        (void)FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr);
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_WrData(): Failed to wr card: data resp token rx'd: %1X.\r\n", resp & FS_DEV_SD_SPI_TOKEN_RESP_MASK));
        return (DEF_FAIL);
    }

    resp = FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr);  /* Wait while busy token rx'd.                          */
    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */

    if (resp != FS_DEV_SD_SPI_RESP_EMPTY) {
        FS_TRACE_DBG(("FSDev_SD_SPI_WrData(): Failed to wr card: card busy timed out.\r\n"));
        return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                     FSDev_SD_SPI_WrDataMulti()
*
* Description : Execute command & write multiple data blocks to card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               cmd             Command to execute.
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSDev_SD_SPI_WrDataMulti (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                               CPU_INT08U           cmd,
                                               CPU_INT32U           arg,
                                               CPU_INT08U          *p_src,
                                               CPU_INT32U           size,
                                               CPU_INT32U           cnt)
{
#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
    CPU_INT16U  crc;
#endif
    CPU_INT08U  crc_buf[2];
    CPU_INT08U  resp;
    CPU_INT08U  token;


    FS_DEV_SD_SPI_ENTER(p_sd_spi_data->UnitNbr);                /* 'Enter' SPI access.                                  */

    resp = FSDev_SD_SPI_Cmd(p_sd_spi_data,                      /* Perform send CSD command.                            */
                            cmd,
                            arg);

    if (resp != FS_DEV_SD_SPI_R1_NONE) {
        FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);             /* 'Exit' SPI access.                                   */
        FS_TRACE_DBG(("FSDev_SD_SPI_WrDataMulti(): Failed to wr card: cmd err: %02X.\r\n", resp));
        return (DEF_FAIL);
    }

    while (cnt > 0u) {
        token = FS_DEV_SD_SPI_TOKEN_START_BLK_MULT;
        FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,        /* Wr token.                                            */
                                &token,
                                 1u);

        FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,        /* Wr data.                                             */
                                 p_src,
                                 size);

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
        crc = FSDev_SD_ChkSumCalc_16Bit(p_src, size);
        MEM_VAL_SET_INT16U_BIG((void *)&crc_buf[0], crc);
#endif

        FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,        /* Wr CRC.                                              */
                                &crc_buf[0],
                                 2u);

        FSDev_SD_SPI_BSP_SPI.Rd( p_sd_spi_data->UnitNbr,        /* Rd data resp token.                                  */
                                &resp,
                                 1u);

        if ((resp & FS_DEV_SD_SPI_TOKEN_RESP_MASK) != FS_DEV_SD_SPI_TOKEN_RESP_ACCEPTED) {
                                                                /* Wait while busy token rx'd.                          */
            (void)FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr);
            FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);         /* 'Exit' SPI access.                                   */
            FS_TRACE_DBG(("FSDev_SD_SPI_WrDataMulti(): Failed to wr card: data resp token rx'd: %1X.\r\n", resp & FS_DEV_SD_SPI_TOKEN_RESP_MASK));
            return (DEF_FAIL);
        }

                                                                /* Wait while busy token rx'd.                          */
        resp = FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr);

        if (resp != FS_DEV_SD_SPI_RESP_EMPTY) {
            FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);         /* 'Exit' SPI access.                                   */
            FS_TRACE_DBG(("FSDev_SD_SPI_WrDataMulti(): Failed to wr card: card busy timed out.\r\n"));
            return (DEF_FAIL);
        }

        p_src += size;
        cnt--;
    }

    token = FS_DEV_SD_SPI_TOKEN_STOP_TRAN;
    FSDev_SD_SPI_BSP_SPI.Wr( p_sd_spi_data->UnitNbr,            /* Wr token.                                            */
                            &token,
                             1u);

    resp  = FSDev_SD_SPI_WaitWhileBusy(p_sd_spi_data->UnitNbr); /* Wait while busy token rx'd.                          */
    FS_DEV_SD_SPI_EXIT(p_sd_spi_data->UnitNbr);                 /* 'Exit' SPI access.                                   */

    if (resp != FS_DEV_SD_SPI_RESP_EMPTY) {
        FS_TRACE_DBG(("FSDev_SD_SPI_WrDataMulti(): Failed to wr card: card busy timed out.\r\n"));
        return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_SendCID()
*
* Description : Get Card ID (CID) register from card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
*
*               p_dest          Pointer to 16-byte buffer that will receive SD/MMC Card ID register.
*
* Return(s)   : DEF_OK   if the data was read.
*               DEF_FAIL otherwise.
*
* Note(s)     : (1) (a) For SD cards, the structure of the CID is defined in [Ref 1] Section 5.1.
*
*                   (b) For MMC cards, the structure of the CID is defined in [Ref 2] Section 8.2.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_SPI_SendCID (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                           CPU_INT08U          *p_dest)
{
    CPU_BOOLEAN  ok;


    ok = FSDev_SD_SPI_RdData(p_sd_spi_data,
                             FS_DEV_SD_CMD_SEND_CID,
                             0u,
                             p_dest,
                             FS_DEV_SD_CID_REG_LEN);

    return (ok);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_SendCSD()
*
* Description : Get Card-Specific Data (CSD) register from card.
*
* Argument(s) : p_sd_spi_data   Pointer to SD SPI data.
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
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_SD_SPI_SendCSD (FS_DEV_SD_SPI_DATA  *p_sd_spi_data,
                                           CPU_INT08U          *p_dest)
{
    CPU_BOOLEAN  ok;


    ok = FSDev_SD_SPI_RdData(p_sd_spi_data,
                             FS_DEV_SD_CMD_SEND_CSD,
                             0u,
                             p_dest,
                             FS_DEV_SD_CSD_REG_LEN);

    return (ok);
}


/*
*********************************************************************************************************
*                                        FSDev_SD_SPI_RdByte()
*
* Description : Read one byte from card.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
* Return(s)   : Byte read.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_RdByte (FS_QTY  unit_nbr)
{
    CPU_INT08U  resp;


    FSDev_SD_SPI_BSP_SPI.Rd( unit_nbr,                          /* Rd resp byte.                                        */
                            &resp,
                             1u);

    return (resp);
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_WaitClks()
*
* Description : Send empty data for a certain number of clocks.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
*               nbr_clks    Number of clock cycles.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_SD_SPI_WaitClks (FS_QTY      unit_nbr,
                                     CPU_INT08U  nbr_clks)
{
    CPU_INT08U  clks;
    CPU_INT08U  data;


    clks = 0u;
    while (clks < nbr_clks) {
        data = FS_DEV_SD_SPI_RESP_EMPTY;

        FSDev_SD_SPI_BSP_SPI.Wr( unit_nbr,
                                &data,
                                 1u);

        clks++;
    }
}


/*
*********************************************************************************************************
*                                     FSDev_SD_SPI_WaitForStart()
*
* Description : Wait for card to return start token.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
* Return(s)   : Final token.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_WaitForStart (FS_QTY  unit_nbr)
{
    CPU_INT32U  clks;
    CPU_INT08U  datum;


    clks  = 0u;
    datum = FS_DEV_SD_SPI_RESP_EMPTY;
    while (clks < 312500u) {
        datum = FSDev_SD_SPI_RdByte(unit_nbr);

        if (datum != FS_DEV_SD_SPI_RESP_EMPTY) {
            return (datum);
        }

        datum = FS_DEV_SD_SPI_RESP_EMPTY;

        clks++;
    }

    return (datum);
}


/*
*********************************************************************************************************
*                                    FSDev_SD_SPI_WaitWhileBusy()
*
* Description : Wait while card returns busy token.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
* Return(s)   : Final token.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSDev_SD_SPI_WaitWhileBusy (FS_QTY  unit_nbr)
{
    CPU_INT32U  clks;
    CPU_INT08U  datum;


    clks  = 0u;
    datum = 0x00u;
    while (clks < 781250u) {
        datum = FSDev_SD_SPI_RdByte(unit_nbr);

        if (datum == FS_DEV_SD_SPI_RESP_EMPTY) {
            return (datum);
        }

        datum = 0x00u;

        clks++;
    }

    return (datum);
}


/*
*********************************************************************************************************
*                                      FSDev_SD_SPI_DataFree()
*
* Description : Free a SD SPI data object.
*
* Argument(s) : p_sd_spi_data   Pointer to a SD SPI data object.
*               -------------   Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_SD_SPI_DataFree (FS_DEV_SD_SPI_DATA  *p_sd_spi_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_sd_spi_data->NextPtr   = FSDev_SD_SPI_ListFreePtr;        /* Add to free pool.                                    */
    FSDev_SD_SPI_ListFreePtr = p_sd_spi_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       FSDev_SD_SPI_DataGet()
*
* Description : Allocate & initialize a SD SPI data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a SD SPI data object, if NO errors.
*               Pointer to NULL,                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_SD_SPI_DATA  *FSDev_SD_SPI_DataGet (void)
{
    LIB_ERR              alloc_err;
    CPU_SIZE_T           octets_reqd;
    FS_DEV_SD_SPI_DATA  *p_sd_spi_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FSDev_SD_SPI_ListFreePtr == (FS_DEV_SD_SPI_DATA *)0) {
        p_sd_spi_data = (FS_DEV_SD_SPI_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_SD_SPI_DATA),
                                                             sizeof(CPU_DATA),
                                                            &octets_reqd,
                                                            &alloc_err);
        if (p_sd_spi_data == (FS_DEV_SD_SPI_DATA *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_SD_SPI_DataGet(): Could not alloc mem for SD SPI data: %d octets required.\r\n", octets_reqd));
            return ((FS_DEV_SD_SPI_DATA *)0);
        }
        (void)alloc_err;

        FSDev_SD_SPI_UnitCtr++;


    } else {
        p_sd_spi_data            = FSDev_SD_SPI_ListFreePtr;
        FSDev_SD_SPI_ListFreePtr = FSDev_SD_SPI_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    FSDev_SD_ClrInfo(&p_sd_spi_data->Info);                     /* Clr SD info.                                         */

    p_sd_spi_data->Init                    =  DEF_NO;

#if (FS_CFG_CTR_STAT_EN                    == DEF_ENABLED)      /* Clr stat ctrs.                                       */
    p_sd_spi_data->StatRdCtr               =  0u;
    p_sd_spi_data->StatWrCtr               =  0u;
#endif

#if (FS_CFG_CTR_ERR_EN                     == DEF_ENABLED)      /* Clr err ctrs.                                        */
    p_sd_spi_data->ErrRdCtr                =  0u;
    p_sd_spi_data->ErrWrCtr                =  0u;

    p_sd_spi_data->ErrCmdRespCtr           =  0u;
    p_sd_spi_data->ErrCmdRespTimeoutCtr    =  0u;
    p_sd_spi_data->ErrCmdRespEraseResetCtr =  0u;
    p_sd_spi_data->ErrCmdRespIllegalCmdCtr =  0u;
    p_sd_spi_data->ErrCmdRespCommCRC_Ctr   =  0u;
    p_sd_spi_data->ErrCmdRespEraseSeqCtr   =  0u;
    p_sd_spi_data->ErrCmdRespAddrCtr       =  0u;
    p_sd_spi_data->ErrCmdRespParamCtr      =  0u;
#endif

    p_sd_spi_data->NextPtr                 = (FS_DEV_SD_SPI_DATA *)0;

    return (p_sd_spi_data);
}
