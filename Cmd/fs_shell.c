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
*                                          FS SHELL COMMANDS
*
* Filename : fs_shell.c
* Version  : V4.08.00
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    FS_SHELL_MODULE
#include  <clk.h>
#include  <fs_def.h>
#include  <fs_dir.h>
#include  <fs_entry.h>
#include  <fs_file.h>
#include  <fs_shell.h>
#include  <fs_vol.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_SHELL_NEW_LINE                      (CPU_CHAR *)"\r\n"
#define  FS_SHELL_EMPTY_STR                     (CPU_CHAR *)""
#define  FS_SHELL_STR_HELP                      (CPU_CHAR *)"-h"
#define  FS_SHELL_STR_PATH_SEP                  (CPU_CHAR *)"\\"
#define  FS_SHELL_STR_QUOTE                     (CPU_CHAR *)"\'"
#define  FS_SHELL_OUT_STR_LEN                               2u * FS_CFG_MAX_VOL_NAME_LEN + 50u
#define  FS_SHELL_MAX_FULL_PATH_LEN                         FS_CFG_MAX_PATH_NAME_LEN + FS_CFG_MAX_VOL_NAME_LEN
#define  FS_SHELL_MAX_ERR_STR_LEN                           100u                                                /* Must be longer than the length of any of the     */
                                                                                                                /*    messages define'd lower in this file          */
/*
*********************************************************************************************************
*                                           ERROR MESSAGES
*********************************************************************************************************
*/

#define  FS_SHELL_ERR_CANNOT_COPY               (CPU_CHAR *)"Cannot copy "
#define  FS_SHELL_ERR_CANNOT_FORMAT             (CPU_CHAR *)"Cannot format "
#define  FS_SHELL_ERR_CANNOT_GET_WORKING_DIR    (CPU_CHAR *)"Cannot get working directory"
#define  FS_SHELL_ERR_CANNOT_MAKE               (CPU_CHAR *)"Cannot make "
#define  FS_SHELL_ERR_CANNOT_OPEN               (CPU_CHAR *)"Cannot open "
#define  FS_SHELL_ERR_CANNOT_REMOVE             (CPU_CHAR *)"Cannot remove "
#define  FS_SHELL_ERR_CANNOT_SET_TIME           (CPU_CHAR *)"Cannot set time "
#define  FS_SHELL_ERR_CANNOT_SET_WORKING_DIR    (CPU_CHAR *)"Cannot set working directory "
#define  FS_SHELL_ERR_DEV_EXIST                 (CPU_CHAR *)"Device already exists "
#define  FS_SHELL_ERR_DEV_NOT_EXIST             (CPU_CHAR *)"Device does not exist "
#define  FS_SHELL_ERR_EMPTY_DIR                 (CPU_CHAR *)"Empty directory "
#define  FS_SHELL_ERR_EXIST                     (CPU_CHAR *)"File or directory already exists "
#define  FS_SHELL_ERR_ILLEGAL_PATH              (CPU_CHAR *)"Illegal path "
#define  FS_SHELL_ERR_NO_VOLUMES                (CPU_CHAR *)"No volumes present "
#define  FS_SHELL_ERR_NOT_DIR                   (CPU_CHAR *)"Not a directory "
#define  FS_SHELL_ERR_NOT_EXIST                 (CPU_CHAR *)"No such file or directory "
#define  FS_SHELL_ERR_NOT_FILE                  (CPU_CHAR *)"Not a file "
#define  FS_SHELL_ERR_NOT_ROOT_DIR              (CPU_CHAR *)"Not root directory "
#define  FS_SHELL_ERR_PATH_TOO_LONG             (CPU_CHAR *)"Path too long"
#define  FS_SHELL_ERR_ROOT_DIR                  (CPU_CHAR *)"Root directory "
#define  FS_SHELL_ERR_SAME_FILE                 (CPU_CHAR *)"Same file "
#define  FS_SHELL_ERR_UNKNOWN                   (CPU_CHAR *)"Unknown error "

/*
*********************************************************************************************************
*                                       ARGUMENT ERROR MESSAGES
*********************************************************************************************************
*/

#define  FS_SHELL_ARG_ERR_CAT                   (CPU_CHAR *)"fs_cat: usage: fs_cat [file]"
#define  FS_SHELL_ARG_ERR_CD                    (CPU_CHAR *)"fs_cd: usage: fs_cd [dir]"
#define  FS_SHELL_ARG_ERR_CP                    (CPU_CHAR *)"fs_cp: usage: fs_cp [source_file] [dest_file]\r\n              fs_cp [source_file] [dest_dir]"
#define  FS_SHELL_ARG_ERR_DATE                  (CPU_CHAR *)"fs_date: usage: fs_date\r\n                fs_date mmddhhmmccyy"
#define  FS_SHELL_ARG_ERR_DF                    (CPU_CHAR *)"fs_df: usage: fs_df {[vol]}"
#define  FS_SHELL_ARG_ERR_LS                    (CPU_CHAR *)"fs_ls: usage: fs_ls"
#define  FS_SHELL_ARG_ERR_MKDIR                 (CPU_CHAR *)"fs_mkdir: usage: fs_mkdir [dir]"
#define  FS_SHELL_ARG_ERR_MKFS                  (CPU_CHAR *)"fs_mkfs: usage: fs_mkfs {[vol]}"
#define  FS_SHELL_ARG_ERR_MOUNT                 (CPU_CHAR *)"fs_mount: usage: fs_mount [dev] [vol]"
#define  FS_SHELL_ARG_ERR_MV                    (CPU_CHAR *)"fs_mv: usage: fs_mv [source] [dest]\r\n              fs_mv [source] [dir]"
#define  FS_SHELL_ARG_ERR_OD                    (CPU_CHAR *)"fs_od: usage: fs_od [file]"
#define  FS_SHELL_ARG_ERR_PWD                   (CPU_CHAR *)"fs_pwd: usage: fs_pwd"
#define  FS_SHELL_ARG_ERR_RM                    (CPU_CHAR *)"fs_rm: usage: fs_rm [file]"
#define  FS_SHELL_ARG_ERR_RMDIR                 (CPU_CHAR *)"fs_rmdir: usage: fs_rmdir [dir]"
#define  FS_SHELL_ARG_ERR_TOUCH                 (CPU_CHAR *)"fs_touch: usage: fs_touch [file]"
#define  FS_SHELL_ARG_ERR_UMOUNT                (CPU_CHAR *)"fs_umount: usage: fs_umount [vol]"
#define  FS_SHELL_ARG_ERR_WC                    (CPU_CHAR *)"fs_wc: usage: fs_wc [file]"

/*
*********************************************************************************************************
*                                    COMMAND EXPLANATION MESSAGES
*********************************************************************************************************
*/

#define  FS_SHELL_CMD_EXP_CAT                   (CPU_CHAR *)"               Print [file] contents to terminal output."
#define  FS_SHELL_CMD_EXP_CD                    (CPU_CHAR *)"              Change the working directory to [dir]."
#define  FS_SHELL_CMD_EXP_CP                    (CPU_CHAR *)"              Copy [source_file] to [dest_file] or copy [source_file] into [dest_dir]."
#define  FS_SHELL_CMD_EXP_DATE                  (CPU_CHAR *)"                Write the date & time to terminal output, or set the system date & time."
#define  FS_SHELL_CMD_EXP_DF                    (CPU_CHAR *)"              Report disk free space."
#define  FS_SHELL_CMD_EXP_LS                    (CPU_CHAR *)"              List information about files in the current directory."
#define  FS_SHELL_CMD_EXP_MKDIR                 (CPU_CHAR *)"                 Create [dir], if it does not already exist."
#define  FS_SHELL_CMD_EXP_MKFS                  (CPU_CHAR *)"                Format [vol]."
#define  FS_SHELL_CMD_EXP_MOUNT                 (CPU_CHAR *)"                 Mount [dev] as [vol]."
#define  FS_SHELL_CMD_EXP_MV                    (CPU_CHAR *)"              Rename [source] to [dest] or move [source] to [dir]."
#define  FS_SHELL_CMD_EXP_OD                    (CPU_CHAR *)"              Dump [file] to standard output in specified format."
#define  FS_SHELL_CMD_EXP_PWD                   (CPU_CHAR *)"               Print the current working directory."
#define  FS_SHELL_CMD_EXP_RM                    (CPU_CHAR *)"              Remove [file]."
#define  FS_SHELL_CMD_EXP_RMDIR                 (CPU_CHAR *)"                 Remove [dir], if it is empty."
#define  FS_SHELL_CMD_EXP_TOUCH                 (CPU_CHAR *)"                 Change file access and modification times."
#define  FS_SHELL_CMD_EXP_UMOUNT                (CPU_CHAR *)"                  Unmount [vol]."
#define  FS_SHELL_CMD_EXP_WC                    (CPU_CHAR *)"              Determine the number of newlines, words and bytes in [file]."

/*
*********************************************************************************************************
*                                           FILE ATTRIBUTES
*********************************************************************************************************
*/

#define  FS_SHELL_ATTRIB_DIR                     DEF_BIT_00
#define  FS_SHELL_ATTRIB_ROOT_DIR                DEF_BIT_01
#define  FS_SHELL_ATTRIB_EXIST                   DEF_BIT_02
#define  FS_SHELL_ATTRIB_READ_ONLY               DEF_BIT_03
#define  FS_SHELL_ATTRIB_DEV_EXIST               DEF_BIT_04
#define  FS_SHELL_ATTRIB_MASK                   (FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST)


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

static  const  CPU_CHAR   *FSShell_Month_Name[] = {
    "jan",    "feb",    "mar",    "apr",    "may",    "jun",
    "jul",    "aug",    "sep",    "oct",    "nov",    "dec"
};


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* --------------- PATH HELPER FUNCTIONS -------------- */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSShell_FormValidPath (CPU_CHAR         *path_work,
                                            CPU_CHAR         *path_raw,
                                            CPU_CHAR         *path_entry);

static  CPU_BOOLEAN   FSShell_FormRelPath  (CPU_CHAR         *path_base,
                                            CPU_CHAR         *path_rel,
                                            CPU_CHAR         *path_entry);
#endif

static  void          FSShell_GetVolPath   (CPU_CHAR         *file_path,
                                            CPU_CHAR         *vol_path);

static  void          FSShell_PrintErr     (CPU_CHAR         *str_err,
                                            CPU_CHAR         *file_path,
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);

                                                                /* ---------------- FS HELPER FUNCTIONS --------------- */
