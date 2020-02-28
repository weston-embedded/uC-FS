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
*                                         FILE SYSTEM DEVICE DRIVER
*
*                                             NAND FLASH DEVICES
*
* Filename : fs_dev_nand.c
* Version  : V4.08.00
*
*********************************************************************************************************
* Note(s)  : (1) Supports NAND-type Flash memory devices, including :
*                (a) Parallel NAND Flash.
*                    (1) Small-page (512-B) SLC Devices.
*                    (2) Large-page (2048-, 4096-, 8096-B) SLC Devices.
*                    (3) Some MLC Devices.
*
*            (2) Supported media MUST have the following characteristics :
*                (a) Medium organized into units (called blocks) which are the smallest erasable unit.
*                (b) When erased, all bits are set to 1.
*                (c) Only an erase operation can change a bit from 0 to 1.
*                (d) Each block divided into smaller units (called pages) which are the smallest
*                    writable unit (unless partial-page programming is supported): each page has a
*                    data area as well as a spare area. Spare area requirement are determined
*                    at initialization time depending on different parameters. If the spare area
*                    is insufficient for the parameters, the function FSDev_Open() will return
*                    the error code FS_ERR_DEV_INVALID_LOW_PARAMS.
*
*            (3) Supported media TYPICALLY have the following characteristics :
*                (a) A  program operation takes much longer than a read    operation.
*                (b) An erase   operation takes much longer than a program operation.
*                (c) The number of erase operations per block is limited.
*
*            (4) Logical block indexes are mapped to data blocks but also to update and metadata
*                blocks.
*                (a) Logical indexes 0 to (NUMBER OF LOGICAL DATA BLOCKS - 1) are mapped to logical data
*                    blocks 0 to (NUMBER OF LOGICAL DATA BLOCKS).
*                (b) Logical indexes (NUMBER OF LOGICAL DATA BLOCKS) to (NUMBER OF LOGICAL DATA BLOCKS
*                    + NUMBER OF UPDATE BLOCKS - 1) are mapped to update blocks 0 to (NUMBER OF UPDATE
*                    BLOCKS - 1)
*                (c) Logical index (NUMBER OF LOGICAL DATA BLOCKS + NUMBER OF UPDATE BLOCK) is mapped
*                    to active metadata block.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE

#ifndef  FS_NAND_INTERN_ACCESS
#define  FS_NAND_INTERN_ACCESS  DEF_NO
#endif

#ifdef   FS_NAND_INTERN_ACCESS_EN
    #if (FS_NAND_INTERN_ACCESS == DEF_NO)
        #define  FS_NAND_MODULE
    #endif
#else
    #define FS_NAND_MODULE
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/crc_util.h>
#include  <lib_def.h>
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  <fs_dev_nand_cfg.h>
#include  "../../Source/fs.h"
#include  "../../Source/fs_dev.h"
#include  "../../Source/fs_ctr.h"
#include  "../../Source/fs_util.h"
#include  "fs_dev_nand.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_NAND_DRV_NAME_LEN                          4u

#define  FS_NAND_HDR_BLK_NBR                           1u
#define  FS_NAND_VALID_META_BLK_NBR                    1u

#define  FS_NAND_META_SEQ_QTY_HALF_RANGE        (DEF_GET_U_MAX_VAL(FS_NAND_META_ID) >> 1u)

#define  FS_NAND_MAP_SRCH_SIZE_OCTETS                  4u
#define  FS_NAND_MAP_SRCH_SIZE_BITS             (DEF_OCTET_NBR_BITS * FS_NAND_MAP_SRCH_SIZE_OCTETS)

                                                                /* NAND data types 'undefined' val.                     */
#define  FS_NAND_BLK_IX_INVALID                  DEF_GET_U_MAX_VAL(FS_NAND_BLK_QTY)
#define  FS_NAND_UB_IX_INVALID                   DEF_GET_U_MAX_VAL(FS_NAND_UB_QTY)
#define  FS_NAND_SEC_OFFSET_IX_INVALID           DEF_GET_U_MAX_VAL(FS_NAND_SEC_PER_BLK_QTY)
#define  FS_NAND_ERASE_QTY_INVALID               DEF_GET_U_MAX_VAL(FS_NAND_ERASE_QTY)
#define  FS_NAND_ASSOC_BLK_IX_INVALID            DEF_GET_U_MAX_VAL(FS_NAND_ASSOC_BLK_QTY)

#define  FS_NAND_META_ID_STALE_THRESH           (DEF_GET_U_MAX_VAL(FS_NAND_META_ID) / 4u)


/*
*********************************************************************************************************
*                                          INTERN API ACCESS
*
* Note(s) : (1) To access internal api, the compiler needs to define the "FS_NAND_INTERN_ACCESS_EN"
*               symbol. The user of the internal api must also #define FS_NAND_INTERN_ACCESS to
*               DEF_YES prior to including "fs_dev_nand.c" (note the ".c").
*
*           (2) A translation unit without FS_NAND_INTERN_ACCESS equal to DEF_YES must be compiled in
*               order to create the global symbols (Functions and global variables).
*********************************************************************************************************
*/

#ifdef   FS_NAND_INTERN_ACCESS_EN
    #define  FS_NAND_INTERN
    #if     (FS_NAND_INTERN_ACCESS == DEF_YES)
        #define  FS_NAND_INTERN_API_EN  DEF_DISABLED
    #else
        #define  FS_NAND_INTERN_API_EN  DEF_ENABLED
    #endif
#else
    #define  FS_NAND_INTERN             static
    #define  FS_NAND_INTERN_API_EN      DEF_ENABLED
#endif


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT32U            FS_NAND_ERASE_QTY;
typedef  CPU_INT08U            FS_NAND_META_SEC_QTY;
typedef  CPU_INT16U            FS_NAND_META_ID;
typedef  CPU_INT08U            FS_NAND_META_SEQ_STATUS_STO;


typedef  struct  fs_nand_data  FS_NAND_DATA;

typedef  struct  fs_nand_avail_blk_entry {
    FS_NAND_BLK_QTY    BlkIxPhy;
    FS_NAND_ERASE_QTY  EraseCnt;
} FS_NAND_AVAIL_BLK_ENTRY;

typedef  struct  fs_nand_ub_data {
    FS_NAND_BLK_QTY   BlkIxPhy;
    CPU_INT08U       *SecValidBitMap;
} FS_NAND_UB_DATA;


/*
*********************************************************************************************************
*                                         UB EXTRA DATA TYPE
*
* Note(s) : (1) Contains data related to update blocks that is not stored in metadata sectors. Recreated
*               on mount using stored metadata and actual update block sector out-of-sector data.
*********************************************************************************************************
*/

typedef  struct  fs_nand_ub_extra_data {
    FS_NAND_BLK_QTY          *AssocLogicalBlksTbl;              /* Logical blks associated with UB.                     */
    CPU_INT08U               *LogicalToPhySecMap;               /* Map linking logical sec to phy sec.                  */
    FS_NAND_ASSOC_BLK_QTY     AssocLvl;                         /* Assoc lvl, nbr of associated logical blks.           */
    FS_NAND_SEC_PER_BLK_QTY   NextSecIx;                        /* Ix of next empty sec of UB.                          */
    CPU_INT16U                ActivityCtr;                      /* Ctr updated on wr. Allows idleness calc.             */
#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
    CPU_INT08U               *MetaCachePtr;                     /* Optional cache of meta.                              */
#endif
} FS_NAND_UB_EXTRA_DATA;


/*
*********************************************************************************************************
*                                         UB SEC DATA TYPE
*
* Note(s) : (1) Contains data on location of a specific logical sector in an update block.
*********************************************************************************************************
*/

typedef  struct  fs_nand_ub_sec_data {
    FS_NAND_UB_QTY            UB_Ix;                            /* UB ix.                                               */
    FS_NAND_ASSOC_BLK_QTY     AssocLogicalBlksTblIx;            /* Logical blk's location in assoc logical blks tbl.    */
    FS_NAND_SEC_PER_BLK_QTY   SecOffsetPhy;                     /* Phy offset of sec in UB.                             */
} FS_NAND_UB_SEC_DATA;


/*
*********************************************************************************************************
*                                     NAND DEVICE DATA DATA TYPE
*********************************************************************************************************
*/

struct  fs_nand_data {
                                                                /* -------------------- SIZE INFO --------------------- */
    FS_NAND_BLK_QTY           BlkCnt;                           /* Total blk cnt.                                       */
    FS_NAND_BLK_QTY           BlkIxFirst;                       /* Ix of first blk to be used.                          */
    CPU_SIZE_T                SecSize;                          /* Sec size in octets.                                  */
    CPU_SIZE_T                SecCnt;                           /* Logical sec cnt.                                     */
    FS_NAND_SEC_PER_BLK_QTY   NbrSecPerBlk;                     /* Nbr of sec per blk.                                  */
    CPU_INT08U                NbrSecPerBlkLog2;                 /* Log2 of nbr of sec per blk.                          */
    FS_NAND_BLK_QTY           LogicalDataBlkCnt;                /* Nbr of logical data blks.                            */
    CPU_INT08U                UsedMarkSize;                     /* Size of used mark in octets.                         */

                                                                /* -------------------- STATE INFO -------------------- */
    CPU_BOOLEAN               Fmtd;                             /* Flag to indicate low level fmt validity.             */
    CPU_INT16U                ActivityCtr;                      /* Activity ctr.                                        */

                                                                /* ---------------------- METADATA -------------------- */
    CPU_INT08U               *MetaCache;                        /* Cached copy of metadata stored on dev.               */
    CPU_INT16U                MetaSize;                         /* Total size in octets of meta.                        */
    CPU_INT08U                MetaSecCnt;                       /* Total size in sec    of meta.                        */
    FS_NAND_META_ID           MetaBlkID;                        /* Sequential ID of current metadata block.             */

    CPU_INT16U                MetaOffsetBadBlkTbl;              /* Offset in octets of bad   blk tbl in meta.           */
    CPU_INT16U                MetaOffsetAvailBlkTbl;            /* Offset in octets of avail blk tbl in meta.           */
    CPU_INT16U                MetaOffsetDirtyBitmap;            /* Offset in octets of dirty bitmap  in meta.           */
    CPU_INT16U                MetaOffsetUB_Tbl;                 /* Offset in octets of UB tbl        in meta.           */

                                                                /* ----------------- METADATA BLK INFO ---------------- */
    FS_NAND_BLK_QTY           MetaBlkIxPhy;                     /* Phy blk ix of meta blk.                              */
    CPU_BOOLEAN               MetaBlkFoldNeeded;                /* Flag to indicate needed meta blk fold is needed.     */
    CPU_INT08U               *MetaBlkInvalidSecMap;             /* Bitmap of invalid sec in meta blk.                   */
    FS_NAND_SEC_PER_BLK_QTY   MetaBlkNextSecIx;                 /* Ix of next sec to use in meta blk.                   */

                                                                /* ------------------- DIRTY BITMAP ------------------- */
    FS_NAND_BLK_QTY           DirtyBitmapSrchPos;               /* Cur srch pos in dirty bitmap.                        */

                                                                /* ---------------- DIRTY BITMAP CACHE ---------------- */
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN == DEF_ENABLED)
    CPU_INT08U               *DirtyBitmapCache;                 /* Intact copy of dirty bitmap stored on dev.           */
    CPU_INT16U                DirtyBitmapSize;                  /* Total size in octets for dirty bitmap.               */
#endif

                                                                /* ---------------- AVAIL BLK TBL INFO ---------------- */
    CPU_BOOLEAN               AvailBlkTblInvalidated;           /* Flag actived when avail blk tbl changes.             */
    CPU_INT08U               *AvailBlkTblCommitMap;             /* Bitmap indicating commit state of avail blks.        */
    FS_NAND_BLK_QTY           AvailBlkTblEntryCntMax;           /* Nbr of entries in avail blk tbl.                     */
    CPU_INT08U               *AvailBlkMetaMap;                  /* Bitmap indicating blks that contain metadata.        */
    FS_NAND_META_ID          *AvailBlkMetaID_Tbl;               /* Available metadata block ID table.                   */

                                                                /* ---------------- UNPACKED METADATA ----------------- */
    FS_NAND_BLK_QTY          *LogicalToPhyBlkMap;               /* Logical to phy blk ix map.                           */

                                                                /* --------------------- UB info ---------------------- */
    FS_NAND_UB_QTY            UB_CntMax;                        /* Max nbr of UBs.                                      */
    CPU_INT08U                UB_SecMapNbrBits;                 /* Resolution in bits of UB sec mapping.                */
    FS_NAND_UB_EXTRA_DATA    *UB_ExtraDataTbl;                  /* UB extra data tbl.                                   */
    FS_NAND_UB_QTY            SUB_Cnt;                          /* Nbr of SUBs.                                         */
    FS_NAND_UB_QTY            SUB_CntMax;                       /* Max nbr of SUBs.                                     */
    FS_NAND_ASSOC_BLK_QTY     RUB_MaxAssoc;                     /* Max assoc of RUBs.                                   */
    CPU_INT08U                RUB_MaxAssocLog2;                 /* Log2 of max assoc of RUBs.                           */

                                                                /* ------------------ UB TH PARAMS -------------------- */
                                                                /* See FS_NAND_SecWrInUB() note #2.                     */
    FS_NAND_SEC_PER_BLK_QTY   ThSecWrCnt_MergeRUBStartSUB;      /* Th to start SUB when RUB exists.                     */
    FS_NAND_SEC_PER_BLK_QTY   ThSecRemCnt_ConvertSUBToRUB;      /* Th to convert a SUB to a RUB.                        */
    FS_NAND_SEC_PER_BLK_QTY   ThSecGapCnt_PadSUB;               /* Th to pad a SUB.                                     */

                                                                /* See FS_NAND_UB_Alloc() note #2.                      */
    FS_NAND_SEC_PER_BLK_QTY   ThSecRemCnt_MergeSUB;             /* Th to merge SUB instead of RUB.                      */


                                                                /* -------------------- CTRLR INFO -------------------- */
    FS_NAND_CTRLR_API        *CtrlrPtr;                         /* Ptr to ctrlr api.                                    */
    void                     *CtrlrDataPtr;                     /* Ptr to ctrlr data.                                   */

                                                                /* -------------------- PART INFO --------------------- */
    FS_NAND_PART_DATA        *PartDataPtr;                      /* Ptr to part data.                                    */

                                                                /* ---------------------- BUFFERS --------------------- */
    void                     *BufPtr;                           /* Buffer for sec data.                                 */
    void                     *OOS_BufPtr;                       /* Buffer for OOS data.                                 */

#if ((FS_CFG_CTR_STAT_EN == DEF_ENABLED) || \
     (FS_CFG_CTR_ERR_EN  == DEF_ENABLED))
    FS_NAND_CTRS              Ctrs;                             /* Stat and err ctrs. */
#endif

    FS_NAND_DATA             *NextPtr;
};


/*
*********************************************************************************************************
*                                         NAND HEADER DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT32U   FS_NAND_HDR_MARK_1_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_MARK_2_TYPE;
typedef  CPU_INT16U   FS_NAND_HDR_VER_TYPE;
typedef  FS_SEC_SIZE  FS_NAND_HDR_SEC_SIZE_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_BLK_CNT_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_BLK_NBR_FIRST_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_UB_CNT_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_MAX_ASSOC_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE;
typedef  CPU_SIZE_T   FS_NAND_HDR_OOS_SIZE_TYPE;
typedef  CPU_INT32U   FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE;


/*
*********************************************************************************************************
*                                          NAND OOS DEFINES
*
* Note(s) : (1) FS_NAND_SEC_TYPE_STO must be able to store all the enum values from FS_NAND_SEC_TYPE.
*
*               (a) FS_NAND_SEC_TYPE_STO contains the mark that will be written on the device. To save
*                   space on device, the type chosen must be as small as possible.
*
*               (b) FS_NAND_SEC_TYPE uses enum values to facilitate debugging.
*
*********************************************************************************************************
*/

typedef  enum  fs_nand_sec_type {                               /* Sec types. See note #1.                              */
    FS_NAND_SEC_TYPE_INVALID             = 0x00u,
    FS_NAND_SEC_TYPE_UNUSED              = 0xFFu,
    FS_NAND_SEC_TYPE_STORAGE             = 0xF0u,
    FS_NAND_SEC_TYPE_METADATA            = 0x0Fu,
    FS_NAND_SEC_TYPE_HDR                 = 0xAAu,
    FS_NAND_SEC_TYPE_DUMMY               = 0x55u
} FS_NAND_SEC_TYPE;

typedef  CPU_INT08U  FS_NAND_SEC_TYPE_STO;


                                                                /* Seq statuses.                                        */
typedef  enum  fs_nand_meta_seq_status {
    FS_NAND_META_SEQ_NEW                 = 0xAAu,
    FS_NAND_META_SEQ_UNFINISHED          = 0xFFu,
    FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY  = 0xF0u,
    FS_NAND_META_SEQ_FINISHED            = 0x00u
} FS_NAND_META_SEQ_STATUS;


/*
*********************************************************************************************************
*                                       NAND OOS USAGE DEFINES
*********************************************************************************************************
*/

enum  fs_nand_oos_structure {
                                                                /* Common to all blk types.                             */
    FS_NAND_OOS_SEC_TYPE_OFFSET             =  0u,
    FS_NAND_OOS_ERASE_CNT_OFFSET            =  FS_NAND_OOS_SEC_TYPE_OFFSET           + sizeof(FS_NAND_SEC_TYPE_STO),

                                                                /* Specific to sto blk.                                 */
    FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET   =  FS_NAND_OOS_ERASE_CNT_OFFSET          + sizeof(FS_NAND_ERASE_QTY),
    FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET       =  FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET + sizeof(FS_NAND_BLK_QTY),
    FS_NAND_OOS_STO_SEC_SIZE_REQD           =  FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET     + sizeof(FS_NAND_SEC_PER_BLK_QTY),

                                                                /* Specific to metadata blk.                            */
    FS_NAND_OOS_META_SEC_IX_OFFSET          =  FS_NAND_OOS_ERASE_CNT_OFFSET          + sizeof(FS_NAND_ERASE_QTY),
    FS_NAND_OOS_META_ID_OFFSET              =  FS_NAND_OOS_META_SEC_IX_OFFSET        + sizeof(FS_NAND_META_SEC_QTY),
    FS_NAND_OOS_META_SEQ_STATUS_OFFSET      =  FS_NAND_OOS_META_ID_OFFSET            + sizeof(FS_NAND_META_ID),
    FS_NAND_OOS_META_SEC_SIZE_REQD          =  FS_NAND_OOS_META_SEQ_STATUS_OFFSET    + sizeof(FS_NAND_META_SEQ_STATUS_STO),

                                                                /* Size does not include used mark.                     */
    FS_NAND_OOS_PARTIAL_SIZE_REQD           =  DEF_MAX(FS_NAND_OOS_META_SEC_SIZE_REQD, FS_NAND_OOS_STO_SEC_SIZE_REQD),

    FS_NAND_OOS_SEC_USED_OFFSET             =  FS_NAND_OOS_PARTIAL_SIZE_REQD
};


/*
*********************************************************************************************************
*                                          NAND HDR DEFINES
*
* Note(s) : (1) The flash translation layer (FTL) version number is not equal to the filesystem or driver
*               version number. When the FTL version number is incremented, it means the changes are
*               different enough for previous formats not to be compatible, and that the device must be
*               low-level formatted before further use.
*********************************************************************************************************
*/

                                                                /* ------------------- HDR OFFSETS -------------------- */
enum  FS_NAND_HDR_STRUCTURE {
         FS_NAND_HDR_MARK_1_OFFSET             = 0u,
         FS_NAND_HDR_MARK_2_OFFSET             = FS_NAND_HDR_MARK_1_OFFSET
                                               + sizeof(FS_NAND_HDR_MARK_1_TYPE),

         FS_NAND_HDR_VER_OFFSET                = FS_NAND_HDR_MARK_2_OFFSET
                                               + sizeof(FS_NAND_HDR_MARK_2_TYPE),

                                                            /* Low lvl cfg.                                         */
         FS_NAND_HDR_SEC_SIZE_OFFSET           = FS_NAND_HDR_VER_OFFSET
                                               + sizeof(FS_NAND_HDR_VER_TYPE),

         FS_NAND_HDR_BLK_CNT_OFFSET            = FS_NAND_HDR_SEC_SIZE_OFFSET
                                               + sizeof(FS_NAND_HDR_SEC_SIZE_TYPE),

         FS_NAND_HDR_BLK_NBR_FIRST_OFFSET      = FS_NAND_HDR_BLK_CNT_OFFSET
                                               + sizeof(FS_NAND_HDR_BLK_CNT_TYPE),

         FS_NAND_HDR_UB_CNT_OFFSET             = FS_NAND_HDR_BLK_NBR_FIRST_OFFSET
                                               + sizeof(FS_NAND_HDR_BLK_NBR_FIRST_TYPE),

         FS_NAND_HDR_MAX_ASSOC_OFFSET          = FS_NAND_HDR_UB_CNT_OFFSET
                                               + sizeof(FS_NAND_HDR_UB_CNT_TYPE),

         FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_OFFSET = FS_NAND_HDR_MAX_ASSOC_OFFSET
                                               + sizeof(FS_NAND_HDR_MAX_ASSOC_TYPE),

                                                            /* Part data.                                           */
         FS_NAND_HDR_OOS_SIZE_OFFSET           = FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_OFFSET
                                               + sizeof(FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE),

         FS_NAND_HDR_MAX_BAD_BLK_CNT_OFFSET    = FS_NAND_HDR_OOS_SIZE_OFFSET
                                               + sizeof(FS_NAND_HDR_OOS_SIZE_TYPE),

                                                            /* Total size.                                          */
         FS_NAND_HDR_TOTAL_SIZE                = FS_NAND_HDR_MAX_BAD_BLK_CNT_OFFSET
                                               + sizeof(FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE)
};

                                                                /* ------------------- HDR CONSTS --------------------- */
enum  FS_NAND_HDR_CONSTS {
         FS_NAND_HDR_MARK_WORD1               = 0x534643E6u,    /* Marker word 1 ("  CFS")                               */
         FS_NAND_HDR_MARK_WORD2               = 0x444E414Eu,    /* Marker word 2 ("NAND")                               */
         FS_NAND_HDR_VER                      =     0x0001u     /* FTL version (1) (See Note #1).                       */
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  const  CPU_CHAR  FS_NAND_DrvName[] = "nand";


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

static  FS_NAND_DATA       *FS_NAND_ListFreePtr;
static  FS_NAND_CTRLR_API  *FS_NAND_ListCtrlrImpl[FS_NAND_CFG_MAX_CTRLR_IMPL];

#if (FS_NAND_INTERN_API_EN == DEF_ENABLED)
#define  FS_NAND_CFG_FIELD(type, name, dflt_val) dflt_val,
const  FS_NAND_CFG  FS_NAND_DfltCfg = { FS_NAND_CFG_FIELDS };
#undef   FS_NAND_CFG_FIELD
#endif

/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*
* Note(s) : (1) FS_NAND_BLK_IX_TO_SEC_IX(), FS_NAND_SEC_IX_TO_BLK_IX() & FS_NAND_UB_IX_TO_LOG_BLK_IX
*               require p_nand_data pointer to be not NULL.
*********************************************************************************************************
*/

#define  FS_NAND_BLK_IX_TO_SEC_IX(   p_nand_data, blk_ix)    (FS_SEC_QTY     )(((FS_SEC_QTY)blk_ix) << (p_nand_data)->NbrSecPerBlkLog2)
#define  FS_NAND_SEC_IX_TO_BLK_IX(   p_nand_data, sec_ix)    (FS_NAND_BLK_QTY)((sec_ix) >> (p_nand_data)->NbrSecPerBlkLog2)

#define  FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix)     (FS_NAND_BLK_QTY)((ub_ix)   + (p_nand_data)->LogicalDataBlkCnt)


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* --------------- DEV INTERFACE FNCTS ---------------- */
                                                                /* Get name of driver.                                  */
FS_NAND_INTERN  const  CPU_CHAR         *FS_NAND_NameGet               (void);

                                                                /* Init driver.                                         */


FS_NAND_INTERN  void                     FS_NAND_Init                  (FS_ERR                   *p_err);

                                                                /* Open device.                                         */
FS_NAND_INTERN  void                     FS_NAND_Open                  (FS_DEV                   *p_dev,
                                                                        void                     *p_dev_cfg,
                                                                        FS_ERR                   *p_err);

                                                                /* Close device.                                        */
FS_NAND_INTERN  void                     FS_NAND_Close                 (FS_DEV                   *p_dev);

                                                                /* Read from device.                                    */
FS_NAND_INTERN  void                     FS_NAND_Rd                    (FS_DEV                   *p_dev,
                                                                        void                     *p_dest,
                                                                        FS_SEC_NBR                sec_start,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Write to device.                                     */
FS_NAND_INTERN  void                     FS_NAND_Wr                    (FS_DEV                   *p_dev,
                                                                        void                     *p_src,
                                                                        FS_SEC_NBR                sec_start,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Get device info.                                     */
FS_NAND_INTERN  void                     FS_NAND_Query                 (FS_DEV                   *p_dev,
                                                                        FS_DEV_INFO              *p_info,
                                                                        FS_ERR                   *p_err);

                                                                /* Perform device I/O control.                          */
FS_NAND_INTERN  void                     FS_NAND_IO_Ctrl               (FS_DEV                   *p_dev,
                                                                        CPU_INT08U                opt,
                                                                        void                     *p_data,
                                                                        FS_ERR                   *p_err);


                                                                /* ----------------- LOCAL FUNCTIONS ------------------ */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Erase NAND chip.                                     */
FS_NAND_INTERN  void                     FS_NAND_ChipEraseHandler      (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Low-level fmt NAND.                                  */
FS_NAND_INTERN  void                     FS_NAND_LowFmtHandler         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Low-level mount NAND.                                */
FS_NAND_INTERN  void                     FS_NAND_LowMountHandler       (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Low-level unmount NAND.                              */
FS_NAND_INTERN  void                     FS_NAND_LowUnmountHandler     (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

#if (FS_NAND_CFG_DUMP_SUPPORT_EN == DEF_ENABLED)                /* Dump raw NAND device.                                */
FS_NAND_INTERN  void                     FS_NAND_DumpHandler           (FS_NAND_DATA             *p_nand_data,
                                                                        void                    (*dump_fnct)(void        *buf,
                                                                                                             CPU_SIZE_T   buf_len,
                                                                                                             FS_ERR      *p_err),
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Rd sec in logical blk.                               */
FS_NAND_INTERN  void                     FS_NAND_SecRdHandler          (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_dest,
                                                                        void                     *p_dest_oos,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Rd sec in logical blk w/o refresh.                   */
FS_NAND_INTERN  void                     FS_NAND_SecRdPhyNoRefresh     (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_dest,
                                                                        void                     *p_dest_oos,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Rd 1 or more sec.                                    */
FS_NAND_INTERN  FS_SEC_QTY               FS_NAND_SecRd                 (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_dest,
                                                                        FS_SEC_QTY                sec_ix_logical,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Wr data sec.                                         */
FS_NAND_INTERN  void                     FS_NAND_SecWrHandler          (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_src,
                                                                        void                     *p_src_oos,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr meta sec.                                         */
FS_NAND_INTERN  void                     FS_NAND_MetaSecWrHandler      (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_src,
                                                                        void                     *p_src_oos,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr sec on dev.                                       */
FS_NAND_INTERN  FS_SEC_QTY               FS_NAND_SecWr                 (FS_NAND_DATA            *p_nand_data,
                                                                        void                     *p_src,
                                                                        FS_SEC_QTY                sec_ix_logical,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr sec in UB.                                        */
FS_NAND_INTERN  FS_SEC_QTY               FS_NAND_SecWrInUB             (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_src,
                                                                        FS_SEC_QTY                sec_ix_logical,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_NAND_UB_SEC_DATA       ub_sec_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr sec in RUB.                                       */
FS_NAND_INTERN  FS_SEC_QTY               FS_NAND_SecWrInRUB            (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_src,
                                                                        FS_SEC_QTY                sec_ix_logical,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr sec in SUB.                                       */
FS_NAND_INTERN  FS_SEC_QTY               FS_NAND_SecWrInSUB            (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_src,
                                                                        FS_SEC_QTY                sec_ix_logical,
                                                                        FS_SEC_QTY                sec_cnt,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_ERR                   *p_err);

                                                                /* Generate OOS data for sto sec.                       */
FS_NAND_INTERN  void                     FS_NAND_OOSGenSto             (FS_NAND_DATA             *p_nand_data,
                                                                        void                     *p_oos_buf_v,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Blk erase.                                           */
FS_NAND_INTERN  void                     FS_NAND_BlkEraseHandler       (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Wr dev hdr.                                          */
FS_NAND_INTERN  void                     FS_NAND_HdrWr                 (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_hdr,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Rd dev hdr.                                          */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_HdrRd                 (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Validate hdr params.                                 */
FS_NAND_INTERN  void                     FS_NAND_HdrParamsValidate     (FS_NAND_DATA             *p_nand_data,
                                                                        CPU_INT08U               *p_hdr_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Get phy ix from logical ix.                          */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_BlkIxPhyGet           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Refresh blk.                                         */
FS_NAND_INTERN  void                     FS_NAND_BlkRefresh            (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Mark blk as bad.                                     */
FS_NAND_INTERN  void                     FS_NAND_BlkMarkBad            (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Mark blk as dirty.                                   */
FS_NAND_INTERN  void                     FS_NAND_BlkMarkDirty          (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Unmap blk.                                           */
FS_NAND_INTERN  void                     FS_NAND_BlkUnmap              (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Add blk to avail blk tbl.                            */
FS_NAND_INTERN  void                     FS_NAND_BlkAddToAvail         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Rem blk from avail blk tbl.                          */
FS_NAND_INTERN  FS_NAND_ERASE_QTY        FS_NAND_BlkRemFromAvail       (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Get blk from avail blk tbl.                          */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_BlkGetAvailFromTbl    (FS_NAND_DATA             *p_nand_data,
                                                                        CPU_BOOLEAN               access_rsvd,
                                                                        FS_ERR                   *p_err);

                                                                /* Get blk with committed dirty state.                  */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_BlkGetDirty           (FS_NAND_DATA             *p_nand_data,
                                                                        CPU_BOOLEAN               pending_dirty_chk_en,
                                                                        FS_ERR                   *p_err);

                                                                /* Get an avail, erased blk.                            */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_BlkGetErased          (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Make sure blk is erased.                             */
FS_NAND_INTERN  void                     FS_NAND_BlkEnsureErased       (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Chk if blk is bad according to bad blk tbl.          */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_BlkIsBad              (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Chk if blk is dirty according to dirty bitmap.       */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_BlkIsDirty            (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Chk if blk is avail according to avail blk tbl.      */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_BlkIsAvail            (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Chk if blk is an UB.                                 */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_BlkIsUB               (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Chk if blk is a factory defect.                      */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_BlkIsFactoryDefect    (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Finds the latest meta blk on dev.                    */
FS_NAND_INTERN  void                     FS_NAND_MetaBlkFind           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_hdr,
                                                                        FS_ERR                   *p_err);

                                                                /* Finds a blk with a specific meta seq on dev.         */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_MetaBlkFindID         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_META_ID           meta_blk_id,
                                                                        FS_ERR                   *p_err);

                                                                /* Parse meta blk.                                      */
FS_NAND_INTERN  void                     FS_NAND_MetaBlkParse          (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Parse avail blk tbl temp commits.                    */
FS_NAND_INTERN  void                     FS_NAND_MetaBlkAvailParse     (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Fold current meta blk and start a new one.           */
FS_NAND_INTERN  void                     FS_NAND_MetaBlkFold           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);
#endif

#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED)
                                                                /* Find meta sec.                                       */
FS_NAND_INTERN  FS_NAND_SEC_PER_BLK_QTY  FS_NAND_MetaSecFind           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_META_SEC_QTY      meta_sec_ix,
                                                                        FS_ERR                   *p_err);
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Invalidate meta sec.                                 */
FS_NAND_INTERN  void                     FS_NAND_MetaSecRangeInvalidate(FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_ix_first,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_ix_last);

                                                                /* Commit meta.                                         */
FS_NAND_INTERN  void                     FS_NAND_MetaCommit            (FS_NAND_DATA             *p_nand_data,
                                                                        CPU_BOOLEAN               avail_tbl_only,
                                                                        FS_ERR                   *p_err);

                                                                /* Commit single meta sec.                              */
FS_NAND_INTERN  void                     FS_NAND_MetaSecCommit         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_META_SEC_QTY      meta_sec_ix,
                                                                        FS_NAND_META_SEQ_STATUS   seq_status,
                                                                        FS_ERR                   *p_err);

                                                                /* Gather metadata sec data.                            */
FS_NAND_INTERN  void                     FS_NAND_MetaSecGatherData     (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_META_SEC_QTY      meta_sec_ix,
                                                                        CPU_INT08U               *p_buf);

#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
                                                                /* Detects possible metadata corruption.                */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_MetaBlkCorruptDetect  (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_hdr,
                                                                        FS_ERR                   *p_err);
                                                                /* Erase old metadata blocks to recover from corruption.*/
FS_NAND_INTERN  void                     FS_NAND_MetaBlkCorruptedErase (FS_NAND_DATA              *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_hdr,
                                                                        FS_ERR                    *p_err);
#endif

                                                                /* Commit a tmp avail blk tbl to device.                */
FS_NAND_INTERN  void                     FS_NAND_AvailBlkTblTmpCommit  (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Rd entry in avail blk tbl.                           */
FS_NAND_INTERN  FS_NAND_AVAIL_BLK_ENTRY  FS_NAND_AvailBlkTblEntryRd    (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           tbl_ix);

#if  (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Wr entry in avail blk tbl.                           */
FS_NAND_INTERN  void                     FS_NAND_AvailBlkTblEntryWr    (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           tbl_ix,
                                                                        FS_NAND_AVAIL_BLK_ENTRY   entry);

                                                                /* Fill avail blk tbl.                                  */
FS_NAND_INTERN  void                     FS_NAND_AvailBlkTblFill       (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           nbr_entries_min,
                                                                        CPU_BOOLEAN               pending_dirty_chk_en,
                                                                        FS_ERR                   *p_err);

                                                                /* Cnt entries in avail blk tbl.                        */
FS_NAND_INTERN  FS_NAND_BLK_QTY          FS_NAND_AvailBlkTblEntryCnt (FS_NAND_DATA             *p_nand_data);
#endif

#if  (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Invalidate avail blk tbl.                            */
FS_NAND_INTERN  void                     FS_NAND_AvailBlkTblInvalidate (FS_NAND_DATA             *p_nand_data);
#endif

                                                                /* Rd UB tbl entry.                                     */
FS_NAND_INTERN  FS_NAND_UB_DATA          FS_NAND_UB_TblEntryRd         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            tbl_ix);

#if  (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Wr UB tbl entry.                                     */
FS_NAND_INTERN  void                     FS_NAND_UB_TblEntryWr         (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            tbl_ix,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy);

                                                                /* Invalidate UB tbl.                                   */
FS_NAND_INTERN  void                     FS_NAND_UB_TblInvalidate      (FS_NAND_DATA             *p_nand_data);
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Create a new UB.                                     */
FS_NAND_INTERN  void                     FS_NAND_UB_Create             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_ERR                   *p_err);
#endif

                                                                /* Load UB data from metadata cache and dev.            */
FS_NAND_INTERN  void                     FS_NAND_UB_Load               (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_phy,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Clear UB.                                            */
FS_NAND_INTERN  void                     FS_NAND_UB_Clr                (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix);
#endif

                                                                /* Find UB associated with logical blk.                 */  /* Why does it return info about sec?? */
FS_NAND_INTERN  FS_NAND_UB_SEC_DATA      FS_NAND_UB_Find               (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical);

                                                                /* Finds specified sector in UB.                        */
FS_NAND_INTERN  FS_NAND_UB_SEC_DATA      FS_NAND_UB_SecFind            (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_SEC_DATA       ub_sec_data,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Associate logical blk with UB.                       */  /* Why not Assoc instead of IncAssoc */
FS_NAND_INTERN  void                     FS_NAND_UB_IncAssoc           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical);
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Alloc a new UB.                                      */
FS_NAND_INTERN  FS_NAND_UB_QTY           FS_NAND_UB_Alloc              (FS_NAND_DATA             *p_nand_data,
                                                                        CPU_BOOLEAN               sequential,
                                                                        FS_ERR                   *p_err);

                                                                /* Alloc a new RUB.                                    */
FS_NAND_INTERN  FS_NAND_UB_QTY           FS_NAND_RUB_Alloc             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical,
                                                                        FS_ERR                   *p_err);

                                                                /* Perform full merge of RUB.                           */
FS_NAND_INTERN  void                     FS_NAND_RUB_Merge             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_ERR                   *p_err);

                                                                /* Perform partial merge of RUB for logical blk.        */
FS_NAND_INTERN  void                     FS_NAND_RUB_PartialMerge      (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_NAND_BLK_QTY           data_blk_ix_logical,
                                                                        FS_ERR                   *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* Get assoc blk ix for given UB and logical blk.       */
FS_NAND_INTERN  FS_NAND_ASSOC_BLK_QTY    FS_NAND_RUB_AssocBlkIxGet     (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            ub_ix,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical);
#endif

                                                                /* Allocate new SUB.                                    */
FS_NAND_INTERN  FS_NAND_UB_QTY           FS_NAND_SUB_Alloc             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_BLK_QTY           blk_ix_logical,
                                                                        FS_ERR                   *p_err);

                                                                /* Merge specified SUB.                                 */
FS_NAND_INTERN  void                     FS_NAND_SUB_Merge             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            sub_ix,
                                                                        FS_ERR                   *p_err);

                                                                /* Merge specified SUB until specified sec.             */
FS_NAND_INTERN  void                     FS_NAND_SUB_MergeUntil        (FS_NAND_DATA             *p_nand_data,
                                                                        FS_NAND_UB_QTY            sub_ix,
                                                                        FS_NAND_SEC_PER_BLK_QTY   sec_end,
                                                                        FS_ERR                   *p_err);

                                                                /* Invalidate dirty bitmap.                             */
FS_NAND_INTERN  void                     FS_NAND_DirtyMapInvalidate    (FS_NAND_DATA             *p_nand_data);

                                                                /* Invalidate bad blk tbl.                              */
FS_NAND_INTERN  void                     FS_NAND_BadBlkTblInvalidate   (FS_NAND_DATA             *p_nand_data);
#endif

                                                                /* Get sec type from OOS data.                          */
FS_NAND_INTERN  FS_NAND_SEC_TYPE         FS_NAND_SecTypeParse          (CPU_INT08U               *p_oos_buf);

                                                                /* Chk if sec is used.                                  */
FS_NAND_INTERN  CPU_BOOLEAN              FS_NAND_SecIsUsed             (FS_NAND_DATA             *p_nand_data,
                                                                        FS_SEC_NBR                sec_ix_phy,
                                                                        FS_ERR                   *p_err);

                                                                /* Init ctrlr impl if needed.                           */
FS_NAND_INTERN  void                     FS_NAND_CtrlrReg              (FS_NAND_CTRLR_API        *p_ctrlr_api,
                                                                        FS_ERR                   *p_err);

                                                                /* Calc NAND info.                                      */
FS_NAND_INTERN  void                     FS_NAND_CalcDevInfo           (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Alloc NAND data.                                     */
FS_NAND_INTERN  void                     FS_NAND_AllocDevData          (FS_NAND_DATA             *p_nand_data,
                                                                        FS_ERR                   *p_err);

                                                                /* Init NAND data.                                      */
FS_NAND_INTERN  void                     FS_NAND_InitDevData           (FS_NAND_DATA             *p_nand_data);

                                                                /* Free NAND data.                                      */
FS_NAND_INTERN  void                     FS_NAND_DataFree              (FS_NAND_DATA             *p_nand_data);

                                                                /* Allocate & init nand data                            */
FS_NAND_INTERN  FS_NAND_DATA            *FS_NAND_DataGet               (void);


/*
*********************************************************************************************************
*                                     NAND DRIVER API STRUCTURE
*********************************************************************************************************
*/

#if (FS_NAND_INTERN_API_EN == DEF_ENABLED)
const  FS_DEV_API  FS_NAND = {
                                 FS_NAND_NameGet,               /* Get name of driver.                                  */
                                 FS_NAND_Init,                  /* Init driver.                                         */
                                 FS_NAND_Open,                  /* Open driver.                                         */
                                 FS_NAND_Close,                 /* Close driver.                                        */
                                 FS_NAND_Rd,                    /* Rd from dev.                                         */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                 FS_NAND_Wr,                    /* Wr to dev.                                           */
#endif
                                 FS_NAND_Query,                 /* Get dev info.                                        */
                                 FS_NAND_IO_Ctrl                /* Perform dev I/O ctrl.                                */
};
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  FS_NAND_CFG_MAX_CTRLR_IMPL
#error  "FS_NAND_CFG_MAX_CTRLR_IMPL not #define's in 'fs_dev_nand_cfg.h'"
#elif   (FS_NAND_CFG_MAX_CTRLR_IMPL <= 0)
#error  "FS_NAND_CFG_MAX_CTRLR_IMPL illegally #define'd in 'fs_dev_nand_cfg.h' [MUST be >= 1]"
#endif

#ifndef  FS_NAND_CFG_AUTO_SYNC_EN
#error  "FS_NAND_CFG_AUTO_SYNC_EN               not #define's in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"

#elif  ((FS_NAND_CFG_AUTO_SYNC_EN != DEF_DISABLED) && \
        (FS_NAND_CFG_AUTO_SYNC_EN != DEF_ENABLED ))
#error  "FS_NAND_CFG_AUTO_SYNC_EN               illegally #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"
#endif

#ifndef  FS_NAND_CFG_UB_META_CACHE_EN
#error  "FS_NAND_CFG_UB_META_CACHE_EN           not #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"

#elif  ((FS_NAND_CFG_UB_META_CACHE_EN != DEF_DISABLED) && \
        (FS_NAND_CFG_UB_META_CACHE_EN != DEF_ENABLED ))
#error  "FS_NAND_CFG_UB_META_CACHE_EN           illegally #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"
#endif

#ifndef  FS_NAND_CFG_DIRTY_MAP_CACHE_EN
#error  "FS_NAND_CFG_DIRTY_MAP_CACHE_EN         not #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"

#elif  ((FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_DISABLED) && \
        (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED ))
#error  "FS_NAND_CFG_DIRTY_MAP_CACHE_EN         illegally #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"
#endif

#ifndef  FS_NAND_CFG_UB_TBL_SUBSET_SIZE
#error  "FS_NAND_CFG_UB_TBL_SUBSET_SIZE         not #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"

#elif ((FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0u) && (FS_UTIL_IS_PWR2(FS_NAND_CFG_UB_TBL_SUBSET_SIZE)==DEF_NO))
#error "FS_NAND_CFG_UB_TBL_SUBSET_SIZE must be a power of 2, or 0."
#endif

#ifndef FS_NAND_CFG_RSVD_AVAIL_BLK_CNT
#error "FS_NAND_CFG_RSVD_AVAIL_BLK_CNT must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_RSVD_AVAIL_BLK_CNT < 0u)
#error "FS_NAND_CFG_RSVD_AVAIL_BLK_CNT must be positive"
#endif

#ifndef FS_NAND_CFG_MAX_RD_RETRIES
#error "FS_NAND_CFG_MAX_RD_RETRIES must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_MAX_RD_RETRIES < 2u)
#error "FS_NAND_CFG_MAX_RD_RETRIES must be at least 2"
#endif

#ifndef FS_NAND_CFG_MAX_SUB_PCT
#error "FS_NAND_CFG_MAX_SUB_PCT must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_MAX_SUB_PCT < 0u)
#error "FS_NAND_CFG_MAX_SUB_PCT must be positive"
#elif  (FS_NAND_CFG_MAX_SUB_PCT > 100u)
#error "FS_NAND_CFG_MAX_SUB_PCT must not be larger than 100"
#endif

#ifndef FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB
#error "FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB < 0)
#error "FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB must be positive"
#endif

#ifndef FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB
#error "FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB < 0)
#error "FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB must be positive"
#elif  (FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB > 100)
#error "FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB must be lower than or equal to 100"
#endif

#ifndef FS_NAND_CFG_TH_PCT_PAD_SUB
#error "FS_NAND_CFG_TH_PCT_PAD_SUB must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_TH_PCT_PAD_SUB < 0)
#error "FS_NAND_CFG_TH_PCT_PAD_SUB must be positive"
#elif  (FS_NAND_CFG_TH_PCT_PAD_SUB >= 100)
#error "FS_NAND_CFG_TH_PCT_PAD_SUB must be lower than 100"
#endif

#ifndef FS_NAND_CFG_TH_PCT_MERGE_SUB
#error "FS_NAND_CFG_TH_PCT_MERGE_SUB must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_TH_PCT_MERGE_SUB < 0)
#error "FS_NAND_CFG_TH_PCT_MERGE_SUB must be positive"
#elif  (FS_NAND_CFG_TH_PCT_MERGE_SUB > 100)
#error "FS_NAND_CFG_TH_PCT_MERGE_SUB must be lower than or equal to 100"
#endif

#ifndef FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD
#error "FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD must be #define'd in fs_dev_nand_cfg.h"
#elif  (FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD < 0)
#error "FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD must be positive"
#endif

#ifndef FS_NAND_CFG_CLR_CORRUPT_METABLK
#error  "FS_NAND_CFG_CLR_CORRUPT_METABLK        not #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"

#elif  ((FS_NAND_CFG_CLR_CORRUPT_METABLK != DEF_DISABLED) && \
        (FS_NAND_CFG_CLR_CORRUPT_METABLK != DEF_ENABLED ))
#error  "FS_NAND_CFG_CLR_CORRUPT_METABLK        illegally #define'd in 'fs_dev_nand_cfg.h'"
#error  "                                       [MUST be  DEF_DISABLED]"
#error  "                                       [     ||  DEF_ENABLED ]"
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

#if (FS_NAND_INTERN_API_EN == DEF_YES)
/*
*********************************************************************************************************
*                                          FS_NAND_LowFmt()
*
* Description : Low-level format a NAND device.
*
* Argument(s) : p_name_dev  Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_DEV_INVALID              Argument 'name_dev' specifies an invalid device.
*                               FS_ERR_NAME_NULL                Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NONE                     Device low-level formatted successfully.
*
*
*                               --------------------------RETURNED BY FSDev_IO_Ctrl()---------------------------
*                               See FSDev_IO_Ctrl() for additional return error codes.
*
*                               --------------------------RETURNED BY FSDev_Refresh()---------------------------
*                               See FSDev_Refresh() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a NAND device (e.g., "nand:0:").
*
*               (2) A NAND medium MUST be low-level formatted with this driver prior to
*                   access by the high-level file system, a requirement which the device module enforces.
*
*               (3) If 'FS_ERR_DEV_INVALID_LOW_FMT' is returned, the low-level format information was
*                   not valid.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_NAND_LowFmt (CPU_CHAR  *p_name_dev,
                      FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err  ptr.                                   */
        CPU_SW_EXCEPTION(;);
    }

    if (p_name_dev == DEF_NULL) {                               /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(p_name_dev, (CPU_CHAR *)FS_NAND_DrvName, FS_NAND_DRV_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (p_name_dev[FS_NAND_DRV_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ----------------- LOW-LEVEL FMT DEV ---------------- */
    FSDev_IO_Ctrl(p_name_dev,
                  FS_DEV_IO_CTRL_LOW_FMT,
                  DEF_NULL,
                  p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(p_name_dev, p_err);
    }
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_LowMount()
*
* Description : Low-level mount a NAND device.
*
* Argument(s) : p_name_dev  Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_DEV_INVALID              Argument 'name_dev' specifies an invalid device.
*                               FS_ERR_NAME_NULL                Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NONE                     Device low-level mounted successfully.
*
*                               --------------------------RETURNED BY FSDev_IO_Ctrl()---------------------------
*                               See FSDev_IO_Ctrl() for additional return error codes.
*
*                               --------------------------RETURNED BY FSDev_Refresh()---------------------------
*                               See FSDev_Refresh() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a NAND device (e.g., "nand:0:").
*
*               (2) Low-level mounting parses the on-device structure, detecting the presence of a valid
*                   low-level format.  If 'FS_ERR_DEV_INVALID_LOW_FMT' is returned, the device is NOT
*                   low-level formatted (see 'FS_NAND_LowFmt()  Note #2').
*
*               (3) If an existing on-device low-level format is found but doesn't match the format
*                   prompted by specified device configuration, 'FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS' will
*                   be returned. A low-level format is required.
*
*               (4) If an existing and compatible on-device low-level format is found, but is not
*                   usable because of some metadata corruption, 'FS_ERR_DEV_CORRUPT_LOW_FMT' will be.
*                   A chip erase and/or low-level format is required.
*********************************************************************************************************
*/

void  FS_NAND_LowMount (CPU_CHAR  *p_name_dev,
                        FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err  ptr.                                   */
        CPU_SW_EXCEPTION(;);
    }

    if (p_name_dev == DEF_NULL) {                               /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(p_name_dev, (CPU_CHAR *)FS_NAND_DrvName, FS_NAND_DRV_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (p_name_dev[FS_NAND_DRV_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* ---------------- LOW-LEVEL MOUNT DEV --------------- */
    FSDev_IO_Ctrl(p_name_dev,
                  FS_DEV_IO_CTRL_LOW_MOUNT,
                  DEF_NULL,
                  p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(p_name_dev, p_err);
    }
}


/*
*********************************************************************************************************
*                                         FS_NAND_LowUnmount()
*
* Description : Low-level unmount a NAND device.
*
* Argument(s) : p_name_dev  Device name (see Note #1).
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_DEV_INVALID      Argument 'name_dev' specifies an invalid device.
*                               FS_ERR_NAME_NULL        Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NONE             Device low-level unmounted successfully.
*
*                               ----------------------RETURNED BY FSDev_Refresh()-----------------------
*                               See FSDev_Refresh() for additional return error codes.
*
*                               ----------------------RETURNED BY FSDev_IO_Ctrl()-----------------------
*                               See FSDev_IO_Ctrl() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device MUST be a NAND device (e.g., "nand:0:").
*
*               (2) Low-level unmounting clears software knowledge of the on-disk structures, forcing
*                   the device to again be low-level mounted or formatted prior to further use.
*********************************************************************************************************
*/

void  FS_NAND_LowUnmount (CPU_CHAR  *p_name_dev,
                          FS_ERR    *p_err)
{
    CPU_INT16S  cmp_val;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err  ptr.                                   */
        CPU_SW_EXCEPTION(;);
    }

    if (p_name_dev == DEF_NULL) {                               /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* Validate name str (see Note #1).                     */
    cmp_val = Str_Cmp_N(p_name_dev, (CPU_CHAR *)FS_NAND_DrvName, FS_NAND_DRV_NAME_LEN);
    if (cmp_val != 0) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    if (p_name_dev[FS_NAND_DRV_NAME_LEN] != FS_CHAR_DEV_SEP) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* --------------- LOW-LEVEL UNMOUNT DEV -------------- */
    FSDev_IO_Ctrl(p_name_dev,
                  FS_DEV_IO_CTRL_LOW_UNMOUNT,
                  DEF_NULL,
                  p_err);

    if (*p_err == FS_ERR_NONE) {
       (void)FSDev_Refresh(p_name_dev, p_err);
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
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FS_NAND_NameGet()
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

FS_NAND_INTERN  const  CPU_CHAR  *FS_NAND_NameGet (void)
{
    return (FS_NAND_DrvName);
}


/*
*********************************************************************************************************
*                                            FS_NAND_Init()
*
* Description : Initialize the driver.
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE     Device driver initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should initialize any structures, tables or variables that are common
*                   to all devices or are used to manage devices accessed with the driver.  This function
*                   SHOULD NOT initialize any devices; that will be done individually for each with
*                   device driver's 'Open()' function.
*
*               (2) FS_NAND_ListFreePtr and FS_NAND_UnitCtr MUST ALWAYS be accessed excluively in
*                   critical sections.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_Init (FS_ERR  *p_err)
{
    CPU_INT08U  ctrlr_impl_ix;
    CPU_SR_ALLOC();


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err  == DEF_NULL) {                                   /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
#endif

    CPU_CRITICAL_ENTER();
    FS_NAND_UnitCtr     = 0u;
    FS_NAND_ListFreePtr = DEF_NULL;
    for (ctrlr_impl_ix =0u; ctrlr_impl_ix < FS_NAND_CFG_MAX_CTRLR_IMPL; ctrlr_impl_ix++) {
        FS_NAND_ListCtrlrImpl[ctrlr_impl_ix] = DEF_NULL;
    }
    CPU_CRITICAL_EXIT();

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FS_NAND_Open()
*
* Description : Open (initialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to open.
*
*               p_dev_cfg   Pointer to device configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_DEV_INVALID_CFG          Device configuration specified is  invalid.
*                               FS_ERR_DEV_INVALID_ECC          Device invalid ECC.
*                               FS_ERR_DEV_INVALID_LOW_FMT      Device needs to be low-level formatted.
*                               FS_ERR_DEV_INVALID_LOW_PARAMS   Device low-level device parameters invalid.
*                               FS_ERR_DEV_INVALID_OP           Device invalid operation.
*                               FS_ERR_DEV_IO                   Device I/O error.
*                               FS_ERR_DEV_TIMEOUT              Device timeout.
*                               FS_ERR_MEM_ALLOC                Memory could not be allocated.
*                               FS_ERR_NONE                     Device opened successfully.
*
*                               --------------------RETURNED BY FS_NAND_AllocDevData()---------------------
*                               See FS_NAND_AllocDevData() for additional return error codes.
*
*                               -------------------RETURNED BY FS_NAND_LowMountHandler()-------------------
*                               See FS_NAND_LowMountHandler() for additional return error codes.
*
*                               ----------------------RETURNED BY FS_NAND_CtrlrReg()-----------------------
*                               See FS_NAND_CtrlrReg() for additional return error codes.
*
*                               ------------------RETURNED BY FS_NAND_LowUnmountHandler()------------------
*                               See FS_NAND_LowUnmountHandler() for additional return error codes.
*
*                               -----------------RETURNED BY p_nand_cfg->CtrlrPtr->Open()------------------
*                               See p_nand_cfg->CtrlrPtr->Open() for additional return error codes.
*
*                               ---------------------RETURNED BY FS_NAND_CalcDevInfo()---------------------
*                               See FS_NAND_CalcDevInfo() for additional return error codes.
*
*                               ----------------RETURNED BY p_nand_data->CtrlrPtr->Setup()-----------------
*                               See p_nand_data->CtrlrPtr->Setup() for additional return error codes.
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
*               (4) See 'FSDev_Open() Note #3'.
*
*               (5) The driver should identify the device instance to be opened by checking
*                   'p_dev->UnitNbr'.  For example, if "nand:2:" is to be opened, then
*                   'p_dev->UnitNbr' will hold the integer 2.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_Open (FS_DEV  *p_dev,
                                    void    *p_dev_cfg,
                                    FS_ERR  *p_err)
{
    FS_NAND_CFG        *p_nand_cfg;
    FS_NAND_DATA       *p_nand_data;
    void               *p_ctrlr_data;
    FS_NAND_PART_DATA  *p_part_data;
    FS_SEC_SIZE         min_sec_size;
    CPU_INT08U          nbr_sec_per_pg;
    FS_NAND_OOS_INFO    OOS_info;
    FS_NAND_PG_SIZE     oos_size_free;
    FS_NAND_PG_SIZE     needed_oos_size;
    FS_NAND_BLK_QTY     last_blk_ix;
    CPU_INT16U          used_mark_size_bits;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == DEF_NULL) {                                    /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_dev_cfg == DEF_NULL) {                                /* Validate dev cfg ptr.                                */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nand_cfg = (FS_NAND_CFG *)p_dev_cfg;

                                                                /* ------------------- VALIDATE CFG ------------------- */
    if (p_nand_cfg->CtrlrPtr == DEF_NULL) {                     /* Validate ctrlr api ptr.                              */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): NAND Ctrlr drv NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if (p_nand_cfg->PartPtr == DEF_NULL) {                      /* Validate part api  ptr.                              */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): NAND Part drv NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if (p_nand_cfg->BSPPtr == DEF_NULL) {                       /* Validate ctrlr BSP ptr.                              */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): NAND Ctrlr BSP drv NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if (p_nand_cfg->CtrlrCfgPtr == DEF_NULL) {                  /* Validate ctrlr cfg ptr.                              */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): NAND Ctrlr cfg NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

                                                                /* ------------------ INIT UNIT INFO ------------------ */
    p_nand_data = FS_NAND_DataGet();
    if (p_nand_data == DEF_NULL) {
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    p_dev->DataPtr = (void *)p_nand_data;


    p_nand_data->CtrlrPtr = p_nand_cfg->CtrlrPtr;


    FS_NAND_CtrlrReg(p_nand_data->CtrlrPtr, p_err);             /* Init ctrlr impl if needed.                           */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* Open ctrlr.                                          */
    p_ctrlr_data = p_nand_cfg->CtrlrPtr->Open(p_nand_cfg->PartPtr,
                                              p_nand_cfg->BSPPtr,
                                              p_nand_cfg->CtrlrCfgPtr,
                                              p_nand_cfg->PartCfgPtr,
                                              p_err);
    if (*p_err != FS_ERR_NONE) {
       FS_NAND_TRACE_DBG(("FS_NAND_Open(): Error %u opening ctrlr.\r\n", *p_err));
       return;
    }

    p_nand_data->CtrlrDataPtr = p_ctrlr_data;

                                                                /* Retrieve part data.                                  */
    p_part_data = p_nand_data->CtrlrPtr->PartDataGet(p_ctrlr_data);
    if (p_part_data == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Error retrieving nand part data.\r\n"));
        return;
    }
    p_nand_data->PartDataPtr  = p_part_data;


    if (p_nand_cfg->BlkCnt == FS_NAND_CFG_DEFAULT) {            /* Validate blk cnt.                                    */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Using default blk cnt (all blocks): %d.\r\n", p_part_data->BlkCnt - p_nand_cfg->BlkIxFirst));
        p_nand_cfg->BlkCnt = p_part_data->BlkCnt - p_nand_cfg->BlkIxFirst;
    }

    if (p_nand_cfg->UB_CntMax >= p_nand_cfg->BlkCnt) {          /* Validate UB cnt.                                     */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Invalid update blk cnt: %d.\r\n", p_nand_cfg->UB_CntMax));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    } else {
        if (p_nand_cfg->UB_CntMax == FS_NAND_CFG_DEFAULT) {
            FS_NAND_TRACE_DBG(("FS_NAND_Open(): Using default update blk cnt: %d.\r\n", 10));
            p_nand_cfg->UB_CntMax = 10;
        }
    }

    if (p_nand_cfg->RUB_MaxAssoc == FS_NAND_CFG_DEFAULT) {      /* Validate max assoc.                                 */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Using default  max associativity %d.\r\n", 2));
        p_nand_cfg->RUB_MaxAssoc = 2;
    }
                                                                /* Validate avail blk tbl nbr of entries.               */
    if (p_nand_cfg->AvailBlkTblEntryCntMax == FS_NAND_CFG_DEFAULT) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Default number of entries in avail blk tbl.\r\n"));
        p_nand_cfg->AvailBlkTblEntryCntMax = DEF_MAX(10u, FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1u);
    } else {
        if (p_nand_cfg->AvailBlkTblEntryCntMax < FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1u) {
            FS_NAND_TRACE_DBG(("FS_NAND_Open(): Nbr of entries in avail blk tbl can't contain FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1 blocks.\r\n"));
           *p_err = FS_ERR_DEV_INVALID_CFG;
            return;
        }
    }

    if (p_nand_cfg->AvailBlkTblEntryCntMax > p_nand_cfg->BlkCnt) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Nbr of entries in avail blk tbl higher than total blk cnt.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

                                                                /* Validate index of first blk to use.                  */
    if (p_nand_cfg->BlkIxFirst > p_part_data->BlkCnt) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Index of first block out of bounds.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }


    switch (p_nand_cfg->SecSize) {                              /* Validate sec size.                                   */
        case  512u:
        case 1024u:
        case 2048u:
        case 4096u:
             break;


        case FS_NAND_CFG_DEFAULT:
             p_nand_cfg->SecSize = p_part_data->PgSize;
             break;


        default:
             FS_NAND_TRACE_DBG(("FS_NAND_Open(): Invalid NAND sec size: %d.\r\n", p_nand_cfg->SecSize));
            *p_err = FS_ERR_DEV_INVALID_CFG;
             return;
    }




    p_nand_data->RUB_MaxAssoc = p_nand_cfg->RUB_MaxAssoc;


                                                                /* ---------- VALIDATE CFG AGAINST PART DATA ---------- */
                                                                /* Validate sector size.                                */
    min_sec_size = p_part_data->PgSize / p_part_data->NbrPgmPerPg;
    if (p_nand_cfg->SecSize < min_sec_size) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Sector size (%u) not supported by device. Minimum size is %u.\r\n",
                           p_nand_cfg->SecSize,
                           min_sec_size));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }
    if (p_nand_cfg->SecSize > p_part_data->PgSize) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Sector size (%u) not supported by device. Maximum size is %u.\r\n",
                           p_nand_cfg->SecSize,
                           p_part_data->PgSize));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }
    p_nand_data->SecSize = p_nand_cfg->SecSize;

                                                                /* Validate blk cnt.                                    */
    p_nand_data->BlkIxFirst = p_nand_cfg->BlkIxFirst;
    last_blk_ix             = p_nand_cfg->BlkIxFirst + p_nand_cfg->BlkCnt - 1u;
    if (last_blk_ix >= p_part_data->BlkCnt) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Invalid blk cnt: %u, max %u with blk nbr first=%u.\r\n",
                           p_nand_cfg->BlkCnt,
                           p_part_data->BlkCnt - p_nand_cfg->BlkIxFirst,
                           p_nand_cfg->BlkIxFirst));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }
    p_nand_data->BlkCnt = p_nand_cfg->BlkCnt;

    if (p_nand_cfg->UB_CntMax >= p_nand_data->BlkCnt) {         /* Validate UB cnt.                                     */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Invalid update blk cnt: %u, total blk cnt: %u.\r\n",
                           p_nand_cfg->UB_CntMax,
                           p_nand_data->UB_CntMax));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }
    p_nand_data->UB_CntMax = p_nand_cfg->UB_CntMax;


                                                                /* ------------ VALIDATE AGAINST HDR SIZE ------------- */
    if (p_nand_data->SecSize < FS_NAND_HDR_TOTAL_SIZE) {
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Sector size is too small to contain hdr info: needed=%u.\r\n",
                           FS_NAND_HDR_TOTAL_SIZE));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }


                                                                /* ------------------- SETUP CTRLR -------------------- */
    OOS_info = p_nand_data->CtrlrPtr->Setup(p_ctrlr_data,
                                            p_nand_data->SecSize,
                                            p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    oos_size_free           = OOS_info.Size;
    p_nand_data->OOS_BufPtr = OOS_info.BufPtr;

                                                                /* ----------------- CALC FTL PARAMS ------------------ */
    p_nand_data->AvailBlkTblEntryCntMax   = p_nand_cfg->AvailBlkTblEntryCntMax;

                                                                /* Determine nbr of sec per blk.                        */
    nbr_sec_per_pg                = p_nand_data->PartDataPtr->PgSize   / p_nand_data->SecSize;
    p_nand_data->NbrSecPerBlk     = p_nand_data->PartDataPtr->PgPerBlk * nbr_sec_per_pg;
    p_nand_data->NbrSecPerBlkLog2 = FSUtil_Log2(p_nand_data->NbrSecPerBlk);

    p_nand_data->UB_SecMapNbrBits = p_nand_data->NbrSecPerBlkLog2 -
                                    FSUtil_Log2(FS_NAND_CFG_UB_TBL_SUBSET_SIZE);

    p_nand_data->RUB_MaxAssocLog2 = FSUtil_Log2(p_nand_data->RUB_MaxAssoc);


    used_mark_size_bits           = p_nand_data->PartDataPtr->ECC_NbrCorrBits * 2u;
    p_nand_data->UsedMarkSize     = FS_UTIL_BIT_NBR_TO_OCTET_NBR(used_mark_size_bits);

                                                                /* Calc limit on nbr of SUBs.                           */
    p_nand_data->SUB_CntMax       = p_nand_data->UB_CntMax * FS_NAND_CFG_MAX_SUB_PCT / 100;
    if (p_nand_data->SUB_CntMax  == 0u) {
        p_nand_data->SUB_CntMax   = 1u;
    }


                                                                /* ------------------- CALC FTL TH -------------------- */
                                                                /* See FS_NAND_SecWrInUB() note #2.                     */
    p_nand_data->ThSecWrCnt_MergeRUBStartSUB = p_nand_data->NbrSecPerBlk * FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB / 100u + 1u;
    p_nand_data->ThSecRemCnt_ConvertSUBToRUB = p_nand_data->NbrSecPerBlk * FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB  / 100u + 1u;
    p_nand_data->ThSecGapCnt_PadSUB          = p_nand_data->NbrSecPerBlk * FS_NAND_CFG_TH_PCT_PAD_SUB             / 100u + 1u;
                                                                /* See FS_NAND_UB_Alloc() note #2.                      */
    p_nand_data->ThSecRemCnt_MergeSUB        = p_nand_data->NbrSecPerBlk * FS_NAND_CFG_TH_PCT_PAD_SUB             / 100u + 1u;


                                                                /* ---------------- DETERMINE OOS USAGE --------------- */
    needed_oos_size = FS_NAND_OOS_PARTIAL_SIZE_REQD + p_nand_data->UsedMarkSize;
    if (needed_oos_size > oos_size_free) {                      /* Check OOS usage.                                     */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): not enough free space in OOS: actual=%d, needed=%d.\r\n",
                            oos_size_free,
                            needed_oos_size));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }

                                                                /* ------------------- CALC NAND INFO ----------------- */
    FS_NAND_CalcDevInfo(p_nand_data, p_err);                    /* Calc dev info.                                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_nand_data->LogicalDataBlkCnt = p_nand_data->SecCnt / p_nand_data->NbrSecPerBlk;


    FS_NAND_AllocDevData(p_nand_data, p_err);                   /* Alloc dev data.                                      */
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ---------------- CHECK METADATA SIZE --------------- */
    if (p_nand_data->MetaSecCnt > p_nand_data->NbrSecPerBlk) {  /* Check metadata size doesn't exceed 1 blk.            */
        FS_NAND_TRACE_DBG(("FS_NAND_Open(): Metadata size(%u) exceeds blk size(%u) (sizes in sec).\r\n",
                           p_nand_data->MetaSecCnt,
                           p_nand_data->NbrSecPerBlk));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }

    FS_NAND_TRACE_INFO(("NAND FLASH FOUND: Name       : \"nand:%d:\"\r\n", p_dev->UnitNbr));
    FS_NAND_TRACE_INFO(("                  Sec Size   : %d bytes\r\n",     p_nand_data->SecSize));
    FS_NAND_TRACE_INFO(("                  Size       : %d sectors\r\n",   p_nand_data->SecCnt));
    FS_NAND_TRACE_INFO(("                  Update blks: %d\r\n",           p_nand_data->UB_CntMax));


                                                                /* ---------------- LOW-LEVEL MOUNT DEV --------------- */
    p_nand_data->Fmtd = DEF_NO;

    FS_NAND_LowUnmountHandler(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FS_NAND_LowMountHandler(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        if ((*p_err == FS_ERR_DEV_CORRUPT_LOW_FMT) &&
            (p_nand_data->MetaBlkIxPhy == last_blk_ix)) {
           *p_err = FS_ERR_NONE;
            FS_NAND_BlkEraseHandler(p_nand_data, last_blk_ix, p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }

            FS_NAND_TRACE_INFO(("Unable to mount with metadata blk %d.\r\n", last_blk_ix));
            FS_NAND_TRACE_INFO(("Discarding metadata blk %d and remounting.\r\n", last_blk_ix));

            FS_NAND_InitDevData(p_nand_data);
            FS_NAND_LowMountHandler(p_nand_data, p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        } else
#endif
        {
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                           FS_NAND_Close()
*
* Description : Close (uninitialize) a device instance.
*
* Argument(s) : p_dev  Pointer to device to close.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) This function will be called EVERY time the device is closed.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_Close (FS_DEV  *p_dev)
{
    FS_NAND_DATA  *p_nand_data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_dev == DEF_NULL) {                                    /* Validate dev ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
#endif


    p_nand_data = (FS_NAND_DATA *)p_dev->DataPtr;
    if (p_nand_data != DEF_NULL) {
        if (p_nand_data->CtrlrDataPtr != DEF_NULL) {            /* Close ctrlr inst.                                    */
            p_nand_data->CtrlrPtr->Close(p_nand_data->CtrlrDataPtr);
        }

        FS_NAND_DataFree(p_nand_data);
    }
}


/*
*********************************************************************************************************
*                                             FS_NAND_Rd()
*
* Description : Read from a device & store data in buffer.
*
* Argument(s) : p_dev       Pointer to device to read from.
*
*               p_dest      Pointer to destination buffer.
*
*               sec_start   Start sector of read.
*
*               sec_cnt     Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_DEV_INVALID_UNIT_NBR     Device unit number is invalid.
*                               FS_ERR_DEV_IO                   Device I/O error.
*                               FS_ERR_DEV_NOT_OPEN             Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT          Device is not present.
*                               FS_ERR_DEV_TIMEOUT              Device timeout.
*                               FS_ERR_NONE                     Sector(s) read.
*
*                               ---------------RETURNED BY FS_NAND_MetaCommit()---------------
*                               See FS_NAND_MetaCommit() for additional return error codes.
*
*                               -----------------RETURNED BY FS_NAND_SecRd()------------------
*                               See FS_NAND_SecRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_Rd (FS_DEV      *p_dev,
                                  void        *p_dest,
                                  FS_SEC_NBR   sec_start,
                                  FS_SEC_QTY   sec_cnt,
                                  FS_ERR      *p_err)
{
    FS_NAND_DATA  *p_nand_data;
    CPU_SIZE_T     rd_cnt_iter;
    CPU_SIZE_T     rd_cnt_total;
    FS_SEC_QTY     sec_ix_logical;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err  ptr.                                   */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == DEF_NULL) {                                    /* Validate dev  ptr.                                   */
       *p_err  = FS_ERR_NULL_PTR;
        return;
    }
    if (p_dest == DEF_NULL) {                                   /* Validate dest ptr.                                   */
       *p_err   = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nand_data = (FS_NAND_DATA *)p_dev->DataPtr;

   *p_err = FS_ERR_NONE;

    sec_ix_logical = sec_start;
    rd_cnt_total   = 0u;

    while ((rd_cnt_total <  sec_cnt)    &&
           (*p_err       == FS_ERR_NONE)  ){
                                                                /* Rd 1 or more sec.                                    */
        rd_cnt_iter = FS_NAND_SecRd(p_nand_data,
                                    p_dest,
                                    sec_ix_logical,
                                    sec_cnt,
                                    p_err);

        rd_cnt_total   += rd_cnt_iter;
        sec_ix_logical += rd_cnt_iter;

        FS_CTR_STAT_ADD(p_nand_data->Ctrs.StatRdCtr, rd_cnt_iter);

                                                                /* Update dest data ptr.                                */
        p_dest = (void *)((CPU_INT08U *)p_dest + (p_nand_data->SecSize * rd_cnt_iter));
    }

    if (*p_err == FS_ERR_DEV_NAND_NO_SUCH_SEC) {
        *p_err = FS_ERR_NONE;
    }

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_Rd(): Error reading %u sectors from index %u.\r\n",
                            sec_cnt,
                            sec_start));

        return;
    }



                                                                /* ----- COMMIT METADATA (IN CASE OF REFRESH) ----- */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
#if (FS_NAND_CFG_AUTO_SYNC_EN == DEF_ENABLED)
    do {
       *p_err = FS_ERR_NONE;

        FS_NAND_MetaCommit(p_nand_data,
                           DEF_NO,
                           p_err);

    } while ((*p_err != FS_ERR_NONE) &&
             (*p_err != FS_ERR_DEV_NAND_NO_AVAIL_BLK));

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_Wr(): Error committing metadata.\r\n"));

        return;
    }
#endif
#endif

}


/*
*********************************************************************************************************
*                                             FS_NAND_Wr()
*
* Description : Write data to a device from a buffer.
*
* Argument(s) : p_dev       Pointer to device to write to.
*
*               p_src       Pointer to source buffer.
*
*               sec_start   Start sector of write.
*
*               sec_cnt     Number of sectors to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_DEV_INVALID_UNIT_NBR     Device unit number is invalid.
*                               FS_ERR_DEV_IO                   Device I/O error.
*                               FS_ERR_DEV_NOT_OPEN             Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT          Device is not present.
*                               FS_ERR_DEV_TIMEOUT              Device timeout.
*                               FS_ERR_NONE                     Sector(s) written.
*
*                               -----------------RETURNED BY FS_NAND_SecWr()------------------
*                               See FS_NAND_SecWr() for additional return error codes.
*
*                               ---------------RETURNED BY FS_NAND_MetaCommit()---------------
*                               See FS_NAND_MetaCommit() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_Wr (FS_DEV      *p_dev,
                                  void        *p_src,
                                  FS_SEC_NBR   sec_start,
                                  FS_SEC_QTY   sec_cnt,
                                  FS_ERR      *p_err)
{
    FS_NAND_DATA  *p_nand_data;
    FS_SEC_QTY     sec_wr_cnt_total;
    FS_SEC_QTY     sec_wr_cnt_iter;
    FS_SEC_QTY     sec_ix_logical;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == DEF_NULL) {                                    /* Validate dev ptr.                                    */
       *p_err  = FS_ERR_NULL_PTR;
        return;
    }
    if (p_src == DEF_NULL) {                                    /* Validate src ptr.                                    */
       *p_err  = FS_ERR_NULL_PTR;
        return;
    }
#endif


    FS_NAND_TRACE_LOG(("FS_NAND_Wr: start=%u, cnt=%u.\r\n", sec_start, sec_cnt));

    p_nand_data = (FS_NAND_DATA *)p_dev->DataPtr;


   *p_err = FS_ERR_NONE;

    sec_ix_logical   = sec_start;
    sec_wr_cnt_total = 0u;

    while (( sec_wr_cnt_total  < sec_cnt)    &&
           (*p_err            == FS_ERR_NONE)  ) {
                                                                /* Wr 1 or more sec.                                    */
        sec_wr_cnt_iter = FS_NAND_SecWr(p_nand_data,
                                        p_src,
                                        sec_ix_logical,
                                        sec_cnt - sec_wr_cnt_total,
                                        p_err);

        sec_wr_cnt_total += sec_wr_cnt_iter;
        sec_ix_logical   += sec_wr_cnt_iter;

        FS_CTR_STAT_ADD(p_nand_data->Ctrs.StatWrCtr, sec_wr_cnt_iter);

                                                                /* Update src data ptr.                                 */
        p_src = (void *)((CPU_INT08U *)p_src + (p_nand_data->SecSize * sec_wr_cnt_iter));

    }

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_Wr(): Error writing sectors on device.\r\n"));
        return;
    }

#if (FS_NAND_CFG_AUTO_SYNC_EN == DEF_ENABLED)
                                                                /* ----------------- COMMIT METADATA ------------------ */
    do {
       *p_err = FS_ERR_NONE;

        FS_NAND_MetaCommit(p_nand_data,
                           DEF_NO,
                           p_err);

    } while ((*p_err != FS_ERR_NONE) &&
             (*p_err != FS_ERR_DEV_NAND_NO_AVAIL_BLK));

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_Wr(): Error committing metadata.\r\n"));

        return;
    }
#endif

}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_Query()
*
* Description : Get information about a device.
*
* Argument(s) : p_dev   Pointer to device to query.
*
*               p_info  Pointer to structure that will receive device information.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_DEV_INVALID_UNIT_NBR     Device unit number is invalid.
*                           FS_ERR_DEV_NOT_OPEN             Device is not open.
*                           FS_ERR_DEV_NOT_PRESENT          Device is not present.
*                           FS_ERR_NONE                     Device information obtained.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_Query (FS_DEV       *p_dev,
                                     FS_DEV_INFO  *p_info,
                                     FS_ERR       *p_err)
{
    FS_NAND_DATA  *p_nand_data;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == DEF_NULL) {                                    /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_info == DEF_NULL) {                                   /* Validate dev info ptr.                               */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    p_nand_data     = (FS_NAND_DATA *)p_dev->DataPtr;
    p_info->SecSize =  p_nand_data->SecSize;
    p_info->Size    =  p_nand_data->SecCnt;
    p_info->Fixed   =  DEF_YES;                                 /* Dev is not removable.                                */

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FS_NAND_IO_Ctrl()
*
* Description : Perform device I/O control operation.
*
* Argument(s) : p_dev   Pointer to device to control.
*
*               opt     Control command.
*
*               p_data  Buffer which holds data to be used for operation.
*                          OR
*                       Buffer in which data will be stored as a result of operation.
*
*               p_err   Pointer to variable that will receive return the error code from this function :
*
*                           FS_ERR_DEV_INVALID_IO_CTRL      I/O control operation unknown to driver.
*                           FS_ERR_DEV_INVALID_UNIT_NBR     Device unit number is invalid.
*                           FS_ERR_DEV_IO                   Device I/O error.
*                           FS_ERR_DEV_NOT_OPEN             Device is not open.
*                           FS_ERR_DEV_NOT_PRESENT          Device is not present.
*                           FS_ERR_DEV_TIMEOUT              Device timeout.
*                           FS_ERR_NONE                     Control operation performed successfully.
*
*                           -------------------RETURNED BY p_ctrlr_api->IO_Ctrl()--------------------
*                           See p_ctrlr_api->IO_Ctrl() for additional return error codes.
*
*                           --------------------RETURNED BY p_ctrlr_api->SecRd()---------------------
*                           See p_ctrlr_api->SecRd() for additional return error codes.
*
*                           ------------------RETURNED BY FS_NAND_LowMountHandler()------------------
*                           See FS_NAND_LowMountHandler() for additional return error codes.
*
*                           --------------------RETURNED BY FS_NAND_MetaCommit()---------------------
*                           See FS_NAND_MetaCommit() for additional return error codes.
*
*                           -------------------RETURNED BY p_ctrlr_api->BlkErase()-------------------
*                           See p_ctrlr_api->BlkErase() for additional return error codes.
*
*                           -----------------RETURNED BY FS_NAND_LowUnmountHandler()-----------------
*                           See FS_NAND_LowUnmountHandler() for additional return error codes.
*
*                           -------------------RETURNED BY FS_NAND_LowFmtHandler()-------------------
*                           See FS_NAND_LowFmtHandler() for additional return error codes.
*
*                           --------------------RETURNED BY p_ctrlr_api->SecWr()---------------------
*                           See p_ctrlr_api->SecWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) Defined I/O control operations are :
*
*                   (a) FS_DEV_IO_CTRL_REFRESH           Refresh device.
*                   (b) FS_DEV_IO_CTRL_LOW_FMT           Low-level format device.
*                   (c) FS_DEV_IO_CTRL_LOW_MOUNT         Low-level mount device.
*                   (d) FS_DEV_IO_CTRL_LOW_UNMOUNT       Low-level unmount device.
*                   (e) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level compact device.
*                   (f) FS_DEV_IO_CTRL_LOW_DEFRAG        Low-level defragment device.
*                   (g) FS_DEV_IO_CTRL_SEC_RELEASE       Release data in sector.
*                   (h) FS_DEV_IO_CTRL_PHY_RD            Read  physical device.
*                   (i) FS_DEV_IO_CTRL_PHY_WR            Write physical device.
*                   (j) FS_DEV_IO_CTRL_PHY_RD_PAGE       Read  physical device page.
*                   (k) FS_DEV_IO_CTRL_PHY_WR_PAGE       Write physical device page.
*                   (l) FS_DEV_IO_CTRL_PHY_ERASE_BLK     Erase physical device block.
*
*                   Not all of these operations are valid for all devices.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_IO_Ctrl (FS_DEV      *p_dev,
                                       CPU_INT08U   opt,
                                       void        *p_data,
                                       FS_ERR      *p_err)
{
    FS_NAND_DATA          *p_nand_data;
    FS_NAND_CTRLR_API     *p_ctrlr_api;
    FS_NAND_IO_CTRL_DATA  *p_io_ctrl_data;
    void                  *p_ctrlr_data;

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev == DEF_NULL) {                                    /* Validate dev ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }

    switch (opt) {                                              /* Validate data ptr for cmds that need it.             */
        case FS_DEV_IO_CTRL_RD_SEC         :                    /* Cmds that need data.                                 */
        case FS_DEV_IO_CTRL_WR_SEC         :
        case FS_DEV_IO_CTRL_PHY_ERASE_BLK  :
        case FS_DEV_IO_CTRL_NAND_DUMP      :
             if (p_data == DEF_NULL) {
                *p_err = FS_ERR_NULL_PTR;
                 return;
             }
             break;


        case FS_DEV_IO_CTRL_REFRESH        :                    /* Cmds that don't.                                     */
        case FS_DEV_IO_CTRL_LOW_FMT        :
        case FS_DEV_IO_CTRL_LOW_MOUNT      :
        case FS_DEV_IO_CTRL_LOW_UNMOUNT    :
        case FS_DEV_IO_CTRL_LOW_COMPACT    :
        case FS_DEV_IO_CTRL_LOW_DEFRAG     :
        case FS_DEV_IO_CTRL_SEC_RELEASE    :
        case FS_DEV_IO_CTRL_PHY_RD         :
        case FS_DEV_IO_CTRL_PHY_WR         :
        case FS_DEV_IO_CTRL_SYNC           :
        case FS_DEV_IO_CTRL_CHIP_ERASE     :
        default                            :
             break;
   }
#endif


    p_nand_data  = (FS_NAND_DATA *)p_dev->DataPtr;
    p_ctrlr_api  = p_nand_data->CtrlrPtr;
    p_ctrlr_data = p_nand_data->CtrlrDataPtr;

   *p_err        = FS_ERR_NONE;                                 /* Init err val.                                        */

                                                                /* ------------------ PERFORM I/O CTL ----------------- */
    switch (opt) {
        case FS_DEV_IO_CTRL_LOW_FMT:                            /* Low fmt dev.                                         */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FS_NAND_LowFmtHandler(p_nand_data, p_err);
#else
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
#endif
             break;


        case FS_DEV_IO_CTRL_LOW_MOUNT:                          /* Low mount dev.                                       */
             FS_NAND_LowMountHandler(p_nand_data, p_err);
             break;


        case FS_DEV_IO_CTRL_LOW_UNMOUNT:                        /* Low unmount dev.                                     */
             FS_NAND_LowUnmountHandler(p_nand_data, p_err);
             break;


        case FS_DEV_IO_CTRL_RD_SEC:                             /* Rd dev sec.                                          */
             p_io_ctrl_data = (FS_NAND_IO_CTRL_DATA *)p_data;
             p_ctrlr_api->SecRd(p_ctrlr_data,
                                p_io_ctrl_data->DataPtr,
                                p_io_ctrl_data->OOS_Ptr,
                                p_io_ctrl_data->IxPhy,
                                p_err);
             break;


        case FS_DEV_IO_CTRL_WR_SEC:                             /* Wr dev sec.                                          */
             p_io_ctrl_data = (FS_NAND_IO_CTRL_DATA *)p_data;
             p_ctrlr_api->SecWr(p_ctrlr_data,
                                p_io_ctrl_data->DataPtr,
                                p_io_ctrl_data->OOS_Ptr,
                                p_io_ctrl_data->IxPhy,
                                p_err);
             break;


        case FS_DEV_IO_CTRL_PHY_ERASE_BLK:                      /* Erase dev blk.                                       */
             p_io_ctrl_data = (FS_NAND_IO_CTRL_DATA *)p_data;
             p_ctrlr_api->BlkErase(p_ctrlr_data,
                                   p_io_ctrl_data->IxPhy,
                                   p_err);
             break;


        case FS_DEV_IO_CTRL_REFRESH:                            /* Refresh dev.                                         */
             if (p_nand_data->Fmtd == DEF_YES) {
                *p_err = FS_ERR_NONE;
             } else {
                *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
             }
             break;


        case FS_DEV_IO_CTRL_CHIP_ERASE:                         /* Erase NAND chip.                                     */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FS_NAND_ChipEraseHandler(p_nand_data, p_err);
#else
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
#endif
             break;


        case FS_DEV_IO_CTRL_SYNC:
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FS_NAND_MetaCommit(p_nand_data,
                                DEF_NO,
                                p_err);
#else
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
#endif
             break;

        case FS_DEV_IO_CTRL_NAND_DUMP:
#if (FS_NAND_CFG_DUMP_SUPPORT_EN == DEF_ENABLED)
             FS_NAND_DumpHandler(p_nand_data,
                                 (void (*)(void  *buf, CPU_SIZE_T  len, FS_ERR  *p_err))p_data,
                                 p_err);
#else
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
#endif
             break;

        case FS_DEV_IO_CTRL_PHY_RD:
        case FS_DEV_IO_CTRL_PHY_WR:
        case FS_DEV_IO_CTRL_LOW_COMPACT:
        case FS_DEV_IO_CTRL_LOW_DEFRAG:
        case FS_DEV_IO_CTRL_SEC_RELEASE:
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
             break;


        default:                                                /* Unknown I/O ctrl cmd, forward to ctrlr.              */
             p_ctrlr_api->IO_Ctrl(p_ctrlr_data,
                                  opt,
                                  p_data,
                                  p_err);
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
*                                             FS_NAND_HdrWr()
*
* Description : Creates the NAND device header in specified block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_hdr      Block index of new header block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_FMT  Device formatted, but could NOT be mounted.
*                                   FS_ERR_DEV_INVALID_OP       Device invalid operation.
*                                   FS_ERR_DEV_IO               Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT          Device timeout.
*                                   FS_ERR_NONE                 Device formatted/mounted.
*
*                                   -------------------RETURNED BY p_ctrlr_api->SecWr()--------------------
*                                   See p_ctrlr_api->SecWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_HdrWr (FS_NAND_DATA     *p_nand_data,
                                     FS_NAND_BLK_QTY   blk_ix_hdr,
                                     FS_ERR           *p_err)
{
    FS_NAND_CTRLR_API                    *p_ctrlr_api;
    void                                 *p_ctrlr_data;
    CPU_INT08U                           *p_hdr_data;
    CPU_INT08U                           *p_oos_data;
    FS_NAND_HDR_MARK_1_TYPE               mark1;
    FS_NAND_HDR_MARK_2_TYPE               mark2;
    FS_NAND_HDR_VER_TYPE                  ver;
    FS_NAND_HDR_SEC_SIZE_TYPE             sec_size_hdr;
    FS_NAND_HDR_BLK_CNT_TYPE              blk_cnt_hdr;
    FS_NAND_HDR_BLK_NBR_FIRST_TYPE        blk_nbr_first_hdr;
    FS_NAND_HDR_UB_CNT_TYPE               ub_cnt_hdr;
    FS_NAND_HDR_MAX_ASSOC_TYPE            max_assoc_hdr;
    FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE   avail_blk_tbl_size_hdr;
    FS_NAND_HDR_OOS_SIZE_TYPE             oos_size_hdr;
    FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE      max_bad_blk_cnt_hdr;
    FS_NAND_SEC_TYPE_STO                  sec_type;
    FS_NAND_ERASE_QTY                     erase_cnt;
    FS_SEC_QTY                            sec_ix_phy;


    FS_NAND_TRACE_LOG(("FS_NAND_HdrWr(): Creating device header at block %u.\r\n", blk_ix_hdr));


    p_ctrlr_api  = p_nand_data->CtrlrPtr;
    p_ctrlr_data = p_nand_data->CtrlrDataPtr;
    p_hdr_data   = (CPU_INT08U *)p_nand_data->BufPtr;
    p_oos_data   = (CPU_INT08U *)p_nand_data->OOS_BufPtr;


                                                                /* ----------------- GATHER HDR DATA ------------------ */
                                                                /* Hdr marks.                                           */
    mark1 = FS_NAND_HDR_MARK_WORD1;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_MARK_1_OFFSET],
                                 &mark1,
                                  sizeof(FS_NAND_HDR_MARK_1_TYPE));

    mark2 = FS_NAND_HDR_MARK_WORD2;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_MARK_2_OFFSET],
                                 &mark2,
                                  sizeof(FS_NAND_HDR_MARK_2_TYPE));

                                                                /* Insert FS_NAND_HDR_VER.                              */
    ver = FS_NAND_HDR_VER;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_VER_OFFSET],
                                 &ver,
                                  sizeof(FS_NAND_HDR_VER_TYPE));

                                                                /* Insert FS_NAND_HDR_SEC_SIZE.                         */
    sec_size_hdr = p_nand_data->SecSize;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_SEC_SIZE_OFFSET],
                                 &sec_size_hdr,
                                  sizeof(FS_NAND_HDR_SEC_SIZE_TYPE));

                                                                /* Insert FS_NAND_HDR_BLK_CNT.                          */
    blk_cnt_hdr = p_nand_data->BlkCnt;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_BLK_CNT_OFFSET],
                                 &blk_cnt_hdr,
                                  sizeof(FS_NAND_HDR_BLK_CNT_TYPE));

                                                                /* Insert FS_NAND_HDR_BLK_NBR_FIRST.                    */
    blk_nbr_first_hdr = p_nand_data->BlkIxFirst;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_BLK_NBR_FIRST_OFFSET],
                                 &blk_nbr_first_hdr,
                                  sizeof(FS_NAND_HDR_BLK_NBR_FIRST_TYPE));

                                                                /* Insert FS_NAND_HDR_UB_CNT.                           */
    ub_cnt_hdr = p_nand_data->UB_CntMax;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_UB_CNT_OFFSET],
                                 &ub_cnt_hdr,
                                  sizeof(FS_NAND_HDR_UB_CNT_TYPE));

                                                                /* Insert FS_NAND_HDR_MAX_ASSOC.                        */
    max_assoc_hdr = p_nand_data->RUB_MaxAssoc;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_MAX_ASSOC_OFFSET],
                                 &max_assoc_hdr,
                                  sizeof(FS_NAND_HDR_MAX_ASSOC_TYPE));

                                                                /* Insert FS_NAND_HDR_AVAIL_BLK_TBL_SIZE.               */
    avail_blk_tbl_size_hdr = p_nand_data->AvailBlkTblEntryCntMax;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_OFFSET],
                                 &avail_blk_tbl_size_hdr,
                                  sizeof(FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE));

                                                                /* Insert FS_NAND_HDR_OOS_SIZE.                         */
    oos_size_hdr = FS_NAND_OOS_PARTIAL_SIZE_REQD;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_OOS_SIZE_OFFSET],
                                 &oos_size_hdr,
                                  sizeof(FS_NAND_HDR_OOS_SIZE_TYPE));

                                                                /* Insert FS_NAND_HDR_MAX_BAD_BLK_CNT.                  */
    max_bad_blk_cnt_hdr = p_nand_data->PartDataPtr->MaxBadBlkCnt;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_hdr_data[FS_NAND_HDR_MAX_BAD_BLK_CNT_OFFSET],
                                 &max_bad_blk_cnt_hdr,
                                  sizeof(FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE));


                                                                /* ------------------ CALC OOS DATA ------------------- */
    Mem_Set(&p_oos_data[FS_NAND_OOS_SEC_USED_OFFSET],           /* Insert sec used mark.                                */
             0x00u,
             p_nand_data->UsedMarkSize);

                                                                /* Insert sec type.                                     */
    sec_type = FS_NAND_SEC_TYPE_HDR;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_data[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                 &sec_type,
                                  sizeof(FS_NAND_SEC_TYPE_STO));

                                                                /* Insert erase cnt.                                    */
    erase_cnt = 1u;                                             /* Can't get true erase cnt; dev not low-lvl format'd...*/
                                                                /* ... see FS_NAND_LowFmtHandler() note #5.             */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_data[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                 &erase_cnt,
                                  sizeof(FS_NAND_ERASE_QTY));


                                                                /* --------------- WR DATA TO FIRST SEC --------------- */
    sec_ix_phy = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_hdr);
    p_ctrlr_api->SecWr(p_ctrlr_data,
                       p_hdr_data,
                       p_oos_data,
                       sec_ix_phy,
                       p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrWr(): Unable to write header block.\r\n"));
        return;
    }

}
#endif


/*
*********************************************************************************************************
*                                             FS_NAND_HdrRd()
*
* Description : Finds and reads low-level device header.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_FMT  Device formatted, but could NOT be mounted.
*                                   FS_ERR_DEV_INVALID_OP       Device invalid operation.
*                                   FS_ERR_DEV_IO               Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT          Device timeout.
*                                   FS_ERR_NONE                 Device formatted/mounted.
*
*                                   -------------------RETURNED BY p_ctrlr_api->SecRd()--------------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*                                   ----------------RETURNED BY FS_NAND_HdrParamsValidate()----------------
*                                   See FS_NAND_HdrParamsValidate() for additional return error codes.
*
* Return(s)   : Block index of header          , if an header was found,
*               Block index of first good block, otherwise.
*
* Note(s)     : (1) Device header is always located at first good block in specified block range.
*
*               (2) The search for the device header should not stop if an ECC error is returned by the
*                   SecRd() function. Since the search might need to look in bad blocks, no action should
*                   be taken when an error occurs: the error code is overwritten because of this reason.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_HdrRd (FS_NAND_DATA  *p_nand_data,
                                                FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY           blk_ix_phy;
    FS_NAND_BLK_QTY           last_blk_ix;
    CPU_BOOLEAN               hdr_found;
    FS_NAND_SEC_TYPE          blk_type;
    CPU_INT08U               *p_hdr_data;
    FS_NAND_HDR_MARK_1_TYPE   mark1;
    FS_NAND_HDR_MARK_2_TYPE   mark2;


    last_blk_ix = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
    blk_ix_phy  = p_nand_data->BlkIxFirst;

                                                                /* ------------------ FIND HDR BLK -------------------- */
    hdr_found   = DEF_NO;
    while ((blk_ix_phy <= last_blk_ix) &&
           (hdr_found  == DEF_NO)        ) {
        FS_NAND_SecRdPhyNoRefresh(p_nand_data,                  /* Rd first blk sec.                                    */
                                  p_nand_data->BufPtr,
                                  p_nand_data->OOS_BufPtr,
                                  blk_ix_phy,
                                  0u,
                                  p_err);
        switch (*p_err) {                                       /* Chk type only if sec rd'able.                        */
            case FS_ERR_ECC_UNCORR:
                blk_ix_phy++;
                break;


            case FS_ERR_NONE:
                blk_type = FS_NAND_SecTypeParse((CPU_INT08U *)(p_nand_data->OOS_BufPtr));

                if (blk_type == FS_NAND_SEC_TYPE_HDR) {
                    hdr_found = DEF_YES;
                } else {
                    blk_ix_phy++;
                }
                break;


            default:
                return (FS_NAND_BLK_IX_INVALID);                /* Prevent 'break NOT reachable' compiler warning.      */
        }
       *p_err = FS_ERR_NONE;                                    /* Overwrite err code to continue srch (see Note #2).   */

    }

    if (hdr_found == DEF_NO) {
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
        return (FS_NAND_BLK_IX_INVALID);
    }

                                                                /* ------------------- PARSE HDR ---------------------- */
    p_hdr_data = (CPU_INT08U *)p_nand_data->BufPtr;
    MEM_VAL_COPY_GET_INTU_LITTLE(&mark1,
                                 &p_hdr_data[FS_NAND_HDR_MARK_1_OFFSET],
                                  sizeof(FS_NAND_HDR_MARK_1_TYPE));
    MEM_VAL_COPY_GET_INTU_LITTLE(&mark2,
                                 &p_hdr_data[FS_NAND_HDR_MARK_2_OFFSET],
                                  sizeof(FS_NAND_HDR_MARK_2_TYPE));

    if ((mark1 == FS_NAND_HDR_MARK_WORD1) ||                    /* If hdr mark corresponds ...                          */
        (mark2 == FS_NAND_HDR_MARK_WORD2)   ) {
                                                                /* ... validate low lvl params against hdr.             */
        FS_NAND_HdrParamsValidate(p_nand_data, p_hdr_data, p_err);
        if (*p_err != FS_ERR_NONE) {
            return (blk_ix_phy);
        }
    } else {                                                    /* ... else require low lvl fmt.                        */
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
        return (blk_ix_phy);
    }

                                                                /* Hdr was successfully rd.                             */
    return (blk_ix_phy);
}


/*
*********************************************************************************************************
*                                       FS_NAND_HdrParamValidate()
*
* Description : Validate low-level parameters against those stored in device header.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_hdr_data      Pointer to header data.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS  Low-level parameters don't match those of header.
*                                   FS_ERR_NONE                         Low-level parameters       match those of header.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_HdrParamsValidate (FS_NAND_DATA  *p_nand_data,
                                                 CPU_INT08U        *p_hdr_data,
                                                 FS_ERR            *p_err)
{
    FS_NAND_HDR_VER_TYPE                 ver;
    FS_NAND_HDR_SEC_SIZE_TYPE            sec_size;
    FS_NAND_HDR_BLK_CNT_TYPE             blk_cnt;
    FS_NAND_HDR_BLK_NBR_FIRST_TYPE       blk_nbr_first;
    FS_NAND_HDR_UB_CNT_TYPE              ub_cnt;
    FS_NAND_HDR_MAX_ASSOC_TYPE           max_assoc;
    FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE  avail_blk_tbl_size;
    FS_NAND_HDR_OOS_SIZE_TYPE            oos_size;
    FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE     max_bad_blk_cnt;


    ver                = 0u;
    sec_size           = 0u;
    blk_cnt            = 0u;
    blk_nbr_first      = 0u;
    ub_cnt             = 0u;
    max_assoc          = 0u;
    avail_blk_tbl_size = 0u;
    oos_size           = 0u;
    max_bad_blk_cnt    = 0u;

                                                                 /* Validate FS_NAND_HDR_VER.                           */
    MEM_VAL_COPY_GET_INTU_LITTLE(&ver,
                                 &p_hdr_data[FS_NAND_HDR_VER_OFFSET],
                                  sizeof(FS_NAND_HDR_VER_TYPE));
    if (ver != FS_NAND_HDR_VER) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Version doesn't match: device=%x software=%x.\r\n",
                           ver,
                           FS_NAND_HDR_VER));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_SEC_SIZE.                      */
    MEM_VAL_COPY_GET_INTU_LITTLE(&sec_size,
                                 &p_hdr_data[FS_NAND_HDR_SEC_SIZE_OFFSET],
                                  sizeof(FS_NAND_HDR_SEC_SIZE_TYPE));
    if (sec_size != p_nand_data->SecSize) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Sec size doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->SecSize,
                           sec_size));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_BLK_CNT.                       */
    MEM_VAL_COPY_GET_INTU_LITTLE(&blk_cnt,
                                 &p_hdr_data[FS_NAND_HDR_BLK_CNT_OFFSET],
                                  sizeof(FS_NAND_HDR_BLK_CNT_TYPE));
    if (blk_cnt != p_nand_data->BlkCnt) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Blk cnt doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->BlkCnt,
                           blk_cnt));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_BLK_NBR_FIRST.                 */
    MEM_VAL_COPY_GET_INTU_LITTLE(&blk_nbr_first,
                                 &p_hdr_data[FS_NAND_HDR_BLK_NBR_FIRST_OFFSET],
                                  sizeof(FS_NAND_HDR_BLK_NBR_FIRST_TYPE));
    if (blk_nbr_first != p_nand_data->BlkIxFirst) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): First blk nbr doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->BlkIxFirst,
                           blk_nbr_first));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_UB_CNT.                        */
    MEM_VAL_COPY_GET_INTU_LITTLE(&ub_cnt,
                                 &p_hdr_data[FS_NAND_HDR_UB_CNT_OFFSET],
                                  sizeof(FS_NAND_HDR_UB_CNT_TYPE));
    if (ub_cnt != p_nand_data->UB_CntMax) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Update blk cnt doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->UB_CntMax,
                           ub_cnt));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_MAX_ASSOC.                     */
    MEM_VAL_COPY_GET_INTU_LITTLE(&max_assoc,
                                 &p_hdr_data[FS_NAND_HDR_MAX_ASSOC_OFFSET],
                                  sizeof(FS_NAND_HDR_MAX_ASSOC_TYPE));
    if (max_assoc != p_nand_data->RUB_MaxAssoc) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Max associativity doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->RUB_MaxAssoc,
                           max_assoc));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                 /* Validate FS_NAND_HDR_AVAIL_BLK_TBL_SIZE.            */
    MEM_VAL_COPY_GET_INTU_LITTLE(&avail_blk_tbl_size,
                                 &p_hdr_data[FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_OFFSET],
                                  sizeof(FS_NAND_HDR_AVAIL_BLK_TBL_SIZE_TYPE));
    if (avail_blk_tbl_size != p_nand_data->AvailBlkTblEntryCntMax) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Avail blk tbl size doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           p_nand_data->AvailBlkTblEntryCntMax,
                           avail_blk_tbl_size));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
    }

                                                                 /* Validate FS_NAND_HDR_OOS_SIZE.                      */
    MEM_VAL_COPY_GET_INTU_LITTLE(&oos_size,
                                 &p_hdr_data[FS_NAND_HDR_OOS_SIZE_OFFSET],
                                  sizeof(FS_NAND_HDR_OOS_SIZE_TYPE));
    if (oos_size != FS_NAND_OOS_PARTIAL_SIZE_REQD) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Out of sector data size doesn't match low lvl hdr: actual=%u hdr=%u.\r\n",
                           FS_NAND_OOS_PARTIAL_SIZE_REQD,
                           oos_size));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

                                                                /* Validate FS_NAND_HDR_MAX_BAD_BLK_CNT.                */
    MEM_VAL_COPY_GET_INTU_LITTLE(&max_bad_blk_cnt,
                                 &p_hdr_data[FS_NAND_HDR_MAX_BAD_BLK_CNT_OFFSET],
                                  sizeof(FS_NAND_HDR_MAX_BAD_BLK_CNT_TYPE));
    if (max_bad_blk_cnt != p_nand_data->PartDataPtr->MaxBadBlkCnt) {
        FS_NAND_TRACE_DBG(("FS_NAND_HdrParamsValidate(): Max bad block count doesn't match low level header: actual=%u hdr=%u.\r\n",
                           p_nand_data->PartDataPtr->MaxBadBlkCnt,
                           max_bad_blk_cnt));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }

}


/*
*********************************************************************************************************
*                                         FS_NAND_BlkIxPhyGet()
*
* Description : Get physical index associated with specified logical block index.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_logical  Logical block index.
*
* Return(s)   : Physical block index,       if logical index specified is valid,
*               FS_NAND_BLK_IX_INVALID,     otherwise.
*
* Note(s)     : (1) Logical block mapping is described in 'fs_dev_nand.c Note #4'.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_BlkIxPhyGet (FS_NAND_DATA     *p_nand_data,
                                                      FS_NAND_BLK_QTY   blk_ix_logical)
{
    FS_NAND_BLK_QTY  blk_ix_phy;
    FS_NAND_UB_DATA  ub_tbl_entry;


    if (blk_ix_logical < p_nand_data->LogicalDataBlkCnt) {
                                                                /* Data blks (see Note #1).                             */
        blk_ix_phy = p_nand_data->LogicalToPhyBlkMap[blk_ix_logical];
    } else if (blk_ix_logical < (p_nand_data->LogicalDataBlkCnt + p_nand_data->UB_CntMax)) {
                                                                /* UBs (see Note #1).                                   */
        ub_tbl_entry = FS_NAND_UB_TblEntryRd(p_nand_data, blk_ix_logical - p_nand_data->LogicalDataBlkCnt);
        blk_ix_phy = ub_tbl_entry.BlkIxPhy;
    } else if (blk_ix_logical == p_nand_data->LogicalDataBlkCnt + p_nand_data->UB_CntMax) {
                                                                /* Active meta blk (see Note #1).                       */
        blk_ix_phy = p_nand_data->MetaBlkIxPhy;
    } else {
        return (FS_NAND_BLK_IX_INVALID);
    }

    return (blk_ix_phy);
}


/*
*********************************************************************************************************
*                                          FS_NAND_BlkRefresh()
*
* Description : Copy (backup) all sectors from the target block (blk_ix_phy) to a valid block from the
*               available block table to cope for degraded cell condition.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block's physical index.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*                                   FS_ERR_NONE                     Device formatted/mounted.
*
*                                   -----------------RETURNED BY FS_NAND_SecIsUsed()------------------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   -----------------RETURNED BY p_ctrlr_api->SecRd()-----------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*                                   -----------------RETURNED BY FS_NAND_BlkMarkBad()-----------------
*                                   See FS_NAND_BlkMarkBad() for additional return error codes.
*
*                                   ----------------RETURNED BY FS_NAND_BlkGetErased()----------------
*                                   See FS_NAND_BlkGetErased() for additional return error codes.
*
*                                   -----------------RETURNED BY p_ctrlr_api->SecWr()-----------------
*                                   See p_ctrlr_api->SecWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkRefresh (FS_NAND_DATA     *p_nand_data,
                                          FS_NAND_BLK_QTY   blk_ix_phy,
                                          FS_ERR           *p_err)
{
    FS_NAND_CTRLR_API        *p_ctrlr_api;
    void                     *p_ctrlr_data;
    CPU_INT08U               *p_oos_buf;
    FS_SEC_NBR                blk_sec_ix_phy;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy;
    FS_SEC_NBR                new_sec_ix_phy;
    FS_NAND_BLK_QTY           new_blk_ix_phy;
    FS_NAND_UB_DATA           ub_tbl_entry;
    FS_NAND_ERASE_QTY         erase_cnt;
    CPU_BOOLEAN               uncorrectable_err_occured;
    CPU_BOOLEAN               done;
    CPU_BOOLEAN               fail;
    CPU_BOOLEAN               found;
    CPU_BOOLEAN               sec_used;
    CPU_INT08U                retry_cnt;
    CPU_SIZE_T                i;


    FS_NAND_TRACE_LOG(("FS_NAND_BlkRefresh: Refreshing phy blk %u.\r\n", blk_ix_phy));


    p_ctrlr_api               = p_nand_data->CtrlrPtr;
    p_ctrlr_data              = p_nand_data->CtrlrDataPtr;
    p_oos_buf                 = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    done                      = DEF_NO;

    while (done != DEF_YES) {
        uncorrectable_err_occured = DEF_NO;

                                                                /* ------------------ GET ERASED BLK ------------------ */
        new_blk_ix_phy = FS_NAND_BlkGetErased(p_nand_data, p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FSDev_NAND_BlkRefresh(): Unable to get an erased block.\r\n"));
            return;
        }


                                                                /* ------------------- COPY SECTORS ------------------- */
        sec_offset_phy = 0u;
        blk_sec_ix_phy = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data,  /* Calc old sec ix.                                     */
                                                  blk_ix_phy);
        new_sec_ix_phy = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data,  /* Calc new sec ix.                                     */
                                                  new_blk_ix_phy);

        fail           = DEF_NO;
        while ((sec_offset_phy <  p_nand_data->NbrSecPerBlk) && /* For all sec, unless some op fails.                   */
               (fail           == DEF_NO)                      ) {
            retry_cnt = 0u;
            sec_used  = DEF_YES;
            do {                                                /* Rd sec, retry if neccessary.                         */
                FS_NAND_SecRdPhyNoRefresh((FS_NAND_DATA *)p_nand_data,
                                                          p_nand_data->BufPtr,
                                                          p_oos_buf,
                                                          blk_ix_phy,
                                                          sec_offset_phy,
                                                          p_err);

                if (*p_err == FS_ERR_ECC_UNCORR) {
                    sec_used = FS_NAND_SecIsUsed(p_nand_data,
                                                 blk_sec_ix_phy + sec_offset_phy,
                                                 p_err);
                }
                retry_cnt++;
            }
            while ((retry_cnt <= FS_NAND_CFG_MAX_RD_RETRIES) &&
                   (*p_err    != FS_ERR_NONE)                &&
                   (sec_used  == DEF_YES));

            switch (*p_err) {
                case FS_ERR_ECC_UNCORR:
                     if (sec_used == DEF_YES) {
                         uncorrectable_err_occured = DEF_YES;    /* Data corruption occured.                             */
                         FS_CTR_ERR_INC(p_nand_data->Ctrs.ErrRefreshDataLoss);
                     }
                    *p_err = FS_ERR_NONE;
                     break;


                case FS_ERR_NONE:
                     break;


                default:
                     return;
            }


            if (sec_offset_phy == 0u) {
                erase_cnt = FS_NAND_BlkRemFromAvail(p_nand_data, new_blk_ix_phy);

                MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                             &erase_cnt,
                                              sizeof(FS_NAND_ERASE_QTY));
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
                if (sec_used != DEF_YES) {
                    FS_NAND_TRACE_DBG(("FS_NAND_BlkRefresh(): Fatal error, blk %u has unwritten sector 0.\r\n",
                                       blk_ix_phy));
                   *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                    CPU_SW_EXCEPTION(;);
                }
#endif
            }


            if (sec_used == DEF_YES) {
                p_ctrlr_api->SecWr(p_ctrlr_data,                /* Wr sec in new blk.                                   */
                                   p_nand_data->BufPtr,
                                   p_oos_buf,
                                   new_sec_ix_phy,
                                   p_err);
                switch (*p_err) {
                    case FS_ERR_DEV_IO:                         /* If pgrm op failed ...                                */
                         FS_NAND_TRACE_DBG(("FS_NAND_BlkRefresh(): Sector programming error at sec %u, marking block as bad.\r\n",
                                             new_sec_ix_phy));

                         FS_NAND_BlkMarkBad(p_nand_data,        /* ... mark blk as bad ...                              */
                                            new_blk_ix_phy,
                                            p_err);
                         fail = DEF_YES;                        /* ... and try with another blk.                        */
                         break;


                    case FS_ERR_NONE:
                         break;


                    default:
                         return;
                }

            }

            sec_offset_phy++;                                   /* Inc sec offsets.                                     */
            new_sec_ix_phy++;
        }

        if (fail == DEF_NO) {
            done = DEF_YES;
        }
    }

                                                                /* ---------------- UPDATE METADATA ------------------- */
    found = DEF_NO;
    i = 0u;
    while ((i < p_nand_data->LogicalDataBlkCnt) &&              /* Scan data blk.                                       */
           (found == DEF_NO)                      ) {
        if (p_nand_data->LogicalToPhyBlkMap[i] == blk_ix_phy) { /* If blk was a logical data blk, ...                   */
                                                                /* ... update logical to phy map.                       */
            p_nand_data->LogicalToPhyBlkMap[i] = new_blk_ix_phy;
            found = DEF_YES;
        }

        i++;
    }


    i = 0u;
    while ((found == DEF_NO)                    &&
           (i      < p_nand_data->UB_CntMax)) {
        ub_tbl_entry = FS_NAND_UB_TblEntryRd(p_nand_data, i);

        if (ub_tbl_entry.BlkIxPhy == blk_ix_phy) {              /* If blk was an UB, ...                                */
                                                                /* ... update tbl.                                      */
            FS_NAND_UB_TblEntryWr(p_nand_data, i, new_blk_ix_phy);
            found = DEF_YES;
        }

        i++;
    }

    if (found == DEF_NO) {
        if (blk_ix_phy == p_nand_data->MetaBlkIxPhy) {          /* If blk was the metadata blk ...                      */
            p_nand_data->MetaBlkIxPhy = new_blk_ix_phy;         /* ... update p_nand_data.                              */
        } else {                                                /* ... else blk is unaccounted for.                     */
            FS_NAND_TRACE_DBG(("FS_NAND_BlkRefresh(): Could not find metadata associated with block %u.\r\n",
                               blk_ix_phy));
           *p_err = FS_ERR_DEV_INVALID_METADATA;
            return;
        }
    }


    FS_NAND_TRACE_LOG(("FS_NAND_BlkRefresh: Refresh operation %s: new phy blk ix is %u.\r\n",
                       uncorrectable_err_occured == DEF_NO ? "succeeded" : "failed",
                       new_blk_ix_phy));

    if (uncorrectable_err_occured == DEF_YES) {                 /* If data loss occured, ...                            */
       *p_err = FS_ERR_ECC_UNCORR;
    }

    FS_CTR_STAT_INC(p_nand_data->Ctrs.StatBlkRefreshCtr);

    return;
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_BlkMarkBad()
*
* Description : Add block to bad block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block's physical index.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_OP   Bad blocks table is full.
*                                   FS_ERR_NONE             Operation succeeded.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkMarkBad (FS_NAND_DATA     *p_nand_data,
                                          FS_NAND_BLK_QTY   blk_ix_phy,
                                          FS_ERR           *p_err)
{
    FS_NAND_BLK_QTY  *p_bad_blk_tbl;
    FS_NAND_BLK_QTY   tbl_ix;
    FS_NAND_BLK_QTY   tbl_entry;
    CPU_BOOLEAN       found_slot;


    p_bad_blk_tbl = (FS_NAND_BLK_QTY *)&p_nand_data->MetaCache[p_nand_data->MetaOffsetBadBlkTbl];

                                                                /* ------------ LOOK FOR AVAIL SLOT IN TBL ------------ */
    tbl_ix     = 0u;
    found_slot = DEF_NO;
    while ((tbl_ix < p_nand_data->PartDataPtr->MaxBadBlkCnt) &&
           (found_slot == DEF_NO)                              ) {

        MEM_VAL_COPY_GET_INTU_LITTLE(&tbl_entry,
                                     &p_bad_blk_tbl[tbl_ix],
                                      sizeof(FS_NAND_BLK_QTY));

        if (tbl_entry == blk_ix_phy) {
            FS_NAND_TRACE_DBG(("FS_NAND_MarkBlkBad(): Block %u is already in bad blk tbl.\r\n", blk_ix_phy));
            return;
        } else if (tbl_entry != FS_NAND_BLK_IX_INVALID) {
            tbl_ix++;
        } else {
            found_slot = DEF_YES;
        }
    }


    if (found_slot != DEF_YES) {                                /* No slot found.                                       */
        FS_NAND_TRACE_DBG(("FS_NAND_BlkMarkBad(): Unable to add entry; bad block table is full.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_OP;
        return;
    }


                                                                /* Add blk to bad blk tbl.                              */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_bad_blk_tbl[tbl_ix],
                                 &blk_ix_phy,
                                  sizeof(FS_NAND_BLK_QTY));

    FS_NAND_BlkUnmap(p_nand_data,                               /* Unmap blk.                                           */
                     blk_ix_phy);

    FS_NAND_BadBlkTblInvalidate(p_nand_data);



}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_BlkMarkDirty()
*
* Description : Add block to dirty block bitmap.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block's physical index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkMarkDirty (FS_NAND_DATA     *p_nand_data,
                                            FS_NAND_BLK_QTY   blk_ix_phy)
{
    CPU_SIZE_T   offset_octet;
    CPU_INT08U   offset_bit;
    CPU_INT08U  *p_dirty_map;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------ ARG VALIDATION ------------------ */
    if ((blk_ix_phy  < p_nand_data->BlkIxFirst) ||              /* Validate blk_ix_phy.                                 */
        (blk_ix_phy >= p_nand_data->BlkCnt + p_nand_data->BlkIxFirst)) {
        CPU_SW_EXCEPTION(;);
    }
#endif

    FS_NAND_TRACE_LOG(("Marking blk %u dirty.\r\n", blk_ix_phy));

                                                                /* Calc bit pos.                                        */
    blk_ix_phy   -= p_nand_data->BlkIxFirst;
    offset_octet  = blk_ix_phy  / DEF_OCTET_NBR_BITS;
    offset_bit    = blk_ix_phy  % DEF_OCTET_NBR_BITS;

                                                                /* Set bit.                                             */
    p_dirty_map  = &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap];

    p_dirty_map[offset_octet] |= DEF_BIT(offset_bit);


    FS_NAND_DirtyMapInvalidate(p_nand_data);                    /* Invalidate bitmap.                                   */
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_BlkUnmap()
*
* Description : Unmaps a block, removing it from (1) Available           blocks table,
*                                                (2) Update              blocks table,
*                                                (3) Dirty               blocks bitmap,
*                                                (4) Logical to physical blocks map.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block's physical index.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkUnmap (FS_NAND_DATA     *p_nand_data,
                                        FS_NAND_BLK_QTY   blk_ix_phy)
{
    CPU_INT08U               *p_metadata;
    CPU_SIZE_T                i;
    FS_NAND_BLK_QTY           blk_ix;
    CPU_SIZE_T                offset_octet;
    CPU_INT08U                offset_bit;
    FS_NAND_UB_DATA           ub_tbl_entry;
    FS_NAND_AVAIL_BLK_ENTRY   avail_blk_tbl_entry;
    CPU_BOOLEAN               srch_done;


                                                                /* ---------------- UNMAP FROM DATA BLK --------------- */
    i         = 0u;
    srch_done = DEF_NO;
    while ((i < p_nand_data->LogicalDataBlkCnt) && (srch_done != DEF_YES)) {
        if (p_nand_data->LogicalToPhyBlkMap[i]  == blk_ix_phy) {
            p_nand_data->LogicalToPhyBlkMap[i]   = FS_NAND_BLK_IX_INVALID;
            srch_done = DEF_YES;
        }
        i++;
    }


                                                                /* ----------------- UNMAP FROM UB TBL ---------------- */
    i          = 0u;
    srch_done  = DEF_NO;
    p_metadata = &p_nand_data->MetaCache[p_nand_data->MetaOffsetUB_Tbl];
    while ((i < p_nand_data->UB_CntMax) && (srch_done != DEF_YES)) {
        ub_tbl_entry = FS_NAND_UB_TblEntryRd(p_nand_data, i);
        if (ub_tbl_entry.BlkIxPhy == blk_ix_phy) {
            FS_NAND_UB_TblEntryWr(p_nand_data, i, FS_NAND_BLK_IX_INVALID);
            srch_done = DEF_YES;
        }
        i++;
    }

                                                                /* ------------- UNMAP FROM AVAIL BLK TBL ------------- */
    i          = 0u;
    srch_done  = DEF_NO;
    while ((i < p_nand_data->AvailBlkTblEntryCntMax) && (srch_done != DEF_YES)) {
        avail_blk_tbl_entry = FS_NAND_AvailBlkTblEntryRd(p_nand_data, i);
        if (avail_blk_tbl_entry.BlkIxPhy == blk_ix_phy) {
            avail_blk_tbl_entry.BlkIxPhy  = FS_NAND_BLK_IX_INVALID;

            FS_NAND_TRACE_LOG(("Removing blk %u from avail blk tbl at ix %u\r\n.",
                               blk_ix_phy,
                               i));

            FS_NAND_AvailBlkTblEntryWr(p_nand_data, i, avail_blk_tbl_entry);
            FS_NAND_AvailBlkTblInvalidate(p_nand_data);
            srch_done = DEF_YES;
        }
        i++;
    }

                                                                /* ----------- UNMAP FROM DIRTY BLK BITMAP ------------ */
    p_metadata   = &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap];

    blk_ix       = blk_ix_phy - p_nand_data->BlkIxFirst;
    offset_octet = blk_ix     / DEF_OCTET_NBR_BITS;
    offset_bit   = blk_ix     % DEF_OCTET_NBR_BITS;

    p_metadata[offset_octet] &= ~DEF_BIT(offset_bit);
    FS_NAND_DirtyMapInvalidate(p_nand_data);

}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_BlkAddToAvail()
*
* Description : Add specified block to available blocks table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block physical index.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_OP_ABORTED   Block NOT added; table is full.
*                                   FS_ERR_NONE             Block successfully added.
*
* Return(s)   : none.
*
* Note(s)     : (1) If a block erase count can't be read, the erase count is reset to 0. This case will
*                   mostly not happen. However, if it happens, it is preferable to ignore the error. The
*                   block will still be usable. If the block is bad, following operations on it will
*                   soon detect it and mark it bad.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkAddToAvail (FS_NAND_DATA     *p_nand_data,
                                             FS_NAND_BLK_QTY   blk_ix_phy,
                                             FS_ERR           *p_err)
{
    CPU_INT08U               *p_oos_buf;
    FS_NAND_AVAIL_BLK_ENTRY   entry;
    FS_NAND_BLK_QTY           avail_blk_tbl_ix;
    FS_NAND_BLK_QTY           avail_blk_tbl_ix_free;
    FS_NAND_SEC_TYPE          blk_type;
    CPU_BOOLEAN               is_meta;
    FS_ERR                    err;


    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;
    err       = FS_ERR_NONE;


                                                                /* --------------------- SCAN TBL --------------------- */
    avail_blk_tbl_ix_free = FS_NAND_BLK_IX_INVALID;
    avail_blk_tbl_ix      = 0u;
    while ((avail_blk_tbl_ix_free == FS_NAND_BLK_IX_INVALID) &&
           (avail_blk_tbl_ix      <  p_nand_data->AvailBlkTblEntryCntMax)) {
        entry = FS_NAND_AvailBlkTblEntryRd(p_nand_data, avail_blk_tbl_ix);

        if (entry.BlkIxPhy == blk_ix_phy) {                     /* Entry already in tbl.                                */
            return;
        } else if (entry.BlkIxPhy == FS_NAND_BLK_IX_INVALID) {
            avail_blk_tbl_ix_free = avail_blk_tbl_ix;           /* Found free entry in avail blk tbl.                   */
        } else {
            avail_blk_tbl_ix++;
        }
    }

    if (avail_blk_tbl_ix_free == FS_NAND_BLK_IX_INVALID) {
       *p_err = FS_ERR_DEV_OP_ABORTED;
        FS_NAND_TRACE_DBG(("FS_NAND_BlkAddToAvail(): No more space in available block table.\r\n"));
        return;
    }

    FS_NAND_TRACE_LOG(("Adding blk %u to avail blk tbl at ix %d.\r\n", blk_ix_phy, avail_blk_tbl_ix_free));



                                                                /* ------------------ GET ERASE CNT ------------------- */
    FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                              p_nand_data->BufPtr,
                              p_nand_data->OOS_BufPtr,
                              blk_ix_phy,
                              0u,
                             &err);
    if (err == FS_ERR_NONE) {                                   /* If sec is rd'able ...                                */
                                                                /* ... read erase cnt from sec.                         */
        MEM_VAL_COPY_GET_INTU_LITTLE(&entry.EraseCnt,
                                     &p_oos_buf[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                      sizeof(FS_NAND_ERASE_QTY));

        blk_type = FS_NAND_SEC_TYPE_INVALID;
        MEM_VAL_COPY_GET_INTU_LITTLE(&blk_type,
                                     &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                     sizeof(FS_NAND_SEC_TYPE_STO));

        is_meta = (blk_type == FS_NAND_SEC_TYPE_METADATA);

                                                                /* Overwrite invalid erase cnt.                         */
        if (entry.EraseCnt == FS_NAND_ERASE_QTY_INVALID) {
            entry.EraseCnt  = 0u;
        }


    } else if (err == FS_ERR_ECC_UNCORR) {                      /* ... else if uncorrectable ecc err ...                */
        entry.EraseCnt = 0;                                     /* ... assume blk is brand new (see Note #1).           */
        is_meta        = DEF_NO;
    } else {
       *p_err = FS_ERR_DEV_OP_ABORTED;
        FS_NAND_TRACE_DBG(("FS_NAND_BlkAddToAvail(): Unexpected error reading sector 0 in block %u.\r\n",
                           blk_ix_phy));
        return;
    }

                                                                /* ----------------- ADD ENTRY TO TBL ----------------- */
                                                                /* Add entry to tbl.                                    */
    entry.BlkIxPhy = blk_ix_phy;


    if (is_meta) {                                              /* Indicate whether the added block is a metadata block.*/
        FSUtil_MapBitSet(p_nand_data->AvailBlkMetaMap, avail_blk_tbl_ix_free);
        MEM_VAL_COPY_GET_INTU_LITTLE(&p_nand_data->AvailBlkMetaID_Tbl[avail_blk_tbl_ix_free],
                                     &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                     sizeof(FS_NAND_META_ID));
    } else {
        FSUtil_MapBitClr(p_nand_data->AvailBlkMetaMap, avail_blk_tbl_ix_free);
    }

                                                                /* Add avail tbl entry.                                 */
    FS_NAND_AvailBlkTblEntryWr(p_nand_data, avail_blk_tbl_ix_free, entry);

    FS_NAND_AvailBlkTblInvalidate(p_nand_data);                 /* Invalidate tbl.                                      */
}
#endif

/*
*********************************************************************************************************
*                                       FS_NAND_BlkRemFromAvail()
*
* Description : Remove specified block from available blocks table and return its stored erase count.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block physical index.
*
*
* Return(s)   : erase count of specified block, if the block was found in the table;
*               FS_NAND_ERASE_QTY_INVALID     , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_ERASE_QTY  FS_NAND_BlkRemFromAvail (FS_NAND_DATA     *p_nand_data,
                                                            FS_NAND_BLK_QTY   blk_ix_phy)
{
    FS_NAND_BLK_QTY          avail_blk_tbl_ix;
    FS_NAND_AVAIL_BLK_ENTRY  avail_blk;
    FS_NAND_ERASE_QTY        erase_cnt;


    erase_cnt        = FS_NAND_ERASE_QTY_INVALID;
    avail_blk_tbl_ix = 0u;
                                                            /* Find entry in tbl.                                   */
    while ((avail_blk_tbl_ix < p_nand_data->AvailBlkTblEntryCntMax) &&
           (erase_cnt == FS_NAND_ERASE_QTY_INVALID)) {
        avail_blk = FS_NAND_AvailBlkTblEntryRd(p_nand_data, avail_blk_tbl_ix);

        if (avail_blk.BlkIxPhy == blk_ix_phy) {
                                                            /* Get erase cnt.                                       */
            erase_cnt = avail_blk.EraseCnt + 1u;

            FS_NAND_TRACE_LOG(("Removing blk %u from avail blk tbl at ix %d.\r\n",
                               blk_ix_phy,
                               avail_blk_tbl_ix));

                                                            /* Rem entry from tbl.                                  */
            avail_blk.BlkIxPhy = FS_NAND_BLK_IX_INVALID;
            FS_NAND_AvailBlkTblEntryWr(p_nand_data, avail_blk_tbl_ix, avail_blk);
            FS_NAND_AvailBlkTblInvalidate(p_nand_data);
        }

        avail_blk_tbl_ix++;
    }

    if (erase_cnt == FS_NAND_ERASE_QTY_INVALID) {
        FS_NAND_TRACE_DBG(("FS_NAND_BlkRemFromAvail(): Unable to find available blocks table entry.\r\n"));
    }

    return (erase_cnt);
}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_BlkGetAvailFromTbl()
*
* Description : Get an available block from the available block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               access_rsvd     DEF_YES, if access to reserved blocks is allowed,
*                               DEF_NO , otherwise.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_IO       Device operation failed.
*                                   FS_ERR_DEV_TIMEOUT  Device timeout.
*                                   FS_ERR_NONE         Operation was successful.
*
* Return(s)   : Block index of available block,     if a block was found,
*               FS_NAND_BLK_IX_INVALID,             otherwise.
*
* Note(s)     : (1) An available block might not be erased. Use FS_NAND_BlkEnsureErased() to make sure it's
*                   erased.
*
*               (2) Some available blocks are reserved for metadata blocks folding. Those entries will only
*                   be returned if access_rsvd has value DEF_YES. Only FS_NAND_MetaBlkFold() should
*                   call this function with access_rsvd set to DEF_YES.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_BlkGetAvailFromTbl (FS_NAND_DATA  *p_nand_data,
                                                             CPU_BOOLEAN    access_rsvd,
                                                             FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY          tbl_ix;
    FS_NAND_BLK_QTY          tbl_ix_committed;
    FS_NAND_BLK_QTY          blk_ix_phy;
    FS_NAND_BLK_QTY          blk_ix_phy_not_committed;
    FS_NAND_BLK_QTY          blk_ix_phy_committed;
    FS_NAND_BLK_QTY          nbr_entries;
    FS_NAND_BLK_QTY          nbr_entries_committed;
    FS_NAND_AVAIL_BLK_ENTRY  tbl_entry;
    FS_NAND_ERASE_QTY        min_erase_cnt;
    FS_NAND_ERASE_QTY        min_erase_cnt_committed;
    CPU_BOOLEAN              is_entry_committed;
    CPU_BOOLEAN              is_meta;
    CPU_BOOLEAN              is_stale_meta;
    FS_NAND_META_ID          meta_id_delta;


    min_erase_cnt            = DEF_GET_U_MAX_VAL(min_erase_cnt);
    min_erase_cnt_committed  = DEF_GET_U_MAX_VAL(min_erase_cnt_committed);
    nbr_entries              = 0u;
    nbr_entries_committed    = 0u;
    blk_ix_phy               = FS_NAND_BLK_IX_INVALID;
    blk_ix_phy_not_committed = FS_NAND_BLK_IX_INVALID;
    blk_ix_phy_committed     = FS_NAND_BLK_IX_INVALID;
    tbl_ix_committed         = FS_NAND_BLK_IX_INVALID;


                                                                /* -------------------- FIND BLKS --------------------- */
    for (tbl_ix = 0u; tbl_ix < p_nand_data->AvailBlkTblEntryCntMax; tbl_ix++) {
        tbl_entry = FS_NAND_AvailBlkTblEntryRd(p_nand_data, tbl_ix);

        if (tbl_entry.BlkIxPhy != FS_NAND_BLK_IX_INVALID) {
            nbr_entries++;
                                                                /* Chk if entry is committed.                           */
            is_entry_committed = FSUtil_MapBitIsSet(p_nand_data->AvailBlkTblCommitMap, tbl_ix);
            is_meta            = FSUtil_MapBitIsSet(p_nand_data->AvailBlkMetaMap, tbl_ix);
            is_stale_meta      = DEF_NO;

            if (is_meta) {
                meta_id_delta = p_nand_data->MetaBlkID - p_nand_data->AvailBlkMetaID_Tbl[tbl_ix];
                is_stale_meta = (meta_id_delta > FS_NAND_META_ID_STALE_THRESH);
            }


            if (is_entry_committed == DEF_YES) {
                nbr_entries_committed++;
                                                                /* Find committed avail blk with lowest erase cnt.      */
                if (is_stale_meta) {
                    min_erase_cnt_committed = 0u;
                    blk_ix_phy_committed    = tbl_entry.BlkIxPhy;
                } else if (tbl_entry.EraseCnt < min_erase_cnt_committed) {
                    min_erase_cnt_committed  = tbl_entry.EraseCnt;
                    blk_ix_phy_committed     = tbl_entry.BlkIxPhy;
                    tbl_ix_committed         = tbl_ix;
                }

            } else {
                                                                /* Find avail blk with lowest erase cnt.      */
                if (is_stale_meta) {
                    min_erase_cnt            = 0u;
                    blk_ix_phy_not_committed = tbl_entry.BlkIxPhy;
                } else if (tbl_entry.EraseCnt < min_erase_cnt) {
                    min_erase_cnt            = tbl_entry.EraseCnt;
                    blk_ix_phy_not_committed = tbl_entry.BlkIxPhy;
                }
            }

        }

    }


                                                                /* --------------- DETERMINE BLK TO USE --------------- */
    if (access_rsvd == DEF_YES) {
        blk_ix_phy = blk_ix_phy_committed;
                                                                /* Prevent entry from being used again.                 */
        FSUtil_MapBitClr(p_nand_data->AvailBlkTblCommitMap, tbl_ix_committed);

    } else if (nbr_entries_committed > FS_NAND_CFG_RSVD_AVAIL_BLK_CNT) {
        if ( min_erase_cnt_committed < min_erase_cnt) {
            blk_ix_phy = blk_ix_phy_committed;
                                                                /* Prevent entry from being used again.                 */
            FSUtil_MapBitClr(p_nand_data->AvailBlkTblCommitMap, tbl_ix_committed);
        } else {
            blk_ix_phy = blk_ix_phy_not_committed;
        }

    } else {
        blk_ix_phy = blk_ix_phy_not_committed;
        FS_NAND_TRACE_DBG(("FS_NAND_BlkGetAvailFromTbl(): Warning -- unable to get a committed available block table entry "
                           "-- using an uncommitted entry.\r\n"));
    }

    if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {                 /* If no blk was found ...                              */
       *p_err = FS_ERR_DEV_NAND_NO_AVAIL_BLK;
        return (FS_NAND_BLK_IX_INVALID);                        /* ... return with err.                                 */

    } else {
        return (blk_ix_phy);                                    /* Found accessible blk.                                */
    }

}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_BlkGetDirty()
*
* Description : Get a dirty block from dirty blocks bitmap.
*
* Argument(s) : p_nand_data             Pointer to NAND data.
*               -----------             Argument validated by caller.
*
*               pending_dirty_chk_en    Enable/disable pending check of dirty blocks. (see Note #1a).
*
*               p_err                   Pointer to variable that will receive the return error code from this function :
*               -----                   Argument validated by caller.
*
*                                           FS_ERR_DEV_NAND_NO_AVAIL_BLK    No more blocks available.
*                                           FS_ERR_NONE                     Operation was successful.
*
*                                           ---------RETURNED BY FS_NAND_SecRdPhyNoRefresh()---------
*                                           See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
*                                           ------------RETURNED BY FS_NAND_MetaSecFind()------------
*                                           See FS_NAND_MetaSecFind() for additional return error codes.
*
* Return(s)   : Physical block index of dirty block,    if a block was found,
*               FS_NAND_BLK_IX_INVALID,                 otherwise.
*
* Note(s)     : (1) Care must be taken to ensure that the choosen dirty block is not a "pending dirty"
*                   block, i.e. that the block has been marked dirty but the change hasn't yet been
*                   committed to the metadata block. Taking such a block would result in potential data
*                   loss on unexpected power loss.
*
*                   (a) The argument 'pending_dirty_chk_en' should be always be set to DEF_ENABLED to
*                       enable the proper check of "pending dirty blocks". However, when low-level
*                       formatting, the argument must be set to DEF_DISABLED, because all blocks
*                       are dirty but none have been committed to the device yet.
*
*               (2) Dirty block selection is based on round-robin like method. This method could be changed
*                   to select the dirty block with minimum erase count, but it would affect performance.
*
*               (3) If the metadata is corrupted and cannot be read from the device, we must ensure that
*                   it is committed (rewritten on device) before the next shutdown or power loss occur.
*                   For this behavior to happen, we must complete the operation successfully by clearing
*                   the error code.
*********************************************************************************************************
*/
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_BlkGetDirty (FS_NAND_DATA  *p_nand_data,
                                                      CPU_BOOLEAN    pending_dirty_chk_en,
                                                      FS_ERR        *p_err)
{
    CPU_INT08U            *p_dirty_map;
    FS_NAND_BLK_QTY        blk_ix_phy;
    CPU_SIZE_T             word_ix;
    CPU_SIZE_T             word_ix_wrap;
    CPU_SIZE_T             word_ix_start;
    CPU_INT32U             word;
    CPU_INT32U             mask;
    CPU_INT08U             word_offset;
    CPU_INT08U             bit_pos;
    CPU_INT08U             mask_end_pos;
    CPU_BOOLEAN            dirty_found;
    CPU_BOOLEAN            meta_found;
    CPU_SIZE_T             meta_offset;
    CPU_INT08U             meta_byte;
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED)
    FS_NAND_META_SEC_QTY   meta_sec_ix;
    CPU_INT08U            *p_buf;
    FS_SEC_QTY             sec_offset_phy;
#endif


    p_dirty_map    = &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap];


                                                                /* ------------- SCAN TBL FOR A DIRTY BLK ------------- */
    dirty_found    = DEF_NO;
    meta_found     = DEF_NO;
    word_ix_start  = p_nand_data->DirtyBitmapSrchPos >> 5u;
    word_ix_wrap   = (p_nand_data->BlkCnt - 1u)      >> 5u;
    word_ix        = word_ix_start;
    word_offset    = p_nand_data->DirtyBitmapSrchPos &  DEF_BIT_FIELD(5u, 0u);
    do {

        bit_pos = word_offset;

                                                                /* Get a 32 bits word from bitmap.                      */
        MEM_VAL_COPY_GET_INT32U_LITTLE(&word, &p_dirty_map[word_ix * 4u]);

        if (word_ix == word_ix_wrap) {                          /* If last word  ...                                    */
                                                                /* ... adjust mask to cover only what remains.          */
            mask_end_pos = p_nand_data->BlkCnt & DEF_BIT_FIELD(5u, 0u);
            if (mask_end_pos == 0u) {
                mask_end_pos = 32u;
            }
        } else {
            mask_end_pos = 32u;                                 /* Don't care if the end pos goes beyond srch start ix. */
        }

                                                                /* Calc and apply mask.                                 */
        mask  = DEF_BIT_FIELD((CPU_INT08U)(mask_end_pos - word_offset), word_offset);
        word &= mask;

        while ((word != 0u) &&                                  /* If not null, there is a dirty blk in range.          */
               (dirty_found == DEF_NO)) {
            bit_pos = CPU_CntTrailZeros32(word);                /* Find least significant bit set in word ...           */
                                                                /* ... See CPU_CntTrailZeros32() Note #2.               */

            if (pending_dirty_chk_en == DEF_DISABLED) {         /* If pending dirty chk is disabled (see Note #1a) ...  */
                dirty_found = DEF_YES;
            } else {

                                                                /* ------- CHK THAT BLK IS NOT "PENDING" DIRTY -------- */
                                                                /* Chk if dirty status was committed.                   */
                                                                /* Calc offset in metadata info.                        */
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED)
                meta_offset  = p_nand_data->MetaOffsetDirtyBitmap;
#else
                meta_offset  = 0u;
#endif
                meta_offset += word_ix * FS_NAND_MAP_SRCH_SIZE_OCTETS;
                meta_offset += bit_pos >> DEF_OCTET_TO_BIT_SHIFT;
                meta_offset %= p_nand_data->SecSize;            /* Calc octet offset in metadata sec.                   */

#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED)
                meta_sec_ix  = meta_offset / p_nand_data->SecSize; /* Calc sec offset in metadata info.                 */
                                                                   /* Find metadata sec.                                */
                sec_offset_phy = FS_NAND_MetaSecFind(p_nand_data,
                                                     meta_sec_ix,
                                                     p_err);
                if (*p_err != FS_ERR_NONE) {
                    return (FS_NAND_BLK_IX_INVALID);
                } else if (sec_offset_phy == FS_NAND_SEC_OFFSET_IX_INVALID) {
                    FS_NAND_TRACE_DBG(("FS_NAND_BlkGetDirty(): Unable to find metadata sector %u. Data loss may occur if "
                                       "a power loss occurs during brief period following this warning.\r\n",
                                        meta_sec_ix));
                   *p_err = FS_ERR_NONE;
                }

                p_buf       = (CPU_INT08U *)p_nand_data->BufPtr;


                FS_NAND_SecRdPhyNoRefresh(p_nand_data,          /* Rd sec in meta blk.                                  */
                                          p_buf,
                                          p_nand_data->OOS_BufPtr,
                                          p_nand_data->MetaBlkIxPhy,
                                          sec_offset_phy,
                                          p_err);
                if (*p_err == FS_ERR_ECC_UNCORR) {
                                                                /* Meta is unreadable (see note #3).                    */
                    FS_NAND_TRACE_DBG(("FS_NAND_BlkGetDirty(): Unable to read metadata sector. Filesystem could be corrupted "
                                       "if a power loss occur before the metadata is committed.\r\n",
                                        meta_sec_ix));
                   *p_err = FS_ERR_NONE;
                    dirty_found = DEF_YES;
                } else if (*p_err != FS_ERR_NONE) {
                    return (FS_NAND_BLK_IX_INVALID);
                } else {
                    meta_byte  = p_buf[meta_offset];            /* Rd dirty bitmap byte from dev.                       */
                    meta_found = DEF_YES;
                }
#else                                                           /* Rd dirty bitmap byte from cache.                     */
                meta_byte  = p_nand_data->DirtyBitmapCache[meta_offset];
                meta_found = DEF_YES;
#endif

                if (meta_found == DEF_YES) {
                                                                /* If bit is set ...                                    */
                    if (DEF_BIT_IS_SET(meta_byte, DEF_BIT(bit_pos & DEF_OCTET_TO_BIT_MASK))) {
                        dirty_found = DEF_YES;                  /* ... we have found a real dirty blk.                  */
                    } else {
                        word &= ~DEF_BIT(bit_pos);              /* ... else, clr bit at tested pos.                     */
                    }
                }
            }
        }

        if (dirty_found == DEF_NO) {
            word_ix     = (word_ix == word_ix_wrap) ? 0u : word_ix + 1u;
            word_offset = 0u;
        }

    } while ((word_ix     != word_ix_start) &&                  /* Loop until whole bitmap has been srch'd or blk found.*/
             (dirty_found == DEF_NO)          );

    p_nand_data->DirtyBitmapSrchPos = (word_ix << 5u) + bit_pos + 1u;
    if (p_nand_data->DirtyBitmapSrchPos == p_nand_data->BlkCnt) {
        p_nand_data->DirtyBitmapSrchPos = 0u;
    }



    if (dirty_found == DEF_NO) {
       *p_err = FS_ERR_DEV_NAND_NO_AVAIL_BLK;
        return (FS_NAND_BLK_IX_INVALID);
    }

                                                                /* Convert pos in dirty blk bitmap to blk phy ix.       */
    blk_ix_phy  = (word_ix * FS_NAND_MAP_SRCH_SIZE_BITS) + bit_pos;
    blk_ix_phy += p_nand_data->BlkIxFirst;

    if (blk_ix_phy >= p_nand_data->BlkIxFirst + p_nand_data->BlkCnt) {
        *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
         return (FS_NAND_BLK_IX_INVALID);
    }

    return (blk_ix_phy);
}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_BlkGetErased()
*
* Description : Get a block from available block table. Erase it if not already erased.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   -RETURNED BY FS_NAND_BlkGetAvailFromTbl()-
*                                   See FS_NAND_BlkGetAvailFromTbl() for additional return error codes.
*
*                                   --RETURNED BY FS_NAND_AvailBlkTblFill()--
*                                   See FS_NAND_AvailBlkTblFill() for additional return error codes.
*
*                                   ----RETURNED BY FS_NAND_MetaCommit()-----
*                                   See FS_NAND_MetaCommit() for additional return error codes.
*
*                                   --RETURNED BY FS_NAND_BlkEnsureErased()--
*                                   See FS_NAND_BlkEnsureErased() for additional return error codes.
*
*                                   -RETURNED BY FS_NAND_AvailBlkTblTmpCommit()-
*                                   See FS_NAND_AvailBlkTblTmpCommit() for additional return error codes.
*
* Return(s)   : Block index in available blocks table,  if a block was found;
*               FS_NAND_BLK_IX_INVALID,                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_BlkGetErased (FS_NAND_DATA  *p_nand_data,
                                                       FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY  blk_ix_phy;
    FS_NAND_BLK_QTY  avail_blk_cnt;
    CPU_BOOLEAN      meta_committed;
    CPU_BOOLEAN      done;


    meta_committed = DEF_NO;
    done           = DEF_NO;
    do {                                                        /* Until blk can be erased successfully.                */
       *p_err = FS_ERR_NONE;

                                                                /* Fill avail blk tbl.                                  */
        FS_NAND_AvailBlkTblFill(p_nand_data, FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1u, DEF_ENABLED, p_err);
        switch (*p_err) {
            case FS_ERR_DEV_NAND_NO_AVAIL_BLK:
                if (meta_committed == DEF_NO) {
                                                                /* Commit metadata: switch 'pending' dirty to dirty.    */
                    FS_NAND_MetaCommit(p_nand_data, DEF_NO, p_err);
                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Fatal error committing metadata.\r\n"));
                        return (FS_NAND_BLK_IX_INVALID);
                    }
                    meta_committed = DEF_YES;
                } else {                                        /* Fails since a meta commit already has been tried.    */
                    FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Unable to fill available block table; device is full.\r\n"));
                    return (FS_NAND_BLK_IX_INVALID);
                }
                break;


            case FS_ERR_NONE:
                break;


            default:
                FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Error filling available block table.\r\n"));
                return (FS_NAND_BLK_IX_INVALID);                /* Prevent 'break NOT reachable' compiler warning.      */
        }


                                                                /* Commit avail blk tbl on dev if invalid.              */
                                                                /* This might cause the new avail blk to be used.       */
        if (p_nand_data->AvailBlkTblInvalidated == DEF_YES) {
            FS_NAND_AvailBlkTblTmpCommit(p_nand_data, p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Error committing available block table to device.\r\n"));
                return (FS_NAND_BLK_IX_INVALID);
            }
        }

                                                                /* Chk if we still have enough avail blks.              */
        avail_blk_cnt = FS_NAND_AvailBlkTblEntryCnt(p_nand_data);
        if (avail_blk_cnt >= FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1u) {
                                                                /* Get a new avail blk.                                 */
            blk_ix_phy = FS_NAND_BlkGetAvailFromTbl(p_nand_data, DEF_NO, p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Fatal error getting an available block."));
                return (FS_NAND_BLK_IX_INVALID);
            }

                                                                /* Make sure blk is erased.                             */
            FS_NAND_BlkEnsureErased(p_nand_data, blk_ix_phy, p_err);
            switch (*p_err) {
                case FS_ERR_NONE:
                     done = DEF_YES;
                     break;

                case FS_ERR_DEV_IO:
                     break;                                 /* Go through another loop iter.                        */

                default:
                     FS_NAND_TRACE_DBG(("FS_NAND_BlkGetErased(): Error ensuring blk %u is erased.\r\n", blk_ix_phy));
                     return (FS_NAND_BLK_IX_INVALID);       /* Prevent 'break NOT reachable' compiler warning.      */
            }
        }

    } while ((done != DEF_YES));

    return (blk_ix_phy);
}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_BlkEnsureErased()
*
* Description : Make sure a block from the available block table is really erased. Erase it if it isn't.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Physical block index.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE             Operation was successful.
*
*                                   ---------RETURNED BY FS_NAND_SecIsUsed()---------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   ------RETURNED BY FS_NAND_BlkEraseHandler()------
*                                   See FS_NAND_BlkEraseHandler() for additional return error codes.
*
*                                   --------RETURNED BY FS_NAND_BlkMarkBad()---------
*                                   FS_ERR_DEV_INVALID_OP       Bad blocks table is full.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkEnsureErased (FS_NAND_DATA     *p_nand_data,
                                               FS_NAND_BLK_QTY   blk_ix_phy,
                                               FS_ERR           *p_err)
{
    CPU_BOOLEAN  is_used;
    FS_SEC_QTY   sec_ix_phy;


    sec_ix_phy = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);

    is_used = FS_NAND_SecIsUsed(p_nand_data,                    /* Chk if used mark is present.                         */
                                sec_ix_phy,
                                p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_LOG(("FS_NAND_BlkEnsureErased(): No need to erase block %u (err).\r\n", blk_ix_phy));
        return;
    }

    if (is_used == DEF_NO) {                                    /* If the sec is unused ...                             */
        FS_NAND_TRACE_LOG(("FS_NAND_BlkEnsureErased(): No need to erase block %u (not used).\r\n", blk_ix_phy));
        return;                                                 /* ... nothing to do ...                                */
    } else {                                                    /* ... else, erase sec.                                 */
        FS_NAND_BlkEraseHandler(p_nand_data,
                                blk_ix_phy,
                                p_err);
        switch (*p_err) {
            case FS_ERR_DEV_IO:
                 FS_NAND_TRACE_DBG(("FS_NAND_BlkEnsureErased(): Error erasing block %u. Marking it bad.\r\n",
                                     blk_ix_phy));

                *p_err = FS_ERR_NONE;
                 FS_NAND_BlkMarkBad(p_nand_data, blk_ix_phy, p_err);
                 if (*p_err != FS_ERR_NONE) {
                     FS_NAND_TRACE_DBG(("FS_NAND_BlkEnsureErased(): Unable to mark block bad.\r\n"));
                 }

                *p_err = FS_ERR_DEV_IO;                         /* Return original err code.                            */
                 return;


            case FS_ERR_NONE:
                 return;


            default:
                 break;
        }

    }
}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_BlkIsBad()
*
* Description : Determine if block is bad according to bad block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Index of block to check.
*
* Return(s)   : DEF_YES, if block is bad,
*               DEF_NO , otherwise.
*
* Note(s)     : (1) Only checks bad block table; factory defect blocks not having been marked as bad will
*                   not cause returned value to be DEF_YES.
*********************************************************************************************************
*/

FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_BlkIsBad (FS_NAND_DATA     *p_nand_data,
                                               FS_NAND_BLK_QTY   blk_ix_phy)
{
    FS_NAND_BLK_QTY   ix;
    FS_NAND_BLK_QTY   blk_ix_phy_entry;
    FS_NAND_BLK_QTY  *p_bad_blk_tbl;


    p_bad_blk_tbl = (FS_NAND_BLK_QTY *)&p_nand_data->MetaCache[p_nand_data->MetaOffsetBadBlkTbl];

    for (ix = 0u; ix < p_nand_data->PartDataPtr->MaxBadBlkCnt; ix++) {
        MEM_VAL_COPY_GET_INTU_LITTLE(&blk_ix_phy_entry,         /* Get entry's blk ix.                                  */
                                     &p_bad_blk_tbl[ix],
                                      sizeof(FS_NAND_BLK_QTY));
        if (blk_ix_phy_entry == blk_ix_phy) {
            return (DEF_YES);
        }
    }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                         FS_NAND_BlkIsDirty()
*
* Description : Determine if block is dirty according to dirty bitmap.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Index of block to check.
*
* Return(s)   : DEF_YES, if block is dirty,
*               DEF_NO , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_BlkIsDirty (FS_NAND_DATA     *p_nand_data,
                                                 FS_NAND_BLK_QTY   blk_ix_phy)
{
    CPU_SIZE_T    offset_octet;
    CPU_INT08U    offset_bit;
    CPU_INT08U   *p_dirty_map;
    CPU_BOOLEAN   bit_set;


    p_dirty_map  = &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap];

    blk_ix_phy  -=  p_nand_data->BlkIxFirst;                     /* Remove offset of first blk.                          */

    offset_octet =  blk_ix_phy / DEF_OCTET_NBR_BITS;
    offset_bit   =  blk_ix_phy % DEF_OCTET_NBR_BITS;

                                                                 /* Get bit's value.                                     */
    bit_set      =  DEF_BIT_IS_SET(p_dirty_map[offset_octet], DEF_BIT(offset_bit));

    return (bit_set);
}


/*
*********************************************************************************************************
*                                        FS_NAND_BlkIsAvail()
*
* Description : Determine if designated block is in available blocks table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Index of block to check.
*
* Return(s)   : DEF_YES, if block is in available blocks table,
*               DEF_NO , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_BlkIsAvail (FS_NAND_DATA     *p_nand_data,
                                                 FS_NAND_BLK_QTY   blk_ix_phy)
{
    FS_NAND_AVAIL_BLK_ENTRY  entry;
    FS_NAND_BLK_QTY          ix;


    for(ix = 0u; ix < p_nand_data->AvailBlkTblEntryCntMax; ix++) {
        entry = FS_NAND_AvailBlkTblEntryRd(p_nand_data, ix);
        if (entry.BlkIxPhy == blk_ix_phy) {
            return (DEF_YES);
        }
     }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                            FS_NAND_BlkIsUB()
*
* Description : Determine if designated block is an update block, according to the update block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Index of block to check.
*
* Return(s)   : DEF_YES, if block is an update block,
*               DEF_NO , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_BlkIsUB (FS_NAND_DATA     *p_nand_data,
                                              FS_NAND_BLK_QTY   blk_ix_phy)
{
    FS_NAND_UB_DATA  entry;
    FS_NAND_UB_QTY   ix;


    for (ix = 0u; ix < p_nand_data->UB_CntMax; ix++) {
        entry = FS_NAND_UB_TblEntryRd(p_nand_data, ix);         /* Rd update blk tbl entry.                             */
        if (entry.BlkIxPhy == blk_ix_phy) {
            return (DEF_YES);
        }

    }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                     FS_NAND_BlkIsFactoryDefect()
*
* Description : Check if block is a factory defect block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Index of physical block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_ECC  Invalid ECC during read.
*                                   FS_ERR_DEV_INVALID_OP   Device invalid operation.
*                                   FS_ERR_DEV_IO           Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT      Device timeout.
*                                   FS_ERR_NONE             Sector read successfully.
*
*                                   ------RETURNED BY p_ctrlr_api->SpareRdRaw()------
*                                   See p_ctrlr_api->SpareRdRaw() for additional return error codes.
*
*                                   --------RETURNED BY p_ctrlr_api->SecRd()---------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
* Return(s)   : DEF_YES, if block is a factory defect block,
*               DEF_NO,  otherwise.
*
* Note(s)     : (1) ONFI spec states that factory defect are marked as 0x00 or 0x0000(16-bit access). Bit
*                   errors in defect mark are not tolerated, even if ECC-correctable.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_BlkIsFactoryDefect (FS_NAND_DATA     *p_nand_data,
                                                         FS_NAND_BLK_QTY   blk_ix_phy,
                                                         FS_ERR           *p_err)
{
    FS_NAND_PART_DATA  *p_part_data;
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    CPU_INT08U         *p_buf;
    FS_SEC_QTY          pg_sec_ix_phy;
    CPU_INT08U          pg_chk_iter;
    CPU_SIZE_T          i;


    p_ctrlr_api = p_nand_data->CtrlrPtr;
    p_part_data = p_nand_data->PartDataPtr;


    p_buf = (CPU_INT08U *)p_nand_data->BufPtr;                  /* Sec buf used since spare size > OOS size.            */

    for (pg_chk_iter = 0u; pg_chk_iter <= 1u; pg_chk_iter++) {
        if (pg_chk_iter == 0u) {
            switch (p_part_data->DefectMarkType) {
                case DEFECT_SPARE_ANY_PG_1_OR_N_ALL_0:
                case DEFECT_SPARE_B_1_6_W_1_IN_PG_1:
                case DEFECT_SPARE_B_6_W_1_PG_1_OR_2:
                case DEFECT_SPARE_L_1_PG_1_OR_2:
                case DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0:
                     pg_sec_ix_phy  = blk_ix_phy * p_part_data->PgPerBlk;
                     break;


                case DEFECT_PG_L_1_OR_N_PG_1_OR_2:
                     pg_sec_ix_phy = blk_ix_phy * p_nand_data->NbrSecPerBlk;
                     break;


                default:
                    *p_err = FS_ERR_DEV_INVALID_CFG;
                     return (DEF_YES);                          /* Prevent 'break NOT reachable' compiler warning.      */

            }
        } else {
            switch (p_part_data->DefectMarkType) {
                case DEFECT_SPARE_ANY_PG_1_OR_N_ALL_0:
                case DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0:
                     pg_sec_ix_phy = (blk_ix_phy + 1) * p_part_data->PgPerBlk - 1u;
                     break;


                case DEFECT_SPARE_B_6_W_1_PG_1_OR_2:
                case DEFECT_SPARE_L_1_PG_1_OR_2:
                     pg_sec_ix_phy = blk_ix_phy * p_part_data->PgPerBlk + 1u;
                     break;


                case DEFECT_PG_L_1_OR_N_PG_1_OR_2:
                                                                /* Only loc 1 checked.                                  */
                     pg_sec_ix_phy = blk_ix_phy * p_nand_data->NbrSecPerBlk + 1u;
                     break;


                case DEFECT_SPARE_B_1_6_W_1_IN_PG_1:
                     return (DEF_NO);                           /* Prevent 'break NOT reachable' compiler warning.      */


                default:
                    *p_err = FS_ERR_DEV_INVALID_CFG;
                     return (DEF_YES);                          /* Prevent 'break NOT reachable' compiler warning.      */
            }
        }


        switch (p_part_data->DefectMarkType) {
            case DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0:
            case DEFECT_SPARE_ANY_PG_1_OR_N_ALL_0:
            case DEFECT_SPARE_B_6_W_1_PG_1_OR_2:
            case DEFECT_SPARE_L_1_PG_1_OR_2:
            case DEFECT_SPARE_B_1_6_W_1_IN_PG_1:
                p_ctrlr_api->SpareRdRaw(p_nand_data->CtrlrDataPtr,
                                        p_buf,
                                        pg_sec_ix_phy,
                                        0u,
                                        p_part_data->SpareSize,
                                        p_err);
                if (*p_err != FS_ERR_NONE) {
                    return (DEF_YES);
                }
                break;


            case DEFECT_PG_L_1_OR_N_PG_1_OR_2:
                p_ctrlr_api->SecRd(p_nand_data->CtrlrDataPtr,
                                   p_buf,
                                   p_nand_data->OOS_BufPtr,
                                   pg_sec_ix_phy,
                                   p_err);
                if ((*p_err != FS_ERR_NONE)              &&
                    (*p_err != FS_ERR_ECC_CORR)          &&
                    (*p_err != FS_ERR_ECC_CRITICAL_CORR) &&
                    (*p_err != FS_ERR_ECC_UNCORR)) {
                    return (DEF_YES);
                }
               *p_err = FS_ERR_NONE;
                break;


            default:
               *p_err = FS_ERR_DEV_INVALID_CFG;
                return (DEF_YES);

        }

        switch (p_part_data->DefectMarkType) {
            case DEFECT_SPARE_L_1_PG_1_OR_2:
            case DEFECT_PG_L_1_OR_N_PG_1_OR_2:
                 if ( (p_buf[0] != 0xFFu) ||
                     ((p_buf[1] != 0xFFu) && (p_part_data->BusWidth == 16u)) ) {
                     return (DEF_YES);
                 }
                 break;


            case DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0:
                 if ( (p_buf[0] == 0x00u) &&
                     ((p_buf[1] == 0x00u) || (p_part_data->BusWidth == 8u)) ) {
                     return (DEF_YES);
                 }
                 break;


            case DEFECT_SPARE_ANY_PG_1_OR_N_ALL_0:
                 for (i=0; i < p_part_data->SpareSize; i++) {
                     if (p_buf[i] == 0x00u) {
                         return (DEF_YES);
                     }
                 }
                 break;


            case DEFECT_SPARE_B_1_6_W_1_IN_PG_1:
                 if (p_part_data->BusWidth == 16u) {
                     if ((p_buf[0] != 0xFFu) || (p_buf[1] != 0xFFu)) {
                         return (DEF_YES);
                     }
                 } else {
                     if ((p_buf[0] != 0xFFu) || (p_buf[5] != 0xFFu)) {
                         return (DEF_YES);
                     }
                 }
                 break;


            case DEFECT_SPARE_B_6_W_1_PG_1_OR_2:
                if (p_part_data->BusWidth == 16u) {
                    if ((p_buf[0] != 0xFFu) || (p_buf[1] != 0xFFu)) {
                        return (DEF_YES);
                    }
                } else {
                    if (p_buf[5] != 0xFFu) {
                        return (DEF_YES);
                    }
                }
                break;


            default:
               *p_err = FS_ERR_DEV_INVALID_CFG;
                return (DEF_YES);                               /* Prevent 'break NOT reachable' compiler warning.      */
        }
    }

    return (DEF_NO);
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_MetaBlkFind()
*
* Description : Find the latest metadata block on NAND device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_hdr      Index of device header block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ----RETURNED BY p_ctrlr_api->SecRd()-----
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_MetaBlkFind (FS_NAND_DATA     *p_nand_data,
                                           FS_NAND_BLK_QTY   blk_ix_hdr,
                                           FS_ERR           *p_err)
{
    CPU_INT08U            *p_oos_buf;
    FS_NAND_BLK_QTY        blk_ix_phy;
    FS_NAND_BLK_QTY        last_blk_ix;
    FS_NAND_SEC_TYPE_STO   blk_type;
    FS_NAND_META_ID        min_meta_blk_id;
    FS_NAND_META_ID        max_meta_blk_id;
    FS_NAND_META_ID        max_meta_blk_id_lwr;
    FS_NAND_META_ID        min_meta_blk_id_higher;
    FS_NAND_META_ID        meta_blk_id_range;
    FS_NAND_META_ID        seq_nbr;
    FS_NAND_BLK_QTY        max_seq_blk_ix;
    FS_NAND_BLK_QTY        min_seq_blk_ix;
    FS_NAND_BLK_QTY        max_seq_blk_ix_lwr;
    FS_NAND_BLK_QTY        min_seq_blk_ix_higher;


    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

                                                                /* ------------------ FIND META BLK ------------------- */
    min_meta_blk_id        = DEF_GET_U_MAX_VAL(min_meta_blk_id);
    max_meta_blk_id        = 0u;
    max_meta_blk_id_lwr    = 0u;
    min_meta_blk_id_higher = DEF_GET_U_MAX_VAL(min_meta_blk_id_higher);
    max_seq_blk_ix         = FS_NAND_BLK_IX_INVALID;
    min_seq_blk_ix         = FS_NAND_BLK_IX_INVALID;
    max_seq_blk_ix_lwr     = FS_NAND_BLK_IX_INVALID;
    min_seq_blk_ix_higher  = FS_NAND_BLK_IX_INVALID;

    last_blk_ix = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
    for (blk_ix_phy = blk_ix_hdr; blk_ix_phy <= last_blk_ix; blk_ix_phy++) {
        blk_type   = FS_NAND_SEC_TYPE_UNUSED;

        FS_NAND_SecRdPhyNoRefresh(p_nand_data,                  /* Rd sec to get blk type.                              */
                                  p_nand_data->BufPtr,
                                  p_nand_data->OOS_BufPtr,
                                  blk_ix_phy,
                                  0u,
                                  p_err);
        switch (*p_err) {
            case FS_ERR_ECC_UNCORR:
                *p_err = FS_ERR_NONE;
                 break;


            case FS_ERR_NONE:
                *p_err    = FS_ERR_NONE;
                 blk_type = FS_NAND_SEC_TYPE_INVALID;           /* Rd blk type.                                         */
                 MEM_VAL_COPY_GET_INTU_LITTLE(&blk_type,
                                              &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                               sizeof(FS_NAND_SEC_TYPE_STO));
                 break;


            default:
                 return;                                        /* Return err.                                          */
        }


        if (blk_type == FS_NAND_SEC_TYPE_METADATA) {
                                                                /* Determine if meta blk is complete.                   */
            MEM_VAL_COPY_GET_INTU_LITTLE(&seq_nbr,              /* Rd blk ID.                                           */
                                         &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                          sizeof(FS_NAND_META_ID));


            FS_NAND_TRACE_LOG(("Found meta block at physical block %u with ID %u.\r\n", blk_ix_phy, seq_nbr));

                                                                /* Update min and max meta blk seq nbr.                 */
            if (seq_nbr <= min_meta_blk_id) {
                min_meta_blk_id = seq_nbr;
                min_seq_blk_ix  = blk_ix_phy;
            }
            if (seq_nbr >= max_meta_blk_id) {
                max_meta_blk_id = seq_nbr;
                max_seq_blk_ix  = blk_ix_phy;
            }
            if ((seq_nbr >= max_meta_blk_id_lwr) &&             /* Lower half seq range.                                */
                (seq_nbr <= FS_NAND_META_SEQ_QTY_HALF_RANGE)) {
                max_meta_blk_id_lwr = seq_nbr;
                max_seq_blk_ix_lwr  = blk_ix_phy;
            }
                                                                /* Higher half seq range.                               */

            if ((seq_nbr > FS_NAND_META_SEQ_QTY_HALF_RANGE) &&
                (seq_nbr <= min_meta_blk_id_higher)) {
                min_meta_blk_id_higher = seq_nbr;
                min_seq_blk_ix_higher  = blk_ix_phy;
            }
        }
    }

                                                                /* ----------------- FIND LATEST SEQ ------------------ */
    meta_blk_id_range = max_meta_blk_id - min_meta_blk_id;
    if (meta_blk_id_range > FS_NAND_META_SEQ_QTY_HALF_RANGE) {  /* If wrap around, discard seq nbr > half range ...     */
        p_nand_data->MetaBlkIxPhy       = max_seq_blk_ix_lwr;
        p_nand_data->DirtyBitmapSrchPos = min_seq_blk_ix_higher - p_nand_data->BlkIxFirst;
    } else {                                                    /* ... otherwise, take absolute max.                    */
        p_nand_data->MetaBlkIxPhy       = max_seq_blk_ix;
        p_nand_data->DirtyBitmapSrchPos = min_seq_blk_ix - p_nand_data->BlkIxFirst;
    }

    if (p_nand_data->MetaBlkIxPhy == FS_NAND_BLK_IX_INVALID) {
       *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
        return;
    }

    FS_NAND_TRACE_LOG(("FS_NAND_MetaBlkFind(): Found metadata block at block index %u.\r\n",
                       p_nand_data->MetaBlkIxPhy));

}


/*
*********************************************************************************************************
*                                        FS_NAND_MetaBlkFindID()
*
* Description : Find a sector of the specific metadata sequence on NAND device, and return the index of
*               the physical block on which it is written.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               meta_blk_id     ID of metadata block to find.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ----RETURNED BY p_ctrlr_api->SecRd()-----
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*
* Return(s)   : Physical index of block containing sectors with specified sequence number, if successful;
*               FS_NAND_BLK_IX_INVALID,                                                    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_MetaBlkFindID (FS_NAND_DATA     *p_nand_data,
                                                        FS_NAND_META_ID   meta_blk_id,
                                                        FS_ERR           *p_err)
{
    CPU_INT08U             *p_oos_buf;
    FS_NAND_BLK_QTY         blk_ix_phy;
    FS_NAND_BLK_QTY         last_blk_ix;
    CPU_INT08U              rd_retries;
    FS_NAND_SEC_TYPE_STO    sec_type;
    FS_NAND_META_ID         meta_blk_id_rd;


    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;


    blk_ix_phy  = p_nand_data->BlkIxFirst;
    last_blk_ix = blk_ix_phy + p_nand_data->BlkCnt - 1u;

    while (blk_ix_phy <= last_blk_ix) {
                                                                /* ---------------------- RD SEC ---------------------- */
        rd_retries = 0u;
        do {
           *p_err = FS_ERR_NONE;

            FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                      p_nand_data->BufPtr,
                                      p_oos_buf,
                                      blk_ix_phy,
                                      0u,
                                      p_err);
            rd_retries++;
        } while ((rd_retries <= FS_NAND_CFG_MAX_RD_RETRIES) &&
                 (*p_err     == FS_ERR_ECC_UNCORR));

        if (*p_err != FS_ERR_NONE) {
            return (FS_NAND_BLK_IX_INVALID);
        }

        MEM_VAL_COPY_GET_INTU_LITTLE(&sec_type,                 /* Get sec type.                                        */
                                     &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                      sizeof(FS_NAND_SEC_TYPE_STO));

        if (sec_type == FS_NAND_SEC_TYPE_METADATA) {            /* If it's a meta blk ...                               */
            MEM_VAL_COPY_GET_INTU_LITTLE(&meta_blk_id_rd,       /* Get blk ID.                                          */
                                         &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                          sizeof(FS_NAND_META_ID));

            if (meta_blk_id_rd == meta_blk_id) {
                return (blk_ix_phy);                            /* Meta blk     found.                                  */
            }
        }

        blk_ix_phy++;
    }

                                                                /* Meta blk not found.                                  */
    FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkFindSeq(): Unable to find metadata block for sequence %u.\r\n", meta_blk_id));
   *p_err = FS_ERR_DEV_INVALID_METADATA;

    return (FS_NAND_BLK_IX_INVALID);
}


/*
*********************************************************************************************************
*                                        FS_NAND_MetaBlkParse()
*
* Description : Load metadata block contents into RAM.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Successfully loaded metadata block contents into RAM.
*
*                                   ---------------RETURNED BY FS_NAND_SecRdPhyNoRefresh()---------------
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
*                                   -------------------RETURNED BY FS_NAND_SecIsUsed()-------------------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   ------------------RETURNED BY p_ctrlr_api->SecRd()-------------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*                                   -----------------RETURNED BY FS_NAND_MetaBlkFindID()-----------------
*                                   See FS_NAND_MetaBlkFindID() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Combined to the fact that all blocks located in the available block table at mount
*                   time are considered as potential metadata blocks (see FS_NAND_InitDevData()), this
*                   guarantees that all blocks in the available block table will be used after remounting,
*                   thus preventing stale metadata blocks.
*
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_MetaBlkParse (FS_NAND_DATA  *p_nand_data,
                                            FS_ERR        *p_err)
{
    CPU_INT08U                   *p_sec_buf;
    CPU_INT08U                   *p_oos_buf;
    CPU_SIZE_T                    map_size;
    CPU_SIZE_T                    offset_octet;
    CPU_INT08U                    offset_bit;
    FS_NAND_SEC_PER_BLK_QTY       sec_ix;
    FS_NAND_BLK_QTY               meta_blk_ix;
    FS_NAND_META_ID               meta_blk_id;
    FS_NAND_META_SEC_QTY          meta_sec_ix;
    FS_NAND_META_SEQ_STATUS_STO   seq_status;
    FS_SEC_QTY                    sec_base;
    CPU_SIZE_T                    copy_size;
    CPU_INT08U                    rd_retries;
    CPU_INT08U                    meta_map[32];
    CPU_BOOLEAN                   seq_last_sec;
    CPU_BOOLEAN                   last_seq_found;
    CPU_BOOLEAN                   is_sec_used;
    CPU_BOOLEAN                   status_finished;
    CPU_BOOLEAN                   done;
    CPU_DATA                      ix;


    p_sec_buf = (CPU_INT08U *)p_nand_data->BufPtr;
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

                                                                /* ------------- INIT META SEC VALID MAP -------------- */
    map_size  = p_nand_data->MetaSecCnt / DEF_OCTET_NBR_BITS;
    map_size += p_nand_data->MetaSecCnt % DEF_OCTET_NBR_BITS == 0u ? 0u : 1u;

                                                                /* Init map to all zeroes.                              */
    for (ix = 0; ix < map_size; ix++) {
        p_nand_data->MetaBlkInvalidSecMap[ix] = 0x00u;
    }

                                                                /* Mark each sector invalid.                            */
    for (sec_ix = 0u; sec_ix < p_nand_data->MetaSecCnt; sec_ix++) {
        offset_octet = sec_ix / DEF_OCTET_NBR_BITS;
        offset_bit   = sec_ix % DEF_OCTET_NBR_BITS;

        p_nand_data->MetaBlkInvalidSecMap[offset_octet] |= (1u << offset_bit);
    }

                                                                /* --------------------- RD META ---------------------- */
    done                           = DEF_NO;
    meta_blk_ix                    = p_nand_data->MetaBlkIxPhy;
    p_nand_data->MetaBlkNextSecIx  = 0u;
    p_nand_data->MetaBlkFoldNeeded = DEF_NO;
    sec_ix                         = p_nand_data->NbrSecPerBlk - 1u;
    status_finished                = DEF_NO;
    seq_last_sec                   = DEF_NO;
    last_seq_found                 = DEF_NO;

    while (done != DEF_YES) {                                   /* Until all sec have been found.                       */

        sec_base = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, meta_blk_ix);

                                                                /* Find first written sec.                              */
        is_sec_used = DEF_NO;
        while (is_sec_used != DEF_YES) {
            is_sec_used = FS_NAND_SecIsUsed(p_nand_data,
                                            sec_base + sec_ix,
                                            p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
                                                                /* Dec sec ix if not used.                              */
            if (is_sec_used != DEF_YES) {
                if (sec_ix == 0u) {
                    FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkParse(): Fatal error; no metadata sectors found in block.\r\n"));
                   *p_err = FS_ERR_DEV_INVALID_METADATA;
                    return;
                }
                sec_ix--;
            }
        }

                                                                /* Update next meta sec ix if latest meta blk.          */
        if ((p_nand_data->MetaBlkNextSecIx == 0u)      &&
            (meta_blk_ix == p_nand_data->MetaBlkIxPhy) &&
            (p_nand_data->MetaBlkFoldNeeded == DEF_NO)   ) {
            p_nand_data->MetaBlkNextSecIx = sec_ix + 1u;

                                                                /* If ix wraps, blk is full and a fold is needed.       */
            if (p_nand_data->MetaBlkNextSecIx == 0u) {
                p_nand_data->MetaBlkFoldNeeded = DEF_YES;
            } else {
                p_nand_data->MetaBlkFoldNeeded = DEF_NO;
            }
        }

        sec_ix++;
        while ((done != DEF_YES) && (sec_ix > 0)) {             /* For each sec in blk, or until we're done ...         */
            sec_ix--;

            rd_retries = 0u;
            do {                                                /* Rd sec, retrying if ECC err.                         */
                FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                          p_sec_buf,
                                          p_oos_buf,
                                          meta_blk_ix,
                                          sec_ix,
                                          p_err);
                rd_retries++;
            } while ((rd_retries <= FS_NAND_CFG_MAX_RD_RETRIES) &&
                     (*p_err     == FS_ERR_ECC_UNCORR));

            if (*p_err == FS_ERR_ECC_UNCORR) {
                FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkParse(): ECC error reading metadata sector %u. Ignoring sector.\r\n",
                                   sec_base + sec_ix));
               *p_err = FS_ERR_NONE;
            } else if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkParse(): Fatal error reading metadata sector %u.\r\n",
                                   sec_base + sec_ix));
                return;
            } else {
                MEM_VAL_COPY_GET_INTU_LITTLE(&seq_status,       /* Rd seq status.                                       */
                                             &p_oos_buf[FS_NAND_OOS_META_SEQ_STATUS_OFFSET],
                                             sizeof(FS_NAND_META_SEQ_STATUS_STO));

                MEM_VAL_COPY_GET_INTU_LITTLE(&meta_blk_id,
                                             &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                             sizeof(FS_NAND_META_ID));

                if (last_seq_found == DEF_NO) {
                    p_nand_data->MetaBlkID = meta_blk_id;

                    last_seq_found         = DEF_YES;
                }

                switch (seq_status) {                           /* Chk for seq finished mark.                           */
                    case FS_NAND_META_SEQ_FINISHED:
                         status_finished = DEF_YES;
                         break;


                    case FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY:
                         status_finished = DEF_NO;
                         break;


                    default:
                         if (seq_last_sec == DEF_YES) {
                             status_finished = DEF_NO;
                         }
                         break;

                }

                if (seq_status == FS_NAND_META_SEQ_NEW) {       /* Determine if this is the last sec of the seq.        */
                    seq_last_sec = DEF_YES;
                } else {
                    seq_last_sec = DEF_NO;
                }

                if (status_finished == DEF_YES) {               /* Seq finished; we can consider sec.                   */
                    MEM_VAL_COPY_GET_INTU_LITTLE(&meta_sec_ix,  /* Rd meta sec ix.                                      */
                                                 &p_oos_buf[FS_NAND_OOS_META_SEC_IX_OFFSET],
                                                 sizeof(FS_NAND_META_SEC_QTY));

                    offset_octet = meta_sec_ix / DEF_OCTET_NBR_BITS;
                    offset_bit   = meta_sec_ix % DEF_OCTET_NBR_BITS;

                                                                /* Check if meta sec has already been cached.           */
                    if ((p_nand_data->MetaBlkInvalidSecMap[offset_octet] & DEF_BIT(offset_bit)) != 0u) {

                                                                /* Calc size to copy.                                   */
                        if (meta_sec_ix == p_nand_data->MetaSecCnt - 1) {
                                                                /* Last meta sec, copy remaining size.                  */
                            copy_size  =  p_nand_data->MetaSize;
                            copy_size -= (p_nand_data->MetaSecCnt - 1) * p_nand_data->SecSize;
                        } else {
                            copy_size  =  p_nand_data->SecSize;
                        }

                                                                /* Copy metadata.                                       */
                        Mem_Copy(&p_nand_data->MetaCache[meta_sec_ix * p_nand_data->SecSize],
                                 &p_sec_buf[0],
                                 copy_size);


                                                                /* Mark meta sec as up-to-date.                         */
                        p_nand_data->MetaBlkInvalidSecMap[offset_octet] ^= DEF_BIT(offset_bit);

                                                                /* Chk if all sec have been rd.                         */
                        done = DEF_YES;
                        ix   = 0u;
                        while ((done == DEF_YES) &&
                               (ix    < map_size)   ) {
                            if (p_nand_data->MetaBlkInvalidSecMap[ix] != 0x00u) {
                                done = DEF_NO;
                            }
                            ix++;
                        }

                    }
                }
            }
        }

        if (meta_blk_ix == p_nand_data->MetaBlkIxPhy) {         /* If this is the latest meta blk, preserve meta map.   */
            Mem_Copy(&meta_map[0],
                     &p_nand_data->MetaBlkInvalidSecMap[0],
                      map_size);
        }

        if (done != DEF_YES) {                                  /* Blk finished, but not all needed data was found.     */
            meta_blk_id--;                                      /* Wrap if 0.                                           */
            meta_blk_ix = FS_NAND_MetaBlkFindID(p_nand_data,    /* Find previous meta blk.                              */
                                                meta_blk_id,
                                                p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkParse(): Fatal error; could not find metadata block for sequence %u.\r\n",
                                   meta_blk_id));
                return;
            }
        }
    }

                                                                /* See Note # 1.                                        */
    for (ix = 0u; ix < p_nand_data->AvailBlkTblEntryCntMax; ix++) {
        p_nand_data->AvailBlkMetaID_Tbl[ix] = p_nand_data->MetaBlkID - FS_NAND_META_ID_STALE_THRESH - 1u;
    }

                                                                /* ----------- RESTORE META INVALID SEC MAP ----------- */
    Mem_Copy(&p_nand_data->MetaBlkInvalidSecMap[0],
             &meta_map[0],
              map_size);

}

/*
*********************************************************************************************************
*                                      FS_NAND_MetaBlkAvailParse()
*
* Description : Parse available block table commits following the last complete metadata sequence committed.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Successfully loaded metadata block contents into RAM.
*
*                                   ---------------RETURNED BY FS_NAND_SecRdPhyNoRefresh()---------------
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
*                                   ----------------RETURNED BY FS_NAND_BlkEraseHandler()----------------
*                                   See FS_NAND_BlkEraseHandler() for additional return error codes.
*
*                                   ------------------RETURNED BY FS_NAND_BlkMarkBad()-------------------
*                                   See FS_NAND_BlkMarkBad() for additional return error codes.
*
*                                   -------------RETURNED BY p_nand_data->CtrlrPtr->SecWr()--------------
*                                   See p_nand_data->CtrlrPtr->SecWr() for additional return error codes.
*
*                                   -------------RETURNED BY p_nand_data->CtrlrPtr->SecRd()--------------
*                                   See p_nand_data->CtrlrPtr->SecRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaBlkAvailParse(FS_NAND_DATA  *p_nand_data,
                                                FS_ERR        *p_err)
{
    CPU_INT08U                  *p_buf;
    CPU_INT08U                  *p_oos_buf;
    FS_NAND_SEC_PER_BLK_QTY      meta_sec_offset;
    FS_NAND_META_SEQ_STATUS_STO  meta_seq_status_sto;
    FS_NAND_BLK_QTY              avail_tbl_ix;
    FS_NAND_BLK_QTY              tmp_avail_tbl_ix;
    FS_NAND_AVAIL_BLK_ENTRY      avail_blk;
    FS_NAND_AVAIL_BLK_ENTRY      tmp_avail_blk;
    FS_NAND_SEC_TYPE_STO         sec_type_sto;
    FS_NAND_SEC_TYPE             sec_type;
    FS_SEC_QTY                   sec_ix_phy;
    CPU_SIZE_T                   meta_offset;
    CPU_SIZE_T                   avail_blk_map_size;
    CPU_SIZE_T                   ix;
    CPU_BOOLEAN                  in_avail_tbl;
    CPU_BOOLEAN                  blk_is_dirty;
    CPU_BOOLEAN                  entry_added;
    CPU_BOOLEAN                  sec_scan_complete;
    CPU_BOOLEAN                  create_dummy_blk;


    p_buf     = (CPU_INT08U *)p_nand_data->BufPtr;
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    meta_sec_offset = p_nand_data->MetaBlkNextSecIx - 1u;

                                                                /* ------ SCAN META SECS AFTER LAST FULL COMMIT ------- */
    sec_scan_complete = DEF_NO;
    while ((sec_scan_complete == DEF_NO) && (meta_sec_offset > 0)) {
        FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                  p_buf,
                                  p_oos_buf,
                                  p_nand_data->MetaBlkIxPhy,
                                  meta_sec_offset,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Unable to read sector %u of metadata block. "
                               "Some erase counts might be lost.\r\n",
                                meta_sec_offset));
        } else {
            MEM_VAL_COPY_GET_INTU_LITTLE(&meta_seq_status_sto,
                                         &p_oos_buf[FS_NAND_OOS_META_SEQ_STATUS_OFFSET],
                                          sizeof(FS_NAND_META_SEQ_STATUS_STO));

            switch (meta_seq_status_sto) {
                case FS_NAND_META_SEQ_FINISHED:
                     sec_scan_complete = DEF_YES;               /* Seq finished: done processing avail blk tbl commits. */
                     break;


                case FS_NAND_META_SEQ_UNFINISHED:
                     break;


                case FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY:
                                                                /* ---------- SCAN AVAIL TBL FOR NEW ENTRIES ---------- */
                     for (tmp_avail_tbl_ix = 0u; tmp_avail_tbl_ix < p_nand_data->AvailBlkTblEntryCntMax; tmp_avail_tbl_ix++) {

                                                                /* Rd entry.                                            */
                         meta_offset = tmp_avail_tbl_ix * (sizeof(FS_NAND_BLK_QTY) + sizeof(FS_NAND_ERASE_QTY));
                         MEM_VAL_COPY_GET_INTU_LITTLE(&tmp_avail_blk.BlkIxPhy,
                                                      &p_buf[meta_offset],
                                                       sizeof(FS_NAND_BLK_QTY));

                         if (tmp_avail_blk.BlkIxPhy != FS_NAND_BLK_IX_INVALID) {
                                                                /* Chk if already in tbl.                               */
                             in_avail_tbl = FS_NAND_BlkIsAvail(p_nand_data, tmp_avail_blk.BlkIxPhy);
                             if (in_avail_tbl == DEF_NO) {
                                 FS_NAND_TRACE_LOG(("Found available block %u from a temporary available block table commit.\r\n",
                                                    tmp_avail_blk.BlkIxPhy));

                                 meta_offset += sizeof(FS_NAND_BLK_QTY);
                                 MEM_VAL_COPY_GET_INTU_LITTLE(&tmp_avail_blk.EraseCnt,
                                                              &p_buf[meta_offset],
                                                               sizeof(FS_NAND_ERASE_QTY));

                                                                /* Make sure blk is dirty.                              */
                                 blk_is_dirty = FS_NAND_BlkIsDirty(p_nand_data, tmp_avail_blk.BlkIxPhy);
                                 if (blk_is_dirty != DEF_YES) {
                                     FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): New available block %u wasn't dirty in last "
                                                        "full commit. Volume might be corrupted.\r\n",
                                                         tmp_avail_blk.BlkIxPhy));
                                    *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                                     return;
                                 }

                                                                /* ----------- PROCESS NEW AVAIL TBL ENTRY ------------ */
                                 avail_tbl_ix = 0u;
                                 entry_added = DEF_NO;
                                 while ((entry_added == DEF_NO) &&
                                        (avail_tbl_ix < p_nand_data->AvailBlkTblEntryCntMax)) {
                                     avail_blk = FS_NAND_AvailBlkTblEntryRd(p_nand_data, avail_tbl_ix);
                                     if (avail_blk.BlkIxPhy == FS_NAND_BLK_IX_INVALID) {
                                         FS_NAND_TRACE_LOG(("Adding block %u to available block table.\r\n",
                                                            tmp_avail_blk.BlkIxPhy));

                                                                /* Unmap blk.                                           */
                                         FS_NAND_BlkUnmap(p_nand_data, tmp_avail_blk.BlkIxPhy);

                                                                /* Insert avail entry in empty slot.                    */
                                         avail_blk.BlkIxPhy = tmp_avail_blk.BlkIxPhy;
                                         avail_blk.EraseCnt = tmp_avail_blk.EraseCnt;

                                         FS_NAND_AvailBlkTblEntryWr(p_nand_data, avail_tbl_ix, avail_blk);
                                         FS_NAND_AvailBlkTblInvalidate(p_nand_data);

                                         entry_added = DEF_YES;
                                     }

                                     avail_tbl_ix++;
                                 }

                                                                /* No space left: chk if a dummy blk is needed.         */
                                 if (entry_added == DEF_NO) {
                                     FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                                               p_buf,
                                                               p_oos_buf,
                                                               tmp_avail_blk.BlkIxPhy,
                                                               0u,
                                                               p_err);
                                     if (*p_err == FS_ERR_NONE) {
                                         sec_type = FS_NAND_SecTypeParse(p_oos_buf);

                                                                 /* If sec type isn't unused, no need for dummy blk.    */
                                                                 /* (erase cnt will be in OOS in those cases.)          */
                                         if (sec_type == FS_NAND_SEC_TYPE_UNUSED) {
                                             create_dummy_blk = DEF_YES;
                                         } else {
                                             create_dummy_blk = DEF_NO;
                                         }
                                     } else {
                                         create_dummy_blk = DEF_YES;
                                     }

                                    *p_err = FS_ERR_NONE;
                                 } else {
                                     create_dummy_blk = DEF_NO;
                                 }

                                                                /* Create dummy blk to hold erase cnt.                  */
                                 if (create_dummy_blk == DEF_YES) {
                                     FS_NAND_BlkEraseHandler(p_nand_data, tmp_avail_blk.BlkIxPhy, p_err);
                                     if (*p_err != FS_ERR_NONE) {
                                         FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Unable to erase block %u. Marking it bad.\r\n",
                                                             tmp_avail_blk.BlkIxPhy));

                                         FS_NAND_BlkMarkBad(p_nand_data, tmp_avail_blk.BlkIxPhy, p_err);
                                         if (*p_err != FS_ERR_NONE) {
                                             FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Unable to mark block %u bad.\r\n",
                                                                 tmp_avail_blk.BlkIxPhy));
                                             return;
                                         }
                                     }

                                                                 /* Set sec type in OOS.                                */
                                     sec_type     = FS_NAND_SEC_TYPE_DUMMY;
                                     sec_type_sto = sec_type;

                                     MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                                                  &sec_type_sto,
                                                                   sizeof(FS_NAND_SEC_TYPE_STO));

                                                                 /* Set erase cnt in OOS.                               */
                                     MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                                                  &tmp_avail_blk.EraseCnt,
                                                                   sizeof(FS_NAND_ERASE_QTY));

                                                                 /* Wr first sec with erase cnt. Don't care about data. */
                                     sec_ix_phy = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, tmp_avail_blk.BlkIxPhy);
                                     p_nand_data->CtrlrPtr->SecWr(p_nand_data->CtrlrDataPtr,
                                                                  p_buf,
                                                                  p_oos_buf,
                                                                  sec_ix_phy,
                                                                  p_err);
                                     if (*p_err != FS_ERR_NONE) {
                                         FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Unable to write first sector of block %u."
                                                            "Marking block bad.\r\n",
                                                             tmp_avail_blk.BlkIxPhy));

                                         FS_NAND_BlkMarkBad(p_nand_data, tmp_avail_blk.BlkIxPhy, p_err);
                                         if (*p_err != FS_ERR_NONE) {
                                             FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Unable to mark block %u bad.\r\n",
                                                                 tmp_avail_blk.BlkIxPhy));
                                             return;
                                         }
                                     } else {
                                                                /* Mark erased blk dirty.                               */
                                         FS_NAND_BlkMarkDirty(p_nand_data, tmp_avail_blk.BlkIxPhy);
                                     }
                                 }

                                                                /* Re-rd avail blk tbl since p_buf has been used.       */
                                 if (entry_added == DEF_NO) {
                                     FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                                               p_buf,
                                                               p_oos_buf,
                                                               p_nand_data->MetaBlkIxPhy,
                                                               meta_sec_offset,
                                                               p_err);
                                     if (*p_err != FS_ERR_NONE) {
                                         FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkAvailParse(): Fatal error -- unable to read sector %u of metadata block. "
                                                            "Sector was previously readable.\r\n",
                                                             meta_sec_offset));
                                         return;
                                     }
                                 }
                             }

                         }
                     }

                     break;


                case FS_NAND_META_SEQ_NEW:
                default:
                     break;
            }
        }

        meta_sec_offset--;
    }

                                                                /* Set avail blk tbl commit map bits.                   */
    avail_blk_map_size = FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->AvailBlkTblEntryCntMax);
    for (ix = 0; ix < avail_blk_map_size; ix++) {
       *p_nand_data->AvailBlkTblCommitMap = 0xFF;
    }
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_MetaBlkFold()
*
* Description : (1) Creates a new metadata block.
*               (2) Copies valid metadata sectors from the old to the new metadata block.
*               (3) Mark the old metadata block dirty.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_IO                   Could not reach device.
*                                   FS_ERR_DEV_NAND_NO_AVAIL_BLK    No blocks in available blocks table.
*                                   FS_ERR_NONE                     Metadata block folded successfully.
*
*                                   --------------RETURNED BY FS_NAND_BlkGetAvailFromTbl()--------------
*                                   See FS_NAND_BlkGetAvailFromTbl() for additional return error codes.
*
*                                   ---------------RETURNED BY FS_NAND_AvailBlkTblFill()----------------
*                                   See FS_NAND_AvailBlkTblFill() for additional return error codes.
*
*                                   ---------------RETURNED BY FS_NAND_BlkEnsureErased()----------------
*                                   See FS_NAND_BlkEnsureErased() for additional return error codes.
*
*
* Return(s)   : none.
*
* Note(s)     : (1) When folding a metadata block it is impossible to add a new block to the available
*                   block table (since there is no space left in the metadata block being folded). To make
*                   sure blocks are available, a portion of the available blocks table is reserved for
*                   FS_NAND_MetaBlkFold. This reserved space must always be filled, and not used by other
*                   functions than this one.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaBlkFold (FS_NAND_DATA  *p_nand_data,
                                           FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY          blk_ix_phy_new;
    FS_NAND_BLK_QTY          avail_blk_entry_cnt;
    CPU_SIZE_T               inv_sec_map_size;
    CPU_SIZE_T               ix;


    FS_NAND_TRACE_LOG(("Fold metadata block: old=%u.\r\n",
                       p_nand_data->MetaBlkIxPhy));


                                                                /* ----------- CNT ENTRIES IN AVAIL BLK TBL ----------- */
    avail_blk_entry_cnt = FS_NAND_AvailBlkTblEntryCnt(p_nand_data);

    p_nand_data->MetaBlkID++;

                                                                /* ------------------- GET NEW BLK -------------------- */
    do {
       *p_err = FS_ERR_NONE;
                                                                /* Find avail blk.                                      */
        blk_ix_phy_new = FS_NAND_BlkGetAvailFromTbl(p_nand_data,
                                                    DEF_YES,
                                                    p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkFold(): Fatal exception. Unable to get a new metadata block.\r\n"));
           *p_err = FS_ERR_DEV_NAND_NO_AVAIL_BLK;
            return;
        }

        FS_NAND_BlkEnsureErased(p_nand_data,                    /* Make sure the blk is erased.                         */
                                blk_ix_phy_new,
                                p_err);

        avail_blk_entry_cnt++;
    } while (*p_err != FS_ERR_NONE);


                                                                /* --------------- FILL AVAIL BLK TBL ----------------- */
                                                                /* Limit refill to max cnt.                             */
    if (avail_blk_entry_cnt > p_nand_data->AvailBlkTblEntryCntMax) {
        avail_blk_entry_cnt = p_nand_data->AvailBlkTblEntryCntMax;
    }

    FS_NAND_AvailBlkTblFill(p_nand_data,
                            avail_blk_entry_cnt,               /* Refill to same level to make sure callers still have */
                            DEF_ENABLED,                       /* enough avail blks committed.                         */
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* ------------------- UPDATE META -------------------- */
    FS_NAND_BlkMarkDirty(p_nand_data,                           /* Mark old meta blk dirty.                             */
                         p_nand_data->MetaBlkIxPhy);

    p_nand_data->MetaBlkIxPhy     = blk_ix_phy_new;
    p_nand_data->MetaBlkNextSecIx = 0u;

    p_nand_data->MetaBlkFoldNeeded = DEF_NO;


                                                                /* ----------------- INVALIDATE META ------------------ */
    inv_sec_map_size  = (p_nand_data->MetaSecCnt + 1u) / DEF_OCTET_NBR_BITS;
    inv_sec_map_size += (p_nand_data->MetaSecCnt + 1u) % DEF_OCTET_NBR_BITS == 0u ? 0u : 1u;

    for (ix = 0; ix < inv_sec_map_size; ix++) {
        p_nand_data->MetaBlkInvalidSecMap[ix] = 0xFFu;
    }

    FS_NAND_TRACE_LOG(("Fold metadata block: new=%u.\r\n",
                        p_nand_data->MetaBlkIxPhy));
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_MetaSecFind()
*
* Description : Find the physical sector index corresponding to specified metadata sector index.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               meta_sec_ix     Metadata sector index.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   -RETURNED BY FS_NAND_SecRdPhyNoRefresh()-
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
* Return(s)   : Physical sector offset of metadata sector in metadata block, if found,
*               FS_NAND_SEC_OFFSET_IX_INVALID                              , otherwise.
*
* Note(s)     : (1) Each time metadata sectors are committed, the last one will be written with a status
*                   equal to FS_NAND_META_SEQ_FINISHED. This method prevents considering metadata sectors
*                   from an uncompleted sequence. Doing this, integrity is assured, even after an
*                   unexpected power loss that would otherwise cause corruption.
*********************************************************************************************************
*/

#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN != DEF_ENABLED)
FS_NAND_INTERN  FS_NAND_SEC_PER_BLK_QTY  FS_NAND_MetaSecFind (FS_NAND_DATA          *p_nand_data,
                                                              FS_NAND_META_SEC_QTY   meta_sec_ix,
                                                              FS_ERR                *p_err)
{
    CPU_INT08U                   *p_oos_buf;
    FS_NAND_SEC_PER_BLK_QTY       sec_offset_phy;
    FS_SEC_QTY                    sec_rem;
    FS_NAND_META_SEC_QTY          meta_sec_ix_rd;
    FS_NAND_META_SEQ_STATUS_STO   meta_seq_status;
    CPU_BOOLEAN                   meta_seq_valid;
    CPU_BOOLEAN                   meta_seq_last_sec;


    p_oos_buf              = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

                                                                /* Calc range to srch.                                  */
    if (p_nand_data->MetaBlkNextSecIx != 0u) {
        sec_offset_phy = p_nand_data->MetaBlkNextSecIx - 1u;
        sec_rem        = p_nand_data->MetaBlkNextSecIx;
    } else {
        sec_offset_phy = ((FS_NAND_META_SEC_QTY)-1);            /* MetaBlkNextSecIx == 0 implies that meta blk is full. */
        sec_rem        = ((FS_NAND_META_SEC_QTY)-1) + 1u;
    }

    meta_seq_last_sec = DEF_NO;
    meta_seq_valid    = DEF_NO;
    while (sec_rem != 0u) {
        FS_NAND_SecRdPhyNoRefresh(p_nand_data,                   /* Rd sec in meta blk.                                  */
                                  p_nand_data->BufPtr,
                                  p_nand_data->OOS_BufPtr,
                                  p_nand_data->MetaBlkIxPhy,
                                  sec_offset_phy,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return (FS_NAND_SEC_OFFSET_IX_INVALID);
        } else {                                                /* Sec successfully rd.                                 */
            MEM_VAL_COPY_GET_INTU_LITTLE(&meta_sec_ix_rd,       /* Get sec ix.                                          */
                                         &p_oos_buf[FS_NAND_OOS_META_SEC_IX_OFFSET],
                                          sizeof(FS_NAND_META_SEC_QTY));

            MEM_VAL_COPY_GET_INTU_LITTLE(&meta_seq_status,      /* Get seq status.                                      */
                                         &p_oos_buf[FS_NAND_OOS_META_SEQ_STATUS_OFFSET],
                                          sizeof(FS_NAND_META_SEQ_STATUS_STO));

            if (meta_seq_status == FS_NAND_META_SEQ_FINISHED) {
                meta_seq_valid = DEF_YES;
            } else if (meta_seq_status == FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY) {
                meta_seq_valid = DEF_NO;
            } else if (meta_seq_last_sec == DEF_YES) {
                meta_seq_valid = DEF_NO;
            }

            if (meta_seq_status == FS_NAND_META_SEQ_NEW) {
                meta_seq_last_sec = DEF_YES;
            } else {
                meta_seq_last_sec = DEF_NO;
            }

                                                                /* Verify seq is complete (see Note #1).                */
            if ((meta_seq_valid == DEF_YES) &&
                (meta_sec_ix_rd == meta_sec_ix)) {
                return (sec_offset_phy);                        /* Found latest meta sec.                               */
            }

        }
        sec_rem--;                                              /* Prepare for next iteration.                          */
        sec_offset_phy--;
    }


    return (FS_NAND_SEC_OFFSET_IX_INVALID);
}
#endif


/*
*********************************************************************************************************
*                                   FS_NAND_MetaSecRangeInvalidate()
*
* Description : Invalidate a range of metadata sectors. They will be updated on next metadata commit.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               sec_ix_first    First sector of the range to invalidate.
*
*               sec_ix_last     Last sector of the range to invalidate.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaSecRangeInvalidate (FS_NAND_DATA             *p_nand_data,
                                                      FS_NAND_SEC_PER_BLK_QTY   sec_ix_first,
                                                      FS_NAND_SEC_PER_BLK_QTY   sec_ix_last)
{
    FS_NAND_SEC_PER_BLK_QTY  sec_ix;
    FS_NAND_SEC_PER_BLK_QTY  offset_octet;
    CPU_INT08U               offset_bit;


    for (sec_ix =  sec_ix_first; sec_ix <= sec_ix_last; sec_ix++) {
        offset_octet = sec_ix / DEF_OCTET_NBR_BITS;
        offset_bit   = sec_ix % DEF_OCTET_NBR_BITS;

        p_nand_data->MetaBlkInvalidSecMap[offset_octet] |= DEF_BIT(offset_bit);
    }
}
#endif

/*
*********************************************************************************************************
*                                         FS_NAND_MetaCommit()
*
* Description : Commit metadata to device. After this operation, device's state will be consistent with
*               ram-stored structures. If 'avail_tbl_only' is set to DEF_YES, only the available block
*               table will be commited to the device and consistent with the cached table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               avail_tbl_only  Indicates if only the available blocks table must be commited.
*               --------------  Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ---RETURNED BY FS_NAND_MetaSecCommit()---
*                                   See FS_NAND_MetaSecCommit() for additional return error codes.
*
*                                   ----RETURNED BY FS_NAND_MetaBlkFold()----
*                                   See FS_NAND_MetaBlkFold() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Not finalizing a metadata commit operation may be useful when erasing a block since
*                   the erase count of an erased block must be committed before actually erasing a block.
*
*               (2) This function should be retried if an error code is returned, unless the error code
*                   FS_ERR_DEV_NAND_NO_AVAIL_BLK is returned, which means the error is fatal.
*
*               (3) If only the available blocks table is commited, and that the current metadata block
*                   is full, the last valid metadata sequence must be commited on the new metadata
*                   block before commiting the available block table.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaCommit (FS_NAND_DATA  *p_nand_data,
                                          CPU_BOOLEAN    avail_tbl_only,
                                          FS_ERR        *p_err)
{
    FS_NAND_META_SEC_QTY     meta_sec_ix;
    FS_NAND_META_SEC_QTY     meta_sec_ix_first;
    FS_NAND_META_SEC_QTY     meta_sec_ix_last;
    CPU_SIZE_T               offset_octet;
    CPU_INT08U               offset_bit;
    CPU_INT08U               sec_needs_update;
    CPU_INT08U               sec_update_cnt;
    FS_NAND_SEC_PER_BLK_QTY  sec_rem;
    FS_NAND_META_SEQ_STATUS  seq_status;
    CPU_DATA                 avail_blk_tbl_size;
    CPU_DATA                 ix;
    CPU_BOOLEAN              done;


    done           = DEF_NO;
    while (done != DEF_YES) {
                                                                /* -------------- FIND ALL SEC TO COMMIT -------------- */
        meta_sec_ix_first = (FS_NAND_META_SEC_QTY) -1;
        meta_sec_ix_last  = (FS_NAND_META_SEC_QTY) -1;
        if (avail_tbl_only == DEF_YES) {
            sec_update_cnt    = 1u;                             /* Tbl fits 1 sec (see FS_NAND_AllocDevData Note #1a).  */
            meta_sec_ix_first = 0u;
            meta_sec_ix_last  = 0u;
        } else {

            sec_update_cnt   = 0;                               /* Scan metadata sector invalid map.                    */
            for (meta_sec_ix = 0u; meta_sec_ix < p_nand_data->MetaSecCnt; meta_sec_ix++) {
                offset_octet = meta_sec_ix / DEF_OCTET_NBR_BITS;
                offset_bit   = meta_sec_ix % DEF_OCTET_NBR_BITS;

                sec_needs_update = p_nand_data->MetaBlkInvalidSecMap[offset_octet] & DEF_BIT08(offset_bit);

                if (sec_needs_update != 0u) {
                    sec_update_cnt++;

                    meta_sec_ix_last = meta_sec_ix;
                    if (meta_sec_ix_first == (FS_NAND_META_SEC_QTY) -1) {
                        meta_sec_ix_first = meta_sec_ix;
                    }

                }
            }

        }

        if (meta_sec_ix_first == (FS_NAND_META_SEC_QTY) -1) {   /* No meta sec are invalidated.                         */
            return;
        }


                                                                /* -------- PERFORM FOLD OF META BLK IF NEEDED -------- */

                                                                /* Chk if meta fits actual meta blk ...                 */
        sec_rem = p_nand_data->NbrSecPerBlk - p_nand_data->MetaBlkNextSecIx;
        if (sec_update_cnt > sec_rem) {
            p_nand_data->MetaBlkFoldNeeded = DEF_YES;           /* ... fold if not enough sec rem.                      */

            sec_update_cnt    = p_nand_data->MetaSecCnt;
            meta_sec_ix_first = 0u;
            meta_sec_ix_last  = p_nand_data->MetaSecCnt - 1u;
            avail_tbl_only    = DEF_NO;
            FS_NAND_MetaBlkFold(p_nand_data, p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }


                                                                /* ----------------- COMMIT META SEC ------------------ */
        if (avail_tbl_only == DEF_YES) {
            seq_status = FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY;
        } else {
            seq_status = FS_NAND_META_SEQ_NEW;
        }


        meta_sec_ix = meta_sec_ix_first;
        while ((meta_sec_ix <= meta_sec_ix_last) &&
               (p_nand_data->MetaBlkFoldNeeded == DEF_NO)) {

            offset_octet = 0u;
            offset_bit   = 0u;

            if (avail_tbl_only == DEF_YES) {
                sec_needs_update = 1;
            } else {
                offset_octet     = meta_sec_ix / DEF_OCTET_NBR_BITS;
                offset_bit       = meta_sec_ix % DEF_OCTET_NBR_BITS;
                sec_needs_update = p_nand_data->MetaBlkInvalidSecMap[offset_octet] & DEF_BIT08(offset_bit);
            }

            if (sec_needs_update != 0u) {

                if (sec_update_cnt == 1u) {                     /* If last sec to commit, chng seq status to finished.  */
                    if (seq_status != FS_NAND_META_SEQ_AVAIL_BLK_TBL_ONLY) {
                        seq_status =  FS_NAND_META_SEQ_FINISHED;
                    }
                }

                                                                /* Commit sector.                                       */
                FS_NAND_MetaSecCommit(p_nand_data,
                                      meta_sec_ix,
                                      seq_status,
                                      p_err);
                if (*p_err == FS_ERR_DEV_OP_ABORTED) {
                                                                /* Trigger fold and complete meta commit.               */
                    avail_tbl_only                 = DEF_NO;
                    p_nand_data->MetaBlkFoldNeeded = DEF_YES;
                } else if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_MetaCommit(): Error committing sector %u.\r\n", meta_sec_ix));
                    return;
                } else {

                    if (avail_tbl_only != DEF_YES) {            /* Remove from invalid map.                             */
                        p_nand_data->MetaBlkInvalidSecMap[offset_octet] ^= DEF_BIT(offset_bit);
                    }

                    sec_update_cnt--;
                    seq_status = FS_NAND_META_SEQ_UNFINISHED;   /* Chng seq status to unfinished for all next secs.     */


                }
            }

            meta_sec_ix++;

        }

        if (p_nand_data->MetaBlkFoldNeeded == DEF_NO) {         /* If this point is reached without fold needed ...     */
            done = DEF_YES;                                     /* ... operation is done.                               */
        }

    }

                                                                /* Set avail blk tbl commit map bits.                   */
    avail_blk_tbl_size = FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->AvailBlkTblEntryCntMax);
    for (ix = 0; ix < avail_blk_tbl_size; ix++) {
       *p_nand_data->AvailBlkTblCommitMap = 0xFF;
    }

                                                                /* -------------- UPDATE METADATA CACHE --------------- */
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN == DEF_ENABLED)             /* Copy metadata in DirtyBitmapCache.                   */
    if (avail_tbl_only == DEF_NO) {
        Mem_Copy(&p_nand_data->DirtyBitmapCache[0u],
                 &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap],
                  p_nand_data->DirtyBitmapSize);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_MetaSecCommit()
*
* Description : Commit a single metadata sector to the metadata block on device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               meta_sec_ix     Metadata sector's index.
*
*               seq_status      Sector's status to write.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_PARAMS   Low-level parameters invalid.
*
*                                   -----------RETURNED BY FS_NAND_MetaSecWrHandler()------------
*                                   See FS_NAND_MetaSecWrHandler() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should never be called by another function than FS_NAND_MetaCommit to
*                   avoid inconsistency in cache objects which are only updated in FS_NAND_MetaCommit,
*                   after a successful operation is completed.
*
*               (2) The field MetaBlkFoldNeeded of the p_nand_data structure can be set to 0 to force
*                   a fold of the metadata block. The fold will be performed after the function returns
*                   to FS_NAND_MetaCommit(), or the next time it will be called.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaSecCommit (FS_NAND_DATA             *p_nand_data,
                                             FS_NAND_META_SEC_QTY      meta_sec_ix,
                                             FS_NAND_META_SEQ_STATUS   seq_status,
                                             FS_ERR                   *p_err)
{
    CPU_INT08U                   *p_oos_buf;
    FS_NAND_SEC_TYPE_STO          sec_type;
    FS_NAND_META_ID               meta_blk_id;
    FS_NAND_META_SEQ_STATUS_STO   seq_status_sto;
    FS_NAND_ERASE_QTY             erase_cnt;


    FS_CTR_STAT_INC(p_nand_data->Ctrs.StatMetaSecCommitCtr);

    FS_NAND_TRACE_LOG(("Metadata sector %u commit at offset %u of block %u (seq %u).\r\n",
                        meta_sec_ix,
                        p_nand_data->MetaBlkNextSecIx,
                        p_nand_data->MetaBlkIxPhy,
                        p_nand_data->MetaBlkID));

                                                                /* ------------- REM AVAIL BLK TBL ENTRY -------------- */
    if (p_nand_data->MetaBlkNextSecIx == 0u) {
        erase_cnt = FS_NAND_BlkRemFromAvail(p_nand_data,
                                            p_nand_data->MetaBlkIxPhy);
    } else {
        erase_cnt = FS_NAND_ERASE_QTY_INVALID;
    }

                                                                /* ------------------- GATHER META -------------------- */
    FS_NAND_MetaSecGatherData(              p_nand_data,
                                            meta_sec_ix,
                              (CPU_INT08U *)p_nand_data->BufPtr);

                                                                /* -------------------- FILL OOS ---------------------- */
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    Mem_Set(&p_oos_buf[FS_NAND_OOS_SEC_USED_OFFSET],            /* Sec used mark.                                       */
             0x00u,
             p_nand_data->UsedMarkSize);

    sec_type = FS_NAND_SEC_TYPE_METADATA;                       /* Sec type.                                            */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                 &sec_type,
                                  sizeof(FS_NAND_SEC_TYPE_STO));

                                                                /* Erase cnt: Only valid if first sec of blk.           */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                 &erase_cnt,
                                  sizeof(FS_NAND_ERASE_QTY));

                                                                /* Meta sec ix.                                         */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_META_SEC_IX_OFFSET],
                                 &meta_sec_ix,
                                  sizeof(FS_NAND_META_SEC_QTY));

    meta_blk_id = p_nand_data->MetaBlkID;                       /* Meta blk ID.                                         */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                 &meta_blk_id,
                                  sizeof(FS_NAND_META_ID));

    seq_status_sto = seq_status;                                /* Meta seq status.                                     */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf[FS_NAND_OOS_META_SEQ_STATUS_OFFSET],
                                 &seq_status_sto,
                                  sizeof(FS_NAND_META_SEQ_STATUS_STO));


                                                                /* ---------------------- WR SEC ---------------------- */
    FS_NAND_MetaSecWrHandler(p_nand_data,
                             p_nand_data->BufPtr,
                             p_nand_data->OOS_BufPtr,
                             p_nand_data->MetaBlkNextSecIx,
                             p_err);
    switch (*p_err) {
        case FS_ERR_NONE:
             p_nand_data->MetaBlkNextSecIx++;
             if (p_nand_data->MetaBlkNextSecIx >= p_nand_data->NbrSecPerBlk) {
                 p_nand_data->MetaBlkFoldNeeded = DEF_YES;      /* Force folding meta blk (see Note #2).                */
             }
             break;


        case FS_ERR_DEV_OP_ABORTED:
             break;


        default:
             p_nand_data->MetaBlkNextSecIx++;
             if (p_nand_data->MetaBlkNextSecIx >= p_nand_data->NbrSecPerBlk) {
                 p_nand_data->MetaBlkFoldNeeded = DEF_YES;      /* Force folding meta blk (see Note #2).                */
             }
             break;
    }

}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_MetaSecGatherData()
*
* Description : Gather metadata sector data for specified sector.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               meta_sec_ix     Metadata sector's index.
*
*               p_buf           Pointer to buffer that will receive metadata sector data.
*               -----           Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaSecGatherData (FS_NAND_DATA          *p_nand_data,
                                                 FS_NAND_META_SEC_QTY   meta_sec_ix,
                                                 CPU_INT08U            *p_buf)
{
    CPU_SIZE_T   offset_octet;
    CPU_SIZE_T   size;
    CPU_INT08U  *p_metadata;


    p_metadata   = (CPU_INT08U *)p_nand_data->MetaCache;

    offset_octet = meta_sec_ix * p_nand_data->SecSize;
    size         = p_nand_data->MetaSize - offset_octet;

    if (size > p_nand_data->SecSize) {
        size = p_nand_data->SecSize;                            /* Limit data size to sector size.                      */
    }

    Mem_Copy(p_buf, &p_metadata[offset_octet], size);           /* Copy data.                                           */
}
#endif


/*
*********************************************************************************************************
*                                   FS_NAND_MetaBlkCorruptDetect()
*
* Description : Detect any possible metadata blocks corruption by checking the sequential IDs.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*
*               blk_ix_hdr      Block index of new header block.
*
*               p_err           Pointer to variable that will receive the return error code from this
*                               function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ----RETURNED BY FS_NAND_SecRdPhyNoRefresh()-----
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
* Return(s)   : DEF_YES, if a corruption is detected.
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_MetaBlkCorruptDetect (FS_NAND_DATA     *p_nand_data,
                                                           FS_NAND_BLK_QTY   blk_ix_hdr,
                                                           FS_ERR           *p_err)
{
    CPU_INT08U            *p_oos_buf;
    FS_NAND_BLK_QTY        blk_ix_phy;
    FS_NAND_BLK_QTY        blk_ix_phy_prev;
    FS_NAND_BLK_QTY        last_blk_ix;
    FS_NAND_SEC_TYPE_STO   blk_type;
    FS_NAND_META_ID        seq_nbr;
    FS_NAND_META_ID        seq_nbr_prev;
    CPU_INT32U             blk_same_seq_id_cnt;
    CPU_INT32U             blk_same_seq_id_chunk_cnt;


    p_oos_buf                 = (CPU_INT08U *)p_nand_data->OOS_BufPtr;
    seq_nbr_prev              = DEF_GET_U_MAX_VAL(FS_NAND_META_ID);
    blk_ix_phy_prev           = DEF_GET_U_MAX_VAL(FS_NAND_BLK_QTY);
    blk_same_seq_id_cnt       = 0u;
    blk_same_seq_id_chunk_cnt = 0u;

                                                                /* Find all metadada blocks.                            */
    last_blk_ix = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
    for (blk_ix_phy = blk_ix_hdr; blk_ix_phy <= last_blk_ix; blk_ix_phy++) {
        blk_type = FS_NAND_SEC_TYPE_UNUSED;

        FS_NAND_SecRdPhyNoRefresh(p_nand_data,                  /* Rd sec to get blk type.                              */
                                  p_nand_data->BufPtr,
                                  p_nand_data->OOS_BufPtr,
                                  blk_ix_phy,
                                  0u,
                                  p_err);
        switch (*p_err) {
            case FS_ERR_ECC_UNCORR:
                *p_err = FS_ERR_NONE;
                 break;


            case FS_ERR_NONE:
                *p_err    = FS_ERR_NONE;
                 blk_type = FS_NAND_SEC_TYPE_INVALID;           /* Rd blk type.                                         */
                 MEM_VAL_COPY_GET_INTU_LITTLE(&blk_type,
                                              &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                               sizeof(FS_NAND_SEC_TYPE_STO));
                 break;


            default:
                 return (DEF_NO);                               /* Return err.                                          */
        }


        if (blk_type == FS_NAND_SEC_TYPE_METADATA) {

            MEM_VAL_COPY_GET_INTU_LITTLE(&seq_nbr,              /* Rd blk ID.                                           */
                                         &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                          sizeof(FS_NAND_META_ID));

            if (seq_nbr == seq_nbr_prev) {                      /* Verify if consecutive blks with same ID.             */
                blk_same_seq_id_cnt++;
                FS_NAND_TRACE_LOG(("FS_NAND_MetaBlkCorruptDetect(): Current physical block %u has same sequential ID as previous physical block %u.\r\n", blk_ix_phy, blk_ix_phy_prev));

            } else if (blk_same_seq_id_cnt > 0u) {
                blk_same_seq_id_chunk_cnt++;                    /* New chunk of consecutive blks with same ID.          */
                blk_same_seq_id_cnt = 0u;                       /* Reset cnt for next chunk of blks with same ID.       */
            }

            seq_nbr_prev    = seq_nbr;
            blk_ix_phy_prev = blk_ix_phy;
        }
    }

    if (blk_same_seq_id_chunk_cnt > 0u) {
        FS_NAND_TRACE_DBG(("FS_NAND_MetaBlkCorruptDetect(): Corruption detected because of consecutive blocks with same sequential ID.\r\n"));
        return (DEF_YES);                                       /* Corruption detected: consecutive blks with same ID.  */
    }

    *p_err = FS_ERR_NONE;
    return (DEF_NO);
}
#endif


/*
*********************************************************************************************************
*                                   FS_NAND_MetaBlkCorruptedErase()
*
* Description : Fix metadata blocks corruption by erasing all the blocks. The next low-level format
*               will allow to recover the NAND flash.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*
*               blk_ix_hdr      Block index of new header block.
*
*               p_err           Pointer to variable that will receive the return error code from this
*                               function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ----RETURNED BY FS_NAND_SecRdPhyNoRefresh()-----
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
FS_NAND_INTERN  void  FS_NAND_MetaBlkCorruptedErase (FS_NAND_DATA     *p_nand_data,
                                                     FS_NAND_BLK_QTY   blk_ix_hdr,
                                                     FS_ERR           *p_err)
{
    CPU_INT08U            *p_oos_buf;
    FS_NAND_BLK_QTY        blk_ix_phy;
    FS_NAND_BLK_QTY        last_blk_ix;
    FS_NAND_SEC_TYPE_STO   blk_type;


   *p_err     = FS_ERR_NONE;                                    /* Initialize pointer                                   */
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;
                                                                /* Find all metadada blocks.                            */
    last_blk_ix = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
    for (blk_ix_phy = blk_ix_hdr; blk_ix_phy <= last_blk_ix; blk_ix_phy++) {
        blk_type = FS_NAND_SEC_TYPE_UNUSED;

        FS_NAND_SecRdPhyNoRefresh(p_nand_data,                  /* Rd sec to get blk type.                              */
                                  p_nand_data->BufPtr,
                                  p_nand_data->OOS_BufPtr,
                                  blk_ix_phy,
                                  0u,
                                  p_err);
        switch (*p_err) {
            case FS_ERR_ECC_UNCORR:
                *p_err = FS_ERR_NONE;
                 break;


            case FS_ERR_NONE:
                *p_err    = FS_ERR_NONE;
                 blk_type = FS_NAND_SEC_TYPE_INVALID;           /* Rd blk type.                                         */
                 MEM_VAL_COPY_GET_INTU_LITTLE(&blk_type,
                                              &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                               sizeof(FS_NAND_SEC_TYPE_STO));
                 break;


            default:
                 return;                                        /* Return err.                                          */
        }


        if (blk_type == FS_NAND_SEC_TYPE_METADATA) {            /* Erase metadata block to reuse it.                    */

            FS_NAND_BlkEraseHandler(p_nand_data,
                                    blk_ix_phy,
                                    p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_AvailBlkTmpCommit()
*
* Description : Commits metadata to sector to device immediately.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ----RETURNED BY FS_NAND_MetaCommit()-----
*                                   See FS_NAND_MetaCommit() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_AvailBlkTblTmpCommit (FS_NAND_DATA  *p_nand_data,
                                                    FS_ERR        *p_err)
{
    do {
       *p_err = FS_ERR_NONE;

        FS_NAND_MetaCommit(p_nand_data,
                           DEF_YES,
                           p_err);

    } while ((*p_err         != FS_ERR_NONE) &&
             (*p_err         != FS_ERR_DEV_NAND_NO_AVAIL_BLK));
    if (*p_err != FS_ERR_NONE) {
        return;
    }
}
#endif


/*
*********************************************************************************************************
*                                     FS_NAND_AvailBlkTblEntryRd()
*
* Description : Reads an entry from available blocks table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               tbl_ix          Index of entry to read.
*
* Return(s)   : Structure containing available block's physical index and its erase count.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_AVAIL_BLK_ENTRY  FS_NAND_AvailBlkTblEntryRd (FS_NAND_DATA     *p_nand_data,
                                                                     FS_NAND_BLK_QTY   tbl_ix)
{
    FS_NAND_AVAIL_BLK_ENTRY  entry;
    CPU_SIZE_T               metadata_offset;


    metadata_offset  = tbl_ix * (sizeof(FS_NAND_BLK_QTY) + sizeof(FS_NAND_ERASE_QTY));
    metadata_offset += p_nand_data->MetaOffsetAvailBlkTbl;

    MEM_VAL_COPY_GET_INTU_LITTLE(&entry.BlkIxPhy,
                                 &p_nand_data->MetaCache[metadata_offset],
                                  sizeof(FS_NAND_BLK_QTY));

    metadata_offset += sizeof(FS_NAND_BLK_QTY);
    MEM_VAL_COPY_GET_INTU_LITTLE(&entry.EraseCnt,
                                 &p_nand_data->MetaCache[metadata_offset],
                                  sizeof(FS_NAND_ERASE_QTY));

    return (entry);
}


/*
*********************************************************************************************************
*                                     FS_NAND_AvailBlkTblEntryWr()
*
* Description : Writes an entry to available blocks table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               tbl_ix          Index of entry to write.
*
*               entry           Entry to write in available blocks table.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_AvailBlkTblEntryWr (FS_NAND_DATA             *p_nand_data,
                                                  FS_NAND_BLK_QTY           tbl_ix,
                                                  FS_NAND_AVAIL_BLK_ENTRY   entry)
{
    CPU_SIZE_T  metadata_offset;


    metadata_offset  = tbl_ix * (sizeof(FS_NAND_BLK_QTY) + sizeof(FS_NAND_ERASE_QTY));
    metadata_offset += p_nand_data->MetaOffsetAvailBlkTbl;

    MEM_VAL_COPY_SET_INTU_LITTLE(&p_nand_data->MetaCache[metadata_offset],
                                 &entry.BlkIxPhy,
                                  sizeof(FS_NAND_BLK_QTY));

    metadata_offset += sizeof(FS_NAND_BLK_QTY);
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_nand_data->MetaCache[metadata_offset],
                                 &entry.EraseCnt,
                                  sizeof(FS_NAND_ERASE_QTY));


                                                                /* Clear entry bit in avail blk tbl commit map.         */
    FSUtil_MapBitClr(p_nand_data->AvailBlkTblCommitMap, tbl_ix);
}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_AvailBlkTblFill()
*
* Description : Fill the available block table until it contains at least min_nbr_entries.
*
* Argument(s) : p_nand_data             Pointer to NAND data.
*               -----------             Argument validated by caller.
*
*               nbr_entries_min         Minimum number of entries in the table after it's been filled.
*
*               p_err                   Pointer to variable that will receive the return error code from this function :
*               -----                   Argument validated by caller.
*
*                                           FS_ERR_DEV_NAND_NO_AVAIL_BLK    No more blocks available.
*                                           FS_ERR_NONE                     Operation was successful.
*
*                                           ------------RETURNED BY FS_NAND_BlkGetDirty()------------
*                                           See FS_NAND_BlkGetDirty() for additional return error codes.
*
*                                           -----------RETURNED BY FS_NAND_BlkAddToAvail()-----------
*                                           See FS_NAND_BlkAddToAvail() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_AvailBlkTblFill (FS_NAND_DATA     *p_nand_data,
                                               FS_NAND_BLK_QTY   nbr_entries_min,
                                               CPU_BOOLEAN       pending_dirty_chk_en,
                                               FS_ERR           *p_err)
{
    FS_NAND_AVAIL_BLK_ENTRY  entry;
    FS_NAND_BLK_QTY          entry_cnt;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (nbr_entries_min > p_nand_data->AvailBlkTblEntryCntMax) {
        FS_NAND_TRACE_DBG(("FS_NAND_AvailBlkTblFill(): Could not add dirty block to available blocks table. "));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
#endif

                                                                /* ----------- CNT ENTRIES IN AVAIL BLK TBL ----------- */
    entry_cnt = FS_NAND_AvailBlkTblEntryCnt(p_nand_data);

                                                                /* --------------------- FILL TBL --------------------- */
    while (entry_cnt < nbr_entries_min) {
                                                                /* Get dirty blk.                                       */
        entry.BlkIxPhy = FS_NAND_BlkGetDirty(p_nand_data, pending_dirty_chk_en, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (entry.BlkIxPhy == FS_NAND_BLK_IX_INVALID) {         /* No more blks avail.                                  */
           *p_err = FS_ERR_DEV_NAND_NO_AVAIL_BLK;
            return;
        }

                                                                /* Add blk to avail blk tbl.                            */
        FS_NAND_BlkUnmap(p_nand_data, entry.BlkIxPhy);

        FS_NAND_BlkAddToAvail(p_nand_data, entry.BlkIxPhy, p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_AvailBlkTblFill(): Could not add dirty block to available blocks table. "
                               "This block's erase count might be lost in the event of a power-loss.\r\n"));
           *p_err = FS_ERR_NONE;
        }

        entry_cnt++;
    }
}
#endif


/*
*********************************************************************************************************
*                                    FS_NAND_AvailBlkTblEntryCnt()
*
* Description : Counts the number of entries in the available blocks table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*
* Return(s)   : number of available blocks in the available blocks table.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_BLK_QTY  FS_NAND_AvailBlkTblEntryCnt (FS_NAND_DATA  *p_nand_data)
{
    FS_NAND_AVAIL_BLK_ENTRY  entry;
    FS_NAND_BLK_QTY          tbl_ix;
    FS_NAND_BLK_QTY          entry_cnt;


                                                                /* ----------- CNT ENTRIES IN AVAIL BLK TBL ----------- */
    entry_cnt = 0u;
    for (tbl_ix = 0; tbl_ix < p_nand_data->AvailBlkTblEntryCntMax; tbl_ix++) {
        entry = FS_NAND_AvailBlkTblEntryRd(p_nand_data, tbl_ix);

        if (entry.BlkIxPhy != FS_NAND_BLK_IX_INVALID) {
            entry_cnt++;
        }
    }

    return (entry_cnt);
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_UB_TblEntryRd()
*
* Description : Reads an entry from update block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               tbl_ix          Index of entry to read.
*
* Return(s)   : Structure containing update block's physical index and its associated valid sector map.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_UB_DATA  FS_NAND_UB_TblEntryRd (FS_NAND_DATA    *p_nand_data,
                                                        FS_NAND_UB_QTY   tbl_ix)
{
    FS_NAND_UB_DATA  entry;
    CPU_SIZE_T       metadata_offset;


    metadata_offset  = FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->NbrSecPerBlk);
    metadata_offset += sizeof(FS_NAND_BLK_QTY);
    metadata_offset *= tbl_ix;
    metadata_offset += p_nand_data->MetaOffsetUB_Tbl;

    MEM_VAL_COPY_GET_INTU_LITTLE(&entry.BlkIxPhy,
                                 &p_nand_data->MetaCache[metadata_offset],
                                  sizeof(FS_NAND_BLK_QTY));

    metadata_offset      +=  sizeof(FS_NAND_BLK_QTY);
    entry.SecValidBitMap  = &p_nand_data->MetaCache[metadata_offset];

    return (entry);
}


/*
*********************************************************************************************************
*                                        FS_NAND_UB_TblEntryWr()
*
* Description : Writes an entry to update block table.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               tbl_ix          Index of entry to read.
*
*               blk_ix_phy      Physical block index of new update block.
*
* Return(s)   : none.
*
* Note(s)     : (1) The sector valid map may be modified by using FS_NAND_UB_TblEntryRd() and
*                   modifying data pointed by returned field sec_valid_map.
*
*               (2) After modifying the UB (update block) table, the function must invalidate it.
*                   See FS_NAND_UB_TblInvalidate note #1.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_UB_TblEntryWr (FS_NAND_DATA     *p_nand_data,
                                             FS_NAND_UB_QTY    tbl_ix,
                                             FS_NAND_BLK_QTY   blk_ix_phy)
{
    CPU_SIZE_T  metadata_offset;


    metadata_offset  =  FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->NbrSecPerBlk);
    metadata_offset +=  sizeof(FS_NAND_BLK_QTY);
    metadata_offset *=  tbl_ix;
    metadata_offset +=  p_nand_data->MetaOffsetUB_Tbl;

    MEM_VAL_COPY_SET_INTU_LITTLE(&p_nand_data->MetaCache[metadata_offset],
                                 &blk_ix_phy,
                                  sizeof(FS_NAND_BLK_QTY));


    FS_NAND_UB_TblInvalidate(p_nand_data);                      /* Invalidate tbl (See note #2).                        */
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_UB_Create()
*
* Description : Create an update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               ub_ix           Index of new update block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   ---RETURNED BY FS_NAND_BlkGetErased()----
*                                   See FS_NAND_BlkGetErased() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_UB_Create (FS_NAND_DATA    *p_nand_data,
                                         FS_NAND_UB_QTY   ub_ix,
                                         FS_ERR          *p_err)
{
    FS_NAND_BLK_QTY  blk_ix_phy;


                                                                /* ------------------ GET ERASED BLK ------------------ */
    blk_ix_phy = FS_NAND_BlkGetErased(p_nand_data,
                                      p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_UB_Create(): Unable to get an erased block.\r\n"));
        return;
    }

                                                                /* ----------------- UPDATE METADATA ------------------ */
    FS_NAND_UB_TblEntryWr(p_nand_data,
                          ub_ix,
                          blk_ix_phy);

    FS_NAND_TRACE_LOG(("Create UB %u at phy ix %u.\r\n",
                        ub_ix,
                        blk_ix_phy));
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_UB_Load()
*
* Description : Loads metadata structures associated with specified update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Physical block index of new update block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_CORUPT_LOW_FMT       Associativity is higher than maximum.
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata was not found for this block.
*                                   FS_ERR_NONE                     Metadata loaded successfully.
*
*                                   ---------------------RETURNED BY FS_NAND_SecIsUsed()----------------------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   ---------------------RETURNED BY p_ctrlr_api->SecRd()---------------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_UB_Load (FS_NAND_DATA     *p_nand_data,
                                       FS_NAND_BLK_QTY   blk_ix_phy,
                                       FS_ERR           *p_err)
{
    CPU_INT08U               *p_oos_buf;
    FS_NAND_UB_QTY            ub_ix;
    FS_NAND_UB_DATA           entry;
    FS_NAND_UB_EXTRA_DATA    *p_entry_extra;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy;
    FS_NAND_BLK_QTY           blk_ix_logical;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical;
    CPU_BOOLEAN               sec_valid;
    CPU_BOOLEAN               sec_is_used;
    FS_SEC_QTY                sec_ix_base;
    CPU_INT08U                associated_blk_tbl_ix;
    CPU_SIZE_T                ub_sec_map_loc_octet_array;
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0u)
    CPU_SIZE_T                ub_sec_map_pos_bit_array;
#endif
    CPU_DATA                  ub_sec_map_loc_bit_octet;
    CPU_SIZE_T                ub_meta_loc_octet_array;
#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
    CPU_SIZE_T                ub_meta_pos_bit_array;
#endif
    CPU_DATA                  ub_meta_loc_bit_octet;
    CPU_INT08U                rd_retries;
    CPU_BOOLEAN               blk_is_sequential;
    CPU_BOOLEAN               entry_found;
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0u)
    FS_NAND_SEC_PER_BLK_QTY   blk_sec_subset_ix;
#endif


    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;


                                                                /* ---------------- FIND ENTRY IN TBL ----------------- */
    entry_found = DEF_NO;
    ub_ix       = 0u;
    while ((entry_found != DEF_YES) &&
           (ub_ix        < p_nand_data->UB_CntMax)) {
                                                                /* Rd UB tbl entry.                                     */
        entry = FS_NAND_UB_TblEntryRd(p_nand_data, ub_ix);

        if (entry.BlkIxPhy == blk_ix_phy) {
                                                                /* Rd extra data tbl.                                   */
            p_entry_extra = &p_nand_data->UB_ExtraDataTbl[ub_ix];
            entry_found = DEF_YES;
        } else {

            ub_ix++;
        }
    }

    if (entry_found != DEF_YES) {                                /* Blk wasn't found.                                   */
       *p_err = FS_ERR_DEV_INVALID_METADATA;
        return;
    }

                                                                /* Inits ptrs for UB meta info.                         */
    ub_sec_map_loc_octet_array = 0u;
    ub_sec_map_loc_bit_octet   = 0u;
    ub_meta_loc_octet_array    = 0u;
    ub_meta_loc_bit_octet      = 0u;

    sec_ix_base                = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
    blk_is_sequential          = DEF_YES;

                                                                /* ------------ SCAN TO FIND LAST SEC USED ------------ */
    sec_offset_phy = p_nand_data->NbrSecPerBlk;
    while ((p_entry_extra->NextSecIx == 0u) &&
           (sec_offset_phy != 0u)) {
        sec_offset_phy--;

        sec_is_used = FS_NAND_SecIsUsed(p_nand_data, sec_ix_base + sec_offset_phy, p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_Load(): Fatal error determining if sector %u is used.\r\n",
                                sec_ix_base + sec_offset_phy));
            return;
        }

        if (sec_is_used == DEF_YES) {
            p_entry_extra->NextSecIx = sec_offset_phy + 1u;
        }
    }

                                                                /* --------------- SCAN SEC TO GET INFO --------------- */
    for (sec_offset_phy = 0u; sec_offset_phy < p_nand_data->NbrSecPerBlk; sec_offset_phy++) {
        sec_valid = FSUtil_MapBitIsSet(entry.SecValidBitMap, sec_offset_phy);
                                                                /* If sec is valid ...                                  */
        if (sec_valid == DEF_YES) {

                                                                /* ... rd sec & retry if it fails.                      */
            rd_retries = 0u;

            do {
               *p_err = FS_ERR_NONE;

                FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                          p_nand_data->BufPtr,
                                          p_oos_buf,
                                          blk_ix_phy,
                                          sec_offset_phy,
                                          p_err);

                rd_retries++;
            } while ((*p_err     == FS_ERR_ECC_UNCORR) &&
                     (rd_retries  < FS_NAND_CFG_MAX_RD_RETRIES));

            if (*p_err != FS_ERR_NONE) {
                return;
            }


            MEM_VAL_COPY_GET_INTU_LITTLE(&blk_ix_logical,       /* Get blk ix logical for sec.                          */
                                         &p_oos_buf[FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET],
                                          sizeof(FS_NAND_BLK_QTY));

            MEM_VAL_COPY_GET_INTU_LITTLE(&sec_offset_logical,   /* Get sec ix logical for sec.                          */
                                         &p_oos_buf[FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET],
                                          sizeof(FS_NAND_SEC_PER_BLK_QTY));

                                                                /* Find logical blk ix in associated blk tbl.           */
            associated_blk_tbl_ix = 0u;
            while ((p_entry_extra->AssocLogicalBlksTbl[associated_blk_tbl_ix] != blk_ix_logical) &&
                   (associated_blk_tbl_ix < p_nand_data->RUB_MaxAssoc)                              ) {
                                                                /* If space is left, insert it.                         */
                if (p_entry_extra->AssocLogicalBlksTbl[associated_blk_tbl_ix] == FS_NAND_BLK_IX_INVALID) {
                    p_entry_extra->AssocLogicalBlksTbl[associated_blk_tbl_ix] = blk_ix_logical;
                    p_entry_extra->AssocLvl++;
                } else {
                    associated_blk_tbl_ix++;
                }

            }

                                                                /* Handle tbl overflow.                                 */
            if (associated_blk_tbl_ix > p_nand_data->RUB_MaxAssoc) {
                FS_NAND_TRACE_DBG(("FS_NAND_UB_Load(): Update block %u associativity exceeds maximum value.\r\n",
                                    blk_ix_phy));
               *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                return;
            }

                                                                /* Update sec mapping tbl.                              */
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0u)
            blk_sec_subset_ix = sec_offset_phy / FS_NAND_CFG_UB_TBL_SUBSET_SIZE;

                                                                /* Calc pos in tbl.                                     */
            ub_sec_map_pos_bit_array  = (associated_blk_tbl_ix * p_nand_data->NbrSecPerBlk) + sec_offset_logical;
            ub_sec_map_pos_bit_array *= p_nand_data->UB_SecMapNbrBits;

            FS_UTIL_BITMAP_LOC_GET(ub_sec_map_pos_bit_array, ub_sec_map_loc_octet_array, ub_sec_map_loc_bit_octet);

            if (sec_offset_logical != FS_NAND_SEC_OFFSET_IX_INVALID) {
                FSUtil_ValPack32(&p_entry_extra->LogicalToPhySecMap[0],
                                 &ub_sec_map_loc_octet_array,
                                 &ub_sec_map_loc_bit_octet,
                                  blk_sec_subset_ix,
                                  p_nand_data->UB_SecMapNbrBits);
            } else {
                FS_NAND_TRACE_DBG(("FS_NAND_UB_Load(): Sector with physical offset %u of update block %u has invalid logical block sector index.\r\n",
                                    sec_offset_phy,
                                    ub_ix));
            }
#endif
                                                                /* Update UB meta cache.                                */
#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)

            FSUtil_ValPack32(&p_entry_extra->MetaCachePtr[0],
                             &ub_meta_loc_octet_array,
                             &ub_meta_loc_bit_octet,
                              sec_offset_logical,
                              p_nand_data->NbrSecPerBlkLog2);

            FSUtil_ValPack32(&p_entry_extra->MetaCachePtr[0],
                             &ub_meta_loc_octet_array,
                             &ub_meta_loc_bit_octet,
                              associated_blk_tbl_ix,
                              p_nand_data->RUB_MaxAssocLog2);

#endif

            if (sec_offset_logical != sec_offset_phy) {
                blk_is_sequential = DEF_NO;
            }

        } else {
            blk_is_sequential = DEF_NO;                         /* SUB can't have any invalid sec.                      */

#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
                                                                /* Update UB meta cache ptrs.                           */
            ub_meta_pos_bit_array   = ub_meta_loc_octet_array << DEF_OCTET_TO_BIT_SHIFT;
            ub_meta_pos_bit_array  += ub_meta_loc_bit_octet;
            ub_meta_pos_bit_array  += (p_nand_data->NbrSecPerBlkLog2 + p_nand_data->RUB_MaxAssocLog2);

            FS_UTIL_BITMAP_LOC_GET(ub_meta_pos_bit_array, ub_meta_loc_octet_array, ub_meta_loc_bit_octet);
#endif
        }
    }

    p_entry_extra->ActivityCtr = 0u;

                                                                /* ---------------- CHK IF UB IS A SUB ---------------- */
    if ((blk_is_sequential == DEF_YES) &&
        (p_entry_extra->AssocLvl == 1u)) {
        p_entry_extra->AssocLvl = 0u;                           /* Make it a SUB.                                       */

        p_nand_data->SUB_Cnt++;
    }
}


/*
*********************************************************************************************************
*                                         FS_NAND_UB_Clr()
*
* Description : Removes specified update block from update block tables and clears associated data
*               structures.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               ub_ix           Index of update block.
*
* Return(s)   : none.
*
* Note(s)     : (1) Not all fields of the update block extra data (FS_NAND_UB_EXTRA_DATA) must
*                   be cleared. Pointer to buffers need to be maintained. However, the associated
*                   logical blocks table (AssocLogicalBlkTbl) entries must be set to their maximum value,
*                   which means they are invalid. The driver must check for those entries if the data
*                   is valid or not.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_UB_Clr (FS_NAND_DATA    *p_nand_data,
                                      FS_NAND_UB_QTY   ub_ix)
{
    FS_NAND_UB_DATA         ub_data;
    FS_NAND_UB_EXTRA_DATA  *p_ub_extra_data;
    CPU_SIZE_T              size;
    CPU_DATA                ix;


    FS_NAND_TRACE_LOG(("Clearing update blk %u.\r\n", ub_ix));

                                                                /* --------------------- UB DATA ---------------------- */
    ub_data = FS_NAND_UB_TblEntryRd(p_nand_data, ub_ix);

    size = FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->NbrSecPerBlk);
    Mem_Clr(&ub_data.SecValidBitMap[0], size);

    FS_NAND_UB_TblEntryWr(p_nand_data, ub_ix, FS_NAND_BLK_IX_INVALID);


                                                                /* ------------------ UB EXTRA DATA ------------------- */
    p_ub_extra_data              = &p_nand_data->UB_ExtraDataTbl[ub_ix];

    p_ub_extra_data->AssocLvl    =  0u;                         /* Not all fields must be cleared (see #note #1).       */
    p_ub_extra_data->NextSecIx   =  0u;
    p_ub_extra_data->ActivityCtr =  0u;

    for (ix = 0; ix < p_nand_data->RUB_MaxAssoc; ix++) {
        p_ub_extra_data->AssocLogicalBlksTbl[ix] = FS_NAND_BLK_IX_INVALID;
    }
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_UB_Find()
*
* Description : Find update block associated with specified logical block index.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_logical  Index of logical block.
*
* Return(s)   : Index of update block associated with blk_ix_logical,   if found,
*               FS_NAND_BLK_IX_INVALID,                                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_UB_SEC_DATA  FS_NAND_UB_Find (FS_NAND_DATA     *p_nand_data,
                                                      FS_NAND_BLK_QTY   blk_ix_logical)
{
    FS_NAND_UB_QTY         ub_ix;
    FS_NAND_UB_EXTRA_DATA  ub_extra_data;
    FS_NAND_UB_SEC_DATA    ub_sec_data;
    FS_NAND_ASSOC_BLK_QTY  assoc_blk_ix;


    for (ub_ix = 0u; ub_ix < p_nand_data->UB_CntMax; ub_ix++) {
        ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

        for (assoc_blk_ix = 0u; assoc_blk_ix < p_nand_data->RUB_MaxAssoc; assoc_blk_ix++) {
            if (ub_extra_data.AssocLogicalBlksTbl[assoc_blk_ix] == blk_ix_logical) {
                ub_sec_data.AssocLogicalBlksTblIx = assoc_blk_ix;
                ub_sec_data.UB_Ix                 = ub_ix;
                return (ub_sec_data);
            }
        }
    }

    ub_sec_data.UB_Ix = FS_NAND_UB_IX_INVALID;
    return (ub_sec_data);
}


/*
*********************************************************************************************************
*                                         FS_NAND_UB_SecFind()
*
* Description : Finds latest version of specified sector in update block.
*
* Argument(s) : p_nand_data         Pointer to NAND data.
*               -----------         Argument validated by caller.
*
*               ub_sec_data         Struct containing index of update block and associated block index.
*
*               sec_offset_logical  Sector offset relative to logical block.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               -----               Argument validated by caller.
*
*                                       FS_ERR_NONE     Operation was successful.
*
*                                       ---RETURNED BY FS_NAND_SecRdHandler()----
*                                       See FS_NAND_SecRdHandler() for additional return error codes.
*
* Return(s)   : ub_sec_data copy with .SecOffsetPhy set to: Offset of sector in update block,  if found,
*                                                           FS_NAND_SEC_OFFSET_IX_INVALID,     otherwise.
*
* Note(s)     : (1) Even if the sector was not found, p_err will not be set. Caller has to check return
*                   value.
*
*               (2) This function assumes that 'ub_sec_data' contains a valid index of update block
*                   and a valid index of the logical block in the associated blocks table. The function
*                   will then find where the logical sector in located, and set it in the return
*                   FS_NAND_UB_SEC_DATA structure.
*
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_UB_SEC_DATA  FS_NAND_UB_SecFind (FS_NAND_DATA             *p_nand_data,
                                                         FS_NAND_UB_SEC_DATA       ub_sec_data,
                                                         FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical,
                                                         FS_ERR                   *p_err)
{
    FS_NAND_UB_EXTRA_DATA     ub_extra_data;
    FS_NAND_UB_DATA           ub_data;
    FS_NAND_SEC_PER_BLK_QTY   range_begin;
    FS_NAND_SEC_PER_BLK_QTY   range_end;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_rd;
    CPU_SIZE_T                loc_octet_array;
    CPU_DATA                  loc_bit_octet;
    CPU_BOOLEAN               sec_valid;
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
    FS_NAND_SEC_PER_BLK_QTY   subset_ix;
#endif
#if ((FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED) || (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0))
    CPU_SIZE_T                pos_bit_array;
#endif
#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
    FS_NAND_ASSOC_BLK_QTY     assoc_blk_ix_rd;
#else
    FS_NAND_BLK_QTY           blk_ix_logical_rd;
    FS_NAND_BLK_QTY           blk_ix_logical;
    CPU_INT08U               *p_oos_buf;
#endif


#if (FS_NAND_CFG_UB_META_CACHE_EN != DEF_ENABLED)
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;
#else
    (void)p_err;
#endif

    ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_sec_data.UB_Ix];
    ub_data       = FS_NAND_UB_TblEntryRd(p_nand_data, ub_sec_data.UB_Ix);

    if (ub_extra_data.AssocLvl == 0) {                          /* ----------------------- SUB ------------------------ */
        sec_offset_phy = sec_offset_logical;                    /* In a SUB, logical offset is equal to phy offset.     */
                                                                /* Chk if SUB sec has been wr'en.                       */
        if (sec_offset_phy < ub_extra_data.NextSecIx) {
            FS_UTIL_BITMAP_LOC_GET(sec_offset_phy, loc_octet_array, loc_bit_octet);
                                                                /* Check if sec is still valid.                         */
            if (DEF_BIT_IS_SET(ub_data.SecValidBitMap[loc_octet_array], DEF_BIT(loc_bit_octet)) == DEF_YES) {
                ub_sec_data.SecOffsetPhy = sec_offset_phy;
                return (ub_sec_data);
            } else {
                ub_sec_data.SecOffsetPhy = FS_NAND_SEC_OFFSET_IX_INVALID;
                return (ub_sec_data);
            }
        } else {
            ub_sec_data.SecOffsetPhy = FS_NAND_SEC_OFFSET_IX_INVALID;
            return (ub_sec_data);
        }
    } else {                                                    /* ----------------------- RUB ------------------------ */

                                                                /* ------ RUB: DETERMINE SRCH RANGE FROM SUBSET ------- */
                                                                /* See ' NAND FTL PARAMETERS DEFINES' Note #2.          */
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
        pos_bit_array    = ub_sec_data.AssocLogicalBlksTblIx;
        pos_bit_array   *= p_nand_data->NbrSecPerBlk;
        pos_bit_array   *= p_nand_data->UB_SecMapNbrBits;
        pos_bit_array   += (CPU_INT32U)sec_offset_logical * p_nand_data->UB_SecMapNbrBits;

        FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

                                                                /* Unpack subset ix from map.                           */
        subset_ix = FSUtil_ValUnpack32(ub_extra_data.LogicalToPhySecMap,
                                      &loc_octet_array,
                                      &loc_bit_octet,
                                       p_nand_data->UB_SecMapNbrBits);

                                                                /* Calc range from subset ix.                           */
        range_begin = subset_ix   * FS_NAND_CFG_UB_TBL_SUBSET_SIZE;
        range_end   = range_begin + FS_NAND_CFG_UB_TBL_SUBSET_SIZE - 1u;
#else
        range_begin = 0u;
        range_end   = p_nand_data->NbrSecPerBlk - 1u;
#endif
                                                                /* Limit range to wr'en sec in RUB.                     */
        if (ub_extra_data.NextSecIx == 0u) {
            range_end = 0u;
        } else {
            if (range_end >= ub_extra_data.NextSecIx) {
                range_end = ub_extra_data.NextSecIx - 1u;
            }
        }

                                                                /* -------------- RUB: SRCH SEC IN RANGE -------------- */
        sec_offset_phy = range_begin;
        while (sec_offset_phy <= range_end) {
                                                                /* Chk if sec is valid.                                 */
            sec_valid = FSUtil_MapBitIsSet(ub_data.SecValidBitMap, sec_offset_phy);

            if (sec_valid == DEF_YES) {                         /* Sec is valid, may contain logical sec.               */
#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)               /* Srch in meta cache.                                  */
                pos_bit_array   = sec_offset_phy  * (p_nand_data->RUB_MaxAssocLog2 + p_nand_data->NbrSecPerBlkLog2);

                FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

                                                                /* Unpack sec offset.                                   */
                sec_offset_rd   = FSUtil_ValUnpack32(ub_extra_data.MetaCachePtr,
                                                    &loc_octet_array,
                                                    &loc_bit_octet,
                                                     p_nand_data->NbrSecPerBlkLog2);

                                                                /* Unpack assoc blk ix.                                 */
                assoc_blk_ix_rd = FSUtil_ValUnpack32(ub_extra_data.MetaCachePtr,
                                                    &loc_octet_array,
                                                    &loc_bit_octet,
                                                     p_nand_data->RUB_MaxAssocLog2);

                                                                /* Compare against srch'ed values.                      */
                if ((sec_offset_rd == sec_offset_logical) &&
                    (assoc_blk_ix_rd == ub_sec_data.AssocLogicalBlksTblIx)) {
                    ub_sec_data.SecOffsetPhy = sec_offset_phy;
                    return (ub_sec_data);
                }
#else                                                           /* Srch directly on device.                             */
                blk_ix_logical = ub_extra_data.AssocLogicalBlksTbl[ub_sec_data.AssocLogicalBlksTblIx];


                FS_NAND_SecRdHandler(p_nand_data,               /* Rd sec.                                              */
                                     p_nand_data->BufPtr,
                                     p_nand_data->OOS_BufPtr,
                                     UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_sec_data.UB_Ix),
                                     sec_offset_phy,
                                     p_err);

                if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_UB_SecFind: Unable to read sec %u in update block %u.\r\n",
                                        sec_offset_phy,
                                        ub_sec_data.UB_Ix));
                    ub_sec_data.SecOffsetPhy = FS_NAND_SEC_OFFSET_IX_INVALID;
                    return (ub_sec_data);
                }
                                                                /* Get blk sec ix.                                      */
                MEM_VAL_COPY_GET_INTU_LITTLE(&sec_offset_rd,
                                             &p_oos_buf[FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET],
                                              sizeof(FS_NAND_SEC_PER_BLK_QTY));

                                                                /* Get blk ix logical.                                  */
                MEM_VAL_COPY_GET_INTU_LITTLE(&blk_ix_logical_rd,
                                             &p_oos_buf[FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET],
                                              sizeof(FS_NAND_BLK_QTY));

                                                                /* Compare against srch'ed values.                      */
                if ((sec_offset_rd     == sec_offset_logical) &&
                    (blk_ix_logical_rd == blk_ix_logical)) {
                    ub_sec_data.SecOffsetPhy = sec_offset_phy;
                    return (ub_sec_data);
                }
#endif
            }

            sec_offset_phy++;
        }
    }


    ub_sec_data.SecOffsetPhy = FS_NAND_SEC_OFFSET_IX_INVALID;   /* Could not find sec.                                  */
    return (ub_sec_data);
}


/*
*********************************************************************************************************
*                                        FS_NAND_UB_IncAssoc()
*
* Description : Associates update block with specified logical block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               ub_ix           Index of update block to associate with logical block.
*
*               blk_ix_logical  Index of logical block.
*
* Return(s)   : none.
*
* Note(s)     : (1) Caller must ensure associativity value of update block is not maxed prior to call.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_UB_IncAssoc (FS_NAND_DATA     *p_nand_data,
                                           FS_NAND_UB_QTY    ub_ix,
                                           FS_NAND_BLK_QTY   blk_ix_logical)
{
    FS_NAND_UB_EXTRA_DATA  *p_ub_extra_data;
    FS_NAND_ASSOC_BLK_QTY   assoc_blk_ix;
    CPU_BOOLEAN             added;


    FS_NAND_TRACE_LOG(("Associate update blk %u with blk ix logical %u.\r\n",
                        ub_ix,
                        blk_ix_logical));

    p_ub_extra_data = &p_nand_data->UB_ExtraDataTbl[ub_ix];

                                                                /* Ensure that assoc can be increased.                  */
    if (p_ub_extra_data->AssocLvl >= p_nand_data->RUB_MaxAssoc) {
        FS_NAND_TRACE_DBG(("FS_NAND_UB_IncAssoc(): Fatal error. Can't increase associativity for update block %u.\r\n",
                            ub_ix));
    } else {                                                    /* Assoc can be increased.                              */
        added        = DEF_NO;
        assoc_blk_ix = 0u;
        while ((assoc_blk_ix < p_nand_data->RUB_MaxAssoc) &&
               (added == DEF_NO)) {
            if (p_ub_extra_data->AssocLogicalBlksTbl[assoc_blk_ix] == FS_NAND_BLK_IX_INVALID) {
                p_ub_extra_data->AssocLogicalBlksTbl[assoc_blk_ix]  = blk_ix_logical;
                p_ub_extra_data->AssocLvl++;
                added = DEF_YES;
            } else {
                if (p_ub_extra_data->AssocLogicalBlksTbl[assoc_blk_ix] == blk_ix_logical) {
                    p_ub_extra_data->AssocLvl++;
                    added = DEF_YES;                            /* Logical blk already assoc'd.                         */
                }
            }

            assoc_blk_ix++;
        }

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (added == DEF_NO) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_IncAssoc(): Fatal error: unable to associate logical block %u with update block %u.\r\n",
                                blk_ix_logical,
                                ub_ix));
            CPU_SW_EXCEPTION(;);
        }
#endif
    }
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_UB_Alloc()
*
* Description : Allocates an update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               sequential      DEF_YES, if update block will be a sequential update block,
*                               DEF_NO , otherwise.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   -----RETURNED BY FS_NAND_RUB_Merge()-----
*                                   See FS_NAND_RUB_Merge() for additional return error codes.
*
*                                   -----RETURNED BY FS_NAND_SUB_Merge()-----
*                                   See FS_NAND_SUB_Merge() for additional return error codes.
*
*                                   -----RETURNED BY FS_NAND_UB_Create()-----
*                                   See FS_NAND_UB_Create() for additional return error codes.
*
* Return(s)   : Index of allocated update block,    if operation succeeded,
*               FS_NAND_BLK_IX_INVALID,             otherwise.
*
* Note(s)     : (1) Flowchart for function is shown below. Block labels are shown in section comments.
*
*                                         +--------+
*                                         |Alloc UB|
*                                         +----+---+
*                                              |
*                                              |                    (A)
*                                        /-----+-----\      +-----------------+
*                                        |A full SUB |______| Merge SUB in DB |
*                                        |  exists?  |Yes   |    Create UB    |
*                                        \-----+-----/      +-----------------+
*                                              |No
*                                      /-------+------\     /--------------\              (F)
*                                      |              |     |  sequential  |      +-----------------+
*                                      |Empty UB Slot |-----|      ==      |---+--|    Create UB    |
*                                      |              |Yes  |    DEF_NO    |Yes|  +-----------------+
*                                      \-------+------/     \-------+------/   |
*                                              |No                  |No        |
*                                              |            /-------+------\   |
*                                              |            | SUB cnt < max|---+
*                                              |            \-------+------/Yes
*                                              |                    |No
*                                              +--------------------+
*                                              |
*                                              |                                          (B)
*                                      /-------+------\     /----------------\    +-----------------+
*                                      |  sequential  |     |  a RUB exists  |    |  Choose RUB to  |
*                                      |      ==      |-----|   with k < K   |----|    associate    |
*                                      |    DEF_NO    |Yes  |       ?        |Yes |     with LB     |
*                                      \-------+------/     \----------------/    +-----------------+
*                                              |No                  |No                   (C)
*                                              |            /-------+--------\    +-------------------+
*                                              |            |   a SUB with   |    |    Find SUB and   |
*                                              |            |large free space|----|   Convert to RUB  |
*                                              |            |    is idle ?   |Yes |see Notes #2a & #2b|
*                                              |            \-------+--------/    +-------------------+
*                                              |                    |No
*                                              +--------------------+
*                                              |
*                      (E)                     |No                  (D)
*       +----------------------+     /---------+---------\     +-----------------+
*       | Find RUB w/ highest  |_____| SUB with few free |_____|    Find  SUB    |
*       |      merge prio      |No   |  sec or no RUB?   |Yes  |    Merge SUB    |
*       | Merge RUB, Create UB |     |   see Note #2c    |     |    Create UB    |
*       +----------------------+     \-------------------/     +-----------------+
*
*               (2) Various thresholds must be set. These thresholds can however be set according to
*                   application specifics to obtain best performance. Indications are mentioned for every
*                   threshold (in configuration file fs_dev_nand_cfg.h). These indications are guidelines
*                   and specific cases could lead to different behaviors than what is expected.
*
*                   (a) FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD (see C)
*                       This threshold indicates the minimum idle time (specified as the number of
*                       driver accesses since the last access that has written to the SUB) for a
*                       sequential update blocks (SUB) to be converted to a random update block (RUB).
*                       This threshold must be set so that 'hot' SUBs are not converted to RUBs.
*
*                       This threshold can be set (in driver write operations) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD
*
*                   (b) ThSecRemCnt_ConvertSUBToRUB (see C).
*                       This threshold indicates the minimum size (in sectors) of free space needed in a
*                       sequential update block (SUB) to convert it to a random update block (RUB). RUBs
*                       have more flexible write rules, at the expense of a longer merge time. If the
*                       SUB is near full (few free sectors remaining), the SUB will me merged and a new
*                       RUB will be started, instead of performing the conversion from SUB to RUB.
*
*                       This threshold can be set as a percentage  (relative to number of sectors per
*                       block) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB
*
*                           Set higher than default -> Better overall write speed
*                           Set lower  than default -> Better overall wear leveling
*
*                   (c) ThSecRemCnt_MergeSUB (see D)
*                       This threshold indicates the maximum size (in sectors) of free space needed in a
*                       sequential update block (SUB) to merge it to allocate another update block. If
*                       the threshold is exceeded, a random update block (RUB) will be merged instead.
*                       This threshold must be set so that SUBs with significant free space are not
*                       merged. Merging SUBs early will generate additional wear.
*
*                       This threshold can be set as a percentage  (relative to number of sectors per
*                       block) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_PCT_MERGE_SUB
*
*               (3) When a random update block (RUB) must be merged, we choose the RUB with the highest
*                   merge priority. Merge priority is calculated based on 2 criterias :
*
*                   (a) How full is the block. This is determinated by the next sector index (which
*                                              points to the first writable sector of the block).
*                   (b) How idle is the block. This is determinated by the number of write cycles during
*                                              which this update block was not used. To make it relative,
*                                              it is divided by the total number of update blocks.
*
*                   If the block is completely full, it will be granted the highest priority. If it is
*                   not completely full, the priority will be calculated with this formula :
*
*                           prio = next sector index + idle factor
*
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_UB_QTY  FS_NAND_UB_Alloc (FS_NAND_DATA  *p_nand_data,
                                                  CPU_BOOLEAN    sequential,
                                                  FS_ERR        *p_err)
{
    FS_NAND_UB_QTY           ix_sub_full;
    FS_NAND_UB_QTY           ix_sub_idlest;
    FS_NAND_UB_QTY           ix_sub_fullest;
    FS_NAND_UB_QTY           ix_rub_priority;
    FS_NAND_UB_QTY           ix_blk_lowest_assoc;
    FS_NAND_UB_QTY           ix_blk_erased;
    FS_NAND_UB_QTY           ub_ix;
    FS_NAND_UB_EXTRA_DATA    ub_extra_data;
    CPU_BOOLEAN              early_exit;
    FS_NAND_SEC_PER_BLK_QTY  min_free_sec_cnt;
    CPU_INT16U               max_idle_val;
    CPU_INT16U               idle_val;
    CPU_INT08U               min_assoc;
    FS_NAND_SEC_PER_BLK_QTY  min_assoc_free_sec_cnt;
    FS_NAND_SEC_PER_BLK_QTY  free_sec_cnt;
    CPU_INT32U               rub_merge_prio;
    CPU_INT32U               max_rub_merge_prio;


                                                                /* -------------------- INIT VARs --------------------- */
                                                                /* Init max_idle_val to the threshold (see Note #2).    */
    max_idle_val           = FS_NAND_CFG_TH_SUB_MIN_IDLE_TO_FOLD;
    min_free_sec_cnt       = FS_NAND_SEC_OFFSET_IX_INVALID;
    ix_sub_full            = FS_NAND_UB_IX_INVALID;
    ix_blk_lowest_assoc    = FS_NAND_UB_IX_INVALID;
    ix_sub_idlest          = FS_NAND_UB_IX_INVALID;
    ix_sub_fullest         = FS_NAND_UB_IX_INVALID;
    ub_ix                  = FS_NAND_UB_IX_INVALID;
    ix_blk_erased          = FS_NAND_UB_IX_INVALID;
    ix_rub_priority        = FS_NAND_UB_IX_INVALID;
    min_assoc              = p_nand_data->RUB_MaxAssoc;
    min_assoc_free_sec_cnt = FS_NAND_SEC_OFFSET_IX_INVALID;
    max_rub_merge_prio     = 0u;

                                                                /* ------------------- SCAN ALL UBs ------------------- */
    ub_ix      = 0u;
    early_exit = DEF_NO;
    while ((ub_ix       < p_nand_data->UB_CntMax) &&
           (early_exit == DEF_NO)) {
        ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

        if (ub_extra_data.NextSecIx != 0u) {                    /* Used blks.                                           */

                                                                /* Determine idle cnt.                                  */
            if (ub_extra_data.ActivityCtr > p_nand_data->ActivityCtr) {
                                                                /* Wrapped around.                                      */
                idle_val  = DEF_GET_U_MAX_VAL(idle_val) - ub_extra_data.ActivityCtr;
                idle_val += p_nand_data->ActivityCtr;
            } else {
                                                                /* No wrap around.                                      */
                idle_val = p_nand_data->ActivityCtr - ub_extra_data.ActivityCtr;
            }

            if (ub_extra_data.AssocLvl == 0u) {                 /* Blk is a SUB.                                        */

                                                                /* ----------------- SUB: CHK IF FULL ----------------- */
                if (ub_extra_data.NextSecIx == p_nand_data->NbrSecPerBlk) {
                    ix_sub_full = ub_ix;
                    early_exit  = DEF_YES;                      /* No need to continue scanning blks.                   */
                }

                                                                /* ---------------- SUB: FREE SEC CNT ----------------- */
                free_sec_cnt = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;
                if (free_sec_cnt < min_free_sec_cnt) {
                    min_free_sec_cnt = free_sec_cnt;
                    ix_sub_fullest   = ub_ix;
                }

                                                                /* ------------------ SUB: IDLE CNT ------------------- */

                                                                /* Consider only spacious blks.                         */
                if (free_sec_cnt < p_nand_data->ThSecRemCnt_ConvertSUBToRUB) {
                    if (idle_val > max_idle_val) {
                                                                /* New idlest SUB.                                      */
                        max_idle_val  = idle_val;
                        ix_sub_idlest = ub_ix;
                    }
                }

            } else {                                            /* Blk is a RUB.                                        */

                free_sec_cnt = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;

                                                                /* -------------------- RUB: ASSOC -------------------- */
                if (free_sec_cnt != 0u) {
                    if ((ub_extra_data.AssocLvl < min_assoc)) {
                        min_assoc_free_sec_cnt = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;
                        ix_blk_lowest_assoc    = ub_ix;
                    } else {
                        if ((ub_extra_data.AssocLvl == min_assoc) &&
                            (ub_extra_data.AssocLvl  < p_nand_data->RUB_MaxAssoc)) {
                                                                /* Chk if more sec are free in this UB.                 */
                            if (free_sec_cnt > min_assoc_free_sec_cnt) {
                                min_assoc_free_sec_cnt = free_sec_cnt;
                                ix_blk_lowest_assoc    = ub_ix;
                            }
                        }
                    }
                }

                                                                /* ----------------- RUB: MERGE PRIO ------------------ */
                                                                /* If RUB full ...                                      */
                if (ub_extra_data.NextSecIx - 1 >= p_nand_data->NbrSecPerBlk) {
                                                                /* ... set to highest merge prio ...                    */
                    rub_merge_prio = DEF_GET_U_MAX_VAL(rub_merge_prio);
                } else {
                                                                /* ... else, calc merge prio (see Note #3).             */
                    rub_merge_prio  = idle_val / p_nand_data->UB_CntMax;
                    rub_merge_prio += ub_extra_data.NextSecIx;
                }

                if (rub_merge_prio > max_rub_merge_prio) {      /* Get ix of RUB with highest prio.                     */
                    max_rub_merge_prio = rub_merge_prio;
                    ix_rub_priority    = ub_ix;
                }

            }
        } else {
            if ((sequential           == DEF_NO) ||             /* Unused blk, unless SUB over max.                     */
                (p_nand_data->SUB_Cnt  < p_nand_data->SUB_CntMax)) {
                ix_blk_erased = ub_ix;
                early_exit    = DEF_YES;
            }
        }

        ub_ix++;
     }



                                                                /* ---------------- FULL SUB AVAIL (A) ---------------- */
    if (ix_sub_full != FS_NAND_UB_IX_INVALID) {
                                                                /* Convert SUB to DB.                                   */
        FS_NAND_SUB_Merge(p_nand_data,
                          ix_sub_full,
                          p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error merging sequential update blk %u.\r\n",
                                ix_sub_full));
            return (FS_NAND_UB_IX_INVALID);
        }

                                                                /* Create new UB.                                       */
        FS_NAND_UB_Create(p_nand_data,
                          ix_sub_full,
                          p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error creating random update block.\r\n"));
            return (FS_NAND_UB_IX_INVALID);
        }

        return (ix_sub_full);
    }

                                                                /* ------------------ EMPTY SLOT (F) ------------------ */
    if (ix_blk_erased != FS_NAND_UB_IX_INVALID) {
        if ((sequential == DEF_NO) ||                           /* If alloc'ing RUB or (SUB with free SUB slots) ...    */
            (p_nand_data->SUB_Cnt < p_nand_data->SUB_CntMax)) {
                                                                /* ... create new UB.                                   */
            FS_NAND_UB_Create(p_nand_data,
                              ix_blk_erased,
                              p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error creating random update block.\r\n"));
                return (FS_NAND_UB_IX_INVALID);
            }

            return (ix_blk_erased);
        }
    }

    if (sequential == DEF_NO) {
                                                                /* -------------- RUB WITH k<K AVAIL (B) -------------- */
        if (ix_blk_lowest_assoc != FS_NAND_UB_IX_INVALID) {
            return (ix_blk_lowest_assoc);
        }

                                                                /* --------- IDLE AND SPACIOUS SUB AVAIL (C) ---------- */
        if (ix_sub_idlest != FS_NAND_UB_IX_INVALID) {
            ub_extra_data = p_nand_data->UB_ExtraDataTbl[ix_sub_idlest];

            FS_NAND_UB_IncAssoc(p_nand_data,                    /* Convert SUB to RUB with k=1.                         */
                                ix_sub_idlest,
                                ub_extra_data.AssocLogicalBlksTbl[0]);

            p_nand_data->SUB_Cnt--;

            return (ix_sub_idlest);
        }
    }

                                                                /* ---- SUB WITH FEW FREE SECS AVAIL OR NO RUB (D) ---- */
    if ((min_free_sec_cnt < p_nand_data->ThSecRemCnt_MergeSUB) ||
        (ix_rub_priority == FS_NAND_UB_IX_INVALID)) {
                                                                /* Merge SUB.                                           */
        FS_NAND_SUB_Merge(p_nand_data,
                          ix_sub_fullest,
                          p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error during sequential block %u merge.\r\n",
                                ix_sub_fullest));
            return (FS_NAND_UB_IX_INVALID);
        }

                                                                /* Create new UB.                                       */
        FS_NAND_UB_Create(p_nand_data,
                          ix_sub_fullest,
                          p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error creating update block %u.\r\n",
                                ix_sub_fullest));
            return (FS_NAND_UB_IX_INVALID);
        }

        return (ix_sub_fullest);
    }

                                                                /* ------- NO SUB WITH FEW FREE SECS AVAIL (E) -------- */

                                                                /* Merge RUB.                                           */
    FS_NAND_RUB_Merge(p_nand_data,
                      ix_rub_priority,
                      p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error during update block %u merge.\r\n",
                            ix_rub_priority));
        return (FS_NAND_UB_IX_INVALID);
    }

                                                                /* Create new UB.                                       */
    FS_NAND_UB_Create(p_nand_data,
                      ix_rub_priority,
                      p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_UB_Alloc(): Error creating new update block.\r\n"));
        return (FS_NAND_UB_IX_INVALID);
    }

    return (ix_rub_priority);
}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_RUB_Alloc()
*
* Description : Allocates a random update block and associates it with specified logical block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_logical  Index of logical block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   -----RETURNED BY FS_NAND_UB_Alloc()------
*                                   See FS_NAND_UB_Alloc() for additional return error codes.
*
* Return(s)   : Index of allocated update block,    if operation succeeded,
*               FS_NAND_BLK_IX_INVALID,             otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_UB_QTY  FS_NAND_RUB_Alloc (FS_NAND_DATA     *p_nand_data,
                                                   FS_NAND_BLK_QTY   blk_ix_logical,
                                                   FS_ERR           *p_err)
{
    FS_NAND_UB_QTY  ub_ix;


                                                                /* Alloc new UB.                                        */
    ub_ix = FS_NAND_UB_Alloc(p_nand_data,
                             DEF_NO,
                             p_err);

    if (*p_err != FS_ERR_NONE) {
        return (FS_NAND_UB_IX_INVALID);
    }

                                                                /* Associate UB with logical blk.                       */
    FS_NAND_UB_IncAssoc(p_nand_data,
                        ub_ix,
                        blk_ix_logical);

    return (ub_ix);
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_RUB_Merge()
*
* Description : Perform a full merge of specified random update block with associated data block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               ub_ix           Index of logical block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Operation was successful.
*
*                                   -RETURNED BY FS_NAND_RUB_PartialMerge()--
*                                   See FS_NAND_RUB_PartialMerge() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_RUB_Merge (FS_NAND_DATA    *p_nand_data,
                                         FS_NAND_UB_QTY   ub_ix,
                                         FS_ERR          *p_err)
{
    FS_NAND_UB_EXTRA_DATA  ub_extra_data;
    FS_NAND_BLK_QTY        blk_ix_logical;
    CPU_INT08U             assoc_ix;


    FS_CTR_STAT_INC(p_nand_data->Ctrs.StatRUB_MergeCtr);

    FS_NAND_TRACE_LOG(("FS_NAND_RUB_Merge(): Full merge of RUB %u.\r\n",
                        ub_ix));


    ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

                                                                /* ------------------ PERFORM MERGE ------------------- */

                                                                /* Merge 1 blk at a time until full merge is complete.  */
    for (assoc_ix = 0u; assoc_ix < p_nand_data->RUB_MaxAssoc; assoc_ix++) {

        blk_ix_logical = ub_extra_data.AssocLogicalBlksTbl[assoc_ix];

        if (blk_ix_logical != FS_NAND_BLK_IX_INVALID) {
            FS_NAND_RUB_PartialMerge(p_nand_data,
                                     ub_ix,
                                     ub_extra_data.AssocLogicalBlksTbl[assoc_ix],
                                     p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_RUB_Merge(): Error during partial merge of associated block %u.\r\n",
                                    assoc_ix));
                return;
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                    FS_NAND_RUB_PartialMerge()
*
* Description : Perform a merge of the specified data block associated with the random update block (RUB).
*               See Note #1.
*
* Argument(s) : p_nand_data             Pointer to NAND data.
*               -----------             Argument validated by caller.
*
*               ub_ix                   Index of update block.
*                                       Argument validated by caller (see Note #4).
*
*               data_blk_ix_logical     Index of associated logical block in associated block table.
*
*               p_err                   Pointer to variable that will receive the return error code from this function :
*               -----                   Argument validated by caller.
*
*                                           FS_ERR_NONE                     Operation was successful.
*
*                                           ---------RETURNED BY FS_NAND_SecRdPhyNoRefresh()---------
*                                           See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
*                                           -------------RETURNED BY FS_NAND_SecIsUsed()-------------
*                                           See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                           -----------RETURNED BY FS_NAND_SecWrHandler()------------
*                                           FS_ERR_DEV_INVALID_METADATA         Metadata block could not be found.
*                                           FS_ERR_DEV_INVALID_OP               Bad blocks table is full.
*                                           FS_ERR_DEV_OP_ABORTED               Operation failed, but bad block was refreshed succesfully.
*                                           FS_ERR_ECC_UNCORR                   Uncorrectable ECC error.
*
*                                           ------------RETURNED BY FS_NAND_UB_SecFind()-------------
*                                           See FS_NAND_UB_SecFind() for additional return error codes.
*
*                                           -----------RETURNED BY FS_NAND_BlkGetErased()------------
*                                           See FS_NAND_BlkGetErased() for additional return error codes.
*
*                                           -------------RETURNED BY FS_NAND_OOSGenSto()-------------
*                                           See FS_NAND_OOSGenSto() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) A random update block (RUB) is often associated with multiple logical data blocks.
*                   A partial merge consists to only merge the sectors belonging to one data block with
*                   the data block itself. This will also decrease the associativity level of the RUB.
*
*               (2) The new data block (data_blk_ix_phy_new) will be created from the merged content of
*                   2 blocks :
*                                   - the old data block (data_blk_ix_phy_old);
*                                   - the update   block (ub_ix).
*
*                   This function will evaluate, for each sector, which of the 2 blocks contains the
*                   latest valid version of the sector.
*
*               (3) To revert the operation, this function assumes that 'data_blk_ix_phy_old' has not
*                   been changed since its first and only assignation.
*
*               (4) Caller must validate 'ub_ix' to make sure it is a random update block (RUB),
*                   and not a sequential update block (SUB).
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_RUB_PartialMerge (FS_NAND_DATA           *p_nand_data,
                                                FS_NAND_UB_QTY          ub_ix,
                                                FS_NAND_BLK_QTY         data_blk_ix_logical,
                                                FS_ERR                 *p_err)
{
    FS_NAND_UB_DATA           ub_data;
    FS_NAND_UB_EXTRA_DATA    *p_ub_extra_data;
    FS_NAND_UB_SEC_DATA       ub_sec_data;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical_oos;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy;
    FS_NAND_BLK_QTY           data_blk_ix_phy_old;
    FS_NAND_BLK_QTY           data_blk_ix_phy_new;
    FS_NAND_BLK_QTY           blk_ix_logical_src;
    FS_NAND_BLK_QTY           blk_ix_phy_src;
    FS_NAND_ASSOC_BLK_QTY     assoc_blk_ix;
    FS_SEC_QTY                sec_ix_phy;
    CPU_BOOLEAN               is_sec_used;
    CPU_BOOLEAN               sec_found;
    CPU_INT08U               *p_oos_buf;


    FS_CTR_STAT_INC(p_nand_data->Ctrs.StatRUB_PartialMergeCtr);

    p_oos_buf           = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    ub_data             =  FS_NAND_UB_TblEntryRd(p_nand_data, ub_ix);
    p_ub_extra_data     = &p_nand_data->UB_ExtraDataTbl[ub_ix];
    data_blk_ix_phy_old =  p_nand_data->LogicalToPhyBlkMap[data_blk_ix_logical];
    assoc_blk_ix        =  FS_NAND_RUB_AssocBlkIxGet(p_nand_data, ub_ix, data_blk_ix_logical);

    FS_NAND_TRACE_LOG(("FS_NAND_RUB_PartialMerge(): Partial merge of assoc blk %u from RUB %u.\r\n",
                        assoc_blk_ix,
                        ub_ix));

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_ub_extra_data->AssocLvl == 0u) {
        FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Fatal error: associativity level is 0, suggesting a SUB.\r\n"));
        CPU_SW_EXCEPTION(;);
    }
#endif

                                                                /* ------------- GET BLK FOR NEW DATA BLK ------------- */
    data_blk_ix_phy_new = FS_NAND_BlkGetErased(p_nand_data,
                                               p_err);

    FS_NAND_TRACE_LOG(("FS_NAND_RUB_PartialMerge(): Logical blk %u will be merged to data blk %u.\r\n",
                        data_blk_ix_logical,
                        data_blk_ix_phy_new));

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Unable to get an erased block for new data block.\r\n"));
        return;
    }

                                                                /* --------- COPY ALL VALID SEC SEQUENTIALLY ---------- */
    ub_sec_data.UB_Ix                 = ub_ix;
    ub_sec_data.AssocLogicalBlksTblIx = assoc_blk_ix;
    for (sec_offset_logical = 0u; sec_offset_logical < p_nand_data->NbrSecPerBlk; sec_offset_logical++) {
        sec_found          = DEF_NO;
        sec_offset_phy     = FS_NAND_SEC_OFFSET_IX_INVALID;
        blk_ix_logical_src = FS_NAND_BLK_IX_INVALID;

                                                                /* Locate sec in UB.                                    */
        ub_sec_data = FS_NAND_UB_SecFind(p_nand_data,
                                         ub_sec_data,
                                         sec_offset_logical,
                                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Error looking for updated sector %u of logical block %u.\r\n",
                                sec_offset_logical,
                                data_blk_ix_logical));
            return;
        }

                                                                /* Chk if sec was found in UB (see Note #2).            */
        if (ub_sec_data.SecOffsetPhy != FS_NAND_SEC_OFFSET_IX_INVALID) {
                                                                /* Set src blk to UB.                                   */
            blk_ix_logical_src = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix);
            sec_offset_phy     = ub_sec_data.SecOffsetPhy;
            sec_found          = DEF_YES;

                                                                /* Remove from valid sec map.                           */
            FSUtil_MapBitClr(ub_data.SecValidBitMap, sec_offset_phy);
        } else {
            if (data_blk_ix_phy_old != FS_NAND_BLK_IX_INVALID) {
                                                                /* Chk is sec is wr'en in data blk (see Note #2).       */
                sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, data_blk_ix_phy_old);
                sec_ix_phy += sec_offset_logical;

                is_sec_used = FS_NAND_SecIsUsed(p_nand_data, sec_ix_phy, p_err);
                if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Error determining if data block sector %u is used.\r\n",
                                        sec_ix_phy));
                    return;
                }

                if (is_sec_used == DEF_YES) {
                    blk_ix_logical_src = data_blk_ix_logical;   /* Set src blk to data blk.                             */
                    sec_offset_phy     = sec_offset_logical;    /* Offset logical equal to offset phy for a data blk.   */
                    sec_found          = DEF_YES;
                }
            }
        }

        do {                                                    /* Until sec has been successfully wr'en ...            */
            *p_err = FS_ERR_NONE;

                                                                /* If sec can be located, wr it in new data blk.        */
            if (sec_found == DEF_YES) {
                FS_NAND_TRACE_LOG(("%u:%u->%u.\r\n",
                                    blk_ix_logical_src,
                                    sec_offset_phy,
                                    sec_offset_logical));

                                                                /* Rd sec from src blk.                                 */
                blk_ix_phy_src = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_logical_src);
                FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                          p_nand_data->BufPtr,
                                          p_nand_data->OOS_BufPtr,
                                          blk_ix_phy_src,
                                          sec_offset_phy,
                                          p_err);

                if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Error reading updated sector %u for logical block %u.\r\n",
                                        sec_offset_logical,
                                        data_blk_ix_logical));
                }

            }

            if (sec_offset_logical == 0u) {                     /* First sec, so recreate OOS (for erase cnt).          */

                if (sec_found == DEF_YES) {                     /* Prev wr'en sec; preserve blk sec ix.                 */
                    MEM_VAL_COPY_GET_INTU_LITTLE(&sec_offset_logical_oos,
                                                 &p_oos_buf[FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET],
                                                  sizeof(FS_NAND_SEC_PER_BLK_QTY));
                } else {                                        /* Dummy sec.                                           */
                    sec_offset_logical_oos = FS_NAND_SEC_OFFSET_IX_INVALID;
                }

                FS_NAND_OOSGenSto(p_nand_data,
                                  p_nand_data->OOS_BufPtr,
                                  data_blk_ix_logical,
                                  data_blk_ix_phy_new,
                                  sec_offset_logical_oos,
                                  sec_offset_logical,
                                  p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }
                sec_found = DEF_YES;                            /* Even if sec not wr'en, create dummy sec.             */
            }

            if (sec_found == DEF_YES) {
                                                                /* Manipulate LogicalToPhyBlkMap to wr data to new  ... */
                                                                /* ... data blk before it is added to the tbl.          */
                p_nand_data->LogicalToPhyBlkMap[data_blk_ix_logical] = data_blk_ix_phy_new;

                FS_NAND_SecWrHandler(p_nand_data,               /* Wr sec in new data blk.                              */
                                     p_nand_data->BufPtr,
                                     p_nand_data->OOS_BufPtr,
                                     data_blk_ix_logical,
                                     sec_offset_logical,
                                     p_err);
                                                                /* Revert change in LogicalToPhyBlkMap (see Note #3).   */
                p_nand_data->LogicalToPhyBlkMap[data_blk_ix_logical] = data_blk_ix_phy_old;
            }

        } while (*p_err == FS_ERR_DEV_OP_ABORTED);

        if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_RUB_PartialMerge(): Error writing sec %u to new data block.\r\n",
                                    sec_offset_logical));
                return;
        }

    }

                                                                /* ------------------ UPDATE UB DATA ------------------ */
    p_ub_extra_data->AssocLogicalBlksTbl[assoc_blk_ix] = FS_NAND_BLK_IX_INVALID;
    p_ub_extra_data->AssocLvl--;

    if (p_ub_extra_data->AssocLvl == 0u) {                      /* If UB becomes empty, ...                             */
                                                                /* ... mark UB dirty.                                   */
        FS_NAND_BlkMarkDirty(p_nand_data, ub_data.BlkIxPhy);

        FS_NAND_UB_Clr(p_nand_data, ub_ix);                     /* Clr UB data.                                         */

    }

                                                                /* ----------------- UPDATE METADATA ------------------ */
    if (data_blk_ix_phy_old != FS_NAND_BLK_IX_INVALID) {
        FS_NAND_BlkMarkDirty(p_nand_data, data_blk_ix_phy_old); /* Mark old data blk dirty.                             */
    }

                                                                /* Update logical to phy blk tbl.                       */
    p_nand_data->LogicalToPhyBlkMap[data_blk_ix_logical] = data_blk_ix_phy_new;

    FS_NAND_TRACE_LOG(("FS_NAND_RUB_PartialMerge(): Partial merge of assoc blk %u from RUB %u done.\r\n",
                        assoc_blk_ix,
                        ub_ix));

}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_RUB_AssocBlkIxGet()
*
* Description : Get associated block index for specified logical block index in specified update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               ub_ix           Index of update block.
*
*               blk_ix_logical  Index of logical block.
*
* Return(s)   : Index of allocated block    , if found;
*               FS_NAND_ASSOC_BLK_IX_INVALID, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_ASSOC_BLK_QTY  FS_NAND_RUB_AssocBlkIxGet(FS_NAND_DATA     *p_nand_data,
                                                                 FS_NAND_UB_QTY    ub_ix,
                                                                 FS_NAND_BLK_QTY   blk_ix_logical)
{
    FS_NAND_ASSOC_BLK_QTY  assoc_blk_ix;
    FS_NAND_UB_EXTRA_DATA  ub_extra_data;


    assoc_blk_ix = 0u;
    ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];
    while (assoc_blk_ix < p_nand_data->RUB_MaxAssoc) {
        if (ub_extra_data.AssocLogicalBlksTbl[assoc_blk_ix] == blk_ix_logical) {
            return (assoc_blk_ix);
        }

        assoc_blk_ix++;
    }

                                                                /* Logical blk not associated with UB.                  */
    FS_NAND_TRACE_DBG(("FS_NAND_RUB_AssocBlkIxGet(): Logical block index %u is not associated with update block %u.\r\n",
                        blk_ix_logical,
                        ub_ix));
    return (FS_NAND_ASSOC_BLK_IX_INVALID);

}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_SUB_Alloc()
*
* Description : Allocates a sequential update block and associate it with specified logical block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_logical  Index of logical block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   -RETURNED BY FS_NAND_UB_Alloc()-
*                                   See FS_NAND_UB_Alloc() for additional return error codes.
*
* Return(s)   : Index of allocated update block,    if operation succeeded,
*               FS_NAND_BLK_IX_INVALID,             otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_NAND_UB_QTY  FS_NAND_SUB_Alloc (FS_NAND_DATA     *p_nand_data,
                                                   FS_NAND_BLK_QTY   blk_ix_logical,
                                                   FS_ERR           *p_err)
{
    FS_NAND_UB_QTY          ub_ix;
    FS_NAND_UB_EXTRA_DATA  *p_extra_data;


                                                                /* Alloc new UB.                                        */
    ub_ix = FS_NAND_UB_Alloc(p_nand_data,
                             DEF_YES,
                             p_err);

    if (*p_err != FS_ERR_NONE) {
        return (FS_NAND_UB_IX_INVALID);
    }

                                                                /* Associate UB with logical blk.                       */
    FS_NAND_UB_IncAssoc(p_nand_data,
                        ub_ix,
                        blk_ix_logical);

                                                                /* Correct assoc for SUB.                               */
    p_extra_data = &p_nand_data->UB_ExtraDataTbl[ub_ix];
    p_extra_data->AssocLvl = 0u;

    p_nand_data->SUB_Cnt++;                                     /* Update SUB cnt.                                      */


    return (ub_ix);
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_SUB_Merge()
*
* Description : Merge specified sequential update block with associated data block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               sub_ix          Index of logical block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*
*                                   -RETURNED BY FS_NAND_SecWrHandler()-
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*                                   FS_ERR_DEV_OP_ABORTED           Operation failed, but bad block was refreshed succesfully.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   -RETURNED BY FS_NAND_SUB_MergeUntil()-
*                                   See FS_NAND_SUB_MergeUntil() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_SUB_Merge (FS_NAND_DATA    *p_nand_data,
                                         FS_NAND_UB_QTY   sub_ix,
                                         FS_ERR          *p_err)
{
    FS_NAND_UB_EXTRA_DATA    ub_extra_data;
    FS_NAND_UB_DATA          ub_data;
    FS_NAND_BLK_QTY          data_blk_ix_phy;
    FS_NAND_BLK_QTY          data_blk_ix_logical;


    FS_CTR_STAT_INC(p_nand_data->Ctrs.StatSUB_MergeCtr);

    FS_NAND_TRACE_LOG(("FS_NAND_SUB_Merge(): Merge SUB %u.\r\n",
                        sub_ix));


    ub_data             = FS_NAND_UB_TblEntryRd(p_nand_data, sub_ix);
    ub_extra_data       = p_nand_data->UB_ExtraDataTbl[sub_ix];
                                                                /* Find associated logical blk.                         */
    data_blk_ix_logical = ub_extra_data.AssocLogicalBlksTbl[0];

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ub_extra_data.AssocLvl != 0u) {                         /* Validate that assoc lvl is 0.                        */
        FS_NAND_TRACE_DBG(("FS_NAND_SUB_Merge(): Fatal error: associativity level is not 0.\r\n"));
        CPU_SW_EXCEPTION(;);
    }

    if (ub_extra_data.NextSecIx == 0u) {                        /* Validate that sec 0 is wr'en in SUB.                 */
        FS_NAND_TRACE_DBG(("FS_NAND_SUB_Merge(): Fatal error: sector 0 must be wr'en in SUB.\r\n"));
        CPU_SW_EXCEPTION(;);
    }
#endif

    data_blk_ix_phy      = FS_NAND_BlkIxPhyGet(p_nand_data, data_blk_ix_logical);
    if (data_blk_ix_phy != FS_NAND_BLK_IX_INVALID) {
        FS_NAND_SUB_MergeUntil(p_nand_data,                     /* Perform copy of data blk sec until end of blk.       */
                               sub_ix,
                               p_nand_data->NbrSecPerBlk - 1u,
                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
                                                                /* ----------------- UPDATE METADATA ------------------ */

                                                                /* Mark old data blk dirty if there was one.            */
        FS_NAND_BlkMarkDirty(p_nand_data, data_blk_ix_phy);
    }

                                                                /* Update logical to physical blk tbl.                  */
    p_nand_data->LogicalToPhyBlkMap[data_blk_ix_logical] = ub_data.BlkIxPhy;

                                                                /* Clear old SUB data structures.                       */
    FS_NAND_UB_Clr(p_nand_data,
                   sub_ix);

    p_nand_data->SUB_Cnt--;
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_SUB_MergeUntil()
*
* Description : Copy data block sectors corresponding to specified sequential update block from current
*               sequential update block sector to specified sector.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               sub_ix          Index of logical block.
*
*               sec_end         Offset of last sector to merge with sequential update block.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   ----------RETURNED BY FS_NAND_SecRdPhyNoRefresh()-----------
*                                   See FS_NAND_SecRdPhyNoRefresh() for additional return error codes.
*
*                                   --------------RETURNED BY FS_NAND_SecIsUsed()---------------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   -------------RETURNED BY FS_NAND_SecWrHandler()-------------
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*                                   FS_ERR_DEV_OP_ABORTED           Operation failed, but bad block was refreshed succesfully.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*                                   FS_ERR_NONE                     Sector written successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_SUB_MergeUntil (FS_NAND_DATA             *p_nand_data,
                                              FS_NAND_UB_QTY            sub_ix,
                                              FS_NAND_SEC_PER_BLK_QTY   sec_end,
                                              FS_ERR                   *p_err)
{
    FS_NAND_UB_EXTRA_DATA    ub_extra_data;
    FS_NAND_BLK_QTY          data_blk_ix_phy;
    FS_NAND_BLK_QTY          data_blk_ix_logical;
    FS_NAND_BLK_QTY          ub_ix_logical;
    FS_NAND_SEC_PER_BLK_QTY  sec_offset_phy;
    FS_SEC_QTY               sec_ix_phy_base;
    CPU_BOOLEAN              is_sec_used;

    ub_extra_data       = p_nand_data->UB_ExtraDataTbl[sub_ix];
                                                                /* Find associated logical blk.                         */
    data_blk_ix_logical = ub_extra_data.AssocLogicalBlksTbl[0];
    ub_ix_logical       = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, sub_ix);

    data_blk_ix_phy      = FS_NAND_BlkIxPhyGet(p_nand_data, data_blk_ix_logical);
    if (data_blk_ix_phy != FS_NAND_BLK_IX_INVALID) {
        sec_ix_phy_base     = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, data_blk_ix_phy);

        FS_NAND_TRACE_LOG(("FS_NAND_SUB_MergeUntil(): Copying data sectors of old data block %u (log %u) from offset %u to offset %u.\r\n",
                            data_blk_ix_phy,
                            data_blk_ix_logical,
                            ub_extra_data.NextSecIx,
                            sec_end));

                                                                /* --------------- COPY ALL MISSING SEC --------------- */
        while (ub_extra_data.NextSecIx <= sec_end) {

            sec_offset_phy = ub_extra_data.NextSecIx;

                                                                /* Chk if sec is used.                                  */
            is_sec_used = FS_NAND_SecIsUsed(p_nand_data, sec_ix_phy_base + sec_offset_phy, p_err);
            if (*p_err != FS_ERR_NONE) {
                is_sec_used = DEF_NO;                           /* If err, assume sec isn't used.                       */
            }
            if (is_sec_used == DEF_YES) {
                do {                                            /* Do until wr is not aborted.                          */
                    *p_err = FS_ERR_NONE;

                                                                /* Rd data sec.                                         */
                    FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                              p_nand_data->BufPtr,
                                              p_nand_data->OOS_BufPtr,
                                              data_blk_ix_phy,
                                              sec_offset_phy,
                                              p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SUB_MergeUntil(): Fatal error reading sector %u of logical block %u.\r\n",
                                            sec_offset_phy,
                                            data_blk_ix_logical));
                        return;
                    }



                                                                /* Wr sec in SUB.                                       */
                    FS_NAND_SecWrHandler(p_nand_data,
                                         p_nand_data->BufPtr,
                                         p_nand_data->OOS_BufPtr,
                                         ub_ix_logical,
                                         sec_offset_phy,
                                         p_err);
                } while (*p_err == FS_ERR_DEV_OP_ABORTED);

                if (*p_err != FS_ERR_NONE) {                    /* SecWrHandler err code chk.                           */
                    FS_NAND_TRACE_DBG(("FS_NAND_SUB_MergeUntil(): Fatal error writing sector %u of logical block %u.\r\n",
                                        sec_offset_phy,
                                        ub_ix_logical));
                    return;
                }
            }

            ub_extra_data.NextSecIx++;
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                    FS_NAND_AvailBlkTblInvalidate()
*
* Description : Invalidate available block table contents on device. It will be updated on next metadata
*               commit.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_AvailBlkTblInvalidate (FS_NAND_DATA  *p_nand_data)
{
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_start;
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_end;
    CPU_SIZE_T               avail_blk_tbl_end;


    sec_ix_start       = p_nand_data->MetaOffsetAvailBlkTbl / p_nand_data->SecSize;
    avail_blk_tbl_end  = sizeof(FS_NAND_BLK_QTY) + sizeof(FS_NAND_ERASE_QTY);
    avail_blk_tbl_end *= p_nand_data->AvailBlkTblEntryCntMax;
    avail_blk_tbl_end += p_nand_data->MetaOffsetAvailBlkTbl - 1u;
    sec_ix_end         = avail_blk_tbl_end / p_nand_data->SecSize;

    FS_NAND_MetaSecRangeInvalidate(p_nand_data, sec_ix_start, sec_ix_end);

    p_nand_data->AvailBlkTblInvalidated = DEF_YES;
}
#endif


/*
*********************************************************************************************************
*                                     FS_NAND_DirtyMapInvalidate()
*
* Description : Invalidate dirty block bitmap contents on device. It will be updated on next metadata commit.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_DirtyMapInvalidate (FS_NAND_DATA  *p_nand_data)
{
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_start;
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_end;
    CPU_SIZE_T               dirty_map_end;


    dirty_map_end  =  p_nand_data->BlkCnt / DEF_OCTET_NBR_BITS;
    dirty_map_end += (p_nand_data->BlkCnt % DEF_OCTET_NBR_BITS) == 0u ? 0u : 1u;
    dirty_map_end +=  p_nand_data->MetaOffsetDirtyBitmap - 1u;

    sec_ix_start   =  p_nand_data->MetaOffsetDirtyBitmap / p_nand_data->SecSize;
    sec_ix_end     =  dirty_map_end                      / p_nand_data->SecSize;

    FS_NAND_MetaSecRangeInvalidate(p_nand_data, sec_ix_start, sec_ix_end);
}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_UB_TblInvalidate()
*
* Description : Invalidate update block table contents on device. The invalid state means that the
*               UB (update block) table has been modified in volatile memory, but was not committed to
*               the device already. The valid state will be restored after the next metadata commit.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_UB_TblInvalidate (FS_NAND_DATA  *p_nand_data)
{
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_start;
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_end;
    CPU_SIZE_T               ub_tbl_end;


    ub_tbl_end  = sizeof(FS_NAND_BLK_QTY);
    ub_tbl_end += p_nand_data->NbrSecPerBlk / DEF_OCTET_NBR_BITS;
    ub_tbl_end += p_nand_data->NbrSecPerBlk % DEF_OCTET_NBR_BITS == 0u ? 0u : 1u;
    ub_tbl_end *= p_nand_data->UB_CntMax;
    ub_tbl_end += p_nand_data->MetaOffsetUB_Tbl - 1u;

    sec_ix_start = p_nand_data->MetaOffsetUB_Tbl / p_nand_data->SecSize;
    sec_ix_end   = ub_tbl_end                    / p_nand_data->SecSize;

    FS_NAND_MetaSecRangeInvalidate(p_nand_data, sec_ix_start, sec_ix_end);
}
#endif


/*
*********************************************************************************************************
*                                     FS_NAND_BadBlkTblInvalidate()
*
* Description : Invalidate update block table contents on device. It will be updated on next metadata commit.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BadBlkTblInvalidate (FS_NAND_DATA  *p_nand_data)
{
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_start;
    FS_NAND_SEC_PER_BLK_QTY  sec_ix_end;
    CPU_SIZE_T               bad_blk_tbl_end;


    sec_ix_start = p_nand_data->MetaOffsetBadBlkTbl / p_nand_data->SecSize;

    bad_blk_tbl_end  = p_nand_data->MetaOffsetBadBlkTbl;
    bad_blk_tbl_end += p_nand_data->PartDataPtr->MaxBadBlkCnt * sizeof(FS_NAND_BLK_QTY);
    sec_ix_end       = bad_blk_tbl_end / p_nand_data->SecSize;


    FS_NAND_MetaSecRangeInvalidate(p_nand_data, sec_ix_start, sec_ix_end);
}
#endif


/*
*********************************************************************************************************
*                                       FS_NAND_SecTypeParse()
*
* Description : Parse sector type from out of sector (OOS) data.
*
* Argument(s) : p_oos_buf           Pointer to out of sector (OOS) data buffer. Must be populated by caller.
*               ---------           Argument validated by caller.
*
* Return(s)   : Sector type.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_SEC_TYPE  FS_NAND_SecTypeParse (CPU_INT08U *p_oos_buf)
{
    FS_NAND_SEC_TYPE_STO  sec_type_sto;
    FS_NAND_SEC_TYPE      sec_type;


    MEM_VAL_COPY_GET_INTU_LITTLE(&sec_type_sto,
                                 &p_oos_buf[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                  sizeof(FS_NAND_SEC_TYPE_STO));

    switch (sec_type_sto) {
        case FS_NAND_SEC_TYPE_UNUSED:
             sec_type = FS_NAND_SEC_TYPE_UNUSED;
             break;

        case FS_NAND_SEC_TYPE_STORAGE:
             sec_type = FS_NAND_SEC_TYPE_STORAGE;
             break;

        case FS_NAND_SEC_TYPE_METADATA:
             sec_type = FS_NAND_SEC_TYPE_METADATA;
             break;

        case FS_NAND_SEC_TYPE_HDR:
             sec_type = FS_NAND_SEC_TYPE_HDR;
             break;

        case FS_NAND_SEC_TYPE_DUMMY:
             sec_type = FS_NAND_SEC_TYPE_DUMMY;
             break;

        case FS_NAND_SEC_TYPE_INVALID:
        default:
             sec_type = FS_NAND_SEC_TYPE_INVALID;
             break;
    }

    return (sec_type);
}


/*
*********************************************************************************************************
*                                      FS_NAND_ChipEraseHandler()
*
* Description : Erase entire NAND chip. Erase counts for each blocks will also be erased, affecting
*               wear leveling mechanism.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Chip successfully erased
*
*                                   -RETURNED BY FS_NAND_BlkEraseHandler()--
*                                   See FS_NAND_BlkEraseHandler() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Warning: this function will reset erase counts, affecting wear leveling algorithms.
*                   Use this function only in very specific cases when a low-level format does not
*                   suffice.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_ChipEraseHandler (FS_NAND_DATA  *p_nand_data,
                                                FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY     blk_ix;
    FS_ERR              err_rtn;


    err_rtn = FS_ERR_NONE;
    for (blk_ix = p_nand_data->BlkIxFirst;
         blk_ix < p_nand_data->BlkCnt + p_nand_data->BlkIxFirst;
         blk_ix++) {
       *p_err = FS_ERR_NONE;
        FS_NAND_BlkEraseHandler(p_nand_data,
                                blk_ix,
                                p_err);
        if (*p_err != FS_ERR_NONE) {
            err_rtn = *p_err;
        }
    }

   *p_err = err_rtn;
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_LowFmtHandler()
*
* Description : Low-level format NAND.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_FMT  Device formatted, but could NOT be mounted.
*                                   FS_ERR_DEV_INVALID_OP       Device invalid operation.
*                                   FS_ERR_DEV_IO               Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT          Device timeout.
*                                   FS_ERR_NONE                 Device formatted/mounted.
*
*                                   -----------------RETURNED BY FS_NAND_AvailBlkTblFill()-----------------
*                                   See FS_NAND_AvailBlkTblFill() for additional return error codes.
*
*                                   -----------------RETURNED BY FS_NAND_LowMountHandler()-----------------
*                                   See FS_NAND_LowMountHandler() for additional return error codes.
*
*                                   -----------------RETURNED BY FS_NAND_BlkEraseHandler()-----------------
*                                   See FS_NAND_BlkEraseHandler() for additional return error codes.
*
*                                   --------------RETURNED BY p_nand_data->CtrlrPtr->SecRd()---------------
*                                   See p_nand_data->CtrlrPtr->SecRd() for additional return error codes.
*
*                                   ---------------RETURNED BY FS_NAND_BlkGetAvailFromTbl()----------------
*                                   See FS_NAND_BlkGetAvailFromTbl() for additional return error codes.
*
*                                   -------------------RETURNED BY FS_NAND_MetaBlkFind()-------------------
*                                   See FS_NAND_MetaBlkFind() for additional return error codes.
*
*                                   -------------------RETURNED BY FS_NAND_BlkMarkBad()--------------------
*                                   See FS_NAND_BlkMarkBad() for additional return error codes.
*
*                                   -------------------RETURNED BY FS_NAND_MetaCommit()--------------------
*                                   See FS_NAND_MetaCommit() for additional return error codes.
*
*                                   ----------------RETURNED BY FS_NAND_LowUnmountHandler()----------------
*                                   See FS_NAND_LowUnmountHandler() for additional return error codes.
*
*                                   -----------------RETURNED BY FS_NAND_BlkEnsureErased()-----------------
*                                   See FS_NAND_BlkEnsureErased() for additional return error codes.
*
*                                   ----------------------RETURNED BY FS_NAND_HdrWr()----------------------
*                                   See FS_NAND_HdrWr() for additional return error codes.
*
*                                   ----------------------RETURNED BY FS_NAND_HdrRd()----------------------
*                                   See FS_NAND_HdrRd() for additional return error codes.
*
*                                   ---------------RETURNED BY FS_NAND_BlkIsFactoryDefect()----------------
*                                   See FS_NAND_BlkIsFactoryDefect() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device header is always on first good block.
*
*               (2) The NAND device is marked unformatted when formatting commences; it will not be marked
*                   formatted until the low-level mount completes.
*
*               (3) If low level parameters change, the device must be reformatted. Low level format
*                   conversion is not yet implemented.
*
*               (4) If an existing metadata block exists on the device, the initial ID number is
*                   changed in order to make sure FS_NAND_MetaBlkFind() will choose the newly created
*                   metadata block.
*
*               (5) Device should never be low-level formatted by more than one filesystem/driver.
*                   As soon as a block is erased, the erase count and the bad block mark will be lost.
*                   This vital information might affect the performance of the driver. However, when
*                   low-level formatting a device with this driver, care will be taken to keep the erase
*                   counts and the bad block marks.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_LowFmtHandler (FS_NAND_DATA  *p_nand_data,
                                             FS_ERR        *p_err)
{
    CPU_INT08U               *p_oos_buf;
    FS_NAND_BLK_QTY           blk_ix_phy;
    FS_NAND_BLK_QTY           blk_ix_hdr;
    FS_NAND_BLK_QTY           last_blk_ix_phy;
    CPU_BOOLEAN               blk_is_bad;
    CPU_BOOLEAN               done;
#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
    CPU_BOOLEAN               corrupt;
#endif


    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    blk_ix_hdr                = FS_NAND_BLK_IX_INVALID;
    p_nand_data->MetaBlkIxPhy = FS_NAND_BLK_IX_INVALID;
    p_nand_data->MetaBlkID    = 0u;


    FS_NAND_LowUnmountHandler(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* ------------- CHK FOR EXISTING LOW FMT ------------- */
    blk_ix_hdr = FS_NAND_HdrRd(p_nand_data, p_err);
    if ((*p_err == FS_ERR_NONE) ||
        (*p_err == FS_ERR_DEV_CORRUPT_LOW_FMT) ||
        (*p_err == FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS)) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): An existing low-level format exists; some block erase count may "
                           "be lost during the low-level format operation.\r\n"));

       *p_err = FS_ERR_NONE;

#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
       corrupt = FS_NAND_MetaBlkCorruptDetect(p_nand_data, blk_ix_hdr, p_err);
       if (*p_err != FS_ERR_NONE) {
           FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Detecting corrupted metadata blocks failed.\r\n"));
           return;
       }

       if (corrupt == DEF_NO) {                                 /* If no corruption detected, don't search for old ...  */
#endif                                                          /* ... meta blk.                                        */

           FS_NAND_MetaBlkFind(p_nand_data, blk_ix_hdr, p_err); /* Chk for old meta blk.                                */
           if (*p_err == FS_ERR_NONE) {
                                                                /* An old meta blk was found.                           */
               FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                         p_nand_data->BufPtr,
                                         p_nand_data->OOS_BufPtr,
                                         p_nand_data->MetaBlkIxPhy,
                                         0u,
                                         p_err);
               if (*p_err == FS_ERR_NONE) {
                                                                /* Start ID numbering after old blk ID.                 */
                   MEM_VAL_COPY_GET_INTU_LITTLE(&p_nand_data->MetaBlkID,
                                                &p_oos_buf[FS_NAND_OOS_META_ID_OFFSET],
                                                sizeof(FS_NAND_META_ID));

                   p_nand_data->MetaBlkID += 1u;                /* See Note 4.                                          */
               }
           }

#if (FS_NAND_CFG_CLR_CORRUPT_METABLK == DEF_ENABLED)
       } else {
           FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): All metadata blocks will be erased to recover from metadata blocks corruption.\r\n"));
           FS_NAND_MetaBlkCorruptedErase(p_nand_data,
                                         blk_ix_hdr,
                                         p_err);
           if (*p_err != FS_ERR_NONE) {
               return;
           }
       }
#endif
    }

   *p_err = FS_ERR_NONE;


                                                                /* ------------ FIND GOOD BLK FOR DEV HDR ------------- */
    blk_ix_hdr      = p_nand_data->BlkIxFirst;
    last_blk_ix_phy = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
    done            = DEF_NO;
    do {
        blk_is_bad = FS_NAND_BlkIsFactoryDefect(p_nand_data,
                                                blk_ix_hdr,
                                                p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Fatal error determining if block %u is bad.\r\n",
                                blk_ix_hdr));
            return;
        }

        if (blk_is_bad == DEF_NO) {
                                                                /* -------------------- ERASE BLK --------------------- */
            FS_NAND_BlkEraseHandler(p_nand_data,
                                    blk_ix_hdr,
                                    p_err);
            if (*p_err != FS_ERR_NONE) {
                *p_err  = FS_ERR_NONE;                          /* Ignore the err and go through another loop iter.     */
            } else {
                                                                /* -------------------- WR DEV HDR -------------------- */
                FS_NAND_HdrWr(p_nand_data,
                              blk_ix_hdr,
                              p_err);
                if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Error writing header block on device.\r\n"));
                   *p_err = FS_ERR_NONE;
                } else {
                    done = DEF_YES;
                }
            }
        }

        if (done == DEF_NO) {
            blk_ix_hdr++;
        }
    } while ((blk_ix_hdr <= last_blk_ix_phy) &&
             (done       == DEF_NO));

    if (blk_is_bad == DEF_YES) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Could not find a good block to host device header.\r\n"));
       *p_err = FS_ERR_DEV_FULL;
        return;
    }

                                                                /* ----- SCAN ALL BLKS TO MARK DIRTY AND BAD BLKS ----- */
    for (blk_ix_phy = p_nand_data->BlkIxFirst; blk_ix_phy <= last_blk_ix_phy; blk_ix_phy++) {
        if ((blk_ix_phy != p_nand_data->MetaBlkIxPhy)&&         /* Ignore meta and hdr blks.                            */
            (blk_ix_phy != blk_ix_hdr)              )  {
            blk_is_bad = FS_NAND_BlkIsFactoryDefect(p_nand_data,
                                                    blk_ix_phy,
                                                    p_err);
            if (*p_err != FS_ERR_NONE) {
               *p_err = FS_ERR_NONE;
                                                                /* If defect mark can't be rd, assume it's bad.         */
                FS_NAND_BlkMarkBad(p_nand_data, blk_ix_phy, p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }
            } else {
                if (blk_is_bad == DEF_YES) {                    /* If defect, mark as bad ...                           */
                    FS_NAND_BlkMarkBad(p_nand_data, blk_ix_phy, p_err);
                    if (*p_err != FS_ERR_NONE) {
                        return;
                    }
                } else {                                        /* ... else, mark it as dirty.                          */
                    FS_NAND_BlkMarkDirty(p_nand_data, blk_ix_phy);
                }
            }
        }
    }


                                                                /* ---------------- FILL UP AVAIL TBL ----------------- */
    FS_NAND_AvailBlkTblFill(p_nand_data,
                            FS_NAND_CFG_RSVD_AVAIL_BLK_CNT + 1,
                            DEF_DISABLED,
                            p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Fatal error; unable to fill available block table.\r\n"));
        return;
    }


                                                                /* -------- FIND BLK FOR META IN AVAIL BLK TBL -------- */
    p_nand_data->MetaBlkIxPhy = FS_NAND_BlkGetAvailFromTbl(p_nand_data, DEF_NO, p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Fatal error; unable to get available block for metadata block.\r\n"));
        return;
    }

    FS_NAND_BlkEnsureErased(p_nand_data, p_nand_data->MetaBlkIxPhy, p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Fatal error; unable to erase initial metadata block.\r\n"));
        return;
    }


                                                                /* ------------------- COMMIT META -------------------- */
    p_nand_data->MetaBlkFoldNeeded = DEF_NO;
    p_nand_data->MetaBlkNextSecIx  = 0u;

    FS_NAND_MetaSecRangeInvalidate(p_nand_data,
                                   0u,
                                   p_nand_data->MetaSecCnt);

    FS_NAND_MetaCommit(p_nand_data,
                       DEF_NO,
                       p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowFmtHandler(): Fatal error; unable to commit metadata to device.\r\n"));
        return;
    }


                                                                /* --------------------- MOUNT DEV -------------------- */
    FS_NAND_TRACE_INFO(("FS_NAND_LowFmtHandler(): Low-level fmt'ing complete.\r\n"));

    FS_NAND_LowUnmountHandler(p_nand_data, p_err);              /* Unmount dev.                                         */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FS_NAND_LowMountHandler(p_nand_data, p_err);                /* Mount dev (see Note #2).                             */

}
#endif


/*
*********************************************************************************************************
*                                      FS_NAND_LowMountHandler()
*
* Description : Low-level mount NAND.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_CORRUPT_LOW_FMT  Device low-level format is corrupted.
*                                   FS_ERR_NONE                 Device mounted.
*
*                                   ----------------------------------RETURNED BY FS_NAND_MetaBlkAvailParse()-----------------------------------
*                                   See FS_NAND_MetaBlkAvailParse() for additional return error codes.
*
*                                   --------------------------------------RETURNED BY FS_NAND_SecIsUsed()---------------------------------------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   --------------------------------------RETURNED BY p_ctrlr_api->SecRd()--------------------------------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*                                   -------------------------------------RETURNED BY FS_NAND_MetaBlkFind()--------------------------------------
*                                   See FS_NAND_MetaBlkFind() for additional return error codes.
*
*                                   ---------------------------------------RETURNED BY FS_NAND_UB_Load()----------------------------------------
*                                   See FS_NAND_UB_Load() for additional return error codes.
*
*                                   -------------------------------------RETURNED BY FS_NAND_MetaBlkParse()-------------------------------------
*                                   See FS_NAND_MetaBlkParse() for additional return error codes.
*
*                                   ----------------------------------------RETURNED BY FS_NAND_HdrRd()-----------------------------------------
*                                   See FS_NAND_HdrRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_LowMountHandler (FS_NAND_DATA  *p_nand_data,
                                               FS_ERR        *p_err)
{
    CPU_INT08U         *p_sec_buf;
    CPU_INT08U         *p_oos_buf;
    FS_NAND_BLK_QTY     blk_ix_phy_hdr;
    FS_NAND_BLK_QTY     blk_ix_phy;
    FS_NAND_BLK_QTY     blk_ix_logical;
    FS_NAND_BLK_QTY     last_blk_ix;
    FS_NAND_SEC_TYPE    blk_type;
    CPU_BOOLEAN         blk_is_bad;
    CPU_BOOLEAN         blk_is_dirty;
    CPU_BOOLEAN         blk_is_ub;
    CPU_BOOLEAN         blk_is_avail;
    FS_SEC_QTY          sec_ix_phy;
    CPU_BOOLEAN         sec_is_used;
    CPU_INT08U          rd_retries;


    p_sec_buf = (CPU_INT08U *)p_nand_data->BufPtr;
    p_oos_buf = (CPU_INT08U *)p_nand_data->OOS_BufPtr;


                                                                /* ------------------- FIND HDR BLK ------------------- */
    blk_ix_phy_hdr = FS_NAND_HdrRd(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Can't read device header.\r\n"));
        return;
    }

                                                                /* ----------------- FIND MAPPING BLK ----------------- */
    FS_NAND_MetaBlkFind(p_nand_data, blk_ix_phy_hdr, p_err);
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
        return;
    }

                                                                /* ------------------ PARSE META BLK ------------------ */
    FS_NAND_MetaBlkParse(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
        return;
    }

                                                                /* ------------- LOAD DIRTY BITMAP CACHE -------------- */
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN == DEF_ENABLED)
    Mem_Copy(&p_nand_data->DirtyBitmapCache[0u],
             &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap],
              p_nand_data->DirtyBitmapSize);
#endif

                                                                /* -------------- SCAN BLKS TO FILL META -------------- */
    last_blk_ix = p_nand_data->BlkIxFirst + p_nand_data->BlkCnt - 1u;
                                                                /* Ix preincremented to avoid scanning hdr blk.         */
    for (blk_ix_phy = blk_ix_phy_hdr + 1u; blk_ix_phy <= last_blk_ix; blk_ix_phy++) {

                                                                /* Chk that blk isn't bad.                              */
        blk_is_bad = FS_NAND_BlkIsBad(p_nand_data, blk_ix_phy);
        if (blk_is_bad == DEF_NO) {
            blk_is_dirty = FS_NAND_BlkIsDirty(p_nand_data, blk_ix_phy);
            blk_is_avail = FS_NAND_BlkIsAvail(p_nand_data, blk_ix_phy);

            if ((blk_is_dirty == DEF_YES) &&                    /* If blk is both dirty and avail ...                   */
                (blk_is_avail == DEF_YES)) {
                                                                /* ... unmark dirty.                                    */
                FSUtil_MapBitClr(&p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap], blk_ix_phy);
            } else {
                if ((blk_is_dirty == DEF_NO) &&                 /* Nothing to do for dirty or avail blks.               */
                    (blk_is_avail == DEF_NO)   ){
                    sec_ix_phy    = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);

                    blk_is_ub     = FS_NAND_BlkIsUB(p_nand_data, blk_ix_phy);
                    sec_is_used   = FS_NAND_SecIsUsed(p_nand_data, sec_ix_phy, p_err);
                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Unable to determine if sector %u is used.\r\n",
                                            blk_ix_phy));
                       *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                        return;
                    }

                    if (blk_is_ub == DEF_YES) {                 /* Blk is an UB.                                        */

                                                                /* Load blk.                                            */
                        FS_NAND_UB_Load(p_nand_data, blk_ix_phy, p_err);
                        switch (*p_err) {
                            case FS_ERR_NONE:
                                 break;

                            default:
                                 FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Could not load update block at ix=%u.\r\n",
                                                     blk_ix_phy));
                                *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                                 return;
                        }

                    } else {
                        if (sec_is_used == DEF_YES) {           /* Blk is either a meta or a data blk.                  */

                                                                /* Rd sec to determine if blk is meta or data.          */
                            rd_retries = 0u;
                            do {
                                FS_NAND_SecRdPhyNoRefresh(p_nand_data,
                                                          p_sec_buf,
                                                          p_oos_buf,
                                                          blk_ix_phy,
                                                          0u,
                                                          p_err);
                                rd_retries++;
                            } while ((rd_retries <= FS_NAND_CFG_MAX_RD_RETRIES) &&
                                     (*p_err     == FS_ERR_ECC_UNCORR));

                            switch (*p_err) {
                                case FS_ERR_ECC_UNCORR:
                                     FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Fatal ECC error reading block %u.\r\n",
                                                         blk_ix_phy));
                                    *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                                     return;


                                case FS_ERR_NONE:
                                     break;


                                default:
                                     return;
                            }

                                                                /* Parse sec type from OOS meta.                        */
                            blk_type = FS_NAND_SecTypeParse(p_oos_buf);

                            if (blk_type == FS_NAND_SEC_TYPE_STORAGE) {
                                                                /* Blk is a data blk.                                   */

                                                                /* Get assoc logical blk ix.                            */
                                MEM_VAL_COPY_GET_INTU_LITTLE(&blk_ix_logical,
                                                             &p_oos_buf[FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET],
                                                              sizeof(FS_NAND_BLK_QTY));

                                                                /* Validate that the mapping entry is inexistant.       */
                                if (p_nand_data->LogicalToPhyBlkMap[blk_ix_logical] != FS_NAND_BLK_IX_INVALID) {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                    if (blk_ix_phy == last_blk_ix) {
                                        FS_NAND_TRACE_INFO(("Logical block %d to last physical block mapping conflict.\r\n", blk_ix_logical));
                                        FS_NAND_TRACE_INFO(("Discarding last physical block.\r\n"));
                                        FS_NAND_BlkMarkDirty(p_nand_data, last_blk_ix);
                                    } else
#endif
                                    {
                                        FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Fatal error: logical block %u is already mapped "
                                                           "to physical block %u; cannot map it to physical block %u\r\n",
                                                            blk_ix_logical,
                                                            p_nand_data->LogicalToPhyBlkMap[blk_ix_logical],
                                                            blk_ix_phy));
                                       *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                                        return;
                                    }
                                } else {                        /* Put in tbl.                                          */
                                    p_nand_data->LogicalToPhyBlkMap[blk_ix_logical] = blk_ix_phy;
                                }

                            } else if (blk_type == FS_NAND_SEC_TYPE_METADATA) {
                                                                /* Blk is a metadata blk.                               */
                                                                /* Make sure every old meta blk is marked as dirty.     */
                                if (blk_ix_phy != p_nand_data->MetaBlkIxPhy) {
                                    FS_NAND_TRACE_LOG(("FS_NAND_LowMountHandler(): Found old metadata block (phy ix = %d).\r\n", blk_ix_phy));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                    FS_NAND_BlkMarkDirty(p_nand_data, blk_ix_phy);
#endif
                                }
                            } else {
                                FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Invalid block type for block %u.\r\n",
                                                    blk_ix_phy));
                               *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }


#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* --------- PARSE AVAIL BLK TBL TMP COMMITS ---------- */
    FS_NAND_MetaBlkAvailParse(p_nand_data, p_err);
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_DEV_CORRUPT_LOW_FMT;
        FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Error parsing available table temporary commits.\r\n"));
        return;
    }
#endif


    p_nand_data->Fmtd        = DEF_YES;                         /* Update low level fmt validity flag.                  */

    FS_NAND_TRACE_DBG(("FS_NAND_LowMountHandler(): Low level mount succeeded.\r\n"));

}


/*
*********************************************************************************************************
*                                     FS_NAND_LowUnmountHandler()
*
* Description : Low-level unmount NAND.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               ------          Argument validated by caller.
*
*                                   FS_ERR_NONE             Device unmounted.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_LowUnmountHandler (FS_NAND_DATA  *p_nand_data,
                                                 FS_ERR        *p_err)
{
    (void)p_err;
    FS_NAND_InitDevData(p_nand_data);
}


/*
*********************************************************************************************************
*                                         FS_NAND_DumpHandler()
*
* Description : Dumps the entire raw NAND device in multiple data chunks through a user-supplied callback
*               function.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               dump_fnct       Callback that will return the read raw NAND data in chunks.
*               ---------       Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               ------          Argument validated by caller.
*
*                                   ------------------RETURNED BY dump_fnct()------------------
*                                   See dump_fnct() for additional return error codes.
*
*                                   -------------RETURNED BY p_ctrlr->SpareRdRaw()-------------
*                                   See p_ctrlr->SpareRdRaw() for additional return error codes.
*
*                                   ----------------RETURNED BY FS_TRACE_INFO()----------------
*                                   See FS_TRACE_INFO() for additional return error codes.
*
*                                   ---------------RETURNED BY p_ctrlr->IO_Ctrl()---------------
*                                   See p_ctrlr->IO_Ctrl() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/
#if (FS_NAND_CFG_DUMP_SUPPORT_EN == DEF_ENABLED)
FS_NAND_INTERN  void  FS_NAND_DumpHandler (FS_NAND_DATA   *p_nand_data,
                                           void          (*dump_fnct)(void        *buf,
                                                                      CPU_SIZE_T   buf_len,
                                                                      FS_ERR      *p_err),
                                           FS_ERR         *p_err)
{
    FS_NAND_BLK_QTY          blk_ix;
    FS_NAND_PG_PER_BLK_QTY   pg_ix;
    FS_NAND_IO_CTRL_DATA     pg_rd_data;
    FS_NAND_CTRLR_API       *p_ctrlr;


    p_ctrlr = p_nand_data->CtrlrPtr;


    for (blk_ix = 0u; blk_ix < (p_nand_data->BlkCnt + p_nand_data->BlkIxFirst); blk_ix++) {
        FS_TRACE_INFO(("FS_NAND_DumpHandler: dumping block %u.\r\n", blk_ix));
        for (pg_ix = 0u; pg_ix < p_nand_data->PartDataPtr->PgPerBlk; pg_ix++) {
                                                                /* ------------------- DUMP PG AREA ------------------- */
            pg_rd_data.IxPhy   = blk_ix * p_nand_data->PartDataPtr->PgPerBlk + pg_ix;
            pg_rd_data.DataPtr = p_nand_data->BufPtr;
            pg_rd_data.OOS_Ptr = DEF_NULL;

            p_ctrlr->IO_Ctrl(p_nand_data->CtrlrDataPtr,
                             FS_DEV_IO_CTRL_PHY_RD_PAGE,
                            &pg_rd_data,
                             p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_INFO(("FS_NAND_DumpHandler: error %u at blk %u, reading page %u.\r\n",
                               *p_err,
                                blk_ix,
                                pg_ix));
                return;
            }

            dump_fnct(p_nand_data->BufPtr,
                      p_nand_data->PartDataPtr->PgSize,
                      p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_INFO(("FS_NAND_DumpHandler: call to 'dump_fnct' failed w/err %u.\r\n",
                               *p_err));
                return;
            }

                                                                /* ----------------- DUMP SPARE AREA ------------------ */
            p_ctrlr->SpareRdRaw(p_nand_data->CtrlrDataPtr,
                                p_nand_data->BufPtr,
                                blk_ix * p_nand_data->PartDataPtr->PgPerBlk + pg_ix,
                                0u,
                                p_nand_data->PartDataPtr->SpareSize,
                                p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_INFO(("FS_NAND_DumpHandler: error %u at blk %u, reading page %u (spare).\r\n",
                               *p_err,
                                blk_ix,
                                pg_ix));
                return;
            }

            dump_fnct(p_nand_data->BufPtr,
                      p_nand_data->PartDataPtr->SpareSize,
                      p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_INFO(("FS_NAND_DumpHandler: call to 'dump_fnct' failed w/err %u.\r\n",
                               *p_err));
                return;
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                        FS_NAND_SecRdHandler()
*
* Description : (1) Read sector (using logical block index) from a NAND device & store data in buffer.
*               (2) Handles some errors that may occur during read.
*               (3) May refresh an unreadable block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_dest          Pointer to destination buffer.
*               ------          Argument validated by caller.
*
*               p_dest_oos      Pointer to destination spare-area buffer.
*               ----------      Argument validated by caller.
*
*               blk_ix_logical  Logical index of the block to read from.
*
*               sec_offset_phy  Physical offset of the sector to read.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_IO                   Device I/O error.
*                                   FS_ERR_NONE                     Sector read successfully.
*
*                                   ------------RETURNED BY p_ctrlr_api->SecRd()-------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
*                                   ------------RETURNED BY FS_NAND_BlkRefresh()-------------
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*
*                                   ------------RETURNED BY FS_NAND_BlkMarkBad()-------------
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_SecRdHandler (FS_NAND_DATA             *p_nand_data,
                                            void                     *p_dest,
                                            void                     *p_dest_oos,
                                            FS_NAND_BLK_QTY           blk_ix_logical,
                                            FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                            FS_ERR                   *p_err)
{
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    void               *p_ctrlr_data;
    FS_NAND_BLK_QTY     blk_ix_phy;
    FS_SEC_QTY          sec_ix_phy;


    p_ctrlr_api  = p_nand_data->CtrlrPtr;
    p_ctrlr_data = p_nand_data->CtrlrDataPtr;


    while (DEF_YES) {                                           /* Loop until rd success or fatal err.                  */
                                                                /* Calc sec ix phy.                                     */
        blk_ix_phy  = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_logical);
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
            FS_NAND_TRACE_DBG(("FS_NAND_SecRdHandler(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
           *p_err = FS_ERR_DEV_IO;
            return;
        }
#endif

        sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
        sec_ix_phy += sec_offset_phy;

        p_ctrlr_api->SecRd(p_ctrlr_data,                        /* Rd sec.                                              */
                           p_dest,
                           p_dest_oos,
                           sec_ix_phy,
                           p_err);

        if (*p_err == FS_ERR_ECC_CORR) {
           *p_err = FS_ERR_NONE;
            return;
        } else if ((*p_err == FS_ERR_ECC_UNCORR) ||
                   (*p_err == FS_ERR_ECC_CRITICAL_CORR)) {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
           *p_err =  FS_ERR_NONE;
            FS_NAND_BlkRefresh(p_nand_data,                     /* Refresh blk.                                         */
                               blk_ix_phy,
                               p_err);
            if (*p_err != FS_ERR_NONE) {                        /* If refresh failed ...                                */
                FS_NAND_TRACE_DBG(("FS_NAND_SecRdHandler(): Uncorrectable read err in blk %u, marking it as bad.\r\n",
                                    blk_ix_phy));

               *p_err = FS_ERR_NONE;
                FS_NAND_BlkMarkBad(p_nand_data,                 /* ... mark blk as bad.                                 */
                                   blk_ix_phy,
                                   p_err);
                if (*p_err != FS_ERR_NONE) {
                    FS_NAND_TRACE_DBG(("FS_NAND_SecRdHandler(): Could not mark failing blk %u bad.\r\n",
                                        blk_ix_phy));
                }
               *p_err = FS_ERR_ECC_UNCORR;
                return;
            } else {                                            /* ... else, mark blk as dirty.                         */
                FS_NAND_BlkMarkDirty(p_nand_data,
                                     blk_ix_phy);
            }

           *p_err = FS_ERR_NONE;
#else
            return;                                             /* Ret err to caller.                                   */
#endif
        } else if (*p_err != FS_ERR_NONE) {                     /* Unhandled err.                                       */
            return;
        } else {                                                /* Success.                                             */
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                      FS_NAND_SecRdPhyNoRefresh()
*
* Description : Read a sector in a physical block from a NAND device & store data in buffer. Handles
*               some errors that may occur during read. May NOT refresh an unreadable block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_dest          Pointer to destination buffer.
*               ------          Argument validated by caller.
*
*               p_dest_oos      Pointer to destination spare-area buffer.
*               ----------      Argument validated by caller.
*
*               blk_ix_phy      Physical index of block containing sector to read.
*
*               sec_offset_phy  Physical offset of the sector to read.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_IO       Device I/O error.
*                                   FS_ERR_ECC_UNCORR   Unreadable sector: uncorrectable ECC error.
*                                   FS_ERR_NONE         Sector read successfully.
*
*                                   ---------------RETURNED BY p_ctrlr_api->SecRd()----------------
*                                   See p_ctrlr_api->SecRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_SecRdPhyNoRefresh(FS_NAND_DATA             *p_nand_data,
                                                void                     *p_dest,
                                                void                     *p_dest_oos,
                                                FS_NAND_BLK_QTY           blk_ix_phy,
                                                FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                FS_ERR                   *p_err)
{
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    void               *p_ctrlr_data;
    FS_SEC_QTY          sec_ix_phy;
    CPU_INT08U          rd_retries;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecRdHandlerNoRefresh(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif


    p_ctrlr_api    = p_nand_data->CtrlrPtr;
    p_ctrlr_data   = p_nand_data->CtrlrDataPtr;


    rd_retries   = 0u;
    while (rd_retries < FS_NAND_CFG_MAX_RD_RETRIES) {           /* Loop until rd success or retry cnt exceeded.         */
                                                                /* Calc sec ix phy.                                     */
        sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
        sec_ix_phy += sec_offset_phy;

        p_ctrlr_api->SecRd(p_ctrlr_data,                        /* Rd sec.                                              */
                           p_dest,
                           p_dest_oos,
                           sec_ix_phy,
                           p_err);

        if ((*p_err == FS_ERR_ECC_CORR) ||
            (*p_err == FS_ERR_ECC_CRITICAL_CORR)) {
           *p_err = FS_ERR_NONE;
            return;
        } else if (*p_err == FS_ERR_ECC_UNCORR) {               /* Retry on uncorrectable ecc err.                      */
            CPU_INT32U  sec_used_mark;                          /* Chk used mark.                                       */
            CPU_INT08U  set_bit_cnt;

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
            if (sizeof(sec_used_mark) < p_nand_data->UsedMarkSize) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecRdPhyNoRefresh(): Local variable sec_used_mark is too small to contain an appropriate used mark.\r\n"));
               *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
                return;
            }
#endif

            sec_used_mark = 0u;
            MEM_VAL_COPY_GET_INTU_LITTLE(&sec_used_mark,
                              (CPU_CHAR *)p_dest_oos + FS_NAND_OOS_SEC_USED_OFFSET,
                                          p_nand_data->UsedMarkSize);

            set_bit_cnt = CRCUtil_PopCnt_32(sec_used_mark);

            if (set_bit_cnt < (p_nand_data->UsedMarkSize * DEF_INT_08_NBR_BITS / 2u)) {
                rd_retries++;                                   /* Sec is used, retry.                                  */
               *p_err = FS_ERR_NONE;
            } else {
                return;                                         /* Sec isn't used, no need to retry.                    */
            }

        } else if (*p_err != FS_ERR_NONE) {                     /* Unhandled err.                                       */
            return;
        } else {                                                /* Success.                                             */
            return;
        }
    }

                                                                /* Max retry cnt reached.                               */
   *p_err = FS_ERR_ECC_UNCORR;
    return;
}


/*
*********************************************************************************************************
*                                           FS_NAND_SecRd()
*
* Description : Read one sector from a NAND device & store data in buffer.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_dest          Pointer to destination buffer.
*               ------          Argument validated by caller.
*
*               sec_ix_logical  Logical index of the sector to read.
*
*               sec_cnt         Number of sectors to read (unused).
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_ECC  Invalid ECC during read.
*                                   FS_ERR_DEV_INVALID_OP   Device invalid operation.
*                                   FS_ERR_DEV_IO           Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT      Device timeout.
*                                   FS_ERR_NONE             Sector read successfully.
*
*                                   ---------RETURNED BY FS_NAND_SecIsUsed()---------
*                                   See FS_NAND_SecIsUsed() for additional return error codes.
*
*                                   --------RETURNED BY FS_NAND_UB_SecFind()---------
*                                   See FS_NAND_UB_SecFind() for additional return error codes.
*
*                                   -------RETURNED BY FS_NAND_SecRdHandler()--------
*                                   See FS_NAND_SecRdHandler() for additional return error codes.
*
* Return(s)   : Number of sectors read from device.
*
* Note(s)     : (1) The function must search for the sector(s) in the UB (update blocks) before searching
*                   data blocks to make sure to get the latest version of the sector written on the device.
*
*               (2) For the moment, the parameter 'sec_cnt' is not used, and only one sector is read for
*                   each call. However, the parameter is left for future implementation.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_SEC_QTY  FS_NAND_SecRd (FS_NAND_DATA  *p_nand_data,
                                           void          *p_dest,
                                           FS_SEC_QTY     sec_ix_logical,
                                           FS_SEC_QTY     sec_cnt,
                                           FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY           blk_ix_logical;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical_rd;
    FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy;
    FS_NAND_UB_SEC_DATA       ub_sec_data;
    FS_NAND_UB_DATA           ub_data;
    FS_SEC_QTY                sec_ix_phy;
    FS_NAND_BLK_QTY           blk_ix_phy;
    CPU_BOOLEAN               is_sec_in_ub;
    CPU_BOOLEAN               is_sec_used;
    CPU_BOOLEAN              *p_dest_oos;


    (void)sec_cnt;
    p_dest_oos = (CPU_INT08U *)p_nand_data->OOS_BufPtr;

    blk_ix_logical     = FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);
    sec_offset_logical = sec_ix_logical - FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_logical);

                                                                /* --------------- FIND VALID SEC IN UB --------------- */
    is_sec_in_ub = DEF_NO;

    ub_sec_data  = FS_NAND_UB_Find(p_nand_data,
                                   blk_ix_logical);

    if (ub_sec_data.UB_Ix != FS_NAND_UB_IX_INVALID) {
                                                                /* Find sec in UB.                                      */
        ub_sec_data = FS_NAND_UB_SecFind(p_nand_data,
                                         ub_sec_data,
                                         sec_offset_logical,
                                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_SecRd(): Error looking for sector %u in update block %u.\r\n",
                                sec_offset_logical,
                                ub_sec_data.UB_Ix));

            return (0u);
        }

        if (ub_sec_data.SecOffsetPhy != FS_NAND_SEC_OFFSET_IX_INVALID) {
            sec_offset_phy = ub_sec_data.SecOffsetPhy;

                                                                /* Chk is sec is valid.                                 */
            ub_data = FS_NAND_UB_TblEntryRd(p_nand_data,
                                            ub_sec_data.UB_Ix);

            is_sec_in_ub = FSUtil_MapBitIsSet(ub_data.SecValidBitMap, sec_offset_phy);

            if (is_sec_in_ub == DEF_YES) {
                blk_ix_logical = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_sec_data.UB_Ix);
            }
        }
    }

                                                                /* --------------- FIND SEC ON DATA BLK --------------- */
    if (is_sec_in_ub == DEF_NO) {
                                                                /* Chk if sec is wr'en.                                 */
        blk_ix_phy  = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_logical);

        if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
            is_sec_used = DEF_NO;                               /* No data blk assoc with logical blk.                  */
        } else {
            sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
            sec_ix_phy += sec_offset_logical;

            is_sec_used = FS_NAND_SecIsUsed(p_nand_data, sec_ix_phy, p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecRd(): Error determining if sector %u is used.\r\n",
                                    sec_ix_phy));

                return (0u);
            }
        }

        if (is_sec_used == DEF_YES) {
            sec_offset_phy = sec_offset_logical;
        } else {
           *p_err = FS_ERR_DEV_NAND_NO_SUCH_SEC;
            FS_NAND_TRACE_LOG(("FS_NAND_SecRd(): Logical sector %u does not exist on device.\r\n",
                                sec_ix_logical));

            return (0u);
        }
    }


                                                                /* ------------------ PERFORM SEC RD ------------------ */
    FS_NAND_SecRdHandler(p_nand_data,
                         p_dest,
                         p_nand_data->OOS_BufPtr,
                         blk_ix_logical,
                         sec_offset_phy,
                         p_err);

    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecRd(): Error reading physical sector %u in logical block %u.\r\n",
                            sec_offset_phy,
                            blk_ix_logical));

        return (0u);
    }

                                                                /* Chk if sec is dummy.                                 */
    MEM_VAL_COPY_GET_INTU_LITTLE(&sec_offset_logical_rd,
                                 &p_dest_oos[FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET],
                                  sizeof(FS_NAND_SEC_PER_BLK_QTY));

    if (sec_offset_logical_rd == FS_NAND_SEC_OFFSET_IX_INVALID) {
       *p_err = FS_ERR_DEV_NAND_NO_SUCH_SEC;

        FS_NAND_TRACE_DBG(("FS_NAND_SecRd(): Tried to read an invalid, dummy sector (sec %u).\r\n",
                            sec_ix_logical));

        return (0u);
    }

    return (1u);
}


/*
*********************************************************************************************************
*                                       FS_NAND_SecWrHandler()
*
* Description : Write data sector and associated out of sector data to a NAND device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               p_src_oos       Pointer to source spare-area buffer.
*               ---------       Argument validated by caller.
*
*               blk_ix_logical  Block's logical index.
*
*               sec_offset_phy  Physical offset of the sector to write.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_OP_ABORTED           Operation failed, but bad block was refreshed succesfully.
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   -----------------------------RETURNED BY FS_NAND_BlkRefresh()-----------------------------
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*
*                                   -----------------------------RETURNED BY FS_NAND_BlkMarkBad()-----------------------------
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*
*                                   -----------------------------RETURNED BY p_ctrlr_api->SecWr()-----------------------------
*                                   See p_ctrlr_api->SecWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) When a programming error occurs, the block is refreshed and marked bad. Sector write
*                   operation is aborted. The caller is responsible for retrying the operation. If p_dest
*                   and p_dest_oos are pointers to internal buffers (p_nand_data->BufPtr and p_nand_data->
*                   OOS_BufPtr), the caller MUST refresh their content prior to retrying the operation
*                   since it might have been overwritten by FS_NAND_BlkRefresh() to backup sectors.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_SecWrHandler (FS_NAND_DATA             *p_nand_data,
                                            void                     *p_src,
                                            void                     *p_src_oos,
                                            FS_NAND_BLK_QTY           blk_ix_logical,
                                            FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                            FS_ERR                   *p_err)
{
    FS_NAND_BLK_QTY     blk_ix_phy;
    FS_SEC_QTY          sec_ix_phy;
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    void               *p_ctrlr_data;


    p_ctrlr_api  = p_nand_data->CtrlrPtr;
    p_ctrlr_data = p_nand_data->CtrlrDataPtr;

                                                                /* ------------------ CALC SEC PHY IX ----------------- */
    blk_ix_phy  = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_logical);
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecWrHandler(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif
    sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
    sec_ix_phy += sec_offset_phy;


                                                                /* ------------------ WR SEC TO DEV ------------------- */
    p_ctrlr_api->SecWr(p_ctrlr_data,
                       p_src,
                       p_src_oos,
                       sec_ix_phy,
                       p_err);
    if (*p_err == FS_ERR_DEV_IO) {                              /* ------------------- HANDLE ERRS -------------------- */
       *p_err = FS_ERR_NONE;

        FS_NAND_BlkRefresh(p_nand_data, blk_ix_phy, p_err);     /* Refresh blk.                                         */

        switch (*p_err) {
            case FS_ERR_ECC_UNCORR:                             /* Ignore uncorrectable ECC err.                        */
                *p_err = FS_ERR_NONE;
                 break;


            case FS_ERR_NONE:
                 break;


            default:
                 return;
        }

        FS_NAND_BlkMarkBad(p_nand_data, blk_ix_phy, p_err);     /* Mark blk bad.                                        */
        if (*p_err != FS_ERR_NONE) {
            FS_NAND_TRACE_DBG(("FS_NAND_SecWrHandler(): Failed to mark failing block %u as bad.\r\n",
                                blk_ix_phy));
            return;
        }

       *p_err = FS_ERR_DEV_OP_ABORTED;                          /* Notify caller op has failed.                         */
    }
}
#endif


/*
*********************************************************************************************************
*                                     FS_NAND_MetaSecWrHandler()
*
* Description : Write metadata sector and associated out of sector data to a NAND device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               p_src_oos       Pointer to source spare-area buffer.
*               ---------       Argument validated by caller.
*
*               sec_offset_phy  Physical offset of the sector to write.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_OP_ABORTED   Operation did not complete.
*                                   FS_ERR_NONE             Metadata sector written successfully.
*
*                                   --------------RETURNED BY p_ctrlr_api->SecWr()---------------
*                                   See p_ctrlr_api->SecWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) When a programming error occurs, a new metadata block is started. Metadata is
*                   committed to that new block. Operation aborts and must be restarted by caller.
*                   If p_dest and p_dest_oos are pointers to buffers p_nand_data->BufPtr and
*                   p_nand_data->OOS_BufPtr, the buffers might be overwritten and thus must be repopulated
*                   by the caller.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_MetaSecWrHandler (FS_NAND_DATA             *p_nand_data,
                                                void                     *p_src,
                                                void                     *p_src_oos,
                                                FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                                FS_ERR                   *p_err)
{
    FS_NAND_BLK_QTY           blk_ix_logical;
    FS_NAND_BLK_QTY           blk_ix_phy;
    FS_SEC_QTY                sec_ix_phy;
    FS_NAND_CTRLR_API        *p_ctrlr_api;
    void                     *p_ctrlr_data;


    p_ctrlr_api    = p_nand_data->CtrlrPtr;
    p_ctrlr_data   = p_nand_data->CtrlrDataPtr;
                                                                /* Set blk_ix_logical to wr to meta blk.                */
    blk_ix_logical = p_nand_data->LogicalDataBlkCnt + p_nand_data->UB_CntMax;

                                                                /* ----------------- CALC SEC PHY IX ------------------ */
    blk_ix_phy   = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_logical);
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
        FS_NAND_TRACE_DBG(("FS_NAND_MetaSecWrHandler(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
#endif
    sec_ix_phy  = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_phy);
    sec_ix_phy += sec_offset_phy;


                                                                /* ------------------ WR SEC TO DEV ------------------- */
    p_ctrlr_api->SecWr(p_ctrlr_data,
                       p_src,
                       p_src_oos,
                       sec_ix_phy,
                       p_err);
    if (*p_err == FS_ERR_DEV_IO) {                              /* If meta blk cannot be wr'en ...                      */

        p_nand_data->MetaBlkFoldNeeded = DEF_YES;               /* ... trigger a folding op.                            */
        FS_NAND_TRACE_DBG(("FS_NAND_MetaSecWrHandler(): Meta block on device is not accessible. Corruption could occur.\r\n"));
       *p_err = FS_ERR_DEV_OP_ABORTED;
    }
}
#endif


/*
*********************************************************************************************************
*                                           FS_NAND_SecWr()
*
* Description : Write one logical sector on NAND device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               sec_ix_logical  Sector's logical index.
*
*               sec_cnt         Number of consecutive sectors to write (unused).
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_ECC  Invalid ECC during read.
*                                   FS_ERR_DEV_INVALID_OP   Device invalid operation.
*                                   FS_ERR_DEV_IO           Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT      Device timeout.
*                                   FS_ERR_NONE             Sector read successfully.
*
*                                   ---------RETURNED BY FS_NAND_SecWrInUB()---------
*                                   See FS_NAND_SecWrInUB() for additional return error codes.
*
* Return(s)   : Number of sectors written. May not be equal to cnt.
*
* Note(s)     : (1) For the moment, the parameter 'sec_cnt' is not used, and only one sector is write for
*                   each call. However, the parameter is left for future implementation.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_SEC_QTY  FS_NAND_SecWr (FS_NAND_DATA  *p_nand_data,
                                           void          *p_src,
                                           FS_SEC_QTY     sec_ix_logical,
                                           FS_SEC_QTY     sec_cnt,
                                           FS_ERR        *p_err)
{
    FS_NAND_BLK_QTY          blk_ix_logical;
    FS_NAND_UB_SEC_DATA      ub_sec_data;
    FS_SEC_QTY               sec_wr_cnt;


    blk_ix_logical = FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);


    ub_sec_data.UB_Ix        = FS_NAND_UB_IX_INVALID;
    ub_sec_data.SecOffsetPhy = FS_NAND_SEC_OFFSET_IX_INVALID;

                                                                /* --------- CHK IF UB EXISTS FOR LOGICAL BLK --------- */
    ub_sec_data = FS_NAND_UB_Find(p_nand_data,                  /* Find associated UB.                                  */
                                  blk_ix_logical);


                                                                /* --------------------- WR IN UB --------------------- */
    sec_wr_cnt = FS_NAND_SecWrInUB(p_nand_data,                 /* Wr sec in UB.                                        */
                                   p_src,
                                   sec_ix_logical,
                                   sec_cnt,
                                   ub_sec_data,
                                   p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecWr(): Error writing sector %u in an update block.\r\n.",
                            sec_ix_logical));
        return (sec_wr_cnt);
    }


    return (sec_wr_cnt);
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_SecWrInUB()
*
* Description : Write 1 or more logical sectors in an update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               sec_ix_logical  Sector's logical index.
*
*               sec_cnt         Number of consecutive sectors to write.
*
*               ub_sec_data     Structure containing data on sector's latest update.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_ECC  Invalid ECC during read.
*                                   FS_ERR_DEV_INVALID_OP   Device invalid operation.
*                                   FS_ERR_DEV_IO           Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT      Device timeout.
*                                   FS_ERR_NONE             Sector read successfully.
*
*                                   ---------RETURNED BY FS_NAND_SUB_Merge()---------
*                                   See FS_NAND_SUB_Merge() for additional return error codes.
*
*                                   ---------RETURNED BY FS_NAND_RUB_Merge()---------
*                                   See FS_NAND_RUB_Merge() for additional return error codes.
*
*                                   --------RETURNED BY FS_NAND_SecWrInSUB()---------
*                                   See FS_NAND_SecWrInSUB() for additional return error codes.
*
*                                   --------RETURNED BY FS_NAND_SecWrInRUB()---------
*                                   See FS_NAND_SecWrInRUB() for additional return error codes.
*
*                                   ------RETURNED BY FS_NAND_SUB_MergeUntil()-------
*                                   See FS_NAND_SUB_MergeUntil() for additional return error codes.
*
*                                   -----RETURNED BY FS_NAND_RUB_PartialMerge()------
*                                   See FS_NAND_RUB_PartialMerge() for additional return error codes.
*
*                                   ---------RETURNED BY FS_NAND_RUB_Alloc()---------
*                                   See FS_NAND_RUB_Alloc() for additional return error codes.
*
*                                   ---------RETURNED BY FS_NAND_SUB_Alloc()---------
*                                   See FS_NAND_SUB_Alloc() for additional return error codes.
*
* Return(s)   : Number of sectors written. Might not be equal to sec_cnt.
*
* Note(s)     : (1) Flowchart is shown below. Labels in parenthesis are written in appropriate
*                   section comments.
*
*                                                 +-----------+
*                                                 | Wr in UB  |
*             (A1d)             (A1b)             +-----+-----+
* +---------------+   +-------------+                   |
* |   Wr in RUB   |   |PartMerge RUB|            /------+------\
* +-------+-------+   |  Start SUB  |          +-|Sec 0 of LB? |--+
*         |           +------+------+          | \-------------/  |
*         |No (A1a)          |No (A1)          |Yes (A)           |No  (B)        (B1)
* /-------+-------\   /------+------\   /------+------\    /------'------\   /-------------\  +---------+
* |      RUB      |___| Write count |___| RUB exists  |    | RUB exists  |___|  RUB full?  |__|Wr in RUB|
* |     full?     |Yes|< threshold? |Yes|   for LB?   |    |   for LB?   |Yes|             |No|         |
* |               |   | (Note #2a)  |   |             |    |             |   |             |  |         |
* \-------+-------/   \-------------/   \------+------/    \------+------/   \------+------/  +---------+
*         |Yes  (A1c)                          |No                |No               |Yes
* +-------+---------+            (A3)          |                  |         +-------+--------+
* |    Merge RUB    | +-------------+   /------+------\           |         |   Merge RUB    |
* |Alloc SUB for LB | |  Start SUB  |___| SUB exists  |           |         |Alloc RUB for LB|
* |    Wr in SUB    | |  Wr in SUB  | No|   for LB?   |           |         |   Wr in RUB    |
* +-----------------+ +-------------+   \------+------/           |         +----------------+
*                                              |Yes               |
*                               (A2b)          |   (A2)           |   (B3)
*                     +-------------+   /------+------\    /------+------\  +----------------+
*                     |Merge old SUB|___|Nbr free sec |    | SUB exists  |__|Alloc RUB for LB|
*                     |Start new SUB| No|> threshold? |    |   for LB?   |No|   Wr in RUB    |
*                     |             |   | (Note #2b)  |    |             |  |                |
*                     +-------------+   \------+------/    \------+------/  +----------------+
*                                              |Yes               |Yes
*                                              |  (A2a)           |   (B2)
*                                       +------+------+    /------+------\
*                                       |  SUB->RUB   |  __| Sec already |____
*                                       |  Wr in RUB  |  | |   wr'en?    |   |
*                                       +-------------+  | \-------------/   |
*                                                        |No                 |Yes
*                                        (B2c)           |  (B2b)            |  (B2a)            (B2a2)
*                               +-------------+   /------+------\     /------+------\   +-------------+
*                               |   Pad SUB   |___|  Gap below  |     |Nbr free sec |___|  SUB->RUB   |
*                               |  Wr in SUB  |Yes| threshold?  |     |> threshold? |Yes|  Wr in RUB  |
*                               |             |   | (Note #2c)  |     | (Note #2b)  |   |             |
*                               +-------------+   \------+------/     \------+------/   +-------------+
*                                                        |No                 |No
*                                        (B2d2)          |  (B2d)            |   (B2a1)
*                               +-------------+   /------+------\    +-------+--------+
*                               |             |___|Nbr free sec |    |   Merge SUB    |
*                               |  Wr in SUB  | No|> threshold? |    |Alloc RUB for LB|
*                               |             |   | (Note #2b)  |    |   Wr in RUB    |
*                               +-------------+   \------+------/    +----------------+
*                                                        |Yes
*                                                        | (B2d1)
*                                                 +------+------+
*                                                 |  SUB->RUB   |
*                                                 |  Wr in RUB  |
*                                                 +-------------+
*
*               (2) Various thresholds must be set, relatively to the number of sectors per block.
*                   These thresholds can be set according to application specifics to obtain best
*                   performance. Indications are mentioned for every threshold (in the configuration
*                   file, fs_dev_nand_cfg.h). These indications are guidelines and specific cases
*                   could lead to different behaviors than what is expected.
*
*                   (a) ThSecWrCnt_MergeRUBStartSUB (see A1).
*                       This threshold indicates the minimum size (in sectors) of the write operation
*                       needed to create a sequential update block (SUB) when a random update block (RUB)
*                       already exists. SUBs offer a substantial gain in merge speed when a large
*                       quantity of sectors are written sequentially (within a single or multiple write
*                       operations). However, if many SUBs are created and merged early, the device will
*                       wear faster (less sectors written between block erase operations).
*
*                       This threshold can be set as a percentage  (relative to number of sectors per
*                       block) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_PCT_MERGE_RUB_START_SUB
*
*                           Set higher than default -> Better overall wear leveling
*                           Set lower  than default -> Better overall write speed
*
*                   (b) ThSecRemCnt_ConvertSUBToRUB (see A2, B2a and B2d).
*                       This threshold indicates the minimum size (in sectors) of free space needed in a
*                       sequential update block (SUB) to convert it to a random update block (RUB). RUBs
*                       have more flexible write rules, at the expense of a longer merge time. If the
*                       SUB is near full (few free sectors remaining), the SUB will me merged and a new
*                       RUB will be started, instead of performing the conversion from SUB to RUB.
*
*                       This threshold can be set as a percentage  (relative to number of sectors per
*                       block) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB
*
*                           Set higher than default -> Better overall write speed
*                           Set lower  than default -> Better overall wear leveling
*
*                       To take advantage of this threshold in flowchart's B2d, it must be set higher
*                       than the value of ThSecGapCnt_PadSUB (see Note #2d). Otherwise, this threshold
*                       won't have any effect.
*
*                   (c) ThSecGapCnt_PadSUB (see B2b).
*                       This threshold indicates the maximum size (in sectors) that can be skipped in a
*                       sequential update block (SUB). Since each sector of a SUB must be written at a
*                       single location (sector physical index == sector logical index), it is possible
*                       to allow small gaps in the sequence. Larger gaps are more flexible, and can
*                       improve the overall merge speed, at the cost of faster wear, since some sectors
*                       are left empty between erase operations.
*
*                       This threshold can be set as a percentage  (relative to number of sectors per
*                       block) in fs_dev_nand_cfg.h :
*
*                           #define FS_NAND_CFG_TH_PCT_PAD_SUB
*
*                           Set higher than default -> Better overall write speed
*                           Set lower  than default -> Better overall wear leveling
*
*                       (1) Comparison with this threshold can assume that the gap will always be positive.
*                           Since we are in a sequential update block (SUB), and that the sector has not
*                           been written yet (see flowchart in note #1), the sector to write will
*                           systematically have an offset greater or equal than the first free sector in
*                           sequential update block (SUB).
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_SEC_QTY  FS_NAND_SecWrInUB (FS_NAND_DATA         *p_nand_data,
                                               void                 *p_src,
                                               FS_SEC_QTY            sec_ix_logical,
                                               FS_SEC_QTY            sec_cnt,
                                               FS_NAND_UB_SEC_DATA   ub_sec_data,
                                               FS_ERR               *p_err)
{
    FS_SEC_QTY               sec_base;
    FS_NAND_SEC_PER_BLK_QTY  sec_offset_logical;
    FS_NAND_SEC_PER_BLK_QTY  sec_cnt_in_ub;
    FS_NAND_UB_EXTRA_DATA    ub_extra_data;
    FS_NAND_BLK_QTY          blk_ix_logical;
    FS_NAND_UB_QTY           ub_ix;
    FS_SEC_QTY               wr_cnt;


    sec_base           = FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);
    sec_base           = FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, sec_base);
    sec_offset_logical = sec_ix_logical - sec_base;

    ub_extra_data  = p_nand_data->UB_ExtraDataTbl[ub_sec_data.UB_Ix];
    blk_ix_logical = FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);


    if (sec_offset_logical == 0u) {
                                                                /* ----------- FIRST SEC OF LOGICAL BLK (A) ----------- */
                                                                /* Chk for associated UB.                               */
        if (ub_sec_data.UB_Ix != FS_NAND_UB_IX_INVALID) {

                                                                /* -------------- FIRST SEC + UB EXISTS --------------- */
            if (ub_extra_data.AssocLvl != 0u) {

                                                                /* ----------- FIRST SEC + RUB EXISTS (A1) ------------ */
                                                                /* Chk wr cnt to determine if a SUB should be started.  */
                if (sec_cnt < p_nand_data->ThSecWrCnt_MergeRUBStartSUB) {

                                                                /* -------- FIRST SEC + RUB + LO WR CNT (A1a) --------- */
                                                                /* Chk if space is left in RUB.                         */
                    if (ub_extra_data.NextSecIx < p_nand_data->NbrSecPerBlk) {

                                                                /* ---- FIRST SEC + RUB NOT FULL + LO WR CNT (A1d) ---- */
                                                                /* Wr sec in RUB.                                       */
                        wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                    p_src,
                                                    sec_ix_logical,
                                                    sec_cnt,
                                                    ub_sec_data.UB_Ix,
                                                    p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in update block %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (wr_cnt);
                        }

                        return (wr_cnt);

                    } else {
                                                                /* ------ FIRST SEC + RUB FULL + LO WR CNT (A1c) ------ */
                                                                /* Merge RUB.                                           */
                        FS_NAND_RUB_Merge(p_nand_data, ub_sec_data.UB_Ix, p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing full merge of update block %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (0u);
                        }

                                                                /* Alloc SUB for LB.                                    */
                        ub_ix = FS_NAND_SUB_Alloc(p_nand_data, blk_ix_logical, p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error allocating new sequential update block.\r\n"));

                            return (0u);
                        }

                                                                /* Wr in SUB.                                           */
                        wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                                    p_src,
                                                    sec_ix_logical,
                                                    sec_cnt,
                                                    ub_ix,
                                                    p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing in sequential update block %u.\r\n",
                                                ub_ix));

                            return (wr_cnt);
                        }

                        return (wr_cnt);
                    }


                } else {
                                                                /* -------- FIRST SEC + RUB + HI WR CNT (A1b) --------- */
                                                                /* Partial merge RUB.                                   */
                    FS_NAND_RUB_PartialMerge(p_nand_data,
                                             ub_sec_data.UB_Ix,
                                             blk_ix_logical,
                                             p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing partial merge of update block %u (Assoc blk ix %u).\r\n",
                                            ub_sec_data.UB_Ix,
                                            ub_sec_data.AssocLogicalBlksTblIx));

                        return (0u);
                    }

                                                                /* Create SUB.                                          */
                    ub_ix = FS_NAND_SUB_Alloc(p_nand_data, blk_ix_logical, p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error allocating new sequential update block.\r\n"));

                        return (0u);
                    }

                                                                /* Wr sec in SUB.                                       */
                    wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                                p_src,
                                                sec_ix_logical,
                                                sec_cnt,
                                                ub_ix,
                                                p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in sequential update block %u.\r\n", ub_ix));

                        return (wr_cnt);
                    }

                    return (wr_cnt);
                }
            } else {
                                                                /* ----------- FIRST SEC + SUB EXISTS (A2) ------------ */
                sec_cnt_in_ub = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;
                if (sec_cnt_in_ub > p_nand_data->ThSecRemCnt_ConvertSUBToRUB) {

                                                                /* ------- FIRST SEC + SUB + HI FREE SEC (A2a) -------- */
                    FS_NAND_UB_IncAssoc(p_nand_data,            /* Convert SUB to RUB with k=1.                         */
                                        ub_sec_data.UB_Ix,
                                        ub_extra_data.AssocLogicalBlksTbl[0]);

                    p_nand_data->SUB_Cnt--;

                                                                /* Wr sec in RUB.                                       */
                    wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                p_src,
                                                sec_ix_logical,
                                                sec_cnt,
                                                ub_sec_data.UB_Ix,
                                                p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update block %u.\r\n",
                                            ub_sec_data.UB_Ix));

                        return (wr_cnt);
                    }

                    return (wr_cnt);

                } else {
                                                                /* ----- FIRST SEC + SUB + HI WR CNT OR FULL(A2b) ----- */
                    if (sec_cnt < ub_extra_data.NextSecIx) {    /* Do not merge SUB if all sec will be overwritten.     */
                        FS_NAND_SUB_Merge(p_nand_data,
                                          ub_sec_data.UB_Ix,
                                          p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing full merge of update block %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (0u);
                        }
                    }

                                                                /* Alloc SUB.                                           */
                    ub_ix = FS_NAND_SUB_Alloc(p_nand_data,
                                              blk_ix_logical,
                                              p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to allocate sequential update block.\r\n"));

                        return (0u);
                    }

                                                                /* Wr sec in SUB.                                       */
                    wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                                p_src,
                                                sec_ix_logical,
                                                sec_cnt,
                                                ub_ix,
                                                p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in sequential update block %u.\r\n",
                                            ub_ix));

                        return (wr_cnt);
                    }

                    return (wr_cnt);

                }
            }
        } else {
                                                                /* -------------- FIRST SEC + NO UB (A3) -------------- */
                                                                /* Alloc SUB.                                           */
            ub_ix = FS_NAND_SUB_Alloc(p_nand_data,
                                      blk_ix_logical,
                                      p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to allocate sequential update block.\r\n"));

                return (0u);
            }

                                                                /* Wr sec in SUB.                                       */
            wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                        p_src,
                                        sec_ix_logical,
                                        sec_cnt,
                                        ub_ix,
                                        p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in sequential update block %u.\r\n",
                                    ub_ix));

                return (wr_cnt);
            }

            return (wr_cnt);
        }
    } else {
                                                                /* --------- NOT FIRST SEC OF LOGICAL BLK (B)---------- */
                                                                /* Chk for associated UB.                               */
        if (ub_sec_data.UB_Ix != FS_NAND_UB_IX_INVALID) {

                                                                /* ----------------- NFS + UB EXISTS ------------------ */
            if (ub_extra_data.AssocLvl > 0u) {

                                                                /* -------------- NFS + RUB EXISTS (B1) --------------- */
                if (ub_extra_data.NextSecIx < p_nand_data->NbrSecPerBlk) {

                                                                /* ----------- NFS + RUB EXISTS + FREE SEC ------------ */
                                                                /* Wr sec in RUB.                                       */
                    wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                p_src,
                                                sec_ix_logical,
                                                sec_cnt,
                                                ub_sec_data.UB_Ix,
                                                p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update block %u.\r\n",
                                            ub_sec_data.UB_Ix));

                        return (wr_cnt);
                    }

                    return (wr_cnt);

                } else {
                                                                /* ------------- NFS + RUB EXISTS + FULL -------------- */
                                                                /* Merge RUB.                                           */
                    FS_NAND_RUB_Merge(p_nand_data,
                                      ub_sec_data.UB_Ix,
                                      p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing full merge of update block %u.\r\n",
                                            ub_sec_data.UB_Ix));

                        return (0u);
                    }

                                                                /* Alloc RUB for logical blk.                           */
                    ub_ix = FS_NAND_RUB_Alloc(p_nand_data,
                                              blk_ix_logical,
                                              p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to allocate new random update block.\r\n"));

                        return (0u);
                    }

                                                                /* Wr in RUB.                                           */
                    ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

                    wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                p_src,
                                                sec_ix_logical,
                                                sec_cnt,
                                                ub_ix,
                                                p_err);

                    if (*p_err != FS_ERR_NONE) {
                        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update block %u.\r\n",
                                            ub_ix));

                        return (wr_cnt);
                    }

                    return (wr_cnt);
                }

            } else {
                                                                /* -------------- NFS + SUB ALLOC'D (B2) -------------- */
                                                                /* Chk if sec is wr'en.                                 */
                if (sec_offset_logical < ub_extra_data.NextSecIx) {

                                                                /* ----------- NFS + SUB + SEC WR'EN (B2a) ------------ */
                                                                /* Compare free sec cnt with threshold.                 */
                    sec_cnt_in_ub = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;
                    if (sec_cnt_in_ub > p_nand_data->ThSecRemCnt_ConvertSUBToRUB) {

                                                                /* ---- NFS + SUB + SEC WR'EN + LO FREE SEC (B2a2) ---- */
                                                                /* Change SUB in RUB with k=1.                          */
                        FS_NAND_UB_IncAssoc(p_nand_data,
                                            ub_sec_data.UB_Ix,
                                            blk_ix_logical);

                        p_nand_data->SUB_Cnt--;

                                                                /* Wr in RUB.                                           */
                        wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                    p_src,
                                                    sec_ix_logical,
                                                    sec_cnt,
                                                    ub_sec_data.UB_Ix,
                                                    p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update blk %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (wr_cnt);
                        }

                        return (wr_cnt);
                    } else {
                                                                /* ---- NFS + SUB + SEC WR'EN + HI FREE SEC (B2a1) ---- */
                                                                /* Merge SUB.                                           */
                        FS_NAND_SUB_Merge(p_nand_data,
                                          ub_sec_data.UB_Ix,
                                          p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing merge of sequential update block %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (0u);
                        }

                                                                /* Alloc RUB for logical blk.                           */
                        ub_ix = FS_NAND_RUB_Alloc(p_nand_data,
                                                  blk_ix_logical,
                                                  p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to allocate new random update block.\r\n"));

                            return (0u);
                        }

                                                                /* Wr sec in RUB.                                       */
                        ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

                        wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                    p_src,
                                                    sec_ix_logical,
                                                    sec_cnt,
                                                    ub_ix,
                                                    p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing to random update block %u.\r\n",
                                                ub_ix));

                            return (wr_cnt);
                        }

                        return (wr_cnt);

                    }

                } else {
                                                                /* ------------ NFS + SUB + SEC FREE (B2b) ------------ */
                                                                /* Chk gap size.                                        */
                    sec_cnt_in_ub = sec_offset_logical - ub_extra_data.NextSecIx;
                                                                /* sec_cnt can't be negative (see note #2d1).           */
                    if (sec_cnt_in_ub < p_nand_data->ThSecGapCnt_PadSUB) {

                                                                /* ------ NFS + SUB + SEC FREE + SMALL GAP (B2c) ------ */
                                                                /* Pad SUB until sec with logical blk data.             */
                        FS_NAND_SUB_MergeUntil(p_nand_data,
                                               ub_sec_data.UB_Ix,
                                               sec_offset_logical - 1u,
                                               p_err);
                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error performing merge of sequential update block %u.\r\n",
                                                ub_sec_data.UB_Ix));
                            return (0u);
                        }


                                                                /* Wr sec in SUB.                                       */
                        wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                                    p_src,
                                                    sec_ix_logical,
                                                    sec_cnt,
                                                    ub_sec_data.UB_Ix,
                                                    p_err);

                        if (*p_err != FS_ERR_NONE) {
                            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update block %u.\r\n",
                                                ub_sec_data.UB_Ix));

                            return (wr_cnt);
                        }

                        return (wr_cnt);

                    } else {
                                                                /* ------ NFS + SUB + SEC FREE + LARGE GAP (B2d) ------ */
                        sec_cnt_in_ub = p_nand_data->NbrSecPerBlk - ub_extra_data.NextSecIx;


#if (FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB > FS_NAND_CFG_TH_PCT_PAD_SUB)
                        if (sec_cnt_in_ub > p_nand_data->ThSecRemCnt_ConvertSUBToRUB) {
#endif

                                                                /* ---------- [B2d] + HI FREE SEC CNT (B2d1) ---------- */
                                                                /* Change SUB in RUB with k=1.                          */
                            FS_NAND_UB_IncAssoc(p_nand_data,
                                                ub_sec_data.UB_Ix,
                                                blk_ix_logical);

                            p_nand_data->SUB_Cnt--;

                                                                /* Wr sec in RUB.                                       */
                            wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                                        p_src,
                                                        sec_ix_logical,
                                                        sec_cnt,
                                                        ub_sec_data.UB_Ix,
                                                        p_err);

                            if (*p_err != FS_ERR_NONE) {
                                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in random update block %u.\r\n",
                                                    ub_sec_data.UB_Ix));
                                return (wr_cnt);
                            }

                            return (wr_cnt);

#if (FS_NAND_CFG_TH_PCT_CONVERT_SUB_TO_RUB > FS_NAND_CFG_TH_PCT_PAD_SUB)
                        } else {

                                                                /* ---------- [B2d] + LO FREE SEC CNT (B2d2) ---------- */
                                                                /* Wr sec in SUB.                                       */
                            wr_cnt = FS_NAND_SecWrInSUB(p_nand_data,
                                                        p_src,
                                                        sec_ix_logical,
                                                        sec_cnt,
                                                        ub_sec_data.UB_Ix,
                                                        p_err);

                            if (*p_err != FS_ERR_NONE) {
                                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Error writing sector in sequential update block %u.\r\n",
                                                    ub_sec_data.UB_Ix));
                                return (wr_cnt);
                            }

                            return (wr_cnt);
                        }
#endif
                    }
                }
            }
        } else {
                                                                /* ------------- NFS + NO UB EXISTS (B3) -------------- */
                                                                /* Alloc RUB for logical blk.                           */
            ub_ix = FS_NAND_RUB_Alloc(p_nand_data,
                                      blk_ix_logical,
                                      p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to alloc random update block for logical block index %u.\r\n",
                                    blk_ix_logical));
                return (0u);
            }

                                                                /* Wr sec in RUB.                                       */
            ub_extra_data = p_nand_data->UB_ExtraDataTbl[ub_ix];

            wr_cnt = FS_NAND_SecWrInRUB(p_nand_data,
                                        p_src,
                                        sec_ix_logical,
                                        sec_cnt,
                                        ub_ix,
                                        p_err);

            if (*p_err != FS_ERR_NONE) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInUB(): Unable to write sector %u in update block.\r\n",
                                    sec_ix_logical));
                return (wr_cnt);
            }

            return (wr_cnt);
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_SecWrInRUB()
*
* Description : Write one logical sector in a random update block.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               sec_ix_logical  Sector's logical index.
*
*               sec_cnt         Number of consecutive sectors to write (unused).
*
*               ub_ix           Update block's index.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   -------------RETURNED BY FS_NAND_SecWrHandler()-------------
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*                                   FS_ERR_DEV_OP_ABORTED           Operation failed, but bad block was refreshed succesfully.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   --------------RETURNED BY FS_NAND_UB_SecFind()--------------
*                                   See FS_NAND_UB_SecFind() for additional return error codes.
*
*                                   --------------RETURNED BY FS_NAND_OOSGenSto()---------------
*                                   See FS_NAND_OOSGenSto() for additional return error codes.
*
* Return(s)   : Number of sectors written. Might not be equal to sec_cnt.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_SEC_QTY  FS_NAND_SecWrInRUB (FS_NAND_DATA     *p_nand_data,
                                                void             *p_src,
                                                FS_SEC_QTY        sec_ix_logical,
                                                FS_SEC_QTY        sec_cnt,
                                                FS_NAND_UB_QTY    ub_ix,
                                                FS_ERR           *p_err)
{
    FS_NAND_BLK_QTY            blk_ix_logical;
    FS_NAND_SEC_PER_BLK_QTY    sec_offset_logical;
    FS_NAND_UB_DATA            p_data;
    FS_NAND_UB_EXTRA_DATA     *p_extra_data;
    FS_NAND_UB_SEC_DATA        ub_sec_data;
    FS_NAND_BLK_QTY            blk_ix_phy;
    FS_NAND_BLK_QTY            blk_ix_log;
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
    CPU_SIZE_T                 loc_octet_array;
    CPU_SIZE_T                 pos_bit_array;
    CPU_DATA                   loc_bit_octet;
    FS_NAND_SEC_PER_BLK_QTY    sec_subset_ix;
#endif


    (void)sec_cnt;
    blk_ix_logical     =  FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);
    sec_offset_logical =  sec_ix_logical - FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, blk_ix_logical);
    p_extra_data       = &p_nand_data->UB_ExtraDataTbl[ub_ix];


    FS_NAND_TRACE_LOG(("Wr sector %u in RUB %u at sec offset %u.\r\n",
                        sec_ix_logical,
                        ub_ix,
                        p_extra_data->NextSecIx));

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_extra_data->NextSecIx >= p_nand_data->NbrSecPerBlk) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInRUB(): overflow!!.\r\n"));
    }
#endif

                                                                /* ----------------- FIND OLD UB SEC ------------------ */
    ub_sec_data.AssocLogicalBlksTblIx = FS_NAND_RUB_AssocBlkIxGet(p_nand_data, ub_ix, blk_ix_logical);

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ub_sec_data.AssocLogicalBlksTblIx == FS_NAND_ASSOC_BLK_IX_INVALID) {
       *p_err = FS_ERR_DEV_INVALID_METADATA;
        return (0u);
    }
#endif

    ub_sec_data.UB_Ix                 = ub_ix;

    ub_sec_data = FS_NAND_UB_SecFind(p_nand_data,
                                     ub_sec_data,
                                     sec_offset_logical,
                                     p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecWrInRUB(): Error searching for sector with logical offset %u in update blk %u.\r\n",
                            sec_offset_logical,
                            ub_sec_data.UB_Ix));

        return (0u);
    }

    do {                                                        /* Until sec wr'en successfully.                        */
                                                                /* ------------------ CALC OOS DATA ------------------- */
        blk_ix_log = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix);
        blk_ix_phy = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_log);
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
            FS_NAND_TRACE_DBG(("FS_NAND_SecWrInRUB(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
           *p_err = FS_ERR_DEV_IO;
            return (0u);
        }
#endif
        FS_NAND_OOSGenSto(p_nand_data,
                          p_nand_data->OOS_BufPtr,
                          blk_ix_logical,
                          blk_ix_phy,
                          sec_offset_logical,
                          p_extra_data->NextSecIx,
                          p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

                                                                /* ---------------------- WR SEC ---------------------- */
        blk_ix_log = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix);
        FS_NAND_SecWrHandler(p_nand_data,
                             p_src,
                             p_nand_data->OOS_BufPtr,
                             blk_ix_log,
                             p_extra_data->NextSecIx,
                             p_err);
    } while (*p_err == FS_ERR_DEV_OP_ABORTED);

    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

                                                                /* ----------------- UPDATE METADATA ------------------ */
                                                                /* Update sec valid map.                                */
    p_data = FS_NAND_UB_TblEntryRd(p_nand_data,
                                   ub_ix);

    FSUtil_MapBitSet(p_data.SecValidBitMap, p_extra_data->NextSecIx);

                                                                /* Invalidate old UB sector.                            */
    if (ub_sec_data.SecOffsetPhy != FS_NAND_SEC_OFFSET_IX_INVALID) {
        FSUtil_MapBitClr(p_data.SecValidBitMap, ub_sec_data.SecOffsetPhy);
    }

    FS_NAND_UB_TblInvalidate(p_nand_data);


#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
                                                                /* Update UB mapping tbl.                               */
    pos_bit_array  = ub_sec_data.AssocLogicalBlksTblIx * p_nand_data->NbrSecPerBlk * p_nand_data->UB_SecMapNbrBits;
    pos_bit_array += sec_offset_logical * p_nand_data->UB_SecMapNbrBits;

    FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

    sec_subset_ix = p_extra_data->NextSecIx / FS_NAND_CFG_UB_TBL_SUBSET_SIZE;

    FSUtil_ValPack32(p_extra_data->LogicalToPhySecMap,
                    &loc_octet_array,
                    &loc_bit_octet,
                     sec_subset_ix,
                     p_nand_data->UB_SecMapNbrBits);
#endif

#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
                                                                /* Update UB meta cache.                                */
    pos_bit_array = p_extra_data->NextSecIx * (p_nand_data->RUB_MaxAssocLog2 + p_nand_data->NbrSecPerBlkLog2);

    FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

    FSUtil_ValPack32(p_extra_data->MetaCachePtr,
                    &loc_octet_array,
                    &loc_bit_octet,
                     sec_offset_logical,
                     p_nand_data->NbrSecPerBlkLog2);

    FSUtil_ValPack32(p_extra_data->MetaCachePtr,
                    &loc_octet_array,
                    &loc_bit_octet,
                     ub_sec_data.AssocLogicalBlksTblIx,
                     p_nand_data->RUB_MaxAssocLog2);
#endif

    p_extra_data->NextSecIx++;

    p_extra_data->ActivityCtr = p_nand_data->ActivityCtr;       /* Assign current activity ctr to UB.                   */

    p_nand_data->ActivityCtr++;                                 /* Inc global activity ctr.                             */

    return (1u);
}
#endif


/*
*********************************************************************************************************
*                                         FS_NAND_SecWrInSUB()
*
* Description : Write 1 or more logical sectors in a sequential update block (SUB).
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_src           Pointer to source buffer.
*               -----           Argument validated by caller.
*
*               sec_ix_logical  Sector's logical index.
*
*               sec_cnt         Number of consecutive sectors to write.
*
*               ub_ix           Update block's index.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   -------------RETURNED BY FS_NAND_SecWrHandler()-------------
*                                   FS_ERR_DEV_INVALID_METADATA     Metadata block could not be found.
*                                   FS_ERR_DEV_INVALID_OP           Bad blocks table is full.
*                                   FS_ERR_DEV_OP_ABORTED           Operation failed, but bad block was refreshed succesfully.
*                                   FS_ERR_ECC_UNCORR               Uncorrectable ECC error.
*                                   FS_ERR_NONE                     Sector written successfully.
*
*                                   --------------RETURNED BY FS_NAND_OOSGenSto()---------------
*                                   See FS_NAND_OOSGenSto() for additional return error codes.
*
* Return(s)   : Number of sectors written. Might not be equal to sec_cnt.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  FS_SEC_QTY  FS_NAND_SecWrInSUB (FS_NAND_DATA    *p_nand_data,
                                                void            *p_src,
                                                FS_SEC_QTY       sec_ix_logical,
                                                FS_SEC_QTY       sec_cnt,
                                                FS_NAND_UB_QTY   ub_ix,
                                                FS_ERR          *p_err)
{
    FS_NAND_BLK_QTY                 data_blk_ix_logical;
    FS_NAND_BLK_QTY                 ub_ix_logical;
    FS_NAND_SEC_PER_BLK_QTY         sec_offset_phy;
    FS_SEC_QTY                      wr_cnt;
    FS_NAND_UB_DATA                 p_data;
    FS_NAND_UB_EXTRA_DATA          *p_extra_data;
    FS_NAND_BLK_QTY                 blk_ix_phy;
#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
    CPU_SIZE_T                      loc_octet_array;
    CPU_SIZE_T                      pos_bit_array;
    CPU_DATA                        loc_bit_octet;
    FS_NAND_SEC_PER_BLK_QTY         sec_subset_ix;
#endif


    data_blk_ix_logical =  FS_NAND_SEC_IX_TO_BLK_IX(p_nand_data, sec_ix_logical);
    sec_offset_phy      =  sec_ix_logical - FS_NAND_BLK_IX_TO_SEC_IX(p_nand_data, data_blk_ix_logical);
    wr_cnt              =  0u;
    p_extra_data        = &p_nand_data->UB_ExtraDataTbl[ub_ix];


    while ((wr_cnt         < sec_cnt) &&                        /* Until all sec are wr'en or blk is full.              */
           (sec_offset_phy < p_nand_data->NbrSecPerBlk)) {

        FS_NAND_TRACE_LOG(("Wr sector %u in SUB %u at sec offset %u.\r\n",
                            sec_ix_logical + wr_cnt,
                            ub_ix,
                            sec_offset_phy));


        do {                                                    /* Until sec wr'en successfully.                        */
                                                                /* ------------------ CALC OOS DATA ------------------- */
            blk_ix_phy = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix);
            blk_ix_phy = FS_NAND_BlkIxPhyGet(p_nand_data, blk_ix_phy);
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
            if (blk_ix_phy == FS_NAND_BLK_IX_INVALID) {
                FS_NAND_TRACE_DBG(("FS_NAND_SecWrInSUB(): Index of physical block (blk_ix_phy) is invalid.\r\n"));
               *p_err = FS_ERR_DEV_IO;
                return (0u);
            }
#endif
            FS_NAND_OOSGenSto(p_nand_data,
                              p_nand_data->OOS_BufPtr,
                              data_blk_ix_logical,
                              blk_ix_phy,
                              sec_offset_phy,
                              sec_offset_phy,
                              p_err);
            if (*p_err != FS_ERR_NONE) {
                return (0u);
            }

                                                                /* ---------------------- WR SEC ---------------------- */
            ub_ix_logical = FS_NAND_UB_IX_TO_LOG_BLK_IX(p_nand_data, ub_ix);
            FS_NAND_SecWrHandler(p_nand_data,
                                 p_src,
                                 p_nand_data->OOS_BufPtr,
                                 ub_ix_logical,
                                 sec_offset_phy,
                                 p_err);
        } while (*p_err == FS_ERR_DEV_OP_ABORTED);

        if (*p_err != FS_ERR_NONE) {
            return (wr_cnt);
        }
                                                                /* ----------------- UPDATE METADATA ------------------ */
                                                                /* Update sec valid map.                                */
        p_data = FS_NAND_UB_TblEntryRd(p_nand_data,
                                       ub_ix);

        FSUtil_MapBitSet(p_data.SecValidBitMap, sec_offset_phy);

        FS_NAND_UB_TblInvalidate(p_nand_data);

#if (FS_NAND_CFG_UB_TBL_SUBSET_SIZE != 0)
                                                                /* Update UB mapping tbl.                               */
        pos_bit_array = sec_offset_phy * p_nand_data->UB_SecMapNbrBits;

        FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

        sec_subset_ix = sec_offset_phy / FS_NAND_CFG_UB_TBL_SUBSET_SIZE;

        FSUtil_ValPack32(p_extra_data->LogicalToPhySecMap,
                        &loc_octet_array,
                        &loc_bit_octet,
                         sec_subset_ix,
                         p_nand_data->UB_SecMapNbrBits);
#endif

#if (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
                                                                /* Update UB meta cache.                                */
        pos_bit_array = sec_offset_phy * (p_nand_data->RUB_MaxAssocLog2 + p_nand_data->NbrSecPerBlkLog2);

        FS_UTIL_BITMAP_LOC_GET(pos_bit_array, loc_octet_array, loc_bit_octet);

        FSUtil_ValPack32(p_extra_data->MetaCachePtr,
                        &loc_octet_array,
                        &loc_bit_octet,
                         sec_offset_phy,
                         p_nand_data->NbrSecPerBlkLog2);

        FSUtil_ValPack32(p_extra_data->MetaCachePtr,
                        &loc_octet_array,
                        &loc_bit_octet,
                         0u,
                         p_nand_data->RUB_MaxAssocLog2);
#endif

        sec_offset_phy++;
        p_extra_data->NextSecIx = sec_offset_phy;

        wr_cnt++;
        p_src = (void *)((CPU_INT08U *)p_src + p_nand_data->SecSize);


    }

    p_extra_data->ActivityCtr = p_nand_data->ActivityCtr;       /* Assign current activity ctr to UB.                   */

    p_nand_data->ActivityCtr++;                                 /* Inc global activity ctr.                             */

    return (wr_cnt);
}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_OOSGenSto()
*
* Description : Generate out of sector data for storage sector.
*
* Argument(s) : p_nand_data             Pointer to NAND data.
*               -----------             Argument validated by caller.
*
*               p_oos_buf_v             Pointer to buffer that will receive OOS data.
*
*               blk_ix_logical_data     Logical block index associated with data sector.
*
*               blk_ix_phy              Physical block index of block that will store the data sector.
*
*               sec_offset_logical      Sector offset relative to logical block.
*
*               sec_offset_phy          Sector offset relative to physical block.
*
*               p_err                   Pointer to variable that will receive return the error code from this function :
*               -----                   Argument validated by caller.
*
*                                           FS_ERR_NONE     Sector written successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_OOSGenSto (FS_NAND_DATA             *p_nand_data,
                                         void                     *p_oos_buf_v,
                                         FS_NAND_BLK_QTY           blk_ix_logical_data,
                                         FS_NAND_BLK_QTY           blk_ix_phy,
                                         FS_NAND_SEC_PER_BLK_QTY   sec_offset_logical,
                                         FS_NAND_SEC_PER_BLK_QTY   sec_offset_phy,
                                         FS_ERR                   *p_err)
{
    CPU_INT08U               *p_oos_buf_08;
    FS_NAND_SEC_TYPE_STO      sec_type;
    FS_NAND_ERASE_QTY         erase_cnt;


    (void)p_err;
    p_oos_buf_08 = (CPU_INT08U *)p_oos_buf_v;


    Mem_Set(&p_oos_buf_08[FS_NAND_OOS_SEC_USED_OFFSET],         /* Sec used mark.                                       */
             0x00u,
             p_nand_data->UsedMarkSize);

                                                                /* Sec type.                                            */
    sec_type = FS_NAND_SEC_TYPE_STORAGE;
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf_08[FS_NAND_OOS_SEC_TYPE_OFFSET],
                                 &sec_type,
                                  sizeof(FS_NAND_SEC_TYPE_STO));

                                                                /* Blk ix logical.                                      */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf_08[FS_NAND_OOS_STO_LOGICAL_BLK_IX_OFFSET],
                                 &blk_ix_logical_data,
                                  sizeof(FS_NAND_BLK_QTY));

                                                                /* Blk sec ix.                                          */
    MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf_08[FS_NAND_OOS_STO_BLK_SEC_IX_OFFSET],
                                 &sec_offset_logical,
                                  sizeof(FS_NAND_SEC_PER_BLK_QTY));

                                                                /* Erase cnt.                                           */
    if (sec_offset_phy == 0u) {
                                                                /* Find entry in avail blk tbl.                         */
        erase_cnt = FS_NAND_BlkRemFromAvail(p_nand_data, blk_ix_phy);

                                                                /* Wr erase cnt in OOS.                                 */
        MEM_VAL_COPY_SET_INTU_LITTLE(&p_oos_buf_08[FS_NAND_OOS_ERASE_CNT_OFFSET],
                                     &erase_cnt,
                                      sizeof(FS_NAND_ERASE_QTY));
    }
}
#endif


/*
*********************************************************************************************************
*                                       FS_NAND_BlkEraseHandler()
*
* Description : Erase a block from device.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               blk_ix_phy      Block to delete's index.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_IO       Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT  Device timeout.
*                                   FS_ERR_NONE         Sector read successfully.
*
*                                   -----RETURNED BY p_ctrlr_api->BlkErase()-----
*                                   See p_ctrlr_api->BlkErase() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Erased blocks must have their erase count saved in the available blocks table prior
*                   to calling this function.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_NAND_INTERN  void  FS_NAND_BlkEraseHandler (FS_NAND_DATA     *p_nand_data,
                                               FS_NAND_BLK_QTY   blk_ix_phy,
                                               FS_ERR           *p_err)
{
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    void               *p_ctrlr_data;


    FS_NAND_TRACE_LOG(("FS_NAND_BlkEraseHandler(): Erase block %u.\r\n",
                        blk_ix_phy));


    p_ctrlr_api  = p_nand_data->CtrlrPtr;
    p_ctrlr_data = p_nand_data->CtrlrDataPtr;

    p_ctrlr_api->BlkErase(p_ctrlr_data,
                          blk_ix_phy,
                          p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_NAND_TRACE_DBG(("FS_NAND_BlkEraseHandler(): Error erasing block %u.\r\n",
                            blk_ix_phy));
        return;
    }
}
#endif


/*
*********************************************************************************************************
*                                          FS_NAND_SecIsUsed()
*
* Description : Determine if specified sector is used.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               sec_ix_phy      Physical sector index.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE     Device info calculated.
*
*                                   --RETURNED BY p_ctrlr_api->OOSRdRaw()--
*                                   See p_ctrlr_api->OOSRdRaw() for additional return error codes.
*
* Return(s)   : DEF_YES, if sector is used,
*               DEF_NO , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_NAND_INTERN  CPU_BOOLEAN  FS_NAND_SecIsUsed (FS_NAND_DATA  *p_nand_data,
                                                FS_SEC_NBR     sec_ix_phy,
                                                FS_ERR        *p_err)
{
    FS_NAND_CTRLR_API  *p_ctrlr_api;
    CPU_INT32U          sec_used_mark;
    CPU_INT08U          set_bit_cnt;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (sizeof(sec_used_mark) < p_nand_data->UsedMarkSize) {
        FS_NAND_TRACE_DBG(("FS_NAND_SecIsUsed(): Local variable sec_used_mark is too small to contain an appropriate used mark.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return (DEF_YES);
    }
#endif

    sec_used_mark = 0;


    p_ctrlr_api = p_nand_data->CtrlrPtr;

    p_ctrlr_api->OOSRdRaw(p_nand_data->CtrlrDataPtr,
                         &sec_used_mark,
                          sec_ix_phy,
                          FS_NAND_OOS_SEC_USED_OFFSET,
                          p_nand_data->UsedMarkSize,
                          p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }

                                                                /* Chk used mark.                                       */
    set_bit_cnt = CRCUtil_PopCnt_32(sec_used_mark);
    if (set_bit_cnt < (p_nand_data->UsedMarkSize * DEF_INT_08_NBR_BITS / 2u)) {
        return (DEF_YES);
    } else {
        return (DEF_NO);
    }
}


/*
*********************************************************************************************************
*                                          FS_NAND_CtrlrReg()
*
* Description : Register a controller implementation module and initialize it if necessary.
*
* Argument(s) : p_ctrlr_api     Pointer to controller module API to register.
*
*               p_err           p_err           Pointer to variable that will receive the return error code from this function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_CFG  Invalid cfg, max nbr of controller modules exceeded.
*
*                                   ----------------------RETURNED BY p_ctrlr_api->Init()-----------------------
*                                   See p_ctrlr_api->Init() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_CtrlrReg (FS_NAND_CTRLR_API  *p_ctrlr_api,
                                        FS_ERR             *p_err)
{
    CPU_BOOLEAN  ctrlr_impl_init;
    CPU_INT08U   ctrlr_impl_ix;
    CPU_INT08U   ctrlr_impl_free_slot_ix;
    CPU_SR_ALLOC();


    ctrlr_impl_init         = DEF_NO;
    ctrlr_impl_free_slot_ix = (CPU_INT08U)-1;
    CPU_CRITICAL_ENTER();
    for (ctrlr_impl_ix = 0u; ctrlr_impl_ix < FS_NAND_CFG_MAX_CTRLR_IMPL; ctrlr_impl_ix++) {
        if (FS_NAND_ListCtrlrImpl[ctrlr_impl_ix] == p_ctrlr_api) {
            ctrlr_impl_init = DEF_YES;
        } else {
            if (FS_NAND_ListCtrlrImpl[ctrlr_impl_ix] == DEF_NULL) {
                ctrlr_impl_free_slot_ix = ctrlr_impl_ix;
            }
        }
    }

    if (ctrlr_impl_init == DEF_NO) {                        /* Need to init ctrlr impl -- add to list.              */
        if (ctrlr_impl_free_slot_ix != (CPU_INT08U)-1) {
            FS_NAND_ListCtrlrImpl[ctrlr_impl_free_slot_ix] = p_ctrlr_api;
            if (ctrlr_impl_init == DEF_NO) {
                p_ctrlr_api->Init(p_err);
                if (*p_err != FS_ERR_NONE) {
                    CPU_CRITICAL_EXIT();
                    return;
                }
            }
        } else {
            CPU_CRITICAL_EXIT();
            FS_NAND_TRACE_DBG(("FS_NAND_Open(): Maximum number of NAND controller implementations exceeded.\r\n"));
           *p_err = FS_ERR_DEV_INVALID_CFG;
            return;
        }
    }
    CPU_CRITICAL_EXIT();
}

/*
*********************************************************************************************************
*                                         FS_NAND_CalcDevInfo()
*
* Description : Calculate NAND device data.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_PARAMS   Device low-level parameters invalid.
*                                   FS_ERR_NONE                     Device info calculated.
*
* Return(s)   : none.
*
* Note(s)     : (1) Block & block region information supplied by the phy driver are strictly checked :
*                   (a) At least one block must exist.
*                   (b) The last block number should not exceed the maximum blks number.
*                   (c) The block size must NOT be smaller than the page   size.
*                   (d) The page  size must NOT be smaller than the sector size.
*                   (e) The block size MUST be a multiple of the    page   size.
*                   (f) The page  size MUST be a multiple of the    sector size.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_CalcDevInfo (FS_NAND_DATA  *p_nand_data,
                                           FS_ERR        *p_err)
{
    FS_SEC_QTY  nbr_sec;
    CPU_INT32S  nbr_blk;
    CPU_INT08U  sec_per_pg;
    FS_SEC_QTY  sec_per_blk;


                                                                /* --------- CALC NBR OF LOGICAL BLK TO REPORT -------- */
    nbr_blk  = p_nand_data->BlkCnt;
    nbr_blk -= p_nand_data->PartDataPtr->MaxBadBlkCnt;          /* Potential bad blks.                                  */
    nbr_blk -= p_nand_data->UB_CntMax;                          /* UBs.                                                 */
    nbr_blk -= FS_NAND_HDR_BLK_NBR;                             /* Hdr blk.                                             */
    nbr_blk -= FS_NAND_VALID_META_BLK_NBR;                      /* Meta blk.                                            */
    nbr_blk -= FS_NAND_CFG_RSVD_AVAIL_BLK_CNT;                  /* Rsvd avail blks.                                     */
    nbr_blk -= p_nand_data->RUB_MaxAssoc;                       /* Blks needed for largest merge ops.                   */

    if (nbr_blk <= 0) {
        FS_NAND_TRACE_DBG(("FS_NAND_CalcDevInfo(): There are not enough blks accessible by uC/FS.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_SIZE;
        return;
    }


    sec_per_pg  = (CPU_INT08U)(p_nand_data->PartDataPtr->PgSize / p_nand_data->SecSize);
    sec_per_blk = sec_per_pg * p_nand_data->PartDataPtr->PgPerBlk;
    nbr_sec     = (CPU_INT32U)nbr_blk    * sec_per_blk;

    p_nand_data->SecCnt = nbr_sec;
}


/*
*********************************************************************************************************
*                                        FS_NAND_AllocDevData()
*
* Description : Allocate NAND device data.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_MEM_ALLOC    Memory could not be allocated.
*                                   FS_ERR_NONE         Device data allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The size of the available blocks table must be inferior or equal to the size of
*                       one sector. This restriction is necessary to insure that committing the available
*                       blocks table on device is an atomic operation that could not be half-way through
*                       if an unexpected power-loss occurs. The user can either set sector size to a
*                       higher value, or reduce the number of entries in the available blocks table.
*
*                   (b) The available blocks table must be located at the beginning of the metadata and
*                       its size restricted to one sector to make the search for it trivial : it will
*                       always be contained in the first sector of metadata.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_AllocDevData (FS_NAND_DATA  *p_nand_data,
                                            FS_ERR        *p_err)
{
    CPU_SIZE_T              octets_reqd;
    LIB_ERR                 alloc_err;
    CPU_INT16U              meta_invalid_map_size;
    CPU_INT16U              blk_bitmap_size;
    CPU_INT16U              bad_blk_tbl_size;
    CPU_INT16U              avail_blk_tbl_size;
    CPU_INT08U              avail_blk_tbl_entry_size;
    CPU_INT16U              ub_bitmap_size;
    CPU_INT16U              total_metadata_size;
    CPU_INT16U              blk_sec_bitmap_size;
    FS_NAND_BLK_QTY         tbl_ix;
    FS_NAND_UB_EXTRA_DATA  *p_tbl_entry;
    CPU_SIZE_T              ub_sec_map_size;
    CPU_SIZE_T              ub_meta_cache_size;


                                                                /* ------------------- ALLOC L2P TBL ------------------ */
    p_nand_data->LogicalToPhyBlkMap = (FS_NAND_BLK_QTY *)Mem_HeapAlloc(sizeof(FS_NAND_BLK_QTY) * p_nand_data->LogicalDataBlkCnt,
                                                                       sizeof(CPU_DATA),
                                                                      &octets_reqd,
                                                                      &alloc_err);

    if (p_nand_data->LogicalToPhyBlkMap == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for L2P tbl: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

                                                                /* ------------- DETERMINE METADATA INFO -------------- */
                                                                /* Avail blk tbl at beginning of meta (see Note #1b).   */
    avail_blk_tbl_entry_size           = sizeof(FS_NAND_BLK_QTY) + sizeof(FS_NAND_ERASE_QTY);
    avail_blk_tbl_size                 = p_nand_data->AvailBlkTblEntryCntMax * avail_blk_tbl_entry_size;
    p_nand_data->MetaOffsetAvailBlkTbl = 0u;


    if (avail_blk_tbl_size > p_nand_data->SecSize) {            /* Check avail blk tbl size (see Note #1a).             */
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Available blocks table size(%u) exceeds sector size(%u) (sizes in octets).\r\n",
                            avail_blk_tbl_size,
                            p_nand_data->SecSize));
       *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return;
    }

                                                                /* Bad blk tbl.                                         */
    bad_blk_tbl_size                   =  p_nand_data->PartDataPtr->MaxBadBlkCnt * sizeof(FS_NAND_BLK_QTY);
    p_nand_data->MetaOffsetBadBlkTbl   =  avail_blk_tbl_size;


                                                                /* Dirty blk bitmap.                                    */
    blk_bitmap_size                    =  FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->BlkCnt);

#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN   ==  DEF_ENABLED)
    p_nand_data->DirtyBitmapSize       =  blk_bitmap_size;
#endif

    p_nand_data->MetaOffsetDirtyBitmap =  p_nand_data->MetaOffsetBadBlkTbl + bad_blk_tbl_size;

                                                                /* Update blk sec bitmaps.                              */
    blk_sec_bitmap_size                =  FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->NbrSecPerBlk);

    ub_bitmap_size                     =  blk_sec_bitmap_size + sizeof(FS_NAND_BLK_QTY);
    ub_bitmap_size                    *=  p_nand_data->UB_CntMax;
    p_nand_data->MetaOffsetUB_Tbl      =  p_nand_data->MetaOffsetDirtyBitmap + blk_bitmap_size;

                                                                /* Check total size needed                              */
    total_metadata_size                =  bad_blk_tbl_size +
                                          avail_blk_tbl_size +
                                          blk_bitmap_size +
                                          ub_bitmap_size;

    p_nand_data->MetaSize              =  total_metadata_size;

    p_nand_data->MetaSecCnt            =  total_metadata_size / p_nand_data->SecSize;
    p_nand_data->MetaSecCnt           += (total_metadata_size % p_nand_data->SecSize == 0u) ? 0u : 1u;

                                                                /* -------------- ALLOC METADATA CACHE ---------------- */
    p_nand_data->MetaCache = (CPU_INT08U *)Mem_HeapAlloc(total_metadata_size,
                                                         sizeof(CPU_DATA),
                                                        &octets_reqd,
                                                        &alloc_err);
    if (p_nand_data->MetaCache == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for metadata cache: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

                                                                /* ------------ ALLOC DIRTY BITMAP CACHE -------------- */
#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN == DEF_ENABLED)
    p_nand_data->DirtyBitmapCache = (CPU_INT08U *)Mem_HeapAlloc(blk_bitmap_size,
                                                                sizeof(CPU_DATA),
                                                               &octets_reqd,
                                                               &alloc_err);
    if (p_nand_data->DirtyBitmapCache == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for dirty bitmap cache: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }
#endif


                                                                /* ---------- ALLOC AVAIL BLK TBL COMMIT MAP ---------- */
    p_nand_data->AvailBlkTblCommitMap = (CPU_INT08U *)Mem_HeapAlloc(FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->AvailBlkTblEntryCntMax),
                                                                    sizeof(CPU_INT08U),
                                                                   &octets_reqd,
                                                                   &alloc_err);
    if (p_nand_data->AvailBlkTblCommitMap == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for available blocks table commit map: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }


    p_nand_data->AvailBlkMetaMap = (CPU_INT08U *)Mem_HeapAlloc(FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->AvailBlkTblEntryCntMax),
                                                               sizeof(CPU_INT08U),
                                                               &octets_reqd,
                                                               &alloc_err);
    if (p_nand_data->AvailBlkMetaMap == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for available blocks meta map: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }


    p_nand_data->AvailBlkMetaID_Tbl = (FS_NAND_META_ID *)Mem_HeapAlloc(p_nand_data->AvailBlkTblEntryCntMax * sizeof(FS_NAND_META_ID),
                                                                       sizeof(FS_NAND_META_ID),
                                                                       &octets_reqd,
                                                                       &alloc_err);
    if (p_nand_data->AvailBlkMetaID_Tbl == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for available blocks meta ID table: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }


                                                                /* ------------ ALLOC METADATA INVALID MAP ------------ */
    meta_invalid_map_size  =  p_nand_data->MetaSecCnt / DEF_OCTET_NBR_BITS;
    meta_invalid_map_size += (p_nand_data->MetaSecCnt % DEF_OCTET_NBR_BITS) == 0u ? 0u : 1u;
    p_nand_data->MetaBlkInvalidSecMap = (CPU_INT08U *)Mem_HeapAlloc(sizeof(CPU_INT08U) * meta_invalid_map_size,
                                                                    sizeof(CPU_DATA),
                                                                   &octets_reqd,
                                                                   &alloc_err);
    if (p_nand_data->MetaBlkInvalidSecMap == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for metadata invalid sector bitmap: %d octets req'd.\r\n",
                            octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }


                                                                /* ---------------- ALLOC SECTOR BUF ------------------ */
    p_nand_data->BufPtr = Mem_HeapAlloc(sizeof(CPU_INT08U) * p_nand_data->SecSize,
                                        FS_CFG_BUF_ALIGN_OCTETS,
                                       &octets_reqd,
                                       &alloc_err);

    if (p_nand_data->BufPtr == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for data buf: %d octets req'd.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }


                                                                /* ------------- ALLOC UB EXTRA DATA TBL -------------- */
    p_nand_data->UB_ExtraDataTbl = (FS_NAND_UB_EXTRA_DATA *)Mem_HeapAlloc(sizeof(FS_NAND_UB_EXTRA_DATA) * p_nand_data->UB_CntMax,
                                                                          sizeof(CPU_DATA),
                                                                         &octets_reqd,
                                                                         &alloc_err);
    if (p_nand_data->UB_ExtraDataTbl == DEF_NULL) {
        FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for update block extra data table: %d octets req'd.\r\n",
                            octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    ub_sec_map_size     = p_nand_data->NbrSecPerBlk * p_nand_data->UB_SecMapNbrBits;
    ub_sec_map_size    *= p_nand_data->RUB_MaxAssoc;
    ub_sec_map_size     = FS_UTIL_BIT_NBR_TO_OCTET_NBR(ub_sec_map_size);

    ub_meta_cache_size  = p_nand_data->NbrSecPerBlkLog2 + p_nand_data->RUB_MaxAssocLog2;
    ub_meta_cache_size *= p_nand_data->NbrSecPerBlk;
    ub_meta_cache_size  = FS_UTIL_BIT_NBR_TO_OCTET_NBR(ub_meta_cache_size);


                                                                /* For each entry...                                    */
    for (tbl_ix = 0u; tbl_ix < p_nand_data->UB_CntMax; tbl_ix++) {
        p_tbl_entry = &p_nand_data->UB_ExtraDataTbl[tbl_ix];

                                                                /* Alloc associated blk tbl.                            */
        p_tbl_entry->AssocLogicalBlksTbl = (FS_NAND_BLK_QTY *)Mem_HeapAlloc(sizeof(FS_NAND_BLK_QTY) * p_nand_data->RUB_MaxAssoc,
                                                                            sizeof(CPU_DATA),
                                                                           &octets_reqd,
                                                                           &alloc_err);
        if (p_tbl_entry->AssocLogicalBlksTbl == DEF_NULL) {
            FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for UB %u's associated blocks table: %d octets req'd.\r\n",
                                tbl_ix,
                                octets_reqd));
           *p_err = FS_ERR_MEM_ALLOC;
            return;
        }


                                                                /* Init associated blk tbl.                             */
        Mem_Set(&p_tbl_entry->AssocLogicalBlksTbl[0], 0xFFu, sizeof(FS_NAND_BLK_QTY) * p_nand_data->RUB_MaxAssoc);

        if (ub_sec_map_size != 0u) {                            /* If size if non-null ...                              */
                                                                /* Alloc sec mapping tbl.                               */
            p_tbl_entry->LogicalToPhySecMap = (CPU_INT08U *)Mem_HeapAlloc(sizeof(CPU_INT08U) * ub_sec_map_size,
                                                                          sizeof(CPU_DATA),
                                                                         &octets_reqd,
                                                                         &alloc_err);
            if (p_tbl_entry->LogicalToPhySecMap == DEF_NULL) {
                FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for UB %u's sector mapping table: %d octets req'd.\r\n",
                                    tbl_ix,
                                    octets_reqd));
               *p_err = FS_ERR_MEM_ALLOC;
                return;
            }

                                                                /* Init sec mapping tbl.                                */
            Mem_Set(&p_tbl_entry->LogicalToPhySecMap[0], 0x00u, sizeof(CPU_INT08U) * ub_sec_map_size);
        } else {
            p_tbl_entry->LogicalToPhySecMap = DEF_NULL;
        }


                                                                /* Alloc meta cache.                                    */
#if  (FS_NAND_CFG_UB_META_CACHE_EN == DEF_ENABLED)
        p_tbl_entry->MetaCachePtr = (CPU_INT08U *)Mem_HeapAlloc(sizeof(CPU_INT08U) * ub_meta_cache_size,
                                                                sizeof(CPU_DATA),
                                                               &octets_reqd,
                                                               &alloc_err);
        if (p_tbl_entry->MetaCachePtr == DEF_NULL) {
            FS_NAND_TRACE_DBG(("FS_NAND_AllocDevData(): Could not alloc mem for UB %u's metadata cache: %d octets req'd.\r\n",
                                tbl_ix,
                                octets_reqd));
           *p_err = FS_ERR_MEM_ALLOC;
            return;
        }
#endif

    }

                                                                /* ------------------ INIT DEV DATA ------------------- */
    FS_NAND_InitDevData(p_nand_data);
}


/*
*********************************************************************************************************
*                                        FS_NAND_InitDevData()
*
* Description : Initialize NAND device data.
*
* Argument(s) : p_nand_data     Pointer to NAND data.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE         Device data allocated.
*                                   FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) All blocks located in the available block table at mount time are considered as
*                   potential stale metadata blocks (see also FS_NAND_MetaBlkParse()) to avoid metadata
*                   block ID overflow issues.
*
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_InitDevData (FS_NAND_DATA  *p_nand_data)
{
    CPU_INT08U              *p_buf;
    FS_NAND_UB_QTY           tbl_ix;
    FS_NAND_UB_EXTRA_DATA   *p_tbl_entry;
    CPU_INT16U               meta_invalid_map_size;
    CPU_INT16U               blk_sec_bitmap_size;
    CPU_INT16U               ub_sec_map_size;
    CPU_INT16U               ub_meta_cache_size;


                                                                /* ---------------- INIT NAND DEV DATA ---------------- */
    p_nand_data->AvailBlkTblInvalidated = DEF_NO;
    p_nand_data->SUB_Cnt                = 0u;
    p_nand_data->ActivityCtr            = 0u;
    p_nand_data->DirtyBitmapSrchPos     = 0u;

                                                                /* ------------------- INIT L2P TBL ------------------- */
    Mem_Set((void *)p_nand_data->LogicalToPhyBlkMap, 0xff, sizeof(FS_NAND_BLK_QTY) * p_nand_data->LogicalDataBlkCnt);

                                                                /* ---------------- INIT AVAIL BLK TBL ---------------- */
    p_buf = &p_nand_data->MetaCache[p_nand_data->MetaOffsetAvailBlkTbl];
    Mem_Set(&p_buf[0], 0xFFu, p_nand_data->MetaOffsetBadBlkTbl - 0u);

                                                                /* ----------------- INIT BAD BLK TBL ----------------- */
    p_buf = &p_nand_data->MetaCache[p_nand_data->MetaOffsetBadBlkTbl];
    Mem_Set(&p_buf[0], 0xFFu, p_nand_data->MetaOffsetDirtyBitmap - p_nand_data->MetaOffsetBadBlkTbl);


                                                                /* ---------------- INIT DIRTY BITMAP ----------------- */
    p_buf = &p_nand_data->MetaCache[p_nand_data->MetaOffsetDirtyBitmap];
    Mem_Set(&p_buf[0], 0x00u, p_nand_data->MetaOffsetUB_Tbl - p_nand_data->MetaOffsetDirtyBitmap);

#if (FS_NAND_CFG_DIRTY_MAP_CACHE_EN == DEF_ENABLED)
    p_buf = &p_nand_data->DirtyBitmapCache[0u];
    Mem_Set(&p_buf[0], 0x00u, p_nand_data->MetaOffsetUB_Tbl - p_nand_data->MetaOffsetDirtyBitmap);
#endif

                                                                /* ------------------- INIT UB TBL -------------------- */
    blk_sec_bitmap_size  =  p_nand_data->NbrSecPerBlk / 8u;
    blk_sec_bitmap_size += (p_nand_data->NbrSecPerBlk % 8u) == 0u ? 0u : 1u;
    p_buf = &p_nand_data->MetaCache[p_nand_data->MetaOffsetUB_Tbl];
    for (tbl_ix = 0u; tbl_ix < p_nand_data->UB_CntMax; tbl_ix++) {
        Mem_Set(&p_buf[tbl_ix * (blk_sec_bitmap_size + sizeof(FS_NAND_BLK_QTY))],
                 0xFFu,
                 sizeof(FS_NAND_BLK_QTY));
        Mem_Set(&p_buf[tbl_ix * (blk_sec_bitmap_size + sizeof(FS_NAND_BLK_QTY)) + sizeof(FS_NAND_BLK_QTY)],
                 0x00u,
                 blk_sec_bitmap_size);
    }

                                                                /* -------------- INIT META INVALID MAP --------------- */
    meta_invalid_map_size  =  p_nand_data->MetaSecCnt / DEF_OCTET_NBR_BITS;
    meta_invalid_map_size += (p_nand_data->MetaSecCnt % DEF_OCTET_NBR_BITS) == 0u ? 0u : 1u;

    Mem_Clr((void *)p_nand_data->MetaBlkInvalidSecMap, meta_invalid_map_size);

                                                                /* -------------- INIT UB EXTRA DATA TBL -------------- */

    ub_sec_map_size     = p_nand_data->NbrSecPerBlk * p_nand_data->UB_SecMapNbrBits;
    ub_sec_map_size    *= p_nand_data->RUB_MaxAssoc;
    ub_sec_map_size     = FS_UTIL_BIT_NBR_TO_OCTET_NBR(ub_sec_map_size);

    ub_meta_cache_size  = p_nand_data->NbrSecPerBlkLog2 + p_nand_data->RUB_MaxAssocLog2;
    ub_meta_cache_size *= p_nand_data->NbrSecPerBlk;
    ub_meta_cache_size  = FS_UTIL_BIT_NBR_TO_OCTET_NBR(ub_meta_cache_size);


                                                                /* For each entry...                                    */
    for (tbl_ix = 0u; tbl_ix < p_nand_data->UB_CntMax; tbl_ix++) {
        p_tbl_entry = &p_nand_data->UB_ExtraDataTbl[tbl_ix];

        p_tbl_entry->AssocLvl    = 0u;
        p_tbl_entry->NextSecIx   = 0u;
        p_tbl_entry->ActivityCtr = 0u;

                                                                /* Init associated blk tbl.                             */
        Mem_Set(&p_tbl_entry->AssocLogicalBlksTbl[0], 0xFFu, sizeof(FS_NAND_BLK_QTY) * p_nand_data->RUB_MaxAssoc);

        if (ub_sec_map_size != 0u) {                            /* If size if non-null ...                              */
                                                                /* Init sec mapping tbl.                                */
            Mem_Set(&p_tbl_entry->LogicalToPhySecMap[0], 0x00u, sizeof(CPU_INT08U) * ub_sec_map_size);
        } else {
            p_tbl_entry->LogicalToPhySecMap = DEF_NULL;
        }
    }

                                                                /* See Note #1.                                         */
    Mem_Set(p_nand_data->AvailBlkMetaMap, 0xFF, FS_UTIL_BIT_NBR_TO_OCTET_NBR(p_nand_data->AvailBlkTblEntryCntMax));
}

/*
*********************************************************************************************************
*                                         FS_NAND_DataFree()
*
* Description : Free a NAND data object.
*
* Argument(s) : p_nand_data   Pointer to a nand data object.
*               -----------   Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) FS_NAND_ListFreePtr and FS_NAND_UnitCtr MUST ALWAYS be accessed excluively in
*                   critical sections.
*********************************************************************************************************
*/

FS_NAND_INTERN  void  FS_NAND_DataFree (FS_NAND_DATA  *p_nand_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_nand_data->NextPtr = FS_NAND_ListFreePtr;                 /* Add to free pool.                                    */
    FS_NAND_ListFreePtr  = p_nand_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                        FS_NAND_DataGet()
*
* Description : Allocate & initialize a NAND data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a NAND data object, if NO errors.
*               Null pointer                 , otherwise.
*
* Note(s)     : (1) FS_NAND_ListFreePtr and FS_NAND_UnitCtr MUST ALWAYS be accessed excluively in
*                   critical sections.
*********************************************************************************************************
*/

FS_NAND_INTERN  FS_NAND_DATA  *FS_NAND_DataGet (void)
{
    LIB_ERR        alloc_err;
    CPU_SIZE_T     octets_reqd;
    FS_NAND_DATA  *p_nand_data;
    CPU_SR_ALLOC();


                                                                /* -------------------- ALLOC DATA -------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_NAND_ListFreePtr == DEF_NULL) {
        p_nand_data = (FS_NAND_DATA *)Mem_HeapAlloc(sizeof(FS_NAND_DATA),
                                                    sizeof(CPU_DATA),
                                                   &octets_reqd,
                                                   &alloc_err);
        if (p_nand_data == DEF_NULL) {
            CPU_CRITICAL_EXIT();
            FS_NAND_TRACE_DBG(("FS_NAND_DataGet(): Could not alloc mem for NAND data: %d octets required.\r\n", octets_reqd));
            return (DEF_NULL);
        }
        (void)alloc_err;

                                                                /* -------------------- INIT DATA --------------------- */
#if ((FS_CFG_CTR_STAT_EN == DEF_ENABLED) || \
     (FS_CFG_CTR_ERR_EN  == DEF_ENABLED))
        FS_NAND_CtrsTbl[FS_NAND_UnitCtr] = &p_nand_data->Ctrs;
#endif
        FS_NAND_UnitCtr++;


    } else {
        p_nand_data         = FS_NAND_ListFreePtr;
        FS_NAND_ListFreePtr = FS_NAND_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* --------------------- CLR DATA --------------------- */
    Mem_Clr(p_nand_data, sizeof(FS_NAND_DATA));

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)                         /* Clr stat ctrs.                                       */
    p_nand_data->Ctrs.StatRdCtr               = 0u;
    p_nand_data->Ctrs.StatWrCtr               = 0u;

    p_nand_data->Ctrs.StatMetaSecCommitCtr    = 0u;
    p_nand_data->Ctrs.StatSUB_MergeCtr        = 0u;
    p_nand_data->Ctrs.StatRUB_MergeCtr        = 0u;
    p_nand_data->Ctrs.StatRUB_PartialMergeCtr = 0u;

    p_nand_data->Ctrs.StatBlkRefreshCtr       = 0u;
#endif

#if (FS_CFG_CTR_ERR_EN == DEF_ENABLED)                          /* Clr err ctrs.                                        */
    p_nand_data->Ctrs.ErrRefreshDataLoss      = 0u;
#endif

    p_nand_data->NextPtr   = DEF_NULL;

    return (p_nand_data);
}

#endif
