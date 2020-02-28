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
*                                     FILE SYSTEM FAT MANAGEMENT
*
*                                             FILE ACCESS
*
* Filename : fs_fat_file.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_FILE_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_dev.h"
#include  "../Source/fs_entry.h"
#include  "../Source/fs_file.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_file.h"
#include  "fs_fat_journal.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
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

static  MEM_POOL  FS_FAT_FileDataPool;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


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
*                                       FS_FAT_FileModuleInit()
*
* Description : Initialize FAT File module.
*
* Argument(s) : file_cnt    Number of files to allocate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE         Module initialized.
*                               FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_FileModuleInit (FS_QTY   file_cnt,
                             FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


    Mem_PoolCreate(&FS_FAT_FileDataPool,
                    DEF_NULL,
                    0,
                    file_cnt,
                    sizeof(FS_FAT_FILE_DATA),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
        *p_err  = FS_ERR_MEM_ALLOC;
         FS_TRACE_INFO(("FS_FAT_FileModuleInit(): Could not alloc mem for FAT file info: %d octets req'd.\r\n", octets_reqd));
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       SYSTEM DRIVER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FS_FAT_FileClose()
*
* Description : Close a file.
*
* Argument(s) : p_file      Pointer to a file.
*               -----       Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE        File closed.
*                               FS_ERR_NULL_PTR    Argument 'p_buf' is a NULL pointer.
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to free the file data back to the file data pool.
*********************************************************************************************************
*/

void  FS_FAT_FileClose  (FS_FILE  *p_file,
                         FS_ERR   *p_err)
{
    FS_FAT_FILE_DATA  *p_fat_file_data;
    LIB_ERR            pool_err;
    FS_ERR             err;
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_BUF            *p_buf;
#endif


    p_fat_file_data = (FS_FAT_FILE_DATA *)p_file->DataPtr;

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                         /* ------------------ UPDATE DIR SEC ------------------ */
    if (DEF_BIT_IS_SET(p_fat_file_data->Mode, FS_FILE_ACCESS_MODE_CACHED | FS_FILE_ACCESS_MODE_WR) == DEF_YES) {
        if (p_fat_file_data->UpdateReqd == DEF_YES) {
            p_buf = FSBuf_Get(p_file->VolPtr);
            if (p_buf == (FS_BUF *)0) {
               *p_err = FS_ERR_NULL_PTR;
                return;
            }

            FS_FAT_LowEntryUpdate(p_file->VolPtr,
                                  p_buf,
                                  p_fat_file_data,
                                  DEF_YES,
                                  p_err);
            FSBuf_Flush(p_buf, p_err);
            FSBuf_Free(p_buf);
        }
    }
#endif



                                                                /* ------------------ FREE FILE DATA ------------------ */
    FS_OS_Lock(&err);                                           /* Acquire FS lock (see Note #1).                       */
    if (err != FS_ERR_NONE) {
       *p_err = err;
        return;
    }

    Mem_PoolBlkFree(        &FS_FAT_FileDataPool,
                    (void *) p_fat_file_data,
                            &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_OS_Unlock();
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    FS_OS_Unlock();

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FS_FAT_FileOpen()
*
* Description : Open a file.
*
* Argument(s) : p_file          Pointer to a file.
*               ------          Argument validated by caller.
*
*               name_file       Name of the file.
*               ---------       Pointer validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      File opened successfully.
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_DEV_FULL                  Device is full (space could not be allocated).
*                                   FS_ERR_DIR_FULL                  Directory is full (space could not be allocated).
*                                   FS_ERR_ENTRY_CORRUPT             File system entry is corrupt.
*                                   FS_ERR_ENTRY_EXISTS              File system entry exists.
*                                   FS_ERR_ENTRY_NOT_FILE            File system entry NOT a file.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to get the file data from the file data pool.
*
*               (2) (a) If long file names are NOT used :
*
*                       (1) The file name      is limited to   8 characters (excluding period & extension).
*
*                       (2) The file extension is limited to   3 characters.
*
*                       (3) A directory name   is limited to   8 characters.
*
*                       (4) The full path      is limited to  80 characters (including a trailing NULL character).
*
*                       (5) The file name may contain up to ONE period, which separates the name from the
*                           extensions.  This period MAY NOT be the first character of the file name.
*
*                   (b) If long file names ARE used :
*
*                       (1) The file name      is limited to 255 characters (including period & extension).
*
*                       (2) A directory name   is limited to 255 characters.
*
*                       (3) The full path      is limited to 260 characters (including a trailing NULL character).
*
*                   (c) If the file name passed by 'pname_file' violates any of these rules, then NO
*                       modification will occur to the disk information & an error code will be returned.
*
*                   (d) Trailing periods are ignored.
*
*                   (e) Leading & trailing spaces are ignored.
*
*               (3) According to [Ref 1], "a zero-length file--a file that has no data allocated to it--
*                   has a first cluster number of 0 placed in its directory entry".
*
*               (4) If journaling is enabled & journaling started, logs may be written to the journal ...
*
*                   (a) ... (from 'FS_FAT_LowEntryCreate()') to ensure that either the new file is fully
*                       created or no changes occur to the file system, if the file does not exist & the
*                       FS_FAT_MODE_CREATE flag is set.
*
*                   (b) ... (from 'FS_FAT_LowEntryTruncate()') to ensure that either the existing file is
*                       fully truncated or no changes occur to the file system, if the file does exist &
*                       the FS_FAT_MODE_TRUNCATE flag is set.
*
*                   (c) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*********************************************************************************************************
*/

void  FS_FAT_FileOpen (FS_FILE   *p_file,
                       CPU_CHAR  *name_file,
                       FS_ERR    *p_err)
{
    FS_FAT_FILE_DATA  *p_file_data;
    FS_ERR             err;
    LIB_ERR            pool_err;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_BUF            *p_buf;
#endif


                                                                /* ------------------ ALLOC FILE DATA ----------------- */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock (see Note #1).                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_file_data = (FS_FAT_FILE_DATA *)Mem_PoolBlkGet(&FS_FAT_FileDataPool,
                                                      sizeof(FS_FAT_FILE_DATA),
                                                     &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    FS_OS_Unlock();


    if (p_file_data == (FS_FAT_FILE_DATA *)0) {
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }



                                                                /* --------------------- OPEN FILE -------------------- */
    p_file->DataPtr = (void *)p_file_data;
    FS_FAT_LowEntryFind( p_file->VolPtr,
                         p_file_data,
                         name_file,
                        (p_file->AccessMode | FS_FAT_MODE_FILE),
                         p_err);

    if (*p_err != FS_ERR_NONE) {
         FS_FAT_FileClose(p_file, &err);
         return;
    }

#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_buf = FSBuf_Get(p_file->VolPtr);                          /* Get buf.                                             */
    if (p_buf == DEF_NULL) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FS_FAT_JournalClrReset(p_file->VolPtr, p_buf, p_err);       /* Clr journal (see Note #4c).                          */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }
                                                                /* Flush & free buf.                                    */
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
#endif
}


/*
*********************************************************************************************************
*                                         FS_FAT_FilePosSet()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               pos_new     File position.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File position set successful.
*                               FS_ERR_BUF_NONE_AVAIL   No buffer available.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file's 'occupied' cluster contains or will contain data.  Though additional
*                   clusters may belong to the file (without valid contents), only the occupied cluster
*                   count can be pre-computed & guaranteed to be linked to the cluster chain.
*
*                   (a) For example, a 16384-byte file on a volume with a 4096-byte cluster has 4 clusters
*                       with data & zero or more unoccupied clusters.  To set the file position at byte
*                       16384 or beyond, the position must first be set at the 0th byte of the 5th cluster
*                       of the file :
*
*                       (1) Follow chain start->clus1->clus2->clus3; then
*                       (2) Get or allocate one or more additional cluster(s).
*
*               (2) If journaling is enabled & journaling started, logs may be written to the journal to
*                   ensure that new clusters, after allocation are linked to the file's cluster chain.
*
*                   (a) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*
*               (3) When an error code force the function to exit, FS_FAT_JournalExitFileWr() must be
*                   called. FS_FAT_JournalExitFileWr() can return an error code, however the first
*                   error code to happen has priority over the second.
*
*               (4) Position can only be set in the existing portion of a file. If the position is set
*                   after the file size, the code must call FS_FAT_FileWr() instead to correctly
*                   allocate clusters and fill data region with '0'.
*********************************************************************************************************
*/

void  FS_FAT_FilePosSet (FS_FILE       *p_file,
                         FS_FILE_SIZE   pos_new,
                         FS_ERR        *p_err)
{
    FS_FAT_CLUS_NBR    clus;
    FS_FAT_CLUS_NBR    clus_new;
    FS_FAT_CLUS_NBR    clus_cnt_cur;
    FS_FAT_CLUS_NBR    clus_cnt_file;
    FS_FAT_CLUS_NBR    clus_cnt_new;
    FS_FAT_SEC_NBR     clus_pos_new_sec;
    FS_FAT_SEC_NBR     sec_new;
    FS_SEC_SIZE        sec_pos_new;
    FS_BUF            *p_buf;
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_fat_file_data;


    p_fat_file_data = (FS_FAT_FILE_DATA *)(p_file->DataPtr);
    p_fat_data      = (FS_FAT_DATA      *)(p_file->VolPtr->DataPtr);

    if ((p_fat_file_data->FileFirstClus == 0u) &&
        (pos_new                        == 0u)) {
       *p_err = FS_ERR_NONE;
        return;
    }

    p_buf = FSBuf_Get(p_file->VolPtr);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }


                                                                /* ------------------ CALC CLUS CNTS ------------------ */
                                                                /* Calc occupied clus cnts (see Note #1).               */
    clus_cnt_new  = (pos_new                   > 0u) ? (FS_UTIL_DIV_PWR2(pos_new                   - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);
    clus_cnt_cur  = (p_fat_file_data->FilePos  > 0u) ? (FS_UTIL_DIV_PWR2(p_fat_file_data->FilePos  - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);
    clus_cnt_file = (p_fat_file_data->FileSize > 0u) ? (FS_UTIL_DIV_PWR2(p_fat_file_data->FileSize - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);

    if (((pos_new & (p_fat_data->ClusSize_octet - 1u)) == 0u) &&    /* If new pos is at last byte of any clus ...       */
        ( pos_new                                      != 0u)) {
        sec_pos_new      = p_fat_data->SecSize;                     /* ... set sec pos past last byte of sec  ...       */
        clus_pos_new_sec = p_fat_data->ClusSize_sec - 1u;           /* ... &   clus at last sec of clus.                */

    } else {
        sec_pos_new      = pos_new & (p_fat_data->SecSize - 1u);
        clus_pos_new_sec = FS_UTIL_DIV_PWR2(pos_new & (p_fat_data->ClusSize_octet - 1u), p_fat_data->SecSizeLog2);
    }



                                                                /* ------------------ POS IN CUR CLUS ----------------- */
    if (clus_cnt_new == clus_cnt_cur) {
        clus    = FS_FAT_SEC_TO_CLUS(p_fat_data, p_fat_file_data->FileCurSec);
        sec_new = FS_FAT_CLUS_TO_SEC(p_fat_data, clus) + clus_pos_new_sec;



    } else if (clus_cnt_new <= 1u) {                            /* ------------ POS IN FIRST CLUS, NOT CUR ------------ */
        if (p_fat_file_data->FileFirstClus == 0u) {             /* If first sec zero (no data) ... alloc clus.          */
           *p_err = FS_ERR_VOL_INVALID_OP;
            FSBuf_Free(p_buf);
            return;
        }

        clus_new = p_fat_file_data->FileFirstClus;
        sec_new  = FS_FAT_CLUS_TO_SEC(p_fat_data, clus_new) + clus_pos_new_sec;



    } else if (clus_cnt_new > clus_cnt_file) {                  /* --------------- POS BEYOND LAST CLUS --------------- */
       *p_err = FS_ERR_VOL_INVALID_OP;
        FSBuf_Free(p_buf);
        return;


    } else {                                                    /* ----- POS BEFORE LAST CLUS, NOT FIRST, NOT CUR ----- */
                                                                /* Move to last known clus.                             */
        clus = FS_FAT_ClusChainFollow(p_file->VolPtr,
                                      p_buf,
                                      p_fat_file_data->FileFirstClus,
                                     (clus_cnt_new - 1u),
                                      DEF_NULL,
                                      p_err);

        if (*p_err != FS_ERR_NONE) {
             FSBuf_Free(p_buf);
             return;
        }

        sec_new = FS_FAT_CLUS_TO_SEC(p_fat_data, clus) + clus_pos_new_sec;
    }




                                                                /* ----------------- UPDATE FILE INFO ----------------- */
    p_fat_file_data->FilePos       = pos_new;
    p_fat_file_data->FileCurSec    = sec_new;
    p_fat_file_data->FileCurSecPos = sec_pos_new;

    FSBuf_Free(p_buf);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FS_FAT_FileQuery()
*
* Description : Get information about a file.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               p_info      Pointer to structure that will receive the file information.
*               ------      Pointer validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              File information obtained successful.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_FileQuery (FS_FILE        *p_file,
                        FS_ENTRY_INFO  *p_info,
                        FS_ERR         *p_err)
{
    FS_FLAGS           fat_attrib;
    FS_FLAGS           fs_attrib;
    FS_FAT_FILE_DATA  *p_fat_file_data;
    FS_FAT_DATA       *p_fat_data;


    p_fat_data      = (FS_FAT_DATA      *)(p_file->VolPtr->DataPtr);
    p_fat_file_data = (FS_FAT_FILE_DATA *)(p_file->DataPtr);


                                                                /* ------------------- FILE ATTRIB'S ------------------ */
    fat_attrib = p_fat_file_data->Attrib;
    fs_attrib  = 0u;
    DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_RD);
    if (DEF_BIT_IS_CLR(fat_attrib, FS_FAT_DIRENT_ATTR_READ_ONLY) == DEF_YES) {
        DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_WR);             /* NOT read only.                                       */
    }
    if (DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_HIDDEN   ) == DEF_YES) {
        DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_HIDDEN);         /* Hidden.                                              */
    }
    if (DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
        DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_DIR);            /* Directory.                                           */
    }
    p_info->Attrib = fs_attrib;



                                                                /* --------------------- FILE SIZE -------------------- */
    p_info->Size    =  p_fat_file_data->FileSize;               /* File size.                                           */
    p_info->BlkSize =  p_fat_data->ClusSize_octet;              /* Blk size (bytes per clus) & nbr blks (nbr clus).     */
    p_info->BlkCnt  = (p_fat_file_data->FileSize == 0u) ? (FS_UTIL_DIV_PWR2(p_fat_file_data->FileSize - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);



                                                                /* ----------------- FILE DATE & TIME ----------------- */
    FS_FAT_DateTimeParse(&p_info->DateTimeCreate,               /* Creation date/time.                                  */
                          p_fat_file_data->DateCreate,
                          p_fat_file_data->TimeCreate);

    FS_FAT_DateTimeParse(&p_info->DateTimeWr,                   /* Wr       date/time.                                  */
                          p_fat_file_data->DateWr,
                          p_fat_file_data->TimeWr);

    FS_FAT_DateTimeParse(&p_info->DateAccess,                   /* Access   date.                                       */
                          p_fat_file_data->DateAccess,
                          0u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FS_FAT_FileRd()
*
* Description : Read from a file.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ------      Argument validated by caller.
*
*               size        Number of octets to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              File read successful.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_ENTRY_CORRUPT     File system entry corrupt.
*
* Return(s)   : Number of bytes read, if file read successful.
*               0,                    otherwise.
*
* Note(s)     : (1) The 'FileCurSec' member of 'p_fat_file_data' will NOT be updated if the file read
*                   reaches the end of a sector.  In this case, 'FileCurSecPos' will equal the sector
*                   size; such a condition MUST be checked at the beginning of any file read/write so
*                   that the current sector number can be determined.
*********************************************************************************************************
*/

CPU_SIZE_T  FS_FAT_FileRd (FS_FILE     *p_file,
                           void        *p_dest,
                           CPU_SIZE_T   size,
                           FS_ERR      *p_err)
{
    FS_FAT_SEC_NBR     clus_cur_sec_rem;
    FS_FAT_SEC_NBR     sec_cnt_rd;
    FS_FAT_SEC_NBR     sec_cnt_rem;
    FS_FAT_SEC_NBR     sec_cur;
    FS_FAT_SEC_NBR     sec_next;
    FS_FAT_SEC_NBR     sec_cur_pos;
    CPU_SIZE_T         size_rd;
    CPU_SIZE_T         size_rem;
    FS_BUF            *p_buf;
    CPU_INT08U        *p_dest_08;
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_fat_file_data;
    CPU_INT08U        *p_temp_08;


                                                                /* ------------------ PREPARE FOR RD ------------------ */
    p_buf = FSBuf_Get(p_file->VolPtr);                          /* Get rd buf.                                          */
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return (0u);
    }
    p_fat_file_data = (FS_FAT_FILE_DATA *)(p_file->DataPtr);
    p_fat_data      = (FS_FAT_DATA      *)(p_file->VolPtr->DataPtr);
    p_dest_08       = (CPU_INT08U       *)(p_dest);

                                                                /* If first sec zero (no data) or file pos past EOF ... */
    if ((p_fat_file_data->FileFirstClus == 0u)                        ||
        (p_fat_file_data->FilePos       >= p_fat_file_data->FileSize)) {
        FSBuf_Free(p_buf);
       *p_err           = FS_ERR_NONE;
        return (0u);
    }
                                                                /* Truncate file rd to file rem.                        */
    if (size > (p_fat_file_data->FileSize - p_fat_file_data->FilePos)) {
        size = (p_fat_file_data->FileSize - p_fat_file_data->FilePos);
    }

    if (size == 0u) {
        FSBuf_Free(p_buf);
       *p_err           = FS_ERR_NONE;
        return (0u);
    }

    size_rem         = size;
    sec_cur          = p_fat_file_data->FileCurSec;
    sec_cur_pos      = p_fat_file_data->FileCurSecPos;
    clus_cur_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, sec_cur);

    if (sec_cur_pos == p_fat_data->SecSize) {                   /* Sec pos at end of sec (so move to start of next).    */
        sec_cur_pos = 0u;
        sec_cur     = FS_FAT_SecNextGet(p_file->VolPtr,
                                        p_buf,
                                        sec_cur,
                                        p_err);

        if (*p_err != FS_ERR_NONE) {
            if ((*p_err == FS_ERR_SYS_CLUS_CHAIN_END) ||
                (*p_err == FS_ERR_SYS_CLUS_INVALID)) {
                 *p_err =  FS_ERR_ENTRY_CORRUPT;
            }
            FSBuf_Free(p_buf);
            return (0u);
        }

        clus_cur_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, sec_cur);
    }



                                                                /* ------------- PARTIAL (INITIAL) SEC RD ------------- */
    if (sec_cur_pos != 0u) {
        FSVol_RdLockedEx(p_file->VolPtr,                        /* Rd full sec.                                         */
                         p_buf->DataPtr,
                         sec_cur,
                         1u,
                         FS_VOL_SEC_TYPE_FILE,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            p_fat_file_data->FileCurSec     = sec_cur;
            p_fat_file_data->FileCurSecPos  = sec_cur_pos;
            p_fat_file_data->FilePos       += size - size_rem;
            return (size - size_rem);
        }

                                                                /* More rd octets that bytes in partial sec.            */
        size_rd = DEF_MIN(size_rem, p_fat_data->SecSize - sec_cur_pos);

        p_temp_08 = (CPU_INT08U *)p_buf->DataPtr + sec_cur_pos;
        Mem_Copy((void     *)p_dest_08,                         /* Copy partial sec data.                               */
                 (void     *)p_temp_08,
                 (CPU_SIZE_T)size_rd);

        size_rem    -= size_rd;
        p_dest_08   += size_rd;
        sec_cur_pos += size_rd;
        clus_cur_sec_rem--;

        if (size_rem > 0u) {                                    /* If more rd octets, get next sec nbr.                 */
            sec_cur_pos = 0u;
            sec_cur     = FS_FAT_SecNextGet(p_file->VolPtr,
                                            p_buf,
                                            sec_cur,
                                            p_err);

            if (*p_err != FS_ERR_NONE) {
                if ((*p_err == FS_ERR_SYS_CLUS_CHAIN_END) ||
                    (*p_err == FS_ERR_SYS_CLUS_INVALID)) {
                     *p_err =  FS_ERR_ENTRY_CORRUPT;
                }
                FSBuf_Free(p_buf);
                p_fat_file_data->FileCurSec     = sec_cur;
                p_fat_file_data->FileCurSecPos  = sec_cur_pos;
                p_fat_file_data->FilePos       += size - size_rem;
                return (size - size_rem);
            }

            clus_cur_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, sec_cur);
        }
    }

    sec_next = 0u;

    while (size_rem > 0u) {
        sec_cnt_rem = FS_UTIL_DIV_PWR2(size_rem, p_fat_data->SecSizeLog2);
        if (sec_cnt_rem > 0u) {                                 /* ------------------- FULL SEC RDs ------------------- */

            sec_cnt_rd = 0;
            do {                                                /* Cnt the max nbr of continuous sec's.                 */
                sec_cnt_rd += DEF_MIN(sec_cnt_rem - sec_cnt_rd, clus_cur_sec_rem);

                if((sec_cnt_rem - sec_cnt_rd) > 0u) {
                    sec_next = FS_FAT_SecNextGet(p_file->VolPtr,
                                                 p_buf,
                                                 sec_cur + sec_cnt_rd - 1,
                                                 p_err);
                    if (*p_err != FS_ERR_NONE) {
                        if ((*p_err == FS_ERR_SYS_CLUS_CHAIN_END) ||
                            (*p_err == FS_ERR_SYS_CLUS_INVALID)) {
                             *p_err =  FS_ERR_ENTRY_CORRUPT;
                        }
                        FSBuf_Free(p_buf);
                        p_fat_file_data->FileCurSec     = sec_cur;
                        p_fat_file_data->FileCurSecPos  = sec_cur_pos;
                        p_fat_file_data->FilePos       += size - size_rem;
                        return (size - size_rem);
                    }

                    clus_cur_sec_rem = p_fat_data->ClusSize_sec;
                }

            } while (((sec_cnt_rem - sec_cnt_rd) >   0u) &&
                      (sec_next                  == (sec_cur + sec_cnt_rd)));


            FSVol_RdLockedEx(        p_file->VolPtr,            /* Rd full sec's.                                       */
                             (void *)p_dest_08,
                                     sec_cur,
                                     sec_cnt_rd,
                                     FS_VOL_SEC_TYPE_FILE,
                                     p_err);

            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                p_fat_file_data->FileCurSec     = sec_cur;
                p_fat_file_data->FileCurSecPos  = sec_cur_pos;
                p_fat_file_data->FilePos       += size - size_rem;
                return (size - size_rem);
            }

            size_rem   -= FS_UTIL_MULT_PWR2(sec_cnt_rd, p_fat_data->SecSizeLog2);
            p_dest_08  += FS_UTIL_MULT_PWR2(sec_cnt_rd, p_fat_data->SecSizeLog2);
            sec_cur    += sec_cnt_rd - 1u;

            if((sec_cnt_rem - sec_cnt_rd) > 0u) {               /* Next clus is not contiguous.                         */
                sec_cur     = sec_next;                         /* Set the next iter to start at next clus.             */
                sec_cur_pos = 0u;
            } else if (size_rem > 0u) {                         /* Partial sec left, fetch next sec ix for partial rd.  */
                sec_cur_pos = 0u;
                sec_cur     = FS_FAT_SecNextGet(p_file->VolPtr,
                                                p_buf,
                                                sec_cur,
                                                p_err);
                if (*p_err != FS_ERR_NONE) {
                    if ((*p_err == FS_ERR_SYS_CLUS_CHAIN_END) ||
                        (*p_err == FS_ERR_SYS_CLUS_INVALID)) {
                         *p_err =  FS_ERR_ENTRY_CORRUPT;
                    }
                    FSBuf_Free(p_buf);
                    p_fat_file_data->FileCurSec     = sec_cur;
                    p_fat_file_data->FileCurSecPos  = sec_cur_pos;
                    p_fat_file_data->FilePos       += size - size_rem;
                    return (size - size_rem);
                }
            } else {
                sec_cur_pos = p_fat_data->SecSize;
            }


        } else {                                                /* -------------- PARTIAL (FINAL) SEC RD -------------- */
            FSVol_RdLockedEx(p_file->VolPtr,                    /* Rd full sec.                                         */
                             p_buf->DataPtr,
                             sec_cur,
                             1u,
                             FS_VOL_SEC_TYPE_FILE,
                             p_err);

            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                p_fat_file_data->FileCurSec     = sec_cur;
                p_fat_file_data->FileCurSecPos  = sec_cur_pos;
                p_fat_file_data->FilePos       += size - size_rem;
                return (size - size_rem);
            }

            Mem_Copy((void     *)p_dest_08,                     /* Copy partial sec data.                               */
                     (void     *)p_buf->DataPtr,
                     (CPU_SIZE_T)size_rem);
            sec_cur_pos += size_rem;
            size_rem     = 0u;
        }
    }



                                                                /* ----------------- UPDATE FILE INFO ----------------- */
    p_fat_file_data->FileCurSec     = sec_cur;
    p_fat_file_data->FileCurSecPos  = sec_cur_pos;
    p_fat_file_data->FilePos       += size;

    FSBuf_Free(p_buf);

   *p_err = FS_ERR_NONE;

    return (size);
}


