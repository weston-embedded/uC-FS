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
*                                             IDE DEVICES
*
* Filename     : fs_dev_ide.c
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) CompactFlash Association.  "CF+ and CompactFlash Specification Revision 4.1".
*                    002/16/07.
*
*                (2) ANSI.  "AT Attachment with Packet Interface - 6 -- (ATA/ATAPI-6)."
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_IDE_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "../../Source/fs.h"
#include  "../../Source/fs_buf.h"
#include  "fs_dev_ide.h"


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

#define  FS_DEV_IDE_SEC_SIZE                             512u

#define  FS_DEV_IDE_BUSY_TO_mS                         30000u


/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*
* Note(s) : (1) See [Ref 1], Section 6.2 'CF-ATA Command Description'.
*********************************************************************************************************
*/

#define  FS_DEV_IDE_CMD_IDENT_DEV                       0xECu   /* Identify Device.                                     */
#define  FS_DEV_IDE_CMD_RD_DMA                          0xC8u   /* Read DMA.                                            */
#define  FS_DEV_IDE_CMD_RD_SEC                          0x20u   /* Read Sector(s).                                      */
#define  FS_DEV_IDE_CMD_SET_FEATURES                    0xEFu   /* Set Features.                                        */
#define  FS_DEV_IDE_CMD_WR_DMA                          0xCAu   /* Write DMA.                                           */
#define  FS_DEV_IDE_CMD_WR_SEC                          0x30u   /* Write Sector(s).                                     */

#define  FS_DEV_IDE_CMD_CHK_PWR_MODE                    0xE5u   /* Check Power Mode.                                    */
#define  FS_DEV_IDE_CMD_EXEC_DRV_DIAG                   0x90u   /* Execute Drive Diagnostic.                            */
#define  FS_DEV_IDE_CMD_FLUSH_CACHE                     0xE7u   /* Flush Cache.                                         */
#define  FS_DEV_IDE_CMD_IDLE                            0xE3u   /* Idle.                                                */
#define  FS_DEV_IDE_CMD_IDLE_IMMEDIATE                  0xE1u   /* Idle Immediate.                                      */
#define  FS_DEV_IDE_CMD_INIT_DRV_PARAMS                 0x91u   /* Initialize Drive Parameters.                         */
#define  FS_DEV_IDE_CMD_NOP                             0x00u   /* NOP.                                                 */
#define  FS_DEV_IDE_CMD_RD_BUF                          0xE4u   /* Read Buffer.                                         */
#define  FS_DEV_IDE_CMD_RD_LONG_SEC                     0x22u   /* Read Long Sector.                                    */
#define  FS_DEV_IDE_CMD_RD_MULT                         0xC4u   /* Read Multiple.                                       */
#define  FS_DEV_IDE_CMD_RD_VERIFY_SEC                   0x40u   /* Read Verify Sector(s).                               */
#define  FS_DEV_IDE_CMD_RECALIBRATE                     0x10u   /* Recalibrate.                                         */
#define  FS_DEV_IDE_CMD_REQ_SENSE                       0x03u   /* Request Sense.                                       */
#define  FS_DEV_IDE_CMD_SEEK                            0x70u   /* Seek.                                                */
#define  FS_DEV_IDE_CMD_SET_MULT_MODE                   0xC6u   /* Set Multiple Mode.                                   */
#define  FS_DEV_IDE_CMD_SET_SLEEP_MODE                  0xE6u   /* Set Sleep Mode.                                      */
#define  FS_DEV_IDE_CMD_STANDBY                         0xE2u   /* Standby.                                             */
#define  FS_DEV_IDE_CMD_STANDBY_IMMEDIATE               0xE0u   /* Standby Immediate.                                   */
#define  FS_DEV_IDE_CMD_WR_BUF                          0xE8u   /* Write Buffer.                                        */
#define  FS_DEV_IDE_CMD_WR_LONG_SEC                     0x32u   /* Write Long Sector.                                   */
#define  FS_DEV_IDE_CMD_WR_MULT                         0xC5u   /* Write Multiple.                                      */
#define  FS_DEV_IDE_CMD_WR_VERIFY                       0x3Cu   /* Write Verify.                                        */


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
*
* Note(s) : (1) See [Ref 1], Section 6.1.5 'CF-ATA Registers'.
*********************************************************************************************************
*/

                                                                /* ---------------- ERROR REGISTER BITS --------------- */
#define  FS_DEV_IDE_REG_ERR_BBK_ICRC              DEF_BIT_07    /* Bad block detected OR interface CRC error detected.  */
#define  FS_DEV_IDE_REG_ERR_UNC                   DEF_BIT_06    /* Uncorrectable error encountered.                     */
#define  FS_DEV_IDE_REG_ERR_MC                    DEF_BIT_05    /* Media changed.                                       */
#define  FS_DEV_IDE_REG_ERR_IDNF                  DEF_BIT_04    /* Requested sector ID is in error or cannot be found.  */
#define  FS_DEV_IDE_REG_ERR_MCR                   DEF_BIT_03    /* Media change request.                                */
#define  FS_DEV_IDE_REG_ERR_ABORT                 DEF_BIT_02    /* Command aborted because of CF status condition.      */
#define  FS_DEV_IDE_REG_ERR_NM                    DEF_BIT_01    /* No medium.                                           */
#define  FS_DEV_IDE_REG_ERR_AMNF                  DEF_BIT_00    /* General error.                                       */

                                                                /* ------------- DEVICE/HEAD REGISTER BITS ------------ */
#define  FS_DEV_IDE_REG_DH_one_1                  DEF_BIT_07    /* Specified as 1 for backwards-compatibility.          */
#define  FS_DEV_IDE_REG_DH_LBA                    DEF_BIT_06    /* Flag to select either CHS or LBA mode.               */
#define  FS_DEV_IDE_REG_DH_one_2                  DEF_BIT_05    /* Specified as 1 for backwards-compatibility.          */
#define  FS_DEV_IDE_REG_DH_DRV                    DEF_BIT_04    /* Drive number.                                        */
#define  FS_DEV_IDE_REG_DH_HS3                    DEF_BIT_03    /* Bit 3 of head number.                                */
#define  FS_DEV_IDE_REG_DH_HS2                    DEF_BIT_02    /* Bit 2 of head number.                                */
#define  FS_DEV_IDE_REG_DH_HS1                    DEF_BIT_01    /* Bit 1 of head number.                                */
#define  FS_DEV_IDE_REG_DH_HS0                    DEF_BIT_00    /* Bit 0 of head number.                                */

                                                                /* --------------- STATUS REGISTER BITS --------------- */
#define  FS_DEV_IDE_REG_STATUS_BUSY               DEF_BIT_07    /* Host locked from accessing command register & buffer.*/
#define  FS_DEV_IDE_REG_STATUS_RDY                DEF_BIT_06    /* Device can perform card operations.                  */
#define  FS_DEV_IDE_REG_STATUS_DWF                DEF_BIT_05    /* Write fault occurred.                                */
#define  FS_DEV_IDE_REG_STATUS_DSC                DEF_BIT_04    /* Card is ready.                                       */
#define  FS_DEV_IDE_REG_STATUS_DRQ                DEF_BIT_03    /* Card requires information be transferred.            */
#define  FS_DEV_IDE_REG_STATUS_CORR               DEF_BIT_02    /* Correctable data error encountered.                  */
#define  FS_DEV_IDE_REG_STATUS_zero               DEF_BIT_01    /* Always set to 0.                                     */
#define  FS_DEV_IDE_REG_STATUS_ERR                DEF_BIT_00    /* Previous command ended in error.                     */

                                                                /* ----------- DEVICE CONTROL REGISTER BITS ----------- */
#define  FS_DEV_IDE_REG_DEVCTRL_SWRST             DEF_BIT_02    /* Forces card to perform soft reset.                   */
#define  FS_DEV_IDE_REG_DEVCTRL_nIEN              DEF_BIT_01    /* Enables interrupts.                                  */


/*
*********************************************************************************************************
*                                           FEATURE DEFINES
*
* Note(s) : (1) See [Ref 1], Table 57 'Feature Supported'.
*********************************************************************************************************
*/

