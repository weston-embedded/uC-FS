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
*                                 FILE SYSTEM SUITE VOLUME MANAGEMENT
*
* Filename : fs_vol.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_VOL_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_cache.h"
#include  "fs_dev.h"
#include  "fs_err.h"
#include  "fs_partition.h"
#include  "fs_sys.h"
#include  "fs_type.h"
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

FS_STATIC  MEM_POOL    FSVol_Pool;                              /* Pool of volumes.                                     */
FS_STATIC  FS_VOL    **FSVol_Tbl;                               /* Tbl of vol.                                          */

FS_STATIC  FS_QTY      FSVol_Cnt;                               /* Current number of open volumes.                      */
FS_STATIC  FS_QTY      FSVol_VolCntMax;                         /* Maximum number of open volumes.                      */


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* ------------- VOLUME OBJECT MANAGEMENT ------------- */
static  void     FSVol_ObjClr    (FS_VOL            *p_vol);    /* Clear    a volume object.                            */

static  FS_VOL  *FSVol_ObjFind   (CPU_CHAR          *name_vol,  /* Find     a volume object by name.                    */
                                  FS_ERR            *p_err);

static  FS_VOL  *FSVol_ObjFindEx (CPU_CHAR          *name_vol,  /* Find     a volume object by name OR device/partition.*/
                                  FS_DEV            *p_dev,
                                  FS_PARTITION_NBR   partition_nbr);

static  void     FSVol_ObjFree   (FS_VOL            *p_vol);    /* Free     a volume object.                            */

static  FS_VOL  *FSVol_ObjGet    (void);                        /* Allocate a volume object.                            */

                                                                /* ----------------- LOCKED ACCESS ---------------- */
static  void     FSVol_OpenLocked(FS_VOL            *p_vol,     /* Open volume.                                     */
                                  FS_ERR            *p_err);

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FSVol_CacheAssign()
*
* Description : Assign cache to a volume.
*
* Argument(s) : name_vol        Volume name.
*
*               p_cache_api     Pointer to...
*                               (a) cache API to use; OR
*                               (b) NULL, if default cache API should be used.
*
*               p_cache_data    Pointer to cache data.
*
*               size            Size, in bytes, of cache buffer.
*
*               pct_mgmt        Percent of cache buffer dedicated to management sectors.
*
*               pct_dir         Percent of cache buffer dedicated to directory sectors.
*
*               mode            Cache mode.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Cache created.
*                                   FS_ERR_NAME_NULL                 Argument 'name_vol' passed a NULL pointer.
*                                   FS_ERR_VOL_NOT_OPEN              Volume not open.
*
*                                                                    --- RETURNED BY FSCache_Assign() ---
*                                   FS_ERR_NULL_PTR                  Argument 'p_cache_data' passed a NULL
*                                                                        pointer.
*                                   FS_ERR_CACHE_INVALID_MODE        Mode specified invalid.
*                                   FS_ERR_CACHE_INVALID_SEC_TYPE    Sector type specified invalid.
*                                   FS_ERR_CACHE_TOO_SMALL           Size specified too small for cache.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef FS_CACHE_MODULE_PRESENT
void  FSVol_CacheAssign (CPU_CHAR          *name_vol,
                         FS_VOL_CACHE_API  *p_cache_api,
                         void              *p_cache_data,
                         CPU_INT32U         size,
                         CPU_INT08U         pct_mgmt,
                         CPU_INT08U         pct_dir,
                         FS_FLAGS           mode,
                         FS_ERR            *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_cache_data == (void *)0) {                            /* Validate cache data ptr.                             */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if ((mode != FS_VOL_CACHE_MODE_RD)         &&               /* Validate cache mode.                                 */
        (mode != FS_VOL_CACHE_MODE_WR_THROUGH) &&
        (mode != FS_VOL_CACHE_MODE_WR_BACK)) {
       *p_err = FS_ERR_CACHE_INVALID_MODE;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_NO, p_err);      /* Vol may be unmounted.                                */
    if (p_vol == (FS_VOL *)0) {
        return;
    }



                                                                /* ------------------- CREATE CACHE ------------------- */
    if (p_cache_api == (FS_VOL_CACHE_API *)0) {
        p_cache_api =  (FS_VOL_CACHE_API *)&FSCache_Dflt;
    }
    p_vol->CacheAPI_Ptr = p_cache_api;
    p_vol->CacheAPI_Ptr->Create(p_vol,
                                p_cache_data,
                                size,
                                p_vol->SecSize,
                                pct_mgmt,
                                pct_dir,
                                mode,
                                p_err);
    if (*p_err != FS_ERR_NONE) {
        p_vol->CacheAPI_Ptr = (FS_VOL_CACHE_API *)0;
    }



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                       FSVol_CacheInvalidate()
*
* Description : Invalidate cache on a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE               Cache invalidated.
*                               FS_ERR_NAME_NULL          Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_DEV_CHNGD          Device has changed.
*                               FS_ERR_VOL_NO_CACHE       No cache assigned to volume.
*                               FS_ERR_VOL_NOT_OPEN       Volume not open.
*                               FS_ERR_VOL_NOT_MOUNTED    Volume not mounted.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef FS_CACHE_MODULE_PRESENT
void  FSVol_CacheInvalidate (CPU_CHAR  *name_vol,
                             FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    if (p_vol == (FS_VOL *)0) {
        return;
    }

    if (p_vol->CacheAPI_Ptr == (FS_VOL_CACHE_API *)0) {
       *p_err = FS_ERR_VOL_NO_CACHE;
        FSVol_ReleaseUnlock(p_vol);
        return;
    }



                                                                /* -------------------- CLEAN CACHE ------------------- */
    p_vol->CacheAPI_Ptr->Invalidate(p_vol, p_err);



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                         FSVol_CacheFlush()
*
* Description : Flush cache on a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will the receive the return error code from this function :
*
*                               FS_ERR_NONE                   Cache flushed.
*                               FS_ERR_NAME_NULL              Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_DEV_CHNGD              Device has changed.
*                               FS_ERR_VOL_NO_CACHE           No cache assigned to volume.
*                               FS_ERR_VOL_NOT_OPEN           Volume not open.
*                               FS_ERR_VOL_NOT_MOUNTED        Volume not mounted.
*
*                                                             -------- RETURNED BY CACHE Flush() --------
*                               FS_ERR_DEV_INVALID_SEC_NBR    Sector start or count invalid.
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

#ifdef FS_CACHE_MODULE_PRESENT
void  FSVol_CacheFlush (CPU_CHAR  *name_vol,
                        FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    if (p_vol == (FS_VOL *)0) {
        return;
    }

    if (p_vol->CacheAPI_Ptr == (FS_VOL_CACHE_API *)0) {
       *p_err = FS_ERR_VOL_NO_CACHE;
        FSVol_ReleaseUnlock(p_vol);
        return;
    }



                                                                /* -------------------- FLUSH CACHE ------------------- */
    p_vol->CacheAPI_Ptr->Flush(p_vol, p_err);



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                            FSVol_Close()
*
* Description : Close & free a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE            Volume closed.
*                               FS_ERR_NAME_NULL       Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN    Volume not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSVol_Close (CPU_CHAR  *name_vol,
                   FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_NO, p_err);      /* Vol may be unmounted.                                */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == DEF_NULL) {
        return;
    }

                                                                /* Check if files or directories are open on volume     */
    if (p_vol->RefCnt > 2u) {

        FSVol_ReleaseUnlock(p_vol);

#ifdef FS_DIR_MODULE_PRESENT
        if (p_vol->FileCnt > 0) {
           *p_err = FS_ERR_VOL_FILES_OPEN;
        } else if (p_vol->DirCnt > 0) {
           *p_err = FS_ERR_VOL_DIRS_OPEN;
        } else {
#endif
           *p_err = FS_ERR_VOL_FILES_OPEN;
#ifdef FS_DIR_MODULE_PRESENT
        }
#endif


        return;
    }

                                                                /* -------------------- UNMOUNT VOL ------------------- */
    if (p_vol->State == FS_VOL_STATE_MOUNTED) {
        FSSys_VolClose(p_vol);                                  /* Close vol.                                           */
    }

    FSDev_VolRemove(p_vol->DevPtr, p_vol);
    p_vol->State = FS_VOL_STATE_CLOSING;

                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
    FSVol_Release(p_vol);                                       /* Release vol ref init.                                */

   *p_err = FS_ERR_NONE;

#ifdef FS_CACHE_MODULE_PRESENT                                  /* ------------------- DELETE CACHE ------------------- */
    if (p_vol->CacheAPI_Ptr != (FS_VOL_CACHE_API *)0) {
        p_vol->CacheAPI_Ptr->Del(p_vol,
                                 p_err);
    }
#endif

}


/*
*********************************************************************************************************
*                                             FSVol_Fmt()
*
* Description : Format a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_fs_cfg    Pointer to file system-specific configuration information (see Note #2).
*               --------    Validated by 'FSSys_Fmt()'.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                Volume formatted successfully.
*                               FS_ERR_NAME_NULL           Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_DIRS_OPEN       Directories open on volume.
*                               FS_ERR_VOL_FILES_OPEN      Files open on volume.
*                               FS_ERR_VOL_INVALID_OP      Invalid operation on volume.
*                               FS_ERR_VOL_NOT_OPEN        Volume not open.
*
*                                                          ---------- RETURNED BY FSSys_Fmt() -----------
*                               FS_ERR_DEV                 Device access error.
*                               FS_ERR_DEV_INVALID_SIZE    Invalid device size.
*                               FS_ERR_VOL_INVALID_SYS     Invalid file system parameters.
*
* Return(s)   : none.
*
* Note(s)     : (1) Function blocked if files or directories are open on the volume.  All files &
*                   directories MUST be closed prior to formatting the volume.
*
*               (2) For any file system driver, if 'p_fs_cfg' is a pointer to NULL, then the default
*                   configuration will be selected.  If non-NULL, the argument should be passed a pointer
*                   to the appropriate configuration structure.
*
*               (3) Formatting invalidates cache contents.
*
*               (4) (a) If the volume format or subsequent volume open fails with an invalid system or
*                       size error, the volume is NOT closed, so that further formats can be attempted
*                       with a different set of parameters.  See also 'FSVol_Open() Note #2'.
*
*                   (b) If the volume format for subsequent volume open fails with a device access error,
*                       the volume is NOT closed, so that further formats or volume operations can be
*                       attempted once a working device is connected.  See also 'FSVol_Open() Note #3'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSVol_Fmt (CPU_CHAR  *name_vol,
                 void      *p_fs_cfg,
                 FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_NO, p_err);      /* Vol may be unmounted.                                */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {
        return;
    }

                                                                /* Chk vol mode.                                        */
    if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR) == DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }

    if (p_vol->DevPtr->State != FS_DEV_STATE_LOW_FMT_VALID) {   /* Chk if dev present & low fmt'd.                      */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_DEV;
        return;
    }


                                                                /* -------------------- UNMOUNT VOL ------------------- */
    if (p_vol->State == FS_VOL_STATE_MOUNTED) {
        if (p_vol->FileCnt != (FS_QTY)0) {                      /* See Notes #1.                                        */
            FSVol_ReleaseUnlock(p_vol);
           *p_err = FS_ERR_VOL_FILES_OPEN;
            return;
        }
#ifdef FS_DIR_MODULE_PRESENT
        if (p_vol->DirCnt != (FS_QTY)0) {                       /* See Notes #1.                                        */
            FSVol_ReleaseUnlock(p_vol);
           *p_err = FS_ERR_VOL_DIRS_OPEN;
            return;
        }
#endif
    }


