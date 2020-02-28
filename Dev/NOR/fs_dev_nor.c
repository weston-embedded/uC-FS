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
*
*                                          NOR FLASH DEVICES
*
* Filename : fs_dev_nor.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports NOR-type Flash memory devices, including :
*                (a) Parallel NOR Flash.
*                    (1) AMD-compatible devices.
*                    (2) Intel-compatible devices.
*                    (3) SST devices.
*                    (4) Any other similar memory devices.
*
*                (b) Serial   NOR Flash.
*                    (1) ST M25 SPI devices.
*                    (2) SST SST25 SPI devices.
*                    (3) Any other similar memory devices.
*
*             (2) Supported media MUST have the following characteristics :
*                 (a) Medium organized into units (called blocks) which are erased at the same time.
*                 (b) When erased, all bits are 1.
*                 (c) Only an erase operation can change a bit from a 0 to a 1.
*                 (d) Any bit  can be individually programmed  from a 1 to a 0.
*                 (e) Any word can be individually accessed (read or programmed).
*
*             (3) Supported media TYPICALLY have the following characteristics :
*                 (a) A program operation takes much longer than a read    operation.
*                 (b) An erase  operation takes much longer than a program operation.
*                 (c) The number of erase operations per block is limited.
*
*             (4) Some NOR media have non-uniform layouts, e.g., not all erase blocks are the same
*                 size.  Often, smaller blocks provide code storage in the top or bottom portion of
*                 the memory.  This driver REQUIRES that the medium be uniform, only a uniform portion
*                 of the device be used, or multiple blocks in regions of smaller blocks be represented
*                 as a single block by the physical-layer driver.
*
*             (5) #### Improve allocation of L2P table by packing bits of entries.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  "../../Source/fs.h"
#include  "../../Source/fs_util.h"
#include  "fs_dev_nor.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_BLK_HDR_LEN                           32u

#define  FS_DEV_NOR_SEC_HDR_LEN                            8u
#define  FS_DEV_NOR_SEC_HDR_LEN_LOG                        3u

                                                                /* --------------- SECTOR STATUS MARKERS -------------- */
#define  FS_DEV_NOR_STATUS_SEC_ERASED             0xFFFFFFFFuL  /* Sector is erased (unused).                           */
#define  FS_DEV_NOR_STATUS_SEC_WRITING            0xFFFFFF00uL  /* Sector is in process of being written.               */
#define  FS_DEV_NOR_STATUS_SEC_VALID              0xFFFF0000uL  /* Sector holds valid data.                             */
#define  FS_DEV_NOR_STATUS_SEC_INVALID            0x00000000u   /* Sector data is invalid.                              */

                                                                /* ---------------- BLOCK HEADER FIELDS --------------- */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_MARK1                   0u   /* Marker word 1 offset.                                */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_MARK2                   4u   /* Marker word 2 offset.                                */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT               8u   /* Erase count offset.                                  */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_VER                    12u   /* Format version offset.                               */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_SEC_SIZE               14u   /* Sec size offset.                                     */
#define  FS_DEV_NOR_BLK_HDR_OFFSET_BLK_CNT                16u   /* Blk cnt  offset.                                     */

#define  FS_DEV_NOR_BLK_HDR_MARK_WORD_1           0x4E205346u   /* Marker word 1 ("FS N").                              */
#define  FS_DEV_NOR_BLK_HDR_MARK_WORD_2           0x2020524Fu   /* Marker word 2 ("OR  ").                              */
#define  FS_DEV_NOR_BLK_HDR_ERASE_CNT_INVALID     0xFFFFFFFFuL  /* Erase count invalid.                                 */
#define  FS_DEV_NOR_BLK_HDR_VER                       0x0401u   /* Format version (4.01).                               */

                                                                /* --------------- SECTOR HEADER FIELDS --------------- */
#define  FS_DEV_NOR_SEC_HDR_OFFSET_SEC_NBR                 0u   /* Logical sector number offset.                        */
#define  FS_DEV_NOR_SEC_HDR_OFFSET_STATUS                  4u   /* Status of sector offset.                             */

#define  FS_DEV_NOR_SEC_NBR_INVALID       DEF_INT_32U_MAX_VAL   /* Logical sector number (invalid).                     */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                   NOR FLASH DEVICE DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_nor_data  FS_DEV_NOR_DATA;
struct  fs_dev_nor_data {
                                                                /* ------------------- DEV OPEN INFO ------------------ */
    FS_SEC_QTY            Size;                                 /* Size of disk (in secs).                              */
    FS_SEC_QTY            SecCntTot;                            /* Tot of secs on dev.                                  */
    CPU_INT08U            SecCntTotLog;                         /* Base-2 log of tot of secs on dev.                    */
    FS_SEC_QTY            BlkCntUsed;                           /* Cnt of blks in file system.                          */
    FS_SEC_NBR            BlkNbrFirst;                          /* Nbr  of first blk in file system.                    */
    CPU_INT08U            BlkSizeLog;                           /* Base-2 log of blk size.                              */
    CPU_INT08U            SecSizeLog;                           /* Base-2 log of sec size.                              */
    FS_SEC_QTY            BlkSecCnts;                           /* Cnt of secs in each blk.                             */
    FS_SEC_QTY            AB_Cnt;                               /* Active blk cnt.                                      */


                                                                /* ------------------- DEV OPEN DATA ------------------ */
    void                 *BufPtr;                               /* Ptr to drv buf.                                      */


                                                                /* ------------------ MOUNT DEV INFO ------------------ */
    void                 *BlkSecCntValidTbl;                    /* Tbl of cnts of valid secs in blk.                    */
    void                 *BlkEraseMap;                          /* Map of blk erase status.                             */
    void                 *L2P_Tbl;                              /* Logical to phy addr tbl.                             */
    FS_SEC_NBR           *AB_IxTbl;                             /* Active blk ix tbl.                                   */
    FS_SEC_NBR           *AB_SecNextTbl;                        /* Next sec in active blk tbl.                          */
    CPU_BOOLEAN           BlkWearLevelAvail;                    /* Erased wear level blk avail.                         */
    FS_SEC_QTY            BlkCntValid;                          /* Cnt of blks with valid data.                         */
    FS_SEC_QTY            BlkCntErased;                         /* Cnt of erased blks.                                  */
    FS_SEC_QTY            BlkCntInvalid;                        /* Cnt of blks with only invalid data.                  */
    FS_SEC_QTY            SecCntValid;                          /* Cnt of secs with valid data.                         */
    FS_SEC_QTY            SecCntErased;                         /* Cnt of erased secs.                                  */
    FS_SEC_QTY            SecCntInvalid;                        /* Cnt of secs with invalid data.                       */
    CPU_INT32U            EraseCntMin;                          /* Min erase cnt.                                       */
    CPU_INT32U            EraseCntMax;                          /* Max erase cnt.                                       */
    CPU_BOOLEAN           Mounted;                              /* Low-level mounted.                                   */


                                                                /* --------------------- CFG INFO --------------------- */
    CPU_ADDR              AddrStart;                            /* Start addr of data within flash.                     */
    CPU_INT32U            DevSize;                              /* Size of flash, in octets.                            */
    FS_SEC_SIZE           SecSize;                              /* Sec size of low-level formatted flash.               */
    CPU_INT08U            PctRsvd;                              /* Pct of device area rsvd.                             */
    CPU_INT16U            EraseCntDiffTh;                       /* Erase count difference threshold.                    */


                                                                /* --------------------- PHY INFO --------------------- */
    FS_DEV_NOR_PHY_API   *PhyPtr;                               /* Ptr to phy drv.                                      */
    FS_DEV_NOR_PHY_DATA  *PhyDataPtr;                           /* Ptr to phy info.                                     */


#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* --------------------- STAT CTRS -------------------- */
    FS_CTR                StatRdCtr;                            /* Secs rd.                                             */
    FS_CTR                StatWrCtr;                            /* Secs wr.                                             */
    FS_CTR                StatCopyCtr;                          /* Secs copied.                                         */
    FS_CTR                StatReleaseCtr;                       /* Secs released.                                       */
    FS_CTR                StatRdOctetCtr;                       /* Octets rd.                                           */
    FS_CTR                StatWrOctetCtr;                       /* Octets wr.                                           */
    FS_CTR                StatEraseBlkCtr;                      /* Blks erased.                                         */
    FS_CTR                StatInvalidBlkCtr;                    /* Blks invalidated.                                    */
#endif


#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* --------------------- ERR CTRS --------------------- */
    FS_CTR                ErrRdCtr;                             /* Rd errs.                                             */
    FS_CTR                ErrWrCtr;                             /* Wr errs.                                             */
    FS_CTR                ErrEraseCtr;                          /* Erase errs.                                          */
    FS_CTR                ErrWrFailCtr;                         /* Wr failures.                                         */
    FS_CTR                ErrEraseFailCtr;                      /* Erase failures.                                      */
#endif