#define  FS_DEV_IDE_FEATURE_EN_8BIT_TRANSFER            0x01u   /* Enable 8 bit data transfers.                         */
#define  FS_DEV_IDE_FEATURE_EN_WR_CACHE                 0x02u   /* Enable Write Cache.                                  */
#define  FS_DEV_IDE_FEATURE_SET_TRANSFER_MODE           0x03u   /* Set transfer mode based on value in Sec Cnt reg.     */
#define  FS_DEV_IDE_FEATURE_EN_ADV_PWR_MANAGE           0x05u   /* Enable Advanced Power Management.                    */
#define  FS_DEV_IDE_FEATURE_EN_EXT_PWR_OPS              0x09u   /* Enable Extended Power operations.                    */
#define  FS_DEV_IDE_FEATURE_EN_PWR_LEVEL_1_CMDS         0x0Au   /* Enable Power Level 1 commands.                       */
#define  FS_DEV_IDE_FEATURE_PROD_SPECIFIC_ECC           0x44u   /* Product specific ECC bytes apply on Long commands.   */
#define  FS_DEV_IDE_FEATURE_DIS_RD_LOOK_AHEAD           0x55u   /* Disable Read Look Ahead.                             */
#define  FS_DEV_IDE_FEATURE_DIS_PWR_ON_RESET            0x66u   /* Disable Power on Reset.                              */
#define  FS_DEV_IDE_FEATURE_NOP_1                       0x69u   /* NOP.                                                 */
#define  FS_DEV_IDE_FEATURE_DIS_8BIT_TRANSFER           0x81u   /* Disable 8 bit data transfer.                         */
#define  FS_DEV_IDE_FEATURE_DIS_WR_CACHE                0x82u   /* Disable Write Cache.                                 */
#define  FS_DEV_IDE_FEATURE_DIS_ADV_PWR_MANAGE          0x85u   /* Disable Advanced Power Management.                   */
#define  FS_DEV_IDE_FEATURE_DIS_EXT_PWR_OPS             0x89u   /* Disable Extended Power operations.                   */
#define  FS_DEV_IDE_FEATURE_DIS_PWR_LEVEL_1_CMDS        0x8Au   /* Disable Power Level 1 commands.                      */
#define  FS_DEV_IDE_FEATURE_NOP_2                       0x96u   /* NOP.                                                 */
#define  FS_DEV_IDE_FEATURE_SET_HOST_CURR_SRC_CAP       0x9Au   /* Set host current source capability.                  */
#define  FS_DEV_IDE_FEATURE_EN_RD_LOOK_AHEAD            0xAAu   /* Enable Read Look Ahead.                              */
#define  FS_DEV_IDE_FEATURE_4BYTES_DATA_LONG_CMDS       0xBBu   /* 4 bytes of data apply of Read/Write Long commands.   */
#define  FS_DEV_IDE_FEATURE_EN_PWR_ON_RESET             0xCCu   /* Enable Power on Reset.                               */


/*
*********************************************************************************************************
*                                    DEVICE IDENTIFY INFO DEFINES
*
* Note(s) : (1) See [Ref 1], Section 6.2.1.6.
*
*               (a) Word 49: Capabilities
*                   (1) Bit 11: IORDY Supported
*                   (2) Bit 10: IORDY may be disabled
*                   (3) BIT  8: DMA Supported
*
*               (b) Word 51: PIO Data Transfer Cycle Timing Mode
*                   (1) Bits 15-8 shall be 0x00 for mode 0
*                   (2)  "     "    "    " 0x01  "   "   1
*                   (3)  "     "    "    " 0x02  "   "   2
*
*               (c) Word 53: Translation Parameters Valid
*                   (1) Bit 1: Words 64-70 valid
*                   (2) Bit 2: Word 88 valid; True IDE UDMA transfer modes supported
*
*               (d) Word 63: Multiword DMA transfer
*                   (1) Bit 0: Multiword DMA mode  0   supported
*                   (1) Bit 1: Multiword DMA modes 0-1 supported
*                   (1) Bit 2: Multiword DMA modes 0-2 supported
*
*               (e) Word 64: Advanced PIO transfer modes supported
*                   (1) Bit 0: PIO mode 3 supported
*                   (2) Bit 1: PIO mode 4 supported
*
*               (f) Word 88: True IDE Ultra DMA Modes Supported and Selected
*                   (1) Bit 0: Ultra DMA mode  0   supported
*                   (1) Bit 1: Ultra DMA modes 0-1 supported
*                   (1) Bit 2: Ultra DMA modes 0-2 supported
*                   (1) Bit 3: Ultra DMA modes 0-3 supported
*                   (1) Bit 4: Ultra DMA modes 0-4 supported
*                   (1) Bit 5: Ultra DMA modes 0-5 supported
*********************************************************************************************************
*/

#define  FS_DEV_IDE_IDENTIFY_OFF_SERIAL_NBR               20u
#define  FS_DEV_IDE_IDENTIFY_OFF_FIRMWARE_REV             46u
#define  FS_DEV_IDE_IDENTIFY_OFF_MODEL                    54u
#define  FS_DEV_IDE_IDENTIFY_OFF_MULT_CNT_MAX             94u
#define  FS_DEV_IDE_IDENTIFY_OFF_CAPABILITIES             98u   /* See Note #1a.                                        */
#define  FS_DEV_IDE_IDENTIFY_OFF_PIO_MODE                102u   /* See Note #1b.                                        */
#define  FS_DEV_IDE_IDENTIFY_OFF_TRANS_PARAMS_VALID      106u   /* See Note #1c.                                        */
#define  FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA                 120u
#define  FS_DEV_IDE_IDENTIFY_OFF_MULTIWORD_DMA_TRANSFER  126u   /* See Note #1d.                                        */
#define  FS_DEV_IDE_IDENTIFY_OFF_ADV_PIO_MODE_SUPPORT    128u   /* See Note #1e.                                        */
#define  FS_DEV_IDE_IDENTIFY_OFF_ULTRA_DMA_MODE_SUPPORT  176u   /* See Note #1f.                                        */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      IDE DEVICE DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_ide_data  FS_DEV_IDE_DATA;
struct  fs_dev_ide_data {
    FS_QTY            UnitNbr;
    CPU_BOOLEAN       Init;

    CPU_INT32U        Size;
    CPU_INT08U        MultSecCnt;
    CPU_INT08U        DrvNbr;
    FS_FLAGS          ModeSel;
    CPU_INT08S        PIO_ModeMax;
    CPU_INT08S        DMA_ModeMax;
    CPU_INT08S        UDMA_ModeMax;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR            StatRdCtr;
    FS_CTR            StatWrCtr;
#endif

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
    FS_CTR            ErrRdCtr;
    FS_CTR            ErrWrCtr;

    FS_CTR            ErrBadBlkCRC_Ctr;
    FS_CTR            ErrUnrecoverCtr;
    FS_CTR            ErrMediaChngdCtr;
    FS_CTR            ErrID_NotFoundCtr;
    FS_CTR            ErrMediaChngReqdCtr;
    FS_CTR            ErrAbortCtr;
    FS_CTR            ErrNoMediaCtr;
    FS_CTR            ErrOtherCtr;
#endif