static  CPU_INT08U    FSShell_GetAttrib    (CPU_CHAR         *file_path);

static  CPU_INT08U    FSShell_MatchAttrib  (CPU_CHAR         *file_path,
                                            CPU_INT08U        attrib_yes,
                                            CPU_INT08U        attrib_no,
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);

/*
*********************************************************************************************************
*                                           SHELL COMMANDS
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_CAT_EN    == DEF_ENABLED)
static  CPU_INT16S    FSShell_cat          (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_CD_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_cd           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_CP_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_cp           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif
#endif

#if (FS_SHELL_CFG_CMD_DATE_EN   == DEF_ENABLED)
static  CPU_INT16S    FSShell_date         (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_DF_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_df           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_LS_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_ls           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MKDIR_EN  == DEF_ENABLED)
static  CPU_INT16S    FSShell_mkdir        (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_MKFS_EN   == DEF_ENABLED)
static  CPU_INT16S    FSShell_mkfs         (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif
#endif

#if (FS_SHELL_CFG_CMD_MOUNT_EN  == DEF_ENABLED)
static  CPU_INT16S    FSShell_mount        (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MV_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_mv           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif
#endif

#if (FS_SHELL_CFG_CMD_OD_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_od           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_PWD_EN    == DEF_ENABLED)
static  CPU_INT16S    FSShell_pwd          (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_RM_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_rm           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_RMDIR_EN  == DEF_ENABLED)
static  CPU_INT16S    FSShell_rmdir        (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_TOUCH_EN  == DEF_ENABLED)
static  CPU_INT16S    FSShell_touch        (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif
#endif

#if (FS_SHELL_CFG_CMD_UMOUNT_EN == DEF_ENABLED)
static  CPU_INT16S    FSShell_umount       (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

#if (FS_SHELL_CFG_CMD_WC_EN     == DEF_ENABLED)
static  CPU_INT16S    FSShell_wc           (CPU_INT16U        argc,
                                            CPU_CHAR         *argv[],
                                            SHELL_OUT_FNCT    out_fnct,
                                            SHELL_CMD_PARAM  *p_cmd_param);
#endif

/*
*********************************************************************************************************
*                                         SHELL COMMAND TABLE
*********************************************************************************************************
*/

static  SHELL_CMD     FSShell_CmdTbl[] = {
#if (FS_SHELL_CFG_CMD_CAT_EN    == DEF_ENABLED)
    {"fs_cat",     FSShell_cat    },
#endif

#if (FS_SHELL_CFG_CMD_CD_EN     == DEF_ENABLED)
    {"fs_cd",      FSShell_cd     },
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_CP_EN     == DEF_ENABLED)
    {"fs_cp",      FSShell_cp     },
#endif
#endif

#if (FS_SHELL_CFG_CMD_DATE_EN   == DEF_ENABLED)
    {"fs_date",    FSShell_date   },
#endif

#if (FS_SHELL_CFG_CMD_DF_EN     == DEF_ENABLED)
    {"fs_df",      FSShell_df     },
#endif

#if (FS_SHELL_CFG_CMD_LS_EN     == DEF_ENABLED)
    {"fs_ls",      FSShell_ls     },
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MKDIR_EN  == DEF_ENABLED)
    {"fs_mkdir",   FSShell_mkdir  },
#endif

#if (FS_SHELL_CFG_CMD_MKFS_EN   == DEF_ENABLED)
    {"fs_mkfs",    FSShell_mkfs   },
#endif
#endif

#if (FS_SHELL_CFG_CMD_MOUNT_EN  == DEF_ENABLED)
    {"fs_mount",   FSShell_mount  },
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MV_EN     == DEF_ENABLED)
    {"fs_mv",      FSShell_mv     },
#endif
#endif

#if (FS_SHELL_CFG_CMD_OD_EN     == DEF_ENABLED)
    {"fs_od",      FSShell_od     },
#endif

#if (FS_SHELL_CFG_CMD_PWD_EN    == DEF_ENABLED)
    {"fs_pwd",     FSShell_pwd    },
#endif

#if (FS_CFG_RD_ONLY_EN          == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_RM_EN     == DEF_ENABLED)
    {"fs_rm",      FSShell_rm     },
#endif

#if (FS_SHELL_CFG_CMD_RMDIR_EN  == DEF_ENABLED)
    {"fs_rmdir",   FSShell_rmdir  },
#endif

#if (FS_SHELL_CFG_CMD_TOUCH_EN  == DEF_ENABLED)
    {"fs_touch",   FSShell_touch  },
#endif
#endif

#if (FS_SHELL_CFG_CMD_UMOUNT_EN == DEF_ENABLED)
    {"fs_umount",  FSShell_umount },
#endif

#if (FS_SHELL_CFG_CMD_WC_EN     == DEF_ENABLED)
    {"fs_wc",      FSShell_wc     },
#endif

    {0,            0              }
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           FSShell_Init()
*
* Description : Initialize Shell for FS.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if file system shell commands were added.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSShell_Init (void)
{
    SHELL_ERR    err;
    CPU_BOOLEAN  ok;


    Shell_CmdTblAdd((CPU_CHAR *)"fs", FSShell_CmdTbl, &err);

    ok = (err == SHELL_ERR_NONE) ? DEF_OK : DEF_FAIL;
    return (ok);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           SHELL COMMANDS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            FSShell_cat()
*
* Description : Print a file to the terminal output.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_cat [file]
*
*                   (b) Argument(s) : file      Path of file to print to terminal output.
*
*                   (c) Output      : File contents, in the ASCII character set.  Non-printable/non-space
*                                     characters are transmitted as full stops ("periods", character code
*                                     46).  For a more convenient display of binary files use 'fs_od'.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_CAT_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_cat (CPU_INT16U        argc,
                                 CPU_CHAR         *argv[],
                                 SHELL_OUT_FNCT    out_fnct,
                                 SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
#endif
    FS_FILE      *p_file;
    CPU_INT08U    file_buf[FS_SHELL_CFG_BUF_LEN + 1u];
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR      file_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path;
#endif
    CPU_SIZE_T    file_rd_len;
    FS_ERR        err_fs;
    CPU_SIZE_T    i;
    CPU_BOOLEAN   print;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_CAT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_CAT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_CAT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif


                                                                /* ------------------- OPEN SRC FILE ------------------ */
                                                                /* Form file path.                                      */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path, sizeof(file_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path);
    if (formed == DEF_NO) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path = argv[1];
#endif


                                                                /* ---------------------- CHK FILE -------------------- */
                                                                /* DIR       : May not be dir.                          */
                                                                /* ROOT_DIR  : May not be root dir.                     */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path,
                                                                                   FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR  |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

    p_file = FSFile_Open( file_path,                            /* Open file.                                           */
                          FS_FILE_ACCESS_MODE_RD,
                         &err_fs);

    if (p_file == (FS_FILE *)0) {                               /* File not opened.                                     */
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_OPEN, file_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }


                                                                /* -------------------- PRINT FILE -------------------- */
    do {
        file_rd_len = FSFile_Rd(p_file, file_buf, sizeof(file_buf), &err_fs);
        file_buf[file_rd_len] = (CPU_CHAR)ASCII_CHAR_NULL;

        if (file_rd_len > 0u) {
            for (i = 0u; i < file_rd_len; i++) {
                print = ((ASCII_IS_PRINT(file_buf[i]) == DEF_YES) || (ASCII_IS_SPACE(file_buf[i]) == DEF_YES)) ? DEF_YES : DEF_NO;
                if (print == DEF_NO) {
                    file_buf[i] = (CPU_CHAR)ASCII_CHAR_FULL_STOP;
                }
            }
            (void)out_fnct((CPU_CHAR *)file_buf, (CPU_INT16U)file_rd_len, p_cmd_param->pout_opt);
        }

    } while (file_rd_len > 0u);


                                                                /* -------------------- CLOSE & RTN ------------------- */
    FSFile_Close(p_file, &err_fs);                              /* Close src file.                                      */
    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);
    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_cd()
*
* Description : Change the working directory.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_cd [dir]
*
*                   (b) Argument(s) : dir       Absolute directory path.
*                                                   OR
*                                               Path relative to current working directory.
*
*                   (c) Output      : none.
*
*               (2) The new working directory is formed in three steps :
*
*                   (a) (1) If 'dir' begins with the path separator character (slash, '\'), it will
*                           be interpreted as an absolute directory path on the current volume.  The
*                           preliminary working directory path is formed by the concatenation of the
*                           current volume name & 'dir'.
*                       (2) Otherwise, if 'dir' begins with a volume name, it will be interpreted
*                           as an absolute directory path & will become the preliminary working directory.
*                       (3) Otherwise, the preliminary working directory path is formed by the concatenation
*                           of the current working directory, a path separator character & 'dir'.
*
*                   (b) The preliminary working directory is then resolved, from the first to last path
*                       component :
*                       (1) If the component is a 'dot' component, it is removed.
*                       (2) If the component is a 'dot dot' component, & the preliminary working
*                           directory path is not a root directory, the previous path component is
*                           removed.  In any case, the 'dot dot' component is removed.
*                       (3) Trailing path separator characters are removed, & multiple consecutive path
*                           separator characters are replaced by a single path separator character.
*
*                   (c) The volume is examined to determine whether the preliminary working directory
*                       exists.  If it does, it becomes the new working directory.  Otherwise, an error
*                       is output, & the working directory is unchanged.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_CD_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_cd (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_INT08U    attrib;
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      cwd_path_next[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    FS_ERR        err_fs;
#endif


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_CD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_CD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_CD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }


                                                                /* ------------------- FORM DIR PATH ------------------ */
                                                                /* Form file path.                                      */
    Mem_Clr(cwd_path_next, sizeof(cwd_path_next));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], cwd_path_next);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }


                                                                /* ------------------- CHK DIR PATH ------------------- */
                                                                /* DIR       : Must be dir.                             */
                                                                /* ROOT_DIR  : May be root dir or not.                  */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(cwd_path_next,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                                       FS_SHELL_ATTRIB_ROOT_DIR |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    } else {
        (void)Str_Copy(p_cwd_path, cwd_path_next);
        return (SHELL_ERR_NONE);
    }