    FS_DEV_NOR_DATA      *NextPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  FS_DEV_NOR_NAME_LEN                3u

static  const  CPU_CHAR  FSDev_NOR_Name[] = "nor";


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

static  FS_DEV_NOR_DATA  *FSDev_NOR_ListFreePtr;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                        /* ---- DEV INTERFACE FNCTS --- */
static  const  CPU_CHAR  *FSDev_NOR_NameGet            (void);                          /* Get name of driver.          */

static  void              FSDev_NOR_Init               (FS_ERR           *p_err);       /* Init driver.                 */

static  void              FSDev_NOR_Open               (FS_DEV           *p_dev,        /* Open device.                 */
                                                        void             *p_dev_cfg,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_Close              (FS_DEV           *p_dev);       /* Close device.                */

static  void              FSDev_NOR_Rd                 (FS_DEV           *p_dev,        /* Read from device.            */
                                                        void             *p_dest,
                                                        FS_SEC_NBR        start,
                                                        FS_SEC_QTY        cnt,
                                                        FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_Wr                 (FS_DEV           *p_dev,        /* Write to device.             */
                                                        void             *p_src,
                                                        FS_SEC_NBR        start,
                                                        FS_SEC_QTY        cnt,
                                                        FS_ERR           *p_err);
#endif

static  void              FSDev_NOR_Query              (FS_DEV           *p_dev,        /* Get device info.             */
                                                        FS_DEV_INFO      *p_info,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_IO_Ctrl            (FS_DEV           *p_dev,        /* Perform device I/O control.  */
                                                        CPU_INT08U        opt,
                                                        void             *p_data,
                                                        FS_ERR           *p_err);


                                                                                        /* -------- LOCAL FNCTS ------- */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_LowFmtHandler      (FS_DEV_NOR_DATA  *p_nor_data,   /* Low-level fmt NOR.           */
                                                        FS_ERR           *p_err);
#endif

static  void              FSDev_NOR_LowMountHandler    (FS_DEV_NOR_DATA  *p_nor_data,   /* Low-level mount NOR.         */
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_LowUnmountHandler  (FS_DEV_NOR_DATA  *p_nor_data,   /* Low-level unmount NOR.       */
                                                        FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_LowCompactHandler  (FS_DEV_NOR_DATA  *p_nor_data,   /* Low-level compact NOR.       */
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_LowDefragHandler   (FS_DEV_NOR_DATA  *p_nor_data,   /* Low-level defrag NOR.        */
                                                        FS_ERR           *p_err);
#endif

static  void              FSDev_NOR_PhyRdHandler       (FS_DEV_NOR_DATA  *p_nor_data,   /* Rd octets.                   */
                                                        void             *p_dest,
                                                        CPU_INT32U        start,
                                                        CPU_INT32U        cnt,
                                                        FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_PhyWrHandler       (FS_DEV_NOR_DATA  *p_nor_data,   /* Wr octets.                   */
                                                        void             *p_src,
                                                        CPU_INT32U        start,
                                                        CPU_INT32U        cnt,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_PhyEraseBlkHandler (FS_DEV_NOR_DATA  *p_nor_data,   /* Erase blk.                   */
                                                        CPU_INT32U        start,
                                                        CPU_INT32U        size,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_PhyEraseChipHandler(FS_DEV_NOR_DATA  *p_nor_data,   /* Erase chip.                  */
                                                        FS_ERR           *p_err);
#endif

static  void              FSDev_NOR_CalcDevInfo        (FS_DEV_NOR_DATA  *p_nor_data,   /* Calc NOR info.               */
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_AllocDevData       (FS_DEV_NOR_DATA  *p_nor_data,   /* Alloc NOR data.              */
                                                        FS_ERR           *p_err);



static  void              FSDev_NOR_RdBlkHdr           (FS_DEV_NOR_DATA  *p_nor_data,   /* Rd blk hdr.                  */
                                                        void             *p_dest,
                                                        FS_SEC_NBR        blk_ix,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_RdSecLogical       (FS_DEV_NOR_DATA  *p_nor_data,   /* Rd logical sec.              */
                                                        void             *p_dest,
                                                        FS_SEC_NBR        sec_nbr_logical,
                                                        FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_WrSecLogical       (FS_DEV_NOR_DATA  *p_nor_data,   /* Wr logical sec.              */
                                                        void             *p_src,
                                                        FS_SEC_NBR        sec_nbr_logical,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_WrSecLogicalHandler(FS_DEV_NOR_DATA  *p_nor_data,   /* Wr logical sec handler.      */
                                                        void             *p_src,
                                                        FS_SEC_NBR        sec_nbr_logical,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_InvalidateSec      (FS_DEV_NOR_DATA  *p_nor_data,    /* Invalidate sec.             */
                                                        FS_SEC_NBR        sec_nbr_phy,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_EraseBlkPrepare    (FS_DEV_NOR_DATA  *p_nor_data,   /* Prepare & erase blk.         */
                                                        FS_SEC_NBR        blk_ix,
                                                        FS_ERR           *p_err);

static  void              FSDev_NOR_EraseBlkEmptyFmt   (FS_DEV_NOR_DATA  *p_nor_data,   /* Erase & fmt empty blk.       */
                                                        FS_SEC_NBR        blk_ix,
                                                        FS_ERR           *p_err);

static  FS_SEC_NBR        FSDev_NOR_FindEraseBlk       (FS_DEV_NOR_DATA  *p_nor_data);  /* Find blk to erase.           */

static  FS_SEC_NBR        FSDev_NOR_FindEraseBlkWear   (FS_DEV_NOR_DATA  *p_nor_data);  /* Find blk to erase.           */

static  FS_SEC_NBR        FSDev_NOR_FindErasedBlk      (FS_DEV_NOR_DATA  *p_nor_data);  /* Find blk that is erased.     */
#endif


static  void              FSDev_NOR_AB_ClrAll         (FS_DEV_NOR_DATA  *p_nor_data);   /* Clr AB data.                 */

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_QTY        FSDev_NOR_GetAB_Cnt         (FS_DEV_NOR_DATA  *p_nor_data);   /* Get cnt of ABs.              */

static  CPU_BOOLEAN       FSDev_NOR_IsAB              (FS_DEV_NOR_DATA  *p_nor_data,    /* Chk whether blk is AB.       */
                                                       FS_SEC_NBR        blk_ix);
#endif

static  CPU_BOOLEAN       FSDev_NOR_AB_Add            (FS_DEV_NOR_DATA  *p_nor_data,    /* Add new AB.                  */
                                                       FS_SEC_NBR        blk_ix,
                                                       FS_SEC_NBR        sec_ix_next);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_AB_RemoveAll      (FS_DEV_NOR_DATA  *p_nor_data);   /* Remove all ABs.              */

static  void              FSDev_NOR_AB_Remove         (FS_DEV_NOR_DATA  *p_nor_data,    /* Remove AB.                   */
                                                       FS_SEC_NBR        blk_ix);

static  FS_SEC_QTY        FSDev_NOR_AB_SecCntTotErased(FS_DEV_NOR_DATA  *p_nor_data);   /* Get tot erased secs in blks. */

static  FS_SEC_QTY        FSDev_NOR_AB_SecCntErased   (FS_DEV_NOR_DATA  *p_nor_data,    /* Get erased secs in AB.       */
                                                       FS_SEC_NBR        blk_ix);

static  FS_SEC_NBR        FSDev_NOR_AB_SecFind        (FS_DEV_NOR_DATA  *p_nor_data,    /* Get phy sec from AB.         */
                                                       FS_SEC_NBR        sec_nbr_logical);
#endif


#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_GetBlkInfo        (FS_DEV_NOR_DATA  *p_nor_data,    /* Get blk info.                */
                                                       FS_SEC_NBR        blk_ix,
                                                       CPU_BOOLEAN      *p_erased,
                                                       FS_SEC_QTY       *p_sec_cnt_valid);
#endif

static  void              FSDev_NOR_SetBlkErased      (FS_DEV_NOR_DATA  *p_nor_data,    /* Set blk erased status.       */
                                                       FS_SEC_NBR        blk_ix,
                                                       CPU_BOOLEAN       erased);

static  void              FSDev_NOR_SetBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,    /* Set blk valid sec cnt.       */
                                                       FS_SEC_NBR        blk_ix,
                                                       FS_SEC_QTY        sec_cnt_valid);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_NOR_DecBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,    /* Inc blk valid sec cnt.       */
                                                       FS_SEC_NBR        blk_ix);

static  void              FSDev_NOR_IncBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,    /* Dec blk valid sec cnt.       */
                                                       FS_SEC_NBR        blk_ix);
#endif

static  CPU_INT32U        FSDev_NOR_BlkIx_to_Addr     (FS_DEV_NOR_DATA  *p_nor_data,    /* Get addr of a blk.           */
                                                       FS_SEC_NBR        blk_ix);

static  CPU_INT32U        FSDev_NOR_SecNbrPhy_to_Addr (FS_DEV_NOR_DATA  *p_nor_data,    /* Get addr of a sec.           */
                                                       FS_SEC_NBR        sec_nbr_phy);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR        FSDev_NOR_SecNbrPhy_to_BlkIx(FS_DEV_NOR_DATA  *p_nor_data,    /* Get blk ix of a sec.         */
                                                       FS_SEC_NBR        sec_nbr_phy);

static  FS_SEC_NBR        FSDev_NOR_BlkIx_to_SecNbrPhy(FS_DEV_NOR_DATA  *p_nor_data,    /* Get 1st sec in blk.          */
                                                       FS_SEC_NBR        blk_ix);
#endif

static  void             *FSDev_NOR_L2P_Create        (FS_DEV_NOR_DATA  *p_nor_data,    /* Alloc L2P tbl.               */
                                                       CPU_SIZE_T       *p_octets_reqd);

static  FS_SEC_NBR        FSDev_NOR_L2P_GetEntry      (FS_DEV_NOR_DATA  *p_nor_data,    /* Get L2P entry for log sec.   */
                                                       FS_SEC_NBR        sec_nbr_logical);

static  void              FSDev_NOR_L2P_SetEntry      (FS_DEV_NOR_DATA  *p_nor_data,    /* Set L2P entry for log sec.   */
                                                       FS_SEC_NBR        sec_nbr_logical,
                                                       FS_SEC_NBR        sec_nbr_phy);



static  void              FSDev_NOR_DataFree          (FS_DEV_NOR_DATA  *p_nor_data);   /* Free NOR data.               */

static  FS_DEV_NOR_DATA  *FSDev_NOR_DataGet           (void);                           /* Allocate & init NOR data.    */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_API  FSDev_NOR = {
    FSDev_NOR_NameGet,
    FSDev_NOR_Init,
    FSDev_NOR_Open,
    FSDev_NOR_Close,
    FSDev_NOR_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_NOR_Wr,
#endif
    FSDev_NOR_Query,
    FSDev_NOR_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FSDev_NOR_LowFmt()
*
* Description : Low-level format a NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE                   Device low-level formatted successfully.
*                               FS_ERR_NAME_NULL              Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_DEV_INVALID            Argument 'name_dev' specifies an invalid device.
*
*                                                             ------- RETURNED BY FSDev_IO_Ctrl() -------
*                               FS_ERR_DEV_NOT_OPEN           Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted (see Note #2).
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Low-level formatting associates physical areas (sectors) of the device with logical
*                   sector numbers.  A NOR medium MUST be low-level formatted with this driver prior to
*                   access by the high-level file system, a requirement which the device module enforces.
*
*               (3) If 'FS_ERR_DEV_INVALID_LOW_FMT' is returned, the low-level format information was
*                   written to the device, but the device could not be mounted, perhaps because of a
*                   hardware problem.
*********************************************************************************************************
*/

void  FSDev_NOR_LowFmt (CPU_CHAR  *name_dev,
                        FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ----------------- LOW-LEVEL FMT DEV ---------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_LOW_FMT,
                  (void *)0,
                          p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(name_dev, p_err);
    }
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_LowMount()
*
* Description : Low-level mount a NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE                   Device low-level mounted successfully.
*                               FS_ERR_NAME_NULL              Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_DEV_INVALID            Argument 'name_dev' specifies an invalid device.
*
*                                                             ------- RETURNED BY FSDev_IO_Ctrl() -------
*                               FS_ERR_DEV_NOT_OPEN           Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Low-level mounting parses the on-device structure, detecting the presence of a valid
*                   low-level format.  If 'FS_ERR_DEV_INVALID_LOW_FMT' is returned, the device is NOT
*                   low-level formatted (see 'FSDev_NOR_LowFmt()  Note #2').
*********************************************************************************************************
*/

void  FSDev_NOR_LowMount (CPU_CHAR  *name_dev,
                          FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ---------------- LOW-LEVEL MOUNT DEV --------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_LOW_MOUNT,
                  (void *)0,
                          p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(name_dev, p_err);
    }
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_LowUnmount()
*
* Description : Low-level unmount a NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Device low-level unmounted successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Low-level unmounting clears software knowledge of the on-disk structures, forcing
*                   the device to again be low-level mounted or formatted prior to further use.
*********************************************************************************************************
*/

void  FSDev_NOR_LowUnmount (CPU_CHAR  *name_dev,
                            FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* --------------- LOW-LEVEL UNMOUNT DEV -------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_LOW_UNMOUNT,
                  (void *)0,
                          p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(name_dev, p_err);
    }
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_LowCompact()
*
* Description : Low-level compact a NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Device low-level compacted successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Compacting groups sectors containing high-level data into as few blocks as possible.
*                   If an image of a file system is to be formed for deployment, to be burned into chips
*                   for production, then it should be compacted after all files & directories are created.
*********************************************************************************************************
*/

void  FSDev_NOR_LowCompact (CPU_CHAR  *name_dev,
                            FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* --------------- LOW-LEVEL COMPACT DEV -------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_LOW_COMPACT,
                  (void *)0,
                          p_err);
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_LowDefrag()
*
* Description : Low-level defragment a NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Device low-level defragmented successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Defragmentation groups sectors containing high-level data into as few blocks as
*                   possible, in order of logical sector.  A defragmented file system should have near-
*                   optimal access speeds in a read-only environment.  See also 'FSDev_NOR_LowCompact()  Note #2'.
*********************************************************************************************************
*/

void  FSDev_NOR_LowDefrag (CPU_CHAR  *name_dev,
                           FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* --------------- LOW-LEVEL DEFRAG DEV --------------- */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_LOW_DEFRAG,
                  (void *)0,
                          p_err);
}


/*
*********************************************************************************************************
*                                          FSDev_NOR_PhyRd()
*
* Description : Read from a NOR device & store data in buffer.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_dest      Pointer to destination buffer.
*
*               start       Start address of read (relative to start of device).
*
*               cnt         Number of octets to read.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Octets read successfully.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*********************************************************************************************************
*/

void  FSDev_NOR_PhyRd (CPU_CHAR    *name_dev,
                       void        *p_dest,
                       CPU_INT32U   start,
                       CPU_INT32U   cnt,
                       FS_ERR      *p_err)
{
    CPU_INT16S               cmp_val;
    FS_DEV_NOR_IO_CTRL_DATA  data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_dest == (void *)0) {                                  /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* -------------------- RD PHY DEV -------------------- */
    data.DataPtr = p_dest;
    data.Start   = start;
    data.Size    = cnt;

    FSDev_IO_Ctrl(         name_dev,
                           FS_DEV_IO_CTRL_PHY_RD,
                  (void *)&data,
                           p_err);
}


/*
*********************************************************************************************************
*                                          FSDev_NOR_PhyWr()
*
* Description : Write data to a NOR device from a buffer.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_src       Pointer to source buffer.
*
*               start       Start address of write (relative to start of device).
*
*               cnt         Number of octets to write.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Octets written successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_src' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Care should be taken if this function is used while a file system exists on the
*                   device, or if the device is low-level formatted.  The octet location(s) modified
*                   are NOT verified as being outside any existing file system or low-level format
*                   information.
*
*               (3) During a program operation, only '1' bits can be changed; a '0' bit cannot be changed
*                   to a '1'.  The application MUST know that the octets being programmed have not already
*                   been programmed.
*********************************************************************************************************
*/

void  FSDev_NOR_PhyWr (CPU_CHAR    *name_dev,
                       void        *p_src,
                       CPU_INT32U   start,
                       CPU_INT32U   cnt,
                       FS_ERR      *p_err)
{
    CPU_INT16S               cmp_val;
    FS_DEV_NOR_IO_CTRL_DATA  data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_src == (void *)0) {                                   /* Validate src ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* -------------------- WR PHY DEV -------------------- */
    data.DataPtr = p_src;
    data.Start   = start;
    data.Size    = cnt;

    FSDev_IO_Ctrl(         name_dev,
                           FS_DEV_IO_CTRL_PHY_WR,
                  (void *)&data,
                           p_err);
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PhyEraseBlk()
*
* Description : Erase block of NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               start       Start address of block (relative to start of device).
*
*               size        Size of block, in octets.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Block erased successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) Care should be taken if this function is used while a file system exists on the
*                   device, or if the device is low-level formatted.  The erased block is NOT verified
*                   as being outside any existing file system or low-level format information.
*********************************************************************************************************
*/

void  FSDev_NOR_PhyEraseBlk (CPU_CHAR    *name_dev,
                             CPU_INT32U   start,
                             CPU_INT32U   size,
                             FS_ERR      *p_err)
{
    CPU_INT16S               cmp_val;
    FS_DEV_NOR_IO_CTRL_DATA  data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ------------------- ERASE PHY BLK ------------------ */
    data.DataPtr = (void *)0;
    data.Start   =  start;
    data.Size    =  size;

    FSDev_IO_Ctrl(         name_dev,
                           FS_DEV_IO_CTRL_PHY_ERASE_BLK,
                  (void *)&data,
                           p_err);
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PhyEraseChip()
*
* Description : Erase entire NOR device.
*
* Argument(s) : name_dev    Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE               Device erased successfully.
*                               FS_ERR_NAME_NULL          Argument 'name_dev' passed a NULL pointer.
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
* Note(s)     : (1) The device MUST be a NOR device (e.g., "nor:0:").
*
*               (2) This function should NOT be used while a file system exists on the device, or if the
*                   device is low-level formatted, unless the intent is to destroy all existing information.
*********************************************************************************************************
*/

void  FSDev_NOR_PhyEraseChip (CPU_CHAR  *name_dev,
                              FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(name_dev, (CPU_CHAR *)FSDev_NOR_Name, FS_DEV_NOR_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (name_dev[FS_DEV_NOR_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ------------------- ERASE PHY BLK ------------------ */
    FSDev_IO_Ctrl(        name_dev,
                          FS_DEV_IO_CTRL_PHY_ERASE_CHIP,
                  (void *)0,
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
*                                         FSDev_NOR_NameGet()
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

static  const  CPU_CHAR  *FSDev_NOR_NameGet (void)
{
    return (FSDev_NOR_Name);
}


/*
*********************************************************************************************************
*                                          FSDev_NOR_Init()
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

static  void  FSDev_NOR_Init (FS_ERR  *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
#endif

    FSDev_NOR_UnitCtr     =  0u;
    FSDev_NOR_ListFreePtr = (FS_DEV_NOR_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_NOR_Open()
*
* Description : Open (initialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to open.
*
*               p_dev_cfg   Pointer to device configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Device opened successfully.
*                               FS_ERR_DEV_ALREADY_OPEN          Device is already opened.
*                               FS_ERR_DEV_INVALID_CFG           Device configuration specified invalid.
*                               FS_ERR_DEV_INVALID_LOW_FMT       Device needs to be low-level formatted.
*                               FS_ERR_DEV_INVALID_LOW_PARAMS    Device low-level device parameters invalid.
*                               FS_ERR_DEV_INVALID_UNIT_NBR      Device unit number is invalid.
*                               FS_ERR_DEV_IO                    Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT           Device is not present.
*                               FS_ERR_DEV_TIMEOUT               Device timeout.
*                               FS_ERR_MEM_ALLOC                 Memory could not be allocated.
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

static  void  FSDev_NOR_Open (FS_DEV  *p_dev,
                              void    *p_dev_cfg,
                              FS_ERR  *p_err)
{
    FS_DEV_NOR_DATA      *p_nor_data;
    FS_DEV_NOR_PHY_DATA  *p_phy_data;
    FS_DEV_NOR_CFG       *p_nor_cfg;



#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_dev_cfg == (void *)0) {                               /* Validate cfg ptr.                                    */
        FS_TRACE_DBG(("FSDev_NOR_Open(): NOR cfg NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }
#endif

    p_nor_cfg = (FS_DEV_NOR_CFG *)p_dev_cfg;

    if (p_nor_cfg->DevSize < 1u) {                              /* Validate size.                                       */
        FS_TRACE_DBG(("FSDev_NOR_Open(): Invalid NOR size: %d.\r\n", p_nor_cfg->DevSize));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    switch (p_nor_cfg->SecSize) {                               /* Validate sec size.                                   */
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
             break;

        default:
            FS_TRACE_DBG(("FSDev_NOR_Open(): Invalid NOR sec size: %d.\r\n", p_nor_cfg->SecSize));
           *p_err = FS_ERR_DEV_INVALID_CFG;
            return;
    }

    if ((p_nor_cfg->PctRsvd != 0u) &&                           /* Validate rsvd pct.                                   */
        (p_nor_cfg->PctRsvd < FS_DEV_NOR_PCT_RSVD_MIN) &&
        (p_nor_cfg->PctRsvd > FS_DEV_NOR_PCT_RSVD_MAX)) {
        FS_TRACE_DBG(("FSDev_NOR_Open(): Rsvd space pct specified invalid.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if ((p_nor_cfg->EraseCntDiffTh != 0u) &&                    /* Validate erase cnt diff th.                          */
        (p_nor_cfg->EraseCntDiffTh < FS_DEV_NOR_ERASE_CNT_DIFF_TH_MIN) &&
        (p_nor_cfg->EraseCntDiffTh > FS_DEV_NOR_ERASE_CNT_DIFF_TH_MAX)) {
        FS_TRACE_DBG(("FSDev_NOR_Open(): Erase cnt diff th specified invalid.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if (p_nor_cfg->PhyPtr == (FS_DEV_NOR_PHY_API *)0) {         /* Validate phy drv ptr.                                */
        FS_TRACE_DBG(("FSDev_NOR_Open(): NOR phy drv NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }


                                                                /* ------------------ INIT UNIT INFO ------------------ */
    p_nor_data = FSDev_NOR_DataGet();
    if (p_nor_data == (FS_DEV_NOR_DATA *)0) {
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

                                                                /* Cfg'd info.                                          */
    p_nor_data->AddrStart        =  p_nor_cfg->AddrStart;
    p_nor_data->DevSize          =  p_nor_cfg->DevSize;
    p_nor_data->SecSize          =  p_nor_cfg->SecSize;
    p_nor_data->PctRsvd          = (p_nor_cfg->PctRsvd          ==  0u) ? FS_DEV_NOR_PCT_RSVD_DFLT           : p_nor_cfg->PctRsvd;
    p_nor_data->EraseCntDiffTh   = (p_nor_cfg->EraseCntDiffTh   ==  0u) ? FS_DEV_NOR_ERASE_CNT_DIFF_TH_DFLT  : p_nor_cfg->EraseCntDiffTh;

    p_nor_data->PhyPtr           =  p_nor_cfg->PhyPtr;


                                                                /* Phy cfg'd info.                                      */
    p_phy_data                   =  p_nor_data->PhyDataPtr;
    p_phy_data->UnitNbr          =  p_dev->UnitNbr;

    p_phy_data->AddrBase         =  p_nor_cfg->AddrBase;
    p_phy_data->RegionNbr        =  p_nor_cfg->RegionNbr;

    p_phy_data->BusWidth         =  p_nor_cfg->BusWidth;
    p_phy_data->BusWidthMax      =  p_nor_cfg->BusWidthMax;
    p_phy_data->PhyDevCnt        =  p_nor_cfg->PhyDevCnt;
    p_phy_data->MaxClkFreq       =  p_nor_cfg->MaxClkFreq;

    p_dev->DataPtr               = (void *)p_nor_data;



                                                                /* --------------------- INIT NOR --------------------- */
    p_nor_data->PhyPtr->Open(p_phy_data, p_err);                /* Open NOR.                                            */
    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_Open(): Could not open dev.\r\n"));
        return;
    }



                                                                /* ------------------- CALC NOR INFO ------------------ */
    FSDev_NOR_CalcDevInfo(p_nor_data, p_err);                   /* Calc dev info.                                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FSDev_NOR_AllocDevData(p_nor_data, p_err);                  /* Alloc dev data.                                      */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FS_TRACE_INFO(("NOR FLASH FOUND: Name       : \"nor:%d:\"\r\n",    p_phy_data->UnitNbr));
    FS_TRACE_INFO(("                 Sec Size   : %d bytes\r\n",       p_nor_data->SecSize));
    FS_TRACE_INFO(("                 Size       : %d secs\r\n",        p_nor_data->Size));
    FS_TRACE_INFO(("                 Rsvd       : %d%% (%d secs)\r\n", p_nor_data->PctRsvd, p_nor_data->BlkSecCnts * p_nor_data->BlkCntUsed - p_nor_data->Size));
    FS_TRACE_INFO(("                 Active blks: %d\r\n",             p_nor_data->AB_Cnt));



                                                                /* ---------------- LOW-LEVEL MOUNT DEV --------------- */
    FSDev_NOR_LowUnmountHandler(p_nor_data, p_err);
    FSDev_NOR_LowMountHandler(p_nor_data, p_err);

    if (*p_err != FS_ERR_NONE) {
        if (*p_err != FS_ERR_DEV_INVALID_LOW_FMT) {
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                          FSDev_NOR_Close()
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
*
*               (3) Since FSDev_NOR_Query() returns that the device is fixed, this function should NEVER
*                   be called.  However, since the pointer to the NOR information will be lost when the
*                   device structure is cleared, the invalid NOR information is placed into a linked list
*                   & may be freed later.
*********************************************************************************************************
*/

static  void  FSDev_NOR_Close (FS_DEV  *p_dev)
{
    FS_DEV_NOR_DATA  *p_nor_data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
        FS_TRACE_INFO(("FSDev_NOR_Close(): Cannot close NOR device due to null pointer"));
        return;
    }
#endif

    p_nor_data = (FS_DEV_NOR_DATA *)p_dev->DataPtr;
    FSDev_NOR_DataFree(p_nor_data);
}


/*
*********************************************************************************************************
*                                           FSDev_NOR_Rd()
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
*********************************************************************************************************
*/

static  void  FSDev_NOR_Rd (FS_DEV      *p_dev,
                            void        *p_dest,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    FS_DEV_NOR_DATA  *p_nor_data;
    FS_SEC_NBR        sec_nbr_logical;
    CPU_INT08U       *p_dest_08;
    FS_SEC_QTY        cnt_rem;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_dest == (void *)0) {                                  /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nor_data      = (FS_DEV_NOR_DATA *)p_dev->DataPtr;
    sec_nbr_logical =  start;
    p_dest_08       = (CPU_INT08U *)p_dest;
    cnt_rem         =  cnt;

    while (cnt_rem > 0u) {
        FSDev_NOR_RdSecLogical(p_nor_data,
                               p_dest_08,
                               sec_nbr_logical,
                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        cnt_rem--;
        sec_nbr_logical++;
        p_dest_08 += p_nor_data->SecSize;
    }

    FS_CTR_STAT_ADD(p_nor_data->StatRdCtr, (FS_CTR)cnt);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSDev_NOR_Wr()
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
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_Wr (FS_DEV      *p_dev,
                            void        *p_src,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    FS_DEV_NOR_DATA  *p_nor_data;
    FS_SEC_NBR        sec_nbr_logical;
    CPU_INT08U       *p_src_08;
    FS_SEC_QTY        cnt_rem;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_src == (void *)0) {                                   /* Validate src ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nor_data      = (FS_DEV_NOR_DATA *)p_dev->DataPtr;
    sec_nbr_logical =  start;
    p_src_08        = (CPU_INT08U *)p_src;
    cnt_rem         =  cnt;

    while (cnt_rem > 0u) {
        FSDev_NOR_WrSecLogical(p_nor_data,
                               p_src_08,
                               sec_nbr_logical,
                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        cnt_rem--;
        sec_nbr_logical++;
        p_src_08 += p_nor_data->SecSize;
    }

    FS_CTR_STAT_ADD(p_nor_data->StatWrCtr, (FS_CTR)cnt);
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_NOR_Query()
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

static  void  FSDev_NOR_Query (FS_DEV       *p_dev,
                               FS_DEV_INFO  *p_info,
                               FS_ERR       *p_err)
{
    FS_DEV_NOR_DATA  *p_nor_data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_info == (FS_DEV_INFO *)0) {                           /* Validate info ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nor_data      = (FS_DEV_NOR_DATA *)p_dev->DataPtr;

    p_info->SecSize =  p_nor_data->SecSize;
    p_info->Size    =  p_nor_data->Size;
    p_info->Fixed   =  DEF_YES;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_NOR_IO_Ctrl()
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
*                   (a) FS_DEV_IO_CTRL_REFRESH:          Refresh device.
*                   (b) FS_DEV_IO_CTRL_LOW_FMT           Low-level format device.      [**]
*                   (c) FS_DEV_IO_CTRL_LOW_MOUNT         Low-level mount device.       [**]
*                   (d) FS_DEV_IO_CTRL_LOW_UNMOUNT       Low-level unmount device.     [**]
*                   (e) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level compact device.     [**]
*                   (f) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level defragment device.  [**]
*                   (g) FS_DEV_IO_CTRL_SEC_RELEASE       Release data in sector.
*                   (h) FS_DEV_IO_CTRL_PHY_RD            Read  physical device.        [**]
*                   (i) FS_DEV_IO_CTRL_PHY_WR            Write physical device.        [**]
*                   (j) FS_DEV_IO_CTRL_PHY_RD_PAGE       Read  physical device page.   [*]
*                   (k) FS_DEV_IO_CTRL_PHY_WR_PAGE       Write physical device page.   [*]
*                   (l) FS_DEV_IO_CTRL_PHY_ERASE_BLK     Erase physical device block.  [**]
*                   (;) FS_DEV_IO_CTRL_PHY_ERASE_CHIP    Erase physical device.        [**]
*
*                           [*] NOT SUPPORTED
*                          [**] OCCUR VIA APPLICATION CALLS TO NOR DRIVER INTERFACE FUNCTIONS :
*
*                                   FSDev_NOR_LowFmt()
*                                   FSDev_NOR_LowMount()
*                                   FSDev_NOR_LowUnmount()
*                                   FSDev_NOR_LowCompact()
*                                   FSDev_NOR_LowDefrag()
*
*                                   FSDev_NOR_PhyRd()
*                                   FSDev_NOR_PhyWr()
*                                   FSDev_NOR_PhyEraseBlk()
*                                   FSDev_NOR_PhyEraseChip()
*********************************************************************************************************
*/

static  void  FSDev_NOR_IO_Ctrl (FS_DEV      *p_dev,
                                 CPU_INT08U   opt,
                                 void        *p_data,
                                 FS_ERR      *p_err)
{
    FS_DEV_NOR_IO_CTRL_DATA  *p_io_ctrl_data;
    FS_DEV_NOR_DATA          *p_nor_data;
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_SEC_NBR                sec_nbr_logical;
    FS_SEC_NBR                sec_nbr_phy;
#endif


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == (FS_DEV *)0) {                                 /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nor_data = (FS_DEV_NOR_DATA *)p_dev->DataPtr;

    switch (opt) {
        case FS_DEV_IO_CTRL_REFRESH:                            /* -------------------- REFRESH DEV ------------------- */
             if (p_nor_data->Mounted == DEF_YES) {
                *p_err = FS_ERR_NONE;
             } else {
                *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
             }
             break;



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_LOW_FMT:                            /* -------------------- LOW FMT DEV ------------------- */
             FSDev_NOR_LowFmtHandler(p_nor_data, p_err);
             break;
#endif


        case FS_DEV_IO_CTRL_LOW_MOUNT:                          /* ------------------- LOW MOUNT DEV ------------------ */
             FSDev_NOR_LowMountHandler(p_nor_data, p_err);
             break;



        case FS_DEV_IO_CTRL_LOW_UNMOUNT:                        /* ------------------ LOW UNMOUNT DEV ----------------- */
             FSDev_NOR_LowUnmountHandler(p_nor_data, p_err);
             break;



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_LOW_COMPACT:                        /* ------------------ LOW COMPACT DEV ----------------- */
             FSDev_NOR_LowCompactHandler(p_nor_data, p_err);
             break;



        case FS_DEV_IO_CTRL_LOW_DEFRAG:                         /* ------------------ LOW DEFRAG DEV ------------------ */
             FSDev_NOR_LowDefragHandler(p_nor_data, p_err);
             break;
#endif


#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_SEC_RELEASE:                        /* ------------------ RELEASE DEV SEC ----------------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif
             sec_nbr_logical = *(FS_SEC_NBR *)p_data;
             sec_nbr_phy     =   FSDev_NOR_L2P_GetEntry(p_nor_data,
                                                        sec_nbr_logical);

             if (sec_nbr_phy != FS_DEV_NOR_SEC_NBR_INVALID) {
                 FSDev_NOR_InvalidateSec(p_nor_data, sec_nbr_phy, p_err);
                 FSDev_NOR_L2P_SetEntry(p_nor_data,
                                        sec_nbr_logical,
                                        FS_DEV_NOR_SEC_NBR_INVALID);
                 FS_CTR_STAT_INC(p_nor_data->StatReleaseCtr);
             } else {
                *p_err = FS_ERR_NONE;
             }
             break;
#endif


        case FS_DEV_IO_CTRL_PHY_RD:                             /* -------------------- RD PHY DEV -------------------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif

             p_io_ctrl_data = (FS_DEV_NOR_IO_CTRL_DATA *)p_data;
             FSDev_NOR_PhyRdHandler(p_nor_data,                 /* Rd dev.                                              */
                                    p_io_ctrl_data->DataPtr,
                                    p_io_ctrl_data->Start,
                                    p_io_ctrl_data->Size,
                                    p_err);
             break;



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_PHY_WR:                             /* -------------------- WR PHY DEV -------------------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif

             p_io_ctrl_data = (FS_DEV_NOR_IO_CTRL_DATA *)p_data;
             FSDev_NOR_PhyWrHandler(p_nor_data,                 /* Wr dev.                                              */
                                    p_io_ctrl_data->DataPtr,
                                    p_io_ctrl_data->Start,
                                    p_io_ctrl_data->Size,
                                    p_err);
             break;
#endif



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_PHY_ERASE_BLK:                      /* ----------------- ERASE PHY DEV BLK ---------------- */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
             if (p_data == (void *)0) {                         /* Validate data ptr.                                   */
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
#endif

             p_io_ctrl_data = (FS_DEV_NOR_IO_CTRL_DATA *)p_data;
             FSDev_NOR_PhyEraseBlkHandler(p_nor_data,           /* Erase dev blk.                                       */
                                          p_io_ctrl_data->Start,
                                          p_io_ctrl_data->Size,
                                          p_err);
             break;
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_DEV_IO_CTRL_PHY_ERASE_CHIP:
             FSDev_NOR_PhyEraseChipHandler(p_nor_data,          /* Erase chip.                                          */
                                           p_err);
             break;
#endif

        case FS_DEV_IO_CTRL_PHY_RD_PAGE:                        /* --------------- UNSUPPORTED I/O CTRL --------------- */
        case FS_DEV_IO_CTRL_PHY_WR_PAGE:
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
*                                      FSDev_NOR_LowFmtHandler()
*
* Description : Low-level format NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Device formatted/mounted.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device formatted, but could NOT be mounted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) The NOR device is marked unmounted when formatting commences; it will not be marked
*                   mounted until the low-level mount completes.
*
*               (2) #### Preserve erase counts from previous format by reading block headers.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_LowFmtHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                       FS_ERR           *p_err)
{
    CPU_INT08U  blk_hdr[FS_DEV_NOR_BLK_HDR_LEN];
    FS_SEC_NBR  blk_ix;
    CPU_INT32U  blk_addr;


    if (p_nor_data->Mounted == DEF_YES) {
        p_nor_data->Mounted =  DEF_NO;                          /* See Note #1.                                         */
        FSDev_NOR_LowUnmountHandler(p_nor_data, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }


    Mem_Clr((void *)&blk_hdr[0], FS_DEV_NOR_BLK_HDR_LEN);
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK1],     FS_DEV_NOR_BLK_HDR_MARK_WORD_1);
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK2],     FS_DEV_NOR_BLK_HDR_MARK_WORD_2);
                                                                /* See Note #2.                                         */
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT], 0u);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_VER],       FS_DEV_NOR_BLK_HDR_VER);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_SEC_SIZE],  p_nor_data->SecSize);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_BLK_CNT],   p_nor_data->BlkCntUsed);

    blk_ix = 0u;
    while (blk_ix < p_nor_data->BlkCntUsed) {
        blk_addr = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);


                                                                /* --------------------- ERASE BLK -------------------- */
        FSDev_NOR_PhyEraseBlkHandler(p_nor_data,
                                     blk_addr,
                                     p_nor_data->PhyDataPtr->BlkSize,
                                     p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FSDev_NOR_LowFmtHandler(): Failed to erase blk %d (0x%08X).\r\n", p_nor_data->BlkNbrFirst + blk_ix, blk_addr));
            return;
        }

        FS_TRACE_LOG(("FSDev_NOR_LowFmtHandler(): Erased blk % 4d (0x%08X).\r\n", p_nor_data->BlkNbrFirst + blk_ix, blk_addr));



                                                                /* -------------------- WR BLK HDR -------------------- */
        FSDev_NOR_PhyWrHandler( p_nor_data,
                               &blk_hdr[0],
                                blk_addr,
                                FS_DEV_NOR_BLK_HDR_LEN,
                                p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FSDev_NOR_LowFmtHandler(): Failed to wr blk hdr %d (0x%08X).\r\n", p_nor_data->BlkNbrFirst + blk_ix, blk_addr));
            return;
        }

        blk_ix++;
    }


                                                                /* ---------------- UPDATE INFO & MOUNT --------------- */
    FS_TRACE_DBG(("FSDev_NOR_LowFmtHandler(): Low-level fmt'ing complete.\r\n"));

    FSDev_NOR_LowMountHandler(p_nor_data, p_err);               /* Mount dev (see Note #1).                             */
}
#endif


/*
*********************************************************************************************************
*                                     FSDev_NOR_LowMountHandler()
*
* Description : Low-level mount NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Device mounted.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDev_NOR_LowFmtHandler()  Note #1'.
*
*               (2) A block may be invalid upon mount if the erase process was interrupted by power loss
*                   after the block was erased, but before the block header was written.
*
*                   (a) If more than one block is invalid, then the device is (probably) NOT formatted
*                       OR it has been corrupted.
*
*               (3) At most one invalid block should be found (see also Note #2).  This block MUST be
*                   erased & a correct header written prior to other operations which might created
*                   invalid blocks.
*
*               (4) Two physical sectors may be assigned the same logical sector upon mount if the write
*                   process was interrupted after writing the 'new' sector data & header, but before
*                   invalidating the old sector.  It is impossible to determine which is the 'old' data;
*                   the first physical sector discovered is kept.  Any subsequent sectors will be marked
*                   invalid.
*
*               (5) There may be no erased blocks upon mount if a block move process was interrupted
*                   before copying all sectors & erasing & re-marking the emptied block.  It will be
*                   possible to resume that move, since the number of erased sectors in the active block
*                   (i.e., the block to which data was being moved) will be less than the number of valid
*                   sectors in the source block.  Additional writes may disturb this, so a block MUST be
*                   erased here.  See also 'FSDev_NOR_PhyEraseBlkHandler()'.
*
*                   (a) The block selected SHOULD be the block with the fewest valid sectors.  The min/
*                       max erase counts are assigned after block erasure to ensure that wear-leveling
*                       concerns are not factored into block selection.
*
*                   (b) The block selected MUST NOT be the active block.  The 'valid' sectors of the
*                       active block are the valid written sectors plus the erased sectors.  If this were
*                       less than the valid sector count of any other block, the move would be impossible.
*
*                   (c) The active block MUST have more erased sectors than valid sectors in the selected
*                       block.
*
*                   (d) The global maximum erase count may have been updated during the block erasure
*                       operation: .EraseCntMax can be updated within FSDev_NOR_EraseBlkPrepare(). In
*                       that case, the check against the local variable ensures that the global maximum
*                       erase count has the proper value after the block erasure.
*
*               (6) There may be multiple blocks found upon mount with valid or invalid sectors AND
*                   erased sectors, if the active block was selected for erasure, but the block move
*                   was interrupted before copying all sectors.  Since only one block will be selected as
*                   the active block, the erased sectors at the end of any other block MUST be classified
*                   as invalid.
*********************************************************************************************************
*/

static  void  FSDev_NOR_LowMountHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                         FS_ERR           *p_err)
{
    CPU_INT08U   blk_hdr[FS_DEV_NOR_BLK_HDR_LEN];
    CPU_INT08U   sec_hdr[FS_DEV_NOR_SEC_HDR_LEN];
    CPU_BOOLEAN  active;
    CPU_INT32U   blk_addr;
    FS_SEC_QTY   blk_cnt;
    FS_SEC_QTY   blk_cnt_erased;
    FS_SEC_QTY   blk_cnt_valid;
    FS_SEC_NBR   blk_ix;
    FS_SEC_NBR   blk_ix_invalid;
    CPU_BOOLEAN  erased;
    CPU_INT32U   erase_cnt;
    CPU_INT32U   erase_cnt_min;
    CPU_INT32U   erase_cnt_max;
    CPU_INT32U   mark1;
    CPU_INT32U   mark2;
    CPU_INT16U   ver;
    FS_SEC_QTY   sec_cnt_erased;
    FS_SEC_QTY   sec_cnt_invalid;
    FS_SEC_QTY   sec_cnt_valid;
    FS_SEC_NBR   sec_ix;
    FS_SEC_NBR   sec_ix_next;
    CPU_INT32U   sec_nbr_logical;
    FS_SEC_NBR   sec_nbr_phy;
    FS_SEC_NBR   sec_nbr_phy_old;
    FS_SEC_SIZE  sec_size;
    CPU_BOOLEAN  sec_valid;
    CPU_INT32U   status;


    if (p_nor_data->Mounted == DEF_YES) {
        p_nor_data->Mounted =  DEF_NO;                          /* See Note #1.                                         */
        FSDev_NOR_LowUnmountHandler(p_nor_data, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }


                                                                /* ------------------ VERIFY BLK HDRS ----------------- */
    blk_ix         = 0u;
    blk_ix_invalid = FS_DEV_NOR_SEC_NBR_INVALID;
    blk_cnt_valid  = 0u;
    erase_cnt_min  = DEF_INT_32U_MAX_VAL;
    erase_cnt_max  = 0u;
    while (blk_ix < p_nor_data->BlkCntUsed) {
                                                                /* Rd blk hdr.                                          */
        FSDev_NOR_RdBlkHdr(p_nor_data, &blk_hdr[0], blk_ix, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

                                                                /* Chk marks & ver.                                     */
        mark1     =              MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK1]);
        mark2     =              MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK2]);
        erase_cnt =              MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT]);
        ver       =              MEM_VAL_GET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_VER]);
        sec_size  = (FS_SEC_SIZE)MEM_VAL_GET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_SEC_SIZE]);
        blk_cnt   = (FS_SEC_QTY )MEM_VAL_GET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_BLK_CNT]);
        if ((mark1    == FS_DEV_NOR_BLK_HDR_MARK_WORD_1) &&
            (mark2    == FS_DEV_NOR_BLK_HDR_MARK_WORD_2) &&
            (ver      == FS_DEV_NOR_BLK_HDR_VER)         &&
            (sec_size == p_nor_data->SecSize)            &&
            (blk_cnt  == p_nor_data->BlkCntUsed)) {
            blk_cnt_valid++;

            if (erase_cnt_min > erase_cnt) {
                erase_cnt_min = erase_cnt;
            }
            if (erase_cnt_max < erase_cnt) {
                erase_cnt_max = erase_cnt;
            }

        } else {                                                /* Blk is NOT valid (see Note #2).                      */
            blk_ix_invalid = blk_ix;
        }

        blk_ix++;
    }

    if (blk_cnt_valid < p_nor_data->BlkCntUsed - 1u) {          /* If more than 1 invalid blk (see Note #2a) ...        */
        FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Low-level format invalid: %d invalid blks found.\r\n", p_nor_data->BlkCntUsed - blk_cnt_valid));
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;                     /* ... low fmt invalid.                                 */
        return;
    }



                                                                /* ----------------- CONSTRUCT L2P TBL ---------------- */
    p_nor_data->SecCntValid   = 0u;                             /* Clr sec cnts.                                        */
    p_nor_data->SecCntErased  = 0u;
    p_nor_data->SecCntInvalid = 0u;

    blk_ix         = 0u;
    sec_nbr_phy    = 0u;
    blk_cnt_valid  = 0u;
    blk_cnt_erased = 0u;

    while (blk_ix < p_nor_data->BlkCntUsed) {
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
        FSDev_NOR_RdBlkHdr(p_nor_data, &blk_hdr[0], blk_ix, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
        erase_cnt = MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT]);
#endif


        if (blk_ix == blk_ix_invalid) {                         /* If blk invalid, all secs invalid & should be erased. */
            p_nor_data->SecCntInvalid += p_nor_data->BlkSecCnts;
            sec_nbr_phy               += p_nor_data->BlkSecCnts;
            FS_TRACE_LOG(("FSDev_NOR_LowMountHandler(): Invalid blk found %d.\r\n", p_nor_data->BlkNbrFirst + blk_ix));
            sec_cnt_valid = 0u;

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
            blk_addr = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);

            Mem_Clr(&blk_hdr[0], FS_DEV_NOR_BLK_HDR_LEN);
            MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK1],     FS_DEV_NOR_BLK_HDR_MARK_WORD_1);
            MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK2],     FS_DEV_NOR_BLK_HDR_MARK_WORD_2);
            MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT], erase_cnt_max);
            MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_VER],       FS_DEV_NOR_BLK_HDR_VER);
            MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_SEC_SIZE],  p_nor_data->SecSize);
            MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_BLK_CNT],   p_nor_data->BlkCntUsed);

            FSDev_NOR_PhyEraseBlkHandler(p_nor_data,            /* Erase blk ...                                        */
                                         blk_addr,
                                         p_nor_data->PhyDataPtr->BlkSize,
                                         p_err);

            FSDev_NOR_PhyWrHandler( p_nor_data,                 /* ... & wr hdr (see Note #3).                          */
                                   &blk_hdr[0],
                                    blk_addr,
                                    FS_DEV_NOR_BLK_HDR_LEN,
                                    p_err);

            erased = DEF_YES;
