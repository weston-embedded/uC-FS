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
*                                  FILE SYSTEM DIRECTORY MANAGEMENT
*
* Filename : fs_dir.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DIR_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>
#include  "fs.h"
#include  "fs_sys.h"
#include  "fs_dir.h"
#include  "fs_vol.h"
#include  "fs_dev.h"
#include  "../FAT/fs_fat.h"
#include  "../FAT/fs_fat_type.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_dir.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_DIR_MODULE_PRESENT                                  /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_DIR_MAX_FULL_PATH_LEN               FS_CFG_MAX_PATH_NAME_LEN + FS_CFG_MAX_VOL_NAME_LEN

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

FS_STATIC  MEM_POOL     FSDir_Pool;                                     /* Pool of dirs.                                */
FS_STATIC  FS_DIR     **FSDir_Tbl;                                      /* Tbl  of dirs.                                */

FS_STATIC  CPU_BOOLEAN  FSDir_ModuleEn;                                 /* Module enabled.                              */

FS_STATIC  FS_QTY       FSDir_DirCntMax;                                /* Maximum number of open directories.          */
FS_STATIC  FS_QTY       FSDir_Cnt;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                /* ---------- ACCESS CONTROL ---------- */
static  FS_DIR       *FSDir_AcquireLockChk(FS_DIR        *p_dir,                /* Acquire directory reference & lock.  */
                                           CPU_BOOLEAN    chngd,
                                           FS_ERR        *p_err);

static  FS_DIR       *FSDir_Acquire       (FS_DIR        *p_dir);               /* Acquire directory reference.         */

static  void          FSDir_ReleaseUnlock (FS_DIR        *p_dir);               /* Release directory reference & lock.  */

static  void          FSDir_Release       (FS_DIR        *p_dir);               /* Release directory reference.         */

static  CPU_BOOLEAN   FSDir_Lock          (FS_DIR        *p_dir);               /* Acquire directory lock.              */

static  void          FSDir_Unlock        (FS_DIR        *p_dir);               /* Release directory lock.              */


                                                                                /* ---- DIRECTORY OBJECT MANAGEMENT --- */
static  void          FSDir_ObjClr        (FS_DIR        *p_dir);               /* Clear    a directory object.         */

static  void          FSDir_ObjFree       (FS_DIR        *p_dir);               /* Free     a directory object.         */

static  FS_DIR       *FSDir_ObjGet        (void);                               /* Allocate a directory object.         */


                                                                                /* ----------- NAME PARSING ----------- */
static  CPU_CHAR     *FSDir_NameParseChk  (CPU_CHAR       *name_full,           /* Parse full dir name & get vol ptr.   */
                                           FS_VOL        **pp_vol,
                                           FS_ERR         *p_err);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            FSDir_Close()
*
* Description : Close & free a directory.
*
* Argument(s) : p_dir       Pointer to a directory.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE            Directory closed.
*                               FS_ERR_NULL_PTR        Argument 'p_dir' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE    Argument 'p_dir's TYPE is invalid or unknown.
*                               FS_ERR_DIR_DIS         Directory module disabled.
*                               FS_ERR_DIR_NOT_OPEN    Directory NOT open.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDir_ModuleInit()  Note #1'.
*
*               (2) After a directory is closed, the application MUST desist from accessing its directory
*                   pointer.  This could cause file system corruption, since this handle may be re-used
*                   for a different directory.
*********************************************************************************************************
*/

void  FSDir_Close (FS_DIR  *p_dir,
                   FS_ERR  *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dir == (FS_DIR *)0) {                                 /* Validate dir ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    if (FSDir_ModuleEn == DEF_DISABLED) {                       /* Dir module run-time dis'd (see Note #1).             */
       *p_err = FS_ERR_DIR_DIS;
        return;
    }



                                                                /* ----------------- ACQUIRE DIR LOCK ----------------- */
    (void)FSDir_AcquireLockChk(p_dir, DEF_YES, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* --------------------- CLOSE DIR -------------------- */
    FSSys_DirClose(p_dir);

    FSVol_DirRemove(p_dir->VolPtr, p_dir);                      /* Unlink dir from vol dir list.                        */
    p_dir->State = FS_DIR_STATE_CLOSING;



                                                                /* ----------------- RELEASE DIR LOCK ----------------- */
    FSDir_ReleaseUnlock(p_dir);
    FSDir_Release(p_dir);                                       /* Release dir ref init.                                */
}



