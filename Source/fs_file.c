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
*                                  FILE SYSTEM SUITE FILE MANAGEMENT
*
* Filename : fs_file.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FILE_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>
#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_buf.h"
#include  "fs_dev.h"
#include  "fs_file.h"
#include  "fs_sys.h"
#include  "fs_vol.h"
#include  "../FAT/fs_fat.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define FS_FILE_ALL_WR_ACCESS_MODE          (FS_FILE_ACCESS_MODE_WR       | FS_FILE_ACCESS_MODE_CREATE | \
                                             FS_FILE_ACCESS_MODE_EXCL     | FS_FILE_ACCESS_MODE_APPEND | \
                                             FS_FILE_ACCESS_MODE_TRUNCATE)

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

FS_STATIC  MEM_POOL    FSFile_Pool;                             /* Pool of files.                                       */
FS_STATIC  FS_FILE   **FSFile_Tbl;                              /* Tbl of files.                                        */

FS_STATIC  FS_QTY      FSFile_FileCntMax;                       /* Maximum number of open files.                        */
FS_STATIC  FS_QTY      FSFile_Cnt;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                            /* ------------- BUFFER ACCESS ------------ */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
static  void         FSFile_BufEmpty          (FS_FILE       *p_file,       /* Empty a file's buffer.                   */
                                               FS_ERR        *p_err);

static  CPU_SIZE_T   FSFile_BufRd             (FS_FILE       *p_file,       /* Read from a file (through file buffer).  */
                                               void          *p_dest,
                                               CPU_SIZE_T     size,
                                               FS_ERR        *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_SIZE_T   FSFile_BufWr             (FS_FILE       *p_file,       /* Write to a file (through file buffer).   */
                                               void          *p_src,
                                               CPU_SIZE_T     size,
                                               FS_ERR        *p_err);
#endif
#endif


                                                                            /* ------------ ACCESS CONTROL ------------ */
static  FS_FILE      *FSFile_AcquireLockChk   (FS_FILE       *p_file,       /* Acquire file reference & lock.           */
                                               FS_ERR        *p_err);

static  FS_FILE      *FSFile_Acquire          (FS_FILE       *p_file);      /* Acquire file reference.                  */

static  void          FSFile_ReleaseUnlock    (FS_FILE       *p_file);      /* Release file reference & lock.           */

static  void          FSFile_Release          (FS_FILE       *p_file);      /* Release file reference.                  */

static  CPU_BOOLEAN   FSFile_Lock             (FS_FILE       *p_file);      /* Acquire file lock.                       */

static  void          FSFile_Unlock           (FS_FILE       *p_file);      /* Release file lock.                       */


                                                                            /* ----------- FILE LOCK CONTROL ---------- */
#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
static  CPU_BOOLEAN   FSFile_LockAcceptHandler(FS_FILE       *p_file);      /* Accept  file lock.                       */

static  CPU_BOOLEAN   FSFile_LockGetHandler   (FS_FILE       *p_file);      /* Acquire file lock.                       */

static  CPU_BOOLEAN   FSFile_LockSetHandler   (FS_FILE       *p_file);      /* Release file lock.                       */
#endif


                                                                            /* -------- FILE OBJECT MANAGEMENT -------- */
static  void          FSFile_ObjClr           (FS_FILE       *p_file);      /* Clear    a file object.                  */

static  void          FSFile_ObjFree          (FS_FILE       *p_file);      /* Free     a file object.                  */

static  FS_FILE      *FSFile_ObjGet           (FS_ERR        *p_err);       /* Allocate a file object.                  */


                                                                            /* ------------- NAME PARSING ------------- */
static  CPU_CHAR     *FSFile_NameParseChk     (CPU_CHAR      *name_full,    /* Parse full file name & get volume ptr.   */
                                               FS_VOL       **pp_vol,
                                               FS_ERR        *p_err);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FSFile_BufAssign()
*
* Description : Assign buffer to a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_buf       Pointer to buffer.
*
*               mode        Buffer mode :
*
*                               FS_FILE_BUF_MODE_RD       Data buffered for reads.
*                               FS_FILE_BUF_MODE_WR       Data buffered for writes.
*                               FS_FILE_BUF_MODE_RD_WR    Data buffered for reads & writes.
*
*               size        Size of buffer, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                         File buffer assigned.
*                               FS_ERR_NULL_PTR                     Argument 'p_file' or 'p_buf' passed a
*                                                                       NULL pointer.
*                               FS_ERR_INVALID_TYPE                 Argument 'p_file's TYPE is invalid or
*                                                                       unknown.
*                               FS_ERR_FILE_INVALID_BUF_MODE        Invalid buffer mode.
*                               FS_ERR_FILE_INVALID_BUF_SIZE        Invalid buffer size.
*                               FS_ERR_FILE_BUF_ALREADY_ASSIGNED    Buffer already assigned.
*
*                                                                   --- RETURNED BY FSFile_AcquireLockChk() ---
*                               FS_ERR_DEV_CHNGD                    Device has changed.
*                               FS_ERR_FILE_NOT_OPEN                File NOT open.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'FSFile_BufAssign()' MUST be used after 'p_file' is opened but before any other
*                   operation is performed on 'p_file', except 'FSFile_Query()'.
*
*               (2) 'size' MUST be more than or equal to the size of one sector; it will be rounded DOWN
*                   to the nearest size of a multiple of full sectors.
*
*               (3) Once a buffer is assigned to a file, a new buffer may not be assigned nor may the
*                   assigned buffer be removed.  To change the buffer, the file should be closed &
*                   re-opened.
*
*               (4) Upon power loss, any data stored in file buffers will be lost.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
void  FSFile_BufAssign (FS_FILE     *p_file,
                        void        *p_buf,
                        FS_FLAGS     mode,
                        CPU_SIZE_T   size,
                        FS_ERR      *p_err)
{
    FS_FLAGS    access_mode;
    FS_STATE    buf_status;
    CPU_SIZE_T  sec_size;
    CPU_SIZE_T  size_rem;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_buf == (void *)0) {                                   /* Validate buf ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (mode == FS_FILE_BUF_MODE_NONE) {                        /* Validate 'none' cache mode.                          */
       *p_err = FS_ERR_NONE;
        return;
    }
    if ((mode != FS_FILE_BUF_MODE_RD) &&                        /* Validate cache mode.                                 */
        (mode != FS_FILE_BUF_MODE_WR) &&
        (mode != FS_FILE_BUF_MODE_RD_WR)) {
       *p_err = FS_ERR_FILE_INVALID_BUF_MODE;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }



                                                                /* ------------------- VALIDATE BUF ------------------- */
    sec_size = (CPU_SIZE_T)p_file->BufSecSize;
    if (size < sec_size) {                                      /* Rtn err if size is less than sec size.               */
       *p_err = FS_ERR_FILE_INVALID_BUF_SIZE;
        FSFile_ReleaseUnlock(p_file);
        return;
    }

    access_mode = p_file->AccessMode;                           /* If buf mode does not match access mode ... rtn err.  */
    if (((mode == FS_FILE_BUF_MODE_RD) && (DEF_BIT_IS_SET(access_mode, FS_FILE_ACCESS_MODE_RD) == DEF_NO)) ||
        ((mode == FS_FILE_BUF_MODE_WR) && (DEF_BIT_IS_SET(access_mode, FS_FILE_ACCESS_MODE_WR) == DEF_NO))) {
       *p_err = FS_ERR_FILE_INVALID_BUF_MODE;
        FSFile_ReleaseUnlock(p_file);
        return;
    }

    buf_status = p_file->BufStatus;                             /* If buf has already been assigned (see Note #3).      */
    if (buf_status != FS_FILE_BUF_STATUS_NONE) {
       *p_err = FS_ERR_FILE_BUF_ALREADY_ASSIGNED;
        FSFile_ReleaseUnlock(p_file);
        return;
    }



                                                                /* -------------------- ASSIGN BUF -------------------- */
                                                                /* Rnd size DOWN to nearest sec size mult (see Note #2).*/
    size_rem           = size % sec_size;
    size              -= size_rem;
    p_file->BufMode    = mode;
    p_file->BufStatus  = FS_FILE_BUF_STATUS_EMPTY;
    p_file->BufStart   = 0u;
    p_file->BufMaxPos  = 0u;
    p_file->BufSize    = size;
    p_file->BufPtr     = p_buf;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                            FSFile_BufFlush()
*
* Description : Flush buffer contents to file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                File buffer flushed successfully.
*                               FS_ERR_NULL_PTR            Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE        Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_ERR            File has error (see Note #3).
*
*                                                          ----- RETURNED BY FSFile_AcquireLockChk() ----
*                               FS_ERR_DEV_CHNGD           Device has changed.
*                               FS_ERR_FILE_NOT_OPEN       File NOT open.
*
*                                                          -------- RETURNED BY FSFile_BufEmpty() -------
*                               FS_ERR_BUF_NONE_AVAIL      No buffer available.
*                               FS_ERR_DEV                 Device access error.
*                               FS_ERR_DEV_FULL            Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT       File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) If the most recent operation is output (write), all unwritten data is written
*                       to the file.
*
*                   (b) If the most recent operation is input  (read),  all buffered  data is cleared.
*
*                   (c) If a read or write error occurs, the error indicator is set.
*
*               (2) See 'fs_flush()  Note(s)'.
*
*               (3) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
void  FSFile_BufFlush (FS_FILE  *p_file,
                       FS_ERR   *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #3).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return;
    }



                                                                /* --------------------- EMPTY BUF -------------------- */
    if ((p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_RD) ||
        (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_WR)) {
        FSFile_BufEmpty(p_file, p_err);                         /* Empty buf (see Notes #1a, 1b).                       */

        if (*p_err != FS_ERR_NONE) {                            /* Rtn if rd/wr err (see Note #1c).                     */
            p_file->FlagErr = DEF_YES;
            FSFile_ReleaseUnlock(p_file);
            return;
        }
    }

    p_file->IO_State = FS_FILE_IO_STATE_NONE;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
}
#endif


/*
*********************************************************************************************************
*                                           FSFile_Close()
*
* Description : Close & free a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File closed.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
*                                                       --------- RETURNED BY FSFile_BufEmpty() ---------
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) After a file is closed, the application must desist from accessing its file pointer.
*                   This could cause file system corruption, since this handle may be re-used for a
*                   different file..
*
*               (2) (a) If the most recent operation is output (write), all unwritten data is written
*                       to the file.
*
*                   (b) Any buffer assigned with 'FSFile_BufAssign()' shall no longer be accessed by the
*                       file system & may be re-used by the application.
*********************************************************************************************************
*/

void  FSFile_Close (FS_FILE  *p_file,
                    FS_ERR   *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }



                                                                /* -------------------- CLOSE FILE -------------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)                         /* Flush file buf (see Note #2a).                       */
    FSFile_BufEmpty(p_file, p_err);
#endif

    FSSys_FileClose(p_file, p_err);

    FSVol_FileRemove(p_file->VolPtr, p_file);                   /* Unlink file from vol file list.                      */
    p_file->State = FS_FILE_STATE_CLOSING;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
    FSFile_Release(p_file);                                     /* Release file ref init.                               */
}



/*
*********************************************************************************************************
*                                           FSFile_ClrErr()
*
* Description : Clear EOF & error indicators on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             Error & end-of-file indicators cleared.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : none.
*
* Note(s)     : (1) If an error occurred during the last access, either a read or a write will be possible
*                   following the clearing of the error indicator.
*********************************************************************************************************
*/