#ifdef FS_CACHE_MODULE_PRESENT                                  /* Flush & clean cache (see Notes #3).                  */
    if (p_vol->CacheAPI_Ptr != (FS_VOL_CACHE_API *)0) {
        p_vol->CacheAPI_Ptr->Flush(p_vol, p_err);
        p_vol->CacheAPI_Ptr->Invalidate(p_vol, p_err);
    }
#endif


    if (p_vol->State == FS_VOL_STATE_MOUNTED) {
        p_vol->State = FS_VOL_STATE_OPEN;

        FSSys_VolClose(p_vol);
    }



                                                                /* ---------------------- FMT VOL --------------------- */
    FSSys_VolFmt(p_vol,
                 p_fs_cfg,
                 p_vol->SecSize,
                 p_vol->PartitionSize,
                 p_err);



    switch (*p_err) {
        case FS_ERR_NONE:
             FSSys_VolOpen(p_vol, p_err);
             switch (*p_err) {
                 case FS_ERR_NONE:
                      p_vol->State      = FS_VOL_STATE_MOUNTED;
                      p_vol->RefreshCnt = p_vol->DevPtr->RefreshCnt;
                      break;


                 case FS_ERR_VOL_INVALID_SYS:                   /* Allow user for fmt the vol later (see Note #3a).     */
                     *p_err = FS_ERR_PARTITION_NOT_FOUND;
                      p_vol->State = FS_VOL_STATE_PRESENT;
                      break;


                 case FS_ERR_DEV:                               /* Allow dev to be conn'd later (see Note #3b).         */
                 default:
                      FSVol_ReleaseUnlock(p_vol);
                      p_vol->State = FS_VOL_STATE_OPEN;
                      return;
             }
             break;


        case FS_ERR_VOL_INVALID_SYS:                            /* Allow user for fmt the vol later (see Note #3a).     */
        case FS_ERR_DEV_INVALID_SIZE:
             FSVol_ReleaseUnlock(p_vol);
            *p_err = FS_ERR_PARTITION_NOT_FOUND;
             p_vol->State = FS_VOL_STATE_PRESENT;
             return;


        case FS_ERR_DEV:                                         /* Allow dev to be conn'd later (see Note #3b).         */
        case FS_ERR_DEV_INVALID_LOW_FMT:
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        default:
             FSVol_ReleaseUnlock(p_vol);
             p_vol->State = FS_VOL_STATE_OPEN;
             return;
    }



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                          FSVol_IsMounted()
*
* Description : Determine whether a volume is mounted.
*
* Argument(s) : name_vol    Volume name.
*
* Return(s)   : DEF_YES if the volume is     open &  is     mounted.
*               DEF_NO  if the volume is not open or is not mounted.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSVol_IsMounted (CPU_CHAR  *name_vol)
{
    FS_VOL  *p_vol;
    FS_ERR   err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
        return (DEF_NO);
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, &err);      /* Vol MUST be mounted.                                 */
    (void)err;                                                 /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {
        return (DEF_NO);
    }



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                          FSVol_LabelGet()
*
* Description : Get volume label.
*
* Argument(s) : name_vol    Volume name.
*
*               label       String buffer that will receive volume label.
*
*               len_max     Size of string buffer (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   Label gotten.
*                               FS_ERR_DEV_CHNGD              Device has changed.
*                               FS_ERR_NAME_NULL              Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_NULL_PTR               Argument 'label' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_MOUNTED        Volume not mounted.
*                               FS_ERR_VOL_NOT_OPEN           Volume not open.
*
*                                                             ----- RETURNED BY FSSys_VolLabelSet() -----
*                               FS_ERR_DEV                    Device access error.
*                               FS_ERR_VOL_LABEL_NOT_FOUND    Volume label was not found.
*                               FS_ERR_VOL_LABEL_TOO_LONG     Volume label is too long.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'len_max' is the maximum length string that can be stored in the buffer 'label'; it
*                   does NOT include the final NULL character.  The buffer 'label' MUST be of at least
*                   'len_max' + 1 characters.
*********************************************************************************************************
*/

void  FSVol_LabelGet (CPU_CHAR    *name_vol,
                      CPU_CHAR    *label,
                      CPU_SIZE_T   len_max,
                      FS_ERR      *p_err)
{
    FS_VOL       *p_vol;
    CPU_BOOLEAN   empty;
    CPU_SIZE_T    pos;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (label == (CPU_CHAR *)0) {                               /* Validate label ptr.                                  */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (len_max == 0u) {                                        /* Validate buf length.                                 */
       *p_err = FS_ERR_DEV_INVALID_SIZE;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    if (p_vol == (FS_VOL *)0) {
        return;
    }


    label[0] = (CPU_CHAR)ASCII_CHAR_NULL;
                                                                /* ------------------- GET VOL LABEL ------------------ */
    FSSys_VolLabelGet(p_vol,
                      label,
                      len_max,
                      p_err);


    pos = (len_max > 12u) ? (11u) : (len_max - 1u);
    empty = DEF_YES;
    while(pos > 0u) {
        pos--;
        if(label[pos] != (CPU_CHAR)ASCII_CHAR_SPACE) {
            label[pos + 1u] = (CPU_CHAR)ASCII_CHAR_NULL;
            empty = DEF_NO;
            break;
        }
    }

    if(empty == DEF_YES) {
        label[0] = (CPU_CHAR)ASCII_CHAR_NULL;
    }

                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                          FSVol_LabelSet()
*
* Description : Set volume label.
*
* Argument(s) : name_vol    Volume name.
*
*               label       Volume label.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  Label set.
*                               FS_ERR_DEV_CHNGD             Device has changed.
*                               FS_ERR_NAME_NULL             Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_NULL_PTR              Argument 'label' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_MOUNTED       Volume not mounted.
*                               FS_ERR_VOL_NOT_OPEN          Volume not open.
*
*                                                            ------ RETURNED BY FSSys_VolLabelSet() -----
*                               FS_ERR_DEV                   Device access error.
*                               FS_ERR_DIR_FULL              Directory is full (space could not be allocated).
*                               FS_ERR_DEV_FULL              Device is full (space could not be allocated).
*                               FS_ERR_ENTRY_CORRUPT         File system entry is corrupt.
*                               FS_ERR_VOL_LABEL_INVALID     Volume label is invalid.
*                               FS_ERR_VOL_LABEL_TOO_LONG    Volume label is too long.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSVol_LabelSet (CPU_CHAR  *name_vol,
                      CPU_CHAR  *label,
                      FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (label == (CPU_CHAR *)0) {                               /* Validate label ptr.                                  */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    if (p_vol == (FS_VOL *)0) {
        return;
    }



                                                                /* ------------------- SET VOL LABEL ------------------ */
    FSSys_VolLabelSet(p_vol,
                      label,
                      p_err);



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                            FSVol_Open()
*
* Description : Open a volume & mount on the file system.
*
* Argument(s) : name_vol        Volume name.  See Note #1.
*
*               name_dev        Device name.
*
*               partition_nbr   Partition number.  If 0, the default partition will be mounted.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                   Volume opened.
*                                   FS_ERR_DEV_NOT_OPEN           Device is not open.
*                                   FS_ERR_NAME_NULL              Argument 'name_vol'/'name_dev' passed
*                                                                     a NULL pointer.
*                                   FS_ERR_VOL_ALREADY_OPEN       Volume is already open.
*                                   FS_ERR_VOL_INVALID_NAME       Volume name invalid.
*                                   FS_ERR_VOL_NONE_AVAIL         No volumes available.
*
*                                                                 --- RETURNED BY FSVol_OpenLocked() ---
*                                   FS_ERR_PARTITION_NOT_FOUND    No partition found.
*                                   FS_ERR_DEV                    Device error.
*                                   FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                                   FS_ERR_DEV_IO                 Device I/O error.
*                                   FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                                   FS_ERR_DEV_TIMEOUT            Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) Full valid path names follow the pattern
*
*                            [<vol_name>:]<... Path ...><File>
*
*                       where   <vol_name>     is the name of the volume, identical to the string
*                                              passed as 'name_vol' to this function..
*
*                               <... Path ...> is the file path, which MUST always begin AND end with a '\'.
*
*                               <File>         is the file (or leaf directory) name, including extension.
*
*                       For example :
*                                                      <vol_name>    <... Path ...>    <File>
*                           nand:0:\dir1\file.txt      nand:0            \dir1\        file.txt
*                           nand:0:\file.txt           nand:0            \             file.txt
*                           nand:\dir1\file.txt        nand              \dir1\        file.txt
*                           \dir1\file.txt                               \dir1\        file.txt
*                           \file.txt                                    \             file.txt
*
*                   (b) For file- & directory-access functions ('FSFile_Open()', etc.), the volume
*                       name MAY be omitted.  In this case, the volume used will be the FIRST volume
*                       opened.
*
*                   (c) A name is invalid if ...
*
*                       (1) ... it cannot be parsed :
*
*                           (a) Volume name too long.
*                           (b) First character is colon.
*
*                       (2) ... it includes text after a valid volume name.
*
*                       (3) ... it is an empty string.
*
*               (2) If FS_ERR_PARTITION_NOT_FOUND is returned, then no valid partition (or valid file
*                   system) was found on the device.  It is still placed into the list of used volumes;
*                   however, it cannot be addressed as a mounted volume (e.g., files cannot be accessed).
*                   Thereafter, unless a new device is inserted, the only valid commands are :
*
*                   (a) 'FSVol_Fmt()',   which places a file system on the device instance;
*                   (b) 'FSVol_Close()', which removes the entry from the list of volumes;
*                   (c) 'FSVol_Query()', which returns information about the device.
*
*               (3) If FS_ERR_DEV, FS_ERR_DEV_NOT_PRESENT, FS_ERR_DEV_IO or FS_ERR_DEV_TIMEOUT is returned,
*                   then the volume has been added to the file system, though the underlying device is
*                   probably not present.  The volume will need to be either closed & re-added, or
*                   refreshed.
*
*               (4) #### Support read-only volume access mode.
*********************************************************************************************************
*/

void  FSVol_Open (CPU_CHAR          *name_vol,
                  CPU_CHAR          *name_dev,
                  FS_PARTITION_NBR   partition_nbr,
                  FS_ERR            *p_err)
{
    FS_DEV       *p_dev;
    CPU_CHAR     *name_file;
    CPU_CHAR      name_vol_copy[FS_CFG_MAX_VOL_NAME_LEN + 1u];
    FS_VOL       *p_vol;
    CPU_BOOLEAN   lock_success;
    FS_ERR        err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ------------------ PARSE VOL NAME ------------------ */
    name_file = FS_PathParse(name_vol,                          /* Parse file name.                                     */
                             name_vol_copy,
                             p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

    if (name_file == (CPU_CHAR *)0) {                           /* Name could not be parsed (see Note #2c1).            */
       *p_err  = FS_ERR_VOL_INVALID_NAME;
        return;
    }

    if (*name_file != (CPU_CHAR)ASCII_CHAR_NULL) {              /* Name inc's text after good vol name (see Note #2c2). */
        *p_err = FS_ERR_VOL_INVALID_NAME;
         return;
    }

    if (name_vol_copy[0] == (CPU_CHAR)ASCII_CHAR_NULL) {        /* Vol name is empty str (see Note #2c3).               */
       *p_err = FS_ERR_VOL_INVALID_NAME;
        return;
    }



                                                                /* ---------------------- GET DEV --------------------- */
    p_dev = FSDev_Acquire(name_dev);                            /* Acquire ref to dev.                                  */
    if (p_dev == (FS_DEV *)0) {
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }



                                                                /* ------------------ INIT VOL STRUCT ----------------- */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock.                                     */
    if (*p_err != FS_ERR_NONE) {
        FSDev_Release(p_dev);
        return;
    }

    p_vol = FSVol_ObjFindEx(&name_vol_copy[0],                  /* Chk if vol is open.                                  */
                             p_dev,
                             partition_nbr);
    if (p_vol != (FS_VOL *)0) {
        FS_OS_Unlock();
        FSDev_Release(p_dev);
       *p_err = FS_ERR_VOL_ALREADY_OPEN;
        return;
    }

    p_vol = FSVol_ObjGet();                                     /* Alloc vol.                                           */
    if (p_vol == (FS_VOL *)0) {
        FS_OS_Unlock();
        FSDev_Release(p_dev);
       *p_err = FS_ERR_VOL_NONE_AVAIL;
        return;
    }

    p_vol->PartitionNbr = partition_nbr;
    p_vol->State        = FS_VOL_STATE_OPENING;
    p_vol->AccessMode   = FS_VOL_ACCESS_MODE_RDWR;              /* See Note #4.                                         */
    p_vol->DevPtr       = p_dev;
    p_vol->RefCnt       = 1u;
    Str_Copy_N(p_vol->Name, name_vol_copy, FS_CFG_MAX_VOL_NAME_LEN);

    FS_OS_Unlock();                                             /* Release FS lock.                                     */



                                                                /* ------------------- VALIDATE DEV ------------------- */
    FSDev_AccessLock(name_dev, 0u, p_err);                      /* Acquire dev access lock.                             */
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_OS_LOCK;
        FSDev_Release(p_dev);
        return;
    }

    lock_success = FSDev_Lock(p_dev);                           /* Acquire dev lock.                                    */
    if (lock_success != DEF_YES) {
       *p_err = FS_ERR_OS_LOCK;
        FSDev_AccessUnlock(name_dev, &err);
        (void)err;                                             /* Err ignored (Already in err state).                  */
        FSDev_Release(p_dev);
        return;
    }

    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:                                 /* Dev is not present or not low fmt'd ...              */
        case FS_DEV_STATE_PRESENT:
             p_vol->State = FS_VOL_STATE_OPEN;                  /* ... so vol is not open              ...              */
             FSDev_VolAdd(p_dev, p_vol);                        /* ... but still add vol to dev's list.                 */
            *p_err = FS_ERR_DEV_NOT_PRESENT;
             FSDev_Unlock(p_dev);
             FSDev_AccessUnlock(name_dev, &err);
             (void)err;                                        /* Err ignored (Already in err state).                  */
             return;


        case FS_DEV_STATE_LOW_FMT_VALID:
             break;


        case FS_DEV_STATE_CLOSED:                               /* Dev closing or not rdy ...                           */
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
             p_vol->State = FS_VOL_STATE_CLOSING;               /* ... so vol should not be opened.                     */
            *p_err = FS_ERR_DEV_NOT_OPEN;
             FSDev_Unlock(p_dev);
             FSDev_AccessUnlock(name_dev, &err);
             (void)err;                                        /* Err ignored (Already in err state).                  */
             FSDev_Release(p_dev);
             return;

    }



                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_OpenLocked(p_vol, p_err);                             /* Open vol.                                            */

    switch (*p_err) {
        case FS_ERR_NONE:                                       /* Vol is mounted.                                      */
             p_vol->State = FS_VOL_STATE_MOUNTED;
             break;


        case FS_ERR_PARTITION_NOT_FOUND:                        /* Partition/sys NOT valid ... vol NOT mounted.         */
             p_vol->State = FS_VOL_STATE_PRESENT;
             break;


        case FS_ERR_DEV:                                        /* Dev access error ... vol dev NOT present.            */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             p_vol->State = FS_VOL_STATE_OPEN;
             break;


        case FS_ERR_PARTITION_INVALID_NBR:
        default:
             p_vol->State = FS_VOL_STATE_CLOSING;
             FSDev_Unlock(p_dev);
             FSDev_AccessUnlock(name_dev, &err);
             (void)err;                                        /* Err ignored (Already in err state).                  */
             FSDev_Release(p_dev);
             return;
    }

    FSDev_VolAdd(p_dev, p_vol);                                 /* Add vol to dev's list.                               */

#ifdef FS_CACHE_MODULE_PRESENT
    p_vol->CacheDataPtr = (void         *)0;
    p_vol->CacheAPI_Ptr = (FS_VOL_CACHE_API *)0;
#endif



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_Unlock(p_vol);                                        /* Keep init ref.                                       */
}



/*
*********************************************************************************************************
*                                            FSVol_Query()
*
* Description : Obtain information about a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_info      Pointer to structure that will receive volume information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE            Volume information obtained.
*                               FS_ERR_DEV             Device access error.
*                               FS_ERR_NAME_NULL       Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_NULL_PTR        Argument 'p_info' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN    Volume not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSVol_Query (CPU_CHAR     *name_vol,
                   FS_VOL_INFO  *p_info,
                   FS_ERR       *p_err)
{
    FS_DEV_INFO   dev_info;
    FS_SYS_INFO   sys_info;
    FS_DEV       *p_dev;
    FS_VOL       *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_info == (FS_VOL_INFO *)0) {                           /* Validate info ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* Clr info.                                            */
    p_info->State         = FS_VOL_STATE_CLOSED;
    p_info->DevState      = FS_DEV_STATE_CLOSED;
    p_info->DevSize       = 0u;
    p_info->DevSecSize    = 0u;
    p_info->PartitionSize = 0u;
    p_info->VolBadSecCnt  = 0u;
    p_info->VolUsedSecCnt = 0u;
    p_info->VolFreeSecCnt = 0u;
    p_info->VolTotSecCnt  = 0u;



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_NO, p_err);      /* Vol may be unmounted.                                */
    if (p_vol == (FS_VOL *)0) {
        return;
    }



                                                                /* -------------------- GET DEV INFO ------------------ */
    p_dev = p_vol->DevPtr;
    if ((p_vol->State == FS_VOL_STATE_PRESENT) ||
        (p_vol->State == FS_VOL_STATE_MOUNTED)) {
        FSDev_QueryLocked( p_dev,
                          &dev_info,
                           p_err);
        if (*p_err != FS_ERR_NONE) {
            *p_err  = FS_ERR_DEV;
             FSVol_ReleaseUnlock(p_vol);
             return;
        }

        p_info->DevState   = dev_info.State;
        p_info->DevSize    = dev_info.Size;
        p_info->DevSecSize = dev_info.SecSize;

    } else {
        p_info->DevState   = p_dev->State;
    }



                                                                /* -------------------- GET SYS INFO ------------------ */
    if (p_vol->State == FS_VOL_STATE_MOUNTED) {                 /* Vol mounted ... get sys info.                        */
        p_info->State = FS_VOL_STATE_MOUNTED;
        FSSys_VolQuery( p_vol,
                       &sys_info,
                        p_err);
        if (*p_err == FS_ERR_NONE) {
            p_info->PartitionSize = p_vol->PartitionSize;
            p_info->VolBadSecCnt  = sys_info.BadSecCnt;
            p_info->VolFreeSecCnt = sys_info.FreeSecCnt;
            p_info->VolUsedSecCnt = sys_info.UsedSecCnt;
            p_info->VolTotSecCnt  = sys_info.TotSecCnt;
        }

    } else {                                                    /* Vol not mounted.                                     */
        p_info->State         = p_vol->State;
        p_info->PartitionSize = p_vol->PartitionSize;
    }



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                             FSVol_Rd()
*
* Description : Read data from volume sector(s).
*
* Argument(s) : name_vol    Volume name.
*
*               p_dest      Pointer to destination buffer.
*
*               start       Start sector of read.
*               -----       Argument checked in 'FSVol_RdLocked()'.
*
*               cnt         Number of sectors to read.
*               ---         Argument checked in 'FSVol_RdLocked()'.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE               Sector(s) read.
*                               FS_ERR_DEV                Device access error.
*                               FS_ERR_DEV_CHNGD          Device has changed.
*                               FS_ERR_NAME_NULL          Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_dest' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_MOUNTED    Volume not mounted.
*                               FS_ERR_VOL_NOT_OPEN       Volume not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSVol_Rd (CPU_CHAR    *name_vol,
                void        *p_dest,
                FS_SEC_NBR   start,
                FS_SEC_QTY   cnt,
                FS_ERR      *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_dest == (void *)0) {                                  /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {
        return;
    }



                                                                /* ---------------------- RD VOL ---------------------- */
    FSVol_RdLocked(p_vol,
                   p_dest,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                             FSVol_Refresh()
*
* Description : Refresh a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Device refreshed.
*                               FS_ERR_NAME_NULL               Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_VOL_OPEN            Volume open on device.
*
*                                                              ---------- FSDev_RefreshLocked() ---------
*                               FS_ERR_DEV_INVALID_SEC_SIZE    Invalid device sector size.
*                               FS_ERR_DEV_INVALID_SIZE        Invalid device size.
*                               FS_ERR_DEV_UNKNOWN             Unknown device error.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*                               FS_ERR_OS_LOCK                 OS Lock NOT acquired.
*
* Return(s)   : DEF_YES, if volume has     changed.
*               DEF_NO,  if volume has NOT changed.
*
* Note(s)     : (1) If device has changed, all volumes open on the device must be refreshed & all files
*                   closed and reopened.
*
*               (2) Errors resulting from device access are handled in 'FSDev_WrLocked()'.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSVol_Refresh (CPU_CHAR  *name_vol,
                            FS_ERR    *p_err)
{
    FS_VOL       *p_vol;
    CPU_BOOLEAN   chngd;
    CPU_BOOLEAN   vol_lock_ok;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_NO);
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_Acquire(name_vol, p_err);

    if (p_vol == (FS_VOL *)0) {                                 /* Rtn err if vol not found.                            */
       *p_err = FS_ERR_VOL_NOT_OPEN;
        return (DEF_NO);
    }

    vol_lock_ok = FSVol_Lock(p_vol);
    if (vol_lock_ok == DEF_NO) {
        FSVol_Release(p_vol);
       *p_err = FS_ERR_OS_LOCK;
        return (DEF_NO);
    }


                                                                /* ------------------- VALIDATE VOL ------------------- */
    switch (p_vol->State) {
        case FS_VOL_STATE_OPEN:
        case FS_VOL_STATE_PRESENT:
        case FS_VOL_STATE_MOUNTED:
             break;


        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:
        default:
             FSVol_ReleaseUnlock(p_vol);
            *p_err = FS_ERR_VOL_NOT_OPEN;
             return (DEF_NO);
    }



                                                                /* -------------------- REFRESH VOL ------------------- */
   (void)FSDev_RefreshLocked(p_vol->DevPtr, p_err);             /* Refresh dev.                                         */
    if (*p_err != FS_ERR_NONE) {
        FSVol_ReleaseUnlock(p_vol);
        return (DEF_NO);
    }

    chngd = FSVol_RefreshLocked(p_vol, p_err);                  /* Refresh vol.                                         */



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);

    return (chngd);
}


/*
*********************************************************************************************************
*                                             FSVol_Wr()
*
* Description : Write data to volume sector(s).
*
* Argument(s) : name_vol    Volume name.
*
*               p_src       Pointer to source buffer.
*
*               start       Start sector of read.
*               -----       Argument checked in 'FSVol_WrLocked()'.
*
*               cnt         Number of sectors to read.
*               ---         Argument checked in 'FSVol_WrLocked()'.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE               Sector(s) written.
*                               FS_ERR_DEV                Device access error.
*                               FS_ERR_DEV_CHNGD          Device has changed.
*                               FS_ERR_NAME_NULL          Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_NULL_PTR           Argument 'p_src' passed a NULL pointer.
*                               FS_ERR_VOL_INVALID_OP     Invalid operation on volume.
*                               FS_ERR_VOL_NOT_MOUNTED    Volume not mounted.
*                               FS_ERR_VOL_NOT_OPEN       Volume not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSVol_Wr (CPU_CHAR    *name_vol,
                void        *p_src,
                FS_SEC_NBR   start,
                FS_SEC_QTY   cnt,
                FS_ERR      *p_err)
{
    FS_VOL  *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_src == (void *)0) {                                   /* Validate src ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol MUST be mounted.                                 */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {
        return;
    }

                                                                /* Chk vol mode.                                        */
    if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR) == DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }



                                                                /* ---------------------- WR VOL ---------------------- */
    FSVol_WrLocked(p_vol,
                   p_src,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     VOLUME MANAGEMENT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSVol_GetVolCnt()
*
* Description : Get the number of open volumes.
*
* Argument(s) : none.
*
* Return(s)   : The number of open volumes.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSVol_GetVolCnt (void)
{
    FS_QTY  vol_cnt;
    FS_ERR  err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return (0u);
    }



                                                                /* -------------------- GET VOL CNT ------------------- */
    vol_cnt = FSVol_Cnt;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (vol_cnt);
}


/*
*********************************************************************************************************
*                                        FSVol_GetVolCntMax()
*
* Description : Get the maximum possible number of open volumes.
*
* Argument(s) : none.
*
* Return(s)   : The maximum number of open volumes.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSVol_GetVolCntMax (void)
{
    FS_QTY  vol_cnt_max;


                                                                /* ------------------ GET VOL CNT MAX ----------------- */
    vol_cnt_max = FSVol_VolCntMax;

    return (vol_cnt_max);
}


