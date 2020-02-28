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
*                                     FILE SYSTEM SUITE API LAYER
*
*                                         POSIX API FUNCTIONS
*
* Filename  : fs_api.c
* Version   : V4.08.00
*********************************************************************************************************
* Notice(s) : (1) The Institute of Electrical and Electronics Engineers and The Open Group, have given
*                 us permission to reprint portions of their documentation.  Portions of this text are
*                 reprinted and reproduced in electronic form from the IEEE Std 1003.1, 2004 Edition,
*                 Standard for Information Technology -- Portable Operating System Interface (POSIX),
*                 The Open Group Base Specifications Issue 6, Copyright (C) 2001-2004 by the Institute
*                 of Electrical and Electronics Engineers, Inc and The Open Group.  In the event of any
*                 discrepancy between these versions and the original IEEE and The Open Group Standard,
*                 the original IEEE and The Open Group Standard is the referee document.  The original
*                 Standard can be obtained online at http://www.opengroup.org/unix/online.html.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_API_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/clk.h>
#include  "fs.h"
#include  "fs_api.h"
#include  "fs_cfg_fs.h"
#include  "fs_def.h"
#include  "fs_dir.h"
#include  "fs_file.h"
#include  "fs_type.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_api.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_API_MODULE_PRESENT                                  /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                           LINT INHIBITION
*
* Note(s) : (1) The functions prototyped in this file conform to the POSIX standard, with the same
*               parameter types & return values.  Certain MISRA guidelines are thereby violated.
*
*                   "Note 970: Use of modifier or type 'int' outside of a typedef [MISRA 2004 Rule 6.3]"
*                   "Note 970: Use of modifier or type 'char' outside of a typedef [MISRA 2004 Rule 6.3]"
*********************************************************************************************************
*/

/*lint -e970*/


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
*********************************************************************************************************
*                                     WORKING DIRECTORY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             fs_chdir()
*
* Description : Set the working directory for the current task.
*
* Argument(s) : path_dir    String that specifies EITHER...
*                               (a) the absolute working directory path to set;
*                               (b) a relative path that will be applied to the current working directory.
*
* Return(s)   :  0, if no error occurs.
*               -1, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
int  fs_chdir (const  char  *path_dir)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (path_dir == (const char *)0) {                          /* Validate pointer to path                             */
        return ((int)-1);
    }
#endif

    FS_WorkingDirSet((CPU_CHAR *) path_dir,
                                 &err);

    rtn = (err == FS_ERR_NONE) ? (int)0 : (int)-1;

    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                             fs_getcwd()
*
* Description : Get the working directory for the current task.
*
* Argument(s) : path_dir    String buffer that will receive the working directory path.
*
*               size        Size of string buffer.
*
* Return(s)   : Pointer to 'path_dir', if no error occurs.
*               Pointer to NULL,       otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
char  *fs_getcwd (char       *path_dir,
                  fs_size_t   size)
{
    FS_ERR  err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (path_dir == (char *)0) {                                /* Validate pointer to path                             */
        return ((char *)0);
    }
#endif

    FS_WorkingDirGet((CPU_CHAR   *) path_dir,
                     (CPU_SIZE_T  ) size,
                                   &err);

    if (err == FS_ERR_NONE) {
        return ((char *)path_dir);
    }

    return ((char *)0);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         DIRECTORY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            fs_closedir()
*
* Description : Close & free a directory.
*
* Argument(s) : p_dir       Pointer to a directory.
*
* Return(s)   :  0, if directory is successfully closed.
*               -1, if any error was encountered.
*
* Note(s)     : (1) After a directory is closed, the application MUST desist from accessing its directory
*                   pointer.  This could cause file system corruption, since this handle may be re-used
*                   for a different directory.
*********************************************************************************************************
*/
#ifdef FS_DIR_MODULE_PRESENT
int  fs_closedir (FS_DIR  *p_dir)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_dir == (FS_DIR *)0) {                                 /* Validate pointer to dir                              */
        return ((int)-1);
    }
#endif

    FSDir_Close(p_dir, &err);
    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)-1);
    return (rtn);
}
#endif

/*
*********************************************************************************************************
*                                            fs_opendir()
*
* Description : Open a directory.
*
* Argument(s) : name_full   Name of the directory.
*
* Return(s)   : Pointer to a directory, if NO errors.
*               Pointer to NULL,        otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef FS_DIR_MODULE_PRESENT
FS_DIR  *fs_opendir (const  char  *name_full)
{
    FS_ERR   err;
    FS_DIR  *dirp;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((FS_DIR *)0);
    }
#endif

    dirp = FSDir_Open((CPU_CHAR *)name_full, &err);
    (void)err;

    return (dirp);
}
#endif

/*
*********************************************************************************************************
*                                           fs_readdir_r()
*
* Description : Read a directory entry from a directory.
*
* Argument(s) : p_dir           Pointer to a directory.
*
*               p_dir_entry     Pointer to variable that will receive directory entry information.
*
*               pp_result       Pointer to variable that will receive :
*
*                                   (a) ... 'p_dir_entry'    if NO error occurs AND directory does not encounter  EOF.
*                                   (b) ...  pointer to NULL if an error occurs OR  directory          encounters EOF.
*
* Return(s)   : 1, if an error occurs.
*               0, if no error occurs.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'readdir() : DESCRIPTION' states that :
*
*                   (a) "The 'readdir()' function shall not return directory entries containing empty names.
*                        If entries for dot or dot-dot exist, one entry shall be returned for dot and one
*                        entry shall be returned for dot-dot; otherwise, they shall not be returned."
*
*                   (b) "If a file is removed from or added to the directory after the most recent call
*                        to 'opendir()' or 'rewinddir()', whether a subsequent call to 'readdir()' returns
*                        an entry for that file is unspecified."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'readdir() : RETURN VALUE' states that "[i]f
*                   successful, the 'readdir_r()' function shall return zero; otherwise, an error shall
*                   be returned to indicate the error".
*********************************************************************************************************
*/
#ifdef FS_DIR_MODULE_PRESENT
int  fs_readdir_r (FS_DIR              *p_dir,
                   struct  fs_dirent   *p_dir_entry,
                   struct  fs_dirent  **pp_result)
{
    FS_ERR       err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_dir == (FS_DIR *)0) {                                 /* Validate pointer to dir                              */
        return ((int)1);
    }
    if (p_dir_entry == (struct  fs_dirent *)0) {                /* Validate pointer to dir entry                        */
        return ((int)1);
    }
    if (pp_result == (struct  fs_dirent **)0) {                 /* Validate pointer to result                           */
        return ((int)1);
    }