#else
    FS_WorkingDirSet(argv[1], &err_fs);
    if (err_fs == FS_ERR_NONE) {
        return (SHELL_ERR_NONE);
    } else {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_SET_WORKING_DIR, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_cp()
*
* Description : Copy a file.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_cp [source_file] [dest_file]
*
*                                     fs_cp [source_file] [dest_dir]
*
*                   (b) Argument(s) : source_file   Source file path.
*
*                                     dest_file     Destination file path.
*
*                                     dest_dir      Destination directory path.
*
*                   (c) Output      : none.
*
*               (2) (a) In the first form of this command, neither argument may be an existing directory.
*                       The contents of 'source_file' will be copied to a file named 'dest_file' located
*                       in the same directory as 'source_file'.
*
*                   (b) In the second form of this command, the first argument must not be an existing
*                       directory and the second argument must be an existing directory.  The contents of
*                       'source_file' will be copied to a file with name formed by concatenating
*                       'dest_dir', a path separator character and the final component of 'source_file'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN      == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_CP_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_cp (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
    FS_ERR        err;
    CPU_CHAR     *p_file_name_src;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      file_path_src[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_CHAR      file_path_dest[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path_src;
    CPU_CHAR     *file_path_dest;
#endif
    CPU_SIZE_T    len_dest;
    CPU_SIZE_T    len_src_name;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_CP, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_CP, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 3u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_CP, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif


                                                                /* ---------------- FORM SRC FILE PATH ---------------- */
                                                                /* Form file path.                                      */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path_src, sizeof(file_path_src));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path_src);
    if (formed == DEF_NO) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path_src = argv[1];
#endif

                                                                /* ---------------- FORM DEST FILE PATH --------------- */
                                                                /* Form file path.                                      */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path_dest, sizeof(file_path_dest));
    formed = FSShell_FormValidPath(p_cwd_path, argv[2], file_path_dest);
    if (formed == DEF_NO) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[2], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path_dest = argv[2];
#endif

                                                                /* ------------------- CHK SRC PATH ------------------- */
                                                                /* DIR       : Must not be dir.                         */
                                                                /* ROOT_DIR  : Must not be root dir.                    */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path_src,
                                                                                     FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR   | FS_SHELL_ATTRIB_ROOT_DIR  |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }


                                                                /* ------------------- CHK DEST PATH ------------------ */
                                                                /* DIR       : May be dir or file.                      */
                                                                /* ROOT_DIR  : May be root dir or not.                  */
                                                                /* EXIST     : May exist or not.                        */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path_dest,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

                                                                /* Dest exists & is a dir ...                           */
    if (DEF_BIT_IS_SET(attrib, FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_DIR) == DEF_YES) {
            p_file_name_src = Str_Char_Last_N(file_path_src, FS_SHELL_MAX_FULL_PATH_LEN, (CPU_CHAR)ASCII_CHAR_REVERSE_SOLIDUS);
            if (p_file_name_src == (CPU_CHAR *)0) {             /* ... append src file name.                            */
                p_file_name_src = file_path_src;
            } else {
                p_file_name_src++;
            }

            len_src_name = Str_Len_N(p_file_name_src, FS_CFG_MAX_FILE_NAME_LEN);
            len_dest     = Str_Len_N(file_path_dest,  FS_CFG_MAX_PATH_NAME_LEN);
            if (len_dest + len_src_name + 1u > FS_CFG_MAX_FULL_NAME_LEN) {
                FSShell_PrintErr(FS_SHELL_ERR_PATH_TOO_LONG, (CPU_CHAR *)0, out_fnct, p_cmd_param);
                return (SHELL_EXEC_ERR);
            }
            if (len_dest != 1u) {
                (void)Str_Cat(file_path_dest, (CPU_CHAR *)"\\");
            }
            (void)Str_Cat(file_path_dest, p_file_name_src);
    }

    if (Str_Cmp_N(file_path_src, file_path_dest, FS_SHELL_MAX_FULL_PATH_LEN + 1u) == 0) {
        FSShell_PrintErr(FS_SHELL_ERR_SAME_FILE, file_path_dest, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

    FSEntry_Copy( file_path_src,
                  file_path_dest,
                  DEF_YES,
                 &err);
    if (err != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_COPY, file_path_src, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }



                                                                /* -------------------- CLOSE & RTN ------------------- */
    return (SHELL_ERR_NONE);
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_date()
*
* Description : Write the date & time to terminal output, or set the system date & time.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_date
*
*                                     fs_date  [time]
*
*                   (b) Argument(s) : time      If specified, time to set, in the form 'mmddhhmmccyy'
*                                                   where the 1st mm   is the month  (1-12);
*                                                         the     dd   is the day    (1-29, 30 or 31);
*                                                         the     hh   is the hour   (0-23)
*                                                         the 2nd mm   is the minute (0-59)
*                                                         the     ccyy is the year   (1900 or larger)
*
*                   (c) Output      : If no argument, date & time.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_DATE_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_date (CPU_INT16U        argc,
                                  CPU_CHAR         *argv[],
                                  SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_BOOLEAN   dig;
    CPU_SIZE_T    i;
    CPU_SIZE_T    len;
    CPU_BOOLEAN   ok;
    CPU_CHAR      out_str[27];
    CLK_DATE_TIME stime;
    CPU_INT16U    year;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_DATE, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_DATE, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if ((argc != 1u) &&
        (argc != 2u)) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_DATE, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

    stime.Sec      = 0u;
    stime.Min      = 0u;
    stime.Hr       = 0u;
    stime.Day      = 0u;
    stime.Month    = 0u;
    stime.Yr       = 0u;
    stime.DayOfWk  = 0u;
    stime.DayOfYr  = 0u;



    if (argc == 1u) {                                           /* ------------------- GET DATE/TIME ------------------ */

        ok = Clk_GetDateTime(&stime);
        if (ok == DEF_YES) {
            ok =  Clk_DateTimeToStr(            &stime,         /* Convert date/time to str.                            */
                                                 FS_TIME_FMT,
                                    (CPU_CHAR *) out_str,
                                                 FS_TIME_STR_MIN_LEN);
        }

        if (ok == DEF_YES) {                                    /* Wr date/time.                                        */
            (void)out_fnct(out_str, 26u, p_cmd_param->pout_opt);
        } else {
            (void)out_fnct((char *)"Time could not be gotten.", 25u, p_cmd_param->pout_opt);
            (void)out_fnct(        FS_SHELL_NEW_LINE,            2u, p_cmd_param->pout_opt);
        }

    } else {                                                    /* ------------------- SET DATE/TIME ------------------ */
        len = Str_Len_N(argv[1], 13u);
        ok  = DEF_FAIL;
        if (len == 12u) {
            ok = DEF_OK;
            for (i = 0u; i < 12u; i++) {
                dig = ASCII_IS_DIG(argv[1][i]);
                if (dig == DEF_NO) {
                    ok  =  DEF_FAIL;
                }
            }
        }

        if (ok == DEF_OK) {
            stime.Month  = ((CPU_INT08U)argv[1][1]  - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT08U)argv[1][0]  - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);
            stime.Day    = ((CPU_INT08U)argv[1][3]  - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT08U)argv[1][2]  - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);
            stime.Hr     = ((CPU_INT08U)argv[1][5]  - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT08U)argv[1][4]  - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);
            stime.Min    = ((CPU_INT08U)argv[1][7]  - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT08U)argv[1][6]  - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);

            year          = ((CPU_INT16U)argv[1][9]  - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT16U)argv[1][8]  - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);
            year         *=  DEF_NBR_BASE_DEC;
            year         *=  DEF_NBR_BASE_DEC;
            year         += ((CPU_INT16U)argv[1][11] - ASCII_CHAR_DIGIT_ZERO) + (((CPU_INT16U)argv[1][10] - ASCII_CHAR_DIGIT_ZERO) * DEF_NBR_BASE_DEC);
            if ((year < CLK_UNIX_EPOCH_YR_START) || (year > CLK_UNIX_EPOCH_YR_END)) {
                ok = DEF_FAIL;
            } else {
                stime.Yr = year;
            }
        }

        if (ok != DEF_OK) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_DATE, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_EXEC_ERR);
        }

        ok = Clk_SetDateTime(&stime);
        if (ok != DEF_YES) {
            (void)out_fnct((char *)"Time could not be set.", 22u, p_cmd_param->pout_opt);
            (void)out_fnct(        FS_SHELL_NEW_LINE,         2u, p_cmd_param->pout_opt);
        }
    }

    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_df()
*
* Description : Report disk free space.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_df
*
*                                     fs_df  [vol]
*
*                   (b) Argument(s) : vol       If specified, volume on which to report free space.
*
*                   (c) Output      : Name, total space, free space & used space of volume(s).
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_DF_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_df (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    FS_ERR       err;
    CPU_CHAR     out_str[FS_SHELL_OUT_STR_LEN];
    FS_QTY       ix;
    CPU_SIZE_T   len;
    FS_QTY       nbr_vols;
    CPU_INT32U   space;
    FS_VOL_INFO  vol_info;
    CPU_CHAR     vol_name[FS_CFG_MAX_VOL_NAME_LEN + 1u];


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_DF, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_DF, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if ((argc != 1u) &&
        (argc != 2u)) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_DF, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }



                                                                /* --------------------- DISP HDR --------------------- */
    Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
    Str_Copy(&out_str[FS_CFG_MAX_VOL_NAME_LEN + 16u], "512-blocks");
    (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);

    Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
    Str_Copy(out_str, "Filesystem");
    len = Str_Len_N("Filesystem", 12u);
    out_str[len] = (CPU_CHAR)ASCII_CHAR_SPACE;

    Str_Copy(&out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u], "    Used    Available     Capacity   Mounted on ");
    (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);



    if (argc == 2u) {                                           /* -------------- DISP INFO ABOUT ONE VOL ------------- */
        FSVol_Query( argv[1],
                    &vol_info,
                    &err);
        if (err == FS_ERR_NONE) {
            Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
            Str_Copy(out_str, argv[1]);
            len = Str_Len_N(argv[1], FS_CFG_MAX_VOL_NAME_LEN);
            out_str[len] = (CPU_CHAR)ASCII_CHAR_SPACE;

            space = vol_info.VolUsedSecCnt * (vol_info.DevSecSize / 512u);
            (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u]);

            space = vol_info.VolFreeSecCnt * (vol_info.DevSecSize / 512u);
            (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 13u]);

            space = vol_info.VolTotSecCnt  * (vol_info.DevSecSize / 512u);
            (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 26u]);

            Str_Copy(&out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 39u], argv[1]);
            (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
            (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);

        } else {
            Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(argv[1]));
            Str_Copy(out_str, vol_name);
            Str_Cat(out_str, " could not be accessed.\r\n");

            (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
        }


    } else {                                                    /* ------------- DISP INFO ABOUT ALL VOLS ------------- */
        nbr_vols = FSVol_GetVolCnt();

        for (ix = 0u; ix < nbr_vols; ix++) {
            FSVol_GetVolName(ix, vol_name, &err);
            if (err != FS_ERR_NONE) {
                Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
                Str_Copy(out_str, "Volume name could not be accessed.\r\n");

                (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);            }
            if (vol_name[0] != (CPU_CHAR)ASCII_CHAR_NULL) {
                FSVol_Query( vol_name,
                            &vol_info,
                            &err);
                if (err == FS_ERR_NONE) {
                    Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
                    Str_Copy(out_str, vol_name);
                    len = Str_Len_N(vol_name, FS_CFG_MAX_VOL_NAME_LEN);
                    out_str[len] = (CPU_CHAR)ASCII_CHAR_SPACE;

                    space = vol_info.VolUsedSecCnt * (vol_info.DevSecSize / 512u);
                    (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u]);

                    space = vol_info.VolFreeSecCnt * (vol_info.DevSecSize / 512u);
                    (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 13u]);

                    space = vol_info.VolTotSecCnt  * (vol_info.DevSecSize / 512u);
                    (void)Str_FmtNbr_Int32U(space, 8u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 26u]);

                    Str_Copy(&out_str[FS_CFG_MAX_VOL_NAME_LEN + 1u + 39u], vol_name);
                    (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
                    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);

                } else {
                    Mem_Set(out_str, (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(out_str));
                    Str_Copy(out_str, vol_name);
                    Str_Cat(out_str, " could not be accessed.\r\n");

                    (void)out_fnct(out_str, (CPU_INT16U)Str_Len_N(out_str, FS_SHELL_OUT_STR_LEN), p_cmd_param->pout_opt);
                }
            }
        }
    }

    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_ls()