#else
            erased = DEF_NO;
#endif


        } else {
            blk_addr        = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);
            blk_addr       += FS_DEV_NOR_BLK_HDR_LEN;

                                                                /* Chk hdr of each sec.                                 */
            sec_ix          = 0u;
            sec_ix_next     = 0u;
            sec_valid       = DEF_NO;
            sec_cnt_erased  = 0u;
            sec_cnt_valid   = 0u;
            sec_cnt_invalid = 0u;
            while (sec_ix < p_nor_data->BlkSecCnts) {
                FSDev_NOR_PhyRdHandler( p_nor_data,
                                       &sec_hdr[0],
                                        blk_addr,
                                        FS_DEV_NOR_SEC_HDR_LEN,
                                        p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }
                sec_nbr_logical = MEM_VAL_GET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_SEC_NBR]);
                status          = MEM_VAL_GET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS]);

                switch (status) {
                    case FS_DEV_NOR_STATUS_SEC_ERASED:
                         sec_cnt_erased++;
                         break;

                    case FS_DEV_NOR_STATUS_SEC_VALID:
                         if (sec_nbr_logical < p_nor_data->Size) {
                             sec_nbr_phy_old = FSDev_NOR_L2P_GetEntry(p_nor_data,
                                                                      sec_nbr_logical);
                             if (sec_nbr_phy_old == FS_DEV_NOR_SEC_NBR_INVALID) {
                                 FSDev_NOR_L2P_SetEntry(p_nor_data,
                                                        sec_nbr_logical,
                                                        sec_nbr_phy);
                                 sec_cnt_valid++;
                                 sec_valid = DEF_YES;           /* Blk has valid data.                                  */


                             } else {                           /* Duplicate entry (see Note #4).                       */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                 MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS], FS_DEV_NOR_STATUS_SEC_INVALID);
                                                                /* Mark sec as 'invalid'.                               */
                                 FSDev_NOR_PhyWrHandler( p_nor_data,
                                                        &sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],
                                                         blk_addr + FS_DEV_NOR_SEC_HDR_OFFSET_STATUS,
                                                         4u,
                                                         p_err);