#endif

    FSDir_Rd(p_dir, p_dir_entry,&err);

    if (err == FS_ERR_NONE) {

       *pp_result =  p_dir_entry;
        return ((int)0);

    } else {

        if (err == FS_ERR_EOF) {
           *pp_result = (struct fs_dirent *)0;
            return ((int)0);
        }
       *pp_result = (struct fs_dirent *)0;
        return ((int)1);

    }




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
*                                             fs_mkdir()
*
* Description : Create a directory.
*
* Argument(s) : name_full   Name of the directory.
*
* Return(s)   : -1, if an error occurs.
*                0, if no error occurs.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
int  fs_mkdir (const  char  *name_full)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((int)-1);
    }
#endif

    FSEntry_Create((CPU_CHAR *) name_full,
                                FS_ENTRY_TYPE_DIR,
                                DEF_YES,
                               &err);

    rtn = (err == FS_ERR_NONE) ? (0) : (-1);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                             fs_remove()
*
* Description : Delete a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*
* Return(s)   :  0, if the entry is     removed.
*               -1, if the entry is NOT removed.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'remove() : DESCRIPTION' states that :
*
*                   (a) "If 'path' does not name a directory, 'remove(path)' shall be equivalent to
*                       'unlink(path)'."
*
*                   (b) "If path names a directory, remove(path) shall be equivalent to rmdir(path)."
*
*                       (1) See 'fs_rmdir()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
int  fs_remove (const  char  *name_full)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((int)-1);
    }
#endif

    FSEntry_Del((CPU_CHAR *) name_full,
                             FS_ENTRY_TYPE_ANY,
                            &err);

    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)-1);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                             fs_rename()
*
* Description : Rename a file or directory.
*
* Argument(s) : name_full_old   Old path of the entry.
*
*               name_full_new   New path of the entry.
*
* Return(s)   :  0, if the entry is     renamed.
*               -1, if the entry is NOT renamed.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'rename() : DESCRIPTION' states that :
*
*                   (a) "If the 'old' argument and the 'new' argument resolve to the same existing file,
*                        'rename()' shall return successfully and perform no other action."
*
*                   (b) "If the 'old' argument points to the pathname of a file that is not a directory,
*                        the 'new' argument shall not point to the pathname of a directory.  If the link
*                        named by the 'new' argument exists, it shall be removed and 'old' renamed to
*                        'new'."
*
*                   (c) "If the 'old' argument points to the pathname of a directory, the 'new' argument
*                        shall not point to the pathname of a file that is not a directory.  If the
*                       directory named by the 'new' argument exists, it shall be removed and 'old'
*                       renamed to 'new'."
*
*                       (1) "If 'new' names an existing directory, it shall be required to be an empty
*                            directory."
*
*                   (d) "The 'new' pathname shall not contain a path prefix that names 'old'."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'rename() : RETURN VALUE' states that "[u]pon
*                   successful completion, 'rename()' shall return 0; otherwise, -1 shall be returned".
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
int  fs_rename (const  char  *name_full_old,
                const  char  *name_full_new)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full_old == (const char *)0) {                     /* Validate pointer to name                             */
        return ((int)-1);
    }
    if (name_full_new == (const char *)0) {                     /* Validate pointer to name                             */
        return ((int)-1);
    }
#endif

    FSEntry_Rename((CPU_CHAR *) name_full_old,
                   (CPU_CHAR *) name_full_new,
                                DEF_NO,
                               &err);

    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)-1);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                             fs_rmdir()
*
* Description : Delete a directory.
*
* Argument(s) : name_full   Name of the directory.
*
* Return(s)   :  0, if the directory is     removed.
*               -1, if the directory is NOT removed.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'rmdir() : DESCRIPTION' states that :
*
*                   (a) "The 'rmdir()' function shall remove a directory whose name is given by path. The
*                        directory shall be removed only if it is an empty directory."
*
*                   (b) "If the directory is the root directory or the current working directory of any
*                        process, it is unspecified whether the function succeeds, or whether it shall fail"
*
*               (1) IEEE Std 1003.1, 2004 Edition, Section 'rmdir() : RETURN VALUE' states that "[u]pon
*                   successful completion, the function 'rmdir()' shall return 0.  Otherwise, -1 shall
*                   be returned".
*
*               (2) The root directory CANNOT be removed.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
int  fs_rmdir (const  char  *name_full)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((int)-1);
    }
#endif

    FSEntry_Del((CPU_CHAR *) name_full,
                             FS_ENTRY_TYPE_DIR,
                            &err);

    rtn = (err == FS_ERR_NONE) ? (0) : (-1);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                              fs_stat()
*
* Description : Get information about a file or directory.
*
* Argument(s) : name_full   Name of the entry.
*
*               p_info      Pointer to structure that will receive the entry information.
*
* Return(s)   :  0, if the function succeeds.
*               -1, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  fs_stat (const  char             *name_full,
                     struct  fs_stat  *p_info)
{
    FS_ENTRY_INFO  info;
    FS_ERR         err;
    fs_mode_t      mode;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((int)-1);
    }
    if (p_info == (struct  fs_stat *)0) {                       /* Validate pointer to result                           */
        return ((int)-1);
    }
#endif

    FSEntry_Query((CPU_CHAR *) name_full,
                              &info,
                              &err);

    if (err != FS_ERR_NONE) {
        return ((int)-1);
    }

    mode = (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) ? FS_S_IFDIR
                                                                         : FS_S_IFREG;

    if (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_RD) == DEF_YES) {
        mode |= FS_S_IRUSR | FS_S_IRGRP | FS_S_IROTH;
    }

    if (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
        mode |= FS_S_IWUSR | FS_S_IWGRP | FS_S_IWOTH;
    }

    p_info->st_dev     =  0u;
    p_info->st_ino     =  0u;
    p_info->st_mode    =  mode;
    p_info->st_nlink   =  1u;
    p_info->st_uid     =  0u;
    p_info->st_gid     =  0u;
    p_info->st_size    = (fs_off_t    ) info.Size;
    p_info->st_atime   = (fs_time_t   )-1;
    p_info->st_ctime   = (fs_time_t   ) info.DateTimeCreate;
    p_info->st_mtime   = (fs_time_t   ) info.DateTimeWr;
    p_info->st_blksize = (fs_blksize_t) info.BlkSize;
    p_info->st_blocks  = (fs_blkcnt_t ) info.BlkCnt;

    return ((int)0);
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
*                                            fs_clearerr()
*
* Description : Clear EOF & error indicators on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  fs_clearerr (FS_FILE *p_file)
{
    FS_ERR  err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return;
    }
