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
*                                            ENTRY ACCESS
*
* Filename : fs_fat_entry.h
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

#ifndef  FS_FAT_ENTRY_H
#define  FS_FAT_ENTRY_H

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

#ifdef   FS_FAT_ENTRY_MODULE
#define  FS_FAT_ENTRY_EXT
#else
#define  FS_FAT_ENTRY_EXT  extern
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
*                                  SYSTEM DRIVER FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void      FS_FAT_EntryAttribSet(FS_VOL         *p_vol,          /* Set file or directory's attributes.                  */
                                CPU_CHAR       *name_entry,
                                FS_FLAGS        attrib,
                                FS_ERR         *p_err);

void      FS_FAT_EntryCreate   (FS_VOL         *p_vol,          /* Create a file or directory.                          */
                                CPU_CHAR       *name_entry,
                                FS_FLAGS        entry_type,
                                CPU_BOOLEAN     excl,
                                FS_ERR         *p_err);

void      FS_FAT_EntryDel      (FS_VOL         *p_vol,          /* Delete a file or directory.                          */
                                CPU_CHAR       *name_entry,
                                FS_FLAGS        entry_type,
                                FS_ERR         *p_err);
#endif

void      FS_FAT_EntryQuery    (FS_VOL         *p_vol,          /* Delete a file or directory.                          */
                                CPU_CHAR       *name_entry,
                                FS_ENTRY_INFO  *p_info,
                                FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void      FS_FAT_EntryRename   (FS_VOL         *p_vol,          /* Rename a file or directory.                          */
                                CPU_CHAR       *name_entry_old,
                                CPU_CHAR       *name_entry_new,
                                CPU_BOOLEAN     excl,
                                FS_ERR         *p_err);

void      FS_FAT_EntryTimeSet  (FS_VOL         *p_vol,          /* Set file or directory's date/time.                   */
                                CPU_CHAR       *name_entry,
                                CLK_DATE_TIME  *p_time,
                                CPU_INT08U      time_type,
                                FS_ERR         *p_err);
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