#endif

                                 FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Logical sec %d already assigned to phy sec %d before %d.\r\n", sec_nbr_logical, sec_nbr_phy_old, sec_nbr_phy));
                                 sec_cnt_invalid++;
                             }

                         } else {
                             sec_cnt_invalid++;
                         }
                         sec_cnt_invalid += sec_cnt_erased;     /* No erased secs precede valid secs.                   */
                         sec_cnt_erased   = 0u;
                         sec_ix_next      = sec_ix + 1u;        /* Set next potentially erased sec.                     */
                         break;

                    case FS_DEV_NOR_STATUS_SEC_WRITING:         /* No erased secs precede invalid secs.                 */
                    case FS_DEV_NOR_STATUS_SEC_INVALID:
                    default:
                         sec_cnt_invalid += sec_cnt_erased + 1u;
                         sec_cnt_erased   = 0u;
                         sec_ix_next      = sec_ix + 1u;        /* Set next potentially erased sec.                     */
                         break;
                }

                blk_addr += p_nor_data->SecSize + FS_DEV_NOR_SEC_HDR_LEN;
                sec_ix++;
                sec_nbr_phy++;
            }

            if ((sec_ix_next == p_nor_data->BlkSecCnts) &&      /* If final sec is invalid     ...                      */
                (sec_valid   == DEF_NO)) {                      /* ... & blk has no valid data ...                      */
                                                                /* ... add blk to erase q.                              */
                erased = DEF_NO;
                FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Valid blk found % 4d w/ erase cnt % 4d; blk should be erased.\r\n", p_nor_data->BlkNbrFirst + blk_ix, erase_cnt));


            } else if (sec_ix_next == 0u) {
                erased = DEF_YES;
                blk_cnt_erased++;
                FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Valid blk found % 4d w/ erase cnt % 4d; blk is erased.\r\n", p_nor_data->BlkNbrFirst + blk_ix, erase_cnt));


            } else {
                erased = DEF_NO;

                if (sec_cnt_erased > 0u) {
                    active = FSDev_NOR_AB_Add(p_nor_data,       /* Try to add to list of active blks ...                */
                                              blk_ix,
                                              sec_ix_next);
                    if (active != DEF_OK) {                     /* ... if it could NOT be added      ...                */
                        FS_TRACE_LOG(("FSDev_NOR_LowMountHandler(): Could not make blk %d active blk; reclassifying %d erased secs as invalid.\r\n", blk_ix, sec_cnt_erased));
                        sec_cnt_invalid += sec_cnt_erased;      /* ... reclassify secs as invalid.                      */
                        sec_cnt_erased   = 0u;
                    }
                }

                if ((sec_cnt_erased != 0u) || (sec_cnt_valid != 0u)) {
                    blk_cnt_valid++;
                    FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Valid blk found % 4d w/ erase cnt % 4d; blk has % 4d valid, % 4d erased secs.\r\n", p_nor_data->BlkNbrFirst + blk_ix, erase_cnt, sec_cnt_valid, (sec_ix_next == FS_DEV_NOR_SEC_NBR_INVALID) ? 0u : (p_nor_data->BlkSecCnts - sec_ix_next)));

                }
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
                  else {
                    FS_TRACE_DBG(("FSDev_NOR_LowMountHandler(): Valid blk found % 4d w/ erase cnt % 4d; blk should be erased.\r\n", p_nor_data->BlkNbrFirst + blk_ix, erase_cnt));
                }
#endif
            }

            p_nor_data->SecCntErased  += sec_cnt_erased;
            p_nor_data->SecCntValid   += sec_cnt_valid;
            p_nor_data->SecCntInvalid += sec_cnt_invalid;
        }

        FSDev_NOR_SetBlkErased(p_nor_data,                      /* Update erase map.                                    */
                               blk_ix,
                               erased);

        FSDev_NOR_SetBlkSecCntValid(p_nor_data,                 /* Update sec cnt valid tbl.                            */
                                    blk_ix,
                                    sec_cnt_valid);

        blk_ix++;
    }



                                                                /* -------------------- UPDATE INFO ------------------- */
    p_nor_data->BlkWearLevelAvail =  DEF_NO;

    p_nor_data->BlkCntValid       =  blk_cnt_valid;
    p_nor_data->BlkCntErased      =  blk_cnt_erased;
    p_nor_data->BlkCntInvalid     = (p_nor_data->BlkCntUsed - blk_cnt_valid) - blk_cnt_erased;

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if ((p_nor_data->BlkCntErased  == 0u) &&                    /* If no erased blks (see Note #5) ...                  */
        (p_nor_data->BlkCntInvalid == 0u)) {
        blk_ix = FSDev_NOR_FindEraseBlk(p_nor_data);

        if (blk_ix != FS_DEV_NOR_SEC_NBR_INVALID) {
            active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
            if (active == DEF_YES) {                            /* See Note #5b.                                        */
                FS_TRACE_INFO(("FSDev_NOR_LowMountHandler(): Cannot mount dev; no erase blk, active blk has fewest valid secs."));
               *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
                return;
            }

            FSDev_NOR_GetBlkInfo( p_nor_data,
                                  blk_ix,
                                 &erased,
                                 &sec_cnt_valid);
                                                                /* See Note #5c.                                        */
            sec_cnt_erased = FSDev_NOR_AB_SecCntTotErased(p_nor_data);
            if (sec_cnt_valid > sec_cnt_erased) {
                FS_TRACE_INFO(("FSDev_NOR_LowMountHandler(): Cannot mount dev; no erase blk, active blk has too few erased secs (%d < %d).", sec_cnt_erased, sec_cnt_valid));
               *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
                return;
            }

            FSDev_NOR_EraseBlkPrepare(p_nor_data,               /* ... erase 1 blk.                                     */
                                      blk_ix,
                                      p_err);
        }
    }
