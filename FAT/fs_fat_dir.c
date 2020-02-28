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
*                                          DIRECTORY ACCESS
*
* Filename : fs_fat_dir.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_DIR_MODULE


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
#include  "../Source/fs_dir.h"
#include  "../Source/fs_file.h"
#include  "../Source/fs_unicode.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_dir.h"
#include  "fs_fat_lfn.h"
#include  "fs_fat_sfn.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*
*           (2) See 'fs_dir.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */
#ifdef   FS_DIR_MODULE_PRESENT                                  /* See Note #2.                                         */


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

static  MEM_POOL  FS_FAT_DirDataPool;


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
*                                       FS_FAT_DirModuleInit()
*
* Description : Initialize FAT Directory module.
*
* Argument(s) : dir_cnt     Number of directories to allocate.
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

void  FS_FAT_DirModuleInit (FS_QTY   dir_cnt,
                            FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


    Mem_PoolCreate(&FS_FAT_DirDataPool,
                    DEF_NULL,
                    0,
                    dir_cnt,
                    sizeof(FS_FAT_FILE_DATA),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
        *p_err  = FS_ERR_MEM_ALLOC;
         FS_TRACE_INFO(("FS_FAT_DirModuleInit(): Could not alloc mem for FAT file info: %d octets req'd.\r\n", octets_reqd));
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
*                                          FS_FAT_DirClose()
*
* Description : Close a directory.
*
* Argument(s) : p_dir       Pointer to a directory.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to free the directory data back to the directory
*                   data pool.
*********************************************************************************************************
*/

void  FS_FAT_DirClose (FS_DIR  *p_dir)
{
    FS_FAT_FILE_DATA  *p_dir_data;
    LIB_ERR            pool_err;
    FS_ERR             err;


    p_dir_data = (FS_FAT_FILE_DATA *)p_dir->DataPtr;



                                                                /* ------------------- FREE DIR DATA ------------------ */
    FS_OS_Lock(&err);                                           /* Acquire FS lock (see Note #1).                       */
    if (err != FS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);                                    /* Fatal err.                                           */
    }

    Mem_PoolBlkFree(        &FS_FAT_DirDataPool,                /* Free FAT dir data.                                   */
                    (void *) p_dir_data,
                            &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
        CPU_SW_EXCEPTION(;);                                    /* Fatal err.                                           */
    }

    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*                                          FS_FAT_DirOpen()
*
* Description : Open a directory.
*
* Argument(s) : p_dir       Pointer to associated directory.
*
*               name_dir    Name of the directory.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Directory opened successfully.
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to get the directory data from the directory data
*                   pool.
*********************************************************************************************************
*/

void  FS_FAT_DirOpen (FS_DIR    *p_dir,
                      CPU_CHAR  *name_dir,
                      FS_ERR    *p_err)
{
    FS_FAT_FILE_DATA  *p_dir_data;
    LIB_ERR            pool_err;


                                                                /* ------------------ ALLOC DIR DATA ------------------ */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock (see Note #1).                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_data = (FS_FAT_FILE_DATA *)Mem_PoolBlkGet(&FS_FAT_DirDataPool,
                                                     sizeof(FS_FAT_FILE_DATA),
                                                    &pool_err);

    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    FS_OS_Unlock();

    if (p_dir_data == DEF_NULL) {
        return;
    }



                                                                /* --------------------- OPEN DIR --------------------- */
    p_dir->DataPtr = (void *)p_dir_data;
    FS_FAT_LowEntryFind( p_dir->VolPtr,
                         p_dir_data,
                         name_dir,
                        (FS_FAT_MODE_RD | FS_FAT_MODE_DIR),
                         p_err);

    if (*p_err != FS_ERR_NONE) {
         FS_FAT_DirClose(p_dir);
    }
}


/*
*********************************************************************************************************
*                                            FSSys_DirRd()
*
* Description : Read a directory entry from a directory.
*
* Argument(s) : p_dir           Pointer to a directory.
*
*               p_dir_entry     Pointer to a directory entry.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE              Directory entry read successfully.
*                                   FS_ERR_EOF               End of directory reached.
*                                   FS_ERR_DEV               Device access error.
*                                   FS_ERR_BUF_NONE_AVAIL    Buffer not available.
*
* Return(s)   : none.
*
* Note(s)     : (1) A block, for the purpose of the file information, is a cluster.  The number of blocks
*                   is the number of clusters holding file data; it is possible that additional empty
*                   clusters may be linked to the file.
*********************************************************************************************************
*/

void  FS_FAT_DirRd (FS_DIR        *p_dir,
                    FS_DIR_ENTRY  *p_dir_entry,
                    FS_ERR        *p_err)
{
    FS_BUF            *p_buf;
    FS_FAT_FILE_DATA  *p_dir_data;
    FS_FAT_DATE        date_val;
    FS_FAT_DIR_POS     end_pos;
    CPU_INT08U         fat_attrib;
    FS_FAT_DATA       *p_fat_data;
    FS_FLAGS           fs_attrib;
    FS_FAT_DIR_POS     pos;
    CPU_INT08U        *p_temp;
    FS_FAT_TIME        time_val;
#if  defined(FS_FAT_LFN_MODULE_PRESENT) && \
            (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_WCHAR         *p_name_rtn;
    CPU_WCHAR          name_lfn[FS_FAT_MAX_FILE_NAME_LEN + 1u];
#endif


                                                                /* ------------------ PREPARE FOR RD ------------------ */
    p_buf = FSBuf_Get(p_dir->VolPtr);                           /* Get rd buf.                                          */
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_dir_data = (FS_FAT_FILE_DATA *)(p_dir->DataPtr);
    p_fat_data = (FS_FAT_DATA      *)(p_dir->VolPtr->DataPtr);

    pos.SecNbr = p_dir_data->FileCurSec;
    pos.SecPos = p_dir_data->FileCurSecPos;


                                                                /* ------------------ FIND DIR ENTRY ------------------ */
#if  defined(FS_FAT_LFN_MODULE_PRESENT) && \
            (FS_CFG_UTF8_EN == DEF_ENABLED)
    FS_FAT_FN_API_Active.NextDirEntryGet(         p_dir->VolPtr,
                                                  p_buf,
                                         (void *)&name_lfn[0],
                                                 &pos,
                                                 &end_pos,
                                                  p_err);
#else
    FS_FAT_FN_API_Active.NextDirEntryGet(         p_dir->VolPtr,
                                                  p_buf,
                                         (void *)&p_dir_entry->Name[0],
                                                 &pos,
                                                 &end_pos,
                                                  p_err);
#endif

                                                                /* ---------------- GET DIR ENTRY INFO ---------------- */
    switch (*p_err) {
        case FS_ERR_EOF:                                        /* EOF encountered.                                     */
             p_dir_data->FileCurSec    = end_pos.SecNbr;
             p_dir_data->FileCurSecPos = end_pos.SecPos;
             break;


        case FS_ERR_NONE:
             p_temp = (CPU_INT08U *)p_buf->DataPtr + end_pos.SecPos;
#if  defined(FS_FAT_LFN_MODULE_PRESENT) && \
            (FS_CFG_UTF8_EN == DEF_ENABLED)
             p_name_rtn = &name_lfn[0];
             WC_StrToMB(&p_dir_entry->Name[0],                  /* Copy wide-char str to multi-byte str buf.            */
                        &p_name_rtn,
                         FS_FAT_MAX_FILE_NAME_LEN);
#endif

             fs_attrib  = 0u;
             fat_attrib = MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_ATTR));;
                                                                /* Parse attrib's.                                      */
             DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_RD);
             if (DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_READ_ONLY) == DEF_NO) {
                 DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_WR);
             }
             if (DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_HIDDEN   ) == DEF_YES) {
                 DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_HIDDEN);
             }
             if (DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
                 DEF_BIT_SET(fs_attrib, FS_ENTRY_ATTRIB_DIR);
             }

             p_dir_entry->Info.Attrib  =   fs_attrib;
             p_dir_entry->Info.Size    =  (FS_FILE_SIZE)MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_FILESIZE));

                                                                /* Calc blk cnt (see Note #1).                          */
             p_dir_entry->Info.BlkSize =  p_fat_data->ClusSize_octet;
             if ((p_dir_entry->Info.Size & (p_fat_data->ClusSize_octet - 1u)) != 0u) {
                p_dir_entry->Info.BlkCnt = FS_UTIL_DIV_PWR2(p_dir_entry->Info.Size, p_fat_data->ClusSizeLog2_octet) + 1u;
             } else {
                p_dir_entry->Info.BlkCnt = FS_UTIL_DIV_PWR2(p_dir_entry->Info.Size, p_fat_data->ClusSizeLog2_octet);
             }

             date_val = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_CRTDATE));
             time_val = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_CRTTIME));
             FS_FAT_DateTimeParse(&(p_dir_entry->Info.DateTimeCreate), date_val, time_val);

             date_val = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_WRTDATE));
             time_val = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp + FS_FAT_DIRENT_OFF_WRTTIME));
             FS_FAT_DateTimeParse(&(p_dir_entry->Info.DateTimeWr),     date_val, time_val);

             p_dir_data->FileCurSec    = end_pos.SecNbr;
             p_dir_data->FileCurSecPos = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;
             break;


        default:                                                /* Other error.                                         */
             break;
    }


    FSBuf_Free(p_buf);
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_dir.h  MODULE  Note #1'.
*
*           (2) See 'MODULE  Note #2' & 'fs_fat.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of directory module include (see Note #2).       */
#endif                                                          /* End of FAT       module include (see Note #1).       */
