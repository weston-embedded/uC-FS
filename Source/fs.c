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
*                                       FILE SYSTEM SOURCE FILE
*
* Filename : fs.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_buf.h"
#include  "fs_ctr.h"
#include  "fs_dev.h"
#include  "fs_dir.h"
#include  "fs_entry.h"
#include  "fs_file.h"
#include  "fs_sys.h"
#include  "fs_vol.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_MAX_FULL_PATH_LEN               FS_CFG_MAX_PATH_NAME_LEN + FS_CFG_MAX_VOL_NAME_LEN


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

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
typedef  struct  fs_working_dir  FS_WORKING_DIR;

struct  fs_working_dir {
    CPU_CHAR        *Name;
    FS_WORKING_DIR  *NextPtr;
};
#endif


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

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  FS_WORKING_DIR  *FS_WorkingDirListFreePtr;
static  FS_WORKING_DIR  *FS_WorkingDirListWaitPtr;
static  FS_CTR           FS_WorkingDirCtr;
static  FS_CTR           FS_WorkingDirFreeCtr;
#endif

static  FS_SEC_SIZE      FS_MaxSecSize;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  void       FS_WorkingDirModuleInit     (void);

static  void       FS_WorkingDirPathFormHandler(CPU_CHAR  *path_work,
                                                CPU_CHAR  *path_raw,
                                                CPU_CHAR  *path_entry,
                                                FS_ERR    *p_err);

static  CPU_CHAR  *FS_WorkingDirObjGet         (void);                  /* Get working dir obj.                             */
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                              FS_Init()
*
* Description : Initialize file system suite.
*
* Argument(s) : p_fs_cfg    Pointer to file system configuration.
*
* Return(s)   : FS_ERR_NONE,                        if NO errors.

*               Specific initialization error code, otherwise.
*
* Note(s)     : (1) FS_Init() MUST be called ...
*
*                   (a) ONLY ONCE from a product's application; ...
*                   (b) (1) AFTER  product's OS has been initialized
*                       (2) BEFORE product's application calls any file system suite function(s)
*
*               (2) FS_Init() MUST ONLY be called ONCE from product's application.
*
*               (3) (a) If any file system initialization error occurs, any remaining file system
*                       initialization is immediately aborted & the specific initialization error code is
*                       returned.
*
*                   (b) File system error codes are listed in 'fs_err.h', organized by file system
*                       modules &/or layers.  A search of the specific error code number(s) provides the
*                       corresponding error code label(s).  A search of the error code label(s) provides
*                       the source code location of the file system initialization error(s).
*
*********************************************************************************************************
*/

