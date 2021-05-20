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
*                                FS SHELL COMMANDS CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : fs_shell_cfg.h
* Version  : V4.08.01
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          INCLUDE GUARD
*********************************************************************************************************
*/

#ifndef  FS_SHELL_CFG_H
#define  FS_SHELL_CFG_H


/*
*********************************************************************************************************
*                                              FS SHELL
*
* Note(s) : (1) Defines the length of the buffer used to read/write from files during file access
*               operations.
*
*           (2) Enable/disable the FS shell command.
*********************************************************************************************************
*/

#define  FS_SHELL_CFG_BUF_LEN                            512u   /* Cfg buf len          (see Note #1).                  */

#define  FS_SHELL_CFG_CMD_CAT_EN                 DEF_ENABLED    /* En/dis fs_cat.       (see Note #2).                  */
#define  FS_SHELL_CFG_CMD_CD_EN                  DEF_ENABLED    /* En/dis fs_cd.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_CP_EN                  DEF_ENABLED    /* En/dis fs_cp.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_DATE_EN                DEF_ENABLED    /* En/dis fs_date.      ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_DF_EN                  DEF_ENABLED    /* En/dis fs_df.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_LS_EN                  DEF_ENABLED    /* En/dis fs_ls.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_MKDIR_EN               DEF_ENABLED    /* En/dis fs_mkdir.     ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_MKFS_EN                DEF_ENABLED    /* En/dis fs_mkfs.      ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_MOUNT_EN               DEF_ENABLED    /* En/dis fs_mount.     ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_MV_EN                  DEF_ENABLED    /* En/dis fs_mv.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_OD_EN                  DEF_ENABLED    /* En/dis fs_od.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_PWD_EN                 DEF_ENABLED    /* En/dis fs_pwd.       ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_RM_EN                  DEF_ENABLED    /* En/dis fs_rm.        ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_RMDIR_EN               DEF_ENABLED    /* En/dis fs_rmdir.     ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_TOUCH_EN               DEF_ENABLED    /* En/dis fs_touch.     ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_UMOUNT_EN              DEF_ENABLED    /* En/dis fs_umount.    ( "   "   " ).                  */
#define  FS_SHELL_CFG_CMD_WC_EN                  DEF_ENABLED    /* En/dis fs_wc.        ( "   "   " ).                  */


/*
*********************************************************************************************************
*                                        INCLUDE GUARD END
*********************************************************************************************************
*/

#endif
