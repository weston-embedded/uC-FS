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
*                                      FILE SYSTEM ENTRY ACCESS
*
* Filename : fs_entry.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_ENTRY_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>
#include  <Source/clk.h>
#include  "fs.h"
#include  "fs_entry.h"
#include  "fs_err.h"
#include  "fs_file.h"
#include  "fs_dir.h"
#include  "fs_vol.h"
#include  "fs_buf.h"
#include  "fs_def.h"
#include  "fs_dev.h"
#include  "fs_sys.h"


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

                                                                            /* ------------- HANDLER FNCTS ------------ */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void      FSEntry_AttribSetHandler(CPU_CHAR       *name_full,       /* Set a file or directory's attributes.    */
                                           FS_FLAGS        attrib,
                                           FS_ERR         *p_err);

static  void      FSEntry_CreateHandler   (CPU_CHAR       *name_full,       /* Create a file or directory.              */
                                           FS_FLAGS        entry_type,
                                           CPU_BOOLEAN     excl,
                                           FS_ERR         *p_err);

static  void      FSEntry_DelHandler      (CPU_CHAR       *name_full,       /* Delete a file or directory.              */
                                           FS_FLAGS        entry_type,
                                           FS_ERR         *p_err);
#endif

static  void      FSEntry_QueryHandler    (CPU_CHAR       *name_full,       /* Get info about a file or directory.      */
                                           FS_ENTRY_INFO  *p_info,
                                           FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void      FSEntry_RenameHandler   (CPU_CHAR       *name_full_old,   /* Rename a file or directory.              */
                                           CPU_CHAR       *name_full_new,
                                           CPU_BOOLEAN     excl,
                                           FS_ERR         *p_err);

static  void      FSEntry_TimeSetHandler  (CPU_CHAR       *name_full,       /* Set a file or directory's date/time.     */
                                           CLK_DATE_TIME  *p_time,
                                           CPU_INT08U      time_type,
                                           FS_ERR         *p_err);
#endif


                                                                            /* ---------- MISCELLANEOUS FNCTS --------- */
static  CPU_CHAR  *FSEntry_NameParseChk   (CPU_CHAR       *name_full,       /* Parse full entry name & get vol ptr.     */
                                           FS_VOL        **pp_vol,
                                           FS_ERR         *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_CHAR  *FSEntry_NameParseChkEx (CPU_CHAR       *name_full,       /* Parse full entry name & get vol ptr.     */
                                           CPU_CHAR       *name_vol,
                                           CPU_BOOLEAN    *p_dflt,
                                           FS_VOL        **pp_vol,
                                           FS_ERR         *p_err);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FSEntry_AttribSet()
*
* Description : Set a file or directory's attributes.
*
* Argument(s) : name_full   Name of the entry.
*
*               attrib      Entry attributes to set (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry attributes set successfully.
*                               FS_ERR_FILE_INVALID_ATTRIB       Entry attributes specified invalid (see Note #2c).
*                               FS_ERR_NAME_NULL                 Argument 'name_full' passed a NULL pointer.
*
*                               FS_ERR_ENTRY_ROOT_DIR            File system entry is a root directory (see Note #3).
*                               FS_ERR_VOL_INVALID_OP            Invalid operation on volume.
*
*                                                                --- RETURNED BY FSEntry_NameParseChk() ---
*                               FS_ERR_NAME_INVALID              Entry name specified invalid OR volume
*                                                                   could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                --- RETURNED BY FSSys_EntryAttribSet() ---
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) If the entry does not exist, an error is returned.
*
*               (2) Three entry attributes may be modified by this function :
*
*                       FS_ENTRY_ATTRIB_RD         Entry is readable.
*                       FS_ENTRY_ATTRIB_WR         Entry is writeable.
*                       FS_ENTRY_ATTRIB_HIDDEN     Entry is hidden from user-level processes.
*
*                   (a) An attribute will be cleared if its flag is not OR'd into 'attrib'.
*
*                   (b) An attribute will be set     if its flag is     OR'd into 'attrib'.
*
*                   (c) If another flag besides these is set, then an error will be returned.
*
*               (3) The attributes of the root directory may NOT be set.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_AttribSet (CPU_CHAR  *name_full,
                         FS_FLAGS   attrib,
                         FS_ERR    *p_err)
{
    CPU_CHAR  *name_full_temp;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
                                                                /* Validate attrib.                                     */
    if (DEF_BIT_IS_SET_ANY(attrib, ~(FS_ENTRY_ATTRIB_RD | FS_ENTRY_ATTRIB_WR | FS_ENTRY_ATTRIB_HIDDEN)) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ATTRIB;
        return;
    }
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#else
    name_full_temp = name_full;
#endif



                                                                /* -------------------- SET ATTRIB -------------------- */
    FSEntry_AttribSetHandler(name_full_temp,
                             attrib,
                             p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                           FSEntry_Copy()
*
* Description : Copy a file.
*
* Argument(s) : name_full_src   Name of the source      file.
*
*               name_full_dest  Name of the destination file.
*
*               excl            Indicates whether creation of new entry should be exclusive :
*
*                                   DEF_YES, if the entry will be copied ONLY if 'name_full_dest' does
*                                                not exist.
*                                   DEF_NO,  if the entry will be copied even if 'name_full_dest' does
*                                                exist.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      File copied successfully.
*                                   FS_ERR_NAME_NULL                 Argument 'name_full_src'/'name_full_dest'
*                                                                        passed a NULL pointer.
*                                   FS_ERR_VOL_INVALID_OP            Invalid operation on volume.
*
*                                                                    --- RETURNED BY FSEntry_NameParseChkEx() ---
*                                                                    -------- RETURNED BY FS_NameParse() --------
*                                   FS_ERR_NAME_INVALID              Source or destination name specified
*                                                                        invalid OR volume could not be
*                                                                        found.
*                                   FS_ERR_NAME_PATH_TOO_LONG        Source or destination name specified
*                                                                        too long.
*                                   FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                                   FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                    --------- RETURNED BY FSFile_Open() --------
*                                                                    --------- RETURNED BY FSFile_Rd() --------
*                                                                    --------- RETURNED BY FSFile_Wr() --------
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_ENTRY_EXISTS              File system entry exists.
*                                   FS_ERR_ENTRY_NOT_FILE            File system entry NOT a file.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'name_full_src' must be an existing file.
*
*               (2) If 'excl' is DEF_NO, 'name_full_dest' must either not exist or be an existing file;
*                   it may not be an existing directory.  If 'excl' is DEF_YES, 'name_full_dest' must
*                   not exist.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_Copy (CPU_CHAR     *name_full_src,
                    CPU_CHAR     *name_full_dest,
                    CPU_BOOLEAN   excl,
                    FS_ERR       *p_err)
{
    FS_BUF       *p_buf;
    CPU_BOOLEAN   done;
    FS_ERR        err;
    FS_FILE      *p_file_dest;
    FS_FILE      *p_file_src;
    CPU_SIZE_T    file_rd_len;
    FS_FLAGS      mode;
    CPU_BOOLEAN   no_err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full_src == (CPU_CHAR *)0) {                       /* Validate src name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (name_full_dest == (CPU_CHAR *)0) {                      /* Validate dest name ptr.                              */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* --------------------- COPY FILE -------------------- */
                                                                /* Open src file.                                       */
    p_file_src = FSFile_Open(name_full_src, FS_FILE_ACCESS_MODE_RD, p_err);
    if (p_file_src == DEF_NULL) {
        return;
    } else {
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }

    if (excl == DEF_YES) {
        mode =  FS_FILE_ACCESS_MODE_WR | FS_FILE_ACCESS_MODE_CREATE | FS_FILE_ACCESS_MODE_EXCL;
    } else {
        mode =  FS_FILE_ACCESS_MODE_WR | FS_FILE_ACCESS_MODE_CREATE | FS_FILE_ACCESS_MODE_TRUNCATE;
    }
    p_file_dest = FSFile_Open(name_full_dest, mode, p_err);     /* Open dest file.                                      */
    if (p_file_dest == DEF_NULL) {
        FSFile_Close(p_file_src, &err);
        return;
    }

    p_buf = FSBuf_Get(DEF_NULL);
    if (p_buf == DEF_NULL) {
        FSFile_Close(p_file_src, &err);
        FSFile_Close(p_file_dest, &err);
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

                                                                /* Copy file.                                           */
    done = DEF_NO;
    do {
        file_rd_len = FSFile_Rd(p_file_src,
                                p_buf->DataPtr,
                                p_buf->Size,
                                p_err);

        no_err = (*p_err == FS_ERR_NONE);

        if ((file_rd_len > 0u) && no_err) {
           (void)FSFile_Wr(p_file_dest,
                           p_buf->DataPtr,
                           file_rd_len,
                           p_err);

            if ((file_rd_len != p_buf->Size) || (*p_err != FS_ERR_NONE)) {
                done = DEF_YES;
            }

        } else {
            done = DEF_YES;
        }
    } while (done == DEF_NO);

    FSFile_Close(p_file_src, &err);
    FSFile_Close(p_file_dest, &err);
    FSBuf_Free  (p_buf);
}
#endif


/*
*********************************************************************************************************
*                                          FSEntry_Create()
*
* Description : Create a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*
*               entry_type  Indicates whether the new entry shall be a directory or a file :
*
*                               FS_ENTRY_TYPE_DIR,  if the entry shall be a directory.
*                               FS_ENTRY_TYPE_FILE, if the entry shall be a file.
*
*               excl        Indicates whether creation of new entry shall be exclusive :
*
*                               DEF_YES, if the entry shall be created ONLY if 'name_full' does not exist.
*                               DEF_NO,  if the entry shall be created even if 'name_full' does     exist.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry created successfully.
*                               FS_ERR_NAME_NULL                 Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_ENTRY_TYPE_INVALID        Argument 'entry_type' is invalid.
*
*                               FS_ERR_VOL_INVALID_OP            Invalid operation on volume.
*
*                                                                --- RETURNED BY FSEntry_NameParseChk() ---
*                               FS_ERR_NAME_INVALID              Entry name specified invalid OR volume
*                                                                    could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                ----- RETURNED BY FSSys_EntryCreate() ----
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
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
* Note(s)     : (1) If the entry exists & is a file, 'dir' is DEF_NO & 'excl' is DEF_NO, then the
*                   existing entry will be truncated.  If the entry exists & is a directory & 'dir'
*                   is DEF_YES, then no change will be made to the file system.
*
*               (2) If the entry exists & is a directory, 'dir' is DEF_NO & 'excl' is DEF_NO, then
*                   no change will be made to the file system.  Similarly, if the entry exists & is
*                   a file & 'dir' is DEF_YES, then no change will be made to the file system.
*
*               (3) The root directory may NOT be created.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_Create (CPU_CHAR     *name_full,
                      FS_FLAGS      entry_type,
                      CPU_BOOLEAN   excl,
                      FS_ERR       *p_err)
{
    CPU_CHAR  *name_full_temp;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (entry_type != FS_ENTRY_TYPE_FILE) {
        if (entry_type != FS_ENTRY_TYPE_DIR) {
           *p_err = FS_ERR_ENTRY_TYPE_INVALID;
        }
    }
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#else
    name_full_temp = name_full;
#endif



                                                                /* ------------------- CREATE ENTRY ------------------- */
    FSEntry_CreateHandler(name_full_temp,
                          entry_type,
                          excl,
                          p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
}
#endif

/*
*********************************************************************************************************
*                                            FSEntry_Del()
*
* Description : Delete a file or directory.
*
* Argument(s) : name_full   Name of the entry.
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
*                               FS_ERR_NONE                     Entry deleted successfully.
*                               FS_ERR_NAME_NULL                Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_ENTRY_ROOT_DIR           File system entry is a root directory (see Note #3).
*                               FS_ERR_VOL_INVALID_OP           Invalid operation on volume.
*                               FS_ERR_ENTRY_OPEN               Open entry cannot be deleted.
*
*                                                                ------ RETURNED BY FSFile_IsOpen() -------
*                                                                ----------- AND BY FSDir_IsOpen() --------
*                               FS_ERR_NONE                     File/Dir opened successfully.
*                               FS_ERR_NULL_PTR                 Argument 'name_full' passed a NULL ptr.
*                               FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                               FS_ERR_ENTRY_NOT_FILE           Entry not a file or dir.
*                               FS_ERR_NAME_INVALID             Invalid name or path.
*                               FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*
*                                                                --- RETURNED BY FSEntry_NameParseChk() ---
*                               FS_ERR_NAME_INVALID              Entry name specified invalid OR volume
*                                                                    could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                ------ RETURNED BY FSSys_EntryDel() ------
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_FILE            File system entry NOT a file.
*                               FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                               FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) When a file is removed, the space occupied by the file is freed and shall no longer
*                   be accessible.
*
*               (2) (a) A directory can be removed only if it is an empty directory.
*
*                   (b) The root directory cannot be deleted.
*
*               (3) See 'fs_remove()  Note(s)'.
*
*               (4) See 'fs_rmdir()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_Del (CPU_CHAR     *name_full,
                   FS_FLAGS      entry_type,
                   FS_ERR       *p_err)
{
    CPU_CHAR  *name_full_temp;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (((entry_type &  FS_ENTRY_TYPE_ANY) == 0) ||
        ((entry_type & ~FS_ENTRY_TYPE_ANY) != 0)) {
       *p_err = FS_ERR_ENTRY_TYPE_INVALID;
        return;
    }
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#else
    name_full_temp = name_full;
#endif



                                                                /* --------------------- DEL ENTRY -------------------- */
    FSEntry_DelHandler(name_full_temp,
                       entry_type,
                       p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                           FSEntry_Query()
*
* Description : Get information about a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*
*               p_info      Pointer to structure that will receive the entry information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Entry information gotten successfully.
*                               FS_ERR_NAME_NULL                 Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_NULL_PTR                  Argument 'p_info' passed a NULL pointer.
*
*                                                                ---- RETURNED BY FSEntry_NameParseChk() ----
*                               FS_ERR_NAME_INVALID              Entry name specified invalid OR volume
*                                                                    could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                ------ RETURNED BY FSSys_EntryQuery() ------
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

void  FSEntry_Query (CPU_CHAR       *name_full,
                     FS_ENTRY_INFO  *p_info,
                     FS_ERR         *p_err)

{
    CPU_CHAR  *name_full_temp;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_info == (FS_ENTRY_INFO *)0) {                         /* Validate info ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#else
    name_full_temp = name_full;
#endif


                                                                /* ------------------ GET ENTRY INFO ------------------ */
    FSEntry_QueryHandler(name_full_temp,
                         p_info,
                         p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
}


/*
*********************************************************************************************************
*                                          FSEntry_Rename()
*
* Description : Rename a file or directory.
*
* Argument(s) : name_full_old   Old path of the entry.
*
*               name_full_new   New path of the entry.
*
*               excl            Indicates whether creation of new entry should be exclusive (see Note #3):
*
*                                   DEF_YES, if the entry shall be renamed ONLY if 'name_full_new'
*                                                does not exist.
*                                   DEF_NO,  if the entry shall be renamed even if 'name_full_new'
*                                                does exist.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Entry renamed successfully.
*                                   FS_ERR_NAME_NULL                 Argument 'name_full_old'/'name_full_new'
*                                                                        passed a NULL pointer.
*                                   FS_ERR_ENTRIES_VOLS_DIFF         Paths specify file system entries
*                                                                        on different vols (see Note #1).
*                                   FS_ERR_ENTRY_ROOT_DIR            File system entry is a root directory (see Note #4).
*                                   FS_ERR_VOL_INVALID_OP            Invalid operation on volume.
*
*                                                                    --- RETURNED BY FSEntry_NameParseChkEx() ---
*                                   FS_ERR_NAME_INVALID              Entry name specified invalid OR
*                                                                        volume could not be found.
*                                   FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                                   FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                                   FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                    ------ RETURNED BY FSSys_EntryRename() -----
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_ENTRIES_TYPE_DIFF         Paths do not both specify files
*                                                                        OR directories (see Notes #2b1, #2c1).
*                                   FS_ERR_ENTRY_EXISTS              File system entry exists.
*                                   FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) If 'name_full_old' & 'name_full_new' specify entries on different volumes, then
*                   'name_full_old' MUST specify a file.  If 'name_full_old' specifies a directory, an
*                   error will be returned.
*
*               (2) (a) If 'name_full_old' and 'name_full_new' specify the same entry, the volume
*                       will not be modified and no error will be returned.
*
*                   (b) If 'name_full_old' specifies a file:
*
*                       (1) ... 'name_full_new' must NOT specify a directory
*
*                       (2) ... if 'excl' is DEF_NO and 'name_full_new' is a file, 'name_full_new'
*                               will be removed.
*
*                   (c) If 'name_full_old' specifies a directory:
*
*                       (1) ... 'name_full_new' must NOT specify a file
*
*                       (2) ... if 'excl' is DEF_NO and 'name_full_new' is a directory, 'name_full_new'
*                               MUST be empty; if so, it will be removed.
*
*               (3) If 'excl' is DEF_NO, 'name_full_new' must not exist.
*
*               (4) The root directory may NOT be renamed.
*
*               (5) See 'fs_rename()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_Rename (CPU_CHAR     *name_full_old,
                      CPU_CHAR     *name_full_new,
                      CPU_BOOLEAN   excl,
                      FS_ERR       *p_err)
{
    CPU_CHAR  *name_full_temp_old;
    CPU_CHAR  *name_full_temp_new;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full_old == (CPU_CHAR *)0) {                       /* Validate old name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (name_full_new == (CPU_CHAR *)0) {                       /* Validate new name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp_old = FS_WorkingDirPathForm(name_full_old,   /* Try to form path.                                    */
                                               p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


    name_full_temp_new = FS_WorkingDirPathForm(name_full_new,   /* Try to form path.                                    */
                                               p_err);
    if (*p_err != FS_ERR_NONE) {
        if (name_full_temp_old != name_full_old) {
            FS_WorkingDirObjFree(name_full_temp_old);
        }
        return;
    }
#else
    name_full_temp_old = name_full_old;
    name_full_temp_new = name_full_new;
#endif

                                                                /* ------------------- RENAME ENTRY ------------------- */
    FSEntry_RenameHandler(name_full_temp_old,
                          name_full_temp_new,
                          excl,
                          p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp_new != name_full_new) {
        FS_WorkingDirObjFree(name_full_temp_new);
    }
    if (name_full_temp_old != name_full_old) {
        FS_WorkingDirObjFree(name_full_temp_old);
    }
#endif
}
#endif



/*
*********************************************************************************************************
*                                          FSEntry_TimeSet()
*
* Description : Set a file or directory's date/time.
*
* Argument(s) : name_full   Name of the entry.
*
*               p_time      Pointer to date/time.
*
*               time_type   Flag to indicate which Date/Time should be set
*
*                               FS_DATE_TIME_CREATE
*                               FS_DATE_TIME_MODIFY
*                               FS_DATE_TIME_ACCESS
*                               FS_DATE_TIME_ALL
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                         Entry date/time set successfully.
*                               FS_ERR_NAME_NULL                    Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_NULL_PTR                     Argument 'p_time' passed a NULL pointer.
*                               FS_ERR_ENTRY_ROOT_DIR               File system entry is a root directory (see Note #1).
*                               FS_ERR_FILE_INVALID_DATE_TIME       Date/time specified invalid.
*                               FS_ERR_VOL_INVALID_OP               Invalid operation on volume.
*                               FS_ERR_FILE_INVALID_DATE_TIME_TYPE  time_type flag specified is invalid.
*
*                                                                   ---- RETURNED BY FSEntry_NameParseChk() ----
*                               FS_ERR_NAME_INVALID                 Entry name specified invalid OR volume
*                                                                       could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG           Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN                 Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED              Volume was not mounted.
*
*                                                                   ----- RETURNED BY FSSys_EntryTimeSet() -----
*                               FS_ERR_BUF_NONE_AVAIL               Buffer not available.
*                               FS_ERR_DEV                          Device access error.
*                               FS_ERR_ENTRY_NOT_FOUND              File system entry NOT found
*                               FS_ERR_ENTRY_PARENT_NOT_FOUND       Entry parent NOT found.
*                               FS_ERR_ENTRY_PARENT_NOT_DIR         Entry parent NOT a directory.
*                               FS_ERR_NAME_INVALID                 Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) The date/time of the root directory may NOT be set.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSEntry_TimeSet (CPU_CHAR      *name_full,
                       CLK_DATE_TIME *p_time,
                       CPU_INT08U     time_type,
                       FS_ERR        *p_err)
{
    CPU_CHAR     *name_full_temp;
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN   valid;
#endif


                                                                /* ------------------- VALIDATE ARGS ------------------ */
#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_full == (CPU_CHAR *)0) {                           /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }

    if (p_time == (CLK_DATE_TIME *)0) {                         /* Validate date/time ptr.                              */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }

    valid = Clk_IsUnixDateTimeValid(p_time);                    /* Validate date/time.                                  */
    if (valid == DEF_NO) {
       *p_err = FS_ERR_FILE_INVALID_DATE_TIME;
        return;
    }

    valid = FS_IS_VALID_DATE_TIME_TYPE(time_type);              /* Validate date/time type flag                         */
    if (valid == DEF_NO) {
       *p_err = FS_ERR_FILE_INVALID_DATE_TIME_TYPE;
        return;
    }
#endif

                                                                /* ------------------ FORM FULL PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    name_full_temp = FS_WorkingDirPathForm(name_full, p_err);   /* Try to form path.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }
#else
    name_full_temp = name_full;
#endif


                                                                /* ------------------- SET DATE/TIME ------------------ */
    FSEntry_TimeSetHandler(name_full_temp,
                           p_time,
                           time_type,
                           p_err);

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    if (name_full_temp != name_full) {
        FS_WorkingDirObjFree(name_full_temp);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     FSEntry_AttribSetHandler()
*
* Description : Set a file or directory's attributes.
*
* Argument(s) : name_full   Name of the entry.
*               ---------   Argument validated by caller.
*
*               attrib      Entry attributes to set.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_NONE    Entry attributes set successfully.
*                               See 'FSEntry_AttribSet()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSEntry_AttribSetHandler (CPU_CHAR  *name_full,
                                        FS_FLAGS   attrib,
                                        FS_ERR    *p_err)
{
    CPU_CHAR     *name_entry;
    FS_VOL       *p_vol;
    CPU_BOOLEAN   file_open;
    CPU_BOOLEAN   dir_open;
    CPU_BOOLEAN   entry_open;
    CPU_BOOLEAN   mode_ro;
    FS_FLAGS      mode;


                                                                /* --------------- VERIFY IF ENTRY OPEN --------------- */
    file_open = FSFile_IsOpen(name_full, &mode, p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_ENTRY_NOT_FILE)) {
        return;
    }

#ifdef   FS_DIR_MODULE_PRESENT
    dir_open  = FSDir_IsOpen(name_full, p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_ENTRY_NOT_DIR)) {
        return;
    }
#else
    dir_open = DEF_NO;
#endif

    entry_open = dir_open || file_open;
    if (entry_open == DEF_YES) {
       *p_err = FS_ERR_ENTRY_OPEN;
        return;
    }

                                                                /* ---------------------- GET VOL --------------------- */
    name_entry = FSEntry_NameParseChk( name_full,
                                      &p_vol,
                                       p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if ((p_vol      == (FS_VOL   *)0) ||                        /* Rtn err if no vol found ...                          */
        (name_entry == (CPU_CHAR *)0)) {                        /*                         ... or if name illegal.      */
        return;
    }

                                                                /* Chk vol mode.                                        */
    mode_ro = DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR);
    if (mode_ro == DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }


                                                                /* -------------------- SET ATTRIB -------------------- */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {           /* Rtn err if entry specifies root dir (see Note #3).   */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }

    FSSys_EntryAttribSet(p_vol,
                         name_entry,
                         attrib,
                         p_err);


                                                                /* ----------------- RELEASE VOL & RTN ---------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                       FSEntry_CreateHandler()
*
* Description : Create a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*               ----------  Argument validated by caller.
*
*               entry_type  Indicates whether the new entry shall be a directory or a file :
*
*                               FS_ENTRY_TYPE_DIR,  if the entry shall be a directory.
*                               FS_ENTRY_TYPE_FILE, if the entry shall be a file.
*
*               excl        Indicates whether creation of new entry shall be exclusive :
*
*                               DEF_YES, if the entry shall be created ONLY if 'name_full' does not exist.
*                               DEF_NO,  if the entry shall be created even if 'name_full' does     exist.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Entry created successfully.
*                               See 'FSEntry_Create()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSEntry_CreateHandler (CPU_CHAR     *name_full,
                                     FS_FLAGS      entry_type,
                                     CPU_BOOLEAN   excl,
                                     FS_ERR       *p_err)
{
    CPU_CHAR  *name_entry;
    FS_VOL    *p_vol;


                                                                /* ---------------------- GET VOL --------------------- */
    name_entry = FSEntry_NameParseChk( name_full,
                                      &p_vol,
                                       p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if ((p_vol      == (FS_VOL   *)0) ||                        /* Rtn err if no vol found ...                          */
        (name_entry == (CPU_CHAR *)0)) {                        /*                         ... or if name illegal.      */
        return;
    }

                                                                /* Chk vol mode.                                        */
    if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR)== DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }


                                                                /* ------------------- CREATE ENTRY ------------------- */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {           /* Rtn err if entry specifies root dir (see Note #3).   */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }

    FSSys_EntryCreate(p_vol,
                      name_entry,
                      entry_type,
                      excl,
                      p_err);


                                                                /* ----------------- RELEASE VOL & RTN ---------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif



/*
*********************************************************************************************************
*                                        FSEntry_DelHandler()
*
* Description : Delete a file or directory.
*
* Argument(s) : name_full       Name of the entry.
*               ---------       Argument validated by caller.
*
*               entry_type      Indicates whether the new entry can be a directory / a file :
*               ----------      Argument validated by caller.
*
*                                   FS_ENTRY_TYPE_DIR,  if the entry can be a directory.
*                                   FS_ENTRY_TYPE_FILE, if the entry can be a file.
*                                   FS_ENTRY_TYPE_ANY,  if the entry can be any type
*
*                               These values can be OR'd to allow multiple types.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                               FS_ERR_NONE                     Entry deleted successfully.
*                               FS_ERR_NAME_NULL                Argument 'name_full' passed a NULL pointer.
*                               FS_ERR_ENTRY_ROOT_DIR           File system entry is a root directory.
*                               FS_ERR_VOL_INVALID_OP           Invalid operation on volume.
*                               FS_ERR_ENTRY_OPEN               Open entry cannot be deleted.
*
*                                                                ------ RETURNED BY FSFile_IsOpen() -------
*                                                                ----------- AND BY FSDir_IsOpen() --------
*                               FS_ERR_NONE                     File/Dir opened successfully.
*                               FS_ERR_NULL_PTR                 Argument 'name_full' passed a NULL ptr.
*                               FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                               FS_ERR_ENTRY_NOT_FILE           Entry not a file or dir.
*                               FS_ERR_NAME_INVALID             Invalid name or path.
*                               FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*
*                                                                --- RETURNED BY FSEntry_NameParseChk() ---
*                               FS_ERR_NAME_INVALID              Entry name specified invalid OR volume
*                                                                    could not be found.
*                               FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                               FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                               FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                ------ RETURNED BY FSSys_EntryDel() ------
*                               FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                               FS_ERR_DEV                       Device access error.
*                               FS_ERR_ENTRY_NOT_FILE            File system entry NOT a file.
*                               FS_ERR_ENTRY_NOT_DIR             File system entry NOT a directory.
*                               FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
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
static  void  FSEntry_DelHandler (CPU_CHAR  *name_full,
                                  FS_FLAGS   entry_type,
                                  FS_ERR    *p_err)
{
    CPU_CHAR     *name_entry;
    FS_VOL       *p_vol;
    FS_FLAGS      mode;
    FS_FLAGS      del_entry_type;
    CPU_BOOLEAN   file_open;
    CPU_BOOLEAN   dir_open;
    CPU_BOOLEAN   entry_open;
    CPU_BOOLEAN   is_file;
#ifdef   FS_DIR_MODULE_PRESENT
    CPU_BOOLEAN   is_dir;
#endif


                                                                /* --------------- VERIFY IF ENTRY OPEN --------------- */
    dir_open  = DEF_NO;
    file_open = DEF_NO;

    is_file = DEF_BIT_IS_SET(entry_type, FS_ENTRY_TYPE_FILE);
    if (is_file == DEF_YES) {
        file_open = FSFile_IsOpen(name_full, &mode, p_err);
        if ((*p_err != FS_ERR_NONE) &&
            (*p_err != FS_ERR_ENTRY_NOT_FILE)) {
            return;
        }
    }
#ifdef   FS_DIR_MODULE_PRESENT
    is_dir = DEF_BIT_IS_SET(entry_type, FS_ENTRY_TYPE_DIR);
    if (is_dir == DEF_YES) {
        dir_open  = FSDir_IsOpen(name_full, p_err);
    }
#endif
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    del_entry_type = entry_type;

    if (DEF_BIT_IS_SET(del_entry_type, FS_ENTRY_TYPE_ANY)) {
        if (dir_open  == DEF_YES) {                             /* Clr dir  bit if dir  is open.                        */
            DEF_BIT_CLR(del_entry_type, FS_ENTRY_TYPE_DIR);
        }
        if (file_open == DEF_YES) {                             /* Clr file bit if file is open.                        */
            DEF_BIT_CLR(del_entry_type, FS_ENTRY_TYPE_FILE);
        }

    } else {
        entry_open = dir_open || file_open;
        if (entry_open == DEF_YES) {
           *p_err = FS_ERR_ENTRY_OPEN;
            return;
        }
    }

                                                                /* ---------------------- GET VOL --------------------- */
    name_entry = FSEntry_NameParseChk(name_full,
                                     &p_vol,
                                      p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (( p_vol      == (FS_VOL   *)0) ||                       /* Rtn err if no vol found ...                          */
        ( name_entry == (CPU_CHAR *)0) ||                       /*                         ... or if name illegal ...   */
        (*p_err      !=  FS_ERR_NONE)) {                        /*                         ... or if there is an error. */
        return;
    }

                                                                /* Chk vol mode.                                        */
    if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR)== DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }



                                                                /* --------------------- DEL ENTRY -------------------- */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {           /* Rtn err if entry specifies root dir (see Note #2b).  */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }

    FSSys_EntryDel(p_vol,
                   name_entry,
                   del_entry_type,
                   p_err);

    if (DEF_BIT_IS_SET(entry_type, FS_ENTRY_TYPE_ANY)) {        /* Correct rtn'd err if type is FS_ENTRY_TYPE_ANY.      */
        if (*p_err == FS_ERR_ENTRY_NOT_DIR) {
            *p_err  = FS_ERR_ENTRY_OPEN;
        }
        if (*p_err == FS_ERR_ENTRY_NOT_FILE) {
            *p_err  = FS_ERR_ENTRY_OPEN;
        }
    }


                                                                /* ----------------- RELEASE VOL & RTN ---------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                       FSEntry_QueryHandler()
*
* Description : Get information about a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*               ----------  Argument validated by caller.
*
*               p_info      Pointer to structure that will receive the entry information.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Entry information gotten successfully.
*                               See 'FSEntry_Query()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSEntry_QueryHandler (CPU_CHAR       *name_full,
                                    FS_ENTRY_INFO  *p_info,
                                    FS_ERR         *p_err)

{
    CPU_CHAR  *name_entry;
    FS_VOL    *p_vol;


                                                                /* ---------------------- GET VOL --------------------- */
    name_entry = FSEntry_NameParseChk( name_full,
                                      &p_vol,
                                       p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (( p_vol      == (FS_VOL   *)0) ||                       /* Rtn err if no vol found ...                          */
        ( name_entry == (CPU_CHAR *)0) ||                       /*                         ... or if name illegal ...   */
        (*p_err      !=  FS_ERR_NONE)) {                        /*                         ... or if there is an error. */
        return;
    }



                                                                /* ------------------ GET ENTRY INFO ------------------ */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {           /* Root dir.                                            */
        FSVol_ReleaseUnlock(p_vol);
        p_info->Attrib         = FS_ENTRY_ATTRIB_DIR | FS_ENTRY_ATTRIB_DIR_ROOT;
        p_info->Size           = 0u;
        p_info->DateTimeCreate = FS_TIME_TS_INVALID;
        p_info->DateAccess     = FS_TIME_TS_INVALID;
        p_info->DateTimeWr     = FS_TIME_TS_INVALID;
        p_info->BlkCnt         = 0u;
        p_info->BlkSize        = 0u;
       *p_err = FS_ERR_NONE;
        return;
    }

    FSSys_EntryQuery(p_vol,
                     name_entry,
                     p_info,
                     p_err);



                                                                /* ----------------- RELEASE VOL & RTN ---------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                       FSEntry_RenameHandler()
*
* Description : Rename a file or directory.
*
* Argument(s) : name_full_old   Old path of the entry.
*               -------------   Argument validated by caller.
*
*               name_full_new   New path of the entry.
*               -------------   Argument validated by caller.
*
*               excl            Indicates whether creation of new entry should be exclusive (see Note #3):
*
*                                   DEF_YES, if the entry shall be renamed ONLY if 'name_full_new'
*                                                does not exist.
*                                   DEF_NO,  if the entry shall be renamed even if 'name_full_new'
*                                                does exist.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                      Entry renamed successfully.
*                                   FS_ERR_NAME_NULL                 Argument 'name_full_old'/'name_full_new'
*                                                                        passed a NULL pointer.
*                                   FS_ERR_ENTRIES_VOLS_DIFF         Paths specify file system entries
*                                                                        on different vols (see Note #1).
*                                   FS_ERR_ENTRY_ROOT_DIR            File system entry is a root directory (see Note #4).
*                                   FS_ERR_VOL_INVALID_OP            Invalid operation on volume.
*
*                                                                   -------- RETURNED BY FSFile_IsOpen() --------
*                                                                   ------------- AND BY FSDir_IsOpen() ---------
*                                   FS_ERR_NONE                     File/Dir opened successfully.
*                                   FS_ERR_NULL_PTR                 Argument 'name_full' passed a NULL ptr.
*                                   FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                                   FS_ERR_ENTRY_NOT_FILE           Entry not a file or dir.
*                                   FS_ERR_NAME_INVALID             Invalid name or path.
*                                   FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*
*                                                                    --- RETURNED BY FSEntry_NameParseChkEx() ---
*                                   FS_ERR_NAME_INVALID              Entry name specified invalid OR
*                                                                        volume could not be found.
*                                   FS_ERR_NAME_PATH_TOO_LONG        Entry name specified too long.
*                                   FS_ERR_VOL_NOT_OPEN              Volume was not open.
*                                   FS_ERR_VOL_NOT_MOUNTED           Volume was not mounted.
*
*                                                                    ------ RETURNED BY FSSys_EntryRename() -----
*                                   FS_ERR_BUF_NONE_AVAIL            Buffer not available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_ENTRIES_TYPE_DIFF         Paths do not both specify files
*                                                                        OR directories (see Notes #2b1, #2c1).
*                                   FS_ERR_ENTRY_EXISTS              File system entry exists.
*                                   FS_ERR_ENTRY_NOT_EMPTY           File system entry NOT empty.
*                                   FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY             File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID              Invalid file name or path.
*
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSEntry_RenameHandler (CPU_CHAR     *name_full_old,
                                     CPU_CHAR     *name_full_new,
                                     CPU_BOOLEAN   excl,
                                     FS_ERR       *p_err)
{
    CPU_INT16S   cmp_val;
    CPU_BOOLEAN  dflt;
    CPU_BOOLEAN  diff;
    CPU_BOOLEAN  dir_open;
    CPU_BOOLEAN  file_open;
    CPU_BOOLEAN  entry_open;
    FS_FLAGS     mode;
    CPU_SIZE_T   name_entry_len;
    CPU_SIZE_T   file_name_len;
    CPU_CHAR     name_vol_old[FS_CFG_MAX_VOL_NAME_LEN + 1u];
    CPU_CHAR     name_vol_new[FS_CFG_MAX_VOL_NAME_LEN + 1u];
    CPU_CHAR    *name_entry_old;
    CPU_CHAR    *name_entry_new;
    CPU_CHAR    *file_name;
    FS_VOL      *p_vol;


                                                                /* --------------- VERIFY IF ENTRY OPEN --------------- */
    file_open = FSFile_IsOpen(name_full_old, &mode, p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_ENTRY_NOT_FILE)) {
        return;
    }

#ifdef   FS_DIR_MODULE_PRESENT
    dir_open  = FSDir_IsOpen(name_full_old, p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_ENTRY_NOT_DIR)) {
        return;
    }
#else
    dir_open  = DEF_NO;
#endif

    entry_open = dir_open || file_open;
    if (entry_open == DEF_YES) {
       *p_err = FS_ERR_ENTRY_OPEN;
        return;
    }

                                                                /* ----------- VERIFY IF DEST ENTRY IS OPEN ----------- */
    if (excl == DEF_NO) {
        file_open = FSFile_IsOpen(name_full_new, &mode, p_err);
        switch (*p_err) {
            case FS_ERR_NONE:
                 break;


            case FS_ERR_ENTRY_NOT_FILE:
            case FS_ERR_ENTRY_NOT_FOUND:
                *p_err = FS_ERR_NONE;
                 break;


            default:
                 return;
        }

#ifdef   FS_DIR_MODULE_PRESENT
        dir_open  = FSDir_IsOpen(name_full_new, p_err);
        switch (*p_err) {
            case FS_ERR_NONE:
                 break;


            case FS_ERR_ENTRY_NOT_DIR:
            case FS_ERR_ENTRY_NOT_FOUND:
                *p_err = FS_ERR_NONE;
                 break;


            default:
                 return;
        }
#else
        dir_open  = DEF_NO;
#endif

        entry_open = dir_open || file_open;
        if (entry_open == DEF_YES) {
           *p_err = FS_ERR_ENTRY_OPEN;
            return;
        }

    }



                                                                /* ------------------- GET DEST VOL ------------------- */
    name_entry_new = FS_PathParse(name_full_new,
                                  name_vol_new,
                                  p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    if (name_entry_new == (CPU_CHAR *)0) {                      /* Rtn err if name illegal.                             */
       *p_err =  FS_ERR_NAME_INVALID;
        return;
    }

    if (*name_entry_new != FS_CHAR_PATH_SEP) {                  /* Require init path sep char.                          */
        *p_err = FS_ERR_NAME_INVALID;
         return;
    }
    name_entry_new++;                                           /* Ignore  init path sep char.                          */

    if (name_entry_new[0] == (CPU_CHAR)ASCII_CHAR_NULL) {       /* Rtn err if entry specifies root dir (see Note #4).   */
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }

    name_entry_len = Str_Len_N(name_entry_new, FS_CFG_MAX_PATH_NAME_LEN + 1u);
    if (name_entry_len > FS_CFG_MAX_PATH_NAME_LEN) {            /* Rtn err if file name is too long.                    */
       *p_err = FS_ERR_NAME_PATH_TOO_LONG;
        return;
    }
                                                                /* Parse name to evaluate length of the base name       */
    file_name     = Str_Char_Last_N(name_entry_new, FS_CFG_MAX_PATH_NAME_LEN,      (CPU_CHAR)ASCII_CHAR_REVERSE_SOLIDUS);
    file_name_len = Str_Len_N(      file_name,  FS_CFG_MAX_FILE_NAME_LEN + 1u);
    if (file_name_len == 0u) {
        file_name_len = name_entry_len;
    }

    if (file_name_len > FS_CFG_MAX_FILE_NAME_LEN) {
        *p_err = FS_ERR_NAME_BASE_TOO_LONG;                     /* Rtn err if base name is too long.                    */
        return;
    }

                                                                /* -------------------- GET SRC VOL ------------------- */
    name_entry_old = FSEntry_NameParseChkEx( name_full_old,
                                             name_vol_old,
                                            &dflt,
                                            &p_vol,
                                             p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (( p_vol          == (FS_VOL   *)0) ||                   /* Rtn err if no vol found ...                          */
        ( name_entry_old == (CPU_CHAR *)0) ||                   /*                         ... or if name illegal ...   */
        (*p_err         !=  FS_ERR_NONE)) {                     /*                         ... or if there is an error. */
        return;
    }

    if (name_entry_old[0] == (CPU_CHAR)ASCII_CHAR_NULL) {       /* Rtn err if entry specifies root dir (see Note #4).   */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }



                                                                /* ----------------- HANDLE DIFF VOLS ----------------- */
    diff = DEF_NO;
    if (name_vol_new[0] == (CPU_CHAR)ASCII_CHAR_NULL) {         /* Vols diff if new is dflt ...                         */
        if (dflt == DEF_NO) {                                   /*                          ... but old is not.         */
            diff = DEF_YES;
        }
    } else {
        cmp_val = Str_Cmp_N(name_vol_old, name_vol_new, FS_CFG_MAX_VOL_NAME_LEN);
        if (cmp_val != 0) {                                     /* Old & new vol names are diff.                        */
            diff = DEF_YES;
        }
    }

    if (diff == DEF_YES) {                                      /* Vols are diff ...                                    */
        FSVol_ReleaseUnlock(p_vol);

        FSEntry_Copy(name_full_old,                             /*               ... copy file ...                      */
                     name_full_new,
                     excl,
                     p_err);

        if (*p_err != FS_ERR_NONE) {
            if (*p_err == FS_ERR_ENTRY_NOT_FILE) {
                *p_err =  FS_ERR_ENTRIES_VOLS_DIFF;
            }
            return;
        }

        FSEntry_Del(name_full_old,                              /*                             ... del old file.        */
                    FS_ENTRY_TYPE_FILE,
                    p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }



    } else {                                                    /* ------------------ HANDLE SAME VOL ----------------- */
                                                                /* Chk vol mode ...                                     */
        if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR)== DEF_YES) {
            FSVol_ReleaseUnlock(p_vol);
           *p_err = FS_ERR_VOL_INVALID_OP;
            return;
        }

        FSSys_EntryRename(p_vol,                                /*              ... rename file.                        */
                          name_entry_old,
                          name_entry_new,
                          excl,
                          p_err);
        if (*p_err == FS_ERR_ENTRIES_SAME) {
            *p_err =  FS_ERR_NONE;
        }

        FSVol_ReleaseUnlock(p_vol);
    }
}
#endif


/*
*********************************************************************************************************
*                                      FSEntry_TimeSetHandler()
*
* Description : Set a file or directory's date/time.
*
* Argument(s) : name_full   Name of the entry.
*               ----------  Argument validated by caller.
*
*               p_time      Pointer to date/time.
*               ----------  Argument validated by caller.
*
*               time_type   Flag to indicate which Date/Time should be set
*
*                               FS_DATE_TIME_CREATE
*                               FS_DATE_TIME_MODIFY
*                               FS_DATE_TIME_ACCESS
*                               FS_DATE_TIME_ALL
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Entry date/time set successfully.
*                               See 'FSEntry_TimeSet()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSEntry_TimeSetHandler (CPU_CHAR      *name_full,
                                      CLK_DATE_TIME *p_time,
                                      CPU_INT08U     time_type,
                                      FS_ERR        *p_err)
{
    CPU_CHAR  *name_entry;
    FS_VOL    *p_vol;


                                                                /* ---------------------- GET VOL --------------------- */
    name_entry = FSEntry_NameParseChk( name_full,
                                      &p_vol,
                                       p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if ((p_vol      == (FS_VOL   *)0) ||                        /* Rtn err if no vol found ...                          */
        (name_entry == (CPU_CHAR *)0)) {                        /*                         ... or if name illegal.      */
        return;
    }

                                                                /* Chk vol mode.                                        */
    if (DEF_BIT_IS_CLR(p_vol->AccessMode, FS_VOL_ACCESS_MODE_WR)== DEF_YES) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_VOL_INVALID_OP;
        return;
    }



                                                                /* ------------------- SET DATE/TIME ------------------ */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {           /* Rtn err if entry specifies root dir (see Note #1).   */
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_ENTRY_ROOT_DIR;
        return;
    }

    FSSys_EntryTimeSet(p_vol,
                       name_entry,
                       p_time,
                       time_type,
                       p_err);



                                                                /* ----------------- RELEASE VOL & RTN ---------------- */
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                       FSEntry_NameParseChk()
*
* Description : Parse full entry name & get volume pointer & pointer to entry name.
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
* Note(s)     : (1) If the volume name could not be parsed, the volume does not exist or no entry name
*                   is specified, then NULL pointers are returned for BOTH the entry name & the volume.
*                   Otherwise, both the entry name & volume should be valid pointers.
*********************************************************************************************************
*/

static  CPU_CHAR  *FSEntry_NameParseChk (CPU_CHAR   *name_full,
                                         FS_VOL    **pp_vol,
                                         FS_ERR     *p_err)
{
    CPU_SIZE_T   name_entry_len;
    CPU_SIZE_T   file_name_len;
    CPU_CHAR    *name_entry;
    FS_VOL      *p_vol_temp;
    CPU_CHAR    *file_name;
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
    if (name_entry_len > FS_CFG_MAX_PATH_NAME_LEN) {            /* Rtn err if file name is too long.                    */
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

                                                                /* Acquire ref to vol.                                  */
    p_vol_temp = (name_vol_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) ? FSVol_AcquireDflt()
                                                                 : FSVol_Acquire(name_vol_temp, p_err);

    if (*p_err != FS_ERR_NONE) {
        return ((CPU_CHAR *)0);
    }

    if (p_vol_temp == (FS_VOL *)0) {
       *p_err =  FS_ERR_NAME_INVALID;
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
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);


        case FS_VOL_STATE_MOUNTED:
             break;


        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:                              /* Rtn err if vol is not open.                          */
        default:
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);
    }

   *pp_vol = p_vol_temp;
   *p_err  = FS_ERR_NONE;
    return (name_entry);
}


/*
*********************************************************************************************************
*                                      FSEntry_NameParseChkEx()
*
* Description : Parse full entry name & get volume pointer & pointer to entry name.
*
* Argument(s) : name_full   Name of the entry.
*               ----------  Argument validated by caller.
*
*               name_vol    String that will receive the name of the volume.
*               ----------  Argument validated by caller.
*
*               p_dflt      Pointer to variable that will receive default volume indicator :
*               ----------  Argument validated by caller.
*
*                               DEF_YES, volume is default.
*                               DEF_NO,  volume is not default.
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
* Note(s)     : (1) If the volume name could not be parsed, the volume does not exist, or no entry name
*                   is specified, then NULL pointers are returned for BOTH the entry name & the volume.
*                   Otherwise, both the entry name & volume should be valid pointers.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  CPU_CHAR  *FSEntry_NameParseChkEx (CPU_CHAR      *name_full,
                                           CPU_CHAR      *name_vol,
                                           CPU_BOOLEAN   *p_dflt,
                                           FS_VOL       **pp_vol,
                                           FS_ERR        *p_err)
{
    CPU_SIZE_T   name_entry_len;
    CPU_SIZE_T   file_name_len;
    CPU_CHAR    *name_entry;
    CPU_CHAR    *file_name;
    FS_VOL      *p_vol_temp;
    CPU_CHAR     name_vol_temp[FS_CFG_MAX_VOL_NAME_LEN + 1u];
    CPU_BOOLEAN  vol_lock_ok;


   *pp_vol     = (FS_VOL *)0;
   *p_dflt     =  DEF_NO;
    name_entry =  FS_PathParse(name_full,
                               name_vol_temp,
                               p_err);
    if (*p_err != FS_ERR_NONE) {
        return ((CPU_CHAR *)0);
    }

    if (name_entry == (CPU_CHAR *)0) {                          /* If name could not be parsed ...                      */
        *p_err  = FS_ERR_NAME_INVALID;
         return ((CPU_CHAR *)0);                                /* ... rtn NULL vol & file name.                        */
    }

    if (*name_entry != FS_CHAR_PATH_SEP) {                      /* Require init path sep char.                          */
        *p_err =  FS_ERR_NAME_INVALID;
         return ((CPU_CHAR *)0);
    }
    name_entry++;                                               /* Ignore  init path sep char.                          */

    name_entry_len = Str_Len_N(name_entry, FS_CFG_MAX_PATH_NAME_LEN + 1u);
    if (name_entry_len > FS_CFG_MAX_PATH_NAME_LEN) {            /* Rtn err if file name is too long.                    */
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

                                                                /* Acquire ref to vol.                                  */
    p_vol_temp = (name_vol_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) ? FSVol_AcquireDflt()
                                                                 : FSVol_Acquire(name_vol_temp, p_err);

    if (p_vol_temp == (FS_VOL *)0) {
       *p_err =  FS_ERR_NAME_INVALID;
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
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);


        case FS_VOL_STATE_MOUNTED:
             if (*p_err != FS_ERR_NONE) {
                 return (DEF_NULL);
             }
             break;


        case FS_VOL_STATE_CLOSED:
        case FS_VOL_STATE_CLOSING:
        case FS_VOL_STATE_OPENING:                              /* Rtn err if vol is not open.                          */
        default:
             FSVol_ReleaseUnlock(p_vol_temp);
             return ((CPU_CHAR *)0);
    }


    if (name_vol_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) {        /* Get vol name.                                        */
        (void)Str_Copy_N(name_vol, p_vol_temp->Name, FS_CFG_MAX_VOL_NAME_LEN);
       *p_dflt = DEF_YES;
    } else {
        (void)Str_Copy_N(name_vol, name_vol_temp, FS_CFG_MAX_VOL_NAME_LEN);
       *p_dflt = FSVol_IsDflt(name_vol, p_err);
        if (*p_err != FS_ERR_NONE) {
            return ((CPU_CHAR *)0);
        }
    }

   *pp_vol = p_vol_temp;
    return (name_entry);
}
#endif