FS_ERR  FS_Init (FS_CFG  *p_fs_cfg)
{
    FS_ERR  err;


                                                                /* ---------------------- CHK CFG --------------------- */
    if (p_fs_cfg == (FS_CFG *)0) {                              /* Validate ptr.                                        */
        return (FS_ERR_NULL_PTR);
    }

                                                                /* Validate cfg'd cnt range.                            */
    if ((p_fs_cfg->DevCnt    < 1u) ||
        (p_fs_cfg->VolCnt    < 1u) ||
        (p_fs_cfg->FileCnt   < 1u) ||
        (p_fs_cfg->DevDrvCnt < 1u)) {
        return (FS_ERR_INVALID_CFG);
    }

    if (p_fs_cfg->BufCnt < (p_fs_cfg->VolCnt * 2u)) {
        return (FS_ERR_INVALID_CFG);
    }

    if ((p_fs_cfg->MaxSecSize !=  512u) &&
        (p_fs_cfg->MaxSecSize != 1024u) &&
        (p_fs_cfg->MaxSecSize != 2048u) &&
        (p_fs_cfg->MaxSecSize != 4096u)) {
        return (FS_ERR_INVALID_CFG);
    }

    FS_MaxSecSize = p_fs_cfg->MaxSecSize;


                                                                /* ------------------ INIT FS MODULES ----------------- */
    FSDev_ModuleInit(p_fs_cfg->DevCnt,
                     p_fs_cfg->DevDrvCnt,  &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }

    FSBuf_ModuleInit(p_fs_cfg->BufCnt,
                     p_fs_cfg->MaxSecSize, &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }

    FSFile_ModuleInit(p_fs_cfg->FileCnt,   &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }

#ifdef FS_DIR_MODULE_PRESENT
    FSDir_ModuleInit(p_fs_cfg->DirCnt,     &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }
#endif

    FSVol_ModuleInit(p_fs_cfg->VolCnt,     &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }

    FSSys_ModuleInit(p_fs_cfg->VolCnt,
                     p_fs_cfg->FileCnt,
                     p_fs_cfg->DirCnt,     &err);
    if (err != FS_ERR_NONE) {
        return (err);
    }



                                                                /* ------------------ INIT WORK DIRS ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    FS_WorkingDirModuleInit();
#endif

    return (FS_ERR_NONE);
}


/*
*********************************************************************************************************
*                                           FS_VersionGet()
*
* Description : Get file system suite software version.
*
* Argument(s) : none.
*
* Return(s)   : File system suite software version.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  FS_VersionGet (void)
{
    CPU_INT16U  ver;


    ver = FS_VERSION;

    return (ver);
}


/*
*********************************************************************************************************
*                                         FS_MaxSecSizeGet()
*
* Description : Get maximum sector size.
*
* Argument(s) : none.
*
* Return(s)   : Maximum sector size, in octets.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_SIZE_T  FS_MaxSecSizeGet (void)
{
    CPU_SIZE_T  sec_size;


    sec_size = (CPU_SIZE_T)FS_MaxSecSize;

    return (sec_size);
}


/*
*********************************************************************************************************
*                                         FS_WorkingDirGet()
*
* Description : Get the working directory for the current task.
*
* Argument(s) : path_dir    String buffer that will receive the working directory path.
*
*               size        Size of string buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  Working directory obtained.
*                               FS_ERR_NULL_PTR              Argument 'path_dir' passed a NULL pointer.
*                               FS_ERR_NULL_ARG              Argument 'size' passed a NULL value.
*                               FS_ERR_NAME_BUF_TOO_SHORT    Argument 'size' less than length of path.
*                               FS_ERR_VOL_NONE_EXIST        No volumes exist.
*
* Return(s)   : none.
*
* Note(s)     : (1) If no working directory is assigned for the task, the default working directory--
*                   the root directory on the default volume--will be returned in the user buffer & set
*                   as the task's working directory.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_WorkingDirGet (CPU_CHAR    *path_dir,
                        CPU_SIZE_T   size,
                        FS_ERR      *p_err)
{
    CPU_SIZE_T   len;
    CPU_CHAR    *path_buf;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (path_dir == (CPU_CHAR *)0) {                            /* Validate dir path ptr.                               */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
    if (size < 1u) {                                            /* Validate size.                                       */
       *p_err = FS_ERR_INVALID_ARG;
        return;
    }
#endif

    path_dir[0] = (CPU_CHAR)ASCII_CHAR_NULL;



                                                                /* ------------------ GET WORKING DIR ----------------- */
    path_buf = FS_OS_WorkingDirGet();                           /* Find  working dir for task.                          */

    if (path_buf == (CPU_CHAR *)0) {                            /* If no working dir for task ...                       */
        path_buf =   FS_WorkingDirObjGet();                     /*                            ... alloc working dir ... */
        if (path_buf == (CPU_CHAR *)0) {
           *p_err = FS_ERR_WORKING_DIR_NONE_AVAIL;
            return;
        }
        FSVol_GetDfltVolName(path_buf, p_err);                  /*                            ... get dflt vol.         */
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (path_buf[0] == (CPU_CHAR)ASCII_CHAR_NULL) {         /* If no vol exists ...                                 */
           *p_err = FS_ERR_VOL_NONE_EXIST;                      /*                  ... rtn err.                        */
            FS_WorkingDirObjFree(path_buf);
            return;
        }

        Str_Cat(path_buf, FS_STR_PATH_SEP);
        len = Str_Len_N(path_buf, size + 1u);
        if (len >= size) {                                      /* If vol name too long ...                             */
           *p_err = FS_ERR_NAME_BUF_TOO_SHORT;                  /*                      ... rtn err.                    */
            FS_WorkingDirObjFree(path_buf);
            return;
        }
        Str_Copy(path_dir, path_buf);                           /* Copy dflt dir name to app buf.                       */

        FS_OS_WorkingDirSet(path_buf, p_err);                   /* Set working dir for task.                            */
        if(*p_err != FS_ERR_NONE) {
            FS_WorkingDirObjFree(path_buf);
            return;
        }

       *p_err = FS_ERR_NONE;


    } else {                                                    /* If working dir exists ...                            */
        len = Str_Len_N(path_buf, size + 1u);
        if (len >= size) {
           *p_err = FS_ERR_NAME_BUF_TOO_SHORT;
            return;
        }
        Str_Copy(path_dir, path_buf);                           /*                       ... copy to app buf.           */

       *p_err = FS_ERR_NONE;
    }
}
#endif