#endif

    FSFile_ClrErr(p_file, &err);
}


/*
*********************************************************************************************************
*                                             fs_fclose()
*
* Description : Close & free a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : 0,      if the file was successfully closed.
*               FS_EOF, otherwise.
*
* Note(s)     : (1) After a file is closed, the application MUST desist from accessing its file pointer.
*                   This could cause file system corruption, since this handle may be re-used for a
*                   different file.
*
*               (2) (a) If the most recent operation is output (write), all unwritten data is written
*                       to the file.
*
*                   (b) Any buffer assigned with 'fs_setbuf()' or 'fs_setvbuf()' shall no longer be
*                       accessed by the file system & may be re-used by the application.
*********************************************************************************************************
*/

int  fs_fclose (FS_FILE  *p_file)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EOF);
    }
#endif

    FSFile_Close(p_file, &err);

    rtn = (err == FS_ERR_NONE) ? (int)0 : (int)FS_EOF;
    return (rtn);
}


/*
*********************************************************************************************************
*                                              fs_feof()
*
* Description : Test EOF indicator on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : 0,              if EOF indicator is NOT set or if an error occurred.
*               Non-zero value, if EOF indicator is     set.
*
* Note(s)     : (1) The return value from this function should ALWAYS be tested against 0 :
*
*                       rtn = fs_feof(pfile);
*                       if (rtn == 0) {
*                           // EOF indicator is NOT set
*                       } else {
*                           // EOF indicator is     set
*                       }
*
*               (2) If the end-of-file indicator is set (i.e., 'fs_feof()' returns a non-zero value),
*                   'fs_clrerror()' can be used to clear that indicator.
*********************************************************************************************************
*/

int  fs_feof (FS_FILE  *p_file)
{
    CPU_BOOLEAN  is_eof;
    int          rtn;
    FS_ERR       err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EBADF);
    }
#endif

    is_eof = FSFile_IsEOF(p_file, &err);
    rtn    = (is_eof == DEF_YES) ? (int)FS_STDIO_ERR : (int)0;
    return (rtn);
}


/*
*********************************************************************************************************
*                                             fs_ferror()
*
* Description : Test error indicator on a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : 0,              if error indicator is NOT set or if an error occurred.
*               Non-zero value, if error indicator is     set.
*
* Note(s)     : (1) The return value from this function should ALWAYS be tested against 0 :
*
*                       rtn = fs_ferror(pfile);
*                       if (rtn == 0) {
*                           // Error indicator is NOT set
*                       } else {
*                           // Error indicator is     set
*                       }
*
*               (2) If the error indicator is set (i.e., 'fs_ferror() returns a non-zero value),
*                   'fs_clearerr()' can be used to clear that indicator.
*********************************************************************************************************
*/

int  fs_ferror (FS_FILE  *p_file)
{
    CPU_BOOLEAN  is_err;
    int          rtn;
    FS_ERR       err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EBADF);
    }
#endif

    is_err = FSFile_IsErr(p_file, &err);
    rtn    = (is_err == DEF_YES) ? (int)FS_STDIO_ERR : (int)0;
    return (rtn);
}


/*
*********************************************************************************************************
*                                             fs_fflush()
*
* Description : Flush buffer contents to file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : 0,      if flushing succeeds.
*               FS_EOF, otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fflush() : DESCRIPTION' states that :
*
*                   (a) "If 'stream' points to an output stream or an update stream in which the most
*                        recent operation was not input, 'fflush()' shall cause any unwritten data for
*                        that stream to be written to the file."
*
*                   (b) "If 'stream' is a null pointer, the 'fflush' function performs this flushing
*                        action on all streams ...."  #### Currently unimplemented.
*
*                   (c) "Upon successful completion, fflush() shall return 0; otherwise, it shall set the
*                        error indicator for the stream, return EOF."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fflush()' defines no behavior for an input
*                   stream or update stream in which the most recent operation was input.
*
*                   (a) In this implementation, if the most recent operation is input, 'fs_fflush()'
*                       clears all buffered input data.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
int  fs_fflush (FS_FILE  *p_file)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EOF);
    }
#endif

    FSFile_BufFlush(p_file, &err);

    rtn = (err == FS_ERR_NONE) ? (int)0 : (int)FS_EOF;
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                            fs_fgetpos()
*
* Description : Get file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_pos      Pointer to variable that will receive the file position indicator.
*
* Return(s)   : 0,              if no error occurs.
*               Non-zero value, otherwise.
*
* Note(s)     : (1) The return value should be tested against 0 :
*
*                       rtn = fs_fgetpos(pfile, &pos);
*                       if (rtn == 0) {
*                           // No error occurred.
*                       } else {
*                           // Handle error.
*                       }
*
*               (2) The value placed in 'pos' should be passed to 'fs_fsetpos()' to reposition the file
*                   to its position at the time when this function was called.
*********************************************************************************************************
*/

int  fs_fgetpos (FS_FILE    *p_file,
                 fs_fpos_t  *p_pos)
{
    FS_FILE_SIZE  pos_fs;
    FS_ERR        err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EBADF);
    }
    if (p_pos == (fs_fpos_t *)0) {                              /* Validate pointer to pos                              */
        return ((int)FS_EINVAL);
    }
#endif


    pos_fs = FSFile_PosGet( p_file,
                           &err);

    if (err != FS_ERR_NONE) {
       *p_pos = (fs_fpos_t)0;
        return (FS_STDIO_ERR);
    }

   *p_pos = (fs_fpos_t)pos_fs;
    return (0);
}