void  FSFile_ClrErr (FS_FILE  *p_file,
                     FS_ERR   *p_err)
{
    CPU_BOOLEAN  was_err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }



                                                                /* ------------------- CLR ERR & EOF ------------------ */
    was_err         = p_file->FlagErr;
    p_file->FlagErr = DEF_NO;
    p_file->FlagEOF = DEF_NO;

    if (was_err == DEF_YES) {
        p_file->IO_State = FS_FILE_IO_STATE_NONE;               /* See Note #1.                                         */
    }



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSFile_IsEOF()
*
* Description : Test EOF indicator on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             EOF indicator obtained.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : DEF_NO,  if EOF indicator is NOT set OR if an error occurred.
*               DEF_YES, if EOF indicator is     set.
*
* Note(s)     : (1) If the EOF indicator is set (i.e., 'FSFile_IsEOF()' returns DEF_YES), 'FSFile_ClrErr()'
*                   can be used to clear that indicator.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSFile_IsEOF (FS_FILE  *p_file,
                           FS_ERR   *p_err)
{
    CPU_BOOLEAN  is_eof;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NO);
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }

    is_eof = p_file->FlagEOF;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);

   *p_err = FS_ERR_NONE;
    return (is_eof);
}


/*
*********************************************************************************************************
*                                           FSFile_IsErr()
*
* Description : Test error indicator on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             Error indicator obtained.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : DEF_NO,  if error indicator is NOT set OR if an error occurred.
*               DEF_YES, if error indicator is     set.
*
* Note(s)     : (1) If the error indicator is set (i.e., 'FSFile_IsErr()' returns DEF_YES), 'FSFile_ClrErr()'
*                   can be used to clear that indicator.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSFile_IsErr (FS_FILE  *p_file,
                           FS_ERR   *p_err)
{
    CPU_BOOLEAN  is_err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NO);
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }

    is_err = p_file->FlagErr;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);

   *p_err = FS_ERR_NONE;
    return (is_err);
}


/*
*********************************************************************************************************
*                                           FSFile_IsOpen()
*
* Description : Validate if file is already open.
*
* Argument(s) : name_full   Name of the file.
*
*               p_mode      Pointer to variable that will receive the file access mode.
*                           bit-wise OR of one or more of the following:
*
*                               FS_FILE_ACCESS_MODE_RD          File opened for reads.
*                               FS_FILE_ACCESS_MODE_WR          File opened for writes.
*                               FS_FILE_ACCESS_MODE_TRUNCATE    File length is truncated to 0.
*                               FS_FILE_ACCESS_MODE_APPEND      All writes are performed at EOF.
*                               FS_FILE_ACCESS_MODE_CACHED      File data is cached.
*
*               p_err       Pointer to variable that will receive the return error code:
*
*                               FS_ERR_NONE                     File opened successfully.
*                               FS_ERR_NAME_NULL                Argument 'name_full' passed a NULL ptr.
*
*                                                      ---- RETURNED BY FS_FAT_LowFileFirstClusGet() ----
*                               FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                               FS_ERR_ENTRY_NOT_FILE           Entry not a file.
*                               FS_ERR_NAME_INVALID             Invalid file name or path.
*                               FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*                                                                   entry.
*
* Return(s)   : DEF_NO,  if file is NOT open.
*
*               DEF_YES, if file is     open.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSFile_IsOpen(CPU_CHAR  *name_full,
                           FS_FLAGS  *p_mode,
                           FS_ERR    *p_err)
{
    FS_FILE            *p_file;
    FS_QTY              max_ix;
    CPU_INT32U          ix;
    FS_FAT_FILE_DATA    disk_file_fat_data;
    FS_FAT_FILE_DATA   *p_fat_file_data;
    FS_VOL             *p_open_vol;
    FS_VOL             *p_disk_vol;
    CPU_CHAR           *file_name;
    CPU_BOOLEAN         file_open;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_YES);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name  ptr.                                  */
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_YES);
    }
    if (p_mode == (FS_FLAGS *)0) {                              /* Validate mode  ptr.                                  */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NO);
    }
#endif


                                                                /* --------------------- INIT MODE -------------------- */
   *p_mode    = FS_FILE_ACCESS_MODE_NONE;

                                                                /* --------------- GET FILE INFO ON DEV --------------- */
    file_name = FSFile_NameParseChk( name_full,                 /* Acquire vol ptr.                                     */
                                    &p_disk_vol,
                                     p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_NONE;
        return (DEF_NO);
    }

                                                                /* Get fat data from disk.                              */
    FS_FAT_LowEntryFind(p_disk_vol, &disk_file_fat_data, file_name, FS_FAT_MODE_FILE | FS_FAT_MODE_RD, p_err);


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


                                                                /* ------------- CMP TO EVERY FILE IN POOL ------------ */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return (DEF_NO);
    }

    max_ix = FSFile_FileCntMax;

    file_open = DEF_NO;
    ix        = 0u;
    while ((ix < max_ix) &&
           (file_open == DEF_NO)) {
        p_file = FSFile_Tbl[ix];

        if (p_file != DEF_NULL) {
            p_open_vol = p_file->VolPtr;
                                                                /* Cmp vol ptr.                                         */
            if (p_open_vol == p_disk_vol) {
                p_fat_file_data         = (FS_FAT_FILE_DATA *)p_file->DataPtr;

                                                                /* If dir entry has same loc, file is open.             */
                if (p_fat_file_data->DirEndSecPos == disk_file_fat_data.DirEndSecPos) {
                    if (p_fat_file_data->DirEndSec == disk_file_fat_data.DirEndSec) {
                        if (p_fat_file_data->DirStartSecPos == disk_file_fat_data.DirStartSecPos) {
                            if (p_fat_file_data->DirStartSec == disk_file_fat_data.DirStartSec) {
                               *p_mode = p_file->AccessMode;
                                file_open = DEF_YES;
                            }
                        }
                    }
                }
            }
        }

        ix++;
    }

    FS_OS_Unlock();


   *p_err   = FS_ERR_NONE;

    return (file_open);
}


/*
*********************************************************************************************************
*                                          FSFile_LockAccept()
*
* Description : Acquire task ownership of a file (if available).
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File lock acquired.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*                               FS_ERR_FILE_LOCKED      File owned by another task.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'FSFile_LockAccept()' is the non-blocking version of 'FSFile_LockGet()'; if the lock
*                   is not available, the function returns an error.
*
*               (2) See 'FSFile_LockGet()  Note(s)'.
*
*               (3) See 'fs_ftrylockfile()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FSFile_LockAccept (FS_FILE  *p_file,
                         FS_ERR   *p_err)

{
    CPU_BOOLEAN  locked;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    p_file = FSFile_Acquire(p_file);                            /* Acquire file ref.                                    */

    if (p_file == (FS_FILE *)0) {                               /* Rtn err if file not found.                           */
       *p_err = FS_ERR_FILE_NOT_OPEN;
        return;
    }

    locked = FSFile_LockAcceptHandler(p_file);
    FSFile_Release(p_file);                                     /* Release file ref.                                    */

    if (locked == DEF_NO) {
       *p_err = FS_ERR_FILE_LOCKED;
    } else {
       *p_err = FS_ERR_NONE;
    }
}
#endif


/*
*********************************************************************************************************
*                                          FSFile_LockGet()
*
* Description : Acquire task ownership of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File lock acquired.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : none.
*
* Note(s)     : (1) A lock count is associated with each file.
*
*                   (a) The file is unlocked when the lock count is zero.
*
*                   (b) If the lock count is positive, a task owns the file.
*
*                   (c) When 'FSFile_LockGet()' is called, if...
*
*                       (1) ...the lock count is zero                                   OR
*                       (2) ...the lock count is positive and the caller owns the file...
*
*                       ...the lock count will be incremented and the caller will own the file.
*                       Otherwise, the caller waits until the lock count returns to zero.
*
*                   (d) Each call to 'FSFile_LockSet()' increments the lock count.
*
*                   (e) Matching calls to 'FSFile_LockGet()' (or 'FSFile_LockAccept()') & 'FSFile_LockSet()'
*                       can be nested.
*
*               (2) See 'fs_flockfile()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FSFile_LockGet (FS_FILE  *p_file,
                      FS_ERR   *p_err)

{
    CPU_BOOLEAN  locked;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    p_file = FSFile_Acquire(p_file);                            /* Acquire file ref.                                    */

    if (p_file == (FS_FILE *)0) {                               /* Rtn err if file not found.                           */
       *p_err = FS_ERR_FILE_NOT_OPEN;
        return;
    }

    locked = FSFile_LockGetHandler(p_file);
    FSFile_Release(p_file);                                     /* Release file ref.                                    */

    if (locked == DEF_NO) {
       *p_err = FS_ERR_FILE_LOCKED;
    } else {
       *p_err = FS_ERR_NONE;
    }
}
#endif


/*
*********************************************************************************************************
*                                          FSFile_LockSet()
*
* Description : Release task ownership of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE               File lock released.
*                               FS_ERR_NULL_PTR           Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE       Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_NOT_OPEN      File NOT open.
*                               FS_ERR_FILE_NOT_LOCKED    File NOT locked or locked by a different task.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSFile_LockGet()  Note #1b'.
*
*               (2) If the lock is not held by the calling thread, no action is taken.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  FSFile_LockSet (FS_FILE  *p_file,
                      FS_ERR   *p_err)

{
    CPU_BOOLEAN  unlocked;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    p_file = FSFile_Acquire(p_file);                            /* Acquire file ref.                                    */

    if (p_file == (FS_FILE *)0) {                               /* Rtn err if file not found.                           */
       *p_err = FS_ERR_FILE_NOT_OPEN;
        return;
    }

    unlocked = FSFile_LockSetHandler(p_file);
    FSFile_Release(p_file);                                     /* Release file ref.                                    */

    if (unlocked == DEF_NO) {
       *p_err = FS_ERR_FILE_NOT_LOCKED;
    } else {
       *p_err = FS_ERR_NONE;
    }
}
#endif


