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
*                                          FS SHELL COMMANDS
*
* Filename : fs_shell.h
* Version  : V4.08.01
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_SHELL_PRESENT                                       /* See Note #1.                                         */
#define  FS_SHELL_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_SHELL_MODULE
#define  FS_SHELL_EXT
#else
#define  FS_SHELL_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>

#include  <lib_def.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  <lib_ascii.h>

#include  <shell.h>
#include  <fs.h>
#include  <fs_shell_cfg.h>


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

CPU_BOOLEAN  FSShell_Init(void);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  FS_SHELL_CFG_BUF_LEN
#error  "FS_SHELL_CFG_BUF_LEN                  not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be >=   256]                     "
#error  "                                [     && <= 65535]                     "

#elif  ((FS_SHELL_CFG_BUF_LEN <                256u) || \
        (FS_SHELL_CFG_BUF_LEN > DEF_INT_16U_MAX_VAL))
#error  "FS_SHELL_CFG_BUF_LEN            illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be >=   256]                     "
#error  "                                [     && <= 65535]                     "
#endif



#ifndef  FS_SHELL_CFG_CMD_CAT_EN
#error  "FS_SHELL_CFG_CMD_CAT_EN               not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_CAT_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_CAT_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_CAT_EN         illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_CD_EN
#error  "FS_SHELL_CFG_CMD_CD_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_CD_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_CD_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_CD_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_CP_EN
#error  "FS_SHELL_CFG_CMD_CP_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_CP_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_CP_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_CP_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_DATE_EN
#error  "FS_SHELL_CFG_CMD_DATE_EN              not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_DATE_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_DATE_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_DATE_EN        illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_DF_EN
#error  "FS_SHELL_CFG_CMD_DF_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_DF_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_DF_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_DF_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_LS_EN
#error  "FS_SHELL_CFG_CMD_LS_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_LS_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_LS_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_LS_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_MKDIR_EN
#error  "FS_SHELL_CFG_CMD_MKDIR_EN             not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_MKDIR_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_MKDIR_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_MKDIR_EN       illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_MKFS_EN
#error  "FS_SHELL_CFG_CMD_MKFS_EN              not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_MKFS_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_MKFS_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_MKFS_EN        illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_MOUNT_EN
#error  "FS_SHELL_CFG_CMD_MOUNT_EN             not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_MOUNT_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_MOUNT_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_MOUNT_EN       illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_MV_EN
#error  "FS_SHELL_CFG_CMD_MV_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_MV_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_MV_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_MV_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_OD_EN
#error  "FS_SHELL_CFG_CMD_OD_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_OD_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_OD_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_OD_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_PWD_EN
#error  "FS_SHELL_CFG_CMD_PWD_EN               not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_PWD_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_PWD_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_PWD_EN         illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_RM_EN
#error  "FS_SHELL_CFG_CMD_RM_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_RM_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_RM_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_RM_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_RMDIR_EN
#error  "FS_SHELL_CFG_CMD_RMDIR_EN               not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_RMDIR_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_RMDIR_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_RMDIR_EN       illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_TOUCH_EN
#error  "FS_SHELL_CFG_CMD_TOUCH_EN             not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_TOUCH_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_TOUCH_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_TOUCH_EN       illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_UMOUNT_EN
#error  "FS_SHELL_CFG_CMD_UMOUNT_EN            not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_UMOUNT_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_UMOUNT_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_UMOUNT_EN      illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif



#ifndef  FS_SHELL_CFG_CMD_WC_EN
#error  "FS_SHELL_CFG_CMD_WC_EN                not #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "

#elif  ((FS_SHELL_CFG_CMD_WC_EN != DEF_DISABLED) && \
        (FS_SHELL_CFG_CMD_WC_EN != DEF_ENABLED))
#error  "FS_SHELL_CFG_CMD_WC_EN          illegally #define'd in 'fs_shell_cfg.h'"
#error  "                                [MUST be DEF_DISABLED]                 "
#error  "                                [     || DEF_ENABLED ]                 "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of FS_SHELL module include.                      */
