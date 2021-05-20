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
*                                             JOURNALING
*
* Filename : fs_fat_journal.h
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*
*           (2) The following FAT-module-present configuration value MUST be pre-#define'd in
*               'fs_cfg_fs.h' PRIOR to all other file system modules that require FAT Journal Layer
*               Configuration (see 'fs_cfg_fs.h  FAT LAYER CONFIGURATION  Note #2b') :
*
*                   FS_FAT_JOURNAL_MODULE_PRESENT
*********************************************************************************************************
*/

#ifndef  FS_FAT_JOURNAL_H
#define  FS_FAT_JOURNAL_H

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */
#ifdef   FS_FAT_JOURNAL_MODULE_PRESENT                          /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_FAT_JOURNAL_MODULE
#define  FS_FAT_JOURNAL_EXT
#else
#define  FS_FAT_JOURNAL_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_err.h"
#include  "fs_fat_type.h"


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

void             FS_FAT_JournalOpen               (CPU_CHAR              *name_vol,     /* Open  journal.               */
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalClose              (CPU_CHAR              *name_vol,     /* Close journal.               */
                                                   FS_ERR                *p_err);


void             FS_FAT_JournalStart              (CPU_CHAR              *name_vol,     /* Start journaling.            */
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalStop               (CPU_CHAR              *name_vol,     /* Stop  journaling.            */
                                                   FS_ERR                *p_err);

/*
*********************************************************************************************************
*                                    INTERNAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void             FS_FAT_JournalModuleInit         (FS_QTY                 vol_cnt,      /* Init FAT journal module.     */
                                                   FS_ERR                *p_err);


                                                                                        /* ------- JOURNAL CTRL ------- */
void             FS_FAT_JournalInit               (FS_VOL                *p_vol,        /* Init journal.                */
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalExit               (FS_VOL                *p_vol,        /* Exit journal.                */
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalClr                (FS_VOL                *p_vol,        /* Clr  journal.                */
                                                   FS_BUF                *p_buf,
                                                   FS_FAT_FILE_SIZE       start_pos,
                                                   FS_FAT_FILE_SIZE       len,
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalClrAllReset        (FS_VOL                *p_vol,        /* Clr  journal entirely.       */
                                                   FS_BUF                *p_buf,
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalClrReset           (FS_VOL                *p_vol,        /* Clr  journal up to cur pos.  */
                                                   FS_BUF                *p_buf,
                                                   FS_ERR                *p_err);

                                                                                        /* ------- JOURNAL LOGS ------- */
void             FS_FAT_JournalEnterClusChainAlloc(FS_VOL                *p_vol,        /* Enter clus chain alloc log.  */
                                                   FS_BUF                *p_buf,
                                                   FS_FAT_CLUS_NBR        start_clus,
                                                   CPU_BOOLEAN            is_new,
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalEnterClusChainDel  (FS_VOL                *p_vol,        /* Enter clus chain del log.    */
                                                   FS_BUF                *p_buf,
                                                   FS_FAT_CLUS_NBR        start_clus,
                                                   FS_FAT_CLUS_NBR        nbr_clus,
                                                   CPU_BOOLEAN            del_first,
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalEnterEntryCreate   (FS_VOL                *p_vol,        /* Enter entry create log.      */
                                                   FS_BUF                *p_buf,
                                                   FS_FAT_DIR_POS        *p_dir_start_pos,
                                                   FS_FAT_DIR_POS        *p_dir_end_pos,
                                                   FS_ERR                *p_err);

void             FS_FAT_JournalEnterEntryUpdate   (FS_VOL                *p_vol,        /* Enter entry update log.      */
                                                   FS_BUF                *p_buf,
                                                   FS_FAT_DIR_POS        *p_dir_start_pos,
                                                   FS_FAT_DIR_POS        *p_dir_end_pos,
                                                   FS_ERR                *p_err);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of journaling module include (see Note #2).      */
#endif                                                          /* End of FAT        module include (see Note #1).      */
#endif