/*
*********************************************************************************************************
*                                            FSFile_Open()
*
* Description : Open a file.
*
* Argument(s) : name_full   Name of the file.
*
*               mode        File access mode; valid bit-wise OR of one or more of the following (see Note #2):
*
*                               FS_FILE_ACCESS_MODE_RD          File opened for reads.
*                               FS_FILE_ACCESS_MODE_WR          File opened for writes.
*                               FS_FILE_ACCESS_MODE_CREATE      File will be created, if necessary.
*                               FS_FILE_ACCESS_MODE_TRUNCATE    File length will be truncated to 0.
*                               FS_FILE_ACCESS_MODE_APPEND      All writes will be performed at EOF.
*                               FS_FILE_ACCESS_MODE_EXCL        File will be opened if & only if it does
*                                                                   not already exist.
*                               FS_FILE_ACCESS_MODE_CACHED      File data will be cached.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                        File opened successfully.
*                               FS_ERR_NAME_NULL                   Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_FILE_NONE_AVAIL             No file available.
*                               FS_ERR_FILE_INVALID_ACCESS_MODE    Access mode is specified invalid.
*
*                                                                  --- RETURNED BY FSFile_NameParseChk() ---
*                               FS_ERR_NAME_INVALID                Entry name could not be parsed, lacks an
*                                                                     initial path separator character or
*                                                                     includes an invalid volume name.
*                               FS_ERR_NAME_PATH_TOO_LONG          Entry path is too long.
*                               FS_ERR_VOL_NOT_OPEN                Volume not opened.
*                               FS_ERR_VOL_NOT_MOUNTED             Volume not mounted.
*
*                                                                  ------ RETURNED BY FSSys_FileOpen() -----
*                               FS_ERR_BUF_NONE_AVAIL              Buffer not available.
*                               FS_ERR_DEV                         Device access error.
*                               FS_ERR_DEV_FULL                    Device is full (space could not be allocated).
*                               FS_ERR_DIR_FULL                    Directory is full (space could not be allocated).
*                               FS_ERR_ENTRY_CORRUPT               File system entry is corrupt.
*                               FS_ERR_ENTRY_EXISTS                File system entry exists.
*                               FS_ERR_ENTRY_NOT_FILE              File system entry NOT a file.
*                               FS_ERR_ENTRY_NOT_FOUND             File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND      Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR        Entry parent NOT a directory.
*                               FS_ERR_ENTRY_RD_ONLY               File system entry marked read-only.
*                               FS_ERR_NAME_INVALID                Invalid file name or path.
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : (1) Standard 'fopen()' access modes can be translated to mode flags according to the table:
*
*                     ---------------------------------------------------------------------------------------------------------------------------
*                     |        STRINGS       |   READ?   |   WRITE?  | TRUNCATE? |  CREATE?  |  APPEND?  |             EQUIVALENT MODE          |
*                     |-------------------------------------------------------------------------------------------------------------------------|
*                     | "r"  / "rb"          | DEF_TRUE  | DEF_FALSE | DEF_FALSE | DEF_FALSE | DEF_FALSE | RD                                   |
*                     | "w"  / "wb           | DEF_FALSE | DEF_TRUE  | DEF_TRUE  | DEF_TRUE  | DEF_FALSE | WR      | CREATE | TRUNCATE          |
*                     | "a"  / "ab"          | DEF_FALSE | DEF_TRUE  | DEF_FALSE | DEF_TRUE  | DEF_TRUE  | WR      | CREATE            | APPEND |
*                     | "r+" / "rb+" / "r+b" | DEF_TRUE  | DEF_TRUE  | DEF_FALSE | DEF_FALSE | DEF_FALSE | RD | WR                              |
*                     | "w+" / "wb+" / "w+b" | DEF_TRUE  | DEF_TRUE  | DEF_TRUE  | DEF_TRUE  | DEF_FALSE | RD | WR | CREATE | TRUNCATE          |
*                     | "a+" / "ab+" / "a+b" | DEF_TRUE  | DEF_TRUE  | DEF_FALSE | DEF_TRUE  | DEF_TRUE  | RD | WR | CREATE            | APPEND |
*                     ---------------------------------------------------------------------------------------------------------------------------
*
*               (2) (a) The following undefined mode combinations result in return errors :
*
*                       (1) Set FS_FILE_ACCESS_MODE_TRUNCATE & clear FS_FILE_ACCESS_MODE_WR;
*
*                       (2) Set FS_FILE_ACCESS_MODE_EXCL     & clear FS_FILE_ACCESS_MODE_CREATE.
*
*                       (3) Set FS_FILE_ACCESS_MODE_APPEND   & clear FS_FILE_ACCESS_MODE_WR.
*
*                       (4) Any other bits set.
*
*                   (b) FS_FILE_ACCESS_MODE_RD &/or FS_FILE_ACCESS_MODE_WR MUST be set.
*
*               (3) The file position indicator is set to 0 when a file is opened. If the file is opened
*                   in append mode, the position indicator will be set at the end of file for each
*                   subsequent write operation. The file position indicator should then not be set to a
*                   value different than 0 within this function.
*
*               (4) This check is redundant, since either both 'p_vol' & 'name_file' will receive valid
*                   pointers, or neither 'p_vol' nor 'name_file' will receive valid pointers.
*
*               (5) See 'fs_fopen()  Note(s)'.
*********************************************************************************************************
*/

FS_FILE  *FSFile_Open (CPU_CHAR  *name_full,
                       FS_FLAGS   mode,
                       FS_ERR    *p_err)
{
    CPU_CHAR       *name_file;
    CPU_CHAR       *name_full_temp;
    FS_FILE        *p_file;
    FS_VOL         *p_vol;
    FS_ERR          err;
    FS_ENTRY_INFO   file_info;
#if (FS_CFG_CONCURRENT_ENTRIES_ACCESS_EN == DEF_DISABLED)
    CPU_BOOLEAN     file_open;
    CPU_BOOLEAN     is_write_mode;
    FS_FLAGS        open_mode;
#endif


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION((FS_FILE *)0);
    }

#if (FS_CFG_RD_ONLY_EN == DEF_ENABLED)                          /* Rd-only mode                                         */
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *) 0);
    }
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_CREATE) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *) 0);
    }
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_EXCL) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *) 0);
    }
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_APPEND) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *) 0);
    }
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_TRUNCATE) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *) 0);
    }
#endif

    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return ((FS_FILE *)0);
    }
                                                                /* Validate mode (see Note #2b).                        */
    if (DEF_BIT_IS_CLR(mode, FS_FILE_ACCESS_MODE_RD | FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *)0);
    }
                                                                /* See Note #2a1.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_TRUNCATE) == DEF_YES) {
        if (DEF_BIT_IS_CLR(mode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
           *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
            return ((FS_FILE *)0);
        }
    }
                                                                /* See Note #2a2.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_EXCL) == DEF_YES) {
        if (DEF_BIT_IS_CLR(mode, FS_FILE_ACCESS_MODE_CREATE) == DEF_YES) {
           *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
            return ((FS_FILE *)0);
        }
    }
                                                                /* See Note #2a3.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_APPEND) == DEF_YES) {
        if (DEF_BIT_IS_CLR(mode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
           *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
            return ((FS_FILE *)0);
        }
    }
                                                                /* See Note #2a4.                                       */
    if ((mode & (FS_FILE_ACCESS_MODE_RD       | FS_FILE_ACCESS_MODE_WR     | FS_FILE_ACCESS_MODE_APPEND |
                 FS_FILE_ACCESS_MODE_TRUNCATE | FS_FILE_ACCESS_MODE_CREATE | FS_FILE_ACCESS_MODE_EXCL   |
                 FS_FILE_ACCESS_MODE_CACHED                                                             )) != mode) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *)0);
    }
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_ENABLED)
    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_APPEND) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *)0);
    }

    if (DEF_BIT_IS_SET(mode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return ((FS_FILE *)0);
    }
#endif

                                                                /* ------------------- GET FREE FILE ------------------ */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock.                                     */
    if (*p_err != FS_ERR_NONE) {
        return ((FS_FILE *)0);
    }

    p_file = FSFile_ObjGet(p_err);                              /* Alloc file.                                          */
    if (*p_err != FS_ERR_NONE) {
       *p_err = FS_ERR_FILE_NONE_AVAIL;
        FS_OS_Unlock();
        return ((FS_FILE *)0);
    }

    p_file->AccessMode =  mode;
    p_file->RefCnt     =  1u;
    p_file->State      =  FS_FILE_STATE_OPENING;
    p_file->VolPtr     = (FS_VOL *)0;

    FS_OS_Unlock();                                             /* Release FS lock.                                     */

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
    (void)FSFile_LockGetHandler(p_file);
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
        (void)FSFile_LockSetHandler(p_file);
#endif
        FSFile_Release(p_file);
        return ((FS_FILE *)0);
    }
#else
    name_full_temp = name_full;
#endif


                                                                /* ---------- VERIFY IF FILE OPEN IN WR MODE ---------- */
#if (FS_CFG_CONCURRENT_ENTRIES_ACCESS_EN == DEF_DISABLED)
    file_open = FSFile_IsOpen(name_full_temp, &open_mode, p_err);

    if (*p_err != FS_ERR_NONE) {

#if (FS_CFG_FILE_LOCK_EN ==   DEF_ENABLED)
        (void)FSFile_LockSetHandler(p_file);
#endif
        FSFile_Release(p_file);                                 /* ... release file ref.                                */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
        if (name_full_temp != name_full) {
            FS_WorkingDirObjFree(name_full_temp);
        }
#endif

            return ((FS_FILE *)0);
    }


    if (file_open == DEF_YES) {
                                                                /* If file is already open ...                          */
                                                                /*                         ... or going to be open ...  */
                                                                /*                         ... in write mode ...        */
        is_write_mode = ((open_mode & FS_FILE_ALL_WR_ACCESS_MODE) != 0u);
        is_write_mode = ((mode      & FS_FILE_ALL_WR_ACCESS_MODE) != 0u) || is_write_mode;

        if (is_write_mode == DEF_YES) {

#if (FS_CFG_FILE_LOCK_EN ==   DEF_ENABLED)
            (void)FSFile_LockSetHandler(p_file);
#endif
            FSFile_Release(p_file);                             /* ... release file ref.                                */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
            if (name_full_temp != name_full) {
                FS_WorkingDirObjFree(name_full_temp);
            }
#endif

           *p_err = FS_ERR_FILE_ALREADY_OPEN;
            return ((FS_FILE *)0);
        }
    }
#endif


                                                                /* ------------------ ACQUIRE VOL REF ----------------- */
    name_file = FSFile_NameParseChk( name_full_temp,
                                    &p_vol,
                                     p_err);


    if ((p_vol     == (FS_VOL   *)0) ||                         /* If no vol found                           ...        */
        (name_file == (CPU_CHAR *)0)) {                         /* ... or file name not parsed (see Note #4) ...        */
#if (FS_CFG_FILE_LOCK_EN ==   DEF_ENABLED)
        (void)FSFile_LockSetHandler(p_file);
#endif
        FSFile_Release(p_file);                                 /* ... release file ref.                                */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
        if (name_full_temp != name_full) {
            FS_WorkingDirObjFree(name_full_temp);
        }
#endif
        return ((FS_FILE *)0);
    }



                                                                /* --------------------- OPEN FILE -------------------- */
    p_file->VolPtr = p_vol;

    if (name_file[0] == (CPU_CHAR)ASCII_CHAR_NULL) {            /* Rtn err if entry specifies root dir.                 */
        p_file->State = FS_FILE_STATE_CLOSING;
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return ((FS_FILE *)0);
    }

                                                                /* Chk vol mode.                                        */
    if ((DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR)  == DEF_YES) &&
        (DEF_BIT_IS_SET(mode,              FS_FILE_ACCESS_MODE_WR) == DEF_YES)) {
        p_file->State = FS_FILE_STATE_CLOSING;
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return ((FS_FILE *)0);
    }

    FSSys_FileOpen(p_file,                                      /* Open file.                                           */
                   name_file,
                   p_err);
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
    if (*p_err != FS_ERR_NONE) {
         p_file->State = FS_FILE_STATE_CLOSING;
         FSFile_ReleaseUnlock(p_file);
         return ((FS_FILE *)0);
    }

    p_file->State      = FS_FILE_STATE_OPEN;
    p_file->RefreshCnt = p_vol->RefreshCnt;
    FSVol_FileAdd(p_vol, p_file);                               /* Link file to vol file list.                          */

    FSSys_FileQuery(p_file, &file_info, p_err);
    if (*p_err != FS_ERR_NONE) {
         FSSys_FileClose(p_file, &err);
         p_file->State = FS_FILE_STATE_CLOSING;
         FSFile_ReleaseUnlock(p_file);
         return ((FS_FILE *)0);
    }

    p_file->Size = file_info.Size;
    p_file->FlagEOF = DEF_NO;
    p_file->Pos = 0u;                                           /* Set init file pos indicator to 0 (see Note #3).      */


#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
    p_file->BufSecSize = p_vol->SecSize;
#endif


                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_Unlock(p_file);                                      /* Keep init ref.                                       */

    return (p_file);
}


