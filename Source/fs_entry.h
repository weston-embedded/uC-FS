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
*                                      FILE SYSTEM ENTRY ACCESS
*
* Filename : fs_entry.h
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_ENTRY_H
#define  FS_ENTRY_H


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/clk.h>
#include  <cpu.h>
#include  "fs_cfg_fs.h"
#include  "fs_err.h"
#include  "fs_type.h"


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_ENTRY_MODULE
#define  FS_ENTRY_EXT
#else
#define  FS_ENTRY_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       ENTRY ATTRIBUTE DEFINES
*********************************************************************************************************
*/

#define  FS_ENTRY_ATTRIB_NONE                    DEF_BIT_NONE
#define  FS_ENTRY_ATTRIB_RD                      DEF_BIT_00     /* Entry is readable.                                   */
#define  FS_ENTRY_ATTRIB_WR                      DEF_BIT_01     /* Entry is writeable.                                  */
#define  FS_ENTRY_ATTRIB_HIDDEN                  DEF_BIT_02     /* Entry is hidden from user-level processes.           */
#define  FS_ENTRY_ATTRIB_DIR                     DEF_BIT_06     /* Entry is a directory.                                */
#define  FS_ENTRY_ATTRIB_DIR_ROOT                DEF_BIT_07     /* Entry is a root directory.                           */


                                                                /* DEF_BIT_00 not used to avoid confusion with ...      */
                                                                /* ... old parameter that could take the values ...     */
                                                                /* ... of DEF_NO or DEF_YES.                            */

#define  FS_ENTRY_TYPE_FILE                      DEF_BIT_01     /* Entry can be a file                                  */
#define  FS_ENTRY_TYPE_DIR                       DEF_BIT_02     /* Entry can be a directory                             */
                                                                /* Entry can be a directory or a file                   */
#define  FS_ENTRY_TYPE_ANY                      (DEF_BIT_01 | DEF_BIT_02)


#define  FS_DATE_TIME_CREATE                     DEF_BIT_00
#define  FS_DATE_TIME_MODIFY                     DEF_BIT_01
#define  FS_DATE_TIME_ACCESS                     DEF_BIT_02
#define  FS_DATE_TIME_ALL                        DEF_BIT_03

/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/

#define  FS_IS_VALID_DATE_TIME_TYPE(time_type)       (((time_type >= DEF_BIT_00) && (time_type <= DEF_BIT_03)) ? DEF_YES : DEF_NO)

/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        ENTRY INFO DATA TYPE
*********************************************************************************************************
*/

struct  fs_entry_info {
    FS_FLAGS      Attrib;                                       /* Entry attributes.                                    */
    FS_FILE_SIZE  Size;                                         /* File size in octets.                                 */
    CLK_TS_SEC    DateTimeCreate;                               /* Date/time of creation.                               */
    CLK_TS_SEC    DateAccess;                                   /* Date of last access.                                 */
    CLK_TS_SEC    DateTimeWr;                                   /* Date/time of last write.                             */
    FS_SEC_QTY    BlkCnt;                                       /* Number of blocks allocated for file.                 */
    FS_SEC_SIZE   BlkSize;                                      /* Block size.                                          */
};


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void      FSEntry_AttribSet(CPU_CHAR       *name_full,          /* Set a file or directory's attributes.                */
                            FS_FLAGS        attrib,
                            FS_ERR         *p_err);

void      FSEntry_Copy     (CPU_CHAR       *name_full_src,      /* Copy a file.                                         */
                            CPU_CHAR       *name_full_dest,
                            CPU_BOOLEAN     excl,
                            FS_ERR         *p_err);

void      FSEntry_Create   (CPU_CHAR       *name_full,          /* Create a file or directory.                          */
                            FS_FLAGS        entry_type,
                            CPU_BOOLEAN     excl,
                            FS_ERR         *p_err);

void      FSEntry_Del      (CPU_CHAR       *name_full,          /* Delete a file or directory.                          */
                            FS_FLAGS        entry_type,
                            FS_ERR         *p_err);
#endif

void      FSEntry_Query    (CPU_CHAR       *name_full,          /* Get information about a file or directory.           */
                            FS_ENTRY_INFO  *p_info,
                            FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void      FSEntry_Rename   (CPU_CHAR       *name_full_old,      /* Rename a file or directory.                          */
                            CPU_CHAR       *name_full_new,
                            CPU_BOOLEAN     excl,
                            FS_ERR         *p_err);

void      FSEntry_TimeSet  (CPU_CHAR       *name_full,          /* Set a file or directory's date/time.                 */
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