/*
*********************************************************************************************************
*                                         FS_WorkingDirSet()
*
* Description : Set the working directory for the current task.
*
* Argument(s) : path_dir    String that specifies EITHER...
*                               (a) the absolute working directory path to set;
*                               (b) a relative path that will be applied to the current working directory.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                      Working directory set.
*                               FS_ERR_NAME_INVALID              Name invalid.
*                               FS_ERR_NAME_PATH_TOO_LONG        Path too long (see Note #4).
*                               FS_ERR_NAME_NULL                 Argument 'path_dir' passed a NULL pointer.
*                               FS_ERR_VOL_NONE_EXIST            No volumes exist.
*                               FS_ERR_WORKING_DIR_NONE_AVAIL    No working directories available.
*                               FS_ERR_WORKING_DIR_INVALID       Argument 'path_dir' passed an invalid
*                                                                    directory.
*
* Return(s)   : none.
*
* Note(s)     : (1) The new working directory is formed in several steps :
*
*                   (a) (1) If 'path_dir' begins with the path separator character (slash, '\'), it will
*                           be interpreted as an absolute directory path on the current volume.  The
*                           preliminary working directory path is formed by the concatenation of the
*                           current volume name & 'path_dir'. See also Note #2b.
*                       (2) Otherwise, if 'path_dir' begins with a volume name, it will be interpreted
*                           as an absolute directory path & will become the preliminary working directory.
*                       (3) Otherwise, the preliminary working directory path is formed by the concatenation
*                           of the current working directory, a path separator character & 'path_dir'.
*
*                   (b) The preliminary working directory path is then resolved, from the first to last
*                       path component :
*                       (1) If the component is a "dot" component, it is removed.
*                       (2) If the component is a "dot dot" component & the preliminary working directory
*                           path is not a root directory, the previous path component is removed.  In any
*                           case, the "dot dot" component is removed.  See also Note #3.
*                       (3) Trailing path separator characters are removed, & multiple consecutive path
*                           separator characters are replaced by a single path separator character.
*
*                   (c) The volume is examined to determine whether the preliminary working directory
*                       exists.  If it does, it becomes the new working directory.  Otherwise, an error
*                       is output, & the working directory is unchanged.
*
*               (2) (a) If 'path_dir' does not begin with a volume name or a path separator character &
*                       no working directory is assigned for the task, the path will be interpreted as
*                       relative the default working directory--the root directory on the default
*                       volume.
*
*                   (b) If 'path_dir' begins with a path separator character :
*                       (a) ... & no working directory is assigned for the task, the default volume will
*                               be taken as the 'current' volume.
*                       (b) ... otherwise, the volume of the working directory assigned for the task will
*                               be taken as the 'current' volume.
*
*               (3) The logical resolution of "dot dot" path components implies that some intermediate
*                   directories in the resolution process may not be required to exist.  For example, if
*
*                       path_dir                  = "..\dir3\.."
*                       Current Working Directory = "sdcard:0:\dir1\dir2"
*
*                   the intermediate directory
*
*                       "sdcard:0:\dir1\dir3"
*
*                   arrived at after the first two components of 'path_dir' are handled, would not need
*                   to exist, since the leaf directory "dir3" will be removed when the third component of
*                   'path_dir' is handled.
*
*               (4) The final working directory path & EACH intermediate directory path must be no longer
*                   than 'FS_CFG_MAX_FULL_NAME_LEN'.  For example, if
*
*                       path_dir                  = "dir3\long_dir4\..\..\dir5"
*                       Current Working Directory = "sdcard:0:\dir1\dir2"
*                       FS_CFG_MAX_FULL_NAME_LEN  =  30;
*
*                   then the final current working directory would be
*
*                       "sdcard:0:\dir1\dir2\dir5"
*
*                   which has length 24.  However, the intermediate directory
*
*                       "sdcard:0:\dir1\dir2\dir3\long_dir4"
*
*                   arrived at after the first two components of 'path_dir' are handled has length 34.
*                   Processing would stop before this intermediate path is formed since it exceeds
*                  'FS_CFG_MAX_FULL_NAME_LEN' & a 'FS_ERR_NAME_PATH_TOO_LONG' error would be returned.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_WorkingDirSet (CPU_CHAR  *path_dir,
                        FS_ERR    *p_err)
{
    CPU_BOOLEAN     alloc;
    FS_ENTRY_INFO   info;
    CPU_CHAR        path_dir_temp[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_CHAR       *path_buf;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (path_dir == (CPU_CHAR *)0) {                            /* Validate dir path ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- FORM WORKING DIR ----------------- */
    path_buf = FS_OS_WorkingDirGet();                           /* Find working dir for task.                           */

    if (path_buf == (CPU_CHAR *)0) {                            /* If no working dir for task ...                       */
        alloc = DEF_YES;                                        /*                            ... alloc working dir ... */
        FSVol_GetDfltVolName(path_dir_temp, p_err);             /*                            ... get dflt vol.         */
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (path_dir_temp[0] == (CPU_CHAR)ASCII_CHAR_NULL) {    /* If no vol exists ...                                 */
           *p_err = FS_ERR_VOL_NONE_EXIST;                      /*                  ... rtn err.                        */
            return;
        }

        Str_Cat(path_dir_temp, FS_STR_PATH_SEP);
        FS_WorkingDirPathFormHandler(path_dir_temp,             /* Form working dir path.                               */
                                     path_dir,
                                     path_dir_temp,
                                     p_err);
        if (*p_err != FS_ERR_NONE) {
             return;
        }


    } else {                                                    /* If work dir exists ...                               */
        alloc = DEF_NO;                                         /*                    ... update.                       */

        FS_WorkingDirPathFormHandler(path_buf,                  /* Form working dir path.                               */
                                     path_dir,
                                     path_dir_temp,
                                     p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }



                                                                /* ------------------ CHK WORKING DIR ----------------- */
    FSEntry_Query( path_dir_temp,                               /* Get info about dir.                                  */
                  &info,
                   p_err);
    if (*p_err != FS_ERR_NONE) {                                /* Rtn err if dir does not exist.                       */
        *p_err  = FS_ERR_WORKING_DIR_INVALID;
         return;
    }
                                                                /* Rtn err if not dir.                                  */
    if (DEF_BIT_IS_CLR(info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) {
        *p_err  = FS_ERR_WORKING_DIR_INVALID;
         return;
    }



                                                                /* ---------------- UPDATE WORKING DIR ---------------- */
    if (alloc == DEF_YES) {                                     /* If no working dir for task ...                       */
        path_buf = FS_WorkingDirObjGet();                       /*                            ... alloc working dir ... */
        if (path_buf == (CPU_CHAR *)0) {
           *p_err = FS_ERR_WORKING_DIR_NONE_AVAIL;
            return;
        }

        FS_OS_WorkingDirSet(path_buf, p_err);                   /*                            ... set task working dir. */
        if(*p_err != FS_ERR_NONE) {
            return;
        }
    }

    Str_Copy(path_buf, path_dir_temp);

    *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           FS_PathParse()
*
* Description : Extract name of volume & get pointer to beginning of file name.
*
* Argument(s) : name_full       Full file name.
*               ----------      Argument validated by caller.
*
*               name_vol        String buffer that will receive...
*               ----------      Argument validated by caller.
*                                   (a) the volume name; OR
*                                   (b) an empty string, if volume name is not specified.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                   No error.
*                                   FS_ERR_NAME_INVALID           Invalid file name or path.
*                                   FS_ERR_VOL_LABEL_TOO_LONG     Volume label is too long.
*
*
* Return(s)   : Pointer to start of file name.
*
* Note(s)     : (1) See 'FSVol_Open() Notes #1a'.
*
*               (2) 'name_vol' MUST point to a character array of 'FS_CFG_MAX_VOL_NAME_LEN + 1' characters.
*********************************************************************************************************
*/

CPU_CHAR  *FS_PathParse (CPU_CHAR  *name_full,
                         CPU_CHAR  *name_vol,
                         FS_ERR    *p_err)
{
    CPU_SIZE_T   vol_name_len;
    CPU_CHAR    *p_colon;


#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    Mem_Clr((void *)name_vol, FS_CFG_MAX_VOL_NAME_LEN + 1u);
#endif

                                                                /* ----------------- FIND FINAL COLON ----------------- */
                                                                /* Find final colon.                                    */
    p_colon = Str_Char_Last_N(name_full, FS_MAX_FULL_PATH_LEN, FS_CHAR_DEV_SEP);

    if (p_colon == (CPU_CHAR *)0) {                             /* If not found               ...                       */
        name_vol[0] = (CPU_CHAR)ASCII_CHAR_NULL;                /* ... vol name not specified ...                       */
       *p_err       =  FS_ERR_NONE;
        return (name_full);                                     /* ... file name starts at name start.                  */
    }

    if (p_colon == name_full) {                                 /* Rtn err if first char is colon.                      */
       *p_err = FS_ERR_NAME_INVALID;
        return ((CPU_CHAR *)0);
    }

    vol_name_len = (CPU_SIZE_T)(p_colon - name_full) + 1u;      /*lint !e946 !e947 Both ptrs are in str 'name_full'.    */

    if (vol_name_len > FS_CFG_MAX_VOL_NAME_LEN) {               /* Rtn err if vol name too long.                        */
       *p_err = FS_ERR_VOL_LABEL_TOO_LONG;
        return ((CPU_CHAR *)0);
    }

                                                                /* ------------------- COPY VOL NAME ------------------ */
    (void)Str_Copy_N(name_vol,
                     name_full,
                     vol_name_len);
                                                                /* Set last char to end string.                         */
    name_vol[vol_name_len] = ASCII_CHAR_NULL;



                                                                /* ----------------- RTN FILE NAME PTR ---------------- */
    name_full = p_colon + 1;
   *p_err     = FS_ERR_NONE;
    return (name_full);
}


/*
*********************************************************************************************************
*                                       FS_WorkingDirPathForm()
*
* Description : Form entry  name using working directory.
*
* Argument(s) : name_full           Name of the entry.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NAME_INVALID          Entry name could not be formed or
*                                                                        includes an invalid volume name.
*                                       FS_ERR_NAME_PATH_TOO_LONG    Entry name is too long.
*                                       FS_ERR_VOL_NONE_EXIST        No volumes exist.
*
* Return(s)   : Name of entry to use.
*
* Note(s)     : (1) If the returned name buffer is not 'name_full', then it MUST be freed later with
*                   'FS_WorkingDirObjFree()'.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
CPU_CHAR  *FS_WorkingDirPathForm (CPU_CHAR  *name_full,
                                  FS_ERR    *p_err)
{
    CPU_CHAR  *name_full_formed;
    CPU_CHAR  *p_vol_sep;


    p_vol_sep = Str_Char_Last_N(name_full, FS_MAX_FULL_PATH_LEN, FS_CHAR_DEV_SEP);
    if (p_vol_sep != (CPU_CHAR *)0) {                           /* If name begins with vol name ...                     */
       *p_err = FS_ERR_NONE;
        return (name_full);                                     /*                              ... use entry name.     */
    }

    name_full_formed = FS_WorkingDirObjGet();
    if (name_full_formed == (CPU_CHAR *)0) {
       *p_err = FS_ERR_MEM_ALLOC;
        return ((CPU_CHAR *)0);
    }


    FS_WorkingDirGet(name_full_formed,                          /* Get working dir ...                                  */
                     FS_CFG_MAX_FULL_NAME_LEN + 1u,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_WorkingDirObjFree(name_full_formed);
        return ((CPU_CHAR *)0);
    }

    FS_WorkingDirPathFormHandler(name_full_formed,              /*                 ... form path.                       */
                                 name_full,
                                 name_full_formed,
                                 p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_WorkingDirObjFree(name_full_formed);
        return ((CPU_CHAR *)0);
    }

    return (name_full_formed);
}
#endif