/*
*********************************************************************************************************
*                                         FSVol_GetVolName()
*
* Description : Get name of nth open volume.
*
* Argument(s) : vol_nbr     Number of volume to get.
*
*               name_vol    String buffer that will receive the volume name (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                 Volume name obtained.
*                           FS_ERR_NAME_NULL            Argument 'name_vol' passed a NULL pointer.
*                           FS_ERR_POOL_FULL            ALL blocks already available in pool
*                           FS_ERR_NULL_PTR             Argument 'p_pool' passed a NULL pointer.
*                           FS_ERR_INVALID_TYPE         Invalid pool type.
*                           FS_ERR_OS_LOCK              File system access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'name_vol' MUST be a character array of 'FS_CFG_MAX_VOL_NAME_LEN + 1' characters.
*
*               (2) If the volume does not exist 'name_vol' will receive an empty string.
*********************************************************************************************************
*/

void  FSVol_GetVolName (FS_QTY     vol_nbr,
                        CPU_CHAR  *name_vol,
                        FS_ERR    *p_err)
{
    FS_QTY    vol_ix;
    FS_QTY    vol_cnt_cur;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

   *p_err       = FS_ERR_NONE;
    name_vol[0] = (CPU_CHAR)ASCII_CHAR_NULL;                    /* See Note #2.                                         */



                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ------------------- GET VOL NAME ------------------- */
    vol_cnt_cur = 0u;
    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] != DEF_NULL) {
            if (vol_cnt_cur == vol_nbr) {
                Str_Copy_N(name_vol, FSVol_Tbl[vol_ix]->Name, FS_CFG_MAX_VOL_NAME_LEN);
                break;
            } else {
                vol_cnt_cur++;
            }
        }
    }


                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*                                       FSVol_GetDfltVolName()