/*
*********************************************************************************************************
*                                           FSDir_IsOpen()
*
* Description : Test if dir is already open.
*
* Argument(s) : name_full   Name of the directory.
*
*               p_err       Pointer to variable that will receive the return error code:
*
*                               FS_ERR_NONE                     Dir opened successfully.
*                               FS_ERR_NAME_NULL                Argument 'name_full' passed a NULL ptr.
*
*                                                      ---- RETURNED BY FS_FAT_LowFileFirstClusGet() ----
*                               FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                               FS_ERR_ENTRY_NOT_FILE           Entry not a file nor a directory.
*                               FS_ERR_VOL_INVALID_SEC_NBR      Cluster address invalid.
*                               FS_ERR_ENTRY_NOT_FOUND          File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND   Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR     Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID             Invalid name or path.
*                               FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*                                                                       entry.
*
* Return(s)   : DEF_NO,  if dir is NOT open.
*               DEF_YES, if dir is     open.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDir_IsOpen(CPU_CHAR  *name_full,
                          FS_ERR    *p_err)
{
    FS_DIR             *p_dir;
    FS_QTY              max_ix;
    CPU_INT32U          ix;
    FS_FAT_FILE_DATA   *p_fat_file_data;
    FS_VOL             *p_open_vol;
    FS_VOL             *p_disk_vol;
    FS_FAT_CLUS_NBR     open_dir_first_clus_nb;
    FS_FAT_CLUS_NBR     disk_dir_first_clus_nb;
    CPU_CHAR           *dir_name;
    CPU_BOOLEAN         dir_open;



#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_YES);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_YES);
    }
#endif


                                                                /* -------------- GET INFO OF DIR ON DEV -------------- */
    dir_name =  FSDir_NameParseChk( name_full,                  /* Acquire vol ptr.                                     */
                                   &p_disk_vol,
                                    p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_NONE;
        return (DEF_NO);
    }
                                                                /* Get 1st clut addr of dir.                            */
    disk_dir_first_clus_nb = FS_FAT_LowFileFirstClusGet (p_disk_vol,
                                                         dir_name,
                                                         p_err);
    FSVol_Unlock(p_disk_vol);
    FSVol_Release(p_disk_vol);

    switch (*p_err) {
        case FS_ERR_NONE:
             break;


        case FS_ERR_ENTRY_NOT_FOUND:                            /* Entry do not exist. Cannot be open.                  */
        case FS_ERR_ENTRY_PARENT_NOT_FOUND:
        case FS_ERR_ENTRY_PARENT_NOT_DIR:
            *p_err = FS_ERR_NONE;
             return (DEF_NO);


        case FS_ERR_NAME_INVALID:
        case FS_ERR_VOL_INVALID_SEC_NBR:
        case FS_ERR_BUF_NONE_AVAIL:
        case FS_ERR_ENTRY_NOT_FILE:
        default:
             return (DEF_NO);
    }


                                                                /* ------------- CMP TO EVERY DIR IN POOL ------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }

    max_ix = FSDir_Cnt;

    dir_open = DEF_NO;
    ix       = 0u;
    while ((ix < max_ix) &&
           (dir_open == DEF_NO)) {
        p_dir = FSDir_Tbl[ix];

        if (p_dir != DEF_NULL) {
            p_open_vol = p_dir->VolPtr;
                                                                /* Cmp vol ptr.                                         */
            if (p_open_vol == p_disk_vol) {
                p_fat_file_data        = (FS_FAT_FILE_DATA *)p_dir->DataPtr;
                open_dir_first_clus_nb =  p_fat_file_data->FileFirstClus;
                                                                /* Cmp 1st cluster address                              */
                if (open_dir_first_clus_nb == disk_dir_first_clus_nb) {
                    dir_open = DEF_YES;
                }
            }
        }

        ix++;
    }

    FS_OS_Unlock();


   *p_err   = FS_ERR_NONE;

    return (dir_open);
}