/*
*********************************************************************************************************
*                                            FSFile_PosGet()
*
* Description : Get file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File position gotten successfully.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_ERR         File has error (see Note #3).
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : The current file position, if no errors (see Note #1).
*               0,                         otherwise.
*
* Note(s)     : (1) The file position returned is measured in bytes from the beginning of the file.
*
*               (2) See 'fs_ftell()  Note(s)'.
*
*               (3) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

FS_FILE_SIZE  FSFile_PosGet (FS_FILE  *p_file,
                             FS_ERR   *p_err)
{
    FS_FILE_SIZE  pos;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (0u);
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #3).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return (0u);
    }



                                                                /* ---------------------- GET POS --------------------- */
    pos = p_file->Pos;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
    return (pos);
}


/*
*********************************************************************************************************
*                                            FSFile_PosSet()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               offset      Offset from the file position specified by 'origin'.
*
*               origin      Reference position for offset :
*
*                               FS_FILE_ORIGIN_START    Offset is from the beginning of the file.
*                               FS_FILE_ORIGIN_CUR      Offset is from current file position.
*                               FS_FILE_ORIGIN_END      Offset is from the end       of the file.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   File position set successfully.
*                               FS_ERR_NULL_PTR               Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE           Argument 'p_file's TYPE is invalid or
*                                                                 unknown.
*                               FS_ERR_FILE_ERR               File has error (see Note #5).
*                               FS_ERR_FILE_INVALID_OP        Invalid operation on file (see Note #2b).
*                               FS_ERR_FILE_INVALID_ORIGIN    Invalid origin specified.
*                               FS_ERR_FILE_INVALID_OFFSET    Invalid offset specified.
*
*                                                             --- RETURNED BY FSFile_AcquireLockChk() ---
*                               FS_ERR_DEV_CHNGD              Device has changed.
*                               FS_ERR_FILE_NOT_OPEN          File NOT open.
*
*                                                             ------ RETURNED BY FSFile_BufEmpty() ------
*                                                             ------ RETURNED BY FSSys_PosSet() ---------
*                               FS_ERR_BUF_NONE_AVAIL         No buffer available.
*                               FS_ERR_DEV                    Device access error.
*                               FS_ERR_DEV_FULL               Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT          File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) If a read or write error occurs, the error indicator is set.
*
*                   (b) The new file position, measured in bytes from the beginning of the file is
*                       obtained by adding 'offset' to...
*
*                       (1) ...0 (the beginning of the file) if 'origin' is FS_FILE_ORIGIN_START,
*
*                       (2) ...the current file position     if 'origin' is FS_FILE_ORIGIN_CUR.
*
*                       (3) ...the file size                 if 'origin' is FS_FILE_ORIGIN_END.
*
*                   (c) The end-of-file indicator is cleared.
*
*               (2) If the file position indicator is set beyond the file's current data...
*
*                   (a) ...& data is later written to that point, reads from the gap will read 0.
*
*                   (b) ...the file MUST be opened in write or read/write mode.
*
*               (3) See also 'fs_fseek()  Note(s)'.
*
*               (4) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

void  FSFile_PosSet (FS_FILE         *p_file,
                     FS_FILE_OFFSET   offset,
                     FS_STATE         origin,
                     FS_ERR          *p_err)
{
    FS_FILE_OFFSET   chk_pos;
    FS_FILE_SIZE     start_pos;
    FS_FILE_SIZE     set_pos;
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    CPU_INT32U       fill_size;
    CPU_INT32U       wr_size;
    CPU_INT32U       buf_size;
    FS_BUF          *p_buf;
#endif

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (origin != FS_FILE_ORIGIN_START) {                       /* Validate origin.                                     */
        if (origin != FS_FILE_ORIGIN_CUR) {
            if (origin != FS_FILE_ORIGIN_END) {
               *p_err = FS_ERR_FILE_INVALID_ORIGIN;
                return;
            }
        }
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
   (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #5).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return;
    }



                                                                /* ---------------- HANDLE FILE BUFFER ---------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONE) {         /* Blk buf assignment.                                  */
        p_file->BufStatus =  FS_FILE_BUF_STATUS_NEVER;
    }
                                                                /* Flush buf before pos set.                            */
    if ((p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_RD) ||
        (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_WR)) {
        FSFile_BufEmpty(p_file, p_err);

        if (*p_err != FS_ERR_NONE) {                            /* Rtn if rd/wr err (see Note #1a).                     */
            p_file->FlagErr = DEF_YES;
            p_file->FlagEOF = DEF_NO;
            FSFile_ReleaseUnlock(p_file);
            return;
        }
    }
#endif


                                                                /* ------------------- CALC NEW POS ------------------- */
    if ((offset == 0) && (origin == FS_FILE_ORIGIN_CUR)) {      /* If 0 offset from cur       ...                       */
        p_file->IO_State = FS_FILE_IO_STATE_NONE;
        p_file->FlagEOF  = DEF_NO;                              /* ... clr EOF (see Note #1b) ...                       */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_NONE;
        return;                                                 /* ... & rtn.                                           */
    }

    switch (origin) {                                           /* Start (ref) pos for offset (see Note #1b) : ...      */
        case FS_FILE_ORIGIN_CUR:                                /* ... current file pos                        ...      */
             start_pos = p_file->Pos;
             break;

        case FS_FILE_ORIGIN_END:                                /* ... EOF                                     ...      */
             start_pos = p_file->Size;
             break;

        case FS_FILE_ORIGIN_START:                              /* ... start of file.                                   */
        default:
             start_pos = 0u;
             break;

    }

    if (offset < 0) {                                           /* Neg offset ... chk for neg ovf.                      */
        if (offset == FS_FILE_OFFSET_MIN) {
            if ((FS_FILE_SIZE)FS_FILE_OFFSET_MAX + 1u > start_pos) {
               *p_err = FS_ERR_FILE_INVALID_OFFSET;
                FSFile_ReleaseUnlock(p_file);
                return;
            }
        } else {
            chk_pos = (FS_FILE_OFFSET)start_pos + offset;
            if (chk_pos < 0) {
               *p_err = FS_ERR_FILE_INVALID_OFFSET;
                FSFile_ReleaseUnlock(p_file);
                return;
            }
        }
    } else {
        if (offset > 0) {                                       /* Pos offset ... chk for pos ovf.                      */
            if (FS_FILE_SIZE_MAX - (FS_FILE_SIZE)offset < start_pos) {
               *p_err = FS_ERR_FILE_INVALID_OFFSET;
                FSFile_ReleaseUnlock(p_file);
                return;
            }
                                                                /* Chk file mode (see Note #2b).                        */
            if ((FS_FILE_SIZE)offset + start_pos > p_file->Size) {
                if (DEF_BIT_IS_CLR(p_file->AccessMode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
                   *p_err = FS_ERR_FILE_INVALID_OFFSET;
                    FSFile_ReleaseUnlock(p_file);
                    return;
                }
            }
        }
    }

    set_pos = start_pos + offset;                               /* Calc pos to set.                                     */


    if (set_pos > p_file->Size) {                               /* If pos past file data ...                            */
#if (FS_CFG_RD_ONLY_EN == DEF_ENABLED)
       *p_err = FS_ERR_FILE_INVALID_OFFSET;
        FSFile_ReleaseUnlock(p_file);
        return;
#else
                                                                /* ... there is no need to chk file mode because ...    */
                                                                /* ... it has already been chk'd (see Note #2b).        */

                                                                /* Fill empty area with '\0'                            */
        fill_size = set_pos - p_file->Size;
                                                                /* Alloc buf filled with '\0'                           */
        p_buf = FSBuf_Get(p_file->VolPtr);
        if (p_buf == (FS_BUF *)0) {
           *p_err = FS_ERR_BUF_NONE_AVAIL;
            FSFile_ReleaseUnlock(p_file);
            return;
        }
        buf_size = p_buf->Size;

        Mem_Set ((void *)      p_buf->DataPtr,
                 (CPU_INT08U)  ASCII_CHAR_NULL,
                 (CPU_SIZE_T)  buf_size);
                                                                /* Set pos after last valid data                        */
        FSSys_FilePosSet(p_file, p_file->Size, p_err);
        if (*p_err != FS_ERR_NONE) {
            FSFile_ReleaseUnlock(p_file);
            FSBuf_Free(p_buf);
            return;
        }

                                                                /* Write '\0' to file                                   */

                                                                /* Wr rem of sec.                                       */
        wr_size = p_file->VolPtr->SecSize - (p_file->Pos % p_file->VolPtr->SecSize);
        if (fill_size < wr_size) {                              /* Limit wr to tot size.                                */
            wr_size = fill_size;
        }

        while (fill_size > 0u) {
            FSSys_FileWr (p_file,
                          p_buf->DataPtr,
                          wr_size,
                          p_err);
            if (*p_err != FS_ERR_NONE) {
                FSFile_ReleaseUnlock(p_file);
                FSBuf_Free(p_buf);
                return;
            }
            fill_size    -= wr_size;
            p_file->Size += wr_size;

            if (fill_size > buf_size) {                         /* Limit wr to buf size.                                */
                wr_size = buf_size;
            } else {
                wr_size = fill_size;
            }
        }
        FSBuf_Free(p_buf);
#endif
    }

    FSSys_FilePosSet(p_file, set_pos, p_err);

    if (*p_err != FS_ERR_NONE) {
        p_file->FlagErr = DEF_YES;                              /* Set err indicator (see Note #1a).                    */
    } else {
        p_file->FlagEOF = DEF_NO;                               /* Clr EOF (see Note #1b).                              */
        p_file->Pos     = set_pos;
    }

    p_file->IO_State = FS_FILE_IO_STATE_NONE;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
}


/*
*********************************************************************************************************
*                                             FSFile_Query()
*
* Description : Get information about a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_info      Pointer to structure that will receive the file information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File information obtained successfully.
*                               FS_ERR_NULL_PTR         Argument 'p_file'/'p_info' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or
*                                                           unknown.
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
*                                                       --------- RETURNED BY FSSys_FileQuery() ---------
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) If data is stored in the file buffer waiting to be written to the storage medium,
*                   the actual file size may need to be adjusted to account for that buffered data.
*********************************************************************************************************
*/

void  FSFile_Query (FS_FILE        *p_file,
                    FS_ENTRY_INFO  *p_info,
                    FS_ERR         *p_err)

{
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (p_info == (FS_ENTRY_INFO *)0) {                         /* Validate file info ptr.                              */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* --------------------- GET INFO --------------------- */
    FSSys_FileQuery(p_file, p_info, p_err);
    if (*p_err != FS_ERR_NONE) {
        FSFile_ReleaseUnlock(p_file);
        return;
    }


                                                                /* ---------------- HANDLE FILE BUFFER ---------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_WR) {  /* See Note #1.                                         */
        p_info->Size = p_file->Size;


    }
#endif



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
}