/*
*********************************************************************************************************
*                                        FS_FAT_FileTruncate()
*
* Description : Truncate a file.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               size        Size of file after truncation (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              File truncated successful.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_DEV_FULL          Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT     File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) If 'len' is 0, then it is possible for no data cluster to have been assigned to the
*                   file.  In that case, no action need be taken.
*
*               (2) If journaling is enabled & journaling started, logs will be written (from
*                  'FS_FAT_LowEntryTruncated()') to the journal to ensure that either the entry is
*                   truncated to the specified size or no changes occur to the file system.
*
*                   (a) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_FileTruncate (FS_FILE       *p_file,
                           FS_FILE_SIZE   size,
                           FS_ERR        *p_err)
{
    FS_BUF            *p_buf;
    FS_FAT_FILE_DATA  *p_fat_file_data;
    FS_FAT_FILE_SIZE   pos;


                                                                /* ------------------- SET FILE POS ------------------- */
    p_fat_file_data = (FS_FAT_FILE_DATA *)(p_file->DataPtr);

    if ((p_fat_file_data->FileFirstClus == 0u) &&               /* If no data clus's assigned to file      ...          */
        (size                           == 0u)) {               /* ... & file should be truncated to 0 len ...          */
       *p_err = FS_ERR_NONE;                                    /* ... do nothing & rtn (see Note #1).                  */
        return;
    }

    if (size < p_fat_file_data->FilePos) {                      /* If file pos AFTER new EOF ...                        */
        FS_FAT_FilePosSet(p_file,                               /* ... set file pos to new EOF.                         */
                          size,
                          p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }



                                                                /* -------------------- EXTEND FILE ------------------- */
    } else {
        if (size > p_fat_file_data->FileSize) {                 /* If file pos AFTER current EOF ...                    */
            pos = p_fat_file_data->FilePos;                     /* ... save file pos data        ...                    */

            FS_FAT_FilePosSet(p_file,                           /* ... extend file               ...                    */
                              size,
                              p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }

                                                                /* ... restore file pos data.                           */
            FS_FAT_FilePosSet(p_file,                           /* ... extend file               ...                    */
                              pos,
                              p_err);
            return;
        }
    }



                                                                /* ------------------- TRUNCATE FILE ------------------ */
    p_buf = FSBuf_Get(p_file->VolPtr);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FS_FAT_LowEntryTruncate(p_file->VolPtr,
                            p_buf,
                            p_fat_file_data,
                            size,
                            p_err);


    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

                                                                /* -------------------- CLR JOURNAL ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalClrReset(p_file->VolPtr, p_buf, p_err);       /* See Note #2a.                                        */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }
#endif

    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);

}
#endif


/*
*********************************************************************************************************
*                                           FS_FAT_FileWr()
*
* Description : Write to a file.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               -----       Argument validated by caller.
*
*               size        Number of octets to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             File written successful.
*                               FS_ERR_DEV              Device access error.
*                               FS_ERR_DEV_FULL         Device is full (no space could be allocated).
*                               FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : Number of bytes written, if file write successful.
*               0,                       otherwise.
*
* Note(s)     : (1) The sector position is left at the position past the end of the sector when the
*                   next bytes written will be written to the next sector in the file.
*
*               (2) When an error code force the function to exit, FS_FAT_JournalExitFileWr() must be
*                   called. FS_FAT_JournalExitFileWr() can return an error code, however the first
*                   error code to happen has priority over the second.
*
*               (3) The number of bytes used by the file prior to the write operation is used to
*                   determine if the last sector of the write operation has to be read.
*
*               (4) The chain follow operation will overwrite the data stored in the buffer, so the
*                   buffer MUST be flushed before this operation is performed.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_SIZE_T  FS_FAT_FileWr (FS_FILE     *p_file,
                           void        *p_src,
                           CPU_SIZE_T   size,
                           FS_ERR      *p_err)
{
    FS_FAT_CLUS_NBR    clus_start;
    FS_FAT_CLUS_NBR    clus_cur;
    FS_FAT_CLUS_NBR    clus_prev;
    FS_FAT_CLUS_NBR    clus_next;
    FS_FAT_CLUS_NBR    clus_nbr;
    FS_FAT_CLUS_NBR    clus_cnt;
    FS_FAT_SEC_NBR     clus_cur_sec_rem;
    FS_FAT_FILE_SIZE   file_pos;
    FS_FAT_SEC_NBR     sec_cnt_rem;
    FS_FAT_SEC_NBR     sec_cnt_wr;
    FS_FAT_SEC_NBR     sec_cur;
    FS_FAT_SEC_NBR     sec_next;
    FS_FAT_SEC_NBR     sec_cur_pos;
    FS_FAT_FILE_SIZE   file_valid_size_rem;
    CPU_SIZE_T         size_avail_in_file;
    CPU_SIZE_T         size_alloc;
    CPU_SIZE_T         size_rem;
    CPU_SIZE_T         size_wr;
    FS_BUF            *p_buf;
    CPU_INT08U        *p_src_08;
    CPU_INT08U        *p_temp_08;
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_fat_file_data;



                                                                /* ------------------ PREPARE FOR WR ------------------ */
    p_buf = FSBuf_Get(p_file->VolPtr);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return (0u);
    }
    p_fat_file_data = (FS_FAT_FILE_DATA *)(p_file->DataPtr);
    p_fat_data      = (FS_FAT_DATA      *)(p_file->VolPtr->DataPtr);
    p_src_08        = (CPU_INT08U       *)(p_src);


                                                                /* ----------------- ALLOC CLUS CHAIN ----------------- */
    sec_cur_pos = p_fat_file_data->FileCurSecPos;
    sec_cur     = p_fat_file_data->FileCurSec;

                                                                /* Calc nbr of clus to alloc.                           */
    size_alloc  = size;

    if (p_fat_file_data->FileFirstClus != 0u) {
        clus_cur         = FS_FAT_SEC_TO_CLUS(p_fat_data, sec_cur);
        clus_cur_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, sec_cur);

        size_avail_in_file  = p_fat_data->SecSize - sec_cur_pos;            /* Rem size from cur sec.                   */
        size_avail_in_file += p_fat_data->SecSize * (clus_cur_sec_rem - 1u);/* Rem size from cur clus.                  */

        if (size_avail_in_file >= size_alloc) {
            size_alloc = 0u;
        } else {
            size_alloc -= size_avail_in_file;
        }

    } else {
        clus_cur         = 0u;
        clus_cur_sec_rem = p_fat_data->ClusSize_sec;
    }

                                                                /* Nbr of  full    clus.                                */
    clus_nbr  = FS_UTIL_DIV_PWR2(size_alloc, p_fat_data->ClusSizeLog2_octet);
                                                                /* Add one partial clus if needed.                      */
    clus_nbr += ((size_alloc & (p_fat_data->ClusSize_octet - 1u)) != 0u) ? 1u : 0u;

                                                                /* Follow clus chain until end.                         */
    clus_prev = clus_cur;
    if (clus_prev != 0) {
        clus_next = FS_FAT_ClusChainEndFind(p_file->VolPtr,
                                            p_buf,
                                            clus_prev,
                                           &clus_cnt,
                                            p_err);
        if (*p_err != FS_ERR_NONE) {
            if (*p_err == FS_ERR_SYS_CLUS_INVALID) {
                *p_err = FS_ERR_ENTRY_CORRUPT;
            }
            FSBuf_Free(p_buf);
            return (0u);
        }

        if (clus_nbr > clus_cnt) {
            clus_nbr -= clus_cnt;
            clus_prev = clus_next;
        } else {
            clus_nbr = 0;
        }
    }


    clus_start = FS_FAT_ClusChainAlloc(p_file->VolPtr,          /* Alloc non-alloc'd part of chain.                     */
                                       p_buf,
                                       clus_prev,
                                       clus_nbr,
                                       p_err);
    if (*p_err == FS_ERR_NONE) {
        FSBuf_Flush(p_buf, p_err);                              /* Flush buf (see Note #4).                             */
    }
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return (0u);
    }


    if (clus_prev != clus_start) {                              /* If new chain ...                                     */
        clus_cur   = clus_start;
        sec_cur    = FS_FAT_CLUS_TO_SEC(p_fat_data, clus_cur);

        p_fat_file_data->FileFirstClus = clus_start;            /* ... set first clus of file.                          */
    }


                                                                /* --------------------- WR DATA ---------------------- */
    size_rem = size;
                                                                /* Set nb of bytes valid in file prior to wr.           */
    file_valid_size_rem = 0u;
    if (p_fat_file_data->FilePos < p_fat_file_data->FileSize) {
        file_valid_size_rem = p_fat_file_data->FileSize
                            - p_fat_file_data->FilePos;
    }


                                                                /* ------------------ SKIP FULL SEC ------------------- */
    if (sec_cur_pos == p_fat_data->SecSize) {                   /* Sec pos at end of sec (so move to start of next).    */
        sec_cur_pos = 0u;
        if (clus_cur_sec_rem == 1u) {
            clus_cur = FS_FAT_ClusChainFollow(p_file->VolPtr,
                                              p_buf,
                                              clus_cur,
                                              1u,
                                              DEF_NULL,
                                              p_err);

            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                return (0u);
            }

            sec_cur          = FS_FAT_CLUS_TO_SEC(p_fat_data, clus_cur);
            clus_cur_sec_rem = p_fat_data->ClusSize_sec;
        } else {
            sec_cur++;
            clus_cur_sec_rem--;
        }
    }


                                                                /* ------------------ PARTIAL SEC WR ------------------ */
    if (sec_cur_pos != 0u) {
        FSBuf_Flush(p_buf, p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return (0u);
        }

        FSVol_RdLockedEx(p_file->VolPtr,                        /* Rd full sec.                                         */
                         p_buf->DataPtr,
                         sec_cur,
                         1u,
                         FS_VOL_SEC_TYPE_FILE,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return (0u);
        }

        size_wr = DEF_MIN(size_rem, p_fat_data->SecSize - sec_cur_pos);

        p_temp_08 = (CPU_INT08U *)p_buf->DataPtr + sec_cur_pos;
        Mem_Copy((void       *)p_temp_08,                       /* Copy data into buf.                                  */
                 (void       *)p_src_08,
                 (CPU_SIZE_T  )size_wr);

        FSVol_WrLockedEx(p_file->VolPtr,                        /* Wr full sec.                                         */
                         p_buf->DataPtr,
                         sec_cur,
                         1u,
                         FS_VOL_SEC_TYPE_FILE,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return (0u);
        }

        size_rem    -= size_wr;
        p_src_08    += size_wr;
        sec_cur_pos += size_wr;
        clus_cur_sec_rem--;
        if(size_wr > file_valid_size_rem) {
            file_valid_size_rem  = 0u;
        } else {
            file_valid_size_rem -= size_wr;
        }

        if (size_rem > 0u) {                                    /* If more wr octets, get next sec nbr.                 */
            if (clus_cur_sec_rem == 0u) {
                clus_cur = FS_FAT_ClusChainFollow(p_file->VolPtr,
                                                  p_buf,
                                                  clus_cur,
                                                  1u,
                                                  0,
                                                  p_err);
                if (*p_err != FS_ERR_NONE) {
                    FSBuf_Free(p_buf);
                    return (0u);
                }

                sec_cur          = FS_FAT_CLUS_TO_SEC(p_fat_data, clus_cur);
                clus_cur_sec_rem = p_fat_data->ClusSize_sec;
                sec_cur_pos      = 0u;
            } else {
                sec_cur++;
            }
        }
    }


                                                                /* ------------------- FULL SEC WRs ------------------- */
    sec_next = 0u;
    while (size_rem > 0u) {
        sec_cnt_rem = FS_UTIL_DIV_PWR2(size_rem, p_fat_data->SecSizeLog2);

        if (sec_cnt_rem > 0u) {

            sec_cnt_wr = 0;
            do {                                                /* Cnt the max nbr of continuous sec's.                 */
                sec_cnt_wr += DEF_MIN(sec_cnt_rem - sec_cnt_wr, clus_cur_sec_rem);

                if((sec_cnt_rem - sec_cnt_wr) > 0u) {
                    sec_next = FS_FAT_SecNextGet(p_file->VolPtr,
                                                 p_buf,
                                                 sec_cur + sec_cnt_wr - 1,
                                                 p_err);
                    if (*p_err != FS_ERR_NONE) {
                        FSBuf_Free(p_buf);
                        return (0u);
                    }

                    clus_cur_sec_rem = p_fat_data->ClusSize_sec;
                }

            } while (((sec_cnt_rem - sec_cnt_wr) >   0u) &&
                      (sec_next                  == (sec_cur + sec_cnt_wr)));


            FSVol_WrLockedEx(        p_file->VolPtr,            /* Wr full sec's.                                       */
                 (void *)p_src_08,
                         sec_cur,
                         sec_cnt_wr,
                         FS_VOL_SEC_TYPE_FILE,
                         p_err);

            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                return (0u);
            }

            size_rem   -= FS_UTIL_MULT_PWR2(sec_cnt_wr, p_fat_data->SecSizeLog2);
            p_src_08   += FS_UTIL_MULT_PWR2(sec_cnt_wr, p_fat_data->SecSizeLog2);
            sec_cur    += sec_cnt_wr - 1u;

            if((sec_cnt_rem - sec_cnt_wr) > 0u) {               /* Next clus is not contiguous.                         */
                sec_cur     = sec_next;                         /* Set the next iter to start at next clus.             */
                sec_cur_pos = 0u;
            } else if (size_rem > 0u) {                         /* Partial sec left, fetch next sec ix for partial rd.  */
                sec_cur_pos = 0u;
                sec_cur     = FS_FAT_SecNextGet(p_file->VolPtr,
                                                p_buf,
                                                sec_cur,
                                                p_err);
                if (*p_err != FS_ERR_NONE) {
                    FSBuf_Free(p_buf);
                    return (0u);
                }
            } else {
                sec_cur_pos = p_fat_data->SecSize;
            }

        } else {                                                /* -------------- PARTIAL (FINAL) SEC WR -------------- */
            FSBuf_Flush(p_buf, p_err);
            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                return (0u);
            }

            if(file_valid_size_rem > 0u) {                     /* Rd only if there is valid data rem (See Note #3).     */
                FSVol_RdLockedEx(p_file->VolPtr,               /* Rd full sec.                                          */
                                 p_buf->DataPtr,
                                 sec_cur,
                                 1u,
                                 FS_VOL_SEC_TYPE_FILE,
                                 p_err);

                if (*p_err != FS_ERR_NONE) {
                    FSBuf_Free(p_buf);
                    return (0u);
                }
#if  (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
            } else {                                           /* Clr buf data.                                         */
                Mem_Clr(p_buf->DataPtr, p_buf->Size);
            }