/*
*********************************************************************************************************
*                                            FSDir_Open()
*
* Description : Open a directory.
*
* Argument(s) : name_full   Name of the directory.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Directory opened successfully.
*                               FS_ERR_NAME_NULL                 Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_DIR_DIS                   Directory module disabled.
*                               FS_ERR_DIR_NONE_AVAIL            No directory available.
*
*                                                                --- RETURNED BY FSDir_NameParseChk() ---
*                               FS_ERR_NAME_INVALID              Entry name specified invalid or volume
*                                                                    could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name is too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume not opened.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume not mounted.
*
*                                                                ------ RETURNED BY FSSys_DirOpen() -----
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                               FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : Pointer to a directory, if NO errors.
*               Pointer to NULL,        otherwise.
*
* Note(s)     : (1) See 'FSDir_ModuleInit()  Note #1'.
*********************************************************************************************************
*/

FS_DIR  *FSDir_Open  (CPU_CHAR  *name_full,
                      FS_ERR    *p_err)
{
    CPU_CHAR  *name_dir;
    CPU_CHAR  *name_full_temp;
    FS_DIR    *p_dir;
    FS_VOL    *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION((FS_DIR *)0);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return ((FS_DIR *)0);
    }
#endif

    if (FSDir_ModuleEn == DEF_DISABLED) {                       /* Dir module run-time dis'd (see Note #1).             */
       *p_err = FS_ERR_DIR_DIS;
        return ((FS_DIR *)0);
    }


                                                                /* ------------------- GET FREE DIR ------------------- */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock.                                     */
    if (*p_err != FS_ERR_NONE) {
        return ((FS_DIR *)0);
    }

    p_dir = FSDir_ObjGet();                                     /* Alloc dir.                                           */
    if (p_dir == (FS_DIR *)0) {
       *p_err = FS_ERR_DIR_NONE_AVAIL;
        FS_OS_Unlock();
        return ((FS_DIR *)0);
    }

    p_dir->RefCnt =  1u;
    p_dir->State  =  FS_DIR_STATE_OPENING;
    p_dir->VolPtr = (FS_VOL *)0;

    FS_OS_Unlock();                                             /* Release FS lock.                                     */



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        FSDir_Release(p_dir);
        return ((FS_DIR *)0);
    }
#else
    name_full_temp = name_full;