*
* Description : Get name of the default volume.
*
* Argument(s) : name_vol    String buffer that will receive the volume name (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                 Volume name obtained.
*                           FS_ERR_NAME_NULL            Argument 'name_vol' passed a NULL pointer.
*                           FS_ERR_POOL_FULL            ALL blocks already available in pool
*                           FS_ERR_NULL_PTR             Argument 'p_pool' passed a NULL pointer.
*                           FS_ERR_INVALID_TYPE         Invalid pool type.
*                           FS_ERR_OS_LOCK              File system access NOT acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'name_vol' MUST be a character array of 'FS_CFG_MAX_VOL_NAME_LEN + 1' characters.
*
*               (2) If the volume does not exist 'name_vol' will receive an empty string.
*********************************************************************************************************
*/

void  FSVol_GetDfltVolName (CPU_CHAR  *name_vol,
                            FS_ERR    *p_err)
{
    FS_QTY    vol_ix;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

   *p_err       = FS_ERR_NONE;
    name_vol[0] = (CPU_CHAR)ASCII_CHAR_NULL;                    /* See Note #2.                                         */



                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ------------------- GET VOL NAME ------------------- */
    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] != DEF_NULL) {
            Str_Copy_N(name_vol, FSVol_Tbl[vol_ix]->Name, FS_CFG_MAX_VOL_NAME_LEN);
            break;
        }
    }



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*                                           FSVol_IsDflt()
*
* Description : Determine whether a volume is the default volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                 Volume name obtained.
*                           FS_ERR_NAME_NULL            Argument 'name_vol' passed a NULL pointer.
*                           FS_ERR_POOL_FULL            ALL blocks already available in pool
*                           FS_ERR_NULL_PTR             Argument 'p_pool' passed a NULL pointer.
*                           FS_ERR_INVALID_TYPE         Invalid pool type.
*                           FS_ERR_OS_LOCK              File system access NOT acquired.
*
* Return(s)   : DEF_YES, if the volume with name 'name_vol' is the default volume.
*               DEF_NO,  if no  volume with name 'name_vol' exists
*                        or the volume with name 'name_vol' is not the default volume.