/*
*********************************************************************************************************
*                                           fs_flockfile()
*
* Description : Acquire task ownership of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'flockfile(), ftrylockfile(), funlockfile() :
*                   DESCRIPTION' states that :
*
*                   (a) "The 'flockfile()' function shall acquire thread ownership of a (FILE *) object."
*
*                   (b) "The functions shall behave as if there is a lock count associated with each
*                        (FILE *) object."
*
*                       (1) "The (FILE *) object is unlocked when the count is zero."
*
*                       (2) "When the count is positive, a single thread owns the (FILE *) object."
*
*                       (3) "When the 'flockfile()' function is called, if the count is zero or if the
*                            count is positive and the caller owns the (FILE *), the count shall be
*                            incremented.  Otherwise, the calling thread shall be suspended, waiting for
*                            the count to return to zero."
*
*                       (4) "Each call to 'funlockfile()' shall decrement the count."
*
*                       (5) "This allows matching calls to 'flockfile()' (or successful calls to 'ftrylockfile()')
*                            and 'funlockfile()' to be nested."
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  fs_flockfile (FS_FILE  *p_file)
{
    FS_ERR  err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return;
    }
#endif

    FSFile_LockGet(p_file, &err);
}
#endif


/*
*********************************************************************************************************
*                                             fs_fopen()
*
* Description : Open a file.
*
* Argument(s) : name_full   Name of the file.
*
*               str_mode    Access mode of the file (see Note #1a).
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fopen() : DESCRIPTION' states that :
*
*                   (a) "If ['str_mode'] is one of the following, the file is open in the indicated mode." :
*
*                           "r or rb           Open file for reading."
*                           "w or wb           Truncate to zero length or create file for writing."
*                           "a or ab           Append; open and create file for writing at end-of-file."
*                           "r+ or rb+ or r+b  Open file for update (reading and writing)."
*                           "w+ or wb+ or w+b  Truncate to zero length or create file for update."
*                           "a+ or ab+ or a+b  Append; open or create for update, writing at end-of-file.
*
*                   (b) "The character 'b' shall have no effect"
*
*                   (c) "Opening a file with read mode ... shall fail if the file does not exist or
*                        cannot be read"
*
*                   (d) "Opening a file with append mode ... shall cause all subsequent writes to the
*                        file to be forced to the then current end-of-file"
*
*                   (e) "When a file is opened with update mode ... both input and output may be performed....
*                        However, the application shall ensure that output is not directly followed by
*                        input without an intervening call to 'fflush()' or to a file positioning function
*                      ('fseek()', 'fsetpos()', or 'rewind()'), and input is not directly followed by output
*                        without an intervening call to a file positioning function, unless the input
*                        operation encounters end-of-file."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fopen() : RETURN VALUE' states that "[u]pon
*                   successful completion 'fopen()' shall return a pointer to the object controlling the
*                   stream.  Otherwise a null pointer shall be returned'.
*********************************************************************************************************
*/

FS_FILE  *fs_fopen (const  char  *name_full,
                    const  char  *str_mode)
{
    FS_ERR     err;
    FS_FLAGS   mode_flags;
    FS_FILE   *p_file;
    CPU_SIZE_T len;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_full == (const char *)0) {                         /* Validate pointer to name                             */
        return ((FS_FILE *)0);                                  /* ... rtn NULL.                                        */
    }
    if (str_mode == (const char *)0) {                          /* Validate pointer to mode                             */
        return ((FS_FILE *)0);                                  /* ... rtn NULL.                                        */
    }
#endif

    len = Str_Len_N((char *)str_mode, 4u);

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (len > 3u) {                                             /* Validate length of mode                              */
        return ((FS_FILE *)0);                                  /* ... rtn NULL.                                        */
    }
    if (len < 1u) {                                             /* Validate length of mode                              */
        return ((FS_FILE *)0);                                  /* ... rtn NULL.                                        */
    }
#endif

    mode_flags = FSFile_ModeParse((CPU_CHAR *)str_mode, len);

    if (mode_flags == FS_FILE_ACCESS_MODE_NONE) {               /* If mode flags invalid ...                            */
        return ((FS_FILE *)0);                                  /* ... rtn NULL.                                        */
    }

    p_file = FSFile_Open((CPU_CHAR *) name_full,
                                      mode_flags,
                                     &err);

    return (p_file);
}


/*
*********************************************************************************************************
*                                             fs_fread()
*
* Description : Read from a file.
*
* Argument(s) : p_dest       Pointer to destination buffer.
*
*               size        Size of each item to read.
*
*               nitems      Number of items to read.
*
*               p_file      Pointer to a file.
*
* Return(s)   : Number of items read.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fread() : DESCRIPTION' states that :
*
*                   (a) "The 'fread()' function shall read into the array pointed to by 'ptr' up to 'nitems'
*                        elements whose size is specified by 'size' in bytes"
*
*                   (b) "The file position indicator for the stream ... shall be advanced by the number of
*                        bytes successfully read"
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fread() : RETURN VALUE' states that "[u]pon
*                   completion, 'fread()' shall return the number of elements which is less than 'nitems'
*                   only if a read error or end-of-file is encountered".
*
*               (3) See 'fs_fopen()  Note #1e'.
*
*               (4) The file MUST have been opened in read or update (read/write) mode.
*
*               (5) If an error occurs while reading from the file, a value less than 'nitems' will
*                   be returned.  To determine whether the premature return was caused by reaching the
*                   end-of-file, the 'fs_feof()' function should be used :
*
*                       rtn = fs_fread(pbuf, 1, 1000, pfile);
*                       if (rtn < 1000) {
*                           eof = fs_feof();
*                           if (eof != 0) {
*                               // File has reached EOF
*                           } else {
*                               // Error has occurred
*                           }
*                       }
*
*               (6) #### Check for multiplication overflow.
*********************************************************************************************************
*/

fs_size_t  fs_fread (void       *p_dest,
                     fs_size_t   size,
                     fs_size_t   nitems,
                     FS_FILE    *p_file)
{
    CPU_SIZE_T  size_tot;
    CPU_SIZE_T  size_rd;
    fs_size_t   size_rd_items;
    FS_ERR      err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((fs_size_t)0u);
    }
    if (p_dest == (void *)0) {                                  /* Validate pointer to destination buffer               */
        return ((fs_size_t)0u);
    }
#endif

    size_tot      = (CPU_SIZE_T)size * (CPU_SIZE_T)nitems;      /* See Note #6.                                         */
    if (size_tot == 0u) {
        return ((fs_size_t)0u);
    }

    size_rd       = FSFile_Rd(             p_file,
                                           p_dest,
                              (CPU_SIZE_T) size_tot,
                                          &err);

    size_rd_items = (fs_size_t)size_rd / size;

    return (size_rd_items);
}