#endif



                                                                /* ------------------ ACQUIRE VOL REF ----------------- */
    name_dir = FSDir_NameParseChk( name_full_temp,
                                  &p_vol,
                                   p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {                                 /* If no vol found ...                                  */
        FSDir_Release(p_dir);                                   /*                 ... release dir ref.                 */
        return ((FS_DIR *)0);
    }



                                                                /* --------------------- OPEN DIR --------------------- */
    p_dir->VolPtr = p_vol;

    FSSys_DirOpen(p_dir,
                  name_dir,
                  p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif

    if (*p_err != FS_ERR_NONE) {
        p_dir->State = FS_DIR_STATE_CLOSING;
        FSDir_ReleaseUnlock(p_dir);
        return ((FS_DIR *)0);
    }

    p_dir->State      = FS_DIR_STATE_OPEN;
    p_dir->RefreshCnt = p_vol->RefreshCnt;
    FSVol_DirAdd(p_vol, p_dir);                                 /* Link file to vol dir list.                           */



                                                                /* ----------------- RELEASE DIR LOCK ----------------- */
    FSDir_Unlock(p_dir);                                        /* Keep init ref.                                       */
    return (p_dir);
}


/*
*********************************************************************************************************
*                                             FSDir_Rd()
*
* Description : Read a directory entry from a directory.
*
* Argument(s) : p_dir           Pointer to a directory.
*
*               p_dir_entry     Pointer to variable that will receive directory entry information.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE              Directory read successfully.
*                                   FS_ERR_NULL_PTR          Argument 'p_dir'/'p_dir_entry'passed a NULL pointer.
*                                   FS_ERR_INVALID_TYPE      Argument 'p_dir's TYPE is invalid or unknown.
*                                   FS_ERR_DIR_DIS           Directory module disabled.
*
*                                                            ---- RETURNED BY FSDir_AcquireLockChk() ----
*                                   FS_ERR_DEV_CHNGD         Device has changed.
*                                   FS_ERR_DIR_NOT_OPEN      Directory NOT open.
*
*                                                            --------- RETURNED BY FSSys_DirRd() --------
*                                   FS_ERR_EOF               End of directory reached.
*                                   FS_ERR_DEV               Device access error.
*                                   FS_ERR_BUF_NONE_AVAIL    Buffer not available.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDir_ModuleInit()  Note #1'.
*
*               (2) Entries for "dot" (current directory) and "dot-dot" (parent directory) shall be
*                   returned, if present.  No entry with an empty name shall be returned.
*
*               (3) The directory entry information is stored in a structure that may be overwritten by
*                   another call to 'FSDir_Rd()' on the same directory.
*
*               (4) If an entry is removed from or added to the directory after the directory has been
*                   opened, information may or may not be returned for that entry.
*
*               (5) See 'fs_readdir()  Note(s)'.
*********************************************************************************************************
*/

void  FSDir_Rd (FS_DIR        *p_dir,
                FS_DIR_ENTRY  *p_dir_entry,
                FS_ERR        *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dir == (FS_DIR *)0) {                                 /* Validate dir ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_dir_entry == (FS_DIR_ENTRY *)0) {                     /* Validate dir entry ptr.                              */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    if (FSDir_ModuleEn == DEF_DISABLED) {                       /* Dir module run-time dis'd (see Note #1).             */
       *p_err = FS_ERR_DIR_DIS;
        return;
    }



                                                                /* ----------------- ACQUIRE DIR LOCK ----------------- */
    (void)FSDir_AcquireLockChk(p_dir, DEF_NO, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* --------------------- READ DIR --------------------- */
    FSSys_DirRd(p_dir,
                p_dir_entry,
                p_err);




                                                                /* ----------------- RELEASE DIR LOCK ----------------- */
    FSDir_ReleaseUnlock(p_dir);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                   DIRECTORY MANAGEMENT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSDir_GetDirCnt()
*
* Description : Get the number of open directories.
*
* Argument(s) : none.
*
* Return(s)   : The number of open directories.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSDir_GetDirCnt (void)
{
    FS_QTY  dir_cnt;
    FS_ERR  err;


    if (FSDir_ModuleEn == DEF_DISABLED) {                       /* Dir module run-time dis'd.                           */
        return (0u);
    }


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return (0u);
    }



                                                                /* -------------------- GET DIR CNT ------------------- */
    dir_cnt = FSDir_Cnt;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (dir_cnt);
}


/*
*********************************************************************************************************
*                                        FSDir_GetDirCntMax()
*
* Description : Get the maximum possible number of open directories.
*
* Argument(s) : none.
*
* Return(s)   : The maximum number of open directories.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSDir_GetDirCntMax (void)
{
    FS_QTY  dir_cnt_max;


    if (FSDir_ModuleEn == DEF_DISABLED) {                       /* Dir module run-time dis'd.                           */
        return (0u);
    }


                                                                /* ------------------ GET DIR CNT MAX ----------------- */
    dir_cnt_max = FSDir_DirCntMax;

    return (dir_cnt_max);
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
*                                         FSDir_ModuleInit()
*
* Description : Initialize File System Directory Management module.
*
* Argument(s) : dir_cnt     Number of directories to allocate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE         Module initialized.
*                               FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) If no directories are configured, then the directory module is run-time disabled.
*                   All interface functions will return errors.
*********************************************************************************************************
*/