#endif

    p_nor_data->EraseCntMin = erase_cnt_min;                    /* See Note #5a.                                        */
    if (erase_cnt_max > p_nor_data->EraseCntMax) {              /* See Note #5d.                                         */
        p_nor_data->EraseCntMax = erase_cnt_max;
    }

    p_nor_data->Mounted     = DEF_YES;                          /* See Note #1.                                         */

    FS_TRACE_INFO(("NOR FLASH MOUNT: Name         : \"nor:%d:\"\r\n", p_nor_data->PhyDataPtr->UnitNbr));
    FS_TRACE_INFO(("                 Blks valid   : %d\r\n",          p_nor_data->BlkCntValid));
    FS_TRACE_INFO(("                      erased  : %d\r\n",          p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                      erase q : %d\r\n",         (p_nor_data->BlkCntUsed - p_nor_data->BlkCntValid) - p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                 Secs valid   : %d\r\n",          p_nor_data->SecCntValid));
    FS_TRACE_INFO(("                      erased  : %d\r\n",          p_nor_data->SecCntErased));
    FS_TRACE_INFO(("                      invalid : %d\r\n",          p_nor_data->SecCntInvalid));
    FS_TRACE_INFO(("                 Erase cnt min: %d\r\n",          p_nor_data->EraseCntMin));
    FS_TRACE_INFO(("                 Erase cnt max: %d\r\n",          p_nor_data->EraseCntMax));

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    FSDev_NOR_LowUnmountHandler()
*
* Description : Low-level unmount NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Device unmounted.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_LowUnmountHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                           FS_ERR           *p_err)
{
    FS_SEC_NBR  blk_ix;
    FS_SEC_NBR  sec_nbr_logical;


                                                                /* ------------------- CLR BLK INFO ------------------- */
    blk_ix = 0u;
    while (blk_ix < p_nor_data->BlkCntUsed) {
        FSDev_NOR_SetBlkErased(p_nor_data,                      /* Set each entry as NOT erased.                        */
                               blk_ix,
                               DEF_NO);

        FSDev_NOR_SetBlkSecCntValid(p_nor_data,                 /* Set each sec cnt invalid.                            */
                                    blk_ix,
                                    FS_DEV_NOR_SEC_NBR_INVALID);

        blk_ix++;
    }



                                                                /* -------------------- CLR L2P TBL ------------------- */
    sec_nbr_logical = 0u;
    while (sec_nbr_logical < p_nor_data->Size) {                /* Clr L2P tbl.                                         */
        FSDev_NOR_L2P_SetEntry(p_nor_data,
                               sec_nbr_logical,
                               FS_DEV_NOR_SEC_NBR_INVALID);
        sec_nbr_logical++;
    }



                                                                /* ---------------------- CLR ABs --------------------- */
    FSDev_NOR_AB_ClrAll(p_nor_data);



                                                                /* ------------------- CLR NOR INFO ------------------- */
    p_nor_data->BlkWearLevelAvail = DEF_NO;
    p_nor_data->BlkCntValid       = 0u;
    p_nor_data->BlkCntErased      = 0u;
    p_nor_data->BlkCntInvalid     = 0u;

    p_nor_data->SecCntValid       = 0u;
    p_nor_data->SecCntErased      = 0u;
    p_nor_data->SecCntInvalid     = 0u;

    p_nor_data->EraseCntMin       = 0u;
    p_nor_data->EraseCntMax       = 0u;

    p_nor_data->Mounted           = DEF_NO;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    FSDev_NOR_LowCompactHandler()
*
* Description : Low-level compact NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Device compacted.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_LowCompactHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                           FS_ERR           *p_err)
{
    FS_SEC_NBR   blk_ix;
    CPU_BOOLEAN  erased;
    FS_SEC_QTY   sec_cnt_valid;
    CPU_BOOLEAN  active;


    if (p_nor_data->Mounted == DEF_NO) {
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
        return;
    }

    FS_TRACE_INFO(("NOR FLASH COMPACT: BEFORE: Blks valid   : %d\r\n",  p_nor_data->BlkCntValid));
    FS_TRACE_INFO(("                                erased  : %d\r\n",  p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                                erase q : %d\r\n", (p_nor_data->BlkCntUsed - p_nor_data->BlkCntValid) - p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                           Secs valid   : %d\r\n",  p_nor_data->SecCntValid));
    FS_TRACE_INFO(("                                erased  : %d\r\n",  p_nor_data->SecCntErased));
    FS_TRACE_INFO(("                                invalid : %d\r\n",  p_nor_data->SecCntInvalid));

    blk_ix = 0u;
    while (blk_ix <  p_nor_data->BlkCntUsed) {
        FSDev_NOR_GetBlkInfo( p_nor_data,                       /* Get blk info.                                        */
                              blk_ix,
                             &erased,
                             &sec_cnt_valid);

        if (erased == DEF_NO) {                                 /* If blk is NOT erased      ...                        */
            if (sec_cnt_valid != p_nor_data->BlkSecCnts) {      /* ... & blk had invalid secs ...                       */
                                                                /* ... move blk data.                                   */
                active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
                if (active == DEF_NO) {
                    FSDev_NOR_EraseBlkPrepare(p_nor_data, blk_ix, p_err);
                    if (*p_err != FS_ERR_NONE) {
                        return;
                    }
                }
            }
        }

        blk_ix++;
    }

    FS_TRACE_INFO(("                   AFTER : Blks valid   : %d\r\n",  p_nor_data->BlkCntValid));
    FS_TRACE_INFO(("                                erased  : %d\r\n",  p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                                erase q : %d\r\n", (p_nor_data->BlkCntUsed - p_nor_data->BlkCntValid) - p_nor_data->BlkCntErased));
    FS_TRACE_INFO(("                           Secs valid   : %d\r\n",  p_nor_data->SecCntValid));
    FS_TRACE_INFO(("                                erased  : %d\r\n",  p_nor_data->SecCntErased));
    FS_TRACE_INFO(("                                invalid : %d\r\n",  p_nor_data->SecCntInvalid));

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                    FSDev_NOR_LowDefragHandler()
*
* Description : Low-level defragment NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Device defragmented.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_LowDefragHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                          FS_ERR           *p_err)
{
    FS_SEC_NBR  sec_nbr_logical;
    FS_SEC_NBR  sec_nbr_phy;


    if (p_nor_data->Mounted == DEF_NO) {
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
        return;
    }

    FSDev_NOR_LowCompactHandler(p_nor_data, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FSDev_NOR_AB_RemoveAll(p_nor_data);                         /* Remove all active blks.                              */

    sec_nbr_logical = 0u;
    while (sec_nbr_logical < p_nor_data->Size) {
        sec_nbr_phy = FSDev_NOR_L2P_GetEntry(p_nor_data, sec_nbr_logical);
        if (sec_nbr_phy != FS_DEV_NOR_SEC_NBR_INVALID) {
            FSDev_NOR_RdSecLogical(p_nor_data,                  /* Rd sec.                                              */
                                   p_nor_data->BufPtr,
                                   sec_nbr_logical,
                                   p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }

            FSDev_NOR_WrSecLogical(p_nor_data,                  /* Wr sec to new location.                              */
                                   p_nor_data->BufPtr,
                                   sec_nbr_logical,
                                   p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }
        sec_nbr_logical++;
    }

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_NOR_PhyRdHandler()
*
* Description : Read data octets from NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               start       Start address of read (relative to start of device).
*
*               cnt         Number of octets to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Octets read successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PhyRdHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                      void             *p_dest,
                                      CPU_INT32U        start,
                                      CPU_INT32U        cnt,
                                      FS_ERR           *p_err)
{
                                                                /* ---------------------- RD DATA --------------------- */
    p_nor_data->PhyPtr->Rd(p_nor_data->PhyDataPtr,
                           p_dest,
                           start,
                           cnt,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_CTR_ERR_INC(p_nor_data->ErrRdCtr);
        return;
    }

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* Update rd ctrs.                                      */
    if (cnt == p_nor_data->SecSize) {
        FS_CTR_STAT_INC(p_nor_data->StatRdCtr);
    } else {
        FS_CTR_STAT_ADD(p_nor_data->StatRdOctetCtr, cnt);
    }
#endif
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PhyWrHandler()
*
* Description : Write data octets to NOR.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               start       Start address of write (relative to start of device).
*
*               cnt         Number of octets to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Octets written successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) A program operation can only change a '1' bit to a '0' bit.  Other program operations
*                   may cause low-level driver errors, or the program operation may fail.  A write
*                   that attempts to change a '0' bit to a '1' bit indicates improper driver function.
*
*               (2) The success of high-level write operations can be checked by volume write check
*                   functionality.  The check here covers write operations for low-level data (e.g.,
*                   block & sector headers).
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_PhyWrHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                      void             *p_src,
                                      CPU_INT32U        start,
                                      CPU_INT32U        cnt,
                                      FS_ERR           *p_err)
{
#if (FS_DEV_NOR_CFG_WR_CHK_EN == DEF_ENABLED)
    CPU_INT08U   data_rd[2];
    CPU_INT08U   data_wr_01;
    CPU_INT08U   data_wr_02;
    CPU_INT08U  *p_src_08;
    CPU_INT32U   addr;
    CPU_INT32U   cnt_rem;
#endif


#if (FS_DEV_NOR_CFG_WR_CHK_EN == DEF_ENABLED)                   /* ------------------- CHK PGM VALID ------------------ */
    p_src_08 = (CPU_INT08U *)p_src;
    addr     =  start;
    cnt_rem  =  cnt;
    while (cnt_rem >= 2u) {
        FSDev_NOR_PhyRdHandler( p_nor_data,                     /* Rd cur data.                                         */
                               &data_rd[0],
                                addr,
                                2u,
                                p_err);

        data_wr_01 = *p_src_08++;
        data_wr_02 = *p_src_08++;
                                                                /* Chk bits (see Note #1).                              */
        if ((((CPU_INT08U)(data_wr_01 ^ data_rd[0]) & (CPU_INT08U)~data_rd[0]) != 0x00u) ||
            (((CPU_INT08U)(data_wr_02 ^ data_rd[1]) & (CPU_INT08U)~data_rd[1]) != 0x00u)) {
            FS_CTR_ERR_INC(p_nor_data->ErrWrFailCtr);
            FS_TRACE_DBG(("FSDev_NOR_PhyWrHandler(): Trying to write 0x%02X%02X over 0x%02X%02X.\r\n", data_wr_01, data_wr_02, data_rd[0], data_rd[1]));
           *p_err = FS_ERR_DEV_IO;
            return;
        }

        addr    += 2u;
        cnt_rem -= 2u;
    }
#endif


                                                                /* ---------------------- WR DATA --------------------- */
    p_nor_data->PhyPtr->Wr(p_nor_data->PhyDataPtr,
                           p_src,
                           start,
                           cnt,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_CTR_ERR_INC(p_nor_data->ErrWrCtr);
        return;
    }

#if (FS_DEV_NOR_CFG_WR_CHK_EN == DEF_ENABLED)                   /* ------------------ CHK PGM SUCCESS ----------------- */
    p_src_08 = (CPU_INT08U *)p_src;
    addr     =  start;
    cnt_rem  =  cnt;
    while (cnt_rem >= 2u) {
        FSDev_NOR_PhyRdHandler( p_nor_data,                     /* Rd new data.                                         */
                               &data_rd[0],
                                addr,
                                2u,
                                p_err);

        data_wr_01 = *p_src_08++;
        data_wr_02 = *p_src_08++;

        if ((data_wr_01 != data_rd[0]) ||                       /* Cmp to intended data.                                */
            (data_wr_02 != data_rd[1])) {
            FS_CTR_ERR_INC(p_nor_data->ErrWrFailCtr);
            FS_TRACE_DBG(("FSDev_NOR_PhyWrHandler(): Data compare failed 0x%02X%02X != 0x%02X%02X.\r\n", data_wr_01, data_wr_02, data_rd[0], data_rd[1]));
           *p_err = FS_ERR_DEV_IO;
            return;
        }

        addr    += 2u;
        cnt_rem -= 2u;
    }
#endif

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* Update wr ctrs.                                      */
    if (cnt == p_nor_data->SecSize) {
        FS_CTR_STAT_INC(p_nor_data->StatWrCtr);
    } else {
        FS_CTR_STAT_ADD(p_nor_data->StatWrOctetCtr, cnt);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                   FSDev_NOR_PhyEraseBlkHandler()
*
* Description : Erase block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               start       Start address of block (relative to start of device).
*
*               size        Size of block, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Block erased successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) #### Read back block data to check erase success.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_PhyEraseBlkHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                            CPU_INT32U        start,
                                            CPU_INT32U        size,
                                            FS_ERR           *p_err)
{
#if (FS_DEV_NOR_CFG_WR_CHK_EN == DEF_ENABLED)
    CPU_INT32U  addr;
    CPU_INT32U  cnt_rem;
    CPU_INT08U  data_rd[16];
    CPU_INT32U  i;
#endif


    p_nor_data->PhyPtr->EraseBlk(p_nor_data->PhyDataPtr,
                                 start,
                                 size,
                                 p_err);

    if (*p_err != FS_ERR_NONE) {
         FS_CTR_ERR_INC(p_nor_data->ErrEraseCtr);
         return;
    }

#if (FS_DEV_NOR_CFG_WR_CHK_EN == DEF_ENABLED)
    addr    = start;
    cnt_rem = size;
    while (cnt_rem >= sizeof(data_rd)) {
        FSDev_NOR_PhyRdHandler( p_nor_data,                     /* Rd new data.                                         */
                               &data_rd[0],
                                addr,
                                sizeof(data_rd),
                                p_err);

        for (i = 0; i < sizeof(data_rd); i++) {
            if (data_rd[i] != 0xFFu) {                          /* Cmp to erased data.                                  */
                FS_CTR_ERR_INC(p_nor_data->ErrEraseFailCtr);
                FS_TRACE_DBG(("FSDev_NOR_PhyEraseBlkHandler(): Data compare failed 0x02X != 0x%02X at 0x%08X.\r\n", data_rd[i], 0xFFu, addr + i));
               *p_err = FS_ERR_DEV_IO;
                return;
            }
        }

        addr    += sizeof(data_rd);
        cnt_rem -= sizeof(data_rd);
    }
#endif

    FS_CTR_STAT_INC(p_nor_data->StatEraseBlkCtr);
}
#endif


/*
*********************************************************************************************************
*                                   FSDev_NOR_PhyEraseChipHandler()
*
* Description : Erase chip.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Block erased successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_PhyEraseChipHandler(FS_DEV_NOR_DATA  *p_nor_data,
                                            FS_ERR           *p_err)
{
    p_nor_data->PhyPtr->IO_Ctrl(p_nor_data->PhyDataPtr,
                                FS_DEV_IO_CTRL_PHY_ERASE_CHIP,
                                DEF_NULL,
                                p_err);
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_NOR_CalcDevInfo()
*
* Description : Calculate NOR device info.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                      Device info calculated.
*                               FS_ERR_DEV_INVALID_CFG           Device configuration specified invalid.
*                               FS_ERR_DEV_INVALID_LOW_PARAMS    Device low-level parameters invalid.
*                               FS_ERR_DEV_IO                    Device I/O error.
*                               FS_ERR_DEV_TIMEOUT               Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Block & block region information supplied by the phy driver are strictly checked :
*                   (a) As least one block must exist.
*                   (b) All blocks must be large enough for at least one data sector.
*                   (c) The configured data region start must lie within the block region.
*
*               (2) At least one block's worth of sectors MUST be reserved.
*********************************************************************************************************
*/

static  void  FSDev_NOR_CalcDevInfo (FS_DEV_NOR_DATA  *p_nor_data,
                                     FS_ERR           *p_err)
{
    FS_SEC_QTY   ab_cnt;
    CPU_ADDR     blk_addr;
    FS_SEC_QTY   blk_cnt;
    FS_SEC_NBR   blk_nbr;
    FS_SEC_SIZE  data_size_blk;
    FS_SEC_QTY   sec_cnt_blk;
    FS_SEC_QTY   sec_cnt_tot;
    FS_SEC_QTY   sec_cnt_rsvd;
    FS_SEC_QTY   sec_cnt_data;


                                                                /* ------------ CHK PHY INFO (see Note #1) ------------ */
    if (p_nor_data->PhyDataPtr->BlkCnt == 0u) {                 /* Validate blk cnt.                                    */
        FS_TRACE_DBG(("FSDev_NOR_CalcDevInfo(): Phy info invalid: BlkCnt == 0.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }
                                                                /* Validate blk size.                                   */
    if (p_nor_data->PhyDataPtr->BlkSize < p_nor_data->SecSize + FS_DEV_NOR_BLK_HDR_LEN + FS_DEV_NOR_SEC_HDR_LEN) {
        FS_TRACE_DBG(("FSDev_NOR_CalcDevInfo(): Phy info invalid: BlkSize = %d < %d.\r\n", p_nor_data->PhyDataPtr->BlkSize, p_nor_data->SecSize + FS_DEV_NOR_BLK_HDR_LEN + FS_DEV_NOR_SEC_HDR_LEN));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }

    if (p_nor_data->PhyDataPtr->AddrRegionStart < p_nor_data->PhyDataPtr->AddrBase) {
        FS_TRACE_DBG(("FSDev_NOR_CalcDevInfo(): Phy info invalid: AddrRegionStart = %d < %d.\r\n", p_nor_data->PhyDataPtr->AddrRegionStart, p_nor_data->PhyDataPtr->AddrBase));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }



                                                                /* ------------ FIND 1ST BLK OF FILE SYSTEM ----------- */
    blk_nbr  = 0u;
    blk_addr = p_nor_data->PhyDataPtr->AddrRegionStart;
    while ((blk_addr < p_nor_data->AddrStart) &&                /* While addr of blk before start of file system ...    */
           (blk_nbr  < p_nor_data->PhyDataPtr->BlkCnt)) {       /* ... & more blocks rem                         ...    */
                                                                /* ... get start addr of next blk.                      */
        blk_addr += (CPU_ADDR)p_nor_data->PhyDataPtr->BlkSize;
        blk_nbr++;
    }

    if (blk_addr < p_nor_data->AddrStart) {                     /* Start addr after last blk in dev.                    */
        FS_TRACE_DBG(("FSDev_NOR_InfoCalc(): Start address of file system (0x%08X) after last blk in dev region.\r\n", p_nor_data->AddrStart));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }



                                                                /* ------------------- CALC SEC QTY ------------------- */
    p_nor_data->BlkSizeLog = FSUtil_Log2(p_nor_data->PhyDataPtr->BlkSize);
    p_nor_data->SecSizeLog = FSUtil_Log2(p_nor_data->SecSize);


                                                                /* Calc blk cnt.                                        */
    blk_cnt = p_nor_data->DevSize >> p_nor_data->BlkSizeLog;
    if (blk_cnt > p_nor_data->PhyDataPtr->BlkCnt - blk_nbr) {
        blk_cnt = p_nor_data->PhyDataPtr->BlkCnt - blk_nbr;
    }

                                                                /* Calc secs/blk for size.                              */
    sec_cnt_blk   = (p_nor_data->PhyDataPtr->BlkSize - FS_DEV_NOR_BLK_HDR_LEN) >> p_nor_data->SecSizeLog;
    data_size_blk =  FS_DEV_NOR_BLK_HDR_LEN + (sec_cnt_blk << p_nor_data->SecSizeLog) + (sec_cnt_blk << FS_DEV_NOR_SEC_HDR_LEN_LOG);
    while (data_size_blk > p_nor_data->PhyDataPtr->BlkSize) {
        data_size_blk -= p_nor_data->SecSize + FS_DEV_NOR_SEC_HDR_LEN;
        sec_cnt_blk--;
    }

    sec_cnt_tot = sec_cnt_blk * blk_cnt;



                                                                /* ------------------ STO INFO & RTN ------------------ */
    p_nor_data->BlkCntUsed   =   blk_cnt;
    p_nor_data->BlkNbrFirst  =   blk_nbr;
    p_nor_data->BlkSecCnts   =   sec_cnt_blk;
    p_nor_data->SecCntTot    =   sec_cnt_tot;
    p_nor_data->SecCntTotLog =   FSUtil_Log2(sec_cnt_tot);

    sec_cnt_rsvd             =  (sec_cnt_tot * p_nor_data->PctRsvd) / 100u;
    if (sec_cnt_rsvd < sec_cnt_blk) {                           /* If sec cnt rsvd less than sec cnt blk ...            */
        sec_cnt_rsvd = sec_cnt_blk;                             /* ... sec cnt rsvd equals sec cnt blk (see Note #2).   */
    }
    sec_cnt_data             =   sec_cnt_tot - sec_cnt_rsvd;

    ab_cnt                   =   1u;
    p_nor_data->AB_Cnt       =   ab_cnt;

    p_nor_data->Size         =   sec_cnt_data;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_AllocDevData()
*
* Description : Allocate NOR device data.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE         Device data allocated.
*                               FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device data requires memory allocated from the heap :
*
*                   (a) AB_IxTbl               : sizeof(FS_SEC_NBR) * AB_Cnt
*                                                +
*                   (b) AB_SecNextTbl          : sizeof(FS_SEC_QTY) * AB_Cnt
*                                                +
*                   (c) BlkEraseMap            : sizeof(CPU_INT08U) * ceil(BlkCntUsed / 8)
*                                                +
*                   (d) SecCntTbl
*                         If BlkSecCnts < 256  : sizeof(CPU_INT08U) * BlkSecCnts      OR
*                         Else                   sizeof(CPU_INT16U) * BlkSecCnts
*                                                +
*                   (e) L2P_Tbl                : log2(SecCntTot)    * Size
*                                                +
*                   (f) Buf                    : sizeof(CPU_INT08U) * SecSize
*********************************************************************************************************
*/

static  void  FSDev_NOR_AllocDevData (FS_DEV_NOR_DATA  *p_nor_data,
                                      FS_ERR           *p_err)
{
    FS_SEC_NBR  *p_ab_ix_tbl;
    FS_SEC_NBR  *p_ab_sec_next_tbl;
    LIB_ERR      alloc_err;
    void        *p_buf;
    CPU_SIZE_T   erase_map_size;
    void        *p_erase_map;
    void        *p_l2p_tbl;
    CPU_SIZE_T   octets_reqd;
    CPU_SIZE_T   sec_cnt_tbl_size;
    void        *p_sec_cnt_tbl;


                                                                /* --------------- ALLOC AV SEC NEXT TBL -------------- */
    p_ab_ix_tbl = (FS_SEC_NBR *)Mem_HeapAlloc( sizeof(FS_SEC_NBR) * (CPU_SIZE_T)p_nor_data->AB_Cnt,
                                               sizeof(CPU_DATA),
                                              &octets_reqd,
                                              &alloc_err);
    if (p_ab_ix_tbl == (FS_SEC_NBR *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for blk active ix tbl: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->AB_IxTbl = p_ab_ix_tbl;


                                                                /* --------------- ALLOC AB SEC NEXT TBL -------------- */
    p_ab_sec_next_tbl = (FS_SEC_QTY *)Mem_HeapAlloc( sizeof(FS_SEC_QTY) * (CPU_SIZE_T)p_nor_data->AB_Cnt,
                                                     sizeof(CPU_DATA),
                                                    &octets_reqd,
                                                    &alloc_err);
    if (p_ab_sec_next_tbl == (FS_SEC_QTY *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for blk active sec next tbl: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->AB_SecNextTbl = p_ab_sec_next_tbl;


                                                                /* ---------------- ALLOC BLK ERASE MAP --------------- */
    erase_map_size = (((CPU_SIZE_T)p_nor_data->BlkCntUsed + 7u) / 8u);
    p_erase_map    = (void *)Mem_HeapAlloc( sizeof(CPU_INT08U) * erase_map_size,
                                            sizeof(CPU_DATA),
                                           &octets_reqd,
                                           &alloc_err);
    if (p_erase_map == (void *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for blk erase map: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->BlkEraseMap = p_erase_map;


                                                                /* --------------- ALLOC BLK SEC CNT TBL -------------- */
    sec_cnt_tbl_size = (p_nor_data->BlkSecCnts < DEF_INT_08U_MAX_VAL) ? ((CPU_SIZE_T)p_nor_data->BlkCntUsed * sizeof(CPU_INT08U))
                                                                      : ((CPU_SIZE_T)p_nor_data->BlkCntUsed * sizeof(CPU_INT16U));
    p_sec_cnt_tbl    = (void *)Mem_HeapAlloc( sec_cnt_tbl_size,
                                              sizeof(CPU_DATA),
                                             &octets_reqd,
                                             &alloc_err);
    if (p_sec_cnt_tbl == (void *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for blk sec cnt tbl: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->BlkSecCntValidTbl = p_sec_cnt_tbl;


                                                                /* ------------------- ALLOC L2P TBL ------------------ */
    p_l2p_tbl= FSDev_NOR_L2P_Create( p_nor_data,
                                    &octets_reqd);
    if (p_l2p_tbl == (void *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for L2P tbl: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->L2P_Tbl = p_l2p_tbl;


                                                                /* ------------------- ALLOC SEC BUF ------------------ */
    p_buf = (void *)Mem_HeapAlloc( sizeof(CPU_INT08U) * (CPU_SIZE_T)p_nor_data->SecSize,
                                   FS_CFG_BUF_ALIGN_OCTETS,
                                  &octets_reqd,
                                  &alloc_err);
    if (p_buf == (void *)0) {
        FS_TRACE_DBG(("FSDev_NOR_AllocDevData(): Could not alloc mem for buf: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
    p_nor_data->BufPtr = p_buf;
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_RdBlkHdr()
*
* Description : Read header of a block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Block header read successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_RdBlkHdr (FS_DEV_NOR_DATA  *p_nor_data,
                                  void             *p_dest,
                                  FS_SEC_NBR        blk_ix,
                                  FS_ERR           *p_err)
{
    CPU_INT32U  blk_addr;


    blk_addr = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);     /* Get blk addr.                                        */
    if (blk_addr == (CPU_INT32U)DEF_INT_32U_MAX_VAL) {
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    FSDev_NOR_PhyRdHandler(p_nor_data,                          /* Rd blk hdr.                                          */
                           p_dest,
                           blk_addr,
                           FS_DEV_NOR_BLK_HDR_LEN,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_RdBlkHdr(): Failed to rd blk header %d (0x%08X).\r\n", blk_ix, blk_addr));
        return;
    }
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_RdSecLogical()
*
* Description : Read sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               p_dest              Pointer to destination buffer.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical sector number.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE           Sector read successfully.
*                                       FS_ERR_DEV_IO         Device I/O error.
*                                       FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) To read a sector, this driver must :
*
*                   (a) Look up physical sector number in the L2P table.
*
*                   (b) Find/calculate location of physical sector.
*
*                   (c) Read data.
*********************************************************************************************************
*/

static  void  FSDev_NOR_RdSecLogical (FS_DEV_NOR_DATA  *p_nor_data,
                                      void             *p_dest,
                                      FS_SEC_NBR        sec_nbr_logical,
                                      FS_ERR           *p_err)
{
    CPU_INT32U  sec_addr;
    FS_SEC_NBR  sec_nbr_phy;


    sec_nbr_phy = FSDev_NOR_L2P_GetEntry(p_nor_data,            /* Look up phy sec nbr (see Note #1a).                  */
                                         sec_nbr_logical);
    if (sec_nbr_phy == FS_DEV_NOR_SEC_NBR_INVALID) {
        Mem_Set(p_dest, 0xFFu, p_nor_data->SecSize);

       *p_err = FS_ERR_NONE;
    } else {
                                                                /* Get sec addr (see Note #1b).                         */
        sec_addr = FSDev_NOR_SecNbrPhy_to_Addr(p_nor_data, sec_nbr_phy);
        if (sec_addr == (CPU_INT32U)DEF_INT_32U_MAX_VAL) {
            FS_TRACE_DBG(("FSDev_NOR_RdSecLogical(): Failed to get sec addr %d.\r\n", sec_nbr_phy));
           *p_err = FS_ERR_DEV_IO;
            return;
        }

        FSDev_NOR_PhyRdHandler(p_nor_data,                      /* Rd sec (see Note #1c).                               */
                               p_dest,
                               sec_addr + FS_DEV_NOR_SEC_HDR_LEN,
                               p_nor_data->SecSize,
                               p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FSDev_NOR_RdSecLogical(): Failed to rd sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_WrSecLogical()
*
* Description : Write sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               p_src               Pointer to source buffer.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical sector number.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE           Sector written successfully.
*                                       FS_ERR_DEV_IO         Device I/O error.
*                                       FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) To write a sector, this driver must :
*
*                   (a) Check if sufficient free blocks exist in case new block must be allocated.
*
*                   (b) Look up physical sector number in the L2P table.
*                       (1) If found, find/calculate location of physical sector.
*                       (2) Remember as old physical sector location.
*
*                   (c) Write new physical sector.
*                       (1) Find/calculation location of next available physical sector.
*                           (A) Remember as new physical sector location.
*                       (2) Mark new physical sector as 'WRITING'.
*                       (3) Write data into physical sector.
*                       (4) Mark new physical sector as 'VALID'.
*                       (5) Update L2P table.
*
*                   (d) Mark old physical sector as 'INVALID'.
*                       (1) If last valid sector in block :
*                           (A) Move block to  erase queue.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_WrSecLogical (FS_DEV_NOR_DATA  *p_nor_data,
                                      void             *p_src,
                                      FS_SEC_NBR        sec_nbr_logical,
                                      FS_ERR           *p_err)
{
    FS_SEC_NBR  sec_nbr_phy_old;
    FS_SEC_NBR  blk_ix;


                                                                /* -------------- FREE BLK (see Note #1a) ------------- */
    blk_ix = FSDev_NOR_FindEraseBlk(p_nor_data);                /* Chk if blk must be erased.                           */
    while (blk_ix != FS_DEV_NOR_SEC_NBR_INVALID) {
        FSDev_NOR_EraseBlkPrepare(p_nor_data, blk_ix, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
        blk_ix = FSDev_NOR_FindEraseBlk(p_nor_data);
    }



                                                                /* ------------------ WR LOGICAL SEC ------------------ */
    sec_nbr_phy_old = FSDev_NOR_L2P_GetEntry(p_nor_data,        /* Save old phy sec (see Note #1b).                     */
                                             sec_nbr_logical);
    FSDev_NOR_WrSecLogicalHandler(p_nor_data,                   /* See Note #1c.                                        */
                                  p_src,
                                  sec_nbr_logical,
                                  p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ------- INVALIDATE OLD PHY SEC (see Note #1d) ------ */
    if (sec_nbr_phy_old != FS_DEV_NOR_SEC_NBR_INVALID) {
        FSDev_NOR_InvalidateSec(p_nor_data, sec_nbr_phy_old, p_err);
    }
}
#endif


/*
*********************************************************************************************************
*                                   FSDev_NOR_WrSecLogicalHandler()
*
* Description : Write sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               p_src               Pointer to source buffer.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical sector number.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE           Sector written successfully.
*                                       FS_ERR_DEV_IO         Device I/O error.
*                                       FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDev_NOR_WrSecLogical()  Note #1c'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_WrSecLogicalHandler (FS_DEV_NOR_DATA  *p_nor_data,
                                             void             *p_src,
                                             FS_SEC_NBR        sec_nbr_logical,
                                             FS_ERR           *p_err)
{
    CPU_INT32U  sec_addr;
    FS_SEC_NBR  sec_nbr_phy;
    CPU_INT08U  sec_hdr[FS_DEV_NOR_SEC_HDR_LEN];


                                                                /* ----------------- FIND NEW PHY SEC ----------------- */
    sec_nbr_phy = FSDev_NOR_AB_SecFind(p_nor_data,              /* Find active blk sec.                                 */
                                       sec_nbr_logical);
    if (sec_nbr_phy == FS_DEV_NOR_SEC_NBR_INVALID) {
        FS_TRACE_DBG(("FSDev_NOR_WrSecLogicalHandler(): Failed to get sec.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    sec_addr = FSDev_NOR_SecNbrPhy_to_Addr(p_nor_data, sec_nbr_phy);



                                                                /* -------------------- WR NEW DATA ------------------- */
    MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],  FS_DEV_NOR_STATUS_SEC_WRITING);

    FSDev_NOR_PhyWrHandler( p_nor_data,                         /* Mark sec as 'writing'.                               */
                           &sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],
                           (sec_addr + FS_DEV_NOR_SEC_HDR_OFFSET_STATUS),
                            4u,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        p_nor_data->SecCntInvalid++;
        FS_TRACE_DBG(("FSDev_NOR_WrSecLogicalHandler(): Failed to wr hdr of sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
        return;
    }

    FSDev_NOR_PhyWrHandler( p_nor_data,                         /* Wr sec.                                              */
                            p_src,
                           (sec_addr + FS_DEV_NOR_SEC_HDR_LEN),
                            p_nor_data->SecSize,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        p_nor_data->SecCntInvalid++;
        FS_TRACE_DBG(("FSDev_NOR_WrSecLogicalHandler(): Failed to wr sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
        return;
    }

    MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_SEC_NBR], sec_nbr_logical);
    MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],  FS_DEV_NOR_STATUS_SEC_VALID);



                                                                /* --------------- VALIDATE NEW PHY SEC --------------- */
    FSDev_NOR_PhyWrHandler( p_nor_data,                         /* Mark sec as 'written'.                               */
                           &sec_hdr[0],
                            sec_addr,
                            FS_DEV_NOR_SEC_HDR_LEN,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        p_nor_data->SecCntInvalid++;
        FS_TRACE_DBG(("FSDev_NOR_WrSecLogicalHandler(): Failed to wr hdr of sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
        return;
    }

    FSDev_NOR_L2P_SetEntry(p_nor_data,                          /* Update L2P tbl.                                      */
                           sec_nbr_logical,
                           sec_nbr_phy);
    p_nor_data->SecCntValid++;                                  /* Wr sec is now valid.                                 */


   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_NOR_InvalidateSec()
*
* Description : Invalidate sector.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------      Argument validated by caller.
*
*               sec_nbr_phy     Physical sector number.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE           Sector invalidated successfully.
*                                   FS_ERR_DEV_IO         Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_InvalidateSec (FS_DEV_NOR_DATA  *p_nor_data,
                                       FS_SEC_NBR        sec_nbr_phy,
                                       FS_ERR           *p_err)
{
    CPU_BOOLEAN  active;
    FS_SEC_NBR   blk_ix;
    CPU_BOOLEAN  erased;
    CPU_INT32U   sec_addr;
    FS_SEC_QTY   sec_cnt_valid;
    CPU_INT08U   sec_hdr[FS_DEV_NOR_SEC_HDR_LEN];


                                                                /* ---------------- MARK SEC AS INVALID --------------- */
    sec_addr = FSDev_NOR_SecNbrPhy_to_Addr(p_nor_data, sec_nbr_phy);
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
    if (sec_addr == DEF_INT_32U_MAX_VAL) {
        FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): Failed to get addr of sec %d.\r\n", sec_nbr_phy));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif
    MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS], FS_DEV_NOR_STATUS_SEC_INVALID);

    FSDev_NOR_PhyWrHandler( p_nor_data,                         /* Mark sec as 'invalid'.                               */
                           &sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],
                           (sec_addr + FS_DEV_NOR_SEC_HDR_OFFSET_STATUS),
                            4u,
                            p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): Failed to wr hdr of sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
        return;
    }



                                                                /* ------------------ UPDATE BLK INFO ----------------- */
    blk_ix = FSDev_NOR_SecNbrPhy_to_BlkIx(p_nor_data, sec_nbr_phy);
    if (blk_ix == (FS_SEC_QTY)FS_DEV_NOR_SEC_NBR_INVALID) {
        FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): Failed to get ix of blk containing sec %d.\r\n", sec_nbr_phy));
       *p_err = FS_ERR_DEV_IO;
        return;
    }


    FSDev_NOR_GetBlkInfo( p_nor_data,                           /* Get blk valid sec cnt.                               */
                          blk_ix,
                         &erased,
                         &sec_cnt_valid);

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
    if (erased == DEF_YES) {
        FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): L2P sec corrupted: blk %d erased.\r\n", blk_ix));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
    if (sec_cnt_valid == 0u) {
        FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): L2P sec corrupted: blk %d has no valid secs.\r\n", blk_ix));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif

    FSDev_NOR_DecBlkSecCntValid(p_nor_data, blk_ix);            /* Dec blk valid sec cnt.                               */
    sec_cnt_valid--;

    if (sec_cnt_valid == 0u) {                                  /* If no more valid secs in blk ...                     */
        active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
        if (active == DEF_NO) {                                 /* ... & blk is NOT active blk  ...                     */
            FS_TRACE_LOG(("FSDev_NOR_InvalidateSec(): Blk %d invalidated.\r\n", blk_ix));
            p_nor_data->BlkCntInvalid++;
            FS_CTR_STAT_INC(p_nor_data->StatInvalidBlkCtr);
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
            if (p_nor_data->BlkCntValid == 0u) {
                FS_TRACE_DBG(("FSDev_NOR_InvalidateSec(): Valid blk cnt == 0 upon invalidation of blk %d.\r\n", blk_ix));
               *p_err = FS_ERR_DEV_IO;
                return;
            }
#endif
            p_nor_data->BlkCntValid--;                          /* ... erase.                                           */
        }
    }
    p_nor_data->SecCntValid--;
    p_nor_data->SecCntInvalid++;

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     FSDev_NOR_EraseBlkPrepare()
*
* Description : Prepare & erase block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Block erased successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) A block move, if interrupted by power failure, will be resumed by 'FSDev_NOR_LowMountHandler()'
*                   upon mount if the target block (i.e., the active block) was the only erased block.
*                   See also 'FSDev_NOR_LowMountHandler()  Note #5'.
*
*               (2) If there were NO valid sectors in the source block, it MUST have been considered
*                   invalid already.
*
*               (3) The driver is responsible for maintaining an erased block to receive data sectors
*                   during this move.  See also 'FSDev_NOR_FindEraseBlk()  Note #1a2'.
*
*               (4) If the erase of the former active block is interrupted, multiple blocks may be found
*                   upon mount with valid or invalid sectors AND erased sectors.  The sectors in all but
*                   one of those will be classified as invalid upon mount (see 'FSDev_NOR_LowMountHandler()  Note #6').
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_EraseBlkPrepare (FS_DEV_NOR_DATA  *p_nor_data,
                                         FS_SEC_NBR        blk_ix,
                                         FS_ERR           *p_err)
{
    CPU_BOOLEAN  active;
    CPU_INT32U   blk_addr;
    CPU_BOOLEAN  erased;
    CPU_INT32U   sec_addr;
    FS_SEC_QTY   sec_cnt;
    FS_SEC_QTY   sec_cnt_valid;
    CPU_INT08U   sec_hdr[FS_DEV_NOR_SEC_HDR_LEN];
    FS_SEC_NBR   sec_ix;
    FS_SEC_NBR   sec_nbr_logical;
    FS_SEC_NBR   sec_nbr_phy;
    CPU_INT32U   status;


                                                                /* ---------------- INVALIDATE BLK INFO --------------- */
    active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
    if (active == DEF_YES) {                                    /* If blk to erase if active blk ... (see Note #4)      */
        FSDev_NOR_AB_Remove(p_nor_data, blk_ix);                /* ... invalidate blk.                                  */
    }



                                                                /* ----------------- MOVE DATA IN BLK ----------------- */
    FSDev_NOR_GetBlkInfo( p_nor_data,
                          blk_ix,
                         &erased,
                         &sec_cnt_valid);

    if (sec_cnt_valid != 0u) {
        FS_TRACE_LOG(("FSDev_NOR_EraseBlkPrepare(): Moving from blk %d, secs (%d) prior to erase.\r\n", blk_ix, sec_cnt_valid));

        sec_cnt       = p_nor_data->BlkSecCnts;
        blk_addr      = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);
        sec_nbr_phy   = FSDev_NOR_BlkIx_to_SecNbrPhy(p_nor_data, blk_ix);
        sec_addr      = blk_addr + FS_DEV_NOR_BLK_HDR_LEN;

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
        if (blk_addr == DEF_INT_32U_MAX_VAL) {
            FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Failed to get addr of blk ix %d.\r\n", blk_ix));
           *p_err = FS_ERR_DEV_IO;
            return;
        }
#endif

        sec_ix = 0u;
        while (sec_ix < sec_cnt) {
            FSDev_NOR_PhyRdHandler( p_nor_data,                 /* Rd blk hdr.                                          */
                                   &sec_hdr[0],
                                    sec_addr,
                                    FS_DEV_NOR_SEC_HDR_LEN,
                                    p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Failed to rd sec hdr %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
                return;
            }

            sec_nbr_logical = MEM_VAL_GET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_SEC_NBR]);
            status          = MEM_VAL_GET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS]);


                                                                /* ----------------- MOVE DATA IN SEC ----------------- */
            if ((status          == FS_DEV_NOR_STATUS_SEC_VALID) && /* If sec valid                ...                  */
                (sec_nbr_logical <  p_nor_data->Size)) {            /* ... & logical sec nbr valid ...                  */
                                                                    /* ... rd sec                  ...                  */
                FSDev_NOR_PhyRdHandler( p_nor_data,
                                        p_nor_data->BufPtr,
                                       (sec_addr + FS_DEV_NOR_SEC_HDR_LEN),
                                        p_nor_data->SecSize,
                                        p_err);
                if (*p_err != FS_ERR_NONE) {
                    FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Failed to rd sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
                    return;
                }
                                                                    /* ... & wr to free sec       ... (see Note #3)     */
                FSDev_NOR_WrSecLogicalHandler(p_nor_data,
                                              p_nor_data->BufPtr,
                                              sec_nbr_logical,
                                              p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }

                MEM_VAL_SET_INT32U((void *)&sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS], FS_DEV_NOR_STATUS_SEC_INVALID);

                FSDev_NOR_PhyWrHandler( p_nor_data,             /* Mark sec as 'invalid'.                               */
                                       &sec_hdr[FS_DEV_NOR_SEC_HDR_OFFSET_STATUS],
                                        sec_addr + FS_DEV_NOR_SEC_HDR_OFFSET_STATUS,
                                        4u,
                                        p_err);
                if (*p_err != FS_ERR_NONE) {
                    FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Failed to wr hdr of sec %d (0x%08X).\r\n", sec_nbr_phy, sec_addr));
                    return;
                }

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
                if (sec_cnt_valid == 0u) {
                    FSDev_NOR_IncBlkSecCntValid(p_nor_data, blk_ix);
                    FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Too many valid secs found in blk ix %d.\r\n", blk_ix));
                   *p_err = FS_ERR_DEV_IO;
                    return;
                }
#endif
                FSDev_NOR_DecBlkSecCntValid(p_nor_data, blk_ix);
                sec_cnt_valid--;
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
                if (p_nor_data->SecCntValid < 1u) {
                    FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Unexpectedly few valid secs (%d) exist in NOR.\r\n", p_nor_data->SecCntValid));
                   *p_err = FS_ERR_DEV_IO;
                    return;
                }
#endif
                p_nor_data->SecCntValid--;
                p_nor_data->SecCntInvalid++;
                FS_CTR_STAT_INC(p_nor_data->StatCopyCtr);
            }

            sec_addr += p_nor_data->SecSize + FS_DEV_NOR_SEC_HDR_LEN;
            sec_ix++;
            sec_nbr_phy++;
        }

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
        if (sec_cnt_valid != 0u) {
            FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Too few valid secs (%d more expected) found in blk ix %d.\r\n", sec_cnt_valid));

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_DBG)
            if (p_nor_data->SecCntValid < sec_cnt_valid) {
                FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Unexpectedly few valid secs (%d) exist in NOR.\r\n", p_nor_data->SecCntValid));
            }
#endif
           *p_err = FS_ERR_DEV_IO;
            return;
        }
#endif

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
        if (p_nor_data->BlkCntValid < 1u) {
            FS_TRACE_DBG(("FSDev_NOR_EraseBlkPrepare(): Unexpectedly few valid blks (%d) exist in NOR.\r\n", p_nor_data->BlkCntValid));
           *p_err = FS_ERR_DEV_IO;
            return;
        }
#endif
        p_nor_data->BlkCntValid--;                              /* One fewer valid blks (see Note #2).                  */
        p_nor_data->BlkCntInvalid++;
    }





                                                                /* --------------------- ERASE BLK -------------------- */
    FSDev_NOR_EraseBlkEmptyFmt(p_nor_data, blk_ix, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                    FSDev_NOR_EraseBlkEmptyFmt()
*
* Description : Erase empty block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           Block erased successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) To erase a block :
*                   (a) Find/calculation location of block.
*                   (b) Erase block.
*                   (c) Write block header.
*
*               (2) A block may lose its erase count if the block erase is interrupted, or if the block
*                   header is not completely written.  The block is assumed to have the maximum erase
*                   count upon restart.  Wear-leveling erased block approximately uniformly, so this
*                   should be close to true block count.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_EraseBlkEmptyFmt (FS_DEV_NOR_DATA  *p_nor_data,
                                          FS_SEC_NBR        blk_ix,
                                          FS_ERR           *p_err)
{
    CPU_INT32U  blk_addr;
    CPU_INT32U  erase_cnt;
    CPU_INT08U  blk_hdr[FS_DEV_NOR_BLK_HDR_LEN];
    CPU_INT08U  erase_cnt_octets[4];


                                                                /* --------------------- ERASE BLK -------------------- */
    blk_addr      = FSDev_NOR_BlkIx_to_Addr(p_nor_data, blk_ix);
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
    if (blk_addr == DEF_INT_32U_MAX_VAL) {
        FS_TRACE_DBG(("FSDev_NOR_EraseBlkEmptyFmt(): Failed to get addr of blk ix %d.\r\n", blk_ix));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif

    FSDev_NOR_PhyRdHandler( p_nor_data,                         /* Rd erase cnt.                                        */
                           &erase_cnt_octets[0],
                           (blk_addr + FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT),
                            4u,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_EraseBlkEmptyFmt(): Failed to rd erase cnts of blk %d (0x%08X).\r\n", blk_ix, blk_addr));
        return;
    }


    erase_cnt = MEM_VAL_GET_INT32U((void *)&erase_cnt_octets[0]);
    if (erase_cnt == FS_DEV_NOR_BLK_HDR_ERASE_CNT_INVALID) {
        erase_cnt  = p_nor_data->EraseCntMax;                   /* If erase cnt invalid, set to max cnt (see Note #2).  */
    } else {
        erase_cnt++;
        if (p_nor_data->EraseCntMax < erase_cnt) {
            FS_TRACE_LOG(("FSDev_NOR_EraseBlkEmptyFmt(): Max erase cnt now %d (prev was %d).\r\n", erase_cnt, p_nor_data->EraseCntMax));
            p_nor_data->EraseCntMax = erase_cnt;
        }
    }

    FSDev_NOR_PhyEraseBlkHandler(p_nor_data,                    /* Erase blk.                                           */
                                 blk_addr,
                                 p_nor_data->PhyDataPtr->BlkSize,
                                 p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_EraseBlkEmptyFmt(): Failed to erase blk ix %d (0x%08X).\r\n", blk_ix, blk_addr));
        return;
    }

    FS_TRACE_LOG(("FSDev_NOR_EraseBlkEmptyFmt(): Erased blk %d (0x%08X) w/erase cnt %d.\r\n", blk_ix, blk_addr, erase_cnt));


                                                                /* --------------------- WR BLK HDR ------------------- */
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK1],     FS_DEV_NOR_BLK_HDR_MARK_WORD_1);
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_MARK2],     FS_DEV_NOR_BLK_HDR_MARK_WORD_2);
    MEM_VAL_SET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT], erase_cnt);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_VER],       FS_DEV_NOR_BLK_HDR_VER);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_SEC_SIZE],  p_nor_data->SecSize);
    MEM_VAL_SET_INT16U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_BLK_CNT],   p_nor_data->BlkCntUsed);

    FSDev_NOR_PhyWrHandler( p_nor_data,                         /* Wr hdr.                                              */
                           &blk_hdr[0],
                            blk_addr,
                            FS_DEV_NOR_BLK_HDR_LEN,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_NOR_EraseBlkEmptyFmt(): Failed to wr blk hdr ix %d (0x%08X).\r\n", blk_ix, blk_addr));
        return;
    }


                                                                /* -------------------- UPDATE INFO ------------------- */
    p_nor_data->BlkCntErased++;                                 /* Another erased blk.                                  */
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
    if (p_nor_data->BlkCntInvalid == 0u) {
        FS_TRACE_DBG(("FSDev_NOR_EraseBlkEmptyFmt(): Invalid blk cnt == 0 upon erase of blk %d.\r\n", blk_ix));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif
    p_nor_data->BlkCntInvalid--;                                /* One fewer invalid blks.                              */
    p_nor_data->SecCntErased  += p_nor_data->BlkSecCnts;        /* More erased secs.                                    */
    p_nor_data->SecCntInvalid -= p_nor_data->BlkSecCnts;        /* Fewer invalid secs.                                  */

    FSDev_NOR_SetBlkErased(p_nor_data,                          /* Blk is erased.                                       */
                           blk_ix,
                           DEF_YES);

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_NOR_FindEraseBlk()
*
* Description : Find block that needs to be erased.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : Block index,                if    block found that needs to be erased.
*               FS_DEV_NOR_SEC_NBR_INVALID, if NO block            needs to be erased.
*
* Note(s)     : (1) (a) Blocks may be erased under one of two conditions :
*                       (1) The erase count difference threshold is exceeded, i.e., a block exists that
*                           has been erased 'EraseCntDiffTh' fewer times than the most often
*                           erased block.
*                       (2) Fewer block exist than ...
*                           (A) ... 1, if there is an active block assigned;
*                           (B) ... 2, if there is NO active block assigned.
*
*                           This maintains the minimum number erase blocks required by the driver (see also
*                           'FSDev_NOR_EraseBlkPrepare()  Note #3').
*
*                   (b) The block chosen for erasure is either :
*                       (1) The block with the lowest erase count.
*                       (2) The block with the fewest valid sectors.
*                       The former is chosen if the difference between the block's erase count & the
*                       maximum is greater than some maximum value; the default maximum is 100 erasures.
*
*                   (c) Blocks are erased until no more block such as #1a1 exist, & until enough
*                       sectors (#1a2) are unoccupied.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_FindEraseBlk (FS_DEV_NOR_DATA  *p_nor_data)
{
    CPU_BOOLEAN  active;
    FS_SEC_QTY   active_blk_cnt;
    FS_SEC_NBR   blk_ix;
    FS_SEC_NBR   blk_ix_min;
    CPU_BOOLEAN  empty;
    CPU_BOOLEAN  erased;
    FS_SEC_QTY   sec_cnt_valid_min;
    FS_SEC_QTY   sec_cnt_valid;


                                                                /* ----------------- CHK FOR ERASE TH ----------------- */
                                                                /* If enough blks erased                 ...            */
    active_blk_cnt = FSDev_NOR_GetAB_Cnt(p_nor_data);
    if (p_nor_data->BlkCntErased >= 1u) {
                                                                /* ... & erase cnt diff th satisfied  OR ...            */
        if ((p_nor_data->EraseCntMax       < p_nor_data->EraseCntMin + p_nor_data->EraseCntDiffTh) ||
                                                                /* ...   erased wear-level blk exists OR ...            */
            (p_nor_data->BlkWearLevelAvail == DEF_YES)                                             ||
                                                                /* ...   sufficient blks erased       OR ...            */
            (p_nor_data->BlkCntErased      >= 2u)                                                  ||
                                                                /* ...   at least one blk active         ...            */
            (active_blk_cnt                >  0u)) {
            return (FS_DEV_NOR_SEC_NBR_INVALID);                /* ... do not erase a blk yet.                          */
        }
    }



                                                                /* ----------------- ACTIVE WEAR-LEVEL ---------------- */
    blk_ix_min = FSDev_NOR_FindEraseBlkWear(p_nor_data);
    if (blk_ix_min == FS_DEV_NOR_SEC_NBR_INVALID) {             /* If NO minimally-erased blk    ...                    */
                                                                /* ... but if enough blks erased ...                    */
        if ( (p_nor_data->BlkCntErased >= 2u) ||
            ((p_nor_data->BlkCntErased >= 1u) && (active_blk_cnt > 0u))) {
            return (FS_DEV_NOR_SEC_NBR_INVALID);                /* ... do not erase a blk yet.                          */
        }


    } else {                                                    /* If active wear level req'd ...                       */
        FS_TRACE_LOG(("FSDev_NOR_FindEraseBlk(): Wear level performed: Blk %d erased.\r\n", blk_ix_min));
        p_nor_data->BlkWearLevelAvail = DEF_YES;
        return (blk_ix_min);                                    /* ... rtn blk.                                         */
    }



                                                                /* ---------------- CHK FOR PARTIAL BLK --------------- */
    blk_ix            = 0u;
    sec_cnt_valid_min = p_nor_data->BlkSecCnts;
    blk_ix_min        = FS_DEV_NOR_SEC_NBR_INVALID;
    empty             = DEF_NO;
    while ((blk_ix <  p_nor_data->BlkCntUsed) &&                /* Probe each block ...                                 */
           (empty  == DEF_NO)) {                                /* while NO empty blk is found.                         */

        FSDev_NOR_GetBlkInfo( p_nor_data,
                              blk_ix,
                             &erased,
                             &sec_cnt_valid);

        if (erased == DEF_NO) {
            active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
            if (active == DEF_YES) {                                                /* If blk is active blk ...         */
                sec_cnt_valid += FSDev_NOR_AB_SecCntErased(p_nor_data, blk_ix);     /* ... add erased secs.             */
            }

            if (sec_cnt_valid_min > sec_cnt_valid) {            /* Update min sec cnt.                                  */
                sec_cnt_valid_min = sec_cnt_valid;
                blk_ix_min        = blk_ix;

                if (sec_cnt_valid == 0u) {                      /* If blk is empty ...                                  */
                    empty = DEF_YES;                            /* ... brk from loop.                                   */
                }
            }
        }

        blk_ix++;
    }



                                                                /* ----------------- RTN MIN VALID BLK ---------------- */
    return (blk_ix_min);
}
#endif


/*
*********************************************************************************************************
*                                     FSDev_NOR_FindEraseBlkWear()
*
* Description : Find block that needs to be erased (for active wear leveling).
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------          Argument validated by caller.
*
* Return(s)   : Block index,                if    block found that needs to be erased.
*               FS_DEV_NOR_SEC_NBR_INVALID, if NO block            needs to be erased.
*
* Note(s)     : (1) Blocks with seldom-changed data will NOT be erased as often as other blocks with pure
*                   passive wear leveling.  Active wear-leveling purposefully copies data from such blocks
*                   to new (& more often erased) blocks.  The burden of erasures will subsequently fall
*                   more heavily upon these old blocks, evening the erase counts.
*
*                   (a) The minimum erase count MAY not be accurate, since the minimally-erased block(s)
*                       may have been erased; since more than one minimally-erased block may exist, the
*                       minimum erase count is NEVER updated upon erase.
*
*               (2) If all minimally-erased blocks are erased, then no block will be found.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_FindEraseBlkWear (FS_DEV_NOR_DATA  *p_nor_data)
{
    CPU_BOOLEAN  active;
    CPU_INT08U   blk_hdr[FS_DEV_NOR_BLK_HDR_LEN];
    FS_SEC_NBR   blk_ix;
    FS_SEC_NBR   blk_ix_min;
    CPU_BOOLEAN  erased;
    CPU_INT32U   erase_cnt;
    CPU_INT32U   erase_cnt_min;
    FS_SEC_QTY   sec_cnt_valid_min;
    FS_SEC_QTY   sec_cnt_valid;
    FS_ERR       err;


                                                                /* ---------- ACTIVE WEAR-LEVEL (see Note #1) --------- */
                                                                /* If erase cnt th exceeded ...                         */
    if (p_nor_data->EraseCntMax >= p_nor_data->EraseCntMin + p_nor_data->EraseCntDiffTh) {
                                                                /* ... find blk with min erase cnt & valid secs.        */
        blk_ix            = 0u;
        erase_cnt_min     = p_nor_data->EraseCntMax + 1u;
	    sec_cnt_valid_min = DEF_MIN(p_nor_data->SecCntErased, p_nor_data->BlkSecCnts);
        blk_ix_min        = FS_DEV_NOR_SEC_NBR_INVALID;
        while (blk_ix < p_nor_data->BlkCntUsed) {
            FSDev_NOR_GetBlkInfo( p_nor_data,
                                  blk_ix,
                                 &erased,
                                 &sec_cnt_valid);

            FSDev_NOR_RdBlkHdr( p_nor_data,
                               &blk_hdr[0],
                                blk_ix,
                               &err);
            if (err != FS_ERR_NONE) {
                return (FS_DEV_NOR_SEC_NBR_INVALID);            /* #### Add rtn err val.                                */
            }

            erase_cnt = MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT]);
            if (erase_cnt_min > erase_cnt) {                    /* Update min erase cnt.                                */
                erase_cnt_min = erase_cnt;
            }

            if (erased == DEF_NO) {                             /* If blk is NOT erased        ...                      */
                active = FSDev_NOR_IsAB(p_nor_data, blk_ix);
                if (active == DEF_NO) {                         /* ... & blk is NOT active blk ...                      */
                                                                /* ... & erase cnt below th    ...                      */
                    if (p_nor_data->EraseCntMax >= erase_cnt + p_nor_data->EraseCntDiffTh) {
                        if (sec_cnt_valid_min > sec_cnt_valid) {/* ... & fewer valid secs      ...                      */
                            sec_cnt_valid_min = sec_cnt_valid;
                            blk_ix_min        = blk_ix;         /* ... sel blk.                                         */
                        }
                    }
                }
            }

            blk_ix++;
        }

        p_nor_data->EraseCntMin = erase_cnt_min;                /* Update cnt min w/actual erase cnt min.               */

        if (blk_ix_min == FS_DEV_NOR_SEC_NBR_INVALID) {         /* Verify blk ix min (see Note #2).                     */
            return (FS_DEV_NOR_SEC_NBR_INVALID);
        }

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
        if (p_nor_data->EraseCntMin < erase_cnt_min) {
            FS_TRACE_LOG(("FSDev_NOR_FindEraseBlkWear(): Min erase cnt now %d (prev was %d).\r\n", erase_cnt_min, p_nor_data->EraseCntMin));
        }
#endif


                                                                /* If erase cnt th still exceeded (see Note #1a) ...    */
        if (p_nor_data->EraseCntMax >= p_nor_data->EraseCntMin + p_nor_data->EraseCntDiffTh) {
            return (blk_ix_min);                                /* ... erase blk.                                       */
        }
    }



                                                                /* -------------------- RTN NO BLK -------------------- */
    return (FS_DEV_NOR_SEC_NBR_INVALID);
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_NOR_FindErasedBlk()
*
* Description : Find block that is erased.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : Block index,                if    erased block found.
*               FS_DEV_NOR_SEC_NBR_INVALID, if NO erased block found.
*
* Note(s)     : (1) The erased block with the lowest erase count is selected.
*
*                   (a) #### Optimize for cases with one erased blk OR no large erase count differences.
*
*               (2) There may be no block with erase count as low as 'EraseCntMin' (see
*                   'FSDev_NOR_FindEraseBlkWear()  Note #1a').
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_FindErasedBlk (FS_DEV_NOR_DATA  *p_nor_data)
{
    CPU_INT08U   blk_hdr[FS_DEV_NOR_BLK_HDR_LEN];
    FS_SEC_QTY   blk_cnt_erased;
    FS_SEC_NBR   blk_ix;
    FS_SEC_NBR   blk_ix_erased;
    CPU_BOOLEAN  erased;
    CPU_INT32U   erase_cnt;
    CPU_INT32U   erase_cnt_min;
    CPU_BOOLEAN  found;
    FS_SEC_QTY   sec_cnt_valid;
    FS_ERR       err;


    blk_cnt_erased = 0u;
    blk_ix         = 0u;
    blk_ix_erased  = FS_DEV_NOR_SEC_NBR_INVALID;
    erase_cnt_min  = p_nor_data->EraseCntMax + 1u;
    found          = DEF_NO;
    while ((found  == DEF_NO) &&
           (blk_ix <  p_nor_data->BlkCntUsed)) {
        FSDev_NOR_GetBlkInfo( p_nor_data,
                              blk_ix,
                             &erased,
                             &sec_cnt_valid);
        if (erased == DEF_YES) {
            blk_cnt_erased++;

            FSDev_NOR_RdBlkHdr( p_nor_data,
                               &blk_hdr[0],
                                blk_ix,
                               &err);
            if (err != FS_ERR_NONE) {                               /* If hdr could NOT be rd ...                       */
                return (FS_DEV_NOR_SEC_NBR_INVALID);                /* ... rtn invalid blk ix.                          */
            }

            erase_cnt = MEM_VAL_GET_INT32U((void *)&blk_hdr[FS_DEV_NOR_BLK_HDR_OFFSET_ERASE_CNT]);
            if (erase_cnt_min > erase_cnt) {                        /* If lower erase cnt (see Note #1) ...             */
                erase_cnt_min = erase_cnt;
                blk_ix_erased = blk_ix;                             /* ... sel this blk.                                */


                if (erase_cnt_min == p_nor_data->EraseCntMin) {     /* If least erased blk (see Note #2) ...            */
                    found = DEF_YES;                                /* ... stop srch.                                   */
                }
                if (blk_cnt_erased >= p_nor_data->BlkCntErased) {   /* If no more erased blks exist ...                 */
                    found = DEF_YES;                                /* ... stop srch.                                   */
                }
            }
        }

        blk_ix++;
    }

    p_nor_data->BlkWearLevelAvail = DEF_NO;

    return (blk_ix_erased);
}
#endif


/*
*********************************************************************************************************
*                                        FSDev_NOR_AB_ClrAll()
*
* Description : Clear all active block information.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_AB_ClrAll (FS_DEV_NOR_DATA  *p_nor_data)
{
    FS_SEC_QTY  ix;


    ix  = 0u;
    while (ix < p_nor_data->AB_Cnt) {
        p_nor_data->AB_IxTbl[ix]      = FS_DEV_NOR_SEC_NBR_INVALID;
        p_nor_data->AB_SecNextTbl[ix] = 0u;
        ix++;
    }
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_GetAB_Cnt()
*
* Description : Determine number of active blocks.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : Number of active blocks.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_QTY  FSDev_NOR_GetAB_Cnt (FS_DEV_NOR_DATA  *p_nor_data)
{
    FS_SEC_QTY  cnt;
    FS_SEC_QTY  ix;


    cnt = 0u;
    ix  = 0u;
    while (ix < p_nor_data->AB_Cnt) {
        if (p_nor_data->AB_IxTbl[ix] != FS_DEV_NOR_SEC_NBR_INVALID) {
            cnt++;
        }
        ix++;
    }
    return (cnt);
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_NOR_IsAB()
*
* Description : Determine whether block is an active block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : DEF_YES, if block is an active block.
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSDev_NOR_IsAB (FS_DEV_NOR_DATA  *p_nor_data,
                                     FS_SEC_NBR        blk_ix)
{
    FS_SEC_NBR   ix;
    CPU_BOOLEAN  found;


    found = DEF_NO;
    ix    = 0u;
    while ((ix    <  p_nor_data->AB_Cnt) &&
           (found == DEF_NO)) {
        if (p_nor_data->AB_IxTbl[ix] == blk_ix) {
            found = DEF_YES;
        }
        ix++;
    }
    return (found);
}
#endif


/*
*********************************************************************************************************
*                                         FSDev_NOR_AB_Add()
*
* Description : Add new active block.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------      Argument validated by caller.
*
*               blk_ix          Block index.
*
*               sec_ix_next     Next sector in block to use.
*
* Return(s)   : DEF_OK,   if block added.
*               DEF_FAIL, otherwise (see Note #1).
*
* Note(s)     : (1) If all active block slots are used, another active block CANNOT be added.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_NOR_AB_Add (FS_DEV_NOR_DATA  *p_nor_data,
                                       FS_SEC_NBR        blk_ix,
                                       FS_SEC_NBR        sec_ix_next)
{
    FS_SEC_NBR   ix;
    CPU_BOOLEAN  added;


    added = DEF_FAIL;
    ix    = 0u;
    while ((ix    <  p_nor_data->AB_Cnt) &&
           (added != DEF_OK)) {
        if (p_nor_data->AB_IxTbl[ix]      == FS_DEV_NOR_SEC_NBR_INVALID) {
            p_nor_data->AB_IxTbl[ix]      =  blk_ix;
            p_nor_data->AB_SecNextTbl[ix] =  sec_ix_next;
            added = DEF_OK;
        }
        ix++;
    }

    return (added);
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_AB_RemoveAll()
*
* Description : Remove all active blocks.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_AB_RemoveAll (FS_DEV_NOR_DATA  *p_nor_data)
{
    CPU_BOOLEAN  erased;
    FS_SEC_QTY   sec_cnt_valid;
    FS_SEC_QTY   ix;


    ix  = 0u;
    while (ix < p_nor_data->AB_Cnt) {
        if (p_nor_data->AB_IxTbl[ix] != FS_DEV_NOR_SEC_NBR_INVALID) {
            FSDev_NOR_GetBlkInfo( p_nor_data,
                                  p_nor_data->AB_IxTbl[ix],
                                 &erased,
                                 &sec_cnt_valid);
            if (sec_cnt_valid == 0u) {                          /* If no valid secs in blk ...                          */
                p_nor_data->BlkCntInvalid++;                    /* ... blk is now invalid.                              */
                p_nor_data->BlkCntValid--;
            }

            p_nor_data->SecCntInvalid    += p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
            p_nor_data->SecCntErased     -= p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
            p_nor_data->AB_IxTbl[ix]      = FS_DEV_NOR_SEC_NBR_INVALID;
            p_nor_data->AB_SecNextTbl[ix] = 0u;
        }
        ix++;
    }
}
#endif


/*
*********************************************************************************************************
*                                        FSDev_NOR_AB_Remove()
*
* Description : Remove active block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_AB_Remove (FS_DEV_NOR_DATA  *p_nor_data,
                                   FS_SEC_NBR        blk_ix)
{
    CPU_BOOLEAN  erased;
    CPU_BOOLEAN  found;
    FS_SEC_QTY   sec_cnt_valid;
    FS_SEC_NBR   ix;


    found = DEF_NO;
    ix    = 0u;
    while ((ix    <  p_nor_data->AB_Cnt) &&
           (found == DEF_NO)) {
        if (p_nor_data->AB_IxTbl[ix] == blk_ix) {
            FSDev_NOR_GetBlkInfo( p_nor_data,
                                  p_nor_data->AB_IxTbl[ix],
                                 &erased,
                                 &sec_cnt_valid);
            if (sec_cnt_valid == 0u) {                          /* If no valid secs in blk ...                          */
                p_nor_data->BlkCntInvalid++;                    /* ... blk is now invalid.                              */
                p_nor_data->BlkCntValid--;
            }

            p_nor_data->SecCntInvalid    += p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
            p_nor_data->SecCntErased     -= p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
            p_nor_data->AB_IxTbl[ix]      = FS_DEV_NOR_SEC_NBR_INVALID;
            p_nor_data->AB_SecNextTbl[ix] = 0u;
            found                               = DEF_YES;;
        }
        ix++;
    }
}
#endif