/*
*********************************************************************************************************
*                                             fs_fseek()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               offset      Offset from file position specified by 'whence'.
*
*               origin      Reference position for offset :
*
*                               SEEK_SET    Offset is from the beginning of the file.
*                               SEEK_CUR    Offset is from current file position.
*                               SEEK_END    Offset is from the end       of the file.
*
* Return(s)   :  0, if the function succeeds.
*               -1, otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fread() : DESCRIPTION' states that :
*
*                   (a) "If a read or write error occurs, the error indicator for the stream shall be set"
*
*                   (b) "The new position measured in bytes from the beginning of the file, shall be
*                        obtained by adding 'offset' to the position specified by 'whence'. The specified
*                        point is ..."
*
*                       (1) "... the beginning of the file                        for SEEK_SET"
*
*                       (2) "... the current value of the file-position indicator for SEEK_CUR"
*
*                       (3) "... end-of-file                                      for SEEK_END"
*
*                   (c) "A successful call to 'fseek()' shall clear the end-of-file indicator"
*
*                   (d) "The 'fseek()' function shall allow the file-position indicator to be set beyond
*                        the end of existing data in the file.  If data is later written at this point,
*                        subsequent reads of data in the gap shall return bytes with the value 0 until
*                        data is actually written into the gap."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fread() : RETURN VALUE' states that "[t]he
*                   'fseek()' and 'fseeko()' functions shall return 0 if they succeeds.  Otherwise, they
*                   shall return -1".
*
*               (3) If the file position indicator is set beyond the file's current data, the file MUST
*                   be opened in write or read/write mode.
*********************************************************************************************************
*/

int  fs_fseek (FS_FILE    *p_file,
               long  int   offset,
               int         origin)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
#endif

    FSFile_PosSet(                 p_file,
                  (FS_FILE_OFFSET) offset,
                  (FS_STATE      ) origin,
                                  &err);

    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)-1);
    return (rtn);
}


/*
*********************************************************************************************************
*                                            fs_fsetpos()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_pos       Pointer to variable holding the file position.
*
* Return(s)   : 0,              if the function succeeds.
*               Non-zero value, otherwise.
*
* Note(s)     : (1) The return value should be tested against 0 :
*
*                       rtn = fs_fsetpos(pfile, &pos);
*                       if (rtn == 0) {
*                           // No error occurred.
*                       } else {
*                           // Handle error.
*                       }
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fsetpos() : DESCRIPTION' states that :
*
*                   (a) "If a read or write error occurs, the error indicator for the stream is set"
*
*                   (b) "The 'fsetpos()' function shall set the file position and state indicators for
*                        the stream pointed to by stream according to the value of the object pointed to
*                        by 'pos', which the application shall ensure is a value obtained from an earlier
*                        call to 'fgetpos()' on the same stream."
*
*               (3) IEEE Std 1003.1, 2004 Edition, Section 'fsetpos() : RETURN VALUE' states that "[t]he
*                   'fsetpos()' function shall return 0 if it succeeds; otherwise, it shall return a
*                   non-zero value".
*
*               (4) No attempt is made to verify that the value stored in 'pos' was returned from
*                   'fs_fgetpos()'.
*
*               (5) See also 'fs_fseek()  Note #1d'.
*********************************************************************************************************
*/

int  fs_fsetpos (FS_FILE           *p_file,
                 const  fs_fpos_t  *p_pos)
{
    FS_FILE_OFFSET  pos_fs;
    FS_ERR          err;
    int             rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)FS_EBADF);
    }
    if (p_pos == (fs_fpos_t *)0) {
        return ((int)FS_STDIO_ERR);
    }
#endif

    pos_fs = (FS_FILE_OFFSET)*p_pos;

    FSFile_PosSet( p_file,
                   pos_fs,
                   FS_FILE_ORIGIN_START,
                  &err);

    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)FS_STDIO_ERR);

    return (rtn);
}


/*
*********************************************************************************************************
*                                             fs_fstat()
*
* Description : Get information about a file.
*
* Argument(s) : p_file       Pointer to a file.
*
*               p_info       Pointer to structure that will receive the file information.
*
* Return(s)   :  0, if the function succeeds.
*               -1, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  fs_fstat (FS_FILE          *p_file,
               struct  fs_stat  *p_info)
{
    FS_ENTRY_INFO  info;
    FS_ERR         err;
    fs_mode_t      mode;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
    if (p_info == (struct fs_stat *)0) {
        return ((int)-1);
    }
#endif

    FSFile_Query( p_file,
                 &info,
                 &err);

    if (err != FS_ERR_NONE) {
        return ((int)-1);
    }

    mode = (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) ? FS_S_IFDIR
                                                                         : FS_S_IFREG;

    if (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_RD) == DEF_YES) {
        mode |= FS_S_IRUSR | FS_S_IRGRP | FS_S_IROTH;
    }

    if (DEF_BIT_IS_SET(info.Attrib, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
        mode |= FS_S_IWUSR | FS_S_IWGRP | FS_S_IWOTH;
    }

    p_info->st_dev     =  0u;
    p_info->st_ino     =  0u;
    p_info->st_mode    =  mode;
    p_info->st_nlink   =  1u;
    p_info->st_uid     =  0u;
    p_info->st_gid     =  0u;
    p_info->st_size    = (fs_off_t    ) info.Size;
    p_info->st_atime   = (fs_time_t   )-1;
    p_info->st_mtime   = (fs_time_t   ) info.DateTimeWr;
    p_info->st_ctime   = (fs_time_t   ) info.DateTimeCreate;
    p_info->st_blksize = (fs_blksize_t) info.BlkSize;
    p_info->st_blocks  = (fs_blkcnt_t ) info.BlkCnt;

    return ((int)0);
}


/*
*********************************************************************************************************
*                                             fs_ftell()
*
* Description : Get file position indicator.
*
* Argument(s) : p_file     Pointer to a file.
*
* Return(s)   : The current file system position, if the function succeeds.
*               -1,                               otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'ftell() : RETURN VALUE' states that :
*
*                   (a) "Upon successful completion, 'ftell()' and 'ftello()' shall return the current
*                        value of the file-position indicator for the stream measured in bytes from the
*                        beginning of the file."
*
*                   (b) "Otherwise, 'ftell()' and 'ftello()' shall return -1, cast to 'long' and 'off_t'
*                        respectively, and set errno to indicate the error."
*
*               (2) #### Check for overflow in cast.
*********************************************************************************************************
*/