*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSVol_IsDflt (CPU_CHAR  *name_vol,
                           FS_ERR    *p_err)
{
    FS_VOL      *p_vol;
    CPU_INT16S   cmp;
    CPU_BOOLEAN  dflt;
    FS_QTY       vol_ix;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_NO);
    }
#endif


   *p_err = FS_ERR_NONE;
                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }


                                                                /* --------------------- GET DFLT --------------------- */
    p_vol = DEF_NULL;
    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] != DEF_NULL) {
            p_vol = FSVol_Tbl[vol_ix];
            break;
        }
    }

    if (p_vol != (FS_VOL *)0) {
        cmp  = Str_Cmp_N(name_vol, p_vol->Name, FS_CFG_MAX_VOL_NAME_LEN);
        dflt = (cmp == 0) ? DEF_YES : DEF_NO;
    } else {
        dflt = DEF_NO;
    }


                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (dflt);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSVol_ModuleInit()
*
* Description : Initialize File System Volume Management module.
*
* Argument(s) : vol_cnt     Number of volumes to allocate.
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

void  FSVol_ModuleInit (FS_QTY   vol_cnt,
                        FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


#if (FS_CFG_TRIAL_EN == DEF_ENABLED)                            /* Trial limitation: max 1 vol.                         */
    if (vol_cnt > FS_TRIAL_MAX_VOL_CNT) {
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif

    FSVol_VolCntMax = 0u;
    FSVol_Cnt       = 0u;


                                                                /* ------------------- INIT VOL POOL ------------------ */
    Mem_PoolCreate(&FSVol_Pool,
                    DEF_NULL,
                    0,
                    vol_cnt,
                    sizeof(FS_VOL),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSVol_ModuleInit(): Could not alloc mem for vols: %d octets required.\r\n", octets_reqd));
        return;
    }

    FSVol_Tbl = (FS_VOL **)Mem_HeapAlloc(vol_cnt * sizeof(FS_VOL *),
                                         sizeof(CPU_ALIGN),
                                        &octets_reqd,
                                        &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSVol_ModuleInit(): Could not alloc mem for vols: %d octets required.\r\n", octets_reqd));
        return;
    }

    Mem_Clr(FSVol_Tbl, vol_cnt * sizeof(FS_VOL *));

    FSVol_VolCntMax = vol_cnt;
}


/*
*********************************************************************************************************
*                                       FSVol_AcquireLockChk()
*
* Description : Acquire volume reference & lock.
*
* Argument(s) : name_vol    Volume name.
*
*               mounted     Indicates whether volume must be mounted :
*
*                               DEF_YES    Volume must be mounted.
*                               DEF_NO     Volume must be mounted or open.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE               Volume present & lock acquired.
*                               FS_ERR_DEV_CHNGD          Device has changed.
*                               FS_ERR_VOL_NOT_OPEN       Volume is not open.
*                               FS_ERR_VOL_NOT_MOUNTED    Volume is not mounted.
*                               FS_ERR_OS_LOCK            OS Lock NOT acquired.
*
* Return(s)   : Pointer to a volume, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_VOL  *FSVol_AcquireLockChk (CPU_CHAR     *name_vol,
                               CPU_BOOLEAN   mounted,
                               FS_ERR       *p_err)
{
    FS_VOL      *p_vol;
    CPU_BOOLEAN  vol_lock_ok;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION((FS_VOL *)0);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_NO);
    }
#endif

                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_Acquire(name_vol, p_err);                     /* Acquire vol ref.                                     */

    if (p_vol == (FS_VOL *)0) {                                 /* Rtn err if vol not found.                            */
       *p_err = FS_ERR_VOL_NOT_OPEN;
        return ((FS_VOL *)0);
    }

    vol_lock_ok = FSVol_Lock(p_vol);
    if (vol_lock_ok == DEF_NO) {
        FSVol_Release(p_vol);
       *p_err = FS_ERR_OS_LOCK;
        return ((FS_VOL *)0);
    }

                                                                /* ------------------- VALIDATE VOL ------------------- */
    (void)FSVol_RefreshLocked(p_vol, p_err);                    /* Refresh vol.                                         */

    switch (p_vol->State) {
        case FS_VOL_STATE_OPEN:
        case FS_VOL_STATE_PRESENT:
             if (mounted == DEF_YES) {                          /* If vol MUST be mounted ...                           */
                 FSVol_ReleaseUnlock(p_vol);
                 return ((FS_VOL *)0);
             }
            *p_err = FS_ERR_NONE;                               /* Refresh expected to fail when vol not mounted.       */
             break;


        case FS_VOL_STATE_MOUNTED:
             break;


        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:
        default:
             FSVol_ReleaseUnlock(p_vol);
             return ((FS_VOL *)0);
    }



                                                                /* ------------------------ RTN ----------------------- */
    return (p_vol);
}