    FS_DEV_IDE_DATA  *NextPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  const  CPU_CHAR  FSDev_IDE_Name[] = "ide";


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

static  FS_DEV_IDE_DATA  *FSDev_IDE_ListFreePtr;


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

#define  FS_DEV_IDE_STAT_RD_CTR_ADD(p_ide_data, val)         {  FS_CTR_STAT_ADD((p_ide_data)->StatRdCtr, (FS_CTR)(val)); }
#define  FS_DEV_IDE_STAT_WR_CTR_ADD(p_ide_data, val)         {  FS_CTR_STAT_ADD((p_ide_data)->StatWrCtr, (FS_CTR)(val)); }

#else

#define  FS_DEV_IDE_STAT_RD_CTR_ADD(p_ide_data, val)
#define  FS_DEV_IDE_STAT_WR_CTR_ADD(p_ide_data, val)

#endif


#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* ------------------ ERR CTR MACRO'S ----------------- */

#define  FS_DEV_IDE_ERR_RD_CTR_INC(p_ide_data)               {  FS_CTR_ERR_INC((p_ide_data)->ErrRdCtr);                  }
#define  FS_DEV_IDE_ERR_WR_CTR_INC(p_ide_data)               {  FS_CTR_ERR_INC((p_ide_data)->ErrWrCtr);                  }

#define  FS_DEV_IDE_ERR_BAD_BLK_CRC_CTR_INC(p_ide_data)      {  FS_CTR_ERR_INC((p_ide_data)->ErrBadBlkCRC_Ctr);          }
#define  FS_DEV_IDE_ERR_UNRECOVER_CTR_INC(p_ide_data)        {  FS_CTR_ERR_INC((p_ide_data)->ErrUnrecoverCtr);           }
#define  FS_DEV_IDE_ERR_MEDIA_CNHGD_CTR_INC(p_ide_data)      {  FS_CTR_ERR_INC((p_ide_data)->ErrMediaChngdCtr);          }
#define  FS_DEV_IDE_ERR_ID_NOT_FOUND_CTR_INC(p_ide_data)     {  FS_CTR_ERR_INC((p_ide_data)->ErrID_NotFoundCtr);         }
#define  FS_DEV_IDE_ERR_MEDIA_CHNG_REQD_CTR_INC(p_ide_data)  {  FS_CTR_ERR_INC((p_ide_data)->ErrMediaChngReqdCtr);       }
#define  FS_DEV_IDE_ERR_ABORT_CTR_INC(p_ide_data)            {  FS_CTR_ERR_INC((p_ide_data)->ErrAbortCtr);               }
#define  FS_DEV_IDE_ERR_NO_MEDIA_CTR_INC(p_ide_data)         {  FS_CTR_ERR_INC((p_ide_data)->ErrNoMediaCtr);             }
#define  FS_DEV_IDE_ERR_OTHER_CTR_INC(p_ide_data)            {  FS_CTR_ERR_INC((p_ide_data)->ErrOtherCtr);               }

#define  FS_DEV_IDE_HANDLE_ERR_BSP(p_ide_data, reg_err)         FSDev_IDE_HandleErrBSP((p_ide_data), (reg_err))

#else

#define  FS_DEV_IDE_ERR_RD_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_WR_CTR_INC(p_ide_data)

#define  FS_DEV_IDE_ERR_BAD_BLK_CRC_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_UNRECOVER_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_MEDIA_CNHGD_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_ID_NOT_FOUND_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_MEDIA_CHNG_REQD_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_ABORT_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_NO_MEDIA_CTR_INC(p_ide_data)
#define  FS_DEV_IDE_ERR_OTHER_CTR_INC(p_ide_data)

#define  FS_DEV_IDE_HANDLE_ERR_BSP(p_ide_data, reg_err)

#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                    /* ------ DEV INTERFACE FNCTS ----- */
static  const  CPU_CHAR  *FSDev_IDE_NameGet      (void);                            /* Get name of driver.              */

static  void              FSDev_IDE_Init         (FS_ERR           *p_err);         /* Init driver.                     */

static  void              FSDev_IDE_Open         (FS_DEV           *p_dev,          /* Open device.                     */
                                                  void             *p_dev_cfg,
                                                  FS_ERR           *p_err);

static  void              FSDev_IDE_Close        (FS_DEV           *p_dev);         /* Close device.                    */

static  void              FSDev_IDE_Rd           (FS_DEV           *p_dev,          /* Read from device.                */
                                                  void             *p_dest,
                                                  FS_SEC_NBR        start,
                                                  FS_SEC_QTY        cnt,
                                                  FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_IDE_Wr           (FS_DEV           *p_dev,          /* Write to device.                 */
                                                  void             *p_src,
                                                  FS_SEC_NBR        start,
                                                  FS_SEC_QTY        cnt,
                                                  FS_ERR           *p_err);
#endif

static  void              FSDev_IDE_Query        (FS_DEV           *p_dev,          /* Get device info.                 */
                                                  FS_DEV_INFO      *p_info,
                                                  FS_ERR           *p_err);

static  void              FSDev_IDE_IO_Ctrl      (FS_DEV           *p_dev,          /* Perform device I/O control.      */
                                                  CPU_INT08U        opt,
                                                  void             *p_data,
                                                  FS_ERR           *p_err);


                                                                                    /* ---------- LOCAL FNCTS --------- */
static  CPU_BOOLEAN       FSDev_IDE_Refresh      (FS_DEV_IDE_DATA  *p_ide_data,     /* Refresh dev.                     */
                                                  FS_ERR           *p_err);

static  void              FSDev_IDE_RdData       (FS_DEV_IDE_DATA  *p_ide_data,     /* Perform data-in cmd.             */
                                                  CPU_INT08U        cmd[],
                                                  CPU_INT08U       *p_dest,
                                                  CPU_SIZE_T        cnt,
                                                  CPU_SIZE_T        reps,
                                                  FS_FLAGS          mode_type,
                                                  FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_IDE_WrData       (FS_DEV_IDE_DATA  *p_ide_data,     /* Perform data-out cmd.            */
                                                  CPU_INT08U        cmd[],
                                                  CPU_INT08U       *p_src,
                                                  CPU_SIZE_T        cnt,
                                                  CPU_SIZE_T        reps,
                                                  FS_FLAGS          mode_type,
                                                  FS_ERR           *p_err);
#endif

static  void              FSDev_IDE_SetFeature   (FS_DEV_IDE_DATA  *p_ide_data,     /* Set feature.                     */
                                                  CPU_INT08U        feature,
                                                  CPU_INT08U        value);

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
static  void              FSDev_IDE_HandleErrBSP (FS_DEV_IDE_DATA  *p_ide_data,     /* Handle BSP err.                  */
                                                  CPU_INT08U        reg_err);
#endif

static  CPU_BOOLEAN       FSDev_IDE_SelDev       (FS_QTY            unit_nbr,       /* Sel dev.                         */
                                                  CPU_INT08U        drv_nbr);

static  CPU_BOOLEAN       FSDev_IDE_WaitForData  (FS_QTY            unit_nbr);      /* Wait for data req.               */

static  CPU_BOOLEAN       FSDev_IDE_WaitWhileBusy(FS_QTY            unit_nbr);      /* Wait while dev busy.             */


static  void              FSDev_IDE_DataFree     (FS_DEV_IDE_DATA  *p_ide_data);    /* Free IDE data.                   */

static  FS_DEV_IDE_DATA  *FSDev_IDE_DataGet      (void);                            /* Allocate & initialize IDE data.  */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_API  FSDev_IDE = {
    FSDev_IDE_NameGet,
    FSDev_IDE_Init,
    FSDev_IDE_Open,
    FSDev_IDE_Close,
    FSDev_IDE_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_IDE_Wr,
#endif
    FSDev_IDE_Query,
    FSDev_IDE_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSDev_IDE_NameGet()
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

static  const  CPU_CHAR  *FSDev_IDE_NameGet (void)
{
    return (FSDev_IDE_Name);
}


/*
*********************************************************************************************************
*                                          FSDev_IDE_Init()
*
* Description : Initialize the driver.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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

static  void  FSDev_IDE_Init (FS_ERR  *p_err)
{
    FSDev_IDE_UnitCtr     =  0u;
    FSDev_IDE_ListFreePtr = (FS_DEV_IDE_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_IDE_Open()
*
* Description : Open (initialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to open.
*               ----------  Argument validated by caller.
*
*               p_dev_cfg   Pointer to device configuration.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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

static  void  FSDev_IDE_Open (FS_DEV  *p_dev,
                              void    *p_dev_cfg,
                              FS_ERR  *p_err)
{
    FS_DEV_IDE_DATA  *p_ide_data;


    (void)p_dev_cfg;                                            /*lint --e{550} Suppress "Symbol not accessed".         */

    p_ide_data = FSDev_IDE_DataGet();
    if (p_ide_data == (FS_DEV_IDE_DATA *)0) {
      *p_err = FS_ERR_MEM_ALLOC;
       return;
    }

    p_dev->DataPtr      = (void *)p_ide_data;
    p_ide_data->UnitNbr =  p_dev->UnitNbr;

    (void)FSDev_IDE_Refresh(p_ide_data, p_err);
}


/*
*********************************************************************************************************
*                                          FSDev_IDE_Close()
*
* Description : Close (uninitialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to close.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) This function will be called EVERY time the device is closed.
*********************************************************************************************************
*/

static  void  FSDev_IDE_Close (FS_DEV  *p_dev)
{
    FS_DEV_IDE_DATA  *p_ide_data;


    p_ide_data = (FS_DEV_IDE_DATA *)p_dev->DataPtr;
    if (p_ide_data->Init == DEF_YES) {
        FSDev_IDE_BSP_Close(p_ide_data->UnitNbr);
    }
    FSDev_IDE_DataFree(p_ide_data);
}


/*
*********************************************************************************************************
*                                           FSDev_IDE_Rd()
*
* Description : Read from a device & store data in buffer.
*
* Argument(s) : p_dev       Pointer to device to read from.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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
*********************************************************************************************************
*/

static  void  FSDev_IDE_Rd (FS_DEV      *p_dev,
                            void        *p_dest,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    CPU_INT08U        cmd[7];
    CPU_SIZE_T        cnt_rd;
    CPU_INT08U       *p_dest_08;
    FS_DEV_IDE_DATA  *p_ide_data;
    FS_SEC_NBR        sec;
    CPU_BOOLEAN       sel;


    FSDev_IDE_BSP_Lock(p_dev->UnitNbr);

    p_ide_data = (FS_DEV_IDE_DATA *)p_dev->DataPtr;             /* Sel dev.                                             */
    sel        = FSDev_IDE_SelDev(p_ide_data->UnitNbr, p_ide_data->DrvNbr);
    if (sel == DEF_NO) {
        FS_OS_Dly_ms(20u);
        sel  = FSDev_IDE_SelDev(p_ide_data->UnitNbr, p_ide_data->DrvNbr);
        if (sel == DEF_NO) {
            FS_TRACE_DBG(("FSDev_IDE_Rd(): Dev could not be sel'd.\r\n"));
           *p_err = FS_ERR_DEV_TIMEOUT;
            return;
        }
    }

    p_dest_08 = (CPU_INT08U *)p_dest;
    sec       =  start;
    while (cnt > 0u) {
        cnt_rd = (CPU_SIZE_T)DEF_MIN(cnt, DEF_INT_08U_MAX_VAL);

        cmd[0] =   0u;                                          /* Fmt cmd.                                             */
        cmd[1] =  (CPU_INT08U)  cnt_rd;
        cmd[2] =  (CPU_INT08U) (sec >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[3] =  (CPU_INT08U) (sec >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[4] =  (CPU_INT08U) (sec >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[5] =  (CPU_INT08U)((sec >> (3u * DEF_OCTET_NBR_BITS)) & DEF_NIBBLE_MASK)
               |   FS_DEV_IDE_REG_DH_one_1
               |   FS_DEV_IDE_REG_DH_LBA
               |   FS_DEV_IDE_REG_DH_one_2
               | ((p_ide_data->DrvNbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV);
        cmd[6] =  (p_ide_data->ModeSel == FS_DEV_IDE_MODE_TYPE_PIO) ? FS_DEV_IDE_CMD_RD_SEC : FS_DEV_IDE_CMD_RD_DMA;


        FSDev_IDE_RdData(p_ide_data,                            /* Perform rd sec cmd.                                  */
                         cmd,
                         p_dest_08,
                         FS_DEV_IDE_SEC_SIZE,
                         cnt_rd,
                         p_ide_data->ModeSel,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FSDev_IDE_Rd(): Could not rd sec.\r\n"));
            return;
        }

        FS_DEV_IDE_STAT_RD_CTR_ADD(p_ide_data, cnt);

        p_dest_08 +=             cnt_rd * FS_DEV_IDE_SEC_SIZE;
        sec       += (FS_SEC_NBR)cnt_rd;
        cnt       -= (FS_SEC_QTY)cnt_rd;
    }

    FSDev_IDE_BSP_Unlock(p_dev->UnitNbr);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSDev_IDE_Wr()
*
* Description : Write data to a device from a buffer.
*
* Argument(s) : p_dev       Pointer to device to write to.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of write.
*
*               cnt         Number of sectors to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_IDE_Wr (FS_DEV      *p_dev,
                            void        *p_src,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    CPU_INT08U        cmd[7];
    CPU_SIZE_T        cnt_wr;
    FS_DEV_IDE_DATA  *p_ide_data;
    FS_SEC_NBR        sec;
    CPU_BOOLEAN       sel;
    CPU_INT08U       *p_src_08;


    FSDev_IDE_BSP_Lock(p_dev->UnitNbr);

    p_ide_data = (FS_DEV_IDE_DATA *)p_dev->DataPtr;             /* Sel dev.                                             */
    sel        = FSDev_IDE_SelDev(p_ide_data->UnitNbr, p_ide_data->DrvNbr);
    if (sel == DEF_NO) {
        FS_OS_Dly_ms(20u);
        sel  = FSDev_IDE_SelDev(p_ide_data->UnitNbr, p_ide_data->DrvNbr);
        if (sel == DEF_NO) {
            FS_TRACE_DBG(("FSDev_IDE_Wr(): Dev could not be sel'd.\r\n"));
           *p_err = FS_ERR_DEV_TIMEOUT;
            return;
        }
    }

    p_src_08 = (CPU_INT08U *)p_src;
    sec      =  start;
    while (cnt > 0u) {
        cnt_wr = (CPU_SIZE_T)DEF_MIN(cnt, DEF_INT_08U_MAX_VAL);

        cmd[0] =   0u;                                          /* Fmt cmd.                                             */
        cmd[1] =  (CPU_INT08U)  cnt_wr;
        cmd[2] =  (CPU_INT08U) (sec >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[3] =  (CPU_INT08U) (sec >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[4] =  (CPU_INT08U) (sec >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK;
        cmd[5] =  (CPU_INT08U)((sec >> (3u * DEF_OCTET_NBR_BITS)) & DEF_NIBBLE_MASK)
               |   FS_DEV_IDE_REG_DH_one_1
               |   FS_DEV_IDE_REG_DH_LBA
               |   FS_DEV_IDE_REG_DH_one_2
               | ((p_ide_data->DrvNbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV);
        cmd[6] =  (p_ide_data->ModeSel == FS_DEV_IDE_MODE_TYPE_PIO) ? FS_DEV_IDE_CMD_WR_SEC : FS_DEV_IDE_CMD_WR_DMA;



        FSDev_IDE_WrData(p_ide_data,                            /* Perform wr sec cmd.                                  */
                         cmd,
                         p_src_08,
                         FS_DEV_IDE_SEC_SIZE,
                         cnt_wr,
                         p_ide_data->ModeSel,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FSDev_IDE_Wr(): Could not wr sec.\r\n"));
            return;
        }

        FS_DEV_IDE_STAT_WR_CTR_ADD(p_ide_data, cnt);

        p_src_08 +=             cnt_wr * FS_DEV_IDE_SEC_SIZE;
        sec      += (FS_SEC_NBR)cnt_wr;
        cnt      -= (FS_SEC_QTY)cnt_wr;
    }

    FSDev_IDE_BSP_Unlock(p_dev->UnitNbr);

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_IDE_Query()
*
* Description : Get information about a device.
*
* Argument(s) : p_dev       Pointer to device to query.
*               ----------  Argument validated by caller.
*
*               p_info      Pointer to structure that will receive device information.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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

static  void  FSDev_IDE_Query (FS_DEV       *p_dev,
                               FS_DEV_INFO  *p_info,
                               FS_ERR       *p_err)
{
    FS_DEV_IDE_DATA  *p_ide_data;


    p_ide_data      = (FS_DEV_IDE_DATA *)p_dev->DataPtr;

    p_info->SecSize =  FS_DEV_IDE_SEC_SIZE;
    p_info->Size    =  p_ide_data->Size;
    p_info->Fixed   =  DEF_NO;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_IDE_IO_Ctrl()
*
* Description : Perform device I/O control operation.
*
* Argument(s) : p_dev       Pointer to device to control.
*               ----------  Argument validated by caller.
*
*               opt         Control command.
*
*               p_data      Buffer which holds data to be used for operation.
*                              OR
*                           Buffer in which data will be stored as a result of operation.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*               ----------  Argument validated by caller.
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
*                           [*] NOT SUPPORTED
*********************************************************************************************************
*/

static  void  FSDev_IDE_IO_Ctrl (FS_DEV      *p_dev,
                                 CPU_INT08U   opt,
                                 void        *p_data,
                                 FS_ERR      *p_err)
{
    CPU_BOOLEAN       chngd;
    CPU_BOOLEAN      *p_chngd;
    FS_DEV_IDE_DATA  *p_ide_data;


    switch (opt) {
        case FS_DEV_IO_CTRL_REFRESH:                            /* -------------------- RE-OPEN DEV ------------------- */
             p_chngd    = (CPU_BOOLEAN *)p_data;
             p_ide_data = (FS_DEV_IDE_DATA *)p_dev->DataPtr;
             chngd      = FSDev_IDE_Refresh(p_ide_data, p_err);
            *p_chngd    = chngd;
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
*                                         FSDev_IDE_Refresh()
*
* Description : (Re)-open a device.
*
* Argument(s) : p_ide_data  Pointer to IDE data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device opened successfully.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none. &&&& Return value must be documented.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_IDE_Refresh (FS_DEV_IDE_DATA  *p_ide_data,
                                        FS_ERR           *p_err)
{
#ifdef FS_FILE_SYSTEM_BUILD_FULL
    FS_BUF       *p_buf;
#else
    CPU_INT16U    data[512 / 2];
#endif
    CPU_INT08U    cmd[7];
    CPU_INT08U   *p_data_08;
    CPU_BOOLEAN   init_prev;
    CPU_INT08U    mode;
    CPU_INT08U    modes;
    CPU_INT08U    mode_sel;
    FS_FLAGS      mode_types;
    CPU_INT08U    mult_sec_cnt;
    CPU_BOOLEAN   ok;
    CPU_BOOLEAN   sel;
    CPU_INT32U    size;
    CPU_BOOLEAN   supported;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    CPU_CHAR      str[40];
    CPU_SIZE_T    i;
#endif


#ifdef FS_FILE_SYSTEM_BUILD_FULL
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
        FS_TRACE_DBG(("FSDev_IDE_Refresh(): Could not alloc buf.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return (DEF_YES);
    }
    p_data_08 = (CPU_INT08U *)p_buf->DataPtr;
#else
    p_data_08 = (CPU_INT08U *)&data[0];
#endif


                                                                /* ------------------- CHK DEV CHNGD ------------------ */
    if (p_ide_data->Init == DEF_YES) {                          /* If dev already init'd ...                            */
        cmd[0] = 0x00u;
        cmd[1] = 0x00u;
        cmd[2] = 0x00u;
        cmd[3] = 0x00u;
        cmd[4] = 0x00u;
        cmd[5] = (p_ide_data->DrvNbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV;
        cmd[6] = FS_DEV_IDE_CMD_IDENT_DEV;

        FSDev_IDE_RdData(p_ide_data,                            /* ... perform ident dev cmd.                           */
                         cmd,
                         p_data_08,
                         FS_DEV_IDE_SEC_SIZE,
                         1u,
                         FS_DEV_IDE_MODE_TYPE_PIO,
                         p_err);

        if (*p_err == FS_ERR_NONE) {
            size = (CPU_INT32U)((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 0u] <<  0)
                 | (CPU_INT32U)((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 1u] <<  8)
                 | (CPU_INT32U)((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 2u] << 16)
                 | (CPU_INT32U)((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 3u] << 24);

            if (size == p_ide_data->Size) {
               #ifdef FS_FILE_SYSTEM_BUILD_FULL
                FSBuf_Free(p_buf);
               #endif
               *p_err = FS_ERR_NONE;
                return (DEF_NO);
            }
        }

        FSDev_IDE_BSP_Close(p_ide_data->UnitNbr);               /* Uninit HW.                                           */
    }




                                                                /* ---------------------- INIT HW --------------------- */
    init_prev                =  p_ide_data->Init;               /* If init'd prev'ly, dev chngd.                        */
    p_ide_data->Init         =  DEF_NO;                         /* No longer init'd.                                    */
    p_ide_data->Size         =  0u;
    p_ide_data->MultSecCnt   =  0u;
    p_ide_data->ModeSel      =  FS_DEV_IDE_MODE_TYPE_PIO;
    p_ide_data->PIO_ModeMax  =  0;
    p_ide_data->DMA_ModeMax  = -1;
    p_ide_data->UDMA_ModeMax = -1;

    ok = FSDev_IDE_BSP_Open(p_ide_data->UnitNbr);               /* Init HW.                                             */
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return (init_prev);
    }

    FSDev_IDE_BSP_Reset(p_ide_data->UnitNbr);                   /* Reset dev.                                           */
    p_ide_data->DrvNbr = FSDev_IDE_BSP_GetDrvNbr(p_ide_data->UnitNbr);

                                                                /* Soft reset dev.                                      */
    FSDev_IDE_BSP_RegWr(p_ide_data->UnitNbr, FS_DEV_IDE_REG_DEVCTRL, FS_DEV_IDE_REG_DEVCTRL_nIEN | FS_DEV_IDE_REG_DEVCTRL_SWRST);
    FS_OS_Dly_ms(1u);
    FSDev_IDE_BSP_RegWr(p_ide_data->UnitNbr, FS_DEV_IDE_REG_DEVCTRL, FS_DEV_IDE_REG_DEVCTRL_nIEN);
    FS_OS_Dly_ms(3u);

    sel = FSDev_IDE_SelDev(p_ide_data->UnitNbr, p_ide_data->DrvNbr); /* Sel dev.                                             */
    if (sel == DEF_NO) {                                        /* If dev could not be sel'd ...                        */
        FS_TRACE_DBG(("FSDev_IDE_Refresh(): Dev could not be sel'd.\r\n"));
        FSDev_IDE_BSP_Close(p_ide_data->UnitNbr);
       *p_err = FS_ERR_DEV_TIMEOUT;                             /* ... rtn err.                                         */
        return (init_prev);
    }



                                                                /* ------------------ INIT UNIT INFO ------------------ */
    cmd[0] = 0x00u;
    cmd[1] = 0x00u;
    cmd[2] = 0x00u;
    cmd[3] = 0x00u;
    cmd[4] = 0x00u;
    cmd[5] = (p_ide_data->DrvNbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV;
    cmd[6] = FS_DEV_IDE_CMD_IDENT_DEV;

    FSDev_IDE_RdData(p_ide_data,                                /* Perform ident dev cmd.                               */
                     cmd,
                     p_data_08,
                     FS_DEV_IDE_SEC_SIZE,
                     1u,
                     FS_DEV_IDE_MODE_TYPE_PIO,
                     p_err);

    if (*p_err != FS_ERR_NONE) {                                /* If no resp ...                                       */
        FS_TRACE_DBG(("FSDev_IDE_Refresh(): Could not rd ident dev info.\r\n"));
       #ifdef FS_FILE_SYSTEM_BUILD_FULL
        FSBuf_Free(p_buf);
       #endif
        FSDev_IDE_BSP_Close(p_ide_data->UnitNbr);
        return (init_prev);                                     /* ... rtn.                                             */
    }

    if ((p_data_08[0] == 0xFFu) && (p_data_08[1] == 0xFFu)) {   /* If info invalid ...                                  */
        FS_TRACE_DBG(("FSDev_IDE_Refresh(): Ident dev info probably invalid.\r\n"));
       #ifdef FS_FILE_SYSTEM_BUILD_FULL
        FSBuf_Free(p_buf);
       #endif
        FSDev_IDE_BSP_Close(p_ide_data->UnitNbr);
       *p_err = FS_ERR_DEV_IO;                                  /* ... rtn err.                                         */
        return (init_prev);
    }
                                                                /* Rd max LBA.                                          */
    size                   = ((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 0u] << (0u * DEF_OCTET_NBR_BITS))
                           | ((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 1u] << (1u * DEF_OCTET_NBR_BITS))
                           | ((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 2u] << (2u * DEF_OCTET_NBR_BITS))
                           | ((CPU_INT32U)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MAX_LBA + 3u] << (3u * DEF_OCTET_NBR_BITS));

    mult_sec_cnt           =   p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MULT_CNT_MAX];

    p_ide_data->Size       =   size;
    p_ide_data->MultSecCnt =   mult_sec_cnt;
    p_ide_data->Init       =   DEF_YES;



                                                                /* -------------------- GET MODE(S) ------------------- */
                                                                /* Get Multiword DMA modes ...                          */
    supported = DEF_BIT_IS_SET(p_data_08[FS_DEV_IDE_IDENTIFY_OFF_CAPABILITIES + 1u], DEF_BIT_00);
    if (supported == DEF_NO) {                                              /*   ... no DMA modes supported ...         */
        p_ide_data->DMA_ModeMax = -1;
    } else {
        modes = p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MULTIWORD_DMA_TRANSFER];
        if (DEF_BIT_IS_SET(modes, DEF_BIT_02) == DEF_YES) {                 /*   ... DMA 0-2 supported      ...         */
            p_ide_data->DMA_ModeMax = 2;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_01) == DEF_YES) {          /*   ... DMA 0-1 supported      ...         */
            p_ide_data->DMA_ModeMax = 1;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_00) == DEF_YES) {          /*   ... DMA 0   supported      ...         */
            p_ide_data->DMA_ModeMax = 0;
        } else {                                                            /*   ... no DMA modes supported.            */
            p_ide_data->DMA_ModeMax = -1;
        }
    }

                                                                /* Get PIO modes ...                                    */
    supported = DEF_BIT_IS_SET(p_data_08[FS_DEV_IDE_IDENTIFY_OFF_TRANS_PARAMS_VALID], DEF_BIT_01);
    if (supported == DEF_YES) {
        modes = p_data_08[FS_DEV_IDE_IDENTIFY_OFF_ADV_PIO_MODE_SUPPORT];
        if (DEF_BIT_IS_SET(modes, DEF_BIT_01) == DEF_YES) {                 /*   ... PIO 0-4 supported ...              */
            p_ide_data->PIO_ModeMax = 4;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_00) == DEF_YES) {          /*   ... PIO 0-3 supported ...              */
            p_ide_data->PIO_ModeMax = 3;
        } else {
            supported = DEF_NO;
        }
    }
    if (supported == DEF_NO) {
        modes = p_data_08[FS_DEV_IDE_IDENTIFY_OFF_PIO_MODE + 1u];
        if (modes == 2u) {                                                  /*   ... PIO 0-2 supported ...              */
            p_ide_data->PIO_ModeMax = 2;
        } else if (modes == 1u) {                                           /*   ... PIO 0-1 supported ...              */
            p_ide_data->PIO_ModeMax = 1;
        } else {                                                            /*   ... PIO 0   supported.                 */
            p_ide_data->PIO_ModeMax = 0;
        }
    }

                                                                /* Get Ultra DMA modes ...                              */
    supported = DEF_BIT_IS_SET(p_data_08[FS_DEV_IDE_IDENTIFY_OFF_ULTRA_DMA_MODE_SUPPORT], DEF_BIT_02);
    if (supported == DEF_NO) {                                                      /* ... no UDMA modes supported ...  */
        p_ide_data->UDMA_ModeMax = -1;
    } else {
        modes = p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MULTIWORD_DMA_TRANSFER];
        if (DEF_BIT_IS_SET(modes, DEF_BIT_05) == DEF_YES) {                         /* ... UDMA 0-5 supported      ...  */
            p_ide_data->UDMA_ModeMax = 5;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_04) == DEF_YES) {                  /* ... UDMA 0-4 supported      ...  */
            p_ide_data->UDMA_ModeMax = 4;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_03) == DEF_YES) {                  /* ... UDMA 0-3 supported      ...  */
            p_ide_data->UDMA_ModeMax = 3;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_02) == DEF_YES) {                  /* ... UDMA 0-2 supported      ...  */
            p_ide_data->UDMA_ModeMax = 2;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_01) == DEF_YES) {                  /* ... UDMA 0-1 supported      ...  */
            p_ide_data->UDMA_ModeMax = 1;
        } else if (DEF_BIT_IS_SET(modes, DEF_BIT_00) == DEF_YES) {                  /* ... UDMA 0   supported      ...  */
            p_ide_data->UDMA_ModeMax = 0;
        } else {                                                                    /* ... no UDMA modes supported.     */
            p_ide_data->UDMA_ModeMax = -1;
        }
    }



                                                                /* -------------------- SET MODE(S) ------------------- */
    mode     = (CPU_INT08U)p_ide_data->PIO_ModeMax;             /* Set PIO mode timings.                                */
    mode_sel =  FSDev_IDE_BSP_SetMode(p_ide_data->UnitNbr,
                                      FS_DEV_IDE_MODE_TYPE_PIO,
                                      mode);

    FSDev_IDE_SetFeature( p_ide_data,                           /* Sel PIO mode.                                        */
                          FS_DEV_IDE_FEATURE_SET_TRANSFER_MODE,
                         (FS_DEV_IDE_MODE_TYPE_PIO | mode_sel));

    mode_types = FSDev_IDE_BSP_GetModesSupported(p_ide_data->UnitNbr);
    if ((DEF_BIT_IS_SET(mode_types, FS_DEV_IDE_MODE_TYPE_UDMA) == DEF_YES) && (p_ide_data->UDMA_ModeMax >= 0)) {
        mode     = (CPU_INT08U)p_ide_data->UDMA_ModeMax;        /* Set UDMA mode timings.                               */
        mode_sel = FSDev_IDE_BSP_SetMode(p_ide_data->UnitNbr,
                                         FS_DEV_IDE_MODE_TYPE_UDMA,
                                         mode);

        FSDev_IDE_SetFeature( p_ide_data,                       /* Sel UDMA mode.                                       */
                              FS_DEV_IDE_FEATURE_SET_TRANSFER_MODE,
                             (FS_DEV_IDE_MODE_TYPE_UDMA | mode_sel));

        p_ide_data->ModeSel = FS_DEV_IDE_MODE_TYPE_UDMA;

    } else if ((DEF_BIT_IS_SET(mode_types, FS_DEV_IDE_MODE_TYPE_DMA) == DEF_YES) && (p_ide_data->DMA_ModeMax >= 0)) {
        mode     = (CPU_INT08U)p_ide_data->DMA_ModeMax;         /* Set Multiword DMA mode timings.                      */
        mode_sel = FSDev_IDE_BSP_SetMode(p_ide_data->UnitNbr,
                                         FS_DEV_IDE_MODE_TYPE_DMA,
                                         mode);

        FSDev_IDE_SetFeature( p_ide_data,                       /* Sel Multiword DMA mode.                              */
                              FS_DEV_IDE_FEATURE_SET_TRANSFER_MODE,
                             (FS_DEV_IDE_MODE_TYPE_DMA | mode_sel));

        p_ide_data->ModeSel = FS_DEV_IDE_MODE_TYPE_DMA;

    } else {
        p_ide_data->ModeSel = FS_DEV_IDE_MODE_TYPE_PIO;
    }



#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)                        /* ----------------- OUTPUT TRACE INFO ---------------- */
    FS_TRACE_INFO(("IDE/CF FOUND: Name   : \"ide:%d:\"\r\n", p_ide_data->UnitNbr));
    FS_TRACE_INFO(("              # Secs : %d\r\n", size));
    FS_TRACE_INFO(("              Size   : %d MB\r\n", ((size / (1024u / FS_DEV_IDE_SEC_SIZE)) / 1024u)));

    for (i = 0u; i < 20u; i += 2u) {
        str[i + 1u] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_SERIAL_NBR + i];
    }
    for (i = 1u; i < 20u; i += 2u) {
        str[i - 1u] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_SERIAL_NBR + i];
    }
    str[20] = (CPU_CHAR)ASCII_CHAR_NULL;
    FS_TRACE_INFO(("              SN     : %s\r\n", (CPU_CHAR *)&str[0]));

    for (i = 0u; i < 8u; i += 2u) {
        str[i + 1u] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_FIRMWARE_REV + i];
    }
    for (i = 1u; i < 8u; i += 2u) {
        str[i - 1u] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_FIRMWARE_REV + i];
    }
    str[8] = (CPU_CHAR)ASCII_CHAR_NULL;
    FS_TRACE_INFO(("              FW Rev : %s\r\n", (CPU_CHAR *)&str[0]));

    for (i = 1u; i <= 39u; i += 2u) {
        str[i] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MODEL + i - 1u];
    }
    for (i = 0u; i <= 38u; i += 2u) {
        str[i] = (CPU_CHAR)p_data_08[FS_DEV_IDE_IDENTIFY_OFF_MODEL + i + 1u];
    }
    str[20] = (CPU_CHAR)ASCII_CHAR_NULL;
    FS_TRACE_INFO(("              Model  : %s\r\n", (CPU_CHAR *)&str[0]));
#endif

#ifdef FS_FILE_SYSTEM_BUILD_FULL
    FSBuf_Free(p_buf);
#endif

   *p_err = FS_ERR_NONE;
    return (DEF_YES);                                           /* Always chngd if opened.                              */
}


/*
*********************************************************************************************************
*                                         FSDev_IDE_RdData()
*
* Description : Perform data-in command.
*
* Argument(s) : p_ide_data  Pointer to IDE data.
*               ----------  Argument validated by caller.
*
*               cmd         Array holding command.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               cnt         Number of octets to read.
*
*               reps        Number of repetitions.
*
*               mode_type   Transfer mode type :
*
*                               FS_DEV_IDE_MODE_TYPE_PIO     PIO mode.
*                               FS_DEV_IDE_MODE_TYPE_DMA     Multiword DMA mode.
*                               FS_DEV_IDE_MODE_TYPE_UDMA    Ultra-DMA mode.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE               Command performed & data written.
*                               FS_ERR_DEV_IO             Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT    Device is not present.
*                               FS_ERR_DEV_TIMEOUT        Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_IDE_RdData (FS_DEV_IDE_DATA  *p_ide_data,
                                CPU_INT08U        cmd[],
                                CPU_INT08U       *p_dest,
                                CPU_SIZE_T        cnt,
                                CPU_SIZE_T        reps,
                                FS_FLAGS          mode_type,
                                FS_ERR           *p_err)
{
    CPU_BOOLEAN  busy;
    CPU_BOOLEAN  data;
    CPU_INT08U   reg_err;
    CPU_INT08U   reg_status;


                                                                /* --------------------- START CMD -------------------- */
    busy = FSDev_IDE_WaitWhileBusy(p_ide_data->UnitNbr);        /* Wait while dev busy.                                 */
    if (busy == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_RdData(): Dev still busy from prev op.\r\n"));
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

    if (mode_type != FS_DEV_IDE_MODE_TYPE_PIO) {                /* Setup DMA for cmd (init ch).                         */
        FSDev_IDE_BSP_DMA_Start( p_ide_data->UnitNbr,
                                 p_dest,
                                (cnt * reps),
                                 mode_type,
                                 DEF_YES);
    }

    FSDev_IDE_BSP_CmdWr(p_ide_data->UnitNbr, cmd);              /* Wr cmd.                                              */



                                                                /* ---------------------- RD DATA --------------------- */
    if (mode_type == FS_DEV_IDE_MODE_TYPE_PIO) {
        while (reps >= 1u) {
            data = FSDev_IDE_WaitForData(p_ide_data->UnitNbr);  /* Poll status reg for DRQ.                             */
            if (data == DEF_NO) {
                FS_TRACE_DBG(("FSDev_IDE_RdData(): DRQ bit not set for data.\r\n"));
               *p_err = FS_ERR_DEV_TIMEOUT;
                return;
            }

                                                                /* Chk for cmd err.                                     */
            reg_err = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_ERR);
            if (reg_err != 0x00u) {
                FS_DEV_IDE_ERR_RD_CTR_INC(p_ide_data);

                if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_NM) == DEF_YES) {
                    FS_TRACE_DBG(("FSDev_IDE_RdData(): Err after cmd has 'no media' bit set.\r\n"));
                   *p_err = FS_ERR_DEV_NOT_PRESENT;

                } else {
                    FS_TRACE_DBG(("FSDev_IDE_RdData(): Err after cmd: %02X.\r\n", reg_err));
                    FS_DEV_IDE_HANDLE_ERR_BSP(p_ide_data, reg_err);
                   *p_err = FS_ERR_DEV_IO;
                }
                return;
            }

                                                                /* Rd data.                                             */
            FSDev_IDE_BSP_DataRd(p_ide_data->UnitNbr, p_dest, cnt);

            p_dest += cnt;
            reps--;
        }
    }



                                                                /* ---------------------- END CMD --------------------- */
    if (mode_type != FS_DEV_IDE_MODE_TYPE_PIO) {                /* End DMA transfer (& uninit ch).                      */
        FSDev_IDE_BSP_DMA_End( p_ide_data->UnitNbr,
                               p_dest,
                              (cnt * reps),
                               DEF_YES);
    }

    busy = FSDev_IDE_WaitWhileBusy(p_ide_data->UnitNbr);        /* Wait while dev busy.                                 */
    if (busy == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_RdData(): Dev still busy from rd.\r\n"));
       *p_err = FS_ERR_NONE;
        return;
    }

                                                                /* Rd alt status reg (for dly).                         */
    reg_status = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_ALTSTATUS);
    (void)reg_status;
                                                                /* Chk status.                                          */
    reg_status = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_STATUS);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
    if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_DRQ) == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_RdData(): DRQ bit not reset after data.\r\n"));
    }