/*
*********************************************************************************************************
*                                              FSFile_Rd()
*
* Description : Read from a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_dest      Pointer to destination buffer.
*
*               size        Number of octets to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   File read successfully.
*                               FS_ERR_NULL_PTR               Argument 'p_file'/'p_dest' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE           Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_ERR               File has error (see Note #5).
*                               FS_ERR_FILE_INVALID_OP        Invalid operation on file (see Note #3).
*                               FS_ERR_FILE_INVALID_OP_SEQ    Invalid operation sequence on file (see Note #2).
*
*                                                             --- RETURNED BY FSFile_AcquireLockChk() ---
*                               FS_ERR_DEV_CHNGD              Device has changed.
*                               FS_ERR_FILE_NOT_OPEN          File NOT open.
*
*                                                             -------- RETURNED BY FSSys_FileRd() -------
*                               FS_ERR_BUF_NONE_AVAIL         No buffer available.
*                               FS_ERR_DEV                    Device access error.
*                               FS_ERR_ENTRY_CORRUPT          File system entry corrupt.
*
* Return(s)   : Number of bytes read, if no error.
*               0,                    otherwise.
*
* Note(s)     : (1) If 'size' is zero, then the file is unchanged & zero is returned.
*
*               (2) If the last operation is output (write), then a call to 'FSFile_BufFlush()' or
*                  'FSFile_PosSet()' MUST occur before input (read) can be performed.  See 'fs_fopen()
*                   Note #1e'.
*
*                   (a) This check of the buffer status is redundant; the buffer should NEVER be non-
*                       empty with write data without the file being in the I/O write state.
*
*               (3) The file MUST have been opened in read or update (read/write) mode.
*
*               (4) See 'fs_fread()  Note(s)'.
*
*               (5) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

CPU_SIZE_T  FSFile_Rd (FS_FILE     *p_file,
                       void        *p_dest,
                       CPU_SIZE_T   size,
                       FS_ERR      *p_err)
{
    CPU_SIZE_T  size_rd;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (0u);
    }
    if (p_dest == (void *)0) {                                  /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (0u);
    }
#endif


                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

                                                                /* Chk file mode (see Note #3).                         */
    if (DEF_BIT_IS_CLR(p_file->AccessMode, FS_FILE_ACCESS_MODE_RD) == DEF_YES) {
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_INVALID_OP;
        return (0u);
    }

    if (p_file->IO_State == FS_FILE_IO_STATE_WR) {              /* Chk state (see Note #2).                             */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_INVALID_OP_SEQ;
        return (0u);
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #5).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return (0u);
    }

    if (size == 0u) {                                           /* Rtn 0 bytes rd (see Note #1).                        */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_NONE;
        return (0u);
    }



                                                                /* ---------------- HANDLE FILE BUFFER ---------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN  == DEF_DISABLED)
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_WR) {  /* Chk buf status (see Note #2a).                       */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_INVALID_OP_SEQ;
        return (0u);
    }
#endif
                                                                /* Read from buf.                                       */
    if (DEF_BIT_IS_SET(p_file->BufMode, FS_FILE_BUF_MODE_RD) == DEF_YES) {
        size_rd = FSFile_BufRd(p_file,
                               p_dest,
                               size,
                               p_err);

        if (size_rd != 0u) {                                    /* If data rd, set I/O state.                           */
            p_file->IO_State = FS_FILE_IO_STATE_RD;
        }

        FSFile_ReleaseUnlock(p_file);
        return (size_rd);
    }

                                                                /* Blk buf assignment.                                  */
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONE) {
        p_file->BufStatus =  FS_FILE_BUF_STATUS_NEVER;
    }
#endif



                                                                /* ---------------------- RD FILE --------------------- */
    size_rd = FSSys_FileRd(p_file,
                           p_dest,
                           size,
                           p_err);

    p_file->Pos += size_rd;


                                                                /* ----------------- UPDATE FILE FLAGS ---------------- */
    switch (*p_err) {
        case FS_ERR_BUF_NONE_AVAIL:
             break;

        case FS_ERR_NONE:
             if (size_rd != 0u) {
                 p_file->IO_State = FS_FILE_IO_STATE_RD;
             }
             if (size_rd != size) {
                 p_file->FlagEOF = DEF_YES;
             } else {
                 p_file->FlagEOF = DEF_NO;
             }
             break;

        default:                                                /* Update err flag.                                     */
             p_file->FlagEOF = DEF_NO;
             p_file->FlagErr = DEF_YES;
             break;
    }



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
    return (size_rd);
}


/*
*********************************************************************************************************
*                                          FSFile_Truncate()
*
* Description : Truncate a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               size        Size of file after truncation.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File truncated successfully.
*                               FS_ERR_NULL_PTR         Argument 'p_file' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE     Argument 'p_file's TYPE is invalid or unknown.
*                               FS_ERR_FILE_ERR         File has error (see Note #5).
*                               FS_ERR_FILE_INVALID_OP  Invalid operation on file.
*
*                                                       ------ RETURNED BY FSFile_AcquireLockChk() ------
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
*                                                       --------- RETURNED BY FSFile_BufEmpty() ---------
*                                                       --------- RETURNED BY FSSys_Truncate() ----------
*                                                       --------- RETURNED BY FSSys_PosSet() ------------
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file MUST be opened in write or read/write mode.
*
*               (2) If 'FSFile_Truncate()' succeeds, the size of the file shall be equal to 'size':
*
*                   (a) If the size of the file was previously greater than 'size', the extra data shall
*                       no longer be available.
*
*                   (b) If the file previously was smaller than this 'size', the size of the file shall
*                       be increased.
*
*               (3) If the file position indicator before the call to 'FSFile_Truncate()' lay in the
*                   extra data destroyed by the function, then the file position will be set to the
*                   end-of-file.
*
*               (4) See 'fs_ftruncate()  Note(s)'.
*
*               (5) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSFile_Truncate (FS_FILE       *p_file,
                       FS_FILE_SIZE   size,
                       FS_ERR        *p_err)
{
    CPU_INT32U       fill_size;
    CPU_INT32U       wr_size;
    CPU_INT32U       buf_size;
    FS_BUF          *p_buf;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }
                                                                /* Chk file mode (see Note #1).                         */
    if (DEF_BIT_IS_CLR(p_file->AccessMode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_INVALID_OP;
        return;
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #5).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return;
    }



                                                                /* ---------------- HANDLE FILE BUFFER ---------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
                                                                /* Blk buf assignment.                                  */
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONE) {
        p_file->BufStatus =  FS_FILE_BUF_STATUS_NEVER;


                                                                /* Empty buf.                                           */
    } else {
        FSFile_BufEmpty(p_file, p_err);
        if (*p_err != FS_ERR_NONE) {
            FSFile_ReleaseUnlock(p_file);
            return;
        }
    }
#endif



                                                                /* ------------------- TRUNCATE FILE ------------------ */
    if (size == p_file->Size) {                                 /* If no chng in size ...                               */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_NONE;                                    /* ... nothing to do.                                   */
        return;
    }

    if (size > p_file->Size) {                                  /* Check if the file must be truncated or extended.     */
                                                                /* Extension needed ... */
                                                                /* ... fill empty area with '\0'                        */
        fill_size = size - p_file->Size;
                                                                /* Alloc buf filled with '\0'                           */
        p_buf = FSBuf_Get(p_file->VolPtr);
        if (p_buf == (FS_BUF *)0) {
           *p_err = FS_ERR_BUF_NONE_AVAIL;
            FSFile_ReleaseUnlock(p_file);
            return;
        }
        buf_size = p_buf->Size;

        Mem_Set ((void *)      p_buf->DataPtr,
                 (CPU_INT08U)  ASCII_CHAR_NULL,
                 (CPU_SIZE_T)  buf_size);
                                                                /* Set pos after last valid data                        */
        FSSys_FilePosSet(p_file, p_file->Size, p_err);
        if (*p_err != FS_ERR_NONE) {
            FSFile_ReleaseUnlock(p_file);
            FSBuf_Free(p_buf);
            return;
        }

                                                                /* Write '\0' to file                                   */

                                                                /* Wr rem of sec.                                       */
        wr_size = p_file->VolPtr->SecSize - (p_file->Pos % p_file->VolPtr->SecSize);
        if (fill_size < wr_size) {                              /* Limit wr to tot size.                                */
            wr_size = fill_size;
        }

        while (fill_size > 0u) {
            FSSys_FileWr (p_file,
                          p_buf->DataPtr,
                          wr_size,
                          p_err);
            if (*p_err != FS_ERR_NONE) {
                FSFile_ReleaseUnlock(p_file);
                FSBuf_Free(p_buf);
                return;
            }
            fill_size    -= wr_size;
            p_file->Size += wr_size;

            if (fill_size > buf_size) {                         /* Limit wr to buf size.                                */
                wr_size = buf_size;
            } else {
                wr_size = fill_size;
            }
        }

        FSSys_FilePosSet(p_file, p_file->Pos, p_err);           /* Restore the original position.                       */
        if (*p_err != FS_ERR_NONE) {
            FSFile_ReleaseUnlock(p_file);
            FSBuf_Free(p_buf);
            return;
        }

        FSBuf_Free(p_buf);

    } else {
        FSSys_FileTruncate(p_file,                              /* Truncate file.                                       */
                           size,
                           p_err);
    }

    if (*p_err != FS_ERR_NONE) {
        p_file->FlagEOF = DEF_NO;
        p_file->FlagErr = DEF_YES;
        FSFile_ReleaseUnlock(p_file);
        return;
    }

    if (size < p_file->Pos) {
        p_file->Pos = size;                                     /* Set file pos to new EOF (see Note #3).               */
    }

    p_file->Size    =  size;
    p_file->FlagEOF =  DEF_NO;
    p_file->FlagErr = (*p_err != FS_ERR_NONE) ? DEF_YES : DEF_NO;



                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
}
#endif