void  FSDir_ModuleInit (FS_QTY   dir_cnt,
                        FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


#if (FS_CFG_TRIAL_EN == DEF_ENABLED)                            /* Trial limitation: max 1 dir.                         */
    if (dir_cnt > FS_TRIAL_MAX_DIR_CNT) {
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif

    FSDir_ModuleEn  = DEF_DISABLED;
    FSDir_DirCntMax = 0u;
    FSDir_Cnt       = 0u;

    if (dir_cnt == 0u) {                                        /* Dir module run-time dis'd.                           */
       *p_err = FS_ERR_NONE;
        return;
    }

                                                                /* ------------------- INIT DEV POOL ------------------ */
    Mem_PoolCreate(&FSDir_Pool,
                    DEF_NULL,
                    0,
                    dir_cnt,
                    sizeof(FS_DIR),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSDir_ModuleInit(): Could not alloc mem for dirs: %d octets required.\r\n", octets_reqd));
        return;
    }

    FSDir_Tbl = (FS_DIR **)Mem_HeapAlloc(dir_cnt * sizeof(FS_DIR *),
                                         sizeof(CPU_ALIGN),
                                        &octets_reqd,
                                        &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSDir_ModuleInit(): Could not alloc mem for dirs: %d octets required.\r\n", octets_reqd));
        return;
    }

    Mem_Clr(FSDir_Tbl, dir_cnt * sizeof(FS_DIR *));

    FSDir_ModuleEn  = DEF_ENABLED;
    FSDir_DirCntMax = dir_cnt;

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
*                                       FSDir_AcquireLockChk()
*
* Description : Acquire directory reference & lock.
*
* Argument(s) : p_dir       Pointer to directory.
*               ----------  Argument validated by caller.
*
*               chngd       Indicates whether device may have changed (unused):
*
*                               DEF_YES    Device may have changed.
*                               DEF_NO     Device must NOT have changed.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE            Directory reference & lock acquired.
*                               FS_ERR_DEV_CHNGD       Device has changed.
*                               FS_ERR_DIR_NOT_OPEN    Directory is not open.
*
* Return(s)   : Pointer to a directory, if found.
*               Pointer to NULL,        otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DIR  *FSDir_AcquireLockChk (FS_DIR       *p_dir,
                                       CPU_BOOLEAN   chngd,
                                       FS_ERR       *p_err)
{
    CPU_BOOLEAN  lock_success;


    (void)chngd;
                                                                /* ----------------- ACQUIRE DIR LOCK ----------------- */
    p_dir = FSDir_Acquire(p_dir);                               /* Acquire dir ref.                                     */

    if (p_dir == (FS_DIR *)0) {                                 /* Rtn err if dir not found.                            */
       *p_err = FS_ERR_DIR_NOT_OPEN;
        return ((FS_DIR *)0);
    }

    lock_success = FSDir_Lock(p_dir);
    if (lock_success != DEF_YES) {
       *p_err = FS_ERR_OS_LOCK;
        return ((FS_DIR *)0);
    }



                                                                /* ------------------- VALIDATE DIR ------------------- */
    switch (p_dir->State) {
        case FS_DIR_STATE_OPEN:
        case FS_DIR_STATE_EOF:
        case FS_DIR_STATE_ERR:
            *p_err = FS_ERR_NONE;
             return (p_dir);


        case FS_DIR_STATE_CLOSED:                               /* Rtn NULL if dir not open.                            */
        case FS_DIR_STATE_CLOSING:
        case FS_DIR_STATE_OPENING:
        default:
            *p_err = FS_ERR_DIR_NOT_OPEN;
             FSDir_ReleaseUnlock(p_dir);
             return ((FS_DIR *)0);
    }
}


/*
*********************************************************************************************************
*                                           FSDir_Acquire()
*
* Description : Acquire a reference to a directory.
*
* Argument(s) : p_dir       Pointer to directory.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to a directory, if available.
*               Pointer to NULL,        otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DIR  *FSDir_Acquire (FS_DIR  *p_dir)
{
    FS_ERR  err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return ((FS_DIR *)0);
    }



                                                                /* -------------------- INC REF CNT ------------------- */
    if (p_dir->RefCnt == 0u) {                                  /* Rtn NULL if dir not ref'd.                           */
        FS_OS_Unlock();
        return ((FS_DIR *)0);
    }

    p_dir->RefCnt++;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (p_dir);
}


/*
*********************************************************************************************************
*                                        FSDir_ReleaseUnlock()
*
* Description : Release directory reference & lock.
*
* Argument(s) : p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDir_ReleaseUnlock (FS_DIR  *p_dir)
{
    FSDir_Unlock(p_dir);
    FSDir_Release(p_dir);
}


/*
*********************************************************************************************************
*                                           FSDir_Release()
*
* Description : Release a reference to a directory.
*
* Argument(s) : p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) A volume reference might have been acquired in 'FSDir_NameParseChk()'; the volume
*                   reference would NOT have been acquired if the named volume did not exist or existed
*                   but was not mounted.
*********************************************************************************************************
*/

static  void  FSDir_Release (FS_DIR  *p_dir)
{
    FS_ERR   err;
    FS_CTR   ref_cnt;
    FS_VOL  *p_vol;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return;
    }



                                                                /* -------------------- DEC REF CNT ------------------- */
    ref_cnt = p_dir->RefCnt;
    if (ref_cnt == 1u) {
        p_vol = p_dir->VolPtr;
        FSDir_ObjFree(p_dir);
    } else if (ref_cnt > 0u) {
        p_dir->RefCnt = ref_cnt - 1u;
    } else {
        FS_TRACE_DBG(("FSDir_Release(): Release cnt dec'd to zero.\r\n"));
    }

    FS_OS_Unlock();



                                                                /* -------------------- RELEASE VOL ------------------- */
    if (ref_cnt == 1u) {
        if (p_vol != (FS_VOL *)0) {
            FSVol_Release(p_vol);                               /* Release vol (see Note #1).                           */
        }
    }
}


