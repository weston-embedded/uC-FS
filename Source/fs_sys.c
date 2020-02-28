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
*                                FILE SYSTEM SYSTEM DRIVER MANAGEMENT
*
* Filename : fs_sys.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) The available system driver is known at compile time :
*
*                (a) The FAT system driver will be present if FS_CFG_SYS_DRV_SEL is FS_SYS_DRV_SEL_FAT.
*                    This will support FAT12, FAT16 & FAT32 systems (depending on configuration) &
*                    support LFN (depending on configuration).
*
*            (2) The "NO SYS DRV" pre-processor 'else'-conditional code will never be compiled/linked
*                since 'fs_cfg_fs.h' ensures that at least one constant (e.g., FS_FAT_MODULE_PRESENT)
*                will be defined.  The 'else'-conditional code is included for completeness & as an
*                extra precaution in case 'fs_cfg_fs.h' is incorrectly modified.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_SYS_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "fs.h"
#include  "fs_err.h"
#include  "fs_file.h"
#include  "fs_sys.h"
#include  "../FAT/fs_fat.h"
#include  "../FAT/fs_fat_dir.h"
#include  "../FAT/fs_fat_entry.h"
#include  "../FAT/fs_fat_file.h"


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
*                                         FSSys_ModuleInit()
*
* Description : Initialize system driver.
*
* Argument(s) : vol_cnt     Number of volumes in use.
*
*               file_cnt    Number of files in use.
*
*               dir_cnt     Number of directories in use.
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