/*
*********************************************************************************************************
*                                              FSFile_Wr()
*
* Description : Write to a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_src       Pointer to source buffer.
*
*               size        Number of octets to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   File written successfully.
*                               FS_ERR_NULL_PTR               Argument 'p_file'/'p_src' passed a NULL pointer.
*                               FS_ERR_INVALID_TYPE           Argument 'p_file's TYPE is invalid or
*                                                                 unknown.
*                               FS_ERR_FILE_ERR               File has error (see Note #6).
*                               FS_ERR_FILE_INVALID_OP        Invalid operation on file (see Note #3).
*                               FS_ERR_FILE_INVALID_OP_SEQ    Invalid operation sequence on file (see Note #2).
*                               FS_ERR_FILE_OVF               File size/position would be overflowed if
*                                                                 write were executed (see Note #4).
*
*                                                             --- RETURNED BY FSFile_AcquireLockChk() ---
*                               FS_ERR_DEV_CHNGD              Device has changed.
*                               FS_ERR_FILE_NOT_OPEN          File NOT open.
*
*                                                             -------- RETURNED BY FSSys_FileWr() -------
*                                                             -------- RETURNED BY FSSys_PosSet() -------
*                               FS_ERR_BUF_NONE_AVAIL         No buffer available.
*                               FS_ERR_DEV                    Device access error.
*                               FS_ERR_DEV_FULL               Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT          File system entry is corrupt.
*
* Return(s)   : Number of octets written, if no error (see Notes #1a & #1b).
*               0,                        otherwise.
*
* Note(s)     : (1) If 'size' is zero, then the file is unchanged & zero is returned.
*
*               (2) If the last operation is input (read), then a call to 'FSFile_PosSet()' MUST occur
*                   before output (write) can be performed, unless the end-of-file was encountered.  See
*                  'fs_fopen()  Note #1e'.
*
*                   (a) If data is in the read buffer, the buffer MUST be emptied before the write.
*
*               (3) The file MUST have been opened in write or update (read/write) mode.
*
*               (4) If the file was opened in append mode, all writes are forced to the EOF.
*
*                   (a) This is only necessary if the file buffer is non-empty; if the file buffer
*                       contains data, then all writes are automatically at the EOF (since the file
*                       position was set to the EOF before the first write to the buffer).
*
*               (5) Overflow of the file position MUST be checked.  If the write would result in overflow,
*                   no data shall be written & an error shall be returned.
*
*               (6) If an error occurred in the previous file access, the error indicator must be
*                   cleared (with 'FSFile_ClrErr()') before another access will be allowed.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_SIZE_T  FSFile_Wr (FS_FILE     *p_file,
                       void        *p_src,
                       CPU_SIZE_T   size,
                       FS_ERR      *p_err)
{
    FS_FILE_SIZE  pos;
    CPU_SIZE_T    size_wr;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(0u);
    }
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return (0u);
    }
    if (p_src == (void *)0) {                                   /* Validate src ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return (0u);
    }
#endif

                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    (void)FSFile_AcquireLockChk(p_file, p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

                                                                /* Chk file mode (see Note #3).                         */
    if (DEF_BIT_IS_CLR(p_file->AccessMode, FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_INVALID_OP;
        return (0u);
    }

    if (p_file->IO_State == FS_FILE_IO_STATE_RD) {              /* Chk for wr state (see Note #2) ...                   */
        if (p_file->FlagEOF == DEF_NO) {                        /* ... or EOF exceeded            ...                   */
            if (p_file->Pos != p_file->Size) {                  /* ... or EOF met.                                      */
                FSFile_ReleaseUnlock(p_file);
               *p_err = FS_ERR_FILE_INVALID_OP_SEQ;
                return (0u);
            }
        }
    }

    if (p_file->FlagErr == DEF_YES) {                           /* Chk for file err (see Note #6).                      */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_ERR;
        return (0u);
    }

    if (size == 0u) {                                           /* Rtn 0 bytes wr (see Note #1).                        */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_NONE;
        return (0u);
    }

    p_file->FlagEOF = DEF_NO;                                   /* Clr EOF.                                             */



                                                                /* ---------------- HANDLE FILE BUFFER ---------------- */
#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_RD) {  /* Chk buf status (see Note #2a).                       */
        FSFile_BufEmpty(p_file, p_err);

        if (*p_err != FS_ERR_NONE) {                            /* Rtn if rd/wr err.                                    */
            p_file->FlagErr = DEF_YES;
            FSFile_ReleaseUnlock(p_file);
            return (0u);
        }
    }

                                                                /* Wr to buf.                                           */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if (DEF_BIT_IS_SET(p_file->BufMode, FS_FILE_BUF_MODE_WR) == DEF_YES) {
        if (p_file->BufStatus == FS_FILE_BUF_STATUS_EMPTY) {
            if (DEF_BIT_IS_SET(p_file->AccessMode, FS_FILE_ACCESS_MODE_APPEND) == DEF_YES) {
                if (p_file->Pos != p_file->Size) {                  /* If pos NOT at EOF ...                            */
                    FSSys_FilePosSet(p_file, p_file->Size, p_err);  /* ... set pos to EOF (see Note #4a).               */

                    if (*p_err != FS_ERR_NONE) {
                        p_file->FlagErr = DEF_YES;
                        FSFile_ReleaseUnlock(p_file);
                        return (0u);
                    }

                    p_file->Pos = p_file->Size;
                }
            }
        }

        size_wr = FSFile_BufWr(p_file,
                               p_src,
                               size,
                               p_err);

        if (*p_err != FS_ERR_NONE ) {
            p_file->FlagErr = DEF_NO;
            FSFile_ReleaseUnlock(p_file);
            return (0u);
        }

        if (size_wr != 0u) {                                    /* Set I/O state.                                       */
            p_file->IO_State = FS_FILE_IO_STATE_WR;
        }

        FSFile_ReleaseUnlock(p_file);
        return (size_wr);
    }
#endif

                                                                /* Blk buf assignment.                                  */
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONE) {
        p_file->BufStatus =  FS_FILE_BUF_STATUS_NEVER;
    }
#endif


                                                                /* ------------------- SET FILE POS ------------------- */
                                                                /* See Note #4.                                         */
    if (DEF_BIT_IS_SET(p_file->AccessMode, FS_FILE_ACCESS_MODE_APPEND) == DEF_YES) {
        if (p_file->Pos != p_file->Size) {                      /* If pos NOT at EOF ...                                */
            FSSys_FilePosSet(p_file, p_file->Size, p_err);      /* ... set pos to EOF.                                  */

            if (*p_err != FS_ERR_NONE) {
                p_file->FlagErr = DEF_YES;
                FSFile_ReleaseUnlock(p_file);
                return (0u);
            }

            p_file->Pos = p_file->Size;
        }
    }



                                                                /* ---------------------- WR FILE --------------------- */
    pos = p_file->Pos;                                          /* See Note #5.                                         */
    if (FS_FILE_SIZE_MAX - (FS_FILE_SIZE)size < pos) {          /* Rtn err & 0 bytes wr'n if wr will ovf max file size. */
        FSFile_ReleaseUnlock(p_file);
       *p_err = FS_ERR_FILE_OVF;
        return (0u);
    }

    size_wr = FSSys_FileWr(p_file,                              /* Wr to file.                                          */
                           p_src,
                           size,
                           p_err);

    if (*p_err != FS_ERR_NONE ) {
        p_file->FlagErr = DEF_YES;
        FSFile_ReleaseUnlock(p_file);
        return (0u);
    }

    p_file->IO_State = FS_FILE_IO_STATE_WR;
    p_file->Pos += size_wr;
    if (p_file->Size < p_file->Pos) {
        p_file->Size = p_file->Pos;
    }

                                                                /* ----------------- RELEASE FILE LOCK ---------------- */
    FSFile_ReleaseUnlock(p_file);
    return (size_wr);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      FILE MANAGEMENT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSFile_GetFileCnt()
*
* Description : Get the number of open files.
*
* Argument(s) : none.
*
* Return(s)   : The number of open files.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSFile_GetFileCnt (void)
{
    FS_QTY  file_cnt;
    FS_ERR  err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return (0u);
    }



                                                                /* ------------------- GET FILE CNT ------------------- */
    file_cnt = FSFile_Cnt;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (file_cnt);
}


/*
*********************************************************************************************************
*                                       FSFile_GetFileCntMax()
*
* Description : Get the maximum possible number of open files.
*
* Argument(s) : none.
*
* Return(s)   : The maximum number of open files.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSFile_GetFileCntMax (void)
{
    FS_QTY  file_cnt_max;


                                                                /* ----------------- GET FILE CNT MAX ----------------- */
    file_cnt_max = FSFile_FileCntMax;

    return (file_cnt_max);
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
*                                         FSFile_ModuleInit()
*
* Description : Initialize File System File Management module.
*
* Argument(s) : file_cnt    Number of files to allocate.
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

void  FSFile_ModuleInit (FS_QTY   file_cnt,
                         FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


#if (FS_CFG_TRIAL_EN == DEF_ENABLED)                            /* Trial limitation: max 1 file.                        */
    if (file_cnt > FS_TRIAL_MAX_FILE_CNT) {
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif

    FSFile_FileCntMax = 0u;
    FSFile_Cnt        = 0u;

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)                        /* -------------- PERFORM FS/OS FILE INIT ------------- */
    FS_OS_FileInit(file_cnt, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#endif



                                                                /* ------------------ INIT FILE POOL ------------------ */
    Mem_PoolCreate(&FSFile_Pool,
                    DEF_NULL,
                    0,
                    file_cnt,
                    sizeof(FS_FILE),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
        *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSFile_ModuleInit(): Could not alloc mem for files: %d octets required.\r\n", octets_reqd));
        return;
    }

    FSFile_Tbl = (FS_FILE **)Mem_HeapAlloc(file_cnt * sizeof(FS_FILE *),
                                           sizeof(CPU_ALIGN),
                                          &octets_reqd,
                                          &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSFile_ModuleInit(): Could not alloc mem for files: %d octets required.\r\n", octets_reqd));
        return;
    }

    Mem_Clr(FSFile_Tbl, file_cnt * sizeof(FS_FILE *));

    FSFile_FileCntMax = file_cnt;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSFile_ModeParse()
*
* Description : Parse mode string.
*
* Argument(s) : str_mode        Access mode of the file.
*               --------        Argument validated by caller.
*
*               str_len         Length of the mode string.
*
* Return(s)   : Mode identifier, if mode string invalid.
*               0, otherwise.
*
* Note(s)     : (1) See 'FSFile_Open()  Note #2' for a description of mode string interpretation.
*********************************************************************************************************
*/
FS_FLAGS  FSFile_ModeParse (CPU_CHAR    *str_mode,
                            CPU_SIZE_T   str_len)
{
    FS_FLAGS     mode;
    CPU_BOOLEAN  b_present;
    CPU_BOOLEAN  plus_present;


                                                                /* -------------------- PARSE MODE -------------------- */
    switch (*str_mode) {                                        /* Interpret first letter: rd, wr, append.              */
        case ASCII_CHAR_LATIN_LOWER_R:
             mode = FS_FILE_ACCESS_MODE_RD;
             break;

        case ASCII_CHAR_LATIN_LOWER_W:
             mode = FS_FILE_ACCESS_MODE_WR | FS_FILE_ACCESS_MODE_TRUNCATE | FS_FILE_ACCESS_MODE_CREATE;
             break;

        case ASCII_CHAR_LATIN_LOWER_A:
             mode = FS_FILE_ACCESS_MODE_WR | FS_FILE_ACCESS_MODE_CREATE   | FS_FILE_ACCESS_MODE_APPEND;
             break;

        default:
             return (FS_FILE_ACCESS_MODE_NONE);
    }
    str_mode++;
    str_len--;


                                                                /* Interpret remaining str: "", "+", "b", "b+", "+b".   */
    b_present    = DEF_NO;
    plus_present = DEF_NO;

    while (str_len > 0u) {
        if (*str_mode == (CPU_CHAR)ASCII_CHAR_PLUS_SIGN) {

            if (plus_present == DEF_YES) {
                mode  = FS_FILE_ACCESS_MODE_NONE;               /* Invalid mode.                                        */
                return (mode);
            }
            mode |= FS_FILE_ACCESS_MODE_RD | FS_FILE_ACCESS_MODE_WR;
            plus_present = DEF_YES;

        } else if (*str_mode == (CPU_CHAR)ASCII_CHAR_LATIN_LOWER_B) {

            if (b_present == DEF_YES) {
                mode  = FS_FILE_ACCESS_MODE_NONE;               /* Invalid mode.                                        */
                return (mode);
            }
            b_present = DEF_YES;

        } else {

            mode  = FS_FILE_ACCESS_MODE_NONE;                   /* Invalid mode.                                        */
            return(mode);

        }
        str_mode++;
        str_len--;
    }

    return (mode);
}

/*
*********************************************************************************************************
*********************************************************************************************************
*                                             LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            FSFile_BufEmpty()
*
* Description : Empty a file's buffer.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by 'FSFile_Rd()'.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE             File buffer flushed successfully.
*
*                                                       ----------- RETURNED BY FSSys_FileWr() ----------
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) If valid read data exists in the file buffer, the underlying file position might not
*                   correctly reflect the apparent file position to the application, since the maximum
*                   file position reflected in the buffered data may exceed the location last read by
*                   the application.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
static  void  FSFile_BufEmpty (FS_FILE  *p_file,
                               FS_ERR   *p_err)
{
    FS_FILE_SIZE  pos_offset;


    *p_err = FS_ERR_NONE;

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_WR) {
       (void)FSSys_FileWr(p_file,
                          p_file->BufPtr,
                          p_file->BufMaxPos,
                          p_err);

        p_file->BufStart  = 0u;
        p_file->BufMaxPos = 0u;
        p_file->BufStatus = FS_FILE_BUF_STATUS_EMPTY;

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr(p_file->BufPtr, p_file->BufSize);               /* Clr buf.                                             */
#endif
    }
#endif

    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_RD) {
                                                                /* Calc pos offset (see Note #1) ...                    */
        pos_offset = (FS_FILE_SIZE)p_file->BufMaxPos;
        if (pos_offset != 0u) {
            FSSys_FilePosSet(p_file, p_file->Pos, p_err);       /* ... correct file pos.                                */
        }

        p_file->BufStart  = 0u;
        p_file->BufMaxPos = 0u;
        p_file->BufStatus = FS_FILE_BUF_STATUS_EMPTY;

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr(p_file->BufPtr, p_file->BufSize);               /* Clr buf.                                             */
#endif
    }

    if (*p_err != FS_ERR_NONE) {
        p_file->FlagErr = DEF_YES;
    }
}
#endif