long  int  fs_ftell (FS_FILE  *p_file)
{
    FS_FILE_SIZE  pos;
    FS_ERR        err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
#endif

    pos = FSFile_PosGet( p_file,
                        &err);

    if (err != FS_ERR_NONE) {
        return ((long int)-1);
    }

    return ((long int)pos);                                     /* See Note #2.                                         */
}


/*
*********************************************************************************************************
*                                           fs_ftruncate()
*
* Description : Truncate a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               size       Length of file after truncation.
*
* Return(s)   :  0, if the function succeeds.
*               -1, otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'ftruncate() : DESCRIPTION' states that :
*
*                   (a) "If 'fildes' is not a valid file descriptor open for writing, the 'ftruncate()'
*                        function shall fail."
*
*                   (b) "[The] 'ftruncate()' function shall cause the size of the file to be truncated to
*                        'length'."
*
*                       (1) "If the size of the file previously exceeded length, the extra data shall no
*                            longer be available to reads on the file."
*
*                       (2) "If the file previously was smaller than this size, 'ftruncate' shall either
*                            increase the size of the file or fail."  This implementation increases the
*                            size of the file.
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'ftruncate() : DESCRIPTION' states that [u]pon
*                   successful completion, 'ftruncate()' shall return 0; otherwise -1 shall be returned
*                   and 'errno' set to indicate the error".
*
*               (3) If the file position indicator before the call to 'fs_ftruncate()' lay in the
*                   extra data destroyed by the function, then the file position will be set to the
*                   end-of-file.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
int  fs_ftruncate (FS_FILE   *p_file,
                   fs_off_t   size)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
#endif

    FSFile_Truncate(               p_file,
                    (FS_FILE_SIZE) size,
                                  &err);

    rtn = (err == FS_ERR_NONE) ? (0) : (-1);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                          fs_ftrylockfile()
*
* Description : Acquire task ownership of a file (if available).
*
* Argument(s) : p_file    Pointer to a file.
*
* Return(s)   : 0,              if no error occurs & the file lock is acquired.
*               Non-zero value, otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'flockfile(), ftrylockfile(), funlockfile() :
*                   DESCRIPTION' states that :
*
*                   (a) See 'fs_flockfile()  Note(s)'.
*
*                   (b) "The 'ftrylockfile() function shall acquire for a thread ownership of a (FILE *)
*                        object if the object is available; 'ftrylockfile()' is a non-blocking version of
*                       'flockfile()'."
*
*               (1) IEEE Std 1003.1, 2004 Edition, Section 'flockfile(), ftrylockfile(), funlockfile() :
*                   RETURN VALUES' states that "[t]he 'ftrylockfile()' function shall return zero for
*                   success and non-zero to indicate that the lock cannot be acquired".
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
int  fs_ftrylockfile (FS_FILE  *p_file)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
#endif

    FSFile_LockAccept(p_file, &err);

    rtn = (err == FS_ERR_NONE) ? (int)0 : (int)FS_STDIO_ERR;

    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                          fs_funlockfile()
*
* Description : Release task ownership of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'flockfile(), ftrylockfile(), funlockfile() :
*                   DESCRIPTION' states that :
*
*                   (a) See 'fs_flockfile()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_LOCK_EN == DEF_ENABLED)
void  fs_funlockfile (FS_FILE  *p_file)
{
    FS_ERR  err;

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return;
    }

#endif

    FSFile_LockSet(p_file, &err);
}
#endif


/*
*********************************************************************************************************
*                                             fs_fwrite()
*
* Description : Write to a file.
*
* Argument(s) : p_src       Pointer to source buffer.
*
*               size        Size of each item to write.
*
*               nitems      Number of items to write.
*
*               p_file      Pointer to a file.
*
* Return(s)   : Number of items written.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fwrite() : DESCRIPTION' states that :
*
*                   (a) "The 'fwrite()' function shall write, from the array pointed to by 'ptr', up to
*                       'nitems' elements whose size is specified by 'size', to the stream pointed to by
*                       'stream'"
*
*                   (b) "The file position indicator for the stream ... shall be advanced by the number of
*                        bytes successfully written"
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fwrite() : RETURN VALUE' states that
*                   "'fwrite()' shall return the number of elements successfully written, which may be
*                   less than 'nitems' if a write error is encountered".
*
*               (3) See 'fs_fopen()  Notes #1d & #1e'.
*
*               (4) The file MUST have been opened in write or update (read/write) mode.
*
*               (5) #### Check for multiplication overflow.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
fs_size_t  fs_fwrite (const  void  *p_src,
                      fs_size_t     size,
                      fs_size_t     nitems,
                      FS_FILE      *p_file)
{
    CPU_SIZE_T  size_tot;
    CPU_SIZE_T  size_wr;
    fs_size_t   size_wr_items;
    FS_ERR      err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((fs_size_t)0u);
    }
    if (p_src == (const void *)0) {
        return ((fs_size_t)0u);
    }
#endif

    size_tot      = (CPU_SIZE_T)size * (CPU_SIZE_T)nitems;      /* See Note #5.                                         */
    if (size_tot == 0u) {
        return ((fs_size_t)0u);
    }

    size_wr       = FSFile_Wr(               p_file,
                              (void       *) p_src,
                              (CPU_SIZE_T  ) size_tot,
                                            &err);

    size_wr_items = (fs_size_t)size_wr / size;

    return (size_wr_items);
}
#endif


/*
*********************************************************************************************************
*                                             fs_rewind()
*
* Description : Reset file position indicator of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'rewind() : DESCRIPTION' states that :
*
*                       "[T]he call 'rewind(stream)' shall be equivalent to '(void)fseek(stream, 0L, SEEK_SET)'
*                        except that 'rewind()' shall also clear the error indicator."
*********************************************************************************************************
*/

void  fs_rewind (FS_FILE  *p_file)
{
    FS_ERR  err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return;
    }
#endif

    FSFile_ClrErr(p_file, &err);
    (void)fs_fseek(p_file, 0, (int)SEEK_SET);
}


