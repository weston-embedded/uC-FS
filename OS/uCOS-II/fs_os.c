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
*                                          Micrium uC/OS-II
*
* Filename : fs_os.c
* Version  : V4.08.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/OS-II V2.89 is included in the project build.
*
*            (2) REQUIREs the following uC/OS-II features to be ENABLED :
*
*                    ------- FEATURE --------    ---------- MINIMUM CONFIGURATION FOR FS/OS PORT ----------
*
*                (a) OS Events                   OS_MAX_EVENTS >= FS_OS_NBR_EVENTS (see this 'fs_os.h
*                                                                                        OS OBJECT DEFINES')
*
*                (b) Semaphores                                   FS_OS_NBR_SEM    (see Note #2a)
*                    (1) OS_SEM_EN                   Enabled
*
*                (c) Task registers
*                    (1) OS_TASK_REG_TBL_SIZE        >= 2 (if working directory functionality enabled)
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    FS_OS_MODULE
#include  <cpu.h>
#include  <cpu_core.h>
#include  <lib_mem.h>
#include  <ucos_ii.h>
#include  "fs_os.h"
#include  "../../Source/fs.h"
#include  "../../Source/fs_dev.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


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

                                                                /* ---------------------- LOCKS ----------------------- */
static  OS_EVENT    *FS_OS_LockSemPtr;

static  OS_EVENT   **FS_OS_DevLockSemTbl;
static  OS_EVENT   **FS_OS_DevAccessLockSemTbl;

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
static  OS_EVENT   **FS_OS_FileLockSemTbl;
static  OS_TCB     **FS_OS_FileLockTaskID_Tbl;
static  FS_CTR      *FS_OS_FileLockCntTbl;
#endif


#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  CPU_INT08U   FS_OS_RegIdWorkingDir;
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
#if (OS_EVENT_NAME_EN > 0u || FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    INT8U  os_err;
#endif


    FS_OS_LockSemPtr = OSSemCreate(1u);                         /* Create file system lock.                             */
    if (FS_OS_LockSemPtr == (OS_EVENT *)0) {
       *p_err = FS_ERR_OS_INIT_LOCK;
        return;
    }

#if (OS_EVENT_NAME_EN > 0u)
    OSEventNameSet(          FS_OS_LockSemPtr,
                   (INT8U *) FS_LOCK_NAME,
                            &os_err);
    if (os_err != OS_ERR_NONE) {
       *p_err = FS_ERR_OS_INIT_LOCK_NAME;
        return;
    }
#endif


#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    FS_OS_RegIdWorkingDir = OSTaskRegGetID(&os_err);
    if (os_err != OS_ERR_NONE) {
        FS_TRACE_DBG(("FS_OS_Init(): No task register available.\r\n"));
       *p_err = FS_ERR_OS_INIT;
        return;
    }
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
    INT8U  os_err;


    OSSemPend( FS_OS_LockSemPtr,                                /* Acquire file system access ...                       */
               0u,                                              /* ... without timeout (see Note #1).                   */
              &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = FS_ERR_NONE;
             break;


        case OS_ERR_PEVENT_NULL:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_ABORT:
        case OS_ERR_TIMEOUT:
        default:
             FS_TRACE_DBG(("FS_OS_Lock(): Lock failed.\r\n"));
            *p_err = FS_ERR_OS_LOCK;
             break;
    }
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
   (void)OSSemPost(FS_OS_LockSemPtr);                           /* Release file system access.                          */
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
    INT8U      os_err;
    INT32U     reg_val;
    CPU_CHAR  *p_working_dir;


    reg_val = OSTaskRegGet( OS_PRIO_SELF,
                            FS_OS_RegIdWorkingDir,
                           &os_err);

    if (os_err != OS_ERR_NONE) {
        reg_val = 0u;
    }

    p_working_dir = (CPU_CHAR *)reg_val;
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
    INT8U   os_err;
    INT32U  reg_val;


    reg_val = (INT32U)p_working_dir;
    OSTaskRegSet( OS_PRIO_SELF,
                  FS_OS_RegIdWorkingDir,
                  reg_val,
                 &os_err);
    if(os_err != OS_ERR_NONE) {
        FS_TRACE_DBG(("FS_OS_WorkingDirSet(): Error setting task register.\r\n"));
       *p_err = FS_ERR_OS;
        return;
    }

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       FS_OS_WorkingDirFree()
*
* Description : Free working directory assigned to task.
*
* Argument(s) : p_tcb       Pointer to the task control block of the task being deleted.
*               ----------  Argument validated by caller.
*
* Return(s)   : Working directory of active task.
*
* Note(s)     : (1) Application hooks MUST be enabled & this function MUST be called from App_TaskDelHook()
*                   if any task(s) using FS functions will be deleted.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_OS_WorkingDirFree (OS_TCB  *p_tcb)
{
    INT8U      os_err;
    INT32U     reg_val;
    CPU_CHAR  *path_buf;


    reg_val = OSTaskRegGet( p_tcb->OSTCBPrio,
                            FS_OS_RegIdWorkingDir,
                           &os_err);

    if (os_err != OS_ERR_NONE) {
        return;
    }
    if (reg_val == 0u) {
        return;
    }

    path_buf = (CPU_CHAR *)reg_val;
    FS_WorkingDirObjFree(path_buf);
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
    OS_EVENT  *p_event;


    p_event = OSSemCreate(cnt);
    if (p_event == (OS_EVENT *)0) {
        return (DEF_FAIL);
    }

   *p_sem = p_event;
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
    OS_EVENT  *p_event;
    INT8U      os_err;


    p_event = *p_sem;
    if (p_event != (OS_EVENT *)0) {
        OSSemDel(p_event, OS_DEL_ALWAYS, &os_err);
       *p_sem = (OS_EVENT *)0;
            switch (os_err) {
                case OS_ERR_NONE:
                     return (DEF_OK);


                case OS_ERR_DEL_ISR:
                case OS_ERR_INVALID_OPT:
                case OS_ERR_TASK_WAITING:
                case OS_ERR_EVENT_TYPE:
                case OS_ERR_PEVENT_NULL:
                default:
                     return (DEF_FAIL);
            }
    }

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
    OS_EVENT  *p_event;
    INT32U     timeout_ticks;
    INT8U      os_err;


    p_event       = *p_sem;

    timeout_ticks = (timeout * OS_TICKS_PER_SEC + (DEF_TIME_NBR_mS_PER_SEC - 1)) / (DEF_TIME_NBR_mS_PER_SEC);
    OSSemPend(p_event, timeout_ticks, &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
             return (DEF_OK);


        case OS_ERR_TIMEOUT:
        case OS_ERR_PEND_ABORT:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEVENT_NULL:
        case OS_ERR_PEND_LOCKED:
        default:
             return (DEF_FAIL);
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
    OS_EVENT  *p_event;


    p_event = *p_sem;
    OSSemPost(p_event);

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
    CPU_INT32U  dly_ticks;


    dly_ticks = (CPU_INT32U)(((((CPU_INT32U)ms * OS_TICKS_PER_SEC) + (DEF_TIME_NBR_mS_PER_SEC - 1u)) / DEF_TIME_NBR_mS_PER_SEC) + 1u);

    OSTimeDly(dly_ticks);
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
#if (OS_EVENT_NAME_EN > 0u)
    INT8U       os_err;
#endif
    INT8U       ix;
    LIB_ERR     lib_err;
    CPU_SIZE_T  octets_reqd;


                                                                /* ---------------- CREATE DEVICE LOCKS --------------- */
    FS_OS_DevLockSemTbl = (OS_EVENT **)Mem_HeapAlloc( sizeof(OS_EVENT *) * (CPU_SIZE_T)dev_cnt,
                                                      sizeof(CPU_DATA),
                                                     &octets_reqd,
                                                     &lib_err);
    if (FS_OS_DevLockSemTbl == DEF_NULL) {
        FS_TRACE_INFO(("FS_OS_DevInit(): Could not alloc mem for dev locks: %d octets required.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    for (ix = 0u; ix < dev_cnt; ix++) {
        FS_OS_DevLockSemTbl[ix] = OSSemCreate(1u);              /* Create device lock.                                  */

        if (FS_OS_DevLockSemTbl[ix] == DEF_NULL) {
           *p_err = FS_ERR_OS_INIT_LOCK;
            return;
        }

#if (OS_EVENT_NAME_EN > 0u)
        OSEventNameSet(          FS_OS_DevLockSemTbl[ix],
                       (INT8U *) FS_DEV_LOCK_NAME,
                                &os_err);

        if (os_err != OS_ERR_NONE) {
           *p_err = FS_ERR_OS_INIT_LOCK_NAME;
            return;
        }
#endif
    }


    												            /* ------------ CREATE DEVICE ACCESS LOCKS ------------ */
    FS_OS_DevAccessLockSemTbl = (OS_EVENT **)Mem_HeapAlloc( sizeof(OS_EVENT *) * (CPU_SIZE_T)dev_cnt,
                                                            sizeof(CPU_DATA),
                                                           &octets_reqd,
                                                           &lib_err);
    if (FS_OS_DevAccessLockSemTbl == DEF_NULL) {
        FS_TRACE_INFO(("FS_OS_DevInit(): Could not alloc mem for dev access locks: %d octets required.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    for (ix = 0u; ix < dev_cnt; ix++) {
        FS_OS_DevAccessLockSemTbl[ix] = OSSemCreate(1u);        /* Create device access lock.                           */

        if (FS_OS_DevAccessLockSemTbl[ix] == DEF_NULL) {
           *p_err = FS_ERR_OS_INIT_LOCK;
            return;
        }

#if (OS_EVENT_NAME_EN > 0u)
        OSEventNameSet(          FS_OS_DevAccessLockSemTbl[ix],
                       (INT8U *) FS_DEV_ACCESS_LOCK_NAME,
                                &os_err);

        if (os_err != OS_ERR_NONE) {
           *p_err = FS_ERR_OS_INIT_LOCK_NAME;
            return;
        }
#endif
    }

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
    INT8U  os_err;


    OSSemPend( FS_OS_DevLockSemTbl[dev_id],                     /* Acquire device access without timeout (see Note #1). */
               0u,
              &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = FS_ERR_NONE;
             break;


        case OS_ERR_PEVENT_NULL:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_ABORT:
        case OS_ERR_TIMEOUT:
        default:
             FS_TRACE_DBG(("FS_OS_DevLock(): Lock failed for dev %d.\r\n", dev_id));
            *p_err = FS_ERR_OS_LOCK;
             break;
    }
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
    (void)OSSemPost(FS_OS_DevLockSemTbl[dev_id]);               /* Release device access.                               */
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
    CPU_INT32U  timeout_ticks;
    INT8U       os_err;


    timeout_ticks = (((timeout * OS_TICKS_PER_SEC)  + 1000u - 1u) / 1000u);

    OSSemPend( FS_OS_DevAccessLockSemTbl[dev_id],     /* Acquire device access without timeout (see Note #1). */
               timeout_ticks,
              &os_err);

    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = FS_ERR_NONE;
             break;

        case OS_ERR_TIMEOUT:
            *p_err = FS_ERR_OS_LOCK_TIMEOUT;
             break;


        case OS_ERR_PEVENT_NULL:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_ABORT:
        default:
             FS_TRACE_DBG(("FS_OS_DevAccessLock(): Lock failed for dev %d.\r\n", dev_id));
            *p_err = FS_ERR_OS_LOCK;
             break;
    }
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
    (void)OSSemPost(FS_OS_DevAccessLockSemTbl[dev_id]);               /* Release device access lock.                           */
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
#if (OS_EVENT_NAME_EN > 0u)
    INT8U       os_err;
#endif
    INT8U       ix;
    LIB_ERR     lib_err;
    CPU_SIZE_T  octets_reqd;


                                                                /* ----------------- CREATE FILE LOCKS ---------------- */
    FS_OS_FileLockSemTbl = (OS_EVENT **)Mem_HeapAlloc( sizeof(OS_EVENT *) * (CPU_SIZE_T)file_cnt,
                                                       sizeof(OS_EVENT *),
                                                      &octets_reqd,
                                                      &lib_err);
    if (FS_OS_FileLockSemTbl == (OS_EVENT **)0) {
        FS_TRACE_INFO(("FS_OS_FileInit(): Could not alloc mem for file locks: %d octets required.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    for (ix = 0u; ix < file_cnt; ix++) {
        FS_OS_FileLockSemTbl[ix] = OSSemCreate(1u);             /* Create file lock.                                    */

        if (FS_OS_FileLockSemTbl[ix] == (OS_EVENT *)0) {
           *p_err = FS_ERR_OS_INIT_LOCK;
            return;
        }

#if (OS_EVENT_NAME_EN > 0u)
        OSEventNameSet(          FS_OS_FileLockSemTbl[ix],
                       (INT8U *) FS_FILE_LOCK_NAME,
                                &os_err);

        if (os_err != OS_ERR_NONE) {
           *p_err = FS_ERR_OS_INIT_LOCK_NAME;
            return;
        }
#endif
    }



                                                                /* ------------ CREATE FILE LOCK INFO TBLS ------------ */
    FS_OS_FileLockTaskID_Tbl = (OS_TCB **)Mem_HeapAlloc( sizeof(OS_TCB *) * (CPU_SIZE_T)file_cnt,
                                                         sizeof(OS_TCB *),
                                                        &octets_reqd,
                                                        &lib_err);
    if (FS_OS_FileLockTaskID_Tbl == (OS_TCB **)0) {
        FS_TRACE_INFO(("FS_OS_FileInit(): Could not alloc mem for file locks: %d octets required.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    FS_OS_FileLockCntTbl = (FS_CTR *)Mem_HeapAlloc( sizeof(FS_CTR) * (CPU_SIZE_T)file_cnt,
                                                    sizeof(FS_CTR),
                                                   &octets_reqd,
                                                   &lib_err);
    if (FS_OS_FileLockCntTbl == (FS_CTR *)0) {
        FS_TRACE_INFO(("FS_OS_FileInit(): Could not alloc mem for file locks: %d octets required.\r\n", octets_reqd));
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    for (ix = 0u; ix < file_cnt; ix++) {
        FS_OS_FileLockTaskID_Tbl[ix] = (OS_TCB *)0;             /* Clr entry.                                           */
        FS_OS_FileLockCntTbl[ix]     =  0u;
    }


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
    INT16U   os_rtn;
    OS_TCB  *task_id;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    task_id = OSTCBCur;
    if (FS_OS_FileLockCntTbl[file_id] > 0u) {                   /* If task owns file              ...                   */
        if (FS_OS_FileLockTaskID_Tbl[file_id] == task_id) {     /* ... chk if this task owns file ...                   */
            FS_OS_FileLockCntTbl[file_id]++;                    /* ... & inc lock cnt.                                  */
           *p_err = FS_ERR_NONE;
        } else {
           *p_err = FS_ERR_OS_LOCK;
        }
        CPU_CRITICAL_EXIT();
        return;
    }
    CPU_CRITICAL_EXIT();

    os_rtn = OSSemAccept(FS_OS_FileLockSemTbl[file_id]);        /* Acquire file access (see Note #1).                   */
    if (os_rtn == 0u) {
       *p_err = FS_ERR_OS_LOCK;
        return;
    }

    CPU_CRITICAL_ENTER();
    FS_OS_FileLockTaskID_Tbl[file_id] = task_id;                /* Register task ID.                                    */
    FS_OS_FileLockCntTbl[file_id]     = 1u;
    CPU_CRITICAL_EXIT();

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
    INT8U    os_err;
    OS_TCB  *task_id;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    task_id = OSTCBCur;
    if (FS_OS_FileLockCntTbl[file_id] > 0u) {                   /* If task owns file              ...                   */
        if (FS_OS_FileLockTaskID_Tbl[file_id] == task_id) {     /* ... chk if this task owns file ...                   */
            FS_OS_FileLockCntTbl[file_id]++;                    /* ... & inc lock cnt.                                  */
            CPU_CRITICAL_EXIT();
           *p_err = FS_ERR_NONE;
            return;
        }
    }
    CPU_CRITICAL_EXIT();

    OSSemPend( FS_OS_FileLockSemTbl[file_id],                   /* Acquire file access without timeout (see Note #1).   */
               0u,
              &os_err);
    switch (os_err) {
        case OS_ERR_NONE:
            *p_err = FS_ERR_NONE;
             break;


        case OS_ERR_PEVENT_NULL:
        case OS_ERR_EVENT_TYPE:
        case OS_ERR_PEND_ISR:
        case OS_ERR_PEND_ABORT:
        case OS_ERR_TIMEOUT:
        default:
             FS_TRACE_DBG(("FS_OS_FileLock(): Lock failed for file %d\r\n.", file_id));
            *p_err = FS_ERR_OS_LOCK;
             return;
    }

    CPU_CRITICAL_ENTER();
    FS_OS_FileLockTaskID_Tbl[file_id] = task_id;                /* Register task ID.                                    */
    FS_OS_FileLockCntTbl[file_id]     = 1u;
    CPU_CRITICAL_EXIT();

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
    FS_CTR   lock_cnt;
    OS_TCB  *task_id;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    lock_cnt = FS_OS_FileLockCntTbl[file_id];
    if (lock_cnt > 0u) {                                        /* If task owns file              ...                   */
        task_id = OSTCBCur;
        if (FS_OS_FileLockTaskID_Tbl[file_id] == task_id) {     /* ... chk if this task owns file ...                   */
            if (lock_cnt > 1u) {                                /* ... & dec lock cnt.                                  */
                FS_OS_FileLockCntTbl[file_id] = lock_cnt - 1u;
            } else {
                FS_OS_FileLockTaskID_Tbl[file_id] = (OS_TCB *)0;
                FS_OS_FileLockCntTbl[file_id]     =  0u;
                CPU_CRITICAL_EXIT();

                (void)OSSemPost(FS_OS_FileLockSemTbl[file_id]); /* Release file access.                                 */
                return (DEF_YES);
            }
        }
    }
    CPU_CRITICAL_EXIT();

    return (DEF_NO);
}
#endif