/*
*********************************************************************************************************
*                                   FSDev_NOR_AB_SecCntTotErased()
*
* Description : Determine total number of erase sectors in active blocks.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
* Return(s)   : Erased sector count.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_QTY  FSDev_NOR_AB_SecCntTotErased (FS_DEV_NOR_DATA  *p_nor_data)
{
    FS_SEC_QTY  erased_sec_cnt;
    FS_SEC_QTY  ix;


    erased_sec_cnt = 0u;
    ix             = 0u;
    while (ix < p_nor_data->AB_Cnt) {
        if (p_nor_data->AB_IxTbl[ix] != FS_DEV_NOR_SEC_NBR_INVALID) {
            erased_sec_cnt += p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
        }
        ix++;
    }
    return (erased_sec_cnt);
}
#endif


/*
*********************************************************************************************************
*                                     FSDev_NOR_AB_SecCntErased()
*
* Description : Determine number of erase sectors in active block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : Erased sector count.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_QTY  FSDev_NOR_AB_SecCntErased (FS_DEV_NOR_DATA  *p_nor_data,
                                               FS_SEC_NBR        blk_ix)
{
    FS_SEC_QTY   erased_sec_cnt;
    CPU_BOOLEAN  found;
    FS_SEC_NBR   ix;


    erased_sec_cnt = 0u;
    found          = DEF_NO;
    ix             = 0u;
    while ((ix    <  p_nor_data->AB_Cnt) &&
           (found == DEF_NO)) {
        if (p_nor_data->AB_IxTbl[ix] == blk_ix) {
            erased_sec_cnt = p_nor_data->BlkSecCnts - p_nor_data->AB_SecNextTbl[ix];
            found          = DEF_YES;
        }
        ix++;
    }
    return (erased_sec_cnt);
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_NOR_AB_SecFind()
*
* Description : Find active block sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical sector to be stored in active block.
*
* Return(s)   : Physical sector index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_AB_SecFind (FS_DEV_NOR_DATA  *p_nor_data,
                                          FS_SEC_NBR        sec_nbr_logical)
{
    FS_SEC_NBR  blk_ix_ab;
    FS_SEC_NBR  sec_nbr_phy;
    FS_SEC_NBR  ix;
    FS_SEC_NBR  ix_sel;


    ix_sel      = (sec_nbr_logical * p_nor_data->AB_Cnt * 2u) / p_nor_data->Size;
    if (ix_sel >=  p_nor_data->AB_Cnt) {
        ix_sel -=  p_nor_data->AB_Cnt;
    }
    blk_ix_ab = p_nor_data->AB_IxTbl[ix_sel];
    if (blk_ix_ab == FS_DEV_NOR_SEC_NBR_INVALID) {              /* If desired blk ix invalid ...                        */
        blk_ix_ab = FSDev_NOR_FindErasedBlk(p_nor_data);        /* ... find erased blk.                                 */

        if (blk_ix_ab != FS_DEV_NOR_SEC_NBR_INVALID) {
            p_nor_data->AB_IxTbl[ix_sel]      = blk_ix_ab;
            p_nor_data->AB_SecNextTbl[ix_sel] = 0u;

            FSDev_NOR_SetBlkErased(p_nor_data,                  /* Set blk erased status.                               */
                                   blk_ix_ab,
                                   DEF_NO);
            FSDev_NOR_SetBlkSecCntValid(p_nor_data,             /* Set sec cnt valid.                                   */
                                        blk_ix_ab,
                                        0u);

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
            if (p_nor_data->BlkCntErased == 0u) {
                FS_TRACE_DBG(("FSDev_NOR_AB_SecFind(): Erased blk cnt == 0 upon alloc of blk %d.\r\n", blk_ix_active));
                return (FS_DEV_NOR_SEC_NBR_INVALID);
            }
#endif

            p_nor_data->BlkCntErased--;                         /* Erased blk now contains valid data.                  */
            p_nor_data->BlkCntValid++;
        }
    }

    if (blk_ix_ab == FS_DEV_NOR_SEC_NBR_INVALID) {              /* If still no active blk ...                           */
        ix = 0u;
        while (ix < p_nor_data->AB_Cnt) {                       /* ... srch through all blks.                           */
            blk_ix_ab = p_nor_data->AB_IxTbl[ix];
            if (blk_ix_ab != FS_DEV_NOR_SEC_NBR_INVALID) {
                ix_sel = ix;
                break;
            }
            ix++;
        }
    }

    if (blk_ix_ab == FS_DEV_NOR_SEC_NBR_INVALID) {
        return (FS_DEV_NOR_SEC_NBR_INVALID);
    }

                                                                /* Get start phy sec nbr of blk.                        */
    sec_nbr_phy = FSDev_NOR_BlkIx_to_SecNbrPhy(p_nor_data, blk_ix_ab);
