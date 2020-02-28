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
*                                 FILE SYSTEM SUITE CACHE MANAGEMENT
*
* Filename : fs_cache.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_CACHE_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_buf.h"
#include  "fs_cache.h"
#include  "fs_ctr.h"
#include  "fs_dev.h"
#include  "fs_vol.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_cache.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_CACHE_MODULE_PRESENT                                /* See Note #1.                                         */


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
*                                        CACHE DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_cache_data {
    FS_SEC_QTY    Size;
    FS_SEC_QTY    NextBufToUseIx;
    FS_BUF      **BufUsedPtrs;
} FS_CACHE_DATA;

/*
*********************************************************************************************************
*                                           CACHE DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_cache {
    FS_FLAGS         Mode;                                      /* Cache mode.                                          */
    FS_SEC_SIZE      SecSize;                                   /* Size of sector (in bytes).                           */
    FS_SEC_QTY       Size;                                      /* Size of cache (in bufs).                             */

    FS_CACHE_DATA    DataMgmt;                                  /* Mgmt cache data.                                     */
    FS_CACHE_DATA    DataDir;                                   /* Dir  cache data.                                     */
    FS_CACHE_DATA    DataData;                                  /* Data cache data.                                     */

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR           StatHitCtr;                                /* Nbr hits.                                            */
    FS_CTR           StatMissCtr;                               /* Nbr misses.                                          */
    FS_CTR           StatRemoveCtr;                             /* Nbr removes.                                         */
    FS_CTR           StatAllocCtr;                              /* Nbr bufs alloc'd.                                    */
    FS_CTR           StatUpdateCtr;                             /* Nbr bufs updated.                                    */
    FS_CTR           StatRdCtr;                                 /* Nbr rds.                                             */
    FS_CTR           StatRdAvoidCtr;                            /* Nbr rds avoided.                                     */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_CTR           StatWrCtr;                                 /* Nbr wrs.                                             */
    FS_CTR           StatWrAvoidCtr;                            /* Nbr wrs avoided.                                     */
#endif
#endif
} FS_CACHE;


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


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                /* ---------- CACHE API FNCTS --------- */
static  void            FSCache_Create          (FS_VOL          *p_vol,        /* Create cache.                        */
                                                 void            *p_cache_data,
                                                 CPU_INT32U       size,
                                                 FS_SEC_SIZE      sec_size,
                                                 CPU_INT08U       pct_mgmt,
                                                 CPU_INT08U       pct_dir,
                                                 FS_FLAGS         mode,
                                                 FS_ERR          *p_err);

static  void            FSCache_Del             (FS_VOL          *p_vol,        /* Del cache.                           */
                                                 FS_ERR          *p_err);

static  void            FSCache_Rd              (FS_VOL          *p_vol,        /* Rd secs using cache.                 */
                                                 void            *p_dest,
                                                 FS_SEC_NBR       start,
                                                 FS_SEC_QTY       cnt,
                                                 FS_FLAGS         sec_type,
                                                 FS_ERR          *p_err);