#else
        }
#endif



            Mem_Copy((void       *)p_buf->DataPtr,              /* Copy data into buf.                                  */
                     (void       *)p_src_08,
                     (CPU_SIZE_T  )size_rem);

            FSVol_WrLockedEx(p_file->VolPtr,                    /* Wr full sec.                                         */
                             p_buf->DataPtr,
                             sec_cur,
                             1u,
                             FS_VOL_SEC_TYPE_FILE,
                             p_err);

            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                return (0u);
            }

            sec_cur_pos = size_rem;
            size_rem    = 0u;
        }
    }


                                                                /* ----------------- UPDATE DIR ENTRY ----------------- */
    file_pos = p_fat_file_data->FilePos + size;

    p_fat_file_data->FileCurSec    = sec_cur;
    p_fat_file_data->FileCurSecPos = sec_cur_pos;
    p_fat_file_data->FilePos       = file_pos;

    if (file_pos > p_fat_file_data->FileSize) {                 /* New file pos past old file size.                     */
        p_fat_file_data->FileSize = file_pos;
    }

                                                                /* If file uncached ...                                 */
    if (DEF_BIT_IS_CLR(p_fat_file_data->Mode, FS_FILE_ACCESS_MODE_CACHED) == DEF_YES) {
        FS_FAT_LowEntryUpdate(p_file->VolPtr,                   /*                  ... update dir entry now.           */
                              p_buf,
                              p_fat_file_data,
                              DEF_YES,
                              p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return (0u);
        }

    } else {                                                    /* ... otherwise    ...                                 */
        p_fat_file_data->UpdateReqd = DEF_YES;                  /*                  ... update dir entry at close.      */
    }


                                                                /* -------------------- CLR JOURNAL ------------------- */
#ifdef FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalClrReset(p_file->VolPtr, p_buf, p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return (0u);
    }
#endif


    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
   *p_err = FS_ERR_NONE;
    return (size);
}
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_fat.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of FAT module include (see Note #1).             */