/*
*********************************************************************************************************
*                                             fs_setbuf()
*
* Description : Assign buffer to a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_buf       Pointer to buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'setbuf() : DESCRIPTION' states that :
*
*                       "Except that it returns no value, the function call: 'setbuf(stream, buf)'
*                        shall be equivalent to: 'setvbuf(stream, buf, _IOFBF, BUFSIZ)' if 'buf' is not a
*                        null pointer"
*
*               (2) See 'fs_setvbuf()  Note(s)'.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
void  fs_setbuf (FS_FILE  *p_file,
                 char     *p_buf)
{
    int        mode;
    fs_size_t  size;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return;
    }
    if (p_buf == (char *)0) {
        return;
    }
#endif

    mode = (int      )FS__IOFBF;
    size = (fs_size_t)FS_BUFSIZ;

   (void)fs_setvbuf(p_file,
                    p_buf,
                    mode,
                    size);
}
#endif


/*
*********************************************************************************************************
*                                            FS_setvbuf()
*
* Description : Assign buffer to a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_buf       Pointer to buffer.
*
*               mode        Buffer mode.
*
*               size        Size of buffer, in octets.
*
* Return(s)   : -1, if an error occurs.
*                0, if no error occurs.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'setvbuf() : DESCRIPTION' states that :
*
*                   (a) "The setvbuf() function may be used after the stream pointed to by stream is
*                        associated with an open file but before any other operation (other than an
*                        unsuccessful call to setvbuf()) is performed on the stream."
*
*                   (b) "The argument 'mode' determines how 'stream' will be buffered ... "
*
*                       (1) ... FS__IOFBF "causes input/output to be fully buffered".
*
*                       (2) ... FS__IONBF "causes input/output to be unbuffered".
*
*                       (3) No equivalent to '_IOLBF' is supported.
*
*                   (c) "If 'buf' is not a null pointer, the array it points to may be used instead of a
*                        buffer allocated by the 'setvbuf' function and the argument 'size' specifies the
*                        size of the array ...."  This implementation REQUIRES that 'buf' not be a null
*                        pointer; the array 'buf' points to will always be used.
*
*                   (d) The function "returns zero on success, or nonzero if an invalid value is given
*                       for 'mode' or if the request cannot be honored".
*
*               (2) 'size' MUST be more than or equal to the size of one sector & will be rounded DOWN
*                   to the size of a number of full sectors.
*
*               (3) Once a buffer is assigned to a file, a new buffer may not be assigned nor may the
*                   assigned buffer be removed.  To change the buffer, the file should be closed &
*                   re-opened.
*
*               (4) Upon power loss, any data stored in file buffers will be lost.
*********************************************************************************************************
*/

#if (FS_CFG_FILE_BUF_EN == DEF_ENABLED)
int  fs_setvbuf (FS_FILE    *p_file,
                 char       *p_buf,
                 int         mode,
                 fs_size_t   size)
{
    FS_ERR  err;
    int     rtn;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_file == (FS_FILE *)0) {                               /* Validate pointer to file                             */
        return ((int)-1);
    }
    if (p_buf == (char *)0) {
        return ((int)-1);
    }
#endif

    FSFile_BufAssign(               p_file,
                     (void       *) p_buf,
                     (FS_FLAGS    ) mode,
                     (CPU_SIZE_T  ) size,
                                   &err);

    rtn = (err == FS_ERR_NONE) ? ((int)0) : ((int)FS_STDIO_ERR);
    return (rtn);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           TIME FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           fs_asctime_r()
*
* Description : Convert date/time to string.
*
* Argument(s) : p_time      Pointer to date/time to format.
*
*               str_time    String buffer that will receive the date/time string (see Note #1).
*
* Return(s)   : Pointer to 'p_time', if NO errors.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) 'buf' MUST be at least 26 characters long.  Buffer overruns MUST be prevented by caller.
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'asctime() : DESCRIPTION' states that :
*
*                   (a) "The 'asctime()' function shall convert the broken-down time in the structure
*                        pointed to by 'timeptr' into a string in the form:
*
*                           Sun Sep 16 01:03:52 1973\n\0
*********************************************************************************************************
*/

char  *fs_asctime_r (const  struct  fs_tm  *p_time,
                                    char   *str_time)
{
    CLK_DATE_TIME stime;
    CPU_BOOLEAN   conv_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                          /* --------------- VALIDATE ARGS -------------- */
    if (p_time == (struct fs_tm *)0) {                                  /* Validate broken-down time.                   */
        return ((char *)0);
    }
    if (str_time == (char *)0) {                                        /* Validate string buffer.                      */
        return ((char *)0);
    }
#endif



                                                                        /* --------------- FMT DATE/TIME -------------- */
    stime.Sec     = (CPU_INT08U)p_time->tm_sec;
    stime.Min     = (CPU_INT08U)p_time->tm_min;
    stime.Hr      = (CPU_INT08U)p_time->tm_hour;
    stime.Day     = (CPU_INT08U)p_time->tm_mday;
    stime.Month   = (CPU_INT08U)p_time->tm_mon;
    stime.Yr      = (CPU_INT08U)p_time->tm_year;
    stime.DayOfWk = (CPU_INT08U)p_time->tm_wday;

    conv_success =  Clk_DateTimeToStr(            &stime,
                                                   FS_TIME_FMT,
                                      (CPU_CHAR *) str_time,
                                                   FS_TIME_STR_MIN_LEN);

    if (conv_success != DEF_YES) {
        return ((char *)0);
    }

    return (str_time);
}


/*
*********************************************************************************************************
*                                            fs_ctime_r()
*
* Description : Convert timestamp to string.
*
* Argument(s) : p_ts        Pointer to timestamp to format.
*
*               str_time   String buffer that will receive the date/time string (see Note #1).
*
* Return(s)   : Pointer to 'p_ts', if NO errors.
*               Pointer to NULL,   otherwise.
*
* Note(s)     : (1) 'buf' MUST be at least 26 characters long.  buffer overruns MUST be prevented by caller.
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'ctime() : DESCRIPTION' states that :
*
*                   (a) 'ctime' "shall be equivalent to:  'asctime(localtime(clock))'".
*********************************************************************************************************
*/

char  *fs_ctime_r (const  fs_time_t  *p_ts,
                          char       *str_time)
{
    CLK_TS_SEC     ts;
    CLK_DATE_TIME  stime;
    CPU_BOOLEAN    success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_ts == (const fs_time_t *)0) {                         /* Validate time val ptr.                               */
        return ((char *)0);
    }
    if (str_time == (char *)0) {                                /* Validate string buffer.                              */
        return ((char *)0);
    }
