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
*                                     FILE SYSTEM FAT MANAGEMENT
*
*                                            FAT12 SUPPORT
*
* Filename : fs_fat_fat12.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_FAT12_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_sys.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat_fat12.h"
#include  "fs_fat_type.h"
#include  "fs_fat_journal.h"
#include  "fs_fat.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_FAT12_MODULE_PRESENT                            /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_FAT12_CLUS_BAD                        0x0FF7u
#define  FS_FAT_FAT12_CLUS_EOF                        0x0FF8u
#define  FS_FAT_FAT12_CLUS_FREE                       0x0000u


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
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
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void             FS_FAT_FAT12_ClusValWr       (FS_VOL           *p_vol,     /* Write value into cluster.        */
                                                       FS_BUF           *p_buf,
                                                       FS_FAT_CLUS_NBR   clus,
                                                       FS_FAT_CLUS_NBR   val,
                                                       FS_ERR           *p_err);
#endif

static  FS_FAT_CLUS_NBR  FS_FAT_FAT12_ClusValRd       (FS_VOL           *p_vol,     /* Read value from cluster.         */
                                                       FS_BUF           *p_buf,
                                                       FS_FAT_CLUS_NBR   clus,
                                                       FS_ERR           *p_err);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_FAT_TYPE_API  FS_FAT_FAT12_API = {
#if (FS_CFG_RD_ONLY_EN      == DEF_DISABLED)
    FS_FAT_FAT12_ClusValWr,
#endif
    FS_FAT_FAT12_ClusValRd,

    FS_FAT_FAT12_CLUS_BAD,
    FS_FAT_FAT12_CLUS_EOF,
    FS_FAT_FAT12_CLUS_FREE,
    FS_FAT_BS_FAT12_FILESYSTYPE
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        FS_FAT_FAT12_ClusValWr()
*
* Description : Write value into cluster.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               clus        Cluster to modify.
*
*               val         Value to write into cluster.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    Cluster written.
*                               FS_ERR_DEV     Device error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_FAT12_ClusValWr (FS_VOL           *p_vol,
                                      FS_BUF           *p_buf,
                                      FS_FAT_CLUS_NBR   clus,
                                      FS_FAT_CLUS_NBR   val,
                                      FS_ERR           *p_err)
{
    FS_SEC_SIZE       fat_offset;
    FS_FAT_SEC_NBR    fat_sec;
    FS_SEC_SIZE       fat_sec_offset;
    FS_FAT_SEC_NBR    fat_start_sec;
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   val_temp;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;

    fat_start_sec  =  p_fat_data->FAT1_Start;
    fat_offset     = (FS_SEC_SIZE)clus + ((FS_SEC_SIZE)clus / 2u);
    fat_sec        =  fat_start_sec + (FS_FAT_SEC_NBR)FS_UTIL_DIV_PWR2(fat_offset, p_fat_data->SecSizeLog2);
    fat_sec_offset =  fat_offset & (p_fat_data->SecSize - 1u);

    if (fat_sec_offset == p_fat_data->SecSize - 1u) {           /* -------------------- WR (SPLIT) -------------------- */
        FSBuf_Set(p_buf,                                        /* Rd 1st FAT sec.                                      */
                  fat_sec,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (FS_UTIL_IS_ODD(clus) == DEF_YES) {
            val_temp  = MEM_VAL_GET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset));
            val_temp &= DEF_NIBBLE_MASK;
            val_temp |= (val & DEF_NIBBLE_MASK) << DEF_NIBBLE_NBR_BITS;
        } else {
            val_temp  = val;
        }
                                                                /* Wr clus val.                                         */
        MEM_VAL_SET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset), val_temp & DEF_OCTET_MASK);

        FSBuf_MarkDirty(p_buf, p_err);                          /* Wr 1st FAT sec.                                      */
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        FSBuf_Set(p_buf,                                        /* Rd 2nd FAT sec.                                      */
                  fat_sec + 1u,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
                                                                /* Wr clus val.                                         */
        if (FS_UTIL_IS_ODD(clus) == DEF_YES) {
            val_temp  =  val >> DEF_NIBBLE_NBR_BITS;
        } else {
            val_temp  =  MEM_VAL_GET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + 0u));
            val_temp &= ~DEF_NIBBLE_MASK;
            val_temp |= (val >> DEF_OCTET_NBR_BITS) & DEF_NIBBLE_MASK;
        }

        MEM_VAL_SET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + 0u), val_temp);

        FSBuf_MarkDirty(p_buf, p_err);                          /* Wr 2nd FAT sec.                                      */
        if (*p_err != FS_ERR_NONE) {
            return;
        }



    } else {                                                    /* ------------------ WR (NOT SPLIT) ------------------ */
        FSBuf_Set(p_buf,                                        /* Rd FAT sec.                                          */
                  fat_sec,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        val_temp = MEM_VAL_GET_INT16U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset));
        if (FS_UTIL_IS_ODD(clus) == DEF_YES) {
            val_temp  = val_temp & 0x000Fu;
            val     <<= DEF_NIBBLE_NBR_BITS;
        } else {
            val_temp  = val_temp & 0xF000u;
        }
        val_temp |= val;
                                                                /* Wr clus val.                                         */
        MEM_VAL_SET_INT16U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset), val_temp);

        FSBuf_MarkDirty(p_buf, p_err);                          /* Wr FAT sec.                                          */
    }
}
#endif