static  void            FSCache_Release         (FS_VOL          *p_vol,        /* Release secs using cache.            */
                                                 FS_SEC_NBR       start,
                                                 FS_SEC_QTY       cnt,
                                                 FS_ERR          *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void            FSCache_Wr              (FS_VOL          *p_vol,        /* Wr secs using cache.                 */
                                                 void            *p_src,
                                                 FS_SEC_NBR       start,
                                                 FS_SEC_QTY       cnt,
                                                 FS_FLAGS         sec_type,
                                                 FS_ERR          *p_err);
#endif

static  void            FSCache_Invalidate      (FS_VOL          *p_vol,        /* Invalidate cache.                    */
                                                 FS_ERR          *p_err);

static  void            FSCache_Flush           (FS_VOL          *p_vol,        /* Flush cache.                         */
                                                 FS_ERR          *p_err);


                                                                                /* ------------ LOCAL FNCTS ----------- */
                                                                                /* Init Data cache structure            */
static  void           FSCache_DataInit         (FS_VOL          *p_vol,
                                                 FS_CACHE_DATA   *p_data_cache,
                                                 FS_BUF         **p_buf_ptrs,
                                                 FS_SEC_QTY       size);

static  void           FSCache_BufFree          (FS_BUF          *p_buf);       /* Free buf.                            */



static  void           FSCache_EntriesInvalidate(FS_CACHE_DATA   *p_cache_data, /* Invalidate entries from cache.       */
                                                 FS_CACHE        *p_cache);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void           FSCache_EntriesFlush     (FS_VOL          *p_vol,
                                                 FS_CACHE_DATA   *p_cache_data, /* Flush entries from cache.            */
                                                 FS_DEV          *p_dev,
                                                 FS_ERR          *p_err);
#endif

static  void           FSCache_EntriesRelease   (FS_CACHE_DATA   *p_cache_data, /* Release entries from cache.          */
                                                 FS_CACHE        *p_cache,
                                                 FS_SEC_NBR       start,
                                                 FS_SEC_QTY       cnt);

static  void           FSCache_EntryFlush       (FS_BUF          *p_buf,        /* Flush cache entry.                   */
                                                 FS_ERR          *p_err);

static  void           FSCache_UpdateIndex      (FS_CACHE_DATA   *p_cache_data);/* Get next entry in the cache.         */


static  FS_BUF       **FSCache_EntryFind        (FS_CACHE_DATA   *p_cache_data, /* Find entry in cache.                 */
                                                 FS_SEC_NBR       start);

static  void           FSCache_ObjClr           (FS_CACHE        *p_cache);     /* Clr cache obj.                       */

static  CPU_BOOLEAN    FSCache_SecGet           (FS_CACHE        *p_cache,      /* Get sec from cache.                  */
                                                 void            *p_dest,
                                                 FS_SEC_NBR       start,
                                                 FS_FLAGS         sec_type);

static  CPU_BOOLEAN    FSCache_SecPut           (FS_CACHE        *p_cache,      /* Put sec into cache.                  */
                                                 void            *p_src,
                                                 FS_SEC_NBR       start,
                                                 FS_FLAGS         sec_type,
                                                 CPU_BOOLEAN      rd);

static  FS_CACHE_DATA *FSCache_GetData          (FS_CACHE        *p_cache,      /* Get Cache data by sector type        */
                                                 FS_FLAGS         sec_type);

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_VOL_CACHE_API  FSCache_Dflt = {
    FSCache_Create,
    FSCache_Del,
    FSCache_Rd,
    FSCache_Release,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSCache_Wr,
#endif
    FSCache_Invalidate,
    FSCache_Flush
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      CACHE INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSCache_Create()
*
* Description : Create cache.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
*               size            Size, in bytes, of cache buffer.
*
*               sec_size        Sector size, in octets.
*
*               pct_mgmt        Percent of cache buffer dedicated to management sectors.
*
*               pct_dir         Percent of cache buffer dedicated to directory sectors.
*
*               mode            Cache mode :
*
*                                   FS_VOL_CACHE_MODE_RD            Read cache; sectors cached ONLY when read.
*                                   FS_VOL_CACHE_MODE_WR_THROUGH    Write through cache; sectors caches when
*                                                                       read or written; sectors written
*                                                                       immediately to volume.
*                                   FS_VOL_CACHE_MODE_WR_BACK       Write back  cache; sectors caches when
*                                                                       read or written; sectors may not be
*                                                                       written immediately to volume.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                       Cache created.
*                                   FS_ERR_CACHE_INVALID_MODE         Mode specified invalid.
*                                   FS_ERR_CACHE_INVALID_SEC_TYPE     Sector type specified invalid.
*                                   FS_ERR_CACHE_TOO_SMALL            Size specified too small for cache.
*                                   FS_ERR_NULL_PTR                   NULL pointer passed for cache.
*
* Return(s)   : none.
*
* Note(s)     : (1) Write back cache NOT supported.
*********************************************************************************************************
*/

static  void  FSCache_Create (FS_VOL       *p_vol,
                              void         *p_cache_data,
                              CPU_INT32U    size,
                              FS_SEC_SIZE   sec_size,
                              CPU_INT08U    pct_mgmt,
                              CPU_INT08U    pct_dir,
                              FS_FLAGS      mode,
                              FS_ERR       *p_err)
{
    CPU_INT32U    align;
    CPU_INT32U    buf_size;
    FS_CACHE     *p_cache;
    FS_SEC_QTY    cache_size;
    FS_SEC_QTY    cache_size_max;
    FS_SEC_QTY    cache_size_mgmt;
    FS_SEC_QTY    cache_size_dir;
    FS_SEC_QTY    cache_size_data;
    CPU_INT32U    offset;
    FS_BUF      **p_buf_used_ptrs;
    CPU_INT08U   *p_cache_data_08;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if ((mode != FS_VOL_CACHE_MODE_RD)         &&               /* Validate cache mode.                                 */
        (mode != FS_VOL_CACHE_MODE_WR_THROUGH) &&
        (mode != FS_VOL_CACHE_MODE_WR_BACK)) {
       *p_err = FS_ERR_CACHE_INVALID_MODE;
        return;
    }
    if (pct_mgmt + pct_dir > 100u) {                            /* Validate cache pct's.                                */
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif



                                                                /* -------------------- INIT CACHE -------------------- */
    p_vol->CacheDataPtr = (void *)0;

    offset              =  0u;
    p_cache_data_08     = (CPU_INT08U *)p_cache_data;
    align               = (CPU_ADDR    )p_cache_data_08 % sizeof(CPU_ALIGN);

    if (align != 0u) {
        offset          +=  sizeof(CPU_ALIGN) - align;
        p_cache_data_08 +=  sizeof(CPU_ALIGN) - align;
    }

    p_cache          = (FS_CACHE *)p_cache_data_08;             /* Alloc cache ...                                      */
    offset          +=  sizeof(FS_CACHE);
    p_cache_data_08 +=  sizeof(FS_CACHE);
    if (offset >= size)  {                                      /*             ... chk for alloc ovf.                   */
       *p_err = FS_ERR_CACHE_TOO_SMALL;
        return;
    }

    FSCache_ObjClr(p_cache);



                                                                /* ----------------- ALIGN CACHE DATA ----------------- */
    align      = sizeof(FS_BUF) % sizeof(CPU_ALIGN);
    if (align != 0u) {
        align  = sizeof(CPU_ALIGN) - align;
    }
    buf_size   = sizeof(FS_BUF) + align;

    cache_size_max   =  size / (buf_size + sec_size);

    p_buf_used_ptrs  = (FS_BUF **)p_cache_data_08;             /* Alloc used buf ptr array.                            */
    offset          +=  sizeof(CPU_ADDR) * cache_size_max;
    p_cache_data_08 +=  sizeof(CPU_ADDR) * cache_size_max;


    offset += buf_size + sec_size;
    if (offset >= size) {                                       /*               ... chk for alloc ovf.                 */
       *p_err = FS_ERR_CACHE_TOO_SMALL;
        return;
    }

                                                                /* Clr buf ptrs.                                        */
    Mem_Clr((void *)p_buf_used_ptrs, sizeof (CPU_ADDR) * cache_size_max);



                                                                /* -------------------- ALLOC BUFS -------------------- */
    cache_size = 0u;
    while (offset < size) {
        p_buf_used_ptrs[cache_size]           = (FS_BUF *)p_cache_data_08;
        p_cache_data_08                      +=  buf_size;
        p_buf_used_ptrs[cache_size]->DataPtr  =  p_cache_data_08;
        p_cache_data_08                      +=  sec_size;

        offset +=  buf_size + sec_size;
        cache_size++;
    }

    cache_size_mgmt = (cache_size * pct_mgmt + (100u - 1u)) / 100u;
    cache_size_dir  = (cache_size * pct_dir  + (100u - 1u)) / 100u;

    if (cache_size_mgmt + cache_size_dir > cache_size) {
        cache_size_mgmt--;
        if (cache_size_mgmt + cache_size_dir > cache_size) {
            cache_size_dir--;
        }
    }

                                                                /* ------------------ INIT CACHE INFO ----------------- */
    p_cache->Mode    =  mode;
    p_cache->Size    =  cache_size;
    p_cache->SecSize =  sec_size;

                                                                /* Init Mgmt cache data.                                */
    FSCache_DataInit( p_vol,
                     &p_cache->DataMgmt,
                      p_buf_used_ptrs,
                      cache_size_mgmt);
                                                                /* Init Dir cache data.                                 */
    p_buf_used_ptrs += cache_size_mgmt;
    FSCache_DataInit( p_vol,
                     &p_cache->DataDir,
                      p_buf_used_ptrs,
                      cache_size_dir);
                                                                /* Init Data cache data.                                */
    cache_size_data  = (cache_size - cache_size_mgmt) - cache_size_dir;
    p_buf_used_ptrs +=  cache_size_dir;
    FSCache_DataInit( p_vol,
                     &p_cache->DataData,
                      p_buf_used_ptrs,
                      cache_size_data);

    p_vol->CacheDataPtr = (void *)p_cache;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FSCache_Del()
*
* Description : Delete cache.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE     Cache deleted.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_Del (FS_VOL  *p_vol,
                           FS_ERR  *p_err)
{
    FS_CACHE  *p_cache;


    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;
    if (p_cache == (FS_CACHE *)0) {
       *p_err = FS_ERR_NONE;
        return;
    }

    Mem_Clr((void *)p_cache->DataMgmt.BufUsedPtrs, sizeof(CPU_ADDR) * p_cache->Size);

    FSCache_ObjClr(p_cache);
    p_vol->CacheDataPtr = (void *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FSCache_Rd()
*
* Description : Read data from volume sector(s) using cache.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               sec_type    Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Volume sector(s) read.
*                               FS_ERR_VOL_INVALID_SEC_NBR    Sector start or count invalid.
*
*                                                             ------- RETURNED BY FSDev_RdLocked() ------
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

static  void  FSCache_Rd (FS_VOL      *p_vol,
                          void        *p_dest,
                          FS_SEC_NBR   start,
                          FS_SEC_QTY   cnt,
                          FS_FLAGS     sec_type,
                          FS_ERR      *p_err)
{
    FS_CACHE     *p_cache;
    CPU_INT08U   *p_dest_08;
    CPU_BOOLEAN   found;
    FS_SEC_NBR    start_rd;
    FS_SEC_NBR    start_acc;
    FS_SEC_QTY    cnt_acc;
    CPU_INT08U   *p_dest_acc;


    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;

                                                                /* -------------- VALIDATE CACHE FOR VOL -------------- */
    if (p_cache == (FS_CACHE *)0) {                             /* If cache is unused ...                               */
        start += p_vol->PartitionStart;
        FSDev_RdLocked(p_vol->DevPtr,                           /*                    ... rd directly from dev.         */
                       p_dest,
                       start,
                       cnt,
                       p_err);
        return;
    }



                                                                /* ------------------- RD FROM CACHE ------------------ */
    cnt_acc    = 0u;
    start_acc  = 0u;
    p_dest_acc = DEF_NULL;
    p_dest_08  = (CPU_INT08U *)p_dest;
    while (cnt > 0u) {
        found = FSCache_SecGet(p_cache,                         /* Try to rd sec from cache.                            */
                               p_dest_08,
                               start,
                               sec_type);

        if (found == DEF_NO) {                                  /* If sec NOT in cache ...                              */
            if (cnt_acc == 0u) {
                start_acc  = start;
                p_dest_acc = p_dest_08;
            }
            cnt_acc++;                                          /*                     ... acc sec to rd.               */
            FS_CTR_STAT_INC(p_cache->StatRdCtr);

        } else {                                                /* If sec in cache ...                                  */
            if (cnt_acc > 0u) {
                start_rd = start_acc + p_vol->PartitionStart;
                FSDev_RdLocked(p_vol->DevPtr,                   /*                 ... rd acc'd secs ...                */
                               p_dest_acc,
                               start_rd,
                               cnt_acc,
                               p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }

                while (cnt_acc > 0u) {                          /*                                   ... put in cache.  */
                   (void)FSCache_SecPut(p_cache,
                                        p_dest_acc,
                                        start_acc,
                                        sec_type,
                                        DEF_YES);
                    start_acc++;
                    cnt_acc--;
                    p_dest_acc += p_cache->SecSize;
                }
            }
            FS_CTR_STAT_INC(p_cache->StatRdAvoidCtr);
        }

        start++;                                                /* Move to next sec.                                    */
        cnt--;
        p_dest_08 += p_cache->SecSize;
    }



                                                                /* ---------------- RD FINAL ACC'D SECS --------------- */
    if (cnt_acc > 0u) {
        start_rd = start_acc + p_vol->PartitionStart;
        FSDev_RdLocked(p_vol->DevPtr,                           /* Rd acc'd secs ...                                    */
                       p_dest_acc,
                       start_rd,
                       cnt_acc,
                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        while (cnt_acc > 0u) {                                  /*               ... put in cache.                      */
           (void)FSCache_SecPut(p_cache,
                                p_dest_acc,
                                start_acc,
                                sec_type,
                                DEF_YES);
            start_acc++;
            cnt_acc--;
            p_dest_acc += p_cache->SecSize;
        }
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FSCache_Release()
*
* Description : Release volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               start       Start sector of release.
*
*               cnt         Number of sectors to release.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Volume sector(s) released.
*                               FS_ERR_DEV_INVALID_SEC_NBR    Sector start or count invalid.
*
*                                                             ---- RETURNED BY FSDev_ReleaseLocked() ----
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

static  void  FSCache_Release (FS_VOL      *p_vol,
                               FS_SEC_NBR   start,
                               FS_SEC_QTY   cnt,
                               FS_ERR      *p_err)
{
    FS_CACHE  *p_cache;


    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;

                                                                /* -------------- REMOVE SECS FROM CACHE -------------- */
    if (p_cache != (FS_CACHE *)0) {
        FSCache_EntriesRelease(&p_cache->DataMgmt,
                                p_cache,
                                start,
                                cnt);
        FSCache_EntriesRelease(&p_cache->DataDir,
                                p_cache,
                                start,
                                cnt);
        FSCache_EntriesRelease(&p_cache->DataData,
                                p_cache,
                                start,
                                cnt);
    }

   *p_err  = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            FSCache_Wr()
*
* Description : Write data to volume sector(s) using cache.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of write.
*
*               cnt         Number of sectors to write.
*
*               sec_type    Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Volume sector(s) written.
*                               FS_ERR_VOL_INVALID_SEC_NBR    Sector start or count invalid.
*
*                                                             ------- RETURNED BY FSDev_WrLocked() ------
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSCache_Wr (FS_VOL      *p_vol,
                          void        *p_src,
                          FS_SEC_NBR   start,
                          FS_SEC_QTY   cnt,
                          FS_FLAGS     sec_type,
                          FS_ERR      *p_err)
{
    FS_CACHE       *p_cache;
    CPU_INT08U     *p_src_08;
    CPU_INT08U     *p_src_acc;
    CPU_BOOLEAN     vol_wr;
    FS_SEC_NBR      start_acc;
    FS_SEC_QTY      cnt_acc;
    FS_CACHE_DATA  *p_cache_data;


    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;

                                                                /* -------------- VALIDATE CACHE FOR VOL -------------- */
                                                                /* If cache is unused ...                               */
    if (p_cache == (FS_CACHE *)0) {
        start += p_vol->PartitionStart;
        FSDev_WrLocked(p_vol->DevPtr,                           /*                    ... wr directly to dev.           */
                       p_src,
                       start,
                       cnt,
                       p_err);
        return;
    }


    if (p_cache->Mode == FS_VOL_CACHE_MODE_RD) {
        start_acc = start + p_vol->PartitionStart;
        FSDev_WrLocked(p_vol->DevPtr,                           /*                    ... wr directly to dev.           */
                       p_src,
                       start_acc,
                       cnt,
                       p_err);
                                                                /* Invalidate sects. in the cache for data consistency  */
        p_cache_data = FSCache_GetData(p_cache, sec_type);
        if (p_cache_data != (FS_CACHE_DATA *)0) {
            FSCache_EntriesRelease(p_cache_data,
                                   p_cache,
                                   start,
                                   cnt);
        }

       *p_err = FS_ERR_NONE;
        return;
    }

                                                                /* ---------------------- WR CACHE -------------------- */
    cnt_acc   = 0u;
    start_acc = 0u;
    p_src_acc = DEF_NULL;
    p_src_08  = (CPU_INT08U *)p_src;
    while (cnt > 0u) {
        vol_wr = FSCache_SecPut(p_cache,                        /* Try to wr sec into cache.                            */
                                p_src_08,
                                start,
                                sec_type,
                                DEF_NO);


        if (vol_wr == DEF_YES) {                                /* If sec should be wr to vol ...                       */
            if (cnt_acc == 0u) {
                start_acc = start;
                p_src_acc = p_src_08;
            }
            cnt_acc++;
            FS_CTR_STAT_INC(p_cache->StatWrCtr);

        } else {
            if (cnt_acc > 0u) {
                start_acc += p_vol->PartitionStart;
                FSDev_WrLocked(p_vol->DevPtr,                   /*                           ... wr acc'd secs.         */
                               p_src_acc,
                               start_acc,
                               cnt_acc,
                               p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }
                cnt_acc = 0u;
            }
             FS_CTR_STAT_INC(p_cache->StatWrAvoidCtr);

        }

        start++;                                                /* Move to next sec.                                    */
        cnt--;
        p_src_08 += p_cache->SecSize;
    }

    if (cnt_acc > 0u) {
        start_acc += p_vol->PartitionStart;
        FSDev_WrLocked(p_vol->DevPtr,                           /* Wr acc'd secs.                                       */
                       p_src_acc,
                       start_acc,
                       cnt_acc,
                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FSCache_Invalidate()
*
* Description : Invalidate cache.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                                   FS_ERR_NONE     Cache invalidated.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_Invalidate (FS_VOL  *p_vol,
                                  FS_ERR  *p_err)
{
    FS_CACHE  *p_cache;


    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;
    if (p_cache == (FS_CACHE *)0) {
       *p_err = FS_ERR_NONE;
        return;
    }

    FSCache_EntriesInvalidate(&p_cache->DataMgmt, p_cache);     /* Invalidate mgmt cache.                               */
    FSCache_EntriesInvalidate(&p_cache->DataDir,  p_cache);     /* Invalidate dir  cache.                               */
    FSCache_EntriesInvalidate(&p_cache->DataData, p_cache);     /* Invalidate data cache.                               */

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSCache_Flush()
*
* Description : Flush cache.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Cache flushed.
*
*                                                              --- RETURNED BY FSCache_EntriesFlush() ---
*                               FS_ERR_INVALID_SEC_NBR         Sector start or count invalid.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_Flush (FS_VOL  *p_vol,
                             FS_ERR  *p_err)
{
    FS_CACHE  *p_cache;


                                                                /* ------------- VALIDATE CACHE FOR FLUSH ------------- */
    p_cache = (FS_CACHE *)p_vol->CacheDataPtr;
    if (p_cache == (FS_CACHE *)0) {
       *p_err = FS_ERR_NONE;
        return;
    }

    if ((p_cache->Mode == FS_VOL_CACHE_MODE_RD) ||
        (p_cache->Mode == FS_VOL_CACHE_MODE_WR_THROUGH)) {
       *p_err = FS_ERR_NONE;
        return;
    }



#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                         /* -------------------- FLUSH CACHE ------------------- */
    FSCache_EntriesFlush( p_vol,
                         &p_cache->DataMgmt,                    /* Flush mgmt secs.                                     */
                          p_vol->DevPtr,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FSCache_EntriesFlush( p_vol,
                         &p_cache->DataDir,                     /* Flush dir secs.                                      */
                          p_vol->DevPtr,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FSCache_EntriesFlush( p_vol,
                         &p_cache->DataData,                    /* Flush data secs.                                     */
                          p_vol->DevPtr,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }
#endif

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
*                                               FSCache_DataInit()
*
* Description : Initialize the data cache structure.
*
* Argument(s) : p_vol           Pointer to Volume.
*               ----------      Argument validated by caller.
*
*               p_data_cache    Pointer to cache data to initialize.
*               ----------      Argument validated by caller.
*
*               p_buf_ptrs      Pointer to the start address of cache data buffers
*               ----------      Argument validated by caller.
*
*               size            Number of data cache buffer
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_DataInit (FS_VOL          *p_vol,
                                FS_CACHE_DATA   *p_data_cache,
                                FS_BUF         **p_buf_ptrs,
                                FS_SEC_QTY       size)
{
    CPU_INT32U   i;
    FS_BUF      *p_buf;


    p_data_cache->Size           =  size;
    p_data_cache->NextBufToUseIx =  0u;
    p_data_cache->BufUsedPtrs    =  p_buf_ptrs;
    for (i = 0u; i < size; i++) {
         p_buf         = p_data_cache->BufUsedPtrs[i];
         p_buf->State  = FS_BUF_STATE_NONE;
         p_buf->Start  = (FS_SEC_NBR)(-1);
         p_buf->VolPtr = p_vol;
    }
}

/*
*********************************************************************************************************
*                                          FSCache_BufFree()
*
* Description : Free buffer by changing its state to FS_BUF_STATE_NONE.
*
* Argument(s) : p_buf       Pointer to buffer.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_BufFree (FS_BUF  *p_buf)
{
    p_buf->State = FS_BUF_STATE_NONE;
    p_buf->Start = (FS_SEC_NBR)(-1);
}


/*
*********************************************************************************************************
*                                     FSCache_EntriesInvalidate()
*
* Description : Invalidate cache.
*
* Argument(s) : p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
*               p_cache         Pointer to cache.
*               ----------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_EntriesInvalidate (FS_CACHE_DATA  *p_cache_data,
                                         FS_CACHE       *p_cache)
{
    FS_BUF       *p_buf;
    FS_BUF      **p_buf_ptr;
    FS_SEC_QTY    buf_ix;


    (void)p_cache;

    if (p_cache_data->Size == 0u) {
        return;
    }

    p_buf_ptr = p_cache_data->BufUsedPtrs;
    buf_ix    = 0u;

    while (buf_ix < p_cache_data->Size) {
        p_buf = *p_buf_ptr;
        FSCache_BufFree(p_buf);

        buf_ix++;
        p_buf_ptr++;
    }

    p_cache_data->NextBufToUseIx = 0u;
}


/*
*********************************************************************************************************
*                                       FSCache_EntriesFlush()
*
* Description : Flush cache entries.
*
* Argument(s) : p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
*               p_dev           Pointer to device.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                   Cache flushed.
*
*                                                                 ----- RETURNED BY FSDev_WrLocked() ----
*                                   FS_ERR_INVALID_SEC_NBR        Sector start or count invalid.
*                                   FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                                   FS_ERR_DEV_IO                 Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT            Device timeout error.
*                                   FS_ERR_DEV_NOT_PRESENT        Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSCache_EntriesFlush (FS_VOL         *p_vol,
                                    FS_CACHE_DATA  *p_cache_data,
                                    FS_DEV         *p_dev,
                                    FS_ERR         *p_err)
{
    FS_SEC_QTY    buf_ix;
    FS_BUF       *p_buf;
    FS_BUF      **p_buf_ptr;


    (void)p_vol;
    (void)p_dev;

    if (p_cache_data->Size == 0u) {
        return;
    }

    p_buf_ptr = p_cache_data->BufUsedPtrs;
    p_cache_data->NextBufToUseIx = 0u;
    buf_ix    = 0u;
    while (buf_ix < p_cache_data->Size) {
        p_buf = *p_buf_ptr;

        FSCache_EntryFlush(p_buf, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        buf_ix++;
        p_buf_ptr++;
    }

   *p_err = FS_ERR_NONE;
}
#endif




/*
*********************************************************************************************************
*                                      FSCache_EntriesRelease()
*
* Description : Flush cache entries.
*
* Argument(s) : p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
*               p_cache         Pointer to cache.
*               ----------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_EntriesRelease (FS_CACHE_DATA  *p_cache_data,
                                      FS_CACHE       *p_cache,
                                      FS_SEC_NBR      start,
                                      FS_SEC_QTY      cnt)
{
    FS_BUF       *p_buf;
    FS_BUF      **p_buf_ptr;
    FS_SEC_QTY    buf_ix;


    (void)p_cache;

    if (p_cache_data->Size == 0u) {
        return;
    }

    p_buf_ptr = p_cache_data->BufUsedPtrs;
    buf_ix    = 0u;

    while (buf_ix < p_cache_data->Size) {
        p_buf = *p_buf_ptr;

        if ((p_buf->Start >= start) &&
            (p_buf->Start < start + cnt)) {
            FSCache_BufFree(p_buf);
        }

        buf_ix++;
        p_buf_ptr++;
    }

}


/*
*********************************************************************************************************
*                                         FSCache_UpdateIndex()
*
* Description : Get the next cache buffer index where data will be stored in the cache.
*
* Argument(s) : p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_UpdateIndex (FS_CACHE_DATA  *p_cache_data)
{
    p_cache_data->NextBufToUseIx++;
    if (p_cache_data->NextBufToUseIx >= p_cache_data->Size) {
        p_cache_data->NextBufToUseIx = 0u;
    }
}


/*
*********************************************************************************************************
*                                          FSCache_EntryFlush()
*
* Description : Flush cache buffer through device layer.
*
* Argument(s) : p_buf       Pointer to a buffer. Must be a cache buffer.
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

static  void  FSCache_EntryFlush(FS_BUF  *p_buf,
                                 FS_ERR  *p_err)
{
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_SEC_QTY  sec_start;
#endif


#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if (p_buf->State == FS_BUF_STATE_DIRTY) {                   /* ---------------- FLUSH BUF CONTENTS ---------------- */
        sec_start = p_buf->Start + p_buf->VolPtr->PartitionStart;
        FSDev_WrLocked(p_buf->VolPtr->DevPtr,                   /* Wr sec.                                              */
                       p_buf->DataPtr,
                       sec_start,
                       1u,
                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#endif

    p_buf->State = FS_BUF_STATE_NONE;                           /* Update buf state.                                    */

   *p_err = FS_ERR_NONE;
}

/*
*********************************************************************************************************
*                                         FSCache_EntryFind()
*
* Description : Find entry stored in cache.
*
* Argument(s) : p_cache_data    Pointer to cache data.
*               ----------      Argument validated by caller.
*
*               start           Start sector.
*
* Return(s)   : pointer to buffer adresses
*               NULL pointer if not found
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_BUF  **FSCache_EntryFind (FS_CACHE_DATA  *p_cache_data,
                                     FS_SEC_NBR      start)
{
    FS_BUF       *p_buf;
    FS_BUF      **p_buf_ptr;
    FS_SEC_QTY    buf_ix;


    if (p_cache_data->Size == 0u) {
        return ((FS_BUF **)0);
    }

    p_buf_ptr = p_cache_data->BufUsedPtrs;
    buf_ix    = 0u;

    while (buf_ix < p_cache_data->Size) {
        p_buf = *p_buf_ptr;

        if (p_buf->Start == start) {
            p_buf_ptr = &p_cache_data->BufUsedPtrs[buf_ix];
            return (p_buf_ptr);
        }

        buf_ix++;
        p_buf_ptr++;
    }

    return ((FS_BUF **)0);
}


/*
*********************************************************************************************************
*                                          FSCache_ObjClr()
*
* Description : Init cache structure.
*
* Argument(s) : p_cache     Pointer to cache.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSCache_ObjClr (FS_CACHE  *p_cache)
{
    p_cache->Mode                     =  FS_VOL_CACHE_MODE_NONE;
    p_cache->SecSize                  =  0u;
    p_cache->Size                     =  0u;

    p_cache->DataMgmt.Size            =  0u;
    p_cache->DataMgmt.NextBufToUseIx  =  0u;
    p_cache->DataMgmt.BufUsedPtrs     = (FS_BUF **)0;

    p_cache->DataDir.Size             =  0u;
    p_cache->DataDir.NextBufToUseIx   =  0u;
    p_cache->DataDir.BufUsedPtrs      = (FS_BUF **)0;

    p_cache->DataData.Size            =  0u;
    p_cache->DataData.NextBufToUseIx  =  0u;
    p_cache->DataData.BufUsedPtrs     = (FS_BUF **)0;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    p_cache->StatHitCtr               =  0u;
    p_cache->StatMissCtr              =  0u;
    p_cache->StatRemoveCtr            =  0u;
    p_cache->StatAllocCtr             =  0u;
    p_cache->StatUpdateCtr            =  0u;
    p_cache->StatRdCtr                =  0u;
    p_cache->StatRdAvoidCtr           =  0u;
#if (FS_CFG_RD_ONLY_EN  == DEF_DISABLED)
    p_cache->StatWrCtr                =  0u;
    p_cache->StatWrAvoidCtr           =  0u;
#endif
#endif
}


/*
*********************************************************************************************************
*                                          FSCache_SecGet()
*
* Description : Get sec from cache.
*
* Argument(s) : p_cache     Pointer to cache.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector.
*
*               sec_type    Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
* Return(s)   : DEF_NO  if the sector is NOT found in cache.
*               DEF_YES if the sector is     found in cache.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSCache_SecGet (FS_CACHE    *p_cache,
                                     void        *p_dest,
                                     FS_SEC_NBR   start,
                                     FS_FLAGS     sec_type)
{
    FS_BUF          *p_buf;
    FS_BUF         **p_buf_ptr;
    FS_CACHE_DATA   *p_cache_data;


                                                                /* ---------------- FIND ENTRY IN CACHE --------------- */
    p_cache_data = FSCache_GetData(p_cache, sec_type);
    if (p_cache_data == (FS_CACHE_DATA *)0) {
        p_buf_ptr = (FS_BUF **)0;
    } else {
        p_buf_ptr = FSCache_EntryFind(p_cache_data, start);
    }


    if (p_buf_ptr == (FS_BUF **)0) {
        FS_CTR_STAT_INC(p_cache->StatMissCtr);
        return (DEF_NO);
    }

    p_buf = *p_buf_ptr;

    if (p_buf->State == FS_BUF_STATE_NONE) {
        FS_CTR_STAT_INC(p_cache->StatMissCtr);
        return (DEF_NO);
    }

    FS_CTR_STAT_INC(p_cache->StatHitCtr);
    Mem_Copy(p_dest, p_buf->DataPtr, p_cache->SecSize);

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                          FSCache_SecPut()
*
* Description : Put sector into cache.
*
* Argument(s) : p_cache     Pointer to cache.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector.
*
*               sec_type    Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
*               rd          Indicates whether sector is a read or write :
*
*                               DEF_YES, if the sector has been read from the disk (or has just been
*                                        written to the disk).
*                               DEF_NO,  if the sector may be written to the disk.
*
* Return(s)   : DEF_YES if the volume does     need to be updated.
*               DEF_NO  if the volume does NOT need to be updated.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSCache_SecPut (FS_CACHE     *p_cache,
                                     void         *p_src,
                                     FS_SEC_NBR    start,
                                     FS_FLAGS      sec_type,
                                     CPU_BOOLEAN   rd)
{
    CPU_BOOLEAN      update;
    FS_BUF          *p_buf;
    FS_BUF         **p_buf_ptr;
    FS_CACHE_DATA   *p_cache_data;
    FS_ERR           err;


                                                                /* -------------- VALIDATE CACHE FOR SEC -------------- */
    update = (rd == DEF_YES) ? DEF_NO : DEF_YES;

                                                                /* ---------------- FIND ENTRY IN CACHE --------------- */
    p_cache_data = FSCache_GetData(p_cache, sec_type);
    if (p_cache_data == (FS_CACHE_DATA *)0) {
        return (update);
    }

    if (p_cache_data->Size == 0u) {
        return (update);
    }

    p_buf_ptr = FSCache_EntryFind(p_cache_data, start);


                                                                /* ------------------- ALLOC NEW BUF ------------------ */
    if (p_buf_ptr == (FS_BUF **)0) {
        p_buf = p_cache_data->BufUsedPtrs[p_cache_data->NextBufToUseIx];
        FS_CTR_STAT_INC(p_cache->StatRemoveCtr);
        if (p_cache->Mode == FS_VOL_CACHE_MODE_WR_BACK) {
            FSCache_EntryFlush(p_buf, &err);
            if (err != FS_ERR_NONE) {
                return (update);
            }
        }
        p_buf->Start = start;
        p_buf->State = FS_BUF_STATE_USED;
        FSCache_UpdateIndex(p_cache_data);                      /* Update the cache entry index.                        */

    } else{
        p_buf = *p_buf_ptr;
        FS_CTR_STAT_INC(p_cache->StatUpdateCtr);
    }



                                                                /* -------------------- UPDATE BUF -------------------- */
    Mem_Copy(p_buf->DataPtr, p_src, p_cache->SecSize);

    if (rd == DEF_NO) {
        if (p_cache->Mode == FS_VOL_CACHE_MODE_WR_BACK) {       /* Mark cache buf as dirty.                             */
            p_buf->State = FS_BUF_STATE_DIRTY;
            update = DEF_NO;
        }
   }

    return (update);
}


/*
*********************************************************************************************************
*                                           FSCache_GetData()
*
* Description : Get pointer to cache data for a given sector type.
*
* Argument(s) : p_cache     Pointer to cache.
*               ----------  Argument validated by caller.
*
*               sec_type    Type of sector :
*
*                               FS_VOL_SEC_TYPE_MGMT    Management sector.
*                               FS_VOL_SEC_TYPE_DIR     Directory sector.
*                               FS_VOL_SEC_TYPE_FILE    File sector.
*
* Return(s)   : Pointer to cache data if sector type is valid.
*               Null pointer if sector type is invalid.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_CACHE_DATA  *FSCache_GetData (FS_CACHE  *p_cache,
                                         FS_FLAGS   sec_type)
{
    FS_CACHE_DATA  *p_cache_data;


    switch (sec_type) {
        case FS_VOL_SEC_TYPE_MGMT:
             p_cache_data = &p_cache->DataMgmt;
             break;

        case FS_VOL_SEC_TYPE_DIR:
             p_cache_data = &p_cache->DataDir;
             break;

        case FS_VOL_SEC_TYPE_FILE:
             p_cache_data = &p_cache->DataData;
             break;

        default:
             p_cache_data = (FS_CACHE_DATA *)0;
             break;
    }

    return (p_cache_data);
}
/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_cache.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of cache module include (see Note #1).           */