/*
*********************************************************************************************************
*                                           FSVol_Acquire()
*
* Description : Acquire volume reference.
*
* Argument(s) : name_vol    Volume name.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE               Volume present & lock acquired.
*
* Return(s)   : Pointer to a volume, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_VOL  *FSVol_Acquire (CPU_CHAR  *name_vol,
                        FS_ERR    *p_err)
{
    FS_VOL  *p_vol;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_OS_LOCK;
        return ((FS_VOL *)0);
    }



                                                                /* --------------------- FIND VOL --------------------- */
    p_vol = FSVol_ObjFind(name_vol, p_err);                     /* Find vol.                                            */

    if (p_vol == (FS_VOL *)0) {                                 /* Rtn NULL if vol not found.                           */
        FS_OS_Unlock();
       *p_err = FS_ERR_VOL_NOT_MOUNTED;
        return ((FS_VOL *)0);
    }

    p_vol->RefCnt++;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (p_vol);
}


/*
*********************************************************************************************************
*                                         FSVol_AcquireDflt()
*
* Description : Acquire default volume reference.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a volume, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_VOL  *FSVol_AcquireDflt (void)
{
    FS_VOL   *p_vol;
    FS_ERR    err;
    FS_QTY    vol_ix;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return ((FS_VOL *)0);
    }



                                                                /* --------------------- FIND VOL --------------------- */
                                                                /* Try to find vol ...                                  */
    p_vol = DEF_NULL;
    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] != DEF_NULL) {
            p_vol = FSVol_Tbl[vol_ix];
            break;
        }
    }

    if (p_vol != DEF_NULL) {                                    /* ... rtn NULL if vol not found.                       */
       p_vol->RefCnt++;
    }


                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (p_vol);
}


/*
*********************************************************************************************************
*                                        FSVol_ReleaseUnlock()
*
* Description : Release volume reference & lock.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSVol_ReleaseUnlock (FS_VOL  *p_vol)
{
    FSVol_Unlock(p_vol);
    FSVol_Release(p_vol);
}


/*
*********************************************************************************************************
*                                           FSVol_Release()
*
* Description : Release volume reference.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The device reference is acquired in 'FSVol_Open()'.
*********************************************************************************************************
*/

void  FSVol_Release (FS_VOL  *p_vol)
{
    FS_ERR   err;
    FS_CTR   ref_cnt;
    FS_DEV  *p_dev;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return;
    }



                                                                /* -------------------- DEC REF CNT ------------------- */
    ref_cnt = p_vol->RefCnt;
    if (ref_cnt == 1u) {
        p_dev = p_vol->DevPtr;
        FSVol_ObjFree(p_vol);
    } else if (ref_cnt > 0u) {
        p_vol->RefCnt = ref_cnt - 1u;
    } else {
        FS_TRACE_DBG(("FSVol_Release(): Release cnt dec'd to zero.\r\n"));
    }

    FS_OS_Unlock();



                                                                /* -------------------- RELEASE DEV ------------------- */
    if (ref_cnt == 1u) {
        FSDev_Release(p_dev);                                   /* Release dev (see Note #1).                           */
    }
}


/*
*********************************************************************************************************
*                                            FSVol_Lock()
*
* Description : Acquire volume lock.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
* Return(s)   : DEF_YES, if volume lock     acquired.
*               DEF_NO,  if volume lock NOT acquired.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSVol_Lock (FS_VOL  *p_vol)
{
    FS_ERR  err;

                                                                /* -------------- ACQUIRE DEV ACCESS LOCK ------------- */
    FS_OS_DevAccessLock(p_vol->DevPtr->ID, 0u, &err);
    if (err != FS_ERR_NONE) {
        return (DEF_NO);
    }

                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    FS_OS_DevLock(p_vol->DevPtr->ID, &err);
    if (err != FS_ERR_NONE) {
        FS_OS_DevAccessUnlock(p_vol->DevPtr->ID);
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                           FSVol_Unlock()
*
* Description : Release volume lock.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSVol_Unlock (FS_VOL  *p_vol)
{
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FS_OS_DevUnlock(p_vol->DevPtr->ID);

                                                                /* -------------- RELEASE DEV ACCESS LOCK ------------- */
    FS_OS_DevAccessUnlock(p_vol->DevPtr->ID);
}


/*
*********************************************************************************************************
*                                         FSVol_OpenLocked()
*
* Description : Open a volume & mount on the file system.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Volume opened.
*                               FS_ERR_PARTITION_NOT_FOUND    No partition found.
*
*                                                             ---------- RETURNED BY FSSys_Open() ----------
*                                                             ------- RETURNED BY FSPartition_Find() -------
*                               FS_ERR_DEV                    Device error.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

static  void  FSVol_OpenLocked (FS_VOL  *p_vol,
                                FS_ERR  *p_err)
{
    FS_PARTITION_ENTRY  partition_entry;
    FS_SEC_QTY          partition_size;
    FS_SEC_NBR          partition_start;


                                                                /* ------------------ FIND PARTITION ------------------ */
#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
    FSPartition_Find( p_vol->DevPtr,                            /* Get partition params.                                */
                      p_vol->PartitionNbr,
                     &partition_entry,
                      p_err);
#else
    FSPartition_FindSimple( p_vol->DevPtr,                      /* Get partition params.                                */
                           &partition_entry,
                            p_err);
#endif

    switch (*p_err) {
        case FS_ERR_NONE:
             partition_start = partition_entry.Start;
             partition_size  = partition_entry.Size;

             if (partition_start > p_vol->DevPtr->Size) {       /* Verify partition info.                               */
                 partition_start = 0u;
                 partition_size  = p_vol->DevPtr->Size;
             }
             break;


        case FS_ERR_PARTITION_INVALID:                          /* Dev not partitioned.                                 */
        case FS_ERR_PARTITION_ZERO:
             partition_start = 0u;
             partition_size  = p_vol->DevPtr->Size;
             break;


        case FS_ERR_PARTITION_INVALID_SIG:                      /* If no partition ...                                  */
             partition_start = 0u;
             partition_size  = p_vol->DevPtr->Size;
             break;


        case FS_ERR_PARTITION_INVALID_NBR:
             if (p_vol->PartitionNbr == 0u) {                   /* Dev not partitioned.                                 */
                 partition_start = 0u;
                 partition_size  = p_vol->DevPtr->Size;
             } else {                                           /* If no partition ...                                  */
                 p_vol->State = FS_VOL_STATE_PRESENT;           /* ... vol not mounted.                                 */
                *p_err = FS_ERR_PARTITION_NOT_FOUND;
                 return;
             }
             break;


        case FS_ERR_DEV:                                        /* If other err ...                                     */
        default:
             p_vol->State = FS_VOL_STATE_OPEN;                  /* ... vol dev not present.                             */
             return;
    }

    p_vol->PartitionStart = partition_start;
    p_vol->PartitionSize  = partition_size;
    p_vol->RefreshCnt     = p_vol->DevPtr->RefreshCnt;



                                                                /* -------------------- INIT VOL FS ------------------- */
    FSSys_VolOpen(p_vol, p_err);                                /* Get file sys properties.                             */
    switch (*p_err) {
        case FS_ERR_NONE:                                       /* If vol opened ...                                    */
             p_vol->State   = FS_VOL_STATE_MOUNTED;             /* ... vol mounted.                                     */
             p_vol->SecSize = p_vol->DevPtr->SecSize;
             break;


        case FS_ERR_VOL_INVALID_SYS:                            /* If no partition or sys found ...                     */
        case FS_ERR_PARTITION_NOT_FOUND:
             p_vol->State   = FS_VOL_STATE_PRESENT;             /* ... vol not mounted.                                 */
             p_vol->SecSize = p_vol->DevPtr->SecSize;
            *p_err = FS_ERR_PARTITION_NOT_FOUND;
             break;


                                                               /* If other err ...                                     */
        case FS_ERR_DEV:
        default:
             p_vol->State = FS_VOL_STATE_OPEN;                  /* ... vol dev not present.                             */
             return;
    }
}


