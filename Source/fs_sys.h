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
*                                FILE SYSTEM SYSTEM DRIVER MANAGEMENT
*
* Filename : fs_sys.h
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_SYS_H
#define  FS_SYS_H


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <Source/clk.h>
#include  "fs_cfg_fs.h"
#include  "fs_dir.h"
#include  "fs_err.h"
#include  "fs_file.h"
#include  "fs_type.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_SYS_MODULE
#define  FS_SYS_EXT
#else
#define  FS_SYS_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        SYSTEM INFO DATA TYPE
*********************************************************************************************************
*/

struct  fs_sys_info {
    FS_SEC_QTY  BadSecCnt;                                      /* Nbr of bad  sectors on volume.                       */
    FS_SEC_QTY  FreeSecCnt;                                     /* Nbr of free sectors on volume.                       */
    FS_SEC_QTY  UsedSecCnt;                                     /* Nbr of used sectors on volume.                       */
    FS_SEC_QTY  TotSecCnt;                                      /* Tot nbr of  sectors on volume.                       */
};


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

void        FSSys_ModuleInit    (FS_QTY          vol_cnt,       /* Initialize system driver.                            */
                                 FS_QTY          file_cnt,
                                 FS_QTY          dir_cnt,
                                 FS_ERR         *p_err);


                                                                /* ----------------- VOLUME FUNCTIONS ----------------- */
void        FSSys_VolClose      (FS_VOL         *p_vol);        /* Close a volume.                                      */

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void        FSSys_VolFmt        (FS_VOL         *p_vol,         /* Create a volume.                                     */
                                 void           *p_sys_cfg,
                                 FS_SEC_SIZE     sec_size,
                                 FS_SEC_QTY      size,
                                 FS_ERR         *p_err);
#endif

void        FSSys_VolLabelGet   (FS_VOL         *p_vol,         /* Get volume label.                                    */
                                 CPU_CHAR       *label,
                                 CPU_SIZE_T      len_max,
                                 FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void        FSSys_VolLabelSet   (FS_VOL         *p_vol,         /* Set volume label.                                    */
                                 CPU_CHAR       *label,
                                 FS_ERR         *p_err);
#endif

void        FSSys_VolOpen       (FS_VOL         *p_vol,         /* Open a volume.                                       */
                                 FS_ERR         *p_err);

void        FSSys_VolQuery      (FS_VOL         *p_vol,         /* Get info about a volume.                             */
                                 FS_SYS_INFO    *p_info,
                                 FS_ERR         *p_err);


                                                                /* ------------------ FILE FUNCTIONS ------------------ */
void        FSSys_FileClose     (FS_FILE        *p_file,        /* Close a file.                                        */
                                 FS_ERR         *p_err);

void        FSSys_FileOpen      (FS_FILE        *p_file,        /* Open a file.                                         */
                                 CPU_CHAR       *name_file,
                                 FS_ERR         *p_err);

void        FSSys_FilePosSet    (FS_FILE        *p_file,        /* Set file position indicator.                         */
                                 FS_FILE_SIZE    pos_new,
                                 FS_ERR         *p_err);

void        FSSys_FileQuery     (FS_FILE        *p_file,        /* Get info about file.                                 */
                                 FS_ENTRY_INFO  *p_info,
                                 FS_ERR         *p_err);

CPU_SIZE_T  FSSys_FileRd        (FS_FILE        *p_file,        /* Read from a file.                                    */
                                 void           *p_dest,
                                 CPU_SIZE_T      size,
                                 FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void        FSSys_FileTruncate  (FS_FILE        *p_file,        /* Truncate a file.                                     */
                                 FS_FILE_SIZE    size,
                                 FS_ERR         *p_err);
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_SIZE_T  FSSys_FileWr        (FS_FILE        *p_file,        /* Write to a file.                                     */
                                 void           *p_src,
                                 CPU_SIZE_T      size,
                                 FS_ERR         *p_err);
#endif

#ifdef FS_DIR_MODULE_PRESENT
                                                                /* ---------------- DIRECTORY FUNCTIONS --------------- */
void        FSSys_DirClose      (FS_DIR         *p_dir);        /* Close a directory.                                   */

void        FSSys_DirOpen       (FS_DIR         *p_dir,         /* Open a directory.                                    */
                                 CPU_CHAR       *name_dir,
                                 FS_ERR         *p_err);

void        FSSys_DirRd         (FS_DIR         *p_dir,         /* Read a  directory entry.                             */
                                 FS_DIR_ENTRY   *p_dir_entry,
                                 FS_ERR         *p_err);
#endif

                                                                /* ------------------ ENTRY FUNCTIONS ----------------- */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void        FSSys_EntryAttribSet(FS_VOL         *p_vol,         /* Set file or directory's attributes.                  */
                                 CPU_CHAR       *name_entry,
                                 FS_FLAGS        attrib,
                                 FS_ERR         *p_err);

void        FSSys_EntryCreate   (FS_VOL         *p_vol,         /* Create a file or directory.                          */
                                 CPU_CHAR       *name_entry,
                                 FS_FLAGS        entry_type,
                                 CPU_BOOLEAN     excl,
                                 FS_ERR         *p_err);

void        FSSys_EntryDel      (FS_VOL         *p_vol,         /* Delete a file or directory.                          */
                                 CPU_CHAR       *name_entry,
                                 FS_FLAGS        entry_type,
                                 FS_ERR         *p_err);
#endif

void        FSSys_EntryQuery    (FS_VOL         *p_vol,         /* Delete a file or directory.                          */
                                 CPU_CHAR       *name_entry,
                                 FS_ENTRY_INFO  *p_info,
                                 FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void        FSSys_EntryRename   (FS_VOL         *p_vol,         /* Rename a file or directory.                          */
                                 CPU_CHAR       *name_entry_old,
                                 CPU_CHAR       *name_entry_new,
                                 CPU_BOOLEAN     excl,
                                 FS_ERR         *p_err);

void        FSSys_EntryTimeSet  (FS_VOL         *p_vol,         /* Set file or directory's date/time.                   */
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
*********************************************************************************************************
*/

#endif