#endif

    *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_IDE_WrData()
*
* Description : Perform data-out command.
*
* Argument(s) : p_ide_data  Pointer to IDE data.
*               ----------  Argument validated by caller.
*
*               cmd         Array holding command.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               cnt         Number of octets to write.
*
*               reps        Number of repetitions.
*
*               mode_type   Transfer mode type :
*
*                               FS_DEV_IDE_MODE_TYPE_PIO     PIO mode.
*                               FS_DEV_IDE_MODE_TYPE_DMA     Multiword DMA mode.
*                               FS_DEV_IDE_MODE_TYPE_UDMA    Ultra-DMA mode.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE               Command performed & data written.
*                               FS_ERR_DEV_IO             Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT    Device is not present.
*                               FS_ERR_DEV_TIMEOUT        Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_IDE_WrData (FS_DEV_IDE_DATA  *p_ide_data,
                                CPU_INT08U        cmd[],
                                CPU_INT08U       *p_src,
                                CPU_SIZE_T        cnt,
                                CPU_SIZE_T        reps,
                                FS_FLAGS          mode_type,
                                FS_ERR           *p_err)
{
    CPU_BOOLEAN  busy;
    CPU_BOOLEAN  data;
    CPU_INT08U   reg_err;
    CPU_INT08U   reg_status;


                                                                /* --------------------- START CMD -------------------- */
    busy = FSDev_IDE_WaitWhileBusy(p_ide_data->UnitNbr);        /* Wait while dev busy.                                 */
    if (busy == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_WrData(): Dev still busy from prev op.\r\n"));
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

    if (mode_type != FS_DEV_IDE_MODE_TYPE_PIO) {                /* Setup DMA for cmd (init ch).                         */
        FSDev_IDE_BSP_DMA_Start( p_ide_data->UnitNbr,
                                 p_src,
                                (cnt * reps),
                                 mode_type,
                                 DEF_NO);
    }

    FSDev_IDE_BSP_CmdWr(p_ide_data->UnitNbr, cmd);              /* Wr cmd.                                              */



                                                                /* ---------------------- WR DATA --------------------- */
    if (mode_type == FS_DEV_IDE_MODE_TYPE_PIO) {
        while (reps >= 1u) {
            data = FSDev_IDE_WaitForData(p_ide_data->UnitNbr);  /* Poll status reg for DRQ.                             */
            if (data == DEF_NO) {
                FS_TRACE_DBG(("FSDev_IDE_WrData(): DRQ bit not set for data.\r\n"));
               *p_err = FS_ERR_DEV_TIMEOUT;
                return;
            }

                                                                /* Chk for cmd err.                                     */
            reg_err = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_ERR);
            if (reg_err != 0x00u) {
                FS_DEV_IDE_ERR_WR_CTR_INC(p_ide_data);

                if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_NM) == DEF_YES) {
                    FS_TRACE_DBG(("FSDev_IDE_WrData(): Err after cmd has 'no media' bit set.\r\n"));
                   *p_err = FS_ERR_DEV_NOT_PRESENT;

                } else {
                    FS_TRACE_DBG(("FSDev_IDE_WrData(): Err after cmd: %02X.\r\n", reg_err));
                    FS_DEV_IDE_HANDLE_ERR_BSP(p_ide_data, reg_err);
                   *p_err = FS_ERR_DEV_IO;
                }
                return;
            }

                                                                /* Wr data.                                             */
            FSDev_IDE_BSP_DataWr(p_ide_data->UnitNbr, p_src, cnt);

            p_src += cnt;
            reps--;
        }
    }



                                                                /* ---------------------- END CMD --------------------- */
    if (mode_type != FS_DEV_IDE_MODE_TYPE_PIO) {                /* End DMA transfer (& uninit ch).                      */
        FSDev_IDE_BSP_DMA_End(         p_ide_data->UnitNbr,
                              (void *) p_src,
                                      (cnt * reps),
                                       DEF_NO);
    }

    busy = FSDev_IDE_WaitWhileBusy(p_ide_data->UnitNbr);        /* Wait while dev busy.                                 */
    if (busy == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_WrData(): Dev still busy from wr.\r\n"));
       *p_err = FS_ERR_NONE;
        return;
    }

                                                                /* Rd alt status reg (for dly).                         */
    reg_status = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_ALTSTATUS);
   (void)reg_status;
                                                                /* Chk status.                                          */
    reg_status = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_STATUS);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
    if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_DRQ) == DEF_YES) {
        FS_TRACE_DBG(("FSDev_IDE_WrData(): DRQ bit not reset after data.\r\n"));
    }