/*
*********************************************************************************************************
*                                            FSDir_Lock()
*
* Description : Acquire directory lock.
*
* Argument(s) : p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDir_Lock (FS_DIR  *p_dir)
{
    CPU_BOOLEAN  locked;


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    locked = FSVol_Lock(p_dir->VolPtr);

    return (locked);
}


/*
*********************************************************************************************************
*                                           FSDir_Unlock()
*
* Description : Release directory lock.
*
* Argument(s) : p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDir_Unlock (FS_DIR  *p_dir)
{
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSVol_Unlock(p_dir->VolPtr);
}


/*
*********************************************************************************************************
*                                           FSDir_ObjClr()
*
* Description : Clear a directory object.
*
* Argument(s) : p_dir       Pointer to directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSDir_ObjClr (FS_DIR  *p_dir)
{
    p_dir->State          =  FS_DIR_STATE_CLOSED;
    p_dir->RefCnt         =  0u;
    p_dir->RefreshCnt     =  0u;

    p_dir->Offset         =  0u;

    p_dir->VolPtr         = (FS_VOL *)0;
    p_dir->DataPtr        = (void   *)0;

#if (FS_CFG_CTR_STAT_EN   == DEF_ENABLED)
    p_dir->StatRdEntryCtr =  0u;
#endif
}


/*
*********************************************************************************************************
*                                           FSDir_ObjFree()
*
* Description : Free a directory object.
*
* Argument(s) : p_dir       Pointer to a directory.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSDir_ObjFree (FS_DIR  *p_dir)
{
    LIB_ERR  pool_err;
    FS_QTY   dir_ix;


    for (dir_ix = 0u; dir_ix < FSDir_DirCntMax; dir_ix++) {
        if (FSDir_Tbl[dir_ix] == p_dir) {
            FSDir_Tbl[dir_ix] = DEF_NULL;
            break;
        }
    }

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    FSDir_ObjClr(p_dir);
#endif

    Mem_PoolBlkFree(        &FSDir_Pool,
                    (void *) p_dir,
                            &pool_err);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_TRACE_INFO(("FSDir_ObjFree(): Could not free dir to pool.\r\n"));
        return;
    }
#endif

    FSDir_Cnt--;
}


/*
*********************************************************************************************************
*                                           FSDir_ObjGet()
*
* Description : Allocate a directory object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a directory, if NO errors.
*               Pointer to NULL,        otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_DIR  *FSDir_ObjGet (void)
{
    FS_DIR   *p_dir;
    LIB_ERR   pool_err;
    FS_QTY    dir_ix;

    for (dir_ix = 0u; dir_ix < FSDir_DirCntMax; dir_ix++) {
        if (FSDir_Tbl[dir_ix] == DEF_NULL) {
            break;
        }
    }
    if (dir_ix >= FSDir_DirCntMax) {
        FS_TRACE_INFO(("FSDir_ObjGet(): Could not alloc dir from pool. Opened dir limit reached.\r\n"));
        return ((FS_DIR *)0);
    }

    p_dir = (FS_DIR *)Mem_PoolBlkGet(&FSDir_Pool,
                                      sizeof(FS_DIR),
                                     &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    if (p_dir == (FS_DIR *)0) {
        FS_TRACE_INFO(("FSDir_ObjGet(): Could not alloc dir from pool. Opened directory limit reached.\r\n"));
        return ((FS_DIR *)0);
    }

    FSDir_Tbl[dir_ix] = p_dir;

    FSDir_Cnt++;

    FSDir_ObjClr(p_dir);

    return (p_dir);
}


/*
*********************************************************************************************************
*                                        FSDir_NameParseChk()
*
* Description : Parse full directory name & get volume pointer & pointer to entry name.
*
* Argument(s) : name_full   Name of the entry.
*               ----------  Argument validated by caller.
*
*               pp_vol      Pointer to volume pointer which will hold pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NAME_INVALID          Entry name could not be parsed, lacks an
*                                                                initial path separator character or
*                                                                includes an invalid volume name.
*                               FS_ERR_NAME_PATH_TOO_LONG    Entry name is too long.
*                               FS_ERR_VOL_NOT_OPEN          Volume not opened.
*                               FS_ERR_VOL_NOT_MOUNTED       Volume not mounted.
*                               FS_ERR_OS_LOCK               OS Lock NOT acquired.
*
* Return(s)   : Pointer to start of entry name.
*
* Note(s)     : (1) If the volume name could not be parsed, the volume does not exist or no directory
*                   name is specified, then NULL pointers are returned for BOTH the directory name &
*                   the volume.  Otherwise, both the directory name & volume should be valid pointers.
*
*               (2) The volume reference is released in 'FSDir_Release()' after the final reference to
*                   the directory has been released.
*********************************************************************************************************
*/

