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
*                                 FILE SYSTEM OPERATING SYSTEM LAYER
*
*                                                No OS
*
* Filename : fs_os.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    FS_OS_MODULE
#include  <cpu_core.h>
#include  <cpu.h>
#include  "fs_os.h"
#include  "../../Source/fs.h"
#include  "../../Source/fs_dev.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


#define  FS_OS_CNTS_PER_MS                            12000uL  /* $$$$ See 'FS_OS_SemPend()'. */


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
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

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  CPU_CHAR  *FS_OS_WorkingDirPtr;
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            FS_OS_Init()
*
* Description : Perform FS/OS initialization.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                 FS/OS initialization successful.
*                               FS_ERR_OS_INIT_LOCK         FS lock signal NOT successfully initialized.
*                               FS_ERR_OS_INIT_LOCK_NAME    FS lock signal name NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_OS_Init (FS_ERR  *p_err)
{
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    FS_OS_WorkingDirPtr = (void *)0;
#endif

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FS_OS_Lock()
*
* Description : Acquire mutually exclusive access to file system suite.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE       File system access     acquired.
*                               FS_ERR_OS_LOCK    File system access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) File system access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                   Failure to acquire file system access will prevent file system operation(s)
*                   from functioning.
*********************************************************************************************************
*/

void  FS_OS_Lock (FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FS_OS_Unlock()
*
* Description : Release mutually exclusive access to file system suite.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : (1) File system access MUST be released--i.e. MUST unlock access without failure.
*
*                   Failure to release file system access will prevent file system operation(s)
*                   from functioning.
*********************************************************************************************************
*/

void  FS_OS_Unlock (void)
{

}


/*
*********************************************************************************************************
*                                        FS_OS_WorkingDirGet()
*
* Description : Get working directory assigned to active task.
*
* Argument(s) : none.
*
* Return(s)   : Working directory of active task.
*
* Note(s)     : (1) Task register #1 stores the pointer to the working directory.  If the task register
*                   is zero, the working directory has not been assigned.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
CPU_CHAR  *FS_OS_WorkingDirGet (void)
{
    CPU_CHAR  *p_working_dir;


    p_working_dir = FS_OS_WorkingDirPtr;
    return (p_working_dir);
}
#endif


/*
*********************************************************************************************************
*                                        FS_OS_WorkingDirSet()
*
* Description : Assign working directory to active task.
*
* Argument(s) : p_working_dir   Pointer to working directory.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE       Working directory successfully set.
*                                   FS_ERR_OS         Error setting working directory.
*
* Return(s)   : Working directory of active task.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_OS_WorkingDirSet (CPU_CHAR  *p_working_dir,
                           FS_ERR    *p_err)
{
    FS_OS_WorkingDirPtr = p_working_dir;

    *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         FS_OS_SemCreate()
*
* Description : Create a semaphore.
*
* Argument(s) : p_sem       Pointer to semaphore.
*               ----------  Argument validated by caller.
*
*               cnt         Initial value for the semaphore.
*
* Return(s)   : DEF_OK,   if semaphore created.
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FS_OS_SemCreate (FS_OS_SEM  *p_sem,
                              CPU_INT16U  cnt)
{
   *p_sem = (FS_OS_SEM)cnt;                    /* $$$$ Create semaphore with initial count 'cnt'. */
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                           FS_OS_SemDel()
*
* Description : Delete a semaphore.
*
* Argument(s) : p_sem       Pointer to semaphore.
*               ----------  Argument validated by caller.
*
* Return(s)   : DEF_OK,   if semaphore deleted.
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FS_OS_SemDel (FS_OS_SEM  *p_sem)
{
   *p_sem = 0u;                 /* $$$$ Delete semaphore. */
    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                          FS_OS_SemPend()
*
* Description : Wait for a semaphore.
*
* Argument(s) : p_sem       Pointer to semaphore.
*               ----------  Argument validated by caller.
*
*               timeout     If non-zero, timeout period (in milliseconds).
*                           If zero,     wait forever.
*
* Return(s)   : DEF_OK,   if semaphore now owned by caller.
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FS_OS_SemPend (FS_OS_SEM  *p_sem,
                            CPU_INT32U  timeout)
{
    CPU_INT16U       sem_val;
#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    CPU_TS32         ts;
    CPU_TS32         ts_init;
    CPU_TS32         ts_diff;
    CPU_TS32         timeout_ts;
    CPU_TS_TMR_FREQ  tmr_freq;
    CPU_ERR          err;
#endif
    CPU_SR_ALLOC();


#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    if (timeout == 0u) {
#else
    (void)timeout;
#endif
        sem_val = 0u;
        while (sem_val == 0u) {
            CPU_CRITICAL_ENTER();
            sem_val = (CPU_INT16U)*p_sem;           /* $$$$ If semaphore available ...                      */
            if (sem_val > 0u) {
               *p_sem = (FS_OS_SEM)sem_val - 1u;   /*      ... decrement semaphore count.                  */
            }
            CPU_CRITICAL_EXIT();
        }

#if (CPU_CFG_TS_TMR_EN == DEF_ENABLED)
    } else {
        sem_val     =  0;

        tmr_freq    =  CPU_TS_TmrFreqGet(&err);
        if (err != CPU_ERR_NONE) {
            return DEF_FAIL;
        }
        timeout_ts  = timeout * (tmr_freq / 1000);
        ts_init     =  CPU_TS_TmrRd();

        ts_diff     =  0;

        while ((ts_diff < timeout_ts) &&
               (sem_val      == 0u)) {
            CPU_CRITICAL_ENTER();
            sem_val = (CPU_INT16U)*p_sem;           /* $$$$ If semaphore available ...                      */
            if (sem_val > 0) {
               *p_sem = (FS_OS_SEM)sem_val - 1u;   /*      ... decrement semaphore count.                  */
            }
            CPU_CRITICAL_EXIT();

            ts      = CPU_TS_TmrRd();
            ts_diff = ts - ts_init;
        }
    }
#endif

    if (sem_val == 0u) {
        return (DEF_FAIL);
    } else {
        return (DEF_OK);
    }
}


/*
*********************************************************************************************************
*                                          FS_OS_SemPost()
*
* Description : Signal a semaphore.
*
* Argument(s) : p_sem       Pointer to semaphore.
*               ----------  Argument validated by caller.
*
* Return(s)   : DEF_OK,   if semaphore signaled.
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FS_OS_SemPost (FS_OS_SEM  *p_sem)
{
    CPU_INT16U  sem_val;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    sem_val = (CPU_INT16U)*p_sem;               /* $$$$ Increment semaphore value. */
    sem_val++;
   *p_sem   =  (FS_OS_SEM)sem_val;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                          FS_OS_Dly_ms()
*
* Description : Delay for specified time, in milliseconds.
*
* Argument(s) : ms      Time delay value, in milliseconds.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_OS_Dly_ms (CPU_INT16U  ms)
{
    FS_BSP_Dly_ms(ms);
}

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEVICE
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           FS_OS_DevInit()
*
* Description : Perform FS/OS device initialization.
*
* Argument(s) : dev_cnt     Number of device locks to allocate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                 FS/OS device initialization successful.
*                               FS_ERR_OS_INIT_LOCK         FS device lock signal NOT successfully initialized.
*                               FS_ERR_OS_INIT_LOCK_NAME    FS device lock signal name NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_OS_DevInit (FS_QTY   dev_cnt,
                     FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FS_OS_DevLock()
*
* Description : Acquire mutually exclusive access to file system device.
*
* Argument(s) : dev_id      Index of the semaphore to acquire.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE       File system device access     acquired.
*                               FS_ERR_OS_LOCK    File system device access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                   Failure to acquire device access will prevent device operation(s) from functioning.
*
*               (2) When both the device and access locks are required the access lock MUST be
*                   acquired first.
*********************************************************************************************************
*/

void  FS_OS_DevLock (FS_ID    dev_id,
                     FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FS_OS_DevUnlock()
*
* Description : Release mutually exclusive access to file system device.
*
* Argument(s) : dev_id      Index of the semaphore to release.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device access MUST be released--i.e. MUST unlock access without failure.
*
*                   Failure to release device access will prevent device operation(s) from functioning.
*********************************************************************************************************
*/

void  FS_OS_DevUnlock (FS_ID  dev_id)
{

}


/*
*********************************************************************************************************
*                                        FS_OS_DevAccessLock()
*
* Description : Acquire global device access lock and block high level uC/FS operations on that device.
*
* Argument(s) : dev_id      Index of the semaphore to acquire.
*
*               timeout     Timeout value in milliseconds.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE             File system device access     acquired.
*                               FS_ERR_OS_LOCK          File system device access NOT acquired.
*                               FS_ERR_OS_LOCK_TIMEOUT  File system device access timed out.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*               (2) When both the device and access locks are required the access lock MUST be
*                   acquired first.
*
*               (3) The device access lock can be acquired by external applications by calling
*                   FSDev_AccessLock() to access the device layer exclusively.
*********************************************************************************************************
*/

void  FS_OS_DevAccessLock (FS_ID       dev_id,
	                       CPU_INT32U  timeout,
                           FS_ERR     *p_err)
{
	*p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FS_OS_DevAccessUnlock()
*
* Description : Release mutually exclusive access to file system device.
*
* Argument(s) : dev_id      Index of the semaphore to release.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device access MUST be released--i.e. MUST unlock access without failure.
*
*                   Failure to release device access will prevent device operation(s) from functioning.
*********************************************************************************************************
*/

void  FS_OS_DevAccessUnlock (FS_ID  dev_id)
{

}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                                FILE
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FS_OS_FileInit()
*
* Description : Perform FS/OS file initialization.
*
* Argument(s) : file_cnt    Number of file locks to allocate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                 FS/OS initialization successful.
*                               FS_ERR_OS_INIT_LOCK         FS file lock signal NOT successfully initialized.
*                               FS_ERR_OS_INIT_LOCK_NAME    FS file lock signal name NOT successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FS_OS_FileInit (FS_QTY   file_cnt,
                      FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         FS_OS_FileAccept()
*
* Description : Acquire mutually exclusive access to file system file (without waiting).
*
* Argument(s) : file_id     Index of the semaphore to acquire.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE       File system file access     acquired.
*                               FS_ERR_OS_LOCK    File system file access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) File access should be acquired WITHOUT waiting, if available.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FS_OS_FileAccept (FS_ID    file_id,
                        FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FS_OS_FileLock()
*
* Description : Acquire mutually exclusive access to file system file.
*
* Argument(s) : file_id     Index of the semaphore to acquire.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE       File system file access     acquired.
*                               FS_ERR_OS_LOCK    File system file access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) File access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                   Failure to acquire file access will prevent file operation(s) from functioning.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FS_OS_FileLock (FS_ID    file_id,
                      FS_ERR  *p_err)
{
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         FS_OS_FileUnlock()
*
* Description : Release mutually exclusive access to file system file.
*
* Argument(s) : file_id     Index of the semaphore to release.
*
* Return(s)   : DEF_YES, if file lock     released.
*               DEF_NO,  if file lock NOT released.
*
* Note(s)     : (1) File access MUST be released--i.e. MUST unlock access without failure.
*
*                   Failure to release file access will prevent file operation(s) from functioning.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
CPU_BOOLEAN  FS_OS_FileUnlock (FS_ID  file_id)
{
    return (DEF_YES);

}
#endif