#endif



                                                                /* ------------------- FMT DATE/TIME ------------------ */
    ts      = (CLK_TS_SEC)*p_ts;

    success = Clk_TS_UnixToDateTime(              ts,
                                    (CLK_TZ_SEC)  0,
                                                 &stime);
    if (success != DEF_YES) {
        return ((char *)0);
    }

    success = Clk_DateTimeToStr(            &stime,
                                             FS_TIME_FMT,
                                (CPU_CHAR *) str_time,
                                             FS_TIME_STR_MIN_LEN);
    if (success != DEF_YES) {
        return ((char *)0);
    }


    return (str_time);
}


/*
*********************************************************************************************************
*                                          fs_localtime_r()
*
* Description : Convert timestamp to date/time.
*
* Argument(s) : p_ts        Pointer to timestamp to convert.
*
*               p_time      Pointer to variable that will receive the date/time.
*
* Return(s)   : Pointer to 'p_time', if NO errors.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section '4.14 Seconds Since the Epoch()' states that
*
*                   (a) "If the year is <1970 or the value is negative, the relationship is undefined.
*                        If the year is >=1970  and the value is non-negative, the value is related to
*                        coordinated universal time name according to the C-language expression, where
*                        tm_sec, tm_min, tm_hour, tm_yday, and tm_year are all integer types:
*
*                            tm_sec + tm_min*60 + tm_hour*3600 + tm_yday*86400 +
*                           (tm_year-70)*31536000 + ((tm_year-69)/4)*86400 -
*                           ((tm_year-1)/100)*86400 + ((tm_year+299)/400)*86400"
*
*                   (b) "The relationship between the actual time of day and the current value for
*                        seconds since the Epoch is unspecified."
*
*               (2) The expression for the time value can be rewritten :
*
*                       time_val = tm_sec + 60 * (tm_min  +
*                                           60 * (tm_hour +
*                                           24 * (tm_yday + ((tm_year-69)/4) - ((tm_year-1)/100) + ((tm_year+299)/400) +
*                                          365 * (tm_year - 70))))
*********************************************************************************************************
*/

struct  fs_tm  *fs_localtime_r (const  fs_time_t      *p_ts,
                                       struct  fs_tm  *p_time)
{
    CLK_DATE_TIME stime;
    CLK_TS_SEC    ts;
    CPU_BOOLEAN   success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_ts == (const fs_time_t *)0) {                         /* Validate timestamp val ptr.                          */
        return ((struct fs_tm *)0);
    }
    if (p_time == (struct fs_tm *)0) {                          /* Validate time ptr.                                   */
        return ((struct fs_tm *)0);
    }
#endif



                                                                /* ------------ CALC BROKEN-DOWN DATE/TIME ------------ */
    ts = (CLK_TS_SEC)*p_ts;

    success = Clk_TS_UnixToDateTime(              ts,
                                    (CLK_TZ_SEC)  0,
                                                 &stime);

    if (success != DEF_YES) {
        return ((struct fs_tm *)0);
    }


    p_time->tm_sec  = (int)stime.Sec;
    p_time->tm_min  = (int)stime.Min;
    p_time->tm_hour = (int)stime.Hr;
    p_time->tm_mday = (int)stime.Day;
    p_time->tm_mon  = (int)stime.Month;
    p_time->tm_year = (int)stime.Yr;
    p_time->tm_wday = (int)stime.DayOfWk;
    p_time->tm_yday = (int)stime.DayOfYr;

    return (p_time);
}


/*
*********************************************************************************************************
*                                             fs_mktime()
*
* Description : Convert date/time to timestamp.
*
* Argument(s) : p_time      Pointer to date/time to convert.
*
* Return(s)   : Time value,    if NO errors.
*               (fs_time_t)-1, otherwise.
*
* Note(s)     : (1) See 'fs_localtime_r()  Note #1'.
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'mktime() : DESCRIPTION' states that :
*
*                   (a) "The 'mktime()' function shall convert the broken-down time, expressed as local
*                        time, in the structure pointed to by 'timeptr', into a time since the Epoch"
*
*                   (b) "The original values of 'tm_wday' and 'tm_yday' components of the structure are
*                        ignored, and the original values of the other components are not restricted to
*                        the ranges described in <time.h>" (see also Note #3)
*
*                   (c) "Upon successful completion, the values of the 'tm_wday' and 'tm_yday' components
*                        of the structure shall be set appropriately, and the other components set to
*                        represent the specified time since the Epoch, but with their values forced to the
*                        ranges indicated in the <time.h> entry"
*
*               (3) Even though strict range checking is NOT performed, the broken-down date/time
*                   components are restricted to positive values, and the month value MUST be between
*                   0 & 11 (otherwise, the day of year cannot be determined).
*********************************************************************************************************
*/

fs_time_t  fs_mktime (struct  fs_tm  *p_time)
{
    CLK_DATE_TIME stime;
    CLK_TS_SEC    ts;
    CPU_BOOLEAN   success;

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_time == (struct fs_tm *)0) {                          /* Validate broken-down date/time ptr.                  */
        return ((fs_time_t)-1);
    }
#endif



                                                                /* ----------------- COMPUTE TIME VAL ----------------- */
    stime.Sec     = (CPU_INT08U)p_time->tm_sec;
    stime.Min     = (CPU_INT08U)p_time->tm_min;
    stime.Hr      = (CPU_INT08U)p_time->tm_hour;
    stime.Day     = (CPU_INT08U)p_time->tm_mday;
    stime.Month   = (CPU_INT08U)p_time->tm_mon;
    stime.Yr      = (CPU_INT08U)p_time->tm_year;

    success = Clk_DateTimeToTS_Unix(&ts,
                                    &stime);
    if (success != DEF_YES) {
        return ((fs_time_t)-1);
    }

    p_time->tm_sec  = (int)stime.Sec;
    p_time->tm_min  = (int)stime.Min;
    p_time->tm_hour = (int)stime.Hr;
    p_time->tm_mday = (int)stime.Day;
    p_time->tm_mon  = (int)stime.Month;
    p_time->tm_year = (int)stime.Yr;

    return ((fs_time_t)ts);
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_api.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of API module include (see Note #1).             */
