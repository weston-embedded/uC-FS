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
*                                     FILE SYSTEM FAT MANAGEMENT
*
*                                                TYPES
*
* Filename : fs_fat_type.h
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE GUARD
*********************************************************************************************************
*/

#ifndef  FS_FAT_TYPE_H
#define  FS_FAT_TYPE_H


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_err.h"
#include  "../Source/fs_type.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_FAT_TYPE_MODULE
#define  FS_FAT_TYPE_EXT
#else
#define  FS_FAT_TYPE_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_fat_data       FS_FAT_DATA;
typedef  struct  fs_fat_file_data  FS_FAT_FILE_DATA;

typedef  CPU_INT32U  FS_FAT_CLUS_NBR;                           /* Number of clusters/cluster index.                    */
typedef  CPU_INT32U  FS_FAT_SEC_NBR;                            /* Number of sectors/sector index.                      */
typedef  CPU_INT16U  FS_FAT_DATE;                               /* FAT date.                                            */
typedef  CPU_INT16U  FS_FAT_TIME;                               /* FAT time.                                            */
typedef  CPU_INT08U  FS_FAT_DIR_ENTRY_QTY;                      /* Quantity of directory entries.                       */
typedef  CPU_INT32U  FS_FAT_FILE_SIZE;                          /* Size of file, in octets.                             */


/*
*********************************************************************************************************
*                                   FAT FORMAT CONFIGURATION DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_fat_sys_cfg {
    FS_SEC_QTY        ClusSize;
    FS_FAT_SEC_NBR    RsvdAreaSize;
    CPU_INT16U        RootDirEntryCnt;
    CPU_INT08U        FAT_Type;
    CPU_INT08U        NbrFATs;
} FS_FAT_SYS_CFG;

/*
*********************************************************************************************************
*                                  FAT DIRECTORY ENTRY POSITION TYPE
*********************************************************************************************************
*/

typedef  struct  fs_fat_dir_pos {
    FS_FAT_SEC_NBR    SecNbr;
    FS_SEC_SIZE       SecPos;
} FS_FAT_DIR_POS;

/*
*********************************************************************************************************
*                                         FAT TYPE API DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_fat_type_api {
#if (FS_CFG_RD_ONLY_EN      == DEF_DISABLED)
    void             (*ClusValWr)       (FS_VOL            *p_vol,
                                         FS_BUF            *p_buf,
                                         FS_FAT_CLUS_NBR    clus,
                                         FS_FAT_CLUS_NBR    val,
                                         FS_ERR           *p_err);
#endif

    FS_FAT_CLUS_NBR  (*ClusValRd)       (FS_VOL            *p_vol,
                                         FS_BUF            *p_buf,
                                         FS_FAT_CLUS_NBR    clus,
                                         FS_ERR            *p_err);

    FS_FAT_CLUS_NBR    ClusBad;
    FS_FAT_CLUS_NBR    ClusEOF;
    FS_FAT_CLUS_NBR    ClusFree;
    CPU_CHAR           FileSysType[9];
} FS_FAT_TYPE_API;

/*
*********************************************************************************************************
*                                       FILE NAME API DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_fat_fn_api {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    void             (*DirEntryCreate)  (FS_VOL            *p_vol,      /* Create dir entry.                            */
                                         FS_BUF            *p_buf,
                                         CPU_CHAR          *name,
                                         CPU_BOOLEAN        is_dir,
                                         FS_FAT_CLUS_NBR    file_first_clus,
                                         FS_FAT_DIR_POS    *p_dir_start_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);

    void             (*DirEntryCreateAt)(FS_VOL            *p_vol,      /* Create dir entry at sec pos.                 */
                                         FS_BUF            *p_buf,
                                         CPU_CHAR          *name,
                                         FS_FILE_NAME_LEN   name_len,
                                         CPU_INT32U         name_8_3[],
                                         CPU_BOOLEAN        is_dir,
                                         FS_FAT_CLUS_NBR    file_first_clus,
                                         FS_FAT_DIR_POS    *p_dir_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);

    void             (*DirEntryDel)     (FS_VOL            *p_vol,      /* Del dir entry.                               */
                                         FS_BUF            *p_buf,
                                         FS_FAT_DIR_POS    *p_dir_start_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);
#endif

    void             (*DirEntryFind)    (FS_VOL            *p_vol,      /* Srch dir for dir entry.                      */
                                         FS_BUF            *p_buf,
                                         CPU_CHAR          *name,
                                         CPU_CHAR         **p_name_next,
                                         FS_FAT_DIR_POS    *p_dir_start_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);

    void             (*NextDirEntryGet) (FS_VOL            *p_vol,      /* Get next dir entry in dir.                   */
                                         FS_BUF            *p_buf,
                                         void              *name,
                                         FS_FAT_DIR_POS    *p_dir_start_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);
} FS_FAT_FN_API;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of FAT module include (see Note #1).             */
#endif