/*
*********************************************************************************************************
*                                             FSFile_BufRd()
*
* Description : Read from a file (through file buffer).
*
* Argument(s) : p_file      Pointer to a file.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to buffer that will hold data read by this file.
*               ----------  Argument validated by caller.
*
*               size        Number of octets to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE              File read successfully.
*
*                                                        ---------- RETURNED BY FSSys_FileRd() ----------
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_ENTRY_CORRUPT     File system entry corrupt.
*
* Return(s)   : Number of octets read.
*
* Note(s)     : (1) (a) Data is first copied from the buffer, up to the length of the request OR the
*                       amount stored in the buffer (whichever is less).
*
*                   (b) (1) Any large remaining read (longer than the file buffer) is satisfied by
*                           reading directly into the destination buffer.
*
*                       (2) Otherwise, the buffer is then filled (if possible) and data copied into the
*                           user buffer, up to the length of the remaining request OR the amount stored
*                           in the buffer (whichever is less).
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
static  CPU_SIZE_T  FSFile_BufRd (FS_FILE     *p_file,
                                  void        *p_dest,
                                  CPU_SIZE_T   size,
                                  FS_ERR      *p_err)
{
    CPU_SIZE_T   buf_pos;
    CPU_SIZE_T   buf_rem;
    CPU_SIZE_T   size_rd;
    CPU_SIZE_T   size_rem;
    CPU_INT08U  *p_src;


                                                                /* Init *p_err                                          */
   *p_err = FS_ERR_BUF_NONE_AVAIL;

                                                                /* ---------- RD FROM FILE BUF INTO USER BUF ---------- */
    size_rem = size;

    if (p_file->BufStatus == FS_FILE_BUF_STATUS_NONEMPTY_RD) {  /* See Note #1a.                                        */
        buf_pos      = (CPU_SIZE_T)(p_file->Pos - p_file->BufStart);

        buf_rem      =  p_file->BufMaxPos - buf_pos;            /* Calc octets rem in buf ...                           */
        size_rd      =  DEF_MIN(buf_rem, size_rem);             /*                        ... calc size rd from buf.    */
        p_src        = (CPU_INT08U *)p_file->BufPtr + buf_pos;
        Mem_Copy(p_dest, (void *)p_src, size_rd);               /* Copy from file buf to user buf.                      */

        p_file->Pos +=  size_rd;                                /* Adj file pos ...                                     */
        size_rem    -=  size_rd;                                /*              ... adj rd rem ...                      */
        p_dest       = (CPU_INT08U *)p_dest + size_rd;          /*                             ... adj dest buf.        */

        if (buf_rem == size_rd) {                               /* All buf'd data rd ...                                */
            p_file->BufStart  = 0u;
            p_file->BufMaxPos = 0u;
            p_file->BufStatus = FS_FILE_BUF_STATUS_EMPTY;       /* ... buf is now empty.                                */

        } else {                                                /* More data in buf than rem'd ...                      */
            p_file->BufStatus = FS_FILE_BUF_STATUS_NONEMPTY_RD; /* ... buf is still not empty.                          */
        }

       *p_err = FS_ERR_NONE;
    }




    if (p_file->BufStatus == FS_FILE_BUF_STATUS_EMPTY) {
        if (size_rem >= p_file->BufSize) {                      /* ------------ RD FROM FILE INTO USER BUF ------------ */
            size_rd = FSSys_FileRd(p_file,                      /* Rd into user buf (see Note #1b1).                    */
                                   p_dest,
                                   size_rem,
                                   p_err);
            p_file->Pos += size_rd;                             /* Adj file pos ...                                     */
            size_rem    -= size_rd;                             /*              ... adj rd rem.                         */




        } else {                                                /* ------------ RD FROM FILE INTO FILE BUF ------------ */
            size_rd = FSSys_FileRd(p_file,                      /* Rd full file buf (see Note #1b2).                    */
                                   p_file->BufPtr,
                                   p_file->BufSize,
                                   p_err);

            if (size_rd != 0u) {
                buf_rem          = size_rd;                     /* Calc octets rem in buf ...                           */
                size_rd          = DEF_MIN(size_rd, size_rem);  /*                        ... calc size rd from buf.    */
                Mem_Copy(p_dest, p_file->BufPtr, size_rd);      /* Copy data to user buf.                               */

                p_file->BufStart = p_file->Pos;                 /* Save file pos at buf start.                          */
                p_file->Pos     += size_rd;                     /* Adj file pos ...                                     */
                size_rem        -= size_rd;                     /*              ... adj rd rem.                         */

                if (buf_rem  == size_rd) {                      /* All buf'd data rd ...                                */
                    p_file->BufStart   =  0u;
                    p_file->BufMaxPos  =  0u;
                    p_file->BufStatus  =  FS_FILE_BUF_STATUS_EMPTY;               /* ... buf is still empty.            */

                } else {                                        /* More data rd into buf than rem'd ...                 */
                                                                /* ... buf is no longer empty.                          */
                    p_file->BufMaxPos  =  buf_rem;
                    p_file->BufStatus  =  FS_FILE_BUF_STATUS_NONEMPTY_RD;
                }
            }
        }
    }



                                                                /* ------------------- ASSIGN & RTN ------------------- */
    if (*p_err == FS_ERR_NONE) {
         if (size_rem > 0u) {                                   /* If less data rd than rem ...                         */
            p_file->FlagEOF = DEF_YES;                          /* ... file at EOF.                                     */

         } else {                                               /* If all  data rd that rem'd ...                       */
            p_file->FlagEOF = DEF_NO;                           /* ... file NOT at EOF.                                 */
           *p_err = FS_ERR_NONE;
         }

    } else {
        p_file->FlagEOF = DEF_NO;
        p_file->FlagErr = DEF_YES;
    }

    return (size - size_rem);
}
#endif



/*
*********************************************************************************************************
*                                             FSFile_BufWr()
*
* Description : Write to a file (through file buffer).
*
* Argument(s) : p_file      Pointer to a file.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to buffer that contains data to write in this file.
*               ----------  Argument validated by caller.
*
*               size        Number of octets to write.
*
*               p_err       Pointer to variable that will receive the return error code from this
*                           function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE             File written successfully.
*
*                                                       ----------- RETURNED BY FSSys_FileWr() ----------
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN  == DEF_DISABLED)
static  CPU_SIZE_T  FSFile_BufWr (FS_FILE     *p_file,
                                  void        *p_src,
                                  CPU_SIZE_T   size,
                                  FS_ERR      *p_err)
{
    CPU_INT08U    *p_buf;
    CPU_SIZE_T     buf_rem;
    CPU_SIZE_T     size_wr;
    CPU_SIZE_T     size_rem;


                                                                /* ----------------- WR TO END OF BUF ----------------- */
    size_rem = size;
    buf_rem  = p_file->BufSize - p_file->BufMaxPos;             /* Calc octets empty in buf ...                         */
    size_wr  = DEF_MIN(buf_rem, size_rem);                      /*                          ... calc size wr to buf.    */
    if (size_wr > 0u) {
        if (p_file->BufStatus == FS_FILE_BUF_STATUS_EMPTY) {
            p_file->BufStart = p_file->Pos;                 /* Save file pos at buf start.                          */
        }

        p_buf    = (CPU_INT08U *)p_file->BufPtr + p_file->BufMaxPos;
        Mem_Copy((void *)p_buf, p_src, size_wr);                /* Copy from user buf to file buf.                      */


        p_file->Pos       +=  size_wr;                          /* Adj file pos ...                                     */
        size_rem          -=  size_wr;                          /*              ... adj wr rem ...                      */
        p_src              = (CPU_INT08U *)p_src + size_wr;     /*                             ... adj src buf.         */

        p_file->BufMaxPos +=  size_wr;                          /* Save max pos wr in buf.                              */
        p_file->BufStatus  =  FS_FILE_BUF_STATUS_NONEMPTY_WR;
       *p_err = FS_ERR_NONE;
    }



    if (p_file->BufMaxPos == p_file->BufSize) {                 /* ------------------- DUMP TO FILE ------------------- */
        FSSys_FileWr(p_file,                                    /* Wr buf to file ...                                   */
                     p_file->BufPtr,
                     p_file->BufSize,
                     p_err);
        if (*p_err != FS_ERR_NONE) {
            p_file->FlagErr = DEF_YES;
            return (size - size_rem);
        }

        p_file->BufStart  = 0u;                                 /* ... buf now empty.                                   */
        p_file->BufMaxPos = 0u;
        p_file->BufStatus = FS_FILE_BUF_STATUS_EMPTY;
       *p_err = FS_ERR_NONE;



        if (size_rem >= p_file->BufSize) {                      /* ------------------ HANDLE LARGE WR ----------------- */
            size_wr = FSSys_FileWr(p_file,                      /* Wr data to file.                                     */
                                   p_src,
                                   size_rem,
                                   p_err);
            p_file->Pos    +=  size_wr;                         /* Adj file pos ...                                     */
            size_rem       -=  size_wr;                         /*              ... adj wr rem.                         */

            p_file->FlagErr = (*p_err != FS_ERR_NONE) ? DEF_YES : DEF_NO;

            if (*p_err != FS_ERR_NONE) {
                p_file->FlagErr = DEF_YES;
            }



        } else {
            if (size_rem > 0u) {                                /* -------------- WR REM TO START OF BUF -------------- */
                size_wr            = size_rem;
                Mem_Copy(p_file->BufPtr, p_src, size_rem);      /* Copy from user buf to file buf.                      */

                p_file->BufStart   = p_file->Pos;               /* Save file pos at buf start.                          */

                p_file->Pos       += size_wr;                   /* Adj file pos ...                                     */
                size_rem          -= size_wr;                   /*              ... adj wr rem.                         */

                p_file->BufMaxPos  = size_wr;                   /* Save max wr pos in buf.                              */
                p_file->BufStatus  = FS_FILE_BUF_STATUS_NONEMPTY_WR;
               *p_err = FS_ERR_NONE;
            }
        }
    }



                                                                /* ------------------- ASSIGN & RTN ------------------- */
    if (p_file->Size < p_file->Pos) {                           /* Chng file size.                                      */
        p_file->Size = p_file->Pos;
    }
    return (size - size_rem);
}
#endif
#endif


/*
*********************************************************************************************************
*                                       FSFile_AcquireLockChk()
*
* Description : Acquire file reference & lock.
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE             File reference & lock acquired.
*                               FS_ERR_DEV_CHNGD        Device has changed.
*                               FS_ERR_FILE_NOT_OPEN    File NOT open.
*
* Return(s)   : Pointer to a file, if found.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_FILE  *FSFile_AcquireLockChk (FS_FILE      *p_file,
                                         FS_ERR       *p_err)
{
    CPU_BOOLEAN  lock_success;


                                                                /* ----------------- ACQUIRE FILE LOCK ---------------- */
    p_file = FSFile_Acquire(p_file);                            /* Acquire file ref.                                    */

    if (p_file == (FS_FILE *)0) {                               /* Rtn err if file not found.                           */
       *p_err = FS_ERR_FILE_NOT_OPEN;
        return ((FS_FILE *)0);
    }

    lock_success = FSFile_Lock(p_file);
    if (lock_success != DEF_YES) {
       *p_err = FS_ERR_OS_LOCK;
        return ((FS_FILE *)0);
    }


                                                                /* ------------------- VALIDATE FILE ------------------ */
    switch (p_file->State) {
        case FS_FILE_STATE_OPEN:
            *p_err = FS_ERR_NONE;
             return (p_file);


        case FS_FILE_STATE_CLOSED:                              /* Rtn NULL if file not open.                           */
        case FS_FILE_STATE_CLOSING:
        case FS_FILE_STATE_OPENING:
        default:
            *p_err = FS_ERR_FILE_NOT_OPEN;
             FSFile_ReleaseUnlock(p_file);
             return ((FS_FILE *)0);
    }
}