#endif

    *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_IDE_SetFeature()
*
* Description : Set feature.
*
* Argument(s) : p_ide_data  Pointer to IDE data.
*               ----------  Argument validated by caller.
*
*               feature     Feature to set.
*
*               value       Feature value.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_IDE_SetFeature (FS_DEV_IDE_DATA  *p_ide_data,
                                    CPU_INT08U        feature,
                                    CPU_INT08U        value)
{
    CPU_INT08U  cmd[7];
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
    CPU_INT08U  reg_err;
    CPU_INT08U  reg_status;
#endif


    cmd[0] =  feature;                                          /* Fmt cmd.                                             */
    cmd[1] =  value;
    cmd[2] =  0x00u;
    cmd[3] =  0x00u;
    cmd[4] =  0x00u;
    cmd[5] = (p_ide_data->DrvNbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV;
    cmd[6] =  FS_DEV_IDE_CMD_SET_FEATURES;

    FSDev_IDE_BSP_CmdWr(p_ide_data->UnitNbr, cmd);              /* Wr cmd.                                              */

                                                                /* Chk for err.                                         */
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
    FSDev_IDE_BSP_Dly400_ns(p_ide_data->UnitNbr);
    reg_status = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_STATUS);

    if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_ERR) == DEF_YES) {
        reg_err = FSDev_IDE_BSP_RegRd(p_ide_data->UnitNbr, FS_DEV_IDE_REG_ERR);

        if (reg_err != 0x00u) {
            FS_TRACE_DBG(("FSDev_IDE_SetFeature(): Err after cmd: %02X.\r\n", reg_err));
        }
    }