#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
    if (sec_nbr_phy == FS_DEV_NOR_SEC_NBR_INVALID) {
        FS_TRACE_DBG(("FSDev_NOR_AB_SecFind(): Failed to get addr of blk %d.\r\n", blk_ix_active));
        return (FS_DEV_NOR_SEC_NBR_INVALID);
    }
#endif
                                                                /* Get phy sec nbr of next free sec in blk.             */
    sec_nbr_phy += p_nor_data->AB_SecNextTbl[ix_sel];

    p_nor_data->AB_SecNextTbl[ix_sel]++;
    p_nor_data->SecCntErased--;
    FSDev_NOR_IncBlkSecCntValid(p_nor_data, p_nor_data->AB_IxTbl[ix_sel]);

                                                                /* If all secs in active blk used ...                   */
    if (p_nor_data->AB_SecNextTbl[ix_sel] == p_nor_data->BlkSecCnts) {
        p_nor_data->AB_IxTbl[ix_sel]      =  FS_DEV_NOR_SEC_NBR_INVALID;
        p_nor_data->AB_SecNextTbl[ix_sel] =  0u;                /* ... invalidate active blk ix.                        */
    }

    return (sec_nbr_phy);
}
#endif


/*
*********************************************************************************************************
*                                       FSDev_NOR_GetBlkInfo()
*
* Description : Get block information.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               blk_ix              Block index.
*
*               p_erased            Pointer to variable that will receive the erase status.
*               ----------          Argument validated by caller.
*
*               p_sec_cnt_valid     Pointer to variable that will receive the count of valid sectors.
*               ----------          Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_GetBlkInfo (FS_DEV_NOR_DATA  *p_nor_data,
                                    FS_SEC_NBR        blk_ix,
                                    CPU_BOOLEAN      *p_erased,
                                    FS_SEC_QTY       *p_sec_cnt_valid)
{
    FS_SEC_NBR    blk_ix_bit;
    FS_SEC_NBR    blk_ix_byte;
    CPU_INT08U   *p_erase_map;
    CPU_BOOLEAN   erased;
    CPU_INT08U   *p_sec_cnt_tbl_08;
    CPU_INT16U   *p_sec_cnt_tbl_16;
    FS_SEC_QTY    sec_cnt_valid;


    blk_ix_bit  = blk_ix % DEF_INT_08_NBR_BITS;
    blk_ix_byte = blk_ix / DEF_INT_08_NBR_BITS;

    p_erase_map = (CPU_INT08U *)p_nor_data->BlkEraseMap;
    erased      =  DEF_BIT_IS_SET(p_erase_map[blk_ix_byte], DEF_BIT(blk_ix_bit));
   *p_erased    =  erased;

    if (erased == DEF_NO) {
        if (p_nor_data->BlkSecCnts < DEF_INT_08U_MAX_VAL) {
            p_sec_cnt_tbl_08 = (CPU_INT08U *)p_nor_data->BlkSecCntValidTbl;
            sec_cnt_valid    = (FS_SEC_QTY)(p_sec_cnt_tbl_08[blk_ix]);
        } else {
            p_sec_cnt_tbl_16 = (CPU_INT16U *)p_nor_data->BlkSecCntValidTbl;
            sec_cnt_valid    = (FS_SEC_QTY)(p_sec_cnt_tbl_16[blk_ix]);
        }
    } else {
        sec_cnt_valid = 0u;
    }

   *p_sec_cnt_valid = sec_cnt_valid;
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_NOR_SetBlkErased()
*
* Description : Set block erase status.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
*               erased      Erased status.

* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_SetBlkErased (FS_DEV_NOR_DATA  *p_nor_data,
                                      FS_SEC_NBR        blk_ix,
                                      CPU_BOOLEAN       erased)
{
    FS_SEC_NBR   blk_ix_bit;
    FS_SEC_NBR   blk_ix_byte;
    CPU_INT08U  *p_erase_map;


    blk_ix_bit  = blk_ix % DEF_INT_08_NBR_BITS;
    blk_ix_byte = blk_ix / DEF_INT_08_NBR_BITS;

    p_erase_map = (CPU_INT08U *)p_nor_data->BlkEraseMap;
    if (erased == DEF_YES) {
        DEF_BIT_SET(p_erase_map[blk_ix_byte], DEF_BIT(blk_ix_bit));
    } else {
        DEF_BIT_CLR(p_erase_map[blk_ix_byte], DEF_BIT(blk_ix_bit));
    }
}


/*
*********************************************************************************************************
*                                    FSDev_NOR_SetBlkSecCntValid()
*
* Description : Set block valid sector count.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix          Block index.
*
*               sec_cnt_valid   Valid sector count.

* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_SetBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,
                                           FS_SEC_NBR        blk_ix,
                                           FS_SEC_QTY        sec_cnt_valid)
{
    CPU_INT08U  *p_sec_cnt_tbl_08;
    CPU_INT16U  *p_sec_cnt_tbl_16;


    if (p_nor_data->BlkSecCnts < DEF_INT_08U_MAX_VAL) {
        p_sec_cnt_tbl_08         = (CPU_INT08U *)p_nor_data->BlkSecCntValidTbl;
        p_sec_cnt_tbl_08[blk_ix] = (CPU_INT08U  )sec_cnt_valid;
    } else {
        p_sec_cnt_tbl_16         = (CPU_INT16U *)p_nor_data->BlkSecCntValidTbl;
        p_sec_cnt_tbl_16[blk_ix] = (CPU_INT16U  )sec_cnt_valid;
    }
}


/*
*********************************************************************************************************
*                                    FSDev_NOR_DecBlkSecCntValid()
*
* Description : Decrement block valid sector count.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_DecBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,
                                           FS_SEC_NBR        blk_ix)
{
    CPU_INT08U  *p_sec_cnt_tbl_08;
    CPU_INT16U  *p_sec_cnt_tbl_16;
    CPU_INT08U   sec_cnt_08;
    CPU_INT16U   sec_cnt_16;


    if (p_nor_data->BlkSecCnts < DEF_INT_08U_MAX_VAL) {
        p_sec_cnt_tbl_08         = (CPU_INT08U *)p_nor_data->BlkSecCntValidTbl;
        sec_cnt_08               =  p_sec_cnt_tbl_08[blk_ix];

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
        if (sec_cnt_08 == 0u) {
            FS_TRACE_DBG(("FSDev_NOR_DecBlkSecCntValid(): Sec cnt for blk %d already 0.\r\n", blk_ix));
            return;
        }
#endif

        p_sec_cnt_tbl_08[blk_ix] =  sec_cnt_08 - 1u;
    } else {
        p_sec_cnt_tbl_16         = (CPU_INT16U *)p_nor_data->BlkSecCntValidTbl;
        sec_cnt_16               =  p_sec_cnt_tbl_16[blk_ix];

#if (FS_DEV_NOR_CFG_DBG_CHK_EN == DEF_ENABLED)
        if (sec_cnt_16 == 0u) {
            FS_TRACE_DBG(("FSDev_NOR_DecBlkSecCntValid(): Sec cnt for blk %d already 0.\r\n", blk_ix));
            return;
        }
#endif

        p_sec_cnt_tbl_16[blk_ix] =  sec_cnt_16 - 1u;
    }
}
#endif


/*
*********************************************************************************************************
*                                    FSDev_NOR_IncBlkSecCntValid()
*
* Description : Increment block valid sector count.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_NOR_IncBlkSecCntValid (FS_DEV_NOR_DATA  *p_nor_data,
                                           FS_SEC_NBR        blk_ix)
{
    CPU_INT08U  *p_sec_cnt_tbl_08;
    CPU_INT16U  *p_sec_cnt_tbl_16;
    CPU_INT08U   sec_cnt_08;
    CPU_INT16U   sec_cnt_16;


    if (p_nor_data->BlkSecCnts < DEF_INT_08U_MAX_VAL) {
        p_sec_cnt_tbl_08         = (CPU_INT08U *)p_nor_data->BlkSecCntValidTbl;
        sec_cnt_08               =  p_sec_cnt_tbl_08[blk_ix];
        p_sec_cnt_tbl_08[blk_ix] =  sec_cnt_08 + 1u;
    } else {
        p_sec_cnt_tbl_16         = (CPU_INT16U *)p_nor_data->BlkSecCntValidTbl;
        sec_cnt_16               =  p_sec_cnt_tbl_16[blk_ix];
        p_sec_cnt_tbl_16[blk_ix] =  sec_cnt_16 + 1u;
    }
}
#endif


/*
*********************************************************************************************************
*                                      FSDev_NOR_BlkIx_to_Addr()
*
* Description : Get address of a block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index
*
* Return(s)   : Block address, relative start of device.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  FSDev_NOR_BlkIx_to_Addr (FS_DEV_NOR_DATA  *p_nor_data,
                                             FS_SEC_NBR        blk_ix)
{
    CPU_INT32U  blk_addr;


    blk_ix  +=  p_nor_data->BlkNbrFirst;
    blk_addr = (p_nor_data->PhyDataPtr->AddrRegionStart - p_nor_data->PhyDataPtr->AddrBase)
             + (blk_ix << p_nor_data->BlkSizeLog);

    return (blk_addr);
}


/*
*********************************************************************************************************
*                                    FSDev_NOR_SecNbrPhy_to_Addr()
*
* Description : Get address of a physical sector.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------      Argument validated by caller.
*
*               sec_nbr_phy     Physical sector number
*
* Return(s)   : Sector address, relative start of device.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  FSDev_NOR_SecNbrPhy_to_Addr (FS_DEV_NOR_DATA  *p_nor_data,
                                                 FS_SEC_NBR        sec_nbr_phy)
{
    FS_SEC_NBR  blk_ix;
    FS_SEC_NBR  sec_ix;
    CPU_INT32U  sec_addr;


    blk_ix   =  p_nor_data->BlkNbrFirst;
    blk_ix  +=  sec_nbr_phy / p_nor_data->BlkSecCnts;
    sec_ix   =  sec_nbr_phy % p_nor_data->BlkSecCnts;
    sec_addr = (p_nor_data->PhyDataPtr->AddrRegionStart - p_nor_data->PhyDataPtr->AddrBase)
             + (blk_ix << p_nor_data->BlkSizeLog)
             +  FS_DEV_NOR_BLK_HDR_LEN
             + (sec_ix << p_nor_data->SecSizeLog)
             + (sec_ix << FS_DEV_NOR_SEC_HDR_LEN_LOG);

    return (sec_addr);
}


/*
*********************************************************************************************************
*                                   FSDev_NOR_SecNbrPhy_to_BlkIx()
*
* Description : Get index of block in which a physical sector is located.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               sec_nbr_phy     Physical sector number
*
* Return(s)   : Block index.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_SecNbrPhy_to_BlkIx (FS_DEV_NOR_DATA  *p_nor_data,
                                                  FS_SEC_NBR        sec_nbr_phy)
{
    FS_SEC_NBR  blk_ix;


    blk_ix = sec_nbr_phy / p_nor_data->BlkSecCnts;
    return (blk_ix);
}
#endif


/*
*********************************************************************************************************
*                                   FSDev_NOR_BlkIx_to_SecNbrPhy()
*
* Description : Get sector number of a block.
*
* Argument(s) : p_nor_data  Pointer to NOR data.
*               ----------  Argument validated by caller.
*
*               blk_ix      Block index.
*
* Return(s)   : Sector number.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_NBR  FSDev_NOR_BlkIx_to_SecNbrPhy (FS_DEV_NOR_DATA  *p_nor_data,
                                                  FS_SEC_NBR        blk_ix)
{
    FS_SEC_NBR  sec_nbr;


    sec_nbr = blk_ix * p_nor_data->BlkSecCnts;
    return (sec_nbr);
}
#endif



/*
*********************************************************************************************************
*                                       FSDev_NOR_L2P_Create()
*
* Description : Allocate logical-to-physical table.
*
* Argument(s) : p_nor_data      Pointer to NOR data.
*               ----------      Argument validated by caller.
*
*               p_octets_reqd   Pointer to a variable to ... :
*               ----------      Argument validated by caller.
*
*                                   (a) Return the number of octets required to successfully allocate the
*                                           L2P table, if any errors;
*                                   (b) Return 0, otherwise.
*
* Return(s)   : Pointer to L2P table, if NO errors.
*               Pointer to NULL,      otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  *FSDev_NOR_L2P_Create (FS_DEV_NOR_DATA  *p_nor_data,
                                     CPU_SIZE_T       *p_octets_reqd)
{
    LIB_ERR      alloc_err;
    CPU_SIZE_T   l2p_tbl_size;
    CPU_INT32U   l2p_tbl_size_bit;
    void        *p_l2p_tbl;


    l2p_tbl_size_bit = (CPU_INT32U)p_nor_data->Size * (CPU_INT32U)p_nor_data->SecCntTotLog;
    l2p_tbl_size     = (CPU_SIZE_T)((l2p_tbl_size_bit + DEF_OCTET_NBR_BITS - 1u) / DEF_OCTET_NBR_BITS);
    p_l2p_tbl        = (void *)Mem_HeapAlloc( l2p_tbl_size,
                                              sizeof(CPU_DATA),
                                              p_octets_reqd,
                                             &alloc_err);

    return (p_l2p_tbl);
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_L2P_GetEntry()
*
* Description : Get logical-to-physical table entry for a logical sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical sector number.
*               ----------          Argument validated by caller.
*
* Return(s)   : Physical sector number,     if table entry     set.
*               FS_DEV_NOR_SEC_NBR_INVALID, if table entry NOT set (see Note #1).
*
* Note(s)     : (1) FS_DEV_NOR_SEC_NBR_INVALID must be defined to DEF_INT_32U_MAX_VAL.
*
*                   (a) DEF_INT_32U_MAX_VAL cannot be stored in a table entry; consequently, a placeholder
*                       value, the maximum integer for the entry width, is set instead & must be detected/
*                       handled upon get.
*********************************************************************************************************
*/

