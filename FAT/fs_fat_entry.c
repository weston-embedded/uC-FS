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
*                                     FILE SYSTEM FAT MANAGEMENT
*
*                                            ENTRY ACCESS
*
* Filename : fs_fat_entry.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_ENTRY_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_entry.h"
#include  "../Source/fs_file.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_entry.h"
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
*********************************************************************************************************
*                                       SYSTEM DRIVER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FS_FAT_EntryAttribSet()
*
* Description : Set a file or directory's attributes.
*
* Argument(s) : p_vol       Pointer to volume on which the entry resides.
*               ----------  Argument validated by caller.
*
*               name_entry  Name of the entry.
*               ----------  Argument validated by caller.
*
*               attrib      Entry attributes to set (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                      Entry attributes set successfully.
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) FAT does NOT support write-only file attributes.  Consequently, it is NOT
*                       possible to reset a file's read attribute.
*
*                   (b) See 'FSEntry_AttribSet() Note #2'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_EntryAttribSet (FS_VOL    *p_vol,
                             CPU_CHAR  *name_entry,
                             FS_FLAGS   attrib,
                             FS_ERR    *p_err)
{
    CPU_INT08U         fat_attrib;
    FS_FAT_FILE_DATA   entry_data;
    FS_BUF            *p_buf;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------ VALIDATE ATTRIB ----------------- */
    if (DEF_BIT_IS_CLR(attrib, FS_ENTRY_ATTRIB_RD) == DEF_YES) {/* See Note #1.                                         */
       *p_err = FS_ERR_FILE_INVALID_ATTRIB;
        return;
    }
#endif


                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    FS_FAT_LowEntryFind( p_vol,
                        &entry_data,
                         name_entry,
                        (FS_FAT_MODE_RD | FS_FAT_MODE_FILE | FS_FAT_MODE_DIR),
                         p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ----------------- UPDATE DIR ENTRY ----------------- */
    p_buf = FSBuf_Get(p_vol);

    if (p_buf == ((FS_BUF *)0)) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    fat_attrib = entry_data.Attrib;

    if (DEF_BIT_IS_SET(attrib, FS_ENTRY_ATTRIB_HIDDEN) == DEF_YES) {
        DEF_BIT_SET_08(fat_attrib, FS_FAT_DIRENT_ATTR_HIDDEN);
    } else {
        DEF_BIT_CLR_08(fat_attrib, FS_FAT_DIRENT_ATTR_HIDDEN);
    }

    if (DEF_BIT_IS_SET(attrib, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
        DEF_BIT_CLR_08(fat_attrib, FS_FAT_DIRENT_ATTR_READ_ONLY);
    } else {
        DEF_BIT_SET_08(fat_attrib, FS_FAT_DIRENT_ATTR_READ_ONLY);
    }

    entry_data.Attrib = fat_attrib;

    FS_FAT_LowEntryUpdate( p_vol,
                           p_buf,
                          &entry_data,
                           DEF_NO,
                           p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

#ifdef FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalClrReset(p_vol, p_buf, p_err);
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
*                                        FS_FAT_EntryCreate()
*
* Description : Create an entry.
*
* Argument(s) : p_vol       Pointer to volume on which entry resides.
*
*               name_entry  Name of the entry.
*
*               entry_type  Indicates whether the new entry shall be a directory or a file :
*
*                               FS_ENTRY_TYPE_DIR,  if the entry shall be a directory.
*                               FS_ENTRY_TYPE_FILE, if the entry shall be a file.
*
*               excl        Indicates whether creation of new entry should be exclusive (see Note #1b) :
*
*                               DEF_YES, if the entry will be created ONLY if 'name_entry' does not exist.
*                               DEF_NO,  if the entry will be created even if 'name_entry' does     exist.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry created successfully.
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_DEV_FULL                  Device is full (space could not be allocated).
*                               FS_ERR_DIR_FULL                  Directory is full (space could not be allocated).
*                               FS_ERR_ENTRY_CORRUPT             File system entry is corrupt.
*                               FS_ERR_ENTRY_EXISTS              File system entry exists.
*                               FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                               FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                               FS_ERR_ENTRY_NOT_FILE            File system entry NOT a file.
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) If journaling is enabled & journaling started, logs will be written (from
*                  'FS_FAT_LowEntryCreate()') to the journal to ensure that either the new entry is
*                   fully created or no changes occur to the file system.
*
*                   (a) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_EntryCreate (FS_VOL       *p_vol,
                          CPU_CHAR     *name_entry,
                          FS_FLAGS      entry_type,
                          CPU_BOOLEAN   excl,
                          FS_ERR       *p_err)
{
    FS_FAT_FILE_DATA  entry_data;
    FS_FLAGS          mode;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_BUF           *p_buf;
#endif


                                                                /* ----------------- CREATE DIR ENTRY ----------------- */
    mode = FS_FAT_MODE_CREATE | FS_FAT_MODE_WR;

    if (entry_type == FS_ENTRY_TYPE_DIR) {
        mode |= FS_FAT_MODE_DIR;
        if (excl == DEF_YES) {
            mode |= FS_FAT_MODE_MUST_CREATE;
        }
    } else {
        mode |= FS_FAT_MODE_FILE;
        if (excl == DEF_NO) {                                   /* If no excl                             ...           */
            mode |= FS_FAT_MODE_TRUNCATE;                       /* ... truncate file if it already exists ...           */
        } else {                                                /* ... else                               ...           */
            mode |= FS_FAT_MODE_MUST_CREATE;                    /* ... entry MUST not exist.                            */
        }
    }



    FS_FAT_LowEntryFind( p_vol,
                        &entry_data,
                         name_entry,
                         mode,
                         p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* -------------------- CLR JOURNAL ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == DEF_NULL) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FS_FAT_JournalClrReset(p_vol, p_buf, p_err);                /* Clr journal (see Note #1a).                          */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }
                                                                /* Flush & free buf.                                    */
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
#endif
}
#endif


/*
*********************************************************************************************************
*                                          FS_FAT_EntryDel()
*
* Description : Delete a file or directory.
*
* Argument(s) : p_vol       Pointer to volume on which the entry resides.
*
*               name_entry  Name of the entry.
*
*               entry_type  Indicates whether the new entry can be a directory / a file :
*
*                               FS_ENTRY_TYPE_DIR,  if the entry can be a directory.
*                               FS_ENTRY_TYPE_FILE, if the entry can be a file.
*                               FS_ENTRY_TYPE_ANY,  if the entry can be any type
*
*                           These values can be OR'd to allow multiple types.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry deleted successfully.
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                               FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) If journaling is enabled & journaling started, logs will be written (from
*                  'FS_FAT_LowEntryDel()') to the journal to ensure that either the entry is fully
*                   deleted or no changes occur to the file system.
*
*                   (a) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_EntryDel (FS_VOL       *p_vol,
                       CPU_CHAR     *name_entry,
                       FS_FLAGS      entry_type,
                       FS_ERR       *p_err)
{
    FS_FAT_FILE_DATA  entry_data;
    FS_FLAGS          mode;
    CPU_BOOLEAN       is_file;
    CPU_BOOLEAN       is_dir;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_BUF           *p_buf;
#endif

                                                                /* ------------------- DEL DIR ENTRY ------------------ */
    mode = FS_FAT_MODE_WR | FS_FAT_MODE_DEL;

    is_file = DEF_BIT_IS_SET(entry_type, FS_ENTRY_TYPE_FILE);
    if (is_file == DEF_YES) {
        mode |= FS_FAT_MODE_FILE;
    }
    is_dir = DEF_BIT_IS_SET(entry_type, FS_ENTRY_TYPE_DIR);
    if (is_dir == DEF_YES) {
        mode |= FS_FAT_MODE_DIR;
    }

    FS_FAT_LowEntryFind( p_vol,
                        &entry_data,
                         name_entry,
                         mode,
                         p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* -------------------- CLR JOURNAL ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == DEF_NULL) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FS_FAT_JournalClrReset(p_vol, p_buf, p_err);                /* Clr journal (see Note #1a).                          */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }
                                                                /* Flush & free buf.                                    */
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
#endif
}
#endif


/*
*********************************************************************************************************
*                                         FS_FAT_EntryQuery()
*
* Description : Get information about a file or directory.
*
* Argument(s) : p_vol       Pointer to volume on which the entry resides.
*
*               name_entry  Name of the entry.
*
*               p_info      Pointer to structure that will receive the entry information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry information gotten successfully.
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_EntryQuery (FS_VOL         *p_vol,
                         CPU_CHAR       *name_entry,
                         FS_ENTRY_INFO  *p_info,
                         FS_ERR         *p_err)
{
    CPU_INT08U         fat_attrib;
    FS_FLAGS           fs_attrib;
    FS_FAT_FILE_DATA   entry_data;
    FS_FAT_DATA       *p_fat_data;



                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    FS_FAT_LowEntryFind( p_vol,
                        &entry_data,
                         name_entry,
                        (FS_FAT_MODE_RD | FS_FAT_MODE_FILE | FS_FAT_MODE_DIR),
                         p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ------------------- FILE ATTRIB'S ------------------ */
    p_fat_data = (FS_FAT_DATA *)(p_vol->DataPtr);
    fat_attrib =  entry_data.Attrib;
    fs_attrib  =  0u;
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
    p_info->Size    = (FS_FILE_SIZE)entry_data.FileSize;        /* File size.                                           */
    p_info->BlkSize =  p_fat_data->ClusSize_octet;              /* Blk size (bytes per clus) & nbr blks (nbr clus).     */
    p_info->BlkCnt  = (entry_data.FileSize == 0u) ? (0u) : (FS_UTIL_DIV_PWR2(entry_data.FileSize - 1u, p_fat_data->ClusSizeLog2_octet) + 1u);



                                                                /* ----------------- FILE DATE & TIME ----------------- */
    FS_FAT_DateTimeParse(&p_info->DateTimeCreate,               /* Creation date/time.                                  */
                          entry_data.DateCreate,
                          entry_data.TimeCreate);

    FS_FAT_DateTimeParse(&p_info->DateTimeWr,                   /* Wr       date/time.                                  */
                          entry_data.DateWr,
                          entry_data.TimeWr);

    FS_FAT_DateTimeParse(&p_info->DateAccess,                   /* Access    date.                                      */
                          entry_data.DateAccess,
                          0u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_EntryRename()
*
* Description : Rename a file or directory.
*
* Argument(s) : p_vol           Pointer to volume on which the entry resides.
*
*               name_entry_old  Old path of the entry.
*
*               name_entry_new  New path of the entry.
*
*               excl            Indicates whether creation of new entry should be exclusive (see Note #2):
*
*                                   DEF_YES, if the entry will be renamed ONLY if 'name_entry_new' does
*                                                not exist.
*                                   DEF_NO,  if the entry will be renamed even if 'name_entry_new' does
*                                                exist.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Entry renamed successfully.
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_DEV_FULL                  Device is full (space could not be allocated).
*                                   FS_ERR_DIR_FULL                  Directory is full (space could not be allocated).
*                                   FS_ERR_ENTRIES_TYPE_DIFF         Paths do not both specify files
*                                                                        OR directories.
*                                   FS_ERR_ENTRY_CORRUPT             File system entry is corrupt.
*                                   FS_ERR_ENTRY_EXISTS              File system entry exists.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                   FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) 'name_entry_old' MUST contain the full path of the source file.
*
*                   (b) 'name_entry_new' MUST contain the full path of the destination file.
*
*               (2) #### Account for invalidity test :
*
*                           "'name_entry_new' contains a path prefix that names 'name_entry_old'."
*
*               (3) If journaling is enabled & journaling started, logs will be written (from
*                  'FS_FAT_LowEntryRename()') to the journal to ensure that either the entry is renamed
*                   or no changes occur to the file system.  Nested logs to handle deletion or truncation
*                   of an existing file or directory with the new name, or creation of the entry with the
*                   new name, may be written as well (from 'FS_FAT_LowEntryDelete()', 'FS_FAT_LowEntryTruncate()'
*                   or 'FS_FAT_LowEntryCreate()').
*
*                   (a) Since this is a top level action, the journal must be cleared once it is finished
*                       or after an error occurs.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_EntryRename (FS_VOL       *p_vol,
                          CPU_CHAR     *name_entry_old,
                          CPU_CHAR     *name_entry_new,
                          CPU_BOOLEAN   excl,
                          FS_ERR       *p_err)
{
    FS_BUF            *p_buf;
    CPU_BOOLEAN        empty;
    CPU_BOOLEAN        exists;
    FS_FAT_FILE_DATA   entry_data_old;
    FS_FAT_FILE_DATA   entry_data_new;
    FS_FLAGS           mode;
    CPU_CHAR          *p_slash;


                                                                /* ------------------ FIND SRC ENTRY ------------------ */
    FS_FAT_LowEntryFind( p_vol,
                        &entry_data_old,
                         name_entry_old,
                        (FS_FAT_MODE_RD | FS_FAT_MODE_FILE | FS_FAT_MODE_DIR),
                         p_err);

    if (*p_err != FS_ERR_NONE) {                                /* If src does not exist ...                            */
        return;                                                 /* ... rtn.                                             */
    }



                                                                /* ------------------ CHK DEST ENTRY ------------------ */
    if (DEF_BIT_IS_SET(entry_data_old.Attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
        mode = FS_FAT_MODE_DIR;
    } else {
        mode = FS_FAT_MODE_FILE;
    }

    FS_FAT_LowEntryFind( p_vol,
                        &entry_data_new,
                         name_entry_new,
                        (FS_FAT_MODE_RD | mode),
                         p_err);

    if (*p_err == FS_ERR_NONE) {                                /* If entry exists     ...                              */
        if (excl == DEF_YES) {                                  /* ... & entry must NOT exist ...                       */
           *p_err = FS_ERR_ENTRY_EXISTS;                        /* ... rtn err.                                         */
            return;
        }
                                                                /* ... & is same entry ...                              */
        if ((entry_data_old.FileFirstClus    == entry_data_new.FileFirstClus)  &&
            (entry_data_old.DirStartSec      == entry_data_new.DirStartSec)    &&
            (entry_data_old.DirStartSecPos   == entry_data_new.DirStartSecPos) &&
            (entry_data_old.DirEndSec        == entry_data_new.DirEndSec)      &&
            (entry_data_old.DirEndSecPos     == entry_data_new.DirEndSecPos)) {
           *p_err = FS_ERR_ENTRIES_SAME;                        /* ... rtn err.                                         */
            return;
        }

                                                                /* If dir, must be empty.                               */
        if (DEF_BIT_IS_SET(entry_data_new.Attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
            p_buf = FSBuf_Get(p_vol);
            if (p_buf == (FS_BUF *)0) {
               *p_err = FS_ERR_BUF_NONE_AVAIL;
                return;
            }

            empty = FS_FAT_LowDirChkEmpty(p_vol,
                                          p_buf,
                                          entry_data_new.FileFirstClus,
                                          p_err);

            FSBuf_Free(p_buf);

            if (empty == DEF_NO) {
               *p_err =  FS_ERR_ENTRY_NOT_EMPTY;
                return;
            }
        }

        exists = DEF_YES;                                       /* Entry with new name exists.                          */

    } else {
        if (*p_err != FS_ERR_ENTRY_NOT_FOUND) {                 /* If entry was found w/err ...                         */
            if ((*p_err == FS_ERR_ENTRY_NOT_FILE) ||
                (*p_err == FS_ERR_ENTRY_NOT_DIR)) {
                 *p_err =  FS_ERR_ENTRIES_TYPE_DIFF;
            }
            return;                                             /* ... rtn err.                                         */
        }

        exists = DEF_NO;                                        /* Entry with new name does NOT exist.                  */
    }



                                                                /* ------------------- RENAME ENTRY ------------------- */
    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_slash = FS_FAT_Char_LastPathSep(name_entry_new);
    if (p_slash != (CPU_CHAR *)0) {
        name_entry_new = p_slash + 1u;
    }

    FS_FAT_LowEntryRename( p_vol,
                           p_buf,
                          &entry_data_old,
                          &entry_data_new,
                           exists,
                           name_entry_new,
                           p_err);



    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

                                                                /* -------------------- CLR JOURNAL ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalClrReset(p_vol, p_buf, p_err);                /* See Note #3a.                                        */
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
*                                        FS_FAT_EntryTimeSet()
*
* Description : Set a file or directory's date/time.
*
* Argument(s) : p_vol           Pointer to volume on which the entry resides.
*
*               name_entry      Name of the entry.
*
*               p_time          Pointer to date/time.
*
*               time_type       Flag to indicate which Date/Time should be set
*
*                                   FS_DATE_TIME_CREATE
*                                   FS_DATE_TIME_MODIFY
*                                   FS_DATE_TIME_ACCESS
*                                   FS_DATE_TIME_ALL
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                         Entry date/time set successfully.
*                                   FS_ERR_BUF_NONE_AVAIL               Buffer not available.
*                                   FS_ERR_DEV                          Device access error.
*                                   FS_ERR_ENTRY_NOT_FOUND              File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND       Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR         Entry parent NOT a directory.
*                                   FS_ERR_NAME_INVALID                 Invalid file name or path.
*                                   FS_ERR_FILE_INVALID_DATE_TIME_TYPE  Which date/time flag is specified invalid.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_EntryTimeSet (FS_VOL         *p_vol,
                           CPU_CHAR       *name_entry,
                           CLK_DATE_TIME  *p_time,
                           CPU_INT08U      time_type,
                           FS_ERR         *p_err)
{
    FS_BUF            *p_buf;
    CPU_INT08U        *p_dir_entry;
    FS_FAT_FILE_DATA   entry_data;
    FS_FAT_DATE        entry_date;
    FS_FAT_TIME        entry_time;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_DIR_POS     dir_end_pos;
#endif


                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    FS_FAT_LowEntryFind(p_vol,
                       &entry_data,
                        name_entry,
                       (FS_FAT_MODE_RD | FS_FAT_MODE_FILE | FS_FAT_MODE_DIR),
                        p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ------------------ PARSE DATE/TIME ----------------- */
    entry_date = FS_FAT_DateFmt(p_time);
    entry_time = FS_FAT_TimeFmt(p_time);

    switch (time_type) {
        case FS_DATE_TIME_CREATE:
             entry_data.DateCreate = entry_date;
             entry_data.TimeCreate = entry_time;
             break;

        case FS_DATE_TIME_MODIFY:
             entry_data.DateWr     = entry_date;
             entry_data.TimeWr     = entry_time;
             break;

        case FS_DATE_TIME_ACCESS:
             entry_data.DateAccess = entry_date;
             break;

        case FS_DATE_TIME_ALL:
             entry_data.DateCreate = entry_date;
             entry_data.TimeCreate = entry_time;
             entry_data.DateWr     = entry_date;
             entry_data.TimeWr     = entry_time;
             entry_data.DateAccess = entry_date;
             break;

        default:
            *p_err = FS_ERR_FILE_INVALID_DATE_TIME_TYPE;
             return;
    }


    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

                                                                /* ------------------ ENTER JOURNAL ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
        dir_end_pos.SecNbr = entry_data.DirEndSec;
        dir_end_pos.SecPos = entry_data.DirEndSecPos;
        FS_FAT_JournalEnterEntryUpdate(p_vol,
                                       p_buf,
                                      &dir_end_pos,
                                      &dir_end_pos,
                                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#endif


    FSBuf_Set(p_buf,                                            /* Rd dir sec.                                          */
              entry_data.DirEndSec,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + entry_data.DirEndSecPos;
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_CRTDATE),    entry_data.DateCreate);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_CRTTIME),    entry_data.TimeCreate);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_LSTACCDATE), entry_data.DateAccess);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTDATE),    entry_data.DateWr);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTTIME),    entry_data.TimeWr);

                                                                /* Wr dir sec.                                          */
    FSBuf_MarkDirty(p_buf, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalClrReset(p_vol, p_buf, p_err);
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
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of FAT module include (see Note #1).             */