#endif
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_HandleErrBSP()
*
* Description : Handle BSP error.
*
* Argument(s) : p_ide_data  Pointer to IDE data.
*               ----------  Argument validated by caller.
*
*               err         BSP error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)
static  void  FSDev_IDE_HandleErrBSP (FS_DEV_IDE_DATA  *p_ide_data,
                                      CPU_INT08U        reg_err)
{
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_BBK_ICRC) == DEF_YES) {
        FS_DEV_IDE_ERR_BAD_BLK_CRC_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_UNC) == DEF_YES) {
        FS_DEV_IDE_ERR_UNRECOVER_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_MC) == DEF_YES) {
        FS_DEV_IDE_ERR_MEDIA_CNHGD_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_IDNF) == DEF_YES) {
        FS_DEV_IDE_ERR_ID_NOT_FOUND_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_MCR) == DEF_YES) {
        FS_DEV_IDE_ERR_MEDIA_CHNG_REQD_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_ABORT) == DEF_YES) {
        FS_DEV_IDE_ERR_ABORT_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_NM) == DEF_YES) {
        FS_DEV_IDE_ERR_NO_MEDIA_CTR_INC(p_ide_data);
    }
    if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_AMNF) == DEF_YES) {
        FS_DEV_IDE_ERR_OTHER_CTR_INC(p_ide_data);
    }
}
#endif