*
* Description : List directory contents.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_ls
*
*                   (b) Argument(s) : none.
*
*                   (c) Output      : List of directory contents.
*
*               (2) The output resembles the output from the standard UNIX command 'ls -l'.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_LS_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_ls (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    FS_ERR         err;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR       char_last;
    CPU_CHAR      *p_cwd_path;
    CPU_SIZE_T     len;
#else
    CPU_CHAR       p_cwd_path[] = ".";
#endif

#ifdef FS_DIR_MODULE_PRESENT
    FS_DIR        *p_dir;
    FS_DIR_ENTRY   dirent;
    CPU_BOOLEAN    ok;
    CLK_DATE_TIME  stime;
#endif
    CPU_CHAR       out_str[41];


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_LS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_LS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 1u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_LS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (CPU_CHAR *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif


                                                                /* ---------------- LIST DIR CONTENTS ----------------- */
                                                                /*            0000000000011111111122222222223333333333  */
                                                                /*            0123456789012345678901234567890123456789  */
                                                                /* File str: "drw-rw-rw-      12345 Jul 20 2008 07:41 a.pdf" */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    len = Str_Len_N(p_cwd_path, FS_SHELL_MAX_FULL_PATH_LEN);
    if (len == 0u) {
        (void)Str_Cat(p_cwd_path, "\\");
    } else {
        char_last = p_cwd_path[len - 1u];
        if (char_last == ':') {
            (void)Str_Cat(p_cwd_path, "\\");
        }
    }
#endif

#ifdef FS_DIR_MODULE_PRESENT
    p_dir = FSDir_Open((CPU_CHAR *)p_cwd_path, &err);           /* Open dir ...                                         */
    if (p_dir == (FS_DIR *)0) {                                 /* ... if NULL, dir does not exist.                     */
        if (Str_Cmp_N(p_cwd_path, FS_SHELL_EMPTY_STR, FS_SHELL_MAX_FULL_PATH_LEN + 1u) == 0) {
            FSShell_PrintErr(FS_SHELL_ERR_NO_VOLUMES, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        } else {
            FSShell_PrintErr(FS_SHELL_ERR_NOT_EXIST, p_cwd_path, out_fnct, p_cmd_param);
            (void)Str_Copy((CPU_CHAR *)(p_cmd_param->pcur_working_dir), FS_SHELL_EMPTY_STR);
        }
        return (SHELL_EXEC_ERR);
    }

    FSDir_Rd(p_dir, &dirent, &err);                             /* Rd first dir entry ...                               */
    if (err != FS_ERR_NONE) {

        if (err != FS_ERR_EOF) {
            if (Str_Cmp_N(p_cwd_path, FS_SHELL_EMPTY_STR, FS_SHELL_MAX_FULL_PATH_LEN + 1u) == 0) {
                FSShell_PrintErr(FS_SHELL_ERR_NO_VOLUMES, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            } else {
                FSShell_PrintErr(FS_SHELL_ERR_NOT_EXIST, p_cwd_path, out_fnct, p_cmd_param);
                (void)Str_Copy((CPU_CHAR *)(p_cmd_param->pcur_working_dir), FS_SHELL_EMPTY_STR);
            }
        } else {
            FSShell_PrintErr(FS_SHELL_ERR_EMPTY_DIR, p_cwd_path, out_fnct, p_cmd_param);
        }

    } else {

        while ((err == FS_ERR_NONE)) {                          /* Print dir entry info.                                */
            (void)Str_Copy(out_str, (CPU_CHAR *)"drw-rw-rw- ssssssssss mmm dd yyyy hh:mm ");

            if(DEF_BIT_IS_SET(dirent.Info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) {
                out_str[0] = (CPU_CHAR)ASCII_CHAR_LATIN_LOWER_D;
            } else {
                out_str[0] = (CPU_CHAR)ASCII_CHAR_HYPHEN_MINUS;
            }

            if(DEF_BIT_IS_CLR(dirent.Info.Attrib, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
                out_str[2] = (CPU_CHAR)ASCII_CHAR_HYPHEN_MINUS;
                out_str[5] = (CPU_CHAR)ASCII_CHAR_HYPHEN_MINUS;
                out_str[8] = (CPU_CHAR)ASCII_CHAR_HYPHEN_MINUS;
            } else {
                out_str[2] = (CPU_CHAR)ASCII_CHAR_LATIN_LOWER_W;
                out_str[5] = (CPU_CHAR)ASCII_CHAR_LATIN_LOWER_W;
                out_str[8] = (CPU_CHAR)ASCII_CHAR_LATIN_LOWER_W;
            }

            Mem_Set(&out_str[11], (CPU_INT08U)ASCII_CHAR_SPACE, 10u);
            if(dirent.Info.Size == 0u) {
                if (DEF_BIT_IS_CLR(dirent.Info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) {
                    (void)Str_FmtNbr_Int32U(0u, 10u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[11]);
                }
            } else {
                (void)Str_FmtNbr_Int32U(dirent.Info.Size, 10u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[11]);
            }

            if (dirent.Info.DateTimeWr != FS_TIME_TS_INVALID) {
                ok =  Clk_TS_UnixToDateTime  (              dirent.Info.DateTimeWr,
                                              (CLK_TZ_SEC)  0,
                                                           &stime);

                if (ok == DEF_YES) {
                    (void)Str_Copy(&out_str[22], (CPU_CHAR *)FSShell_Month_Name[stime.Month - 1u]);
                    out_str[25] = (CPU_CHAR)ASCII_CHAR_SPACE;
                    (void)Str_FmtNbr_Int32U((CPU_INT32U)stime.Day,        2u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, &out_str[26]);
                    (void)Str_FmtNbr_Int32U((CPU_INT32U)stime.Yr,         4u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, &out_str[29]);
                    (void)Str_FmtNbr_Int32U((CPU_INT32U)stime.Hr,         2u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, &out_str[34]);
                    out_str[36] = (CPU_CHAR)ASCII_CHAR_COLON;
                    (void)Str_FmtNbr_Int32U((CPU_INT32U)stime.Min,        2u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, &out_str[37]);
                    out_str[39] = (CPU_CHAR)ASCII_CHAR_SPACE;
                    out_str[40] = (CPU_CHAR)ASCII_CHAR_NULL;
                }
            } else {
                Mem_Set(&out_str[22], (CPU_INT08U)ASCII_CHAR_SPACE, 17u);
                out_str[40] = (CPU_CHAR)ASCII_CHAR_NULL;
            }

            (void)out_fnct(out_str,           (CPU_INT16U)Str_Len_N(out_str, 41u),                          p_cmd_param->pout_opt);
            (void)out_fnct(dirent.Name,       (CPU_INT16U)Str_Len_N(dirent.Name, FS_CFG_MAX_PATH_NAME_LEN), p_cmd_param->pout_opt);
            (void)out_fnct(FS_SHELL_NEW_LINE, (CPU_INT16U)2,                    p_cmd_param->pout_opt);

            FSDir_Rd(p_dir, &dirent, &err);                     /* Rd next dir entry.                                   */
        }
    }
    FSDir_Close(p_dir, &err);
#else
                                                                /* $$$$ Code should be written to complete this feature */

    (void)p_cwd_path;
    (void)out_str;
    (void)FSShell_Month_Name;
    err = (FS_ERR)SHELL_ERR_CMD_NOT_FOUND;
    if (err != SHELL_ERR_NONE) {
        return (err);
    }
#endif
    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                           FSShell_mkdir()
*
* Description : Make a directory.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_mkdir [dir]
*
*                   (b) Argument(s) : dir       Directory path.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN         == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MKDIR_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_mkdir (CPU_INT16U        argc,
                                   CPU_CHAR         *argv[],
                                   SHELL_OUT_FNCT    out_fnct,
                                   SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      dir_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *dir_path;
#endif
    FS_ERR        err_fs;

                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_MKDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_MKDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_MKDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif

                                                                /* ------------------- FORM DIR PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(dir_path, sizeof(dir_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], dir_path);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    dir_path = argv[1];
#endif

                                                                /* ---------------------- CHK DIR --------------------- */
                                                                /* DIR       : May be dir or not.                       */
                                                                /* ROOT_DIR  : May not be root dir.                     */
                                                                /* EXIST     : Must not exist.                          */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(dir_path,
                                 FS_SHELL_ATTRIB_DIR |                                                     FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR  | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

                                                                /* --------------------- MAKE DIR --------------------- */
    FSEntry_Create( dir_path,
                    FS_ENTRY_TYPE_DIR,
                    DEF_YES,
                   &err_fs);

    if (err_fs != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_MAKE, dir_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    } else {
        return (SHELL_ERR_NONE);
    }
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_mkfs()
*
* Description : Format a volume.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_mkfs [vol]
*
*                   (b) Argument(s) : vol       Volume name.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN        == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MKFS_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_mkfs (CPU_INT16U        argc,
                                  CPU_CHAR         *argv[],
                                  SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S  cmp_val;
    FS_ERR      err;
    CPU_CHAR    vol_name[FS_CFG_MAX_VOL_NAME_LEN];

                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_MKFS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_MKFS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if ((argc != 1u) && (argc != 2u)) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_MKFS, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }


                                                                /* ---------------------- FMT VOL --------------------- */
    if (argc == 1u) {
        FSVol_GetDfltVolName(vol_name, &err);
        if (err != FS_ERR_NONE) {
            (void)out_fnct(FS_SHELL_ERR_NO_VOLUMES, (CPU_INT16U)Str_Len_N(FS_SHELL_ERR_NO_VOLUMES, 40u), p_cmd_param->pout_opt);
            (void)out_fnct(FS_SHELL_NEW_LINE,                   2u,                                      p_cmd_param->pout_opt);
            return (SHELL_EXEC_ERR);        }
        cmp_val = Str_Cmp_N(vol_name, FS_SHELL_EMPTY_STR, 2u);                  /* If no vol ...                                        */
        if (cmp_val == 0) {                                     /* ... rtn err.                                         */
            (void)out_fnct(FS_SHELL_ERR_NO_VOLUMES, (CPU_INT16U)Str_Len_N(FS_SHELL_ERR_NO_VOLUMES, 40u), p_cmd_param->pout_opt);
            (void)out_fnct(FS_SHELL_NEW_LINE,                   2u,                                      p_cmd_param->pout_opt);
            return (SHELL_EXEC_ERR);
        }

    } else{
       (void)Str_Copy(vol_name, argv[1]);
    }

    FSVol_Fmt(vol_name, (void *)0, &err);

    if (err != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_FORMAT, vol_name, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);
    return (SHELL_ERR_NONE);
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_mount()
*
* Description : Mount volume.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_mount  [dev]  [vol]
*
*                   (b) Argument(s) : dev       Device to mount.
*
*                                     vol       Name which will be given to volume.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_MOUNT_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_mount (CPU_INT16U        argc,
                                   CPU_CHAR         *argv[],
                                   SHELL_OUT_FNCT    out_fnct,
                                   SHELL_CMD_PARAM  *p_cmd_param)
{
    FS_ERR      err;
    CPU_INT16U  name_len;

                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_MOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_MOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 3u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_MOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }



                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open( argv[2],
                argv[1],
                0u,
               &err);

    if (err != FS_ERR_NONE) {
                    (void)      out_fnct( (char *)"Could not mount device ", 23u,                     p_cmd_param->pout_opt);
        name_len  = (CPU_INT16U)Str_Len_N(        argv[1],                   FS_CFG_MAX_DEV_NAME_LEN);
                    (void)      out_fnct(         argv[1],                   name_len,                p_cmd_param->pout_opt);
                    (void)      out_fnct( (char *)" as volume ",             11u,                     p_cmd_param->pout_opt);
        name_len  = (CPU_INT16U)Str_Len_N(        argv[1],                   FS_CFG_MAX_VOL_NAME_LEN);
                    (void)      out_fnct(         argv[2],                   name_len,                p_cmd_param->pout_opt);
                    (void)      out_fnct( (char *)".",                       1u,                      p_cmd_param->pout_opt);
                    (void)      out_fnct(         FS_SHELL_NEW_LINE,         2u,                      p_cmd_param->pout_opt);
        return (SHELL_EXEC_ERR);
    }

    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_mv()
*
* Description : Move files.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_mv [source_entry] [dest_entry]
*
*                                     fs_mv [source_entry] [dest_dir]
*
*                   (b) Argument(s) : source_entry  Source entry path.
*
*                                     dest_entry    Destination entry path.
*
*                                     dest_dir      Destination directory path.
*
*                   (c) Output      : none.
*
*               (2) (a) In the first form of this command, the second argument must not be an existing
*                       directory.  The file 'source_entry' will be renamed 'dest_entry'.
*
*                   (b) In the second form of this command, the second argument must be an existing
*                       directory.  'source_entry' will be renamed to an entry with name formed by
*                       concatenating 'dest_dir', a path separator character and the final component of
*                       'source_entry'.
*
*                   (c) In both forms, if 'source_entry' is a directory, the entire directory tree rooted
*                       at 'source_entry' will be copied and then deleted.  Additionally, both
*                       'source_entry' and 'dest_entry' or 'dest_dir' must specify locations on the same
*                       volume.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN      == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_MV_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_mv (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
    CPU_CHAR     *p_file_name_src;
    CPU_INT16U    path_len;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      file_path_src[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_CHAR      file_path_dest[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path_src;
    CPU_CHAR     *file_path_dest;
#endif
    FS_ERR        err_fs;
    CPU_SIZE_T    len_dest;
    CPU_SIZE_T    len_src_name;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_MV, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_MV, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 3u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_MV, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif

                                                                /* ---------------- FORM SRC FILE PATH ---------------- */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path_src, sizeof(file_path_src));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path_src);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path_src = argv[1];
#endif

                                                                /* ---------------- FORM DEST FILE PATH --------------- */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path_dest, sizeof(file_path_dest));
    formed = FSShell_FormValidPath(p_cwd_path, argv[2], file_path_dest);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[2], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path_dest = argv[2];
#endif

                                                                /* ------------------- CHK SRC PATH ------------------- */
                                                                /* DIR       : May be dir or file.                      */
                                                                /* ROOT_DIR  : Must not be root dir.                    */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path_src,
                                 FS_SHELL_ATTRIB_DIR |                            FS_SHELL_ATTRIB_EXIST    | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR |                            FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

                                                                /* ------------------- CHK DEST PATH ------------------ */
                                                                /* DIR       : May be dir or not.                       */
                                                                /* ROOT_DIR  : May be root dir or not.                  */
                                                                /* EXIST     : May exist or not.                        */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path_dest,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_ROOT_DIR | FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

                                                                /* Dest exists & is a dir ...                           */
    if (DEF_BIT_IS_SET(attrib, FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_DIR) == DEF_YES) {
        p_file_name_src = Str_Char_Last_N(file_path_src, FS_SHELL_MAX_FULL_PATH_LEN, (CPU_CHAR)ASCII_CHAR_REVERSE_SOLIDUS);
        if (p_file_name_src == (CPU_CHAR *)0) {                 /* ... append src file name.                            */
            p_file_name_src = file_path_src;
        } else {
            p_file_name_src++;
        }
        len_src_name = Str_Len_N(p_file_name_src, FS_SHELL_MAX_FULL_PATH_LEN);
        len_dest     = Str_Len_N(file_path_dest,  FS_SHELL_MAX_FULL_PATH_LEN);
        if (len_dest + len_src_name + 1u > FS_CFG_MAX_FULL_NAME_LEN) {
            FSShell_PrintErr(FS_SHELL_ERR_PATH_TOO_LONG, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_EXEC_ERR);
        }
        if (len_dest != 1u) {
            (void)Str_Cat(file_path_dest, (CPU_CHAR *)"\\");
        }
        (void)Str_Cat(file_path_dest, p_file_name_src);
    }

                                                                /* --------------------- MOVE FILE -------------------- */
    FSEntry_Rename( file_path_src,
                    file_path_dest,
                    DEF_NO,
                   &err_fs);

    if (err_fs != FS_ERR_NONE) {
                   (void)      out_fnct((CPU_CHAR *)"Could not move ", 15u,                        p_cmd_param->pout_opt);
        path_len = (CPU_INT16U)Str_Len_N(           file_path_src,     FS_SHELL_MAX_FULL_PATH_LEN);
                   (void)      out_fnct(            file_path_src,     path_len,                   p_cmd_param->pout_opt);
                   (void)      out_fnct((CPU_CHAR *)" to ",            4u,                         p_cmd_param->pout_opt);
        path_len = (CPU_INT16U)Str_Len_N(           file_path_src,     FS_SHELL_MAX_FULL_PATH_LEN);
                   (void)      out_fnct(            file_path_dest,    path_len,                   p_cmd_param->pout_opt);
                   (void)      out_fnct(            FS_SHELL_NEW_LINE, 2u,                         p_cmd_param->pout_opt);
        return (SHELL_EXEC_ERR);
    } else {
        return (SHELL_ERR_NONE);
    }
}
#endif
#endif


/*
*********************************************************************************************************
*                                            FSShell_od()
*
* Description : Dump file contents to terminal output.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_od [file]
*
*                   (b) Argument(s) : file          Path of file to dump to terminal output.
*
*                   (c) Output      : File contents, in hexadecimal form.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_OD_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_od (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
    FS_FILE      *p_file;
    CPU_INT08U    file_buf[FS_SHELL_CFG_BUF_LEN];
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      file_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path;
#endif
    CPU_SIZE_T    file_rd_len;
    FS_ERR        err_fs;
    CPU_SIZE_T    i;
    CPU_SIZE_T    j;
    CPU_INT32U    nbr_byte_print;
    CPU_BOOLEAN   print;
    CPU_CHAR      print_buf[33];
    CPU_INT32U    print_acc;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_OD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_OD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_OD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif

                                                                /* ------------------- OPEN SRC FILE ------------------ */
                                                                /* Form file path.                                      */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path, sizeof(file_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path = argv[1];
#endif

                                                                /* --------------------- CHK FILE --------------------- */
                                                                /* DIR       : May not be dir.                          */
                                                                /* ROOT_DIR  : May not be root dir.                     */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path,
                                                                                   FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR  |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

    p_file = FSFile_Open( file_path,                            /* Open file.                                           */
                          FS_FILE_ACCESS_MODE_RD,
                         &err_fs);

    if (p_file == (FS_FILE *)0) {                               /* File not opened.                                     */
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_OPEN, file_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

    nbr_byte_print = 0u;
                                                                /* -------------------- PRINT FILE -------------------- */
    do {
        file_rd_len = FSFile_Rd(p_file, file_buf, sizeof(file_buf), &err_fs);
        i           = 0u;

        while (i < file_rd_len) {
            if ((nbr_byte_print % 32u) == 0u) {
                (void)Str_FmtNbr_Int32U(nbr_byte_print, 8u, DEF_NBR_BASE_HEX, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, print_buf);
                print_buf[8] = (CPU_CHAR)ASCII_CHAR_SPACE;
                print_buf[9] = (CPU_CHAR)ASCII_CHAR_NULL;
                (void)out_fnct(print_buf, 9u, p_cmd_param->pout_opt);
            }

            print_acc = file_buf[i];
            if (file_rd_len - 1u > i) {
                print_acc += ((CPU_INT32U)file_buf[i + 1u] << 8);
            }
            if (file_rd_len - 2u > i) {
                print_acc += ((CPU_INT32U)file_buf[i + 2u] << 16);
            }
            if (file_rd_len - 3u > i) {
                print_acc += ((CPU_INT32U)file_buf[i + 3u] << 24);
            }

            i              += 4u;
            nbr_byte_print += 4u;

            (void)Str_FmtNbr_Int32U(print_acc, 8u, DEF_NBR_BASE_HEX, (CPU_CHAR)ASCII_CHAR_DIGIT_ZERO, DEF_NO, DEF_NO, print_buf);
            print_buf[8] = (CPU_CHAR)ASCII_CHAR_SPACE;
            print_buf[9] = (CPU_CHAR)ASCII_CHAR_NULL;
            (void)out_fnct(print_buf, 9u, p_cmd_param->pout_opt);

            if ((i % 32u) == 0u) {
                (void)out_fnct((CPU_CHAR *)"     ",  5u, p_cmd_param->pout_opt);
                for (j = 0u; j < 32u; j++) {
                    print = ASCII_IS_PRINT(file_buf[(i - 32u) + j]);
                    if (print == DEF_YES) {
                        print_buf[j] = (CPU_CHAR)file_buf[(i - 32u) + j];
                    } else {
                        print_buf[j] = (CPU_CHAR)ASCII_CHAR_FULL_STOP;
                    }
                }
                print_buf[32] = (CPU_CHAR)ASCII_CHAR_SPACE;
                (void)out_fnct(print_buf,         32u, p_cmd_param->pout_opt);
                (void)out_fnct(FS_SHELL_NEW_LINE,  2u, p_cmd_param->pout_opt);
            }
        }
    } while (file_rd_len > 0u);

                                                                /* -------------------- CLOSE & RTN ------------------- */
    (void)FSFile_Close(p_file, &err_fs);                        /* Close src file.                                      */
    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);
    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_pwd()
*
* Description : Write to terminal output pathname of current working directory.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_pwd
*
*                   (b) Argument(s) : none.
*
*                   (c) Output      : Pathname of current working directory.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_PWD_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_pwd (CPU_INT16U        argc,
                                 CPU_CHAR         *argv[],
                                 SHELL_OUT_FNCT    out_fnct,
                                 SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_CHAR  *p_cwd_path;
#if (FS_CFG_WORKING_DIR_EN == DEF_ENABLED)
    CPU_CHAR   cwd_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    FS_ERR     err_fs;
#endif

                                                                /* ------------------- CHK ARGUMENTS ------------------ */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_PWD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_PWD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 1u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_PWD, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#else
    FS_WorkingDirGet(cwd_path, sizeof(cwd_path), &err_fs);
    if (err_fs != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_GET_WORKING_DIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
    p_cwd_path = cwd_path;
#endif

    if (Str_Cmp_N(p_cwd_path, FS_SHELL_EMPTY_STR, FS_SHELL_MAX_FULL_PATH_LEN + 1u) == 0) {
        (void)out_fnct((CPU_CHAR *)"\\", 3u, p_cmd_param->pout_opt);
    } else {
        (void)out_fnct(p_cwd_path, (CPU_INT16U)Str_Len_N(p_cwd_path, FS_CFG_MAX_PATH_NAME_LEN), p_cmd_param->pout_opt);
    }

    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);
    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_rm()
*
* Description : Remove a directory entry.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_rm [entry]
*
*                   (b) Argument(s) : File or directory path.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN      == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_RM_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_rm (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      file_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path;
#endif
    FS_ERR        err_fs;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_RM, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_RM, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_RM, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif

                                                                /* ------------------- FORM FILE PATH ----------------- */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path, sizeof(file_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path = argv[1];
#endif

                                                                /* ---------------------- CHK PATH -------------------- */
                                                                /* DIR       : May not be dir.                          */
                                                                /* ROOT_DIR  : May not be root dir.                     */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path,
                                                                                  FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }

                                                                /* -------------------- REMOVE FILE ------------------- */
    FSEntry_Del( file_path,
                 FS_ENTRY_TYPE_ANY,
                &err_fs);

    if (err_fs != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_REMOVE, file_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    } else {
        return (SHELL_ERR_NONE);
    }
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_rmdir()
*
* Description : Remove a directory.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_rmdir [dir]
*
*                   (b) Argument(s) : Directory path.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN         == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_RMDIR_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_rmdir (CPU_INT16U        argc,
                                   CPU_CHAR         *argv[],
                                   SHELL_OUT_FNCT    out_fnct,
                                   SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT32U    attrib;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      dir_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *dir_path;
#endif
    FS_ERR        err_fs;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_RMDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_RMDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_RMDIR, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif

                                                                /* ------------------- FORM DIR PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(dir_path, sizeof(dir_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], dir_path);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    dir_path = argv[1];
#endif

                                                                /* ---------------------- CHK DIR --------------------- */
                                                                /* DIR       : Must be dir.                             */
                                                                /* ROOT_DIR  : May not be root dir.                     */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(dir_path,
                                 FS_SHELL_ATTRIB_DIR |                            FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                                       FS_SHELL_ATTRIB_ROOT_DIR |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }


                                                                /* -------------------- REMOVE DIR -------------------- */
    FSEntry_Del( dir_path,
                 FS_ENTRY_TYPE_DIR,
                &err_fs);

    if (err_fs != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_REMOVE, dir_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    } else {
        return (SHELL_ERR_NONE);
    }
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_touch()
*
* Description : Change file modification time.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_touch [file]
*
*                   (b) Argument(s) : File path.
*
*                   (c) Output      : none.
*
*               (2) The file modification time is set to the current time.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN         == DEF_DISABLED)
#if (FS_SHELL_CFG_CMD_RMDIR_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_touch (CPU_INT16U        argc,
                                   CPU_CHAR         *argv[],
                                   SHELL_OUT_FNCT    out_fnct,
                                   SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT32U     attrib;
    FS_ERR         err;
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR      *p_cwd_path;
    CPU_CHAR       file_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN    formed;
#else
    CPU_CHAR      *file_path;
#endif
    CLK_DATE_TIME  stime;
    CPU_BOOLEAN    ok;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_TOUCH, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_TOUCH, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_TOUCH, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif



                                                                /* ------------------- FORM FILE PATH ------------------ */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path, sizeof(file_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path);
    if (formed == DEF_FALSE) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path = argv[1];
#endif



                                                                /* ------------------- CHK FILE PATH ------------------ */
                                                                /* DIR       : May be dir or file.                      */
                                                                /* ROOT_DIR  : Must not be root dir.                    */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path,
                                 FS_SHELL_ATTRIB_DIR |                            FS_SHELL_ATTRIB_EXIST    | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR | FS_SHELL_ATTRIB_ROOT_DIR |                            FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }



                                                                /* --------------------- GET TIME --------------------- */
    ok = Clk_GetDateTime(&stime);

    if (ok != DEF_YES) {
        (void)out_fnct((char *)"Time could not be gotten.", 25u, p_cmd_param->pout_opt);
        (void)out_fnct(        FS_SHELL_NEW_LINE,            2u, p_cmd_param->pout_opt);
        return (SHELL_EXEC_ERR);
    }



                                                                /* ------------------ CHNG FILE TIME ------------------ */
    FSEntry_TimeSet(file_path, &stime, FS_DATE_TIME_MODIFY, &err);

    if (err != FS_ERR_NONE) {
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_SET_TIME, file_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    } else {
        return (SHELL_ERR_NONE);
    }
}
#endif
#endif


/*
*********************************************************************************************************
*                                           FSShell_umount()
*
* Description : Unmount volume.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_umount [vol]
*
*                   (b) Argument(s) : vol       Volume to unmount.
*
*                   (c) Output      : none.
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_UMOUNT_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_umount (CPU_INT16U        argc,
                                    CPU_CHAR         *argv[],
                                    SHELL_OUT_FNCT    out_fnct,
                                    SHELL_CMD_PARAM  *p_cmd_param)
{
    FS_ERR     err;
    CPU_INT16U vol_name_len;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_UMOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_UMOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_UMOUNT, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }



                                                                /* --------------------- CLOSE VOL -------------------- */
    FSVol_Close(argv[1], &err);

    if (err != FS_ERR_NONE) {
        vol_name_len  = (CPU_INT16U)Str_Len_N(argv[1], FS_CFG_MAX_VOL_NAME_LEN);

        (void)out_fnct((char *)"Could not unmount volume ", 25u,           p_cmd_param->pout_opt);
        (void)out_fnct(        argv[1],                     vol_name_len , p_cmd_param->pout_opt);
        (void)out_fnct((char *)".",                         1u,            p_cmd_param->pout_opt);
        (void)out_fnct(        FS_SHELL_NEW_LINE,           2u,            p_cmd_param->pout_opt);
        return (SHELL_EXEC_ERR);
    }

    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*                                            FSShell_wc()
*
* Description : Determine the number of newlines, words and bytes in a file.
*
* Argument(s) : argc            The number of arguments.
*
*               argv            Array of arguments.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : SHELL_EXEC_ERR, if an error is encountered.
*               SHELL_ERR_NONE, otherwise.
*
* Caller(s)   : Shell, in response to command execution.
*
* Note(s)     : (1) (a) Usage(s)    : fs_wc [file]
*
*                   (b) Argument(s) : file          Path of file to examine.
*
*                   (c) Output      : Number of newlines, words and bytes; equivalent to :
*
*                                           printf("%d %d %d %s", newline_cnt, word_cnt, byte_cnt, file);
*********************************************************************************************************
*/

#if (FS_SHELL_CFG_CMD_WC_EN == DEF_ENABLED)
static  CPU_INT16S  FSShell_wc (CPU_INT16U        argc,
                                CPU_CHAR         *argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U    attrib;
    CPU_INT32U    cnt_bytes;
    CPU_INT32U    cnt_newlines;
    CPU_INT32U    cnt_words;
    CPU_INT08U    file_buf[FS_SHELL_CFG_BUF_LEN];
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    CPU_CHAR     *p_cwd_path;
    CPU_CHAR      file_path[FS_CFG_MAX_FULL_NAME_LEN + 1u];
    CPU_BOOLEAN   formed;
#else
    CPU_CHAR     *file_path;
#endif
    CPU_SIZE_T    file_rd_len;
    FS_FILE      *p_file;
    FS_ERR        err_fs;
    CPU_SIZE_T    i;
    CPU_BOOLEAN   is_newline;
    CPU_BOOLEAN   is_space;
    CPU_BOOLEAN   is_word;
    CPU_CHAR      out_str[34];
    CPU_INT16U    path_len;


                                                                /* ------------------ CHK ARGUMENTS ------------------- */
    if (argc == 2u) {
        if (Str_Cmp_N(argv[1], FS_SHELL_STR_HELP, 3u) == 0) {
            FSShell_PrintErr(FS_SHELL_ARG_ERR_WC, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            FSShell_PrintErr(FS_SHELL_CMD_EXP_WC, (CPU_CHAR *)0, out_fnct, p_cmd_param);
            return (SHELL_ERR_NONE);
        }
    }

    if (argc != 2u) {
        FSShell_PrintErr(FS_SHELL_ARG_ERR_WC, (CPU_CHAR *)0, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    if (p_cmd_param == (SHELL_CMD_PARAM *)0) {
        return (SHELL_EXEC_ERR);
    } else if (p_cmd_param->pcur_working_dir == (void *)0) {
        return (SHELL_EXEC_ERR);
    } else {
        p_cwd_path = (CPU_CHAR *)(p_cmd_param->pcur_working_dir);
    }
#endif



                                                                /* ------------------ FORM FILE PATH ------------------ */
                                                                /* Form file path.                                      */
#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
    Mem_Clr(file_path, sizeof(file_path));
    formed = FSShell_FormValidPath(p_cwd_path, argv[1], file_path);
    if (formed == DEF_NO) {
        FSShell_PrintErr(FS_SHELL_ERR_ILLEGAL_PATH, argv[1], out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }
#else
    file_path = argv[1];
#endif



                                                                /* --------------------- CHK PATH --------------------- */
                                                                /* DIR       : Must not be dir.                         */
                                                                /* ROOT_DIR  : Must not be root dir.                    */
                                                                /* EXIST     : Must exist.                              */
                                                                /* READ_ONLY : May be rd-only or not.                   */
                                                                /* DEV_EXIST : Must exist.                              */
    attrib = FSShell_MatchAttrib(file_path,
                                                                                     FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_READ_ONLY | FS_SHELL_ATTRIB_DEV_EXIST,
                                 FS_SHELL_ATTRIB_DIR   | FS_SHELL_ATTRIB_ROOT_DIR  |                         FS_SHELL_ATTRIB_READ_ONLY,
                                 out_fnct,
                                 p_cmd_param);

    if (attrib == 0u) {
        return (SHELL_EXEC_ERR);
    }



                                                                /* --------------------- OPEN FILE -------------------- */
    p_file = FSFile_Open( file_path,                            /* Open file.                                           */
                          FS_FILE_ACCESS_MODE_RD,
                         &err_fs);

    if (p_file == (FS_FILE *)0) {                               /* File not opened.                                     */
        FSShell_PrintErr(FS_SHELL_ERR_CANNOT_OPEN, file_path, out_fnct, p_cmd_param);
        return (SHELL_EXEC_ERR);
    }



                                                                /* ----------------- PERFORM ANALYSIS ----------------- */
    cnt_bytes    = 0u;
    cnt_words    = 0u;
    cnt_newlines = 0u;
    is_word      = DEF_NO;
    do {
        file_rd_len = FSFile_Rd(         p_file,
                                (void *) file_buf,
                                         sizeof(file_buf),
                                        &err_fs);

        if (file_rd_len > 0u) {
            cnt_bytes += file_rd_len;

            for (i = 0u; i < file_rd_len; i++) {
                is_space = ASCII_IS_SPACE(file_buf[i]);
                if (is_space == DEF_YES) {                      /* If char is space        ...                          */
                    if (is_word == DEF_YES) {                   /* ... chk if it ends word ...                          */
                        is_word =  DEF_NO;
                        cnt_words++;
                    }
                                                                /* ... & chk if it is newline.                          */
                    is_newline = (file_buf[i] == ASCII_CHAR_LF) ? DEF_YES : DEF_NO;
                    if (is_newline == DEF_YES) {
                        cnt_newlines++;
                    }

                } else {                                        /* Any non-space char begins or continues word.         */
                    is_word = DEF_YES;
                }
            }
        }
    } while (file_rd_len > 0u);

    if (is_word == DEF_YES) {
        cnt_words++;
    }


                                                                /* -------------------- CLOSE & RTN ------------------- */
    (void)FSFile_Close(p_file, &err_fs);                        /* Close file.                                          */

    (void)Str_Copy(out_str, "nlnlnlnlnl wwwwwwwwww bbbbbbbbbb ");
    (void)Str_FmtNbr_Int32U(cnt_newlines, 10u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[0]);
    (void)Str_FmtNbr_Int32U(cnt_words,    10u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[11]);
    (void)Str_FmtNbr_Int32U(cnt_bytes,    10u, DEF_NBR_BASE_DEC, (CPU_CHAR)ASCII_CHAR_SPACE, DEF_NO, DEF_NO, &out_str[22]);

    path_len = (CPU_INT16U)Str_Len_N(argv[1], FS_SHELL_MAX_FULL_PATH_LEN);

    (void)out_fnct(out_str,           33u,      p_cmd_param->pout_opt);
    (void)out_fnct(argv[1],           path_len, p_cmd_param->pout_opt);
    (void)out_fnct(FS_SHELL_NEW_LINE, 2u,       p_cmd_param->pout_opt);

    return (SHELL_ERR_NONE);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                 LOCAL FUNCTIONS (INDEPENDENT OF FS)
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FSShell_FormValidPath()
*
* Description : Forms a file/directory path given a base path & a relative path.
*
* Argument(s) : path_work   Pointer to the Current Working Directory path.
*
*               path_raw    Pointer to the "raw" file path.
*
*               path_entry  Pointer to the output file path.
*
* Return(s)   : DEF_OK   if directory path could be formed.
*               DEF_FAIL otherwise.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSShell_FormValidPath (CPU_CHAR  *path_work,
                                            CPU_CHAR  *path_raw,
                                            CPU_CHAR  *path_entry)
{
    CPU_CHAR     *p_dev_sep;
    CPU_CHAR     *p_path_sep;
    CPU_SIZE_T    vol_name_len;
    CPU_BOOLEAN   rtn;


                                                                /* ------------------ FORM FULL PATH ------------------ */
    p_path_sep = Str_Char(path_raw, FS_CHAR_PATH_SEP);
    p_dev_sep  = Str_Char_Last_N(path_raw, FS_CFG_MAX_PATH_NAME_LEN, FS_CHAR_DEV_SEP);

                                                                /* Absolute path using the dflt vol.                    */
    if (p_path_sep == path_raw) {
        rtn = FSShell_FormRelPath((CPU_CHAR *)"", path_raw + 1, path_entry);

                                                                /* Absolute path using a specified vol.                 */
    } else if (p_dev_sep != (CPU_CHAR *)0) {
        vol_name_len = (CPU_SIZE_T)(p_dev_sep - path_raw) + 1u;
        if (vol_name_len > FS_CFG_MAX_FULL_NAME_LEN) {
            path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;
            return (DEF_NO);
        }

       (void)Str_Copy_N(path_entry, path_raw, vol_name_len);
      *(path_entry + vol_name_len) = (CPU_CHAR)ASCII_CHAR_NULL;

        path_raw += vol_name_len;                               /* Require path sep char after vol name.                */
        if (*path_raw != FS_CHAR_PATH_SEP) {
            path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;
            return (DEF_NO);
        }

        rtn = FSShell_FormRelPath(path_entry, path_raw + 1, path_entry);

    } else {                                                    /* Relative path.                                       */
        rtn = FSShell_FormRelPath(path_work, path_raw, path_entry);
    }

    return (rtn);
}
#endif


/*
*********************************************************************************************************
*                                        FSShell_FormRelPath()
*
* Description : Forms a file/directory path given a base path & a relative path.
*
* Argument(s) : path_base   The base directory path.
*
*               path_rel    The relative file/directory path.
*
*               path_entry  The output file path.
*
* Return(s)   : DEF_OK   if directory path could be formed.
*               DEF_FAIL otherwise.
*
* Caller(s)   : FSShell_FormValidPath().
*
* Note(s)     : (1) #### Handle base path with ending '\' & relative path with starting "..".
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN == DEF_DISABLED)
static  CPU_BOOLEAN  FSShell_FormRelPath (CPU_CHAR  *path_base,
                                          CPU_CHAR  *path_rel,
                                          CPU_CHAR  *path_entry)
{
    CPU_CHAR    *p_cur_dir;
    CPU_SIZE_T   cur_dir_len;
    CPU_INT16S   dot;
    CPU_INT16S   dot_dot;
    CPU_SIZE_T   full_path_len;
    CPU_SIZE_T   rel_path_len;
    CPU_SIZE_T   rel_path_pos;
    CPU_CHAR    *p_slash;


   (void)Str_Copy(path_entry, path_base);

    rel_path_len  = Str_Len_N(path_rel, FS_CFG_MAX_PATH_NAME_LEN);
    rel_path_pos  = 0u;

    full_path_len = Str_Len_N(path_entry, FS_SHELL_MAX_FULL_PATH_LEN);
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
            cur_dir_len = (CPU_SIZE_T)(p_slash - p_cur_dir);

#if 0                                                           /* See Note #1.                                         */
            if (cur_dir_len == 0) {                             /* Zero-len component (dbl '\').                        */
               (void)Str_Copy(p_path_entry, (CPU_CHAR *)"");
                return (DEF_NO);
            }
#endif

        } else {                                                /* If '\' does not exist in rel path ...                */
                                                                /* ... copy rem text.                                   */
            cur_dir_len = rel_path_len - rel_path_pos;
        }


        if (cur_dir_len > 0u) {
            dot = Str_Cmp_N(p_cur_dir, (CPU_CHAR *)".", cur_dir_len + 1u);
            if (dot != 0) {                                     /* If path component is not '.'  = current dir.         */

                dot_dot = Str_Cmp_N(p_cur_dir, (CPU_CHAR *)"..", cur_dir_len + 1u);
                if (dot_dot == 0) {                             /* If path component is '..' = next dir up.             */
                                                                /* Get last instance of '\' is full file path.          */
                    p_slash = Str_Char_Last_N(path_entry, FS_SHELL_MAX_FULL_PATH_LEN, FS_CHAR_PATH_SEP);

                    if (p_slash != (CPU_CHAR *)0) {             /* If '\' exists in full file path ...                  */
                       *p_slash  = (CPU_CHAR  )0;               /* ... end file path.                                   */

                       full_path_len = Str_Len_N(path_entry, FS_SHELL_MAX_FULL_PATH_LEN);

#if 0                                                           /* See Note #2.                                         */
                    } else {                                    /* Otherwise, a "stack underflow" occurred.             */
                       (void)Str_Copy(path_entry, (CPU_CHAR *)"");
                        return (DEF_NO);
#endif
                    }


                } else {                                        /* If path component is something else ...              */
                                                                /* ... path may be too long ...                         */
                    if (cur_dir_len + full_path_len + 1u > FS_CFG_MAX_FULL_NAME_LEN) {
                        path_entry[0] = (CPU_CHAR)ASCII_CHAR_NULL;
                        return (DEF_NO);
                    }

                                                                /* ... else concatenate to current file path.           */
                    if (full_path_len == 0u) {
                       (void)Str_Cat(path_entry, FS_STR_PATH_SEP);
                    } else {
                        if (path_entry[full_path_len - 1u] != FS_CHAR_PATH_SEP) {
                           (void)Str_Cat(path_entry, FS_STR_PATH_SEP);
                        }
                    }
                   (void)Str_Cat_N(path_entry, p_cur_dir, cur_dir_len);

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

    return (DEF_YES);
}
#endif


/*
*********************************************************************************************************
*                                        FSShell_GetVolPath()
*
* Description : Extracts volume portion of file path.
*
* Argument(s) : file_path   File path.
*
*               vol_path    String buffer in which volume path will be stored.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSShell_GetVolPath (CPU_CHAR  *file_path,
                                  CPU_CHAR  *vol_path)
{
    CPU_CHAR    *p_colon;
    CPU_SIZE_T   vol_name_len;


    p_colon = Str_Char_Last_N(file_path, FS_SHELL_MAX_FULL_PATH_LEN, FS_CHAR_DEV_SEP);

    if (p_colon == (CPU_CHAR *)0) {
       (void)Str_Copy(vol_path, FS_SHELL_EMPTY_STR);
    } else {
        vol_name_len = (CPU_SIZE_T)(p_colon - file_path) + 1u;
        if (vol_name_len > FS_CFG_MAX_VOL_NAME_LEN) {
            (void)Str_Copy(vol_path, FS_SHELL_EMPTY_STR);
        } else {
            (void)Str_Copy_N(vol_path, file_path, vol_name_len);
        }
    }
}


/*
*********************************************************************************************************
*                                         FSShell_PrintErr()
*
* Description : Prints an error.
*
* Argument(s) : str_err         Error string to print.
*
*               file_path       File/directory path to print.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSShell_PrintErr (CPU_CHAR         *str_err,
                                CPU_CHAR         *file_path,
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_SIZE_T  path_len;


    (void)out_fnct(str_err, (CPU_INT16U)Str_Len_N(str_err, FS_SHELL_MAX_ERR_STR_LEN), p_cmd_param->pout_opt);

    if (file_path != (CPU_CHAR *)0) {
        (void)out_fnct(FS_SHELL_STR_QUOTE, 1u, p_cmd_param->pout_opt);

        if (Str_Cmp_N(file_path, FS_SHELL_EMPTY_STR, 2u) == 0) {
            (void)out_fnct(FS_SHELL_STR_PATH_SEP, 1u, p_cmd_param->pout_opt);
        } else {
            path_len = Str_Len_N(file_path, FS_SHELL_MAX_FULL_PATH_LEN);

            (void)out_fnct(file_path, (CPU_INT16U)path_len, p_cmd_param->pout_opt);
        }

        (void)out_fnct(FS_SHELL_STR_QUOTE, 1u, p_cmd_param->pout_opt);
    }

    (void)out_fnct(FS_SHELL_NEW_LINE, 2u, p_cmd_param->pout_opt);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                  LOCAL FUNCTIONS (DEPENDENT ON FS)
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSShell_GetAttrib()
*
* Description : Gets attributes of a file or directory.
*
* Argument(s) : file_path   File path.
*
* Return(s)   : File attributes.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSShell_GetAttrib (CPU_CHAR  *file_path)
{
    CPU_INT08U     attrib;
    FS_FLAGS       attrib_fs;
    FS_ERR         err;
    FS_ENTRY_INFO  info;


    attrib = DEF_BIT_NONE;

    FSEntry_Query( file_path,
                  &info,
                  &err);

    switch (err) {
        case FS_ERR_NAME_INVALID:
        case FS_ERR_NAME_PATH_TOO_LONG:
        case FS_ERR_VOL_NOT_OPEN:
        case FS_ERR_VOL_NOT_MOUNTED:
             break;

        case FS_ERR_NONE:
             attrib_fs = info.Attrib;

             DEF_BIT_SET(attrib, FS_SHELL_ATTRIB_EXIST | FS_SHELL_ATTRIB_DEV_EXIST);
             if (DEF_BIT_IS_SET(attrib_fs, FS_ENTRY_ATTRIB_DIR) == DEF_YES) {
                 DEF_BIT_SET(attrib, FS_SHELL_ATTRIB_DIR);
             }

             if (DEF_BIT_IS_CLR(attrib_fs, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
                 DEF_BIT_SET(attrib, FS_SHELL_ATTRIB_READ_ONLY);
             }

             if (DEF_BIT_IS_SET(attrib_fs, FS_ENTRY_ATTRIB_DIR_ROOT) == DEF_YES) {
                 DEF_BIT_SET(attrib, FS_SHELL_ATTRIB_ROOT_DIR);
             }
             break;

        default:
             DEF_BIT_SET(attrib, FS_SHELL_ATTRIB_DEV_EXIST);
             break;
    }

    return (attrib);
}


/*
*********************************************************************************************************
*                                        FSShell_MatchAttrib()
*
* Description : Compares acceptable file attributes to actual file attributes.  If mismatched, then
*               outputs an error.
*
* Argument(s) : file_path       File path.
*
*               attrib_yes      Attributes that can be true.
*
*               attrib_no       Attributes that can be false.
*
*               out_fnct        The output function.
*
*               p_cmd_param     Pointer to the command parameters.
*
* Return(s)   : The dev attributes if the file attributes match acceptable file attributes; 0 otherwise.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT08U  FSShell_MatchAttrib(CPU_CHAR         *file_path,
                                        CPU_INT08U        attrib_yes,
                                        CPU_INT08U        attrib_no,
                                        SHELL_OUT_FNCT    out_fnct,
                                        SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U  attrib_match;
    CPU_INT08U  attrib_no_match;
    CPU_INT08U  attrib_yes_match;
    CPU_CHAR    vol_path[FS_CFG_MAX_VOL_NAME_LEN + 1u];


    attrib_match     = FSShell_GetAttrib(file_path);
    attrib_yes_match = ((CPU_INT08U)(             attrib_match & FS_SHELL_ATTRIB_MASK) &
                        (CPU_INT08U)((CPU_INT08U)~attrib_yes   & FS_SHELL_ATTRIB_MASK));
    attrib_no_match  = ((CPU_INT08U)((CPU_INT08U)~attrib_match & FS_SHELL_ATTRIB_MASK) &
                        (CPU_INT08U)((CPU_INT08U)~attrib_no    & FS_SHELL_ATTRIB_MASK));

    if ((attrib_yes_match == 0u) &&
        (attrib_no_match  == 0u)) {
        return (attrib_match);
    }

                                                                /* ------------------- DEV EXISTANCE ------------------ */
                                                                /* Dev should not exist but does.                       */
    if (DEF_BIT_IS_SET(attrib_yes_match, FS_SHELL_ATTRIB_DEV_EXIST) == DEF_YES) {
        FSShell_GetVolPath(file_path, vol_path);
        FSShell_PrintErr(FS_SHELL_ERR_DEV_EXIST, vol_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* Dev does not exist, but should.                      */
    if (DEF_BIT_IS_SET(attrib_no_match, FS_SHELL_ATTRIB_DEV_EXIST) == DEF_YES) {
        FSShell_GetVolPath(file_path, vol_path);
        FSShell_PrintErr(FS_SHELL_ERR_DEV_NOT_EXIST, vol_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* ------------------ DIR/FILE EXISTENCE -------------- */
                                                                /* Path specifies a file that doesn't exist, but should.*/
    if (DEF_BIT_IS_SET(attrib_yes_match, FS_SHELL_ATTRIB_EXIST) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_EXIST, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* Path does not specify a file that exists, but should.*/
    if (DEF_BIT_IS_SET(attrib_no_match, FS_SHELL_ATTRIB_EXIST) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_NOT_EXIST, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* ---------------------- DIR/FILE -------------------- */
                                                                /* Path specifies a dir, but should specify a file.     */
    if (DEF_BIT_IS_SET(attrib_yes_match, FS_SHELL_ATTRIB_DIR) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_NOT_FILE, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* Path specifies a file, but should specify a dir.     */
    if (DEF_BIT_IS_SET(attrib_no_match, FS_SHELL_ATTRIB_DIR) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_NOT_DIR, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* ---------------------- ROOT DIR -------------------- */
                                                                /* Path specifies a root dir, but should not.           */
    if (DEF_BIT_IS_SET(attrib_yes_match, FS_SHELL_ATTRIB_ROOT_DIR) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_ROOT_DIR, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* Path does not specify a non-root dir, but should.    */
    if (DEF_BIT_IS_SET(attrib_no_match, FS_SHELL_ATTRIB_ROOT_DIR) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_NOT_ROOT_DIR, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* --------------------- READ ONLY -------------------- */
                                                                /* Path specifies a read only file, but shouldn't.      */
    if (DEF_BIT_IS_SET(attrib_yes_match, FS_SHELL_ATTRIB_READ_ONLY) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_EXIST, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* Path doesn't specify read only file, but should.     */
    if (DEF_BIT_IS_SET(attrib_no_match, FS_SHELL_ATTRIB_READ_ONLY) == DEF_YES) {
        FSShell_PrintErr(FS_SHELL_ERR_NOT_EXIST, file_path, out_fnct, p_cmd_param);
        return (0u);
    }

                                                                /* ------------ OTHER (SHOULD NOT GET HERE) ----------- */
    FSShell_PrintErr(FS_SHELL_ERR_UNKNOWN, file_path, out_fnct, p_cmd_param);
    return (0u);
}