static  CPU_CHAR  *FSDir_NameParseChk (CPU_CHAR   *name_full,
                                       FS_VOL    **pp_vol,
                                       FS_ERR     *p_err)
{
    CPU_SIZE_T   name_entry_len;
    CPU_CHAR    *name_entry;
    FS_VOL      *p_vol_temp;
    CPU_CHAR     name_vol_temp[FS_CFG_MAX_VOL_NAME_LEN + 1u];
    CPU_BOOLEAN  vol_lock_ok;


   *pp_vol     = (FS_VOL *)0;
    name_entry =  FS_PathParse(name_full,
                               name_vol_temp,
                               p_err);

    if (*p_err != FS_ERR_NONE) {
        return ((CPU_CHAR *)0);
    }

    if (name_entry == (CPU_CHAR *)0) {                          /* If name could not be parsed ...                      */
       *p_err =  FS_ERR_NAME_INVALID;
        return ((CPU_CHAR *)0);                                 /* ... rtn NULL vol & file name.                        */
    }

    if (*name_entry != FS_CHAR_PATH_SEP) {                      /* Require init path sep char.                          */
       *p_err = FS_ERR_NAME_INVALID;
        return ((CPU_CHAR *)0);
    }
    name_entry++;                                               /* Ignore init path sep char                            */

    name_entry_len = Str_Len_N(name_entry, FS_CFG_MAX_PATH_NAME_LEN + 1u);
    if (name_entry_len > FS_CFG_MAX_PATH_NAME_LEN) {            /* Rtn err if file name is too long.                    */
       *p_err =  FS_ERR_NAME_PATH_TOO_LONG;
        return ((CPU_CHAR *)0);
    }
                                                                /* Acquire ref to vol (see Note #3).                    */
    p_vol_temp = (name_vol_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) ? FSVol_AcquireDflt()
                                                                 : FSVol_Acquire(name_vol_temp, p_err);

    if (p_vol_temp == (FS_VOL *)0) {
       *p_err =  FS_ERR_VOL_NOT_OPEN;
        return ((CPU_CHAR *)0);
    }

    vol_lock_ok = FSVol_Lock(p_vol_temp);                       /* Lock vol.                                            */
    if (vol_lock_ok == DEF_NO) {
       *p_err = FS_ERR_OS_LOCK;
        FSVol_Release(p_vol_temp);
        return ((CPU_CHAR *)0);
    }

    (void)FSVol_RefreshLocked(p_vol_temp, p_err);               /* Refresh vol.                                         */

    switch (p_vol_temp->State) {
        case FS_VOL_STATE_OPEN:                                 /* Rtn err if vol is not mounted.                       */
        case FS_VOL_STATE_PRESENT:
            *p_err = FS_ERR_VOL_NOT_MOUNTED;
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);


        case FS_VOL_STATE_MOUNTED:
             break;


        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:                              /* Rtn err if vol is not open.                          */
        default:
            *p_err = FS_ERR_VOL_JOURNAL_NOT_OPEN;
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);
    }

   *pp_vol = p_vol_temp;
    return (name_entry);
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_dir.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of dir module include (see Note #1).             */
