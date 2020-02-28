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
*                                 FILE SYSTEM OPERATING SYSTEM LAYER
*
*                                          Micrium uC/OS-III
*
* Filename : fs_os.h
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/OS-III V3.03.01 is included in the project build.
*
*            (2) REQUIREs the following uC/OS-III features to be ENABLED :
*
*                    ------- FEATURE --------    ---------- MINIMUM CONFIGURATION FOR FS/OS PORT ----------
*
*                (a) Semaphores
*                    (1) OS_CFG_SEM_EN               Enabled
*
*                (b) Task registers
*                    (1) OS_CFG_TASK_REG_TBL_SIZE    >= 2 (if working directory functionality enabled)
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          INCLUDE GUARD
*********************************************************************************************************
*/

#ifndef  FS_OS_H
#define  FS_OS_H


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <os.h>                                                /* See this 'fs_os.h  Note #1'.                         */


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_OS_MODULE
#define  FS_OS_EXT
#else
#define  FS_OS_EXT  extern
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  FS_OS_PRESENT

/*
*********************************************************************************************************
*                                     OS TASK/OBJECT NAME DEFINES
*********************************************************************************************************
*/

                                                                /* -------------------- OBJ NAMES --------------------- */
#define  FS_LOCK_NAME                       "FS Global Lock"
#define  FS_DEV_LOCK_NAME                   "FS Device Lock"
#define  FS_DEV_ACCESS_LOCK_NAME            "FS Device Access Lock"
#define  FS_FILE_LOCK_NAME                  "FS File Lock"


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


typedef  OS_SEM  FS_OS_SEM;


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


#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_OS_WorkingDirFree(OS_TCB  *p_tcb);
#endif

CPU_BOOLEAN  FS_OS_SemCreate(FS_OS_SEM  *p_sem,
                             CPU_INT16U  cnt);

CPU_BOOLEAN  FS_OS_SemDel   (FS_OS_SEM  *p_sem);

CPU_BOOLEAN  FS_OS_SemPend  (FS_OS_SEM  *p_sem,
                             CPU_INT32U  timeout);

CPU_BOOLEAN  FS_OS_SemPost  (FS_OS_SEM  *p_sem);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if     (OS_VERSION < 30300u)
#error  "OS_VERSION                 illegal uC/OS-III version           "
#error  "                           [MUST be >= V3.03.00]               "
#endif

#if     (OS_CFG_SEM_EN < 1u)
#error  "OS_CFG_SEM_EN              illegally #define'd in 'os_cfg.h'   "
#error  "                           [MUST be  > 0]                      "
#endif

#if     (OS_CFG_SEM_DEL_EN < 1u)
#error  "OS_CFG_SEM_DEL_EN          illegally #define'd in 'os_cfg.h'   "
#error  "                           [MUST be  > 0]                      "
#endif


/*
*********************************************************************************************************
*                                          MODULE END
*********************************************************************************************************
*/

#endif