void  FSSys_ModuleInit (FS_QTY   vol_cnt,
                        FS_QTY   file_cnt,
                        FS_QTY   dir_cnt,
                        FS_ERR  *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_ModuleInit(vol_cnt, file_cnt, dir_cnt, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          VOLUME FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSSys_VolClose()
*
* Description : Free system driver-specific structures.
*
* Argument(s) : p_vol       Pointer to volume.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_VolClose (FS_VOL  *p_vol)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_VolClose(p_vol);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                           FSSys_VolFmt()
*
* Description : Format a volume.
*
* Argument(s) : p_dev       Pointer to device.
*
*               p_sys_cfg   File system configuration.
*
*               start       Start sector of file system.
*
*               size        Number of sectors in file system.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                Volume formatted successfully.
*                               FS_ERR_BUF_NONE_AVAIL      No buf available.
*                               FS_ERR_DEV                 Device access error.
*                               FS_ERR_DEV_INVALID_SIZE    Invalid device size.
*                               FS_ERR_VOL_INVALID_SYS     Invalid volume for file system.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_VolFmt (FS_VOL       *p_vol,
                    void         *p_sys_cfg,
                    FS_SEC_SIZE   sec_size,
                    FS_SEC_QTY    size,
                    FS_ERR       *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_VolFmt(p_vol, p_sys_cfg, sec_size, size, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                         FSSys_VolLabelGet()
*
* Description : Get volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               label       String buffer that will receive volume label.
*
*               len_max     Size of string buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   Label gotten.
*                               FS_ERR_VOL_LABEL_NOT_FOUND    Volume label was not found.
*                               FS_ERR_VOL_LABEL_TOO_LONG     Volume label is too long.
*                               FS_ERR_DEV                    Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_VolLabelGet (FS_VOL      *p_vol,
                         CPU_CHAR    *label,
                         CPU_SIZE_T   len_max,
                         FS_ERR      *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_VolLabelGet(p_vol, label, len_max, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                         FSSys_VolLabelSet()
*
* Description : Set volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               label       Volume label.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  Label set.
*                               FS_ERR_VOL_LABEL_INVALID     Volume label is invalid.
*                               FS_ERR_VOL_LABEL_TOO_LONG    Volume label is too long.
*                               FS_ERR_DEV                   Device access error.
*                               FS_ERR_DIR_FULL              Directory is full (space could not be allocated).
*                               FS_ERR_DEV_FULL              Device is full (space could not be allocated).
*                               FS_ERR_ENTRY_CORRUPT         File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_VolLabelSet (FS_VOL    *p_vol,
                         CPU_CHAR  *label,
                         FS_ERR    *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_VolLabelSet(p_vol, label, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                           FSSys_VolOpen()
*
* Description : Detect file system on volume & initialize associated structures.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              File system information gotten.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_INVALID_SYS       Invalid file system.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_VolOpen (FS_VOL  *p_vol,
                     FS_ERR  *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
     FS_FAT_VolOpen(p_vol, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                          FSSys_VolQuery()
*
* Description : Get info about file system on a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_info      Pointer to structure that will receive file system information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              File system information gotten.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/


void  FSSys_VolQuery (FS_VOL       *p_vol,
                      FS_SYS_INFO  *p_info,
                      FS_ERR       *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_VolQuery(p_vol, p_info, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           FILE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSSys_FileClose()
*
* Description : Close a file.
*
* Argument(s) : p_file      Pointer to a file.
*               -----       Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE        File closed.
*                               FS_ERR_NULL_PTR    FS_FAT_FileClose() NULL pointer error return.
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_FileClose  (FS_FILE  *p_file,
                        FS_ERR   *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_FileClose(p_file, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                          FSSys_FileOpen()
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_FileOpen (FS_FILE   *p_file,
                      CPU_CHAR  *name_file,
                      FS_ERR    *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_FileOpen(p_file, name_file, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                         FSSys_FilePosSet()
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_FilePosSet (FS_FILE       *p_file,
                        FS_FILE_SIZE   pos_new,
                        FS_ERR        *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_FilePosSet(p_file, pos_new, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                          FSSys_FileQuery()
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

void  FSSys_FileQuery (FS_FILE        *p_file,
                       FS_ENTRY_INFO  *p_info,
                       FS_ERR         *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_FileQuery(p_file, p_info, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                           FSSys_FileRd()
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
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_SIZE_T  FSSys_FileRd (FS_FILE     *p_file,
                          void        *p_dest,
                          CPU_SIZE_T   size,
                          FS_ERR      *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    CPU_SIZE_T  size_rd;

    size_rd = FS_FAT_FileRd(p_file, p_dest, size, p_err);
    return (size_rd);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                        FSSys_FileTruncate()
*
* Description : Truncate a file.
*
* Argument(s) : p_file      Pointer to a file.
*               ------      Argument validated by caller.
*
*               size        Size of file after truncation.
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_FileTruncate (FS_FILE       *p_file,
                          FS_FILE_SIZE   size,
                          FS_ERR        *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_FileTruncate(p_file, size, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                           FSSys_FileWr()
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
* Return(s)   : Number of bytes read, if file read successful.
*               0,                    otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_SIZE_T  FSSys_FileWr (FS_FILE     *p_file,
                          void        *p_src,
                          CPU_SIZE_T   size,
                          FS_ERR      *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    CPU_SIZE_T  size_wr;

    size_wr = FS_FAT_FileWr(p_file, p_src, size, p_err);
    return (size_wr);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         DIRECTORY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef FS_DIR_MODULE_PRESENT
/*
*********************************************************************************************************
*                                          FSSys_DirClose()
*
* Description : Close a directory.
*
* Argument(s) : p_dir       Pointer to a directory.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_DirClose (FS_DIR  *p_dir)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_DirClose(p_dir);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                           FSSys_DirOpen()
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_DirOpen (FS_DIR    *p_dir,
                     CPU_CHAR  *name_dir,
                     FS_ERR    *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_DirOpen(p_dir, name_dir, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSSys_DirRd (FS_DIR        *p_dir,
                   FS_DIR_ENTRY  *p_dir_entry,
                   FS_ERR        *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_DirRd(p_dir, p_dir_entry, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           ENTRY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FSSys_EntryAttribSet()
*
* Description : Set a file or directory's attributes.
*
* Argument(s) : p_vol       Pointer to volume on which the entry resides.
*
*               name_entry  Name of the entry.
*
*               attrib      Entry attributes to set (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_EntryAttribSet (FS_VOL    *p_vol,
                            CPU_CHAR  *name_entry,
                            FS_FLAGS   attrib,
                            FS_ERR    *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryAttribSet(p_vol, name_entry, attrib, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                         FSSys_EntryCreate()
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
*               excl        Indicates whether creation of new entry should be exclusive :
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_EntryCreate (FS_VOL       *p_vol,
                         CPU_CHAR     *name_entry,
                         FS_FLAGS      entry_type,
                         CPU_BOOLEAN   excl,
                         FS_ERR       *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryCreate(p_vol, name_entry, entry_type, excl, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif

/*
*********************************************************************************************************
*                                          FSSys_EntryDel()
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_EntryDel (FS_VOL       *p_vol,
                      CPU_CHAR     *name_entry,
                      FS_FLAGS      entry_type,
                      FS_ERR       *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryDel(p_vol, name_entry, entry_type, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                         FSSys_EntryQuery()
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

void  FSSys_EntryQuery (FS_VOL         *p_vol,
                        CPU_CHAR       *name_entry,
                        FS_ENTRY_INFO  *p_info,
                        FS_ERR         *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryQuery(p_vol, name_entry, p_info, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}


/*
*********************************************************************************************************
*                                         FSSys_EntryRename()
*
* Description : Rename a file or directory.
*
* Argument(s) : p_vol           Pointer to volume on which the entry resides.
*
*               name_entry_old  Old path of the entry.
*
*               name_entry_new  New path of the entry.
*
*               excl            Indicates whether creation of new entry should be exclusive:
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
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSSys_EntryRename (FS_VOL       *p_vol,
                         CPU_CHAR     *name_entry_old,
                         CPU_CHAR     *name_entry_new,
                         CPU_BOOLEAN   excl,
                         FS_ERR       *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryRename(p_vol, name_entry_old, name_entry_new, excl, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif


/*
*********************************************************************************************************
*                                        FSSys_EntryTimeSet()
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
void  FSSys_EntryTimeSet (FS_VOL        *p_vol,
                          CPU_CHAR      *name_entry,
                          CLK_DATE_TIME *p_time,
                          CPU_INT08U     time_type,
                          FS_ERR        *p_err)
{
#ifdef FS_FAT_MODULE_PRESENT
    FS_FAT_EntryTimeSet(p_vol, name_entry, p_time, time_type, p_err);
#else
#error  "NO SYS DRIVER PRESENT"                                 /* See 'fs_sys.c  Notes #1'.                            */
#endif
}
#endif
