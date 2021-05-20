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
*                                       SHORT FILE NAME SUPPORT
*
* Filename : fs_fat_sfn.h
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*********************************************************************************************************
*/

#ifndef  FS_FAT_SFN_H
#define  FS_FAT_SFN_H

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_err.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_FAT_SFN_MODULE
#define  FS_FAT_SFN_EXT
#else
#define  FS_FAT_SFN_EXT  extern
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


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  FS_FAT_FN_API  FS_FAT_SFN_API;                   /* Short file name API.                                 */


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

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_SFN_DirEntryFmt(void              *p_dir_entry,    /* Fmt dir entry for SFN.                               */
                             CPU_INT32U         name_8_3[],
                             CPU_BOOLEAN        is_dir,
                             FS_FAT_CLUS_NBR    file_first_clus,
                             CPU_BOOLEAN        name_lower_case,
                             CPU_BOOLEAN        ext_lower_case);
#endif

void  FS_FAT_SFN_Chk        (CPU_CHAR          *name,           /* Chk file name as SFN.                                */
                             FS_FILE_NAME_LEN  *p_name_len,
                             FS_ERR            *p_err);

void  FS_FAT_SFN_Create     (CPU_CHAR          *name,           /* Create SFN.                                          */
                             CPU_INT32U         name_8_3[],
                             CPU_BOOLEAN       *p_name_lower_case,
                             CPU_BOOLEAN       *p_ext_lower_case,
                             FS_ERR            *p_err);

void  FS_FAT_SFN_SFN_Find   (FS_VOL            *p_vol,          /* Srch dir for SFN dir entry.                          */
                             FS_BUF            *p_buf,
                             CPU_INT32U         name_8_3[],
                             FS_FAT_DIR_POS    *p_dir_start_pos,
                             FS_FAT_DIR_POS    *p_dir_end_pos,
                             FS_ERR            *p_err);

void  FS_FAT_SFN_LabelGet   (FS_VOL            *p_vol,          /* Get volume label.                                    */
                             CPU_CHAR          *label,
                             FS_ERR            *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_SFN_LabelSet   (FS_VOL            *p_vol,          /* Set volume label.                                    */
                             CPU_CHAR          *label,
                             FS_ERR            *p_err);
#endif


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