/*
*********************************************************************************************************
*                                        FS_FAT_FAT12_ClusValRd()
*
* Description : Read value from cluster.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               clus        Cluster to modify.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    Cluster read.
*                               FS_ERR_DEV     Device error.
*
* Return(s)   : Cluster value.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_FAT_CLUS_NBR  FS_FAT_FAT12_ClusValRd (FS_VOL           *p_vol,
                                                 FS_BUF           *p_buf,
                                                 FS_FAT_CLUS_NBR   clus,
                                                 FS_ERR           *p_err)
{
    FS_SEC_SIZE       fat_offset;
    FS_FAT_SEC_NBR    fat_sec;
    FS_SEC_SIZE       fat_sec_offset;
    FS_FAT_SEC_NBR    fat_start_sec;
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   val;
    FS_FAT_CLUS_NBR   val_lo;
    FS_FAT_CLUS_NBR   val_hi;
    FS_FAT_CLUS_NBR   val_temp;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;

    fat_start_sec  =  p_fat_data->FAT1_Start;
    fat_offset     = (FS_SEC_SIZE)clus + ((FS_SEC_SIZE)clus / 2u);
    fat_sec        =  fat_start_sec + (FS_FAT_SEC_NBR)FS_UTIL_DIV_PWR2(fat_offset, p_fat_data->SecSizeLog2);
    fat_sec_offset =  fat_offset & (p_fat_data->SecSize - 1u);

    if (fat_sec_offset == p_fat_data->SecSize - 1u) {           /* -------------------- RD (SPLIT) -------------------- */
        FSBuf_Set(p_buf,                                        /* Rd 1st FAT sec.                                      */
                  fat_sec,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
                                                                /* Rd clus val.                                         */
        val_lo = MEM_VAL_GET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset));

        FSBuf_Set(p_buf,                                        /* Rd 2nd FAT sec.                                      */
                  fat_sec + 1u,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
                                                                /* Rd clus val.                                         */
        val_hi  = MEM_VAL_GET_INT08U((void *)((CPU_INT08U *)p_buf->DataPtr + 0u));

        if (FS_UTIL_IS_ODD(clus) == DEF_YES) {
            val_lo  = (val_lo >> DEF_NIBBLE_NBR_BITS) & DEF_NIBBLE_MASK;
            val     = val_lo | (val_hi << DEF_NIBBLE_NBR_BITS);
        } else {
            val_hi &= DEF_NIBBLE_MASK;
            val     = val_lo | (val_hi << DEF_OCTET_NBR_BITS);
        }



    } else {                                                    /* ------------------ RD (NOT SPLIT) ------------------ */
        FSBuf_Set(p_buf,                                        /* Rd FAT sec.                                          */
                  fat_sec,
                  FS_VOL_SEC_TYPE_MGMT,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
                                                                /* Rd clus val.                                         */
        val_temp = MEM_VAL_GET_INT16U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + fat_sec_offset));
        if (FS_UTIL_IS_ODD(clus) == DEF_YES) {
            val = (val_temp & 0xFFF0u) >> DEF_NIBBLE_NBR_BITS;
        } else {
            val = (val_temp & 0x0FFFu);
        }
    }

    return (val);
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_fat.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of FAT12 module include (see Note #1).           */