/*
*********************************************************************************************************
*                                          FSFile_Acquire()
*
* Description : Acquire file reference.
*
* Argument(s) : p_file      Pointer to file.
*
* Return(s)   : Pointer to a file, if available.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_FILE  *FSFile_Acquire (FS_FILE  *p_file)
{
    FS_ERR  err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return ((FS_FILE *)0);
    }



                                                                /* -------------------- INC REF CNT ------------------- */
    if (p_file->RefCnt == 0u) {                                 /* Rtn NULL if file not ref'd.                          */
        FS_OS_Unlock();
        return ((FS_FILE *)0);
    }

    p_file->RefCnt++;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (p_file);
}


/*
*********************************************************************************************************
*                                       FSFile_ReleaseUnlock()
*
* Description : Release file reference & lock.
*
* Argument(s) : p_file      Pointer to file.
*               ------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSFile_ReleaseUnlock (FS_FILE  *p_file)
{
    FSFile_Unlock(p_file);
    FSFile_Release(p_file);
}


/*
*********************************************************************************************************
*                                           FSFile_Release()
*
* Description : Release file reference.
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) A volume reference might have been acquired in 'FSFile_NameParseChk()'; the volume
*                   reference would NOT have been acquired if the named volume did not exist or existed
*                   but was not mounted.
*********************************************************************************************************
*/

static  void  FSFile_Release (FS_FILE  *p_file)
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
    ref_cnt = p_file->RefCnt;
    if (ref_cnt == 1u) {
        p_vol = p_file->VolPtr;
        FSFile_ObjFree(p_file);
    } else if (ref_cnt > 0u) {
        p_file->RefCnt = ref_cnt - 1u;
    } else {
        FS_TRACE_DBG(("FSFile_Release(): Release cnt dec'd to zero.\r\n"));
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
*                                            FSFile_Lock()
*
* Description : Acquire file lock.
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSFile_Lock (FS_FILE  *p_file)
{
    CPU_BOOLEAN  locked;


#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
    locked = FSFile_LockGetHandler(p_file);

    if (locked == DEF_NO) {
        return (DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    locked = FSVol_Lock(p_file->VolPtr);

    return (locked);
}


/*
*********************************************************************************************************
*                                           FSFile_Unlock()
*
* Description : Release file lock.
*
* Argument(s) : p_file      Pointer to file.
*               ------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSFile_Unlock (FS_FILE  *p_file)
{
                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_Unlock(p_file->VolPtr);

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
    (void)FSFile_LockSetHandler(p_file);
#endif
}


/*
*********************************************************************************************************
*                                     FSFile_LockAcceptHandler()
*
* Description : Acquire file lock (if avail).
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
* Return(s)   : DEF_YES, if file lock     acquired.
*               DEF_NO,  if file lock NOT acquired.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
static  CPU_BOOLEAN  FSFile_LockAcceptHandler (FS_FILE  *p_file)
{
    FS_ERR       err;
    CPU_BOOLEAN  locked;


    FS_OS_FileAccept(p_file->ID, &err);                         /* Acquire file lock.                                   */
    locked = (err == FS_ERR_NONE) ? DEF_YES : DEF_NO;

    return (locked);
}
#endif


/*
*********************************************************************************************************
*                                       FSFile_LockGetHandler()
*
* Description : Acquire file lock.
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
* Return(s)   : DEF_YES, if file lock     acquired.
*               DEF_NO,  if file lock NOT acquired.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
static  CPU_BOOLEAN  FSFile_LockGetHandler (FS_FILE  *p_file)
{
    FS_ERR       err;
    CPU_BOOLEAN  locked;


    FS_OS_FileLock(p_file->ID, &err);                           /* Acquire file lock.                                   */
    locked = (err == FS_ERR_NONE) ? DEF_YES : DEF_NO;

    return (locked);
}
#endif


/*
*********************************************************************************************************
*                                       FSFile_LockSetHandler()
*
* Description : Release file lock.
*
* Argument(s) : p_file      Pointer to file.
*               ----------  Argument validated by caller.
*
* Return(s)   : DEF_YES, if file lock     released.
*               DEF_NO,  if file lock NOT released.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
static  CPU_BOOLEAN  FSFile_LockSetHandler (FS_FILE  *p_file)
{
    CPU_BOOLEAN  unlocked;


    unlocked = FS_OS_FileUnlock(p_file->ID);

    return (unlocked);
}
#endif


/*
*********************************************************************************************************
*                                           FSFile_ObjClr()
*
* Description : Clear a file object.
*
* Argument(s) : p_file      Pointer to file.
*               ------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSFile_ObjClr (FS_FILE  *p_file)
{
    p_file->ID           =  0u;
    p_file->State        =  FS_FILE_STATE_CLOSED;
    p_file->RefCnt       =  0u;
    p_file->RefreshCnt   =  0u;

    p_file->AccessMode   =  FS_FILE_ACCESS_MODE_NONE;
    p_file->FlagErr      =  DEF_NO;
    p_file->FlagEOF      =  DEF_NO;
    p_file->IO_State     =  FS_FILE_IO_STATE_NONE;
    p_file->Size         =  0u;
    p_file->Pos          =  0u;

#if (FS_CFG_FILE_BUF_EN  == DEF_ENABLED)
    p_file->BufStart     =  0u;
    p_file->BufMaxPos    =  0u;
    p_file->BufSize      =  0u;
    p_file->BufMode      =  FS_FILE_BUF_MODE_NONE;
    p_file->BufStatus    =  FS_FILE_BUF_STATUS_NONE;
    p_file->BufPtr       = (void *)0;
    p_file->BufSecSize   =  0u;
#endif

    p_file->VolPtr       = (FS_VOL *)0;
    p_file->DataPtr      = (void   *)0;

#if (FS_CFG_CTR_STAT_EN  == DEF_ENABLED)
    p_file->StatRdCtr    =  0u;
    p_file->StatWrCtr    =  0u;
#endif
}


/*
*********************************************************************************************************
*                                          FSFile_ObjFree()
*
* Description : Free a file object.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSFile_ObjFree (FS_FILE  *p_file)
{
    LIB_ERR  pool_err;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate file ptr.                                   */
        FS_TRACE_INFO(("FSFile_ObjFree(): File pointer is invalid.\r\n"));
        return;
    }
#endif

    FSFile_Tbl[p_file->ID] = DEF_NULL;

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    FSFile_ObjClr(p_file);
#endif

    Mem_PoolBlkFree(        &FSFile_Pool,
                    (void *) p_file,
                            &pool_err);

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_TRACE_INFO(("FSFile_ObjFree(): Could not free file to pool.\r\n"));
        return;
    }
#endif

    FSFile_Cnt--;
}


/*
*********************************************************************************************************
*                                           FSFile_ObjGet()
*
* Description : Allocate a file object.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_NONE                     File object successfully returned.
*                               FS_ERR_NULL_PTR                 FSFile_Pool is a NULL pointer.
*                               FS_ERR_POOL_EMPTY               NO blocks available in pool.
*                               FS_ERR_POOL_INVALID_BLK_IX      Invalid pool block index.
*                               FS_ERR_POOL_INVALID_BLK_ADDR    Block not found in used pool pointers.
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_FILE  *FSFile_ObjGet (FS_ERR *p_err)
{
    FS_FILE  *p_file;
    LIB_ERR   pool_err;
    FS_QTY    file_ix;



    for (file_ix = 0u; file_ix < FSFile_FileCntMax; file_ix++) {
        if (FSFile_Tbl[file_ix] == DEF_NULL) {
            break;
        }
    }
    if (file_ix >= FSFile_FileCntMax) {
        FS_TRACE_INFO(("FSFile_ObjGet(): Could not alloc file from pool. Opened file limit reached.\r\n"));
       *p_err = FS_ERR_MEM_ALLOC;
        return ((FS_FILE *)0);
    }

    p_file = (FS_FILE *)Mem_PoolBlkGet(&FSFile_Pool,
                                        sizeof(FS_FILE),
                                       &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_TRACE_INFO(("FSFile_ObjGet(): Could not alloc file from pool. Opened file limit reached.\r\n"));
       *p_err = FS_ERR_MEM_ALLOC;
        return ((FS_FILE *)0);
    }

    FSFile_Tbl[file_ix] = p_file;

    FSFile_Cnt++;

    FSFile_ObjClr(p_file);

    p_file->ID = file_ix;

    return (p_file);
}


/*
*********************************************************************************************************
*                                        FSFile_NameParseChk()
*
* Description : Parse full file name & get volume pointer & pointer to entry name.
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
* Note(s)     : (1) If the volume name could not be parsed, the volume does not exist or no file name
*                   is specified, then NULL pointers are returned for BOTH the file name & the volume.
*                   Otherwise, both the file name & volume should be valid pointers.
*
*               (2) The volume reference is released in 'FSFile_Release()' after the final reference to
*                   the file has been released.
*********************************************************************************************************
*/

static  CPU_CHAR  *FSFile_NameParseChk (CPU_CHAR   *name_full,
                                        FS_VOL    **pp_vol,
                                        FS_ERR     *p_err)
{
    CPU_SIZE_T   name_entry_len;
    CPU_SIZE_T   file_name_len;
    CPU_CHAR    *name_entry;
    CPU_CHAR    *file_name;
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
        *p_err =  FS_ERR_NAME_INVALID;
         return ((CPU_CHAR *)0);
    }
    name_entry++;                                               /* Ignore  init path sep char.                          */

    name_entry_len = Str_Len_N(name_entry, FS_CFG_MAX_PATH_NAME_LEN + 1u);
    if (name_entry_len > FS_CFG_MAX_PATH_NAME_LEN) {            /* Rtn err if path name is too long.                    */
       *p_err =  FS_ERR_NAME_PATH_TOO_LONG;
        return ((CPU_CHAR *)0);
    }

                                                                /* Parse name to evaluate length of the base name       */
    file_name     = Str_Char_Last_N(name_entry, FS_CFG_MAX_PATH_NAME_LEN,      (CPU_CHAR)ASCII_CHAR_REVERSE_SOLIDUS);
    file_name_len = Str_Len_N(      file_name,  FS_CFG_MAX_FILE_NAME_LEN + 1u);
    if (file_name_len == 0u) {
        file_name_len = name_entry_len;
    }

    if (file_name_len > FS_CFG_MAX_FILE_NAME_LEN) {
        *p_err = FS_ERR_NAME_BASE_TOO_LONG;                     /* Rtn err if base name is too long.                    */
        return ((CPU_CHAR *)0);
    }

                                                                /* Acquire ref to vol (see Note #2).                    */
    p_vol_temp = (name_vol_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) ? FSVol_AcquireDflt()
                                                                 : FSVol_Acquire(name_vol_temp, p_err);

    if (p_vol_temp == (FS_VOL *)0) {                            /* Rtn err if vol not found.                            */
       *p_err = FS_ERR_VOL_NOT_OPEN;
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
            *p_err = FS_ERR_VOL_NOT_OPEN;
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);
    }

   *pp_vol = p_vol_temp;
   *p_err  = FS_ERR_NONE;
    return (name_entry);
}