/*
*********************************************************************************************************
*                                          FSVol_RdLocked()
*
* Description : Read data from volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ------      Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
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

void  FSVol_RdLocked (FS_VOL      *p_vol,
                      void        *p_dest,
                      FS_SEC_NBR   start,
                      FS_SEC_QTY   cnt,
                      FS_ERR      *p_err)
{
    FS_DEV      *p_dev;
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    FS_SEC_QTY   size;
#endif


                                                               /* ------------------ VALIDATE ARGS ------------------- */
    p_dev = p_vol->DevPtr;

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    size  = p_vol->PartitionSize;
    if (start + cnt > size) {                                   /* Validate start & cnt.                                */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }
#endif

    start += p_vol->PartitionStart;


                                                                /* ---------------------- RD DEV ---------------------- */
    FSDev_RdLocked(p_dev,
                   p_dest,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- UPDATE VOL STATS ----------------- */
    FS_CTR_STAT_ADD(p_vol->StatRdSecCtr, (FS_CTR)cnt);
}



/*
*********************************************************************************************************
*                                         FSVol_RdLockedEx()
*
* Description : Read data from volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ------      Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
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

void  FSVol_RdLockedEx (FS_VOL      *p_vol,
                        void        *p_dest,
                        FS_SEC_NBR   start,
                        FS_SEC_QTY   cnt,
                        FS_FLAGS     sec_type,
                        FS_ERR      *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    FS_SEC_QTY  size;
#endif


    (void)sec_type;
                                                                /* ------------------ VALIDATE ARGS ------------------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    size = p_vol->PartitionSize;
    if (start + cnt > size) {                                   /* Validate start & cnt.                                */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }
#endif

                                                                /* -------------- CHECK VOLUME VALIDITY --------------- */
    if (p_vol->RefreshCnt != p_vol->DevPtr->RefreshCnt) {       /* Volume is invalid following a device change...       */
    	*p_err = FS_ERR_DEV_CHNGD;                              /* cannot continue, return with error.                  */
    	 return;
    }


#ifdef FS_CACHE_MODULE_PRESENT                                  /* ------------------ RD CACHED DATA ------------------ */
    if (p_vol->CacheAPI_Ptr != (FS_VOL_CACHE_API *)0) {
        p_vol->CacheAPI_Ptr->Rd(p_vol,
                                p_dest,
                                start,
                                cnt,
                                sec_type,
                                p_err);
        FS_CTR_STAT_ADD(p_vol->StatRdSecCtr, (FS_CTR)cnt);
        return;
    }
#endif



                                                                /* ---------------------- RD DEV ---------------------- */
    start += p_vol->PartitionStart;
    FSDev_RdLocked(p_vol->DevPtr,
                   p_dest,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- UPDATE VOL STATS ----------------- */
    FS_CTR_STAT_ADD(p_vol->StatRdSecCtr, (FS_CTR)cnt);
}


/*
*********************************************************************************************************
*                                          FSVol_RefreshLocked()
*
* Description : Refresh a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_NONE                    Volume refreshed.
*
*                                                              ---- RETURNED BY FSDev_RefreshLocked() ---
*                                                              ----- RETURNED BY FSVol_OpenLocked() -----
*                               FS_ERR_DEV_INVALID_SEC_SIZE    Invalid device sector size.
*                               FS_ERR_DEV_INVALID_SIZE        Invalid device size.
*                               FS_ERR_DEV_UNKNOWN             Unknown device error.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : DEF_YES, if volume has     changed.
*               DEF_NO,  if volume has NOT changed.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSVol_RefreshLocked (FS_VOL  *p_vol,
                                  FS_ERR  *p_err)
{
    CPU_BOOLEAN  chngd;


    switch (p_vol->State) {
        case FS_VOL_STATE_OPEN:
        case FS_VOL_STATE_PRESENT:
        case FS_VOL_STATE_MOUNTED:
             break;


                                                                /* If vol not open ...                                  */
        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:
        default:
            *p_err = FS_ERR_VOL_NOT_OPEN;
             return (DEF_NO);                                   /*                 ... vol not chngd.                   */
    }


    chngd = DEF_NO;

    if (p_vol->State == FS_VOL_STATE_MOUNTED) {                 /* If vol was mounted ...                               */
        if (p_vol->RefreshCnt == p_vol->DevPtr->RefreshCnt) {
           *p_err = FS_ERR_NONE;
            return (DEF_NO);
        }

        if(p_vol->RefreshCnt != p_vol->DevPtr->RefreshCnt) {    /*                    ... but dev chngd ...             */
        	if (p_vol->RefCnt > 2u) {                           /*      ... check if files are open on the volume ...   */
        		p_vol->State = FS_VOL_STATE_OPEN;               /*      ... if yes, cannot close...                     */
        	   *p_err = FS_ERR_DEV_CHNGD;
        		chngd  = DEF_YES;
        		return (chngd);
        	} else {
        		FSSys_VolClose(p_vol);                          /*                            ... otherwise close vol.  */
                p_vol->State = FS_VOL_STATE_OPEN;
                chngd        = DEF_YES;
        	}
        }
    }

    if ((p_vol->State == FS_VOL_STATE_OPEN) ||                  /* Vol not mounted ...                                  */
        (p_vol->State == FS_VOL_STATE_PRESENT)) {
        if (p_vol->DevPtr->State != FS_DEV_STATE_LOW_FMT_VALID) {
           (void)FSDev_RefreshLocked(p_vol->DevPtr, p_err);     /*                ... refresh dev ...                   */

            if (*p_err != FS_ERR_NONE) {
                return (chngd);
            }
        }

        if (p_vol->RefCnt > 2u) {                               /* Cannot reopen automatically if files are open.       */
        	*p_err = FS_ERR_DEV_CHNGD;
        	return (chngd);
        }

        FSVol_OpenLocked(p_vol, p_err);                         /*                                ... reopen vol.       */

        if (*p_err == FS_ERR_NONE) {
            chngd = DEF_YES;
        }
    }

    return (chngd);
}


/*
*********************************************************************************************************
*                                        FSVol_ReleaseLocked()
*
* Description : Release volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               start       Start sector of release.
*
*               cnt         Number of sectors to release.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
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

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSVol_ReleaseLocked (FS_VOL      *p_vol,
                           FS_SEC_NBR   start,
                           FS_SEC_QTY   cnt,
                           FS_ERR      *p_err)
{
    FS_SEC_QTY  size;


                                                                /* ------------------ VALIDATE ARGS ------------------- */
    size = p_vol->PartitionSize;
    if (start > size) {                                         /* Validate start.                                      */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }
    if (start + cnt - 1u > size) {                              /* Validate cnt.                                        */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }



#ifdef FS_CACHE_MODULE_PRESENT                                  /* --------------- RELEASED CACHED DATA --------------- */
    if (p_vol->CacheAPI_Ptr != (FS_VOL_CACHE_API *)0) {
        p_vol->CacheAPI_Ptr->Release(p_vol,
                                     start,
                                     cnt,
                                     p_err);
    }
#endif



                                                                /* ----------------- RELEASE DEV SECS ----------------- */
    start += p_vol->PartitionStart;
    FSDev_ReleaseLocked(p_vol->DevPtr,
                        start,
                        cnt,
                        p_err);
}
#endif


/*
*********************************************************************************************************
*                                          FSVol_WrLocked()
*
* Description : Write data to volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               -----       Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
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
void  FSVol_WrLocked (FS_VOL      *p_vol,
                      void        *p_src,
                      FS_SEC_NBR   start,
                      FS_SEC_QTY   cnt,
                      FS_ERR      *p_err)
{
    FS_DEV      *p_dev;
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    FS_SEC_QTY   size;
#endif


                                                                /* ------------------ VALIDATE ARGS ------------------- */
    p_dev = p_vol->DevPtr;

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    size  = p_vol->PartitionSize;
    if (start + cnt > size) {                                   /* Validate start & cnt.                                */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }
#endif

    start += p_vol->PartitionStart;



                                                                /* ---------------------- WR DEV ---------------------- */
    FSDev_WrLocked(p_dev,
                   p_src,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- UPDATE VOL STATS ----------------- */
    FS_CTR_STAT_ADD(p_vol->StatWrSecCtr, (FS_CTR)cnt);
}
#endif


/*
*********************************************************************************************************
*                                         FSVol_WrLockedEx()
*
* Description : Write data to volume sector(s).
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               -----       Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
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
void  FSVol_WrLockedEx (FS_VOL      *p_vol,
                        void        *p_src,
                        FS_SEC_NBR   start,
                        FS_SEC_QTY   cnt,
                        FS_FLAGS     sec_type,
                        FS_ERR      *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    FS_SEC_QTY  size;
#endif


    (void)sec_type;
                                                                /* ------------------ VALIDATE ARGS ------------------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    size = p_vol->PartitionSize;
    if (start + cnt > size) {                                   /* Validate start & cnt.                                */
       *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
        return;
    }
