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
*                                    FILE SYSTEM BUFFER MANAGEMENT
*
* Filename : fs_buf.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_BUF_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_buf.h"
#include  "fs_vol.h"


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

static  MEM_POOL    FSBuf_Pool;                                 /* Pool of bufs.                                        */

static  MEM_POOL    FSBuf_DataPool;                             /* Pool of buf data.                                    */

static  CPU_SIZE_T  FSBuf_BufSize;                              /* Size of each buf.                                    */


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
static  void  FSBuf_Clr(FS_BUF  *p_buf);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSBuf_ModuleInit()
*
* Description : Initialize buffer module.
*
* Argument(s) : buf_cnt     Number of buffers to allocate.
*
*               buf_size    Size of each buffer, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE         Module initialized.
*                               FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSBuf_ModuleInit (FS_QTY       buf_cnt,
                        CPU_SIZE_T   buf_size,
                        FS_ERR      *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


#if (FS_CFG_TRIAL_EN == DEF_ENABLED)                            /* Trial limitation: max 4 buf.                         */
    if (buf_cnt > FS_TRIAL_MAX_BUF_CNT) {
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif

                                                                /* ------------------- INIT BUF POOL ------------------ */
    Mem_PoolCreate(&FSBuf_Pool,                                 /* Create buf pool.                                     */
                    DEF_NULL,
                    0,
                    buf_cnt,
                    sizeof(FS_BUF),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSBuf_ModuleInit(): Could not alloc mem for bufs: %d octets required.\r\n", octets_reqd));
        return;
    }


    Mem_PoolCreate(&FSBuf_DataPool,                             /* Create buf data pool.                                */
                    DEF_NULL,
                    0,
                    buf_cnt,
                    buf_size,
                    FS_CFG_BUF_ALIGN_OCTETS,
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSBuf_ModuleInit(): Could not alloc mem for buf data: %d octets required.\r\n", octets_reqd));
        return;
    }

    FSBuf_BufSize = buf_size;
}


/*
*********************************************************************************************************
*                                             FSBuf_Get()
*
* Description : Allocate & initialize a buffer.
*
* Argument(s) : p_vol       Pointer to volume to associate with buffer.
*
* Return(s)   : Pointer to a buffer, if NO errors.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) Critical sections are necessary to protect the getting & freeing of buffers so no
*                   universal locking mechanism is required during these operations.
*********************************************************************************************************
*/

FS_BUF  *FSBuf_Get (FS_VOL  *p_vol)
{
    FS_BUF   *p_buf;
    void     *p_buf_data;
    LIB_ERR   pool_err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_buf      = (FS_BUF *)Mem_PoolBlkGet(&FSBuf_Pool,
                                           sizeof(FS_BUF),
                                          &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    if (p_buf == (FS_BUF *)0) {
        CPU_CRITICAL_EXIT();
        FS_TRACE_INFO(("FSBuf_Get(): Could not alloc buf from pool.\r\n"));
        return ((FS_BUF *)0);
    }

    p_buf_data = (void *)Mem_PoolBlkGet(&FSBuf_DataPool,
                                         FSBuf_BufSize,
                                        &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    if (p_buf_data == (void *)0) {
        Mem_PoolBlkFree(        &FSBuf_Pool,
                        (void *) p_buf,
                                &pool_err);
        CPU_CRITICAL_EXIT();
        (void)pool_err;                                        /* Err ignored. Already exiting on prev err.            */
        FS_TRACE_INFO(("FSBuf_Get(): Could not alloc buf data from pool.\r\n"));
        return ((FS_BUF *)0);
    }
    CPU_CRITICAL_EXIT();

    p_buf->Size    = FSBuf_BufSize;

    p_buf->State   = FS_BUF_STATE_NONE;
    p_buf->Start   = 0u;
    p_buf->DataPtr = p_buf_data;
    p_buf->VolPtr  = p_vol;

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    Mem_Clr(p_buf->DataPtr, FSBuf_BufSize);
#endif

    return (p_buf);
}


/*
*********************************************************************************************************
*                                            FSBuf_Free()
*
* Description : Free a buffer.
*
* Argument(s) : p_buf       Pointer to a buffer.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSBuf_Get() Note #1'.
*********************************************************************************************************
*/

void  FSBuf_Free (FS_BUF  *p_buf)
{
    void     *p_buf_data;
    LIB_ERR   pool_err;
    CPU_SR_ALLOC();

                                                                /* ------------------- FREE TO POOL ------------------- */
    CPU_CRITICAL_ENTER();
    p_buf_data = p_buf->DataPtr;                                /* Free buf data.                                       */
    if (p_buf_data != (CPU_INT08U *)0) {
        Mem_PoolBlkFree(        &FSBuf_DataPool,
                        (void *) p_buf_data,
                                &pool_err);
       (void)pool_err;                                         /* Err if already free not relevant.                    */
    }

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    FSBuf_Clr(p_buf);
#endif

    Mem_PoolBlkFree(        &FSBuf_Pool,                        /* Free buf.                                            */
                    (void *) p_buf,
                            &pool_err);
   (void)pool_err;                                             /* Err if already free not relevant.                    */
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                            FSBuf_Flush()
*
* Description : Flush buffer.
*
* Argument(s) : p_buf       Pointer to a buffer.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Buffer flushed.
*
*                                                             ------ RETURNED BY FSVol_WrLockedEx() -----
*                                                             ------ RETURNED BY FSVol_RdLockedEx() -----
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSBuf_Flush (FS_BUF  *p_buf,
                   FS_ERR  *p_err)
{
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if (p_buf->State == FS_BUF_STATE_DIRTY) {                   /* ---------------- FLUSH BUF CONTENTS ---------------- */
        FSVol_WrLockedEx(p_buf->VolPtr,                         /* Wr sec.                                              */
                         p_buf->DataPtr,
                         p_buf->Start,
                         1u,
                         p_buf->SecType,
                         p_err);
        if (*p_err != FS_ERR_NONE) {
            p_buf->State = FS_BUF_STATE_NONE;
            return;
        }
    }
#endif

    p_buf->State = FS_BUF_STATE_NONE;                           /* Update buf state.                                    */

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSBuf_MarkDirty()
*
* Description : Mark buffer as dirty.
*
* Argument(s) : p_buf       Pointer to a buffer.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Buffer marked.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSBuf_MarkDirty (FS_BUF  *p_buf,
                       FS_ERR  *p_err)
{
    p_buf->State = FS_BUF_STATE_DIRTY;
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                             FSBuf_Set()
*
* Description : Set buffer sector.
*
* Argument(s) : p_buf       Pointer to a buffer.
*               ----------  Argument validated by caller.
*
*               start       Sector number.
*
*               type        Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
*               rd          Indicates whether sector contents on volume will be read into buffer :
*
*                               DEF_YES    Read sector contents on volume into buffer.
*                               DEF_NO     Leave buffer contents alone.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Buffer set.
*
*                                                             ------ RETURNED BY FSVol_WrLockedEx() -----
*                                                             ------ RETURNED BY FSVol_RdLockedEx() -----
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSBuf_Set (FS_BUF       *p_buf,
                 FS_SEC_NBR    start,
                 FS_FLAGS      sec_type,
                 CPU_BOOLEAN   rd,
                 FS_ERR       *p_err)
{
    if (p_buf->State != FS_BUF_STATE_NONE) {                    /* -------------------- DATA IN BUF ------------------- */
        if (p_buf->Start == start) {
           *p_err = FS_ERR_NONE;
            return;
        }
    }



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if (p_buf->State == FS_BUF_STATE_DIRTY) {                   /* ---------------- FLUSH BUF CONTENTS ---------------- */
        FSVol_WrLockedEx(p_buf->VolPtr,                         /* Wr sec.                                              */
                         p_buf->DataPtr,
                         p_buf->Start,
                         1u,
                         p_buf->SecType,
                         p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        p_buf->State = FS_BUF_STATE_USED;                       /* Update buf state.                                    */
    }
#endif



    if (rd == DEF_YES) {                                        /* ----------------- INIT BUF CONTENTS ---------------- */
        FSVol_RdLockedEx(p_buf->VolPtr,                         /* Rd sec.                                              */
                         p_buf->DataPtr,
                         start,
                         1u,
                         sec_type,
                         p_err);
        if (*p_err != FS_ERR_NONE) {
            p_buf->State = FS_BUF_STATE_NONE;
            return;
        }
    }

    p_buf->State   = FS_BUF_STATE_USED;                         /* Update buf state.                                    */
    p_buf->Start   = start;
    p_buf->SecType = sec_type;
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             FSBuf_Clr()
*
* Description : Clear a buffer.
*
* Argument(s) : p_buf       Pointer to a buffer.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
static  void  FSBuf_Clr (FS_BUF  *p_buf)
{
    p_buf->State   =  FS_BUF_STATE_NONE;
    p_buf->Start   =  0u;
    p_buf->SecType =  FS_VOL_SEC_TYPE_UNKNOWN;
    p_buf->DataPtr = (void   *)0;
    p_buf->VolPtr  = (FS_VOL *)0;
}
#endif