static  FS_SEC_NBR  FSDev_NOR_L2P_GetEntry (FS_DEV_NOR_DATA  *p_nor_data,
                                            FS_SEC_NBR        sec_nbr_logical)
{
    CPU_INT08U   bits_rd;
    CPU_INT08U   bits_rem;
    CPU_INT32U   l2p_bit_off;
    CPU_INT32U   l2p_octet_off;
    CPU_INT08U   l2p_octet_bit_off;
    CPU_INT08U   l2p_rd_octet;
    CPU_INT08U  *p_l2p_tbl_08;
    FS_SEC_NBR   sec_nbr_phy;


    l2p_bit_off       = (CPU_INT32U)sec_nbr_logical * (CPU_INT32U)p_nor_data->SecCntTotLog;
    l2p_octet_off     =              l2p_bit_off / DEF_OCTET_NBR_BITS;
    l2p_octet_bit_off = (CPU_INT08U)(l2p_bit_off % DEF_OCTET_NBR_BITS);
    p_l2p_tbl_08      = (CPU_INT08U *)p_nor_data->L2P_Tbl + l2p_octet_off;
    bits_rem          =  p_nor_data->SecCntTotLog;
    sec_nbr_phy       =  0u;

    while (bits_rem > 0u) {
        bits_rd             =  DEF_MIN(DEF_OCTET_NBR_BITS - l2p_octet_bit_off, bits_rem);

        l2p_rd_octet        = *p_l2p_tbl_08;                    /* Rd tbl octet ...                                     */
        l2p_rd_octet      >>=  l2p_octet_bit_off;               /*              ... shift bits to lowest ...            */
        l2p_rd_octet       &=  DEF_BIT_FIELD(bits_rd, 0u);      /*              ... mask off other bits  ...            */
        sec_nbr_phy        |= (FS_SEC_NBR)l2p_rd_octet << (p_nor_data->SecCntTotLog - bits_rem);    /*   ... OR w/datum.*/
        p_l2p_tbl_08++;

        bits_rem           -=  bits_rd;
        l2p_octet_bit_off   =  0u;                              /* Next rd will be at start of next octet.              */
    }

    if (sec_nbr_phy == DEF_BIT_FIELD(p_nor_data->SecCntTotLog, 0u)) {
        sec_nbr_phy =  DEF_INT_32U_MAX_VAL;
    }

    return (sec_nbr_phy);
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_L2P_SetEntry()
*
* Description : Set logical-to-physical table entry for a logical sector.
*
* Argument(s) : p_nor_data          Pointer to NOR data.
*               ----------          Argument validated by caller.
*
*               sec_nbr_logical     Logical  sector number.
*
*               sec_nbr_phy         Physical sector number,     if table entry to be set.
*                                       OR
*                                   FS_DEV_NOR_SEC_NBR_INVALID, if table entry to be cleared (see Note #1).
*
* Return(s)   : none.
*
* Note(s)     : (1) FS_DEV_NOR_SEC_NBR_INVALID must be defined to DEF_INT_32U_MAX_VAL.
*
*                   (a) DEF_INT_32U_MAX_VAL cannot be stored in a table entry; consequently, a placeholder
*                       value, the maximum integer for the entry width, is set instead & must be detected/
*                       handled upon get.
*********************************************************************************************************
*/

static  void  FSDev_NOR_L2P_SetEntry (FS_DEV_NOR_DATA  *p_nor_data,
                                      FS_SEC_NBR        sec_nbr_logical,
                                      FS_SEC_NBR        sec_nbr_phy)
{
    CPU_INT08U   bits_rem;
    CPU_INT08U   bits_wr;
    CPU_INT32U   l2p_bit_off;
    CPU_INT32U   l2p_octet_off;
    CPU_INT08U   l2p_octet_bit_off;
    CPU_INT08U   l2p_rd_octet;
    CPU_INT08U   l2p_wr_octet;
    CPU_INT08U  *p_l2p_tbl_08;


    if (sec_nbr_phy == DEF_INT_32U_MAX_VAL) {
        sec_nbr_phy =  DEF_BIT_FIELD(p_nor_data->SecCntTotLog, 0u);
    }

    l2p_bit_off       = (CPU_INT32U)sec_nbr_logical * (CPU_INT32U)p_nor_data->SecCntTotLog;
    l2p_octet_off     =              l2p_bit_off / DEF_OCTET_NBR_BITS;
    l2p_octet_bit_off = (CPU_INT08U)(l2p_bit_off % DEF_OCTET_NBR_BITS);
    p_l2p_tbl_08      = (CPU_INT08U *)p_nor_data->L2P_Tbl + l2p_octet_off;
    bits_rem          =  p_nor_data->SecCntTotLog;


    while (bits_rem > 0u) {
        bits_wr             =  DEF_MIN(DEF_OCTET_NBR_BITS - l2p_octet_bit_off, bits_rem);

        l2p_rd_octet        = *p_l2p_tbl_08;                    /* Rd current tbl entry.                                */
        l2p_wr_octet        = (CPU_INT08U)(sec_nbr_phy << l2p_octet_bit_off) & DEF_OCTET_MASK;
        l2p_rd_octet       &= ~DEF_BIT_FIELD(bits_wr, l2p_octet_bit_off);
        l2p_wr_octet       &=  DEF_BIT_FIELD(bits_wr, l2p_octet_bit_off);
        l2p_wr_octet       |=  l2p_rd_octet;                    /* OR current entry with new bits...                    */
       *p_l2p_tbl_08        =  l2p_wr_octet;                    /*                               ... update tbl.        */
        p_l2p_tbl_08++;

        bits_rem           -= bits_wr;
        sec_nbr_phy       >>= bits_wr;
        l2p_octet_bit_off   = 0u;                               /* Next wr will be at start of next octet.              */
    }
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_DataFree()
*
* Description : Free a NOR data object.
*
* Argument(s) : p_nor_data  Pointer to a NOR data object.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_DataFree (FS_DEV_NOR_DATA  *p_nor_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_nor_data->NextPtr   = FSDev_NOR_ListFreePtr;
    FSDev_NOR_ListFreePtr = p_nor_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         FSDev_NOR_DataGet()
*
* Description : Allocate & initialize a NOR data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a NOR data object, if NO errors.
*               Pointer to NULL,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_NOR_DATA  *FSDev_NOR_DataGet (void)
{
    LIB_ERR               alloc_err;
    CPU_SIZE_T            octets_reqd;
    FS_DEV_NOR_DATA      *p_nor_data;
    FS_DEV_NOR_PHY_DATA  *p_phy_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC INFO ------------------- */
    p_nor_data = (FS_DEV_NOR_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_NOR_DATA),
                                                   sizeof(CPU_DATA),
                                                  &octets_reqd,
                                                  &alloc_err);
    if (p_nor_data == (FS_DEV_NOR_DATA *)0) {
        FS_TRACE_INFO(("FSDev_NOR_DataGet(): Could not alloc mem for NOR data: %d octets required.\r\n", octets_reqd));
        return ((FS_DEV_NOR_DATA *)0);
    }
    (void)alloc_err;

    p_phy_data = (FS_DEV_NOR_PHY_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_NOR_PHY_DATA),
                                                       sizeof(CPU_DATA),
                                                      &octets_reqd,
                                                      &alloc_err);
    if (p_phy_data == (FS_DEV_NOR_PHY_DATA *)0) {
        FS_TRACE_INFO(("FSDev_NOR_DataGet(): Could not alloc mem for phy NOR data: %d octets required.\r\n", octets_reqd));
        return ((FS_DEV_NOR_DATA *)0);
    }
    (void)alloc_err;

    CPU_CRITICAL_ENTER();
    FSDev_NOR_UnitCtr++;
    CPU_CRITICAL_EXIT();



                                                                /* ---------------------- CLR INFO -------------------- */

                                                                /* Clr dev open info.                                   */
    p_nor_data->Size              =  0u;
    p_nor_data->SecCntTot         =  0u;
    p_nor_data->SecCntTotLog      =  0u;
    p_nor_data->BlkCntUsed        =  0u;
    p_nor_data->BlkNbrFirst       =  0u;
    p_nor_data->BlkSizeLog        =  0u;
    p_nor_data->SecSizeLog        =  0u;
    p_nor_data->BlkSecCnts        =  0u;
    p_nor_data->AB_Cnt            =  0u;

                                                                /* Clr dev open data ptrs.                              */
    p_nor_data->BufPtr            = (void *)0;

                                                                /* Clr dev mount info.                                  */
    p_nor_data->BlkSecCntValidTbl = (void *)0;
    p_nor_data->BlkEraseMap       = (void *)0;
    p_nor_data->L2P_Tbl           = (void *)0;
    p_nor_data->AB_IxTbl          = (FS_SEC_NBR *)0;
    p_nor_data->AB_SecNextTbl     = (FS_SEC_NBR *)0;
    p_nor_data->BlkWearLevelAvail =  DEF_NO;
    p_nor_data->BlkCntValid       =  0u;
    p_nor_data->BlkCntErased      =  0u;
    p_nor_data->BlkCntInvalid     =  0u;
    p_nor_data->SecCntValid       =  0u;
    p_nor_data->SecCntErased      =  0u;
    p_nor_data->SecCntInvalid     =  0u;
    p_nor_data->EraseCntMin       =  0u;
    p_nor_data->EraseCntMax       =  0u;
    p_nor_data->Mounted           =  DEF_NO;

                                                                /* Clr cfg info.                                        */
    p_nor_data->AddrStart         =  0u;
    p_nor_data->DevSize           =  0u;
    p_nor_data->SecSize           =  0u;
    p_nor_data->PctRsvd           =  0u;
    p_nor_data->EraseCntDiffTh    =  0u;

                                                                /* Clr/set phy info.                                    */
    p_nor_data->PhyPtr            = (FS_DEV_NOR_PHY_API *)0;
    p_nor_data->PhyDataPtr        =  p_phy_data;

#if (FS_CFG_CTR_STAT_EN           == DEF_ENABLED)               /* Clr stat ctrs.                                       */
    p_nor_data->StatRdCtr         =  0u;
    p_nor_data->StatWrCtr         =  0u;
    p_nor_data->StatCopyCtr       =  0u;
    p_nor_data->StatReleaseCtr    =  0u;
    p_nor_data->StatRdOctetCtr    =  0u;
    p_nor_data->StatWrOctetCtr    =  0u;
    p_nor_data->StatEraseBlkCtr   =  0u;
    p_nor_data->StatInvalidBlkCtr =  0u;
#endif

#if (FS_CFG_CTR_ERR_EN            == DEF_ENABLED)               /* Clr err ctrs.                                        */
    p_nor_data->ErrRdCtr          =  0u;
    p_nor_data->ErrWrCtr          =  0u;
    p_nor_data->ErrEraseCtr       =  0u;
    p_nor_data->ErrWrFailCtr      =  0u;
    p_nor_data->ErrEraseFailCtr   =  0u;
#endif

    p_nor_data->NextPtr           = (FS_DEV_NOR_DATA *)0;

    return (p_nor_data);
}