#endif

                                                                /* -------------- CHECK VOLUME VALIDITY --------------- */
    if (p_vol->RefreshCnt != p_vol->DevPtr->RefreshCnt) {       /* Volume is invalid following a device change...       */
       *p_err = FS_ERR_DEV_CHNGD;                               /* cannot continue, return with error.                  */
        return;
    }


#ifdef FS_CACHE_MODULE_PRESENT                                  /* ----------------- WR THROUGH CACHE ----------------- */
    if (p_vol->CacheAPI_Ptr != (FS_VOL_CACHE_API *)0) {
        p_vol->CacheAPI_Ptr->Wr(p_vol,
                                p_src,
                                start,
                                cnt,
                                sec_type,
                                p_err);
        FS_CTR_STAT_ADD(p_vol->StatWrSecCtr, (FS_CTR)cnt);
        return;
    }
#endif



                                                                /* ---------------------- WR DEV ---------------------- */
    start += p_vol->PartitionStart;
    FSDev_WrLocked(p_vol->DevPtr,
                   p_src,
                   start,
                   cnt,
                   p_err);



                                                                /* ----------------- UPDATE VOL STATS ----------------- */
    FS_CTR_STAT_ADD(p_vol->StatWrSecCtr, (FS_CTR)cnt);
}
#endif


/*
*********************************************************************************************************
*                                           FSVol_DirAdd()
*
* Description : Add directory to open directory list.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

#ifdef FS_DIR_MODULE_PRESENT
void  FSVol_DirAdd (FS_VOL  *p_vol,
                    FS_DIR  *p_dir)
{
   (void)p_dir;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */

    p_vol->DirCnt++;
}
#endif


/*
*********************************************************************************************************
*                                          FSVol_DirRemove()
*
* Description : Remove directory from open directory list.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

#ifdef FS_DIR_MODULE_PRESENT
void  FSVol_DirRemove (FS_VOL  *p_vol,
                       FS_DIR  *p_dir)
{
   (void)p_dir;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */

    if (p_vol->DirCnt > 0u) {
        p_vol->DirCnt--;
    }
}
#endif


/*
*********************************************************************************************************
*                                           FSVol_FileAdd()
*
* Description : Add file to open file list.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
*               p_file      Pointer to file.
*               ------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

void  FSVol_FileAdd (FS_VOL   *p_vol,
                     FS_FILE  *p_file)
{
   (void)p_file;                                                /*lint --e{550} Suppress "Symbol not accessed".         */

    p_vol->FileCnt++;
}


/*
*********************************************************************************************************
*                                         FSVol_FileRemove()
*
* Description : Remove file from open file list.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
*               p_file      Pointer to file.
*               ------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the volume & hold the device lock.
*********************************************************************************************************
*/

void  FSVol_FileRemove (FS_VOL   *p_vol,
                        FS_FILE  *p_file)
{
   (void)p_file;                                                /*lint --e{550} Suppress "Symbol not accessed".         */

    if (p_vol->FileCnt > 0u) {
        p_vol->FileCnt--;
    }
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
*                                           FSVol_ObjClr()
*
* Description : Clear a volume object.
*
* Argument(s) : p_vol       Pointer to volume.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSVol_ObjClr (FS_VOL  *p_vol)
{
    p_vol->State          =  FS_VOL_STATE_CLOSED;
    p_vol->RefCnt         =  0u;
    p_vol->RefreshCnt     =  0u;

    p_vol->AccessMode     =  FS_VOL_ACCESS_MODE_NONE;

    Mem_Set((void       *)&p_vol->Name[0],
            (CPU_INT08U  ) ASCII_CHAR_NULL,
            (CPU_SIZE_T  )(FS_CFG_MAX_VOL_NAME_LEN + 1u));

    p_vol->PartitionNbr   =  0u;
    p_vol->PartitionStart =  0u;
    p_vol->PartitionSize  =  0u;
    p_vol->SecSize        =  0u;

    p_vol->FileCnt        =  0u;
#ifdef FS_DIR_MODULE_PRESENT
    p_vol->DirCnt         =  0u;
#endif
    p_vol->DevPtr         = (FS_DEV *)0;
    p_vol->DataPtr        = (void   *)0;
#ifdef FS_CACHE_MODULE_PRESENT
    p_vol->CacheDataPtr   = (void             *)0;
    p_vol->CacheAPI_Ptr   = (FS_VOL_CACHE_API *)0;
#endif

#if (FS_CFG_CTR_STAT_EN   == DEF_ENABLED)
    p_vol->StatRdSecCtr   =  0u;
    p_vol->StatWrSecCtr   =  0u;
#endif
}


/*
*********************************************************************************************************
*                                           FSVol_ObjFind()
*
* Description : Find a volume object by name.
*
* Argument(s) : name_vol    Volume name.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Volume object found.
*
* Return(s)   : Pointer to a volume, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_VOL  *FSVol_ObjFind (CPU_CHAR  *name_vol,
                                FS_ERR    *p_err)
{
    FS_QTY       ix;
    CPU_INT16S   cmp;
    FS_VOL      *p_vol;


    (void)p_err;
    p_vol      = DEF_NULL;

    for (ix = 0u; ix < FSVol_VolCntMax; ix++) {
        if (FSVol_Tbl[ix] != DEF_NULL) {
            cmp = Str_Cmp_N(name_vol, FSVol_Tbl[ix]->Name, FS_CFG_MAX_VOL_NAME_LEN);
            if (cmp == 0) {
                p_vol = FSVol_Tbl[ix];
                break;
            }
        }
    }

    return (p_vol);
}


/*
*********************************************************************************************************
*                                          FSVol_ObjFindEx()
*
* Description : Find a volume object by name OR device/partition.
*
* Argument(s) : name_vol    Volume name.
*
* Return(s)   : Pointer to a volume, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_VOL  *FSVol_ObjFindEx (CPU_CHAR          *name_vol,
                                  FS_DEV            *p_dev,
                                  FS_PARTITION_NBR   partition_nbr)
{
    FS_QTY       vol_ix;
    CPU_INT16S   cmp;
    FS_VOL      *p_vol;


    p_vol      = DEF_NULL;

    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] != DEF_NULL) {
            cmp = Str_Cmp_N(name_vol, FSVol_Tbl[vol_ix]->Name, FS_CFG_MAX_VOL_NAME_LEN);
            if ((cmp == 0)) {                                   /* Cmp names.                                           */
                p_vol = FSVol_Tbl[vol_ix];
                return (p_vol);
            } else {
                if (FSVol_Tbl[vol_ix]->DevPtr == p_dev) {       /* Cmp dev/partition.                                   */
                    if (FSVol_Tbl[vol_ix]->PartitionNbr == partition_nbr) {
                        p_vol = FSVol_Tbl[vol_ix];
                        return (p_vol);
                    }
                }
            }
        }
    }

    return (p_vol);
}


/*
*********************************************************************************************************
*                                           FSVol_ObjFree()
*
* Description : Free a volume object.
*
* Argument(s) : p_vol       Pointer to a volume.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSVol_ObjFree (FS_VOL  *p_vol)
{
    LIB_ERR  pool_err;
    FS_QTY   vol_ix;


    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] == p_vol) {
            FSVol_Tbl[vol_ix] = DEF_NULL;
            break;
        }
    }

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    FSVol_ObjClr(p_vol);
#endif

    Mem_PoolBlkFree(        &FSVol_Pool,
                    (void *) p_vol,
                            &pool_err);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_TRACE_INFO(("FSVol_ObjFree(): Could not free vol to pool.\r\n"));
        return;
    }
#endif

    FSVol_Cnt--;
}


/*
*********************************************************************************************************
*                                           FSVol_ObjGet()
*
* Description : Allocate a volume object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a volume, if NO errors.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_VOL  *FSVol_ObjGet (void)
{
    FS_VOL   *p_vol;
    LIB_ERR   pool_err;
    FS_QTY    vol_ix;


    for (vol_ix = 0u; vol_ix < FSVol_VolCntMax; vol_ix++) {
        if (FSVol_Tbl[vol_ix] == DEF_NULL) {
            break;
        }
    }
    if (vol_ix >= FSVol_VolCntMax) {
        FS_TRACE_INFO(("FSVol_ObjGet(): Could not alloc vol from pool. Opened volume limit reached.\r\n"));
        return (DEF_NULL);
    }


    p_vol = (FS_VOL *)Mem_PoolBlkGet(&FSVol_Pool,
                                      sizeof(FS_VOL),
                                     &pool_err);
   (void)pool_err;                                             /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == DEF_NULL) {
        FS_TRACE_INFO(("FSVol_ObjGet(): Could not alloc vol from pool. Opened volume limit reached.\r\n"));
        return (DEF_NULL);
    }

    FSVol_Tbl[vol_ix] = p_vol;

    FSVol_Cnt++;

    FSVol_ObjClr(p_vol);


    return (p_vol);
}