/*
*********************************************************************************************************
*                                       FS_WorkingDirObjFree()
*
* Description : Free a working directory object.
*
* Argument(s) : path_buf    Path buffer.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) When a task is deleted, its working directory MUST be freed with this function.
*
*               (2) For every working directory allocated, one 'FS_WORKING_DIR' object is placed on the
*                   'FS_WorkingDirListWaitPtr'.  If no items remain in the list when this function is
*                   called, too many working directories have been freed.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
void  FS_WorkingDirObjFree (CPU_CHAR  *path_buf)
{
    FS_WORKING_DIR  *p_working_dir;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_working_dir            = FS_WorkingDirListWaitPtr;

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_working_dir == (FS_WORKING_DIR *)0) {                 /* See Note #2.                                         */
        CPU_CRITICAL_EXIT();
        FS_TRACE_DBG(("FS_WorkingDirObjGet(): Could not free working dir; underrun in working dir obj's.\r\n"));
        return;
    }
#endif

    FS_WorkingDirListWaitPtr = FS_WorkingDirListWaitPtr->NextPtr;

    p_working_dir->Name      = path_buf;
    p_working_dir->NextPtr   = FS_WorkingDirListFreePtr;        /* Add to free pool.                                    */
    FS_WorkingDirListFreePtr = p_working_dir;

    FS_WorkingDirFreeCtr++;
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                        FS_WorkingDirObjGet()
*
* Description : Allocate & initialize a working directory object.
*
* Argument(s) : none.
*
* Return(s)   : Path buffer,     if found.
*               Pointer to NULL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  CPU_CHAR  *FS_WorkingDirObjGet (void)
{
    LIB_ERR          alloc_err;
    CPU_SIZE_T       octets_reqd;
    CPU_CHAR        *path_buf;
    FS_WORKING_DIR  *p_working_dir;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_WorkingDirListFreePtr == (FS_WORKING_DIR *)0) {
        p_working_dir = (FS_WORKING_DIR *)Mem_HeapAlloc( sizeof(FS_WORKING_DIR),
                                                         sizeof(CPU_INT32U),
                                                        &octets_reqd,
                                                        &alloc_err);
        if (p_working_dir == (FS_WORKING_DIR *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FS_WorkingDirObjGet(): Could not alloc mem for working dir: %d octets required.\r\n", octets_reqd + FS_CFG_MAX_FULL_NAME_LEN + 1u));
            return ((CPU_CHAR *)0);
        }
        (void)alloc_err;

        path_buf = (CPU_CHAR *)Mem_HeapAlloc( FS_CFG_MAX_FULL_NAME_LEN + 1u,
                                              sizeof(CPU_INT32U),
                                             &octets_reqd,
                                             &alloc_err);
        if (path_buf == (CPU_CHAR *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FS_WorkingDirObjGet(): Could not alloc mem for working dir: %d octets required.\r\n", octets_reqd));
            return ((CPU_CHAR *)0);
        }
        (void)alloc_err;

        FS_WorkingDirCtr++;

    } else {
        p_working_dir            = FS_WorkingDirListFreePtr;
        FS_WorkingDirListFreePtr = FS_WorkingDirListFreePtr->NextPtr;
        path_buf                 = p_working_dir->Name;
        FS_WorkingDirFreeCtr--;
    }

    p_working_dir->NextPtr   = FS_WorkingDirListWaitPtr;        /* Add to wait pool.                                    */
    FS_WorkingDirListWaitPtr = p_working_dir;
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    Mem_Clr(path_buf, FS_CFG_MAX_FULL_NAME_LEN + 1u);

    return (path_buf);
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
*                                      FS_WorkingDirModuleInit()
*
* Description : Initialize working directory module.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  void  FS_WorkingDirModuleInit (void)
{
    FS_WorkingDirListFreePtr = (FS_WORKING_DIR *)0;
    FS_WorkingDirListWaitPtr = (FS_WORKING_DIR *)0;
    FS_WorkingDirCtr         =  0u;
    FS_WorkingDirFreeCtr     =  0u;
}
#endif


/*
*********************************************************************************************************
*                                   FS_WorkingDirPathFormHandler()
*
* Description : Forms a file/directory path given a base path & a relative path.
*
* Argument(s) : path_work   Working directory path.
*               ---------   Argument validated by caller.
*
*               path_raw    "Raw" entry path.
*               --------    Argument validated by caller.
*
*               path_entry  String buffer that will receive the output entry path.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error from this function ;
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                  Path formed.
*                               FS_ERR_NAME_INVALID          Name invalid.
*                               FS_ERR_NAME_PATH_TOO_LONG    Path too long.
*
* Return(s)   : none.
*
* Note(s)     : (1) The working directory path, 'path_work' MUST ...
*                   (a) begin with a volume name;
*                   (b) be shorter than 'FS_CFG_MAX_FULL_NAME_LEN' characters;
*                   (c) be reduced (containing no ".." or "." components; &
*                   (d) be semantically correct.
*
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
static  void  FS_WorkingDirPathFormHandler (CPU_CHAR  *path_work,
                                            CPU_CHAR  *path_raw,
                                            CPU_CHAR  *path_entry,
                                            FS_ERR    *p_err)
{
    CPU_CHAR    *p_cur_dir;
    CPU_SIZE_T   cur_dir_len;
    CPU_INT16S   dot;
    CPU_INT16S   dot_dot;
    CPU_SIZE_T   full_path_len;
    CPU_CHAR    *path_base;
    CPU_CHAR    *path_rel;
    CPU_SIZE_T   rel_path_len;
    CPU_SIZE_T   rel_path_pos;
    CPU_CHAR    *p_slash;
    CPU_CHAR    *p_vol_sep;
    CPU_SIZE_T   vol_name_len;


    if (path_raw[0] == FS_CHAR_PATH_SEP) {                      /* -------------- ABS PATH ON CURRENT VOL ------------- */
        Str_Copy(path_entry, path_work);
        p_vol_sep = Str_Char_Last_N(path_entry, FS_MAX_FULL_PATH_LEN, FS_CHAR_DEV_SEP);
        if (p_vol_sep != (CPU_CHAR *)0) {
          *(p_vol_sep + 1) = (CPU_CHAR)ASCII_CHAR_NULL;
        }
        path_base = path_entry;
        path_rel  = path_raw + 1;


    } else {
        p_vol_sep = Str_Char_Last_N(path_raw, FS_CFG_MAX_PATH_NAME_LEN, FS_CHAR_DEV_SEP);


        if (p_vol_sep != (CPU_CHAR *)0) {                       /* ------------- ABS PATH ON SPECIFIED VOL ------------ */
            vol_name_len = (CPU_SIZE_T)(p_vol_sep - path_raw) + 1u; /*lint !e946 !e947 Both ptrs are in str 'path_raw'. */
            if (vol_name_len > FS_CFG_MAX_FULL_NAME_LEN) {
                path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;      /* Clr to empty str.                                    */
               *p_err = FS_ERR_NAME_PATH_TOO_LONG;
                return;
            }

            Str_Copy_N(path_entry, path_raw, vol_name_len);
          *(path_entry + vol_name_len) = (CPU_CHAR)ASCII_CHAR_NULL;

            path_raw += vol_name_len;                           /* Require path sep char after vol name.                */
            if (*path_raw != FS_CHAR_PATH_SEP) {
                path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;      /* Clr to empty str.                                    */
               *p_err = FS_ERR_NAME_INVALID;
                return;
            }

            path_base = path_entry;
            path_rel  = path_raw + 1;



        } else {                                                /* --------------------- REL PATH --------------------- */
            path_base = path_work;
            path_rel  = path_raw;
        }
    }


                                                                /* ------------------ FORM FINAL PATH ----------------- */
    Str_Copy(path_entry, path_base);

    rel_path_len  = Str_Len_N(path_rel, FS_CFG_MAX_PATH_NAME_LEN);
    if(rel_path_len == FS_CFG_MAX_PATH_NAME_LEN) {
        if(path_rel[FS_CFG_MAX_PATH_NAME_LEN] != '\0') {
           *p_err = FS_ERR_NAME_PATH_TOO_LONG;
            return;
        }
    }
    rel_path_pos  = 0u;

    full_path_len = Str_Len_N(path_entry, FS_MAX_FULL_PATH_LEN);
    if (full_path_len > 0u) {
        if (path_entry[full_path_len - 1u] == FS_CHAR_PATH_SEP) {
            path_entry[full_path_len - 1u] = (CPU_CHAR)ASCII_CHAR_NULL;
            full_path_len--;
        }
    }

    while (rel_path_pos < rel_path_len) {
                                                                /* Get first instance of '\' in rel path.               */
        p_cur_dir = path_rel + rel_path_pos;
        p_slash   = Str_Char(p_cur_dir, FS_CHAR_PATH_SEP);

        if (p_slash != (CPU_CHAR *)0) {                         /* If '\' exists in rel path ...                        */
                                                                /* ... extract text before '\' (but after prev '\').    */
            cur_dir_len = (CPU_SIZE_T)(p_slash - p_cur_dir);    /*lint !e946 !e947 Both pts are in str 'p_cur_dir'.     */


        } else {                                                /* If '\' does not exist in rel path ...                */
                                                                /* ... copy rem text.                                   */
            cur_dir_len = rel_path_len - rel_path_pos;
        }


        if (cur_dir_len > 0u) {
            dot = Str_Cmp_N(p_cur_dir, (CPU_CHAR *)".", cur_dir_len);
            if (dot != 0) {                                     /* If path component is not '.'  = current dir.         */

                dot_dot = Str_Cmp_N(p_cur_dir, (CPU_CHAR *)"..", cur_dir_len);
                if (dot_dot == 0) {                             /* If path component is '..' = next dir up.             */
                                                                /* Get last instance of '\' is full file path.          */
                    p_slash = Str_Char_Last_N(path_entry, FS_MAX_FULL_PATH_LEN, FS_CHAR_PATH_SEP);

                    if (p_slash != (CPU_CHAR *)0) {             /* If '\' exists in full file path ...                  */
                       *p_slash  = (CPU_CHAR  )0;               /* ... end file path.                                   */

                       full_path_len = Str_Len_N(path_entry, FS_MAX_FULL_PATH_LEN);

                    }


                } else {                                        /* If path component is something else ...              */
                                                                /* ... path may be too long ...                         */
                    if (cur_dir_len + full_path_len + 1u > FS_CFG_MAX_FULL_NAME_LEN) {
                        path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;
                       *p_err = FS_ERR_NAME_PATH_TOO_LONG;
                        return;
                    }

                                                                /* ... else concatenate to current file path.           */
                    Str_Cat(path_entry, FS_STR_PATH_SEP);
                    Str_Cat_N(path_entry, p_cur_dir, cur_dir_len);

                    full_path_len += cur_dir_len + 1u;
                }
            }
        }

        rel_path_pos += cur_dir_len + 1u;
    }

    if (full_path_len == 0u) {
        Str_Copy(path_entry, FS_STR_PATH_SEP);
    } else {
        if (path_entry[full_path_len - 1u] == FS_CHAR_DEV_SEP) {
            Str_Cat(path_entry, FS_STR_PATH_SEP);
        }
    }

   *p_err = FS_ERR_NONE;
}
#endif