/*
*********************************************************************************************************
*                                         FSDev_IDE_SelDev()
*
* Description : Select device.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
*               drv_nbr     Drive number.
*
* Return(s)   : DEF_YES, if device could be selected.
*               DEF_NO,  if device could not be selected.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_IDE_SelDev (FS_QTY      unit_nbr,
                                       CPU_INT08U  drv_nbr)
{
    CPU_BOOLEAN  busy;
    CPU_INT08U   reg_dh;
    CPU_INT08U   reg_err;
    CPU_INT08U   reg_status;
    CPU_BOOLEAN  sel;
    CPU_INT32U   to;


    reg_dh  = (drv_nbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV;
    reg_dh |=  FS_DEV_IDE_REG_DH_one_1 | FS_DEV_IDE_REG_DH_LBA | FS_DEV_IDE_REG_DH_one_2;
    FSDev_IDE_BSP_RegWr(unit_nbr, FS_DEV_IDE_REG_DH, reg_dh);

    to   = 0u;
    busy = DEF_YES;
    while ((to   <  FS_DEV_IDE_BUSY_TO_mS) &&                   /* Wait while timeout not exceeded ...                  */
           (busy == DEF_YES)) {                                 /* ... & dev still busy.                                */
        reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);

        if (DEF_BIT_IS_CLR(reg_status, FS_DEV_IDE_REG_STATUS_BUSY | FS_DEV_IDE_REG_STATUS_DRQ) == DEF_YES) {
            busy = DEF_NO;
        }

        FS_OS_Dly_ms(1u);

        to++;
    }

    if (busy == DEF_NO) {
        busy    =  DEF_YES;
        to      =  0u;

                                                                /* Sel dev.                                             */
        reg_dh  = (drv_nbr == 0u) ? 0u : FS_DEV_IDE_REG_DH_DRV;
        reg_dh |=  FS_DEV_IDE_REG_DH_one_1 | FS_DEV_IDE_REG_DH_LBA | FS_DEV_IDE_REG_DH_one_2;
        FSDev_IDE_BSP_RegWr(unit_nbr, FS_DEV_IDE_REG_DH, reg_dh);

        while ((to   <  FS_DEV_IDE_BUSY_TO_mS) &&               /* Wait while timeout not exceeded ...                  */
               (busy == DEF_YES)) {                             /* ... & dev still busy.                                */
            reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);

            if (DEF_BIT_IS_CLR(reg_status, FS_DEV_IDE_REG_STATUS_BUSY | FS_DEV_IDE_REG_STATUS_DRQ) == DEF_YES) {
                busy = DEF_NO;
            }

            if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_ERR) == DEF_YES) {
                reg_err = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ERR);
                if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_ABORT) == DEF_YES) {
                    reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_STATUS);
                   (void)reg_status;
                }
            }

            FS_OS_Dly_ms(1u);

            to++;
        }
    }

    sel = (busy == DEF_NO) ? DEF_YES : DEF_NO;

    return (sel);
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_WaitForData()
*
* Description : Wait for data request.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
* Return(s)   : DEF_YES, if data request received.
*               DEF_NO,  if data request NOT received.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_IDE_WaitForData (FS_QTY  unit_nbr)
{
    CPU_BOOLEAN  data;
    CPU_INT08U   reg_status;
    CPU_INT32U   to;


    FSDev_IDE_BSP_Dly400_ns(unit_nbr);

    reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);
   (void)reg_status;

    to   = 0u;
    data = DEF_NO;
    while ((to   <  FS_DEV_IDE_BUSY_TO_mS) &&                   /* Wait while timeout not exceeded ...                  */
           (data == DEF_NO)) {                                  /* ... & dev has not req'd data.                        */
        reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_STATUS);

        if (DEF_BIT_IS_CLR(reg_status, FS_DEV_IDE_REG_STATUS_BUSY) == DEF_YES) {
            if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_DRQ) == DEF_YES) {
                data = DEF_YES;
            }
        }

        FS_OS_Dly_ms(1u);

        to++;
    }

    return (data);
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_WaitWhileBusy()
*
* Description : Wait while device is busy.
*
* Argument(s) : unit_nbr    Unit number of device to control.
*
* Return(s)   : DEF_YES, if device is still busy.
*               DEF_NO,  if device is NOT   busy.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_IDE_WaitWhileBusy (FS_QTY  unit_nbr)
{
    CPU_BOOLEAN  busy;
    CPU_INT08U   reg_err;
    CPU_INT08U   reg_status;
    CPU_INT32U   to;


    to   = 0u;
    busy = DEF_YES;
    while ((to   <  FS_DEV_IDE_BUSY_TO_mS) &&                   /* Wait while timeout not exceeded ...                  */
           (busy == DEF_YES)) {                                 /* ... & dev still busy.                                */
        reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);
       (void)reg_status;

        reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);
        if (DEF_BIT_IS_CLR(reg_status, FS_DEV_IDE_REG_STATUS_BUSY) == DEF_YES) {
            busy = DEF_NO;
        }

        if (busy == DEF_YES) {
            FSDev_IDE_BSP_Dly400_ns(unit_nbr);
        }

        to++;
    }

    reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ALTSTATUS);
   (void)reg_status;

    reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_STATUS);
    if (DEF_BIT_IS_SET(reg_status, FS_DEV_IDE_REG_STATUS_ERR) == DEF_YES) {
        reg_err = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_ERR);
        if (DEF_BIT_IS_SET(reg_err, FS_DEV_IDE_REG_ERR_ABORT) == DEF_YES) {
            reg_status = FSDev_IDE_BSP_RegRd(unit_nbr, FS_DEV_IDE_REG_STATUS);
            (void)reg_status;
        }
    }

    return (busy);
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_DataFree()
*
* Description : Free an IDE data object.
*
* Argument(s) : p_ide_data  Pointer to a IDE data object.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_IDE_DataFree (FS_DEV_IDE_DATA  *p_ide_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_ide_data->NextPtr   = FSDev_IDE_ListFreePtr;              /* Add to free pool.                                    */
    FSDev_IDE_ListFreePtr = p_ide_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         FSDev_IDE_DataGet()
*
* Description : Allocate & initialize an IDE data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to an IDE data object, if NO errors.
*               Pointer to NULL,               otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_IDE_DATA  *FSDev_IDE_DataGet (void)
{
    LIB_ERR           alloc_err;
    CPU_SIZE_T        octets_reqd;
    FS_DEV_IDE_DATA  *p_ide_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FSDev_IDE_ListFreePtr == (FS_DEV_IDE_DATA *)0) {
        p_ide_data = (FS_DEV_IDE_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_IDE_DATA),
                                                       sizeof(CPU_DATA),
                                                      &octets_reqd,
                                                      &alloc_err);
        if (p_ide_data == (FS_DEV_IDE_DATA *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_IDE_DataGet(): Could not alloc mem for IDE data: %d octets required.\r\n", octets_reqd));
            return ((FS_DEV_IDE_DATA *)0);
        }
        (void)alloc_err;

        FSDev_IDE_UnitCtr++;


    } else {
        p_ide_data            = FSDev_IDE_ListFreePtr;
        FSDev_IDE_ListFreePtr = FSDev_IDE_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    p_ide_data->Init                =  DEF_NO;

    p_ide_data->Size                =  0u;
    p_ide_data->MultSecCnt          =  0u;
    p_ide_data->DrvNbr              =  0u;
    p_ide_data->ModeSel             =  FS_DEV_IDE_MODE_TYPE_PIO;
    p_ide_data->PIO_ModeMax         =  0;
    p_ide_data->DMA_ModeMax         = -1;
    p_ide_data->UDMA_ModeMax        = -1;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* Clr stat ctrs.                                       */
    p_ide_data->StatRdCtr           =  0u;
    p_ide_data->StatWrCtr           =  0u;
#endif

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* Clr err ctrs.                                        */
    p_ide_data->ErrRdCtr            =  0u;
    p_ide_data->ErrWrCtr            =  0u;

    p_ide_data->ErrBadBlkCRC_Ctr    =  0u;
    p_ide_data->ErrUnrecoverCtr     =  0u;
    p_ide_data->ErrMediaChngdCtr    =  0u;
    p_ide_data->ErrID_NotFoundCtr   =  0u;
    p_ide_data->ErrMediaChngReqdCtr =  0u;
    p_ide_data->ErrAbortCtr         =  0u;
    p_ide_data->ErrNoMediaCtr       =  0u;
    p_ide_data->ErrOtherCtr         =  0u;
#endif

    p_ide_data->NextPtr             = (FS_DEV_IDE_DATA *)0;

    return (p_ide_data);
}
