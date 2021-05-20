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
* Filename     : fs_fat.c
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) Microsoft Corporation.  "Microsoft Extensible Firmware Initiative, FAT32 File
*                    System Specification."  Version 1.03.  December 6, 2000.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_def.h"
#include  "../Source/fs_dev.h"
#include  "../Source/fs_file.h"
#include  "../Source/fs_partition.h"
#include  "../Source/fs_sys.h"
#include  "../Source/fs_type.h"
#include  "../Source/fs_unicode.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_dir.h"
#include  "fs_fat_fat12.h"
#include  "fs_fat_fat16.h"
#include  "fs_fat_fat32.h"
#include  "fs_fat_file.h"
#include  "fs_fat_journal.h"
#include  "fs_fat_lfn.h"
#include  "fs_fat_sfn.h"
#include  "fs_fat_type.h"


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
*                                           LINT INHIBITION
*
* Note(s) : (1) Certain macro's within this file describe commands, command values or register fields
*               that are currently unused.  lint error option #750 is disabled to prevent error messages
*               because of unused macro's :
*
*                   "Info 750: local macro '...' (line ..., file ...) not referenced"
*********************************************************************************************************
*/

/*lint -e750*/


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_MAX_NBR_CLUS_FAT12                      4084u
#define  FS_FAT_MAX_NBR_CLUS_FAT16                     65524u
#define  FS_FAT_CLUS_NBR_TOLERANCE                        16u
#define  FS_FAT_MAX_SIZE_HUGE_FAT16                 33554432u   /*  32 MBytes                                           */

#define  FS_FAT_MAX_SIZE_FAT12                       4394304u   /*   4 Mbytes                                           */
#define  FS_FAT_MAX_SIZE_FAT16                     536870912u   /* 512 Mbytes                                           */


/*
*********************************************************************************************************
*                                           FAT TYPES DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_BS_FAT12_FILESYSTYPE            "FAT12   "

#define  FS_FAT_FAT12_CLUS_BAD                        0x0FF7u
#define  FS_FAT_FAT12_CLUS_EOF                        0x0FF8u
#define  FS_FAT_FAT12_CLUS_FREE                       0x0000u


/*
*********************************************************************************************************
*                                         DIRECTORY ENTRY DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_DIRENT_CLUS_NBR_GET(p_dir_entry)        (((CPU_INT32U)(MEM_VAL_GET_INT16U_LITTLE((void *)((CPU_INT08U *)((void *)(p_dir_entry)) + FS_FAT_DIRENT_OFF_FSTCLUSHI))) << 16) + \
                                                         ((CPU_INT32U)(MEM_VAL_GET_INT16U_LITTLE((void *)((CPU_INT08U *)((void *)(p_dir_entry)) + FS_FAT_DIRENT_OFF_FSTCLUSLO))) <<  0))

#define  FS_FAT_DIRENT_CLUS_NBR_SET(p_dir_entry, clus_nbr)            {MEM_VAL_SET_INT16U_LITTLE((void *)((CPU_INT08U *)((void *)(p_dir_entry)) + FS_FAT_DIRENT_OFF_FSTCLUSHI), (clus_nbr) >> DEF_INT_16_NBR_BITS); \
                                                                       MEM_VAL_SET_INT16U_LITTLE((void *)((CPU_INT08U *)((void *)(p_dir_entry)) + FS_FAT_DIRENT_OFF_FSTCLUSLO), (clus_nbr) &  DEF_INT_16_MASK);}

#define  FS_FAT_DIRENT_DATE_GET_DAY(date)      ((CPU_INT08U) ((CPU_INT08U)((date) >>  0) & 0x1Fu))
#define  FS_FAT_DIRENT_DATE_GET_MONTH(date)    ((CPU_INT08U) ((CPU_INT08U)((date) >>  5) & 0x0Fu))
#define  FS_FAT_DIRENT_DATE_GET_YEAR(date)     ((CPU_INT16U)(((CPU_INT16U)((date) >>  9) & 0x7Fu) + 1980u))

#define  FS_FAT_DIRENT_TIME_GET_SEC(time)      (((CPU_INT08U)((time) >>  0) & 0x1Fu) * 2u)
#define  FS_FAT_DIRENT_TIME_GET_MIN(time)       ((CPU_INT08U)((time) >>  5) & 0x3Fu)
#define  FS_FAT_DIRENT_TIME_GET_HOUR(time)      ((CPU_INT08U)((time) >> 11) & 0x1Fu)

#define  FS_FAT_DIRENT_DATE_LO                        0x0021u
#define  FS_FAT_DIRENT_TIME_LO                        0x0000u

/*
*********************************************************************************************************
*                                           FSINFO DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ FSINFO OFFSETS ------------------ */
#define  FS_FAT_FSI_OFF_LEADSIG                            0u
#define  FS_FAT_FSI_OFF_RESERVED1                          4u
#define  FS_FAT_FSI_OFF_STRUCSIG                         484u
#define  FS_FAT_FSI_OFF_FREE_COUNT                       488u
#define  FS_FAT_FSI_OFF_NXT_FREE                         492u
#define  FS_FAT_FSI_OFF_RESERVED2                        496u
#define  FS_FAT_FSI_OFF_TRAILSIG                         508u

                                                                /* -------------------- FSINFO VAL'S ------------------ */
#define  FS_FAT_FSI_LEADSIG                       0x41615252u
#define  FS_FAT_FSI_STRUCSIG                      0x61417272u
#define  FS_FAT_FSI_TRAILSIG                      0xAA550000u


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_fat_tbl_entry {
    FS_FAT_SEC_NBR  VolSize;
    FS_FAT_SEC_NBR  ClusSize;
} FS_FAT_TBL_ENTRY;


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
#if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE)
static  const  CPU_INT32U        FS_FAT_NameDot[3]    = {0x2020202Eu,
                                                         0x20202020u,
                                                         0x20202020u};

static  const  CPU_INT32U        FS_FAT_NameDotDot[3] = {0x20202E2Eu,
                                                         0x20202020u,
                                                         0x20202020u};
#else
static  const  CPU_INT32U        FS_FAT_NameDot[3]    = {0x2E202020u,
                                                         0x20202020u,
                                                         0x20202020u};

static  const  CPU_INT32U        FS_FAT_NameDotDot[3] = {0x2E2E2020u,
                                                         0x20202020u,
                                                         0x20202020u};
#endif

static  const  FS_FAT_TBL_ENTRY  FS_FAT_TblFAT12[10]  = {{        36u,  0u},
                                                         {        37u,  1u},
                                                         {      4040u,  2u},
                                                         {      8057u,  4u},
                                                         {     16040u,  8u},
                                                         {     32050u, 16u},
                                                         {     64060u, 32u},
                                                         {    128080u, 64u},
                                                         {    256120u,  0u},
                                                         {         0u,  0u}};

static  const  FS_FAT_TBL_ENTRY  FS_FAT_TblFAT16[9]   = {{      8400u,  0u},
                                                         {     32680u,  2u},
                                                         {    262000u,  4u},
                                                         {    524000u,  8u},
                                                         {   1048000u, 16u},
                                                         {   2096000u, 32u},
                                                         {   4194304u, 64u},
                                                         {4294967295u,  0u},
                                                         {         0u,  0u}};

static  const  FS_FAT_TBL_ENTRY  FS_FAT_TblFAT32[7]   = {{     66600u,  0u},
                                                         {    532480u,  1u},
                                                         {  16777216u,  8u},
                                                         {  33554432u, 16u},
                                                         {  67108864u, 32u},
                                                         {4294967295u, 64u},
                                                         {         0u,  0u}};
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

static  MEM_POOL  FS_FAT_DataPool;


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

static  void  FS_FAT_ChkBootSec         (FS_FAT_DATA       *p_fat_data,     /* Calc vol info.                               */
                                         CPU_INT08U        *p_temp_08,
                                         FS_ERR            *p_err);

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
static  void  FS_FAT_ChkFile            (FS_VOL            *p_vol,          /* Chk file integrity.                          */
                                         FS_FAT_FILE_DATA  *p_entry_data,
                                         FS_ERR            *p_err);

static  void  FS_FAT_ChkDir             (FS_VOL            *p_vol,          /* Chk dir  integrity.                          */
                                         FS_FAT_FILE_DATA  *p_entry_data,
                                         FS_ERR            *p_err);
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_GetSysCfg          (FS_SEC_SIZE        sec_size,       /* Get cfg for fmt.                             */
                                         FS_SEC_QTY         size,
                                         FS_FAT_SYS_CFG    *p_sys_cfg,
                                         FS_ERR            *p_err);

static  void  FS_FAT_LowDirFirstClusAdd (FS_VOL            *p_vol,          /* Add 1st clus to dir.                         */
                                         FS_FAT_FILE_DATA  *p_entry_data,
                                         FS_BUF            *p_buf,
                                         FS_FAT_SEC_NBR     dir_parent_sec,
                                         FS_ERR            *p_err);

#endif

static  void  FS_FAT_DataSrch           (FS_VOL            *p_vol,          /* Find file in data.                           */
                                         FS_BUF            *p_buf,
                                         CPU_CHAR          *name_entry,
                                         FS_FAT_SEC_NBR    *p_dir_first_sec,
                                         FS_FAT_DIR_POS    *p_dir_start_pos,
                                         FS_FAT_DIR_POS    *p_dir_end_pos,
                                         FS_ERR            *p_err);

static  void  FS_FAT_FileDataClr        (FS_FAT_FILE_DATA  *p_entry_data);  /* Clr FAT file data struct.                    */

static  void  FS_FAT_FileDataInit       (FS_FAT_FILE_DATA  *p_entry_data,   /* Init FAT file data struct.                   */
                                         CPU_INT08U        *p_dir_entry);

static  void  FS_FAT_DataClr            (FS_FAT_DATA       *p_fat_data);    /* Clr FAT info struct.                         */


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           FS_FAT_VolChk()
*
* Description : Check the file system on a volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              Volume checked without errors.
*                               FS_ERR_NAME_NULL         Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN      Volume not open.
*                               FS_ERR_BUF_NONE_AVAIL    No buffers available.
*                               FS_ERR_DEV               Device error.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The "dot" entry should have ...
*                       (1) ... a size of zero ...
*                       (2) ... & the cluster number of the directory.
*
*                   (b) The "dot dot" entry should have ...
*                       (1) ... a size of zero ...
*                       (2) ... & the cluster number of the parent directory, if not in the root directory
*                                        OR          of zero,                 if     in the root directory.
*
*               (2) #### Check for cross-linked files & lost clusters.
*********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
void  FS_FAT_VolChk (CPU_CHAR  *name_vol,
                     FS_ERR    *p_err)
{
    FS_BUF            *p_buf;
    CPU_INT08U        *p_dir_entry;
    FS_FAT_DATA       *p_fat_data;
    CPU_BOOLEAN        dir;
    FS_FAT_CLUS_NBR    dir_clus;
    FS_FAT_CLUS_NBR    dir_clus_tmp;
    CPU_BOOLEAN        done;
    CPU_INT16S         dot;
    CPU_INT16S         dot_dot;
    CPU_INT08U         fat_attrib;
    CPU_INT08U         i;
    CPU_INT08U         level;
    FS_FAT_DIR_POS     pos;
    FS_FAT_DIR_POS     end_pos;
    CPU_BOOLEAN        valid;
    FS_VOL            *p_vol;
    CPU_CHAR           name[FS_FAT_MAX_FILE_NAME_LEN + 1u];
    FS_FAT_SEC_NBR     sec_nbr_stk[FS_FAT_CFG_VOL_CHK_MAX_LEVELS];
    FS_SEC_SIZE        sec_pos_stk[FS_FAT_CFG_VOL_CHK_MAX_LEVELS];
    FS_FAT_CLUS_NBR    dir_clus_stk[FS_FAT_CFG_VOL_CHK_MAX_LEVELS];
    FS_FAT_FILE_DATA   fat_file_data;
#if  defined(FS_FAT_LFN_MODULE_PRESENT) && \
            (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_WCHAR          name_utf8[FS_FAT_MAX_FILE_NAME_LEN + 1u];
    CPU_WCHAR         *p_name_utf8;
    CPU_SIZE_T         len;
#endif


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif
                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_NO, p_err);      /* Vol may be unmounted.                                */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == (FS_VOL *)0) {
        return;
    }


                                                                /* ------------------ PREPARE FOR CHK ----------------- */
    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == (FS_BUF *)0) {
        FSVol_ReleaseUnlock(p_vol);
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    pos.SecNbr = p_fat_data->RootDirStart;                      /* Start in root dir.                                   */
    pos.SecPos = 0u;
    dir_clus   = 0u;
    level      = 0u;
    done       = DEF_NO;



    while (done == DEF_NO) {
                                                                /* ------------------ FIND DIR ENTRY ------------------ */
#if  defined(FS_FAT_LFN_MODULE_PRESENT) && \
            (FS_CFG_UTF8_EN == DEF_ENABLED)
        FS_FAT_FN_API_Active.NextDirEntryGet(         p_vol,
                                                      p_buf,
                                             (void *)&name_utf8[0],
                                                     &pos,
                                                     &end_pos,
                                                      p_err);

                                                                /* Conversion from wide char to reg char                */
        p_name_utf8 = &name_utf8[0];
        len         = WC_StrToMB(             &name[0],
                                              &p_name_utf8,
                                 (CPU_SIZE_T)  FS_FAT_MAX_FILE_NAME_LEN);

        if (len == (CPU_SIZE_T)-1) {
           *p_err = FS_ERR_ENTRY_CORRUPT;
            FSVol_ReleaseUnlock(p_vol);
            return;
        }
#else
        FS_FAT_FN_API_Active.NextDirEntryGet(         p_vol,
                                                      p_buf,
                                             (void *)&name[0],
                                                     &pos,
                                                     &end_pos,
                                                      p_err);

#endif



        switch (*p_err) {
            case FS_ERR_EOF:                                    /* ----------------- END OF DIRECTORY ----------------- */
                 if (level == 0) {                              /* If in root dir ...                                   */
                     done  = DEF_YES;                           /* ... no more entries to process.                      */
                    *p_err = FS_ERR_NONE;                       /* Make sure No error is returned when no more entries  */

                 } else {                                       /* Otherwise ...                                        */
                     level--;                                   /* ... move to parent dir.                              */

                     pos.SecNbr = sec_nbr_stk[level];
                     pos.SecPos = sec_pos_stk[level];
                     dir_clus   = dir_clus_stk[level];
                 }
                 break;




            case FS_ERR_NONE:                                   /* ---------------- DIR ENTRY DIR FOUND --------------- */
                 p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + end_pos.SecPos;
                 fat_attrib  = MEM_VAL_GET_INT08U_LITTLE(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                 fat_file_data.DirStartSec    = pos.SecNbr;
                 fat_file_data.DirStartSecPos = pos.SecPos;
                 fat_file_data.DirEndSec      = end_pos.SecNbr;
                 fat_file_data.DirEndSecPos   = end_pos.SecPos;
                 fat_file_data.Attrib         = fat_attrib;
                 fat_file_data.FileFirstClus  = FS_FAT_DIRENT_CLUS_NBR_GET(p_dir_entry);
                 fat_file_data.FileSize       = MEM_VAL_GET_INT32U_LITTLE(p_dir_entry + FS_FAT_DIRENT_OFF_FILESIZE);
#endif

                 dir = DEF_BIT_IS_SET(fat_attrib, FS_FAT_DIRENT_ATTR_DIRECTORY);
                 if (dir == DEF_YES) {                          /* If dir ...                                           */
                                                                /* ... srch files in dir.                               */
                     FS_TRACE_DBG(("FS_FAT_VolChk(): Found dir  "));
                     for (i = 0u; i < level; i++) {
                         FS_TRACE_DBG(("  "));
                     }
                     FS_TRACE_DBG(("%s\r\n", name));

                     dot = Str_Cmp_N(name, (CPU_CHAR *)".", FS_CFG_MAX_PATH_NAME_LEN);
                     if (dot == 0) {                            /* "Dot" entry (see Note #1a).                         */
                         dir_clus_tmp = fat_file_data.FileFirstClus;
                         if (level == 0u) {
                             FS_TRACE_INFO(("FS_FAT_VolChk(): \".\" entry in root dir.\r\n"));
                         } else {
                             if (dir_clus_tmp != dir_clus) {    /* If clus nbr bad ...                                  */
                                 FS_TRACE_INFO(("FS_FAT_VolChk(): \".\" entry of dir in root dir with invalid clus nbr: %d (!= %d).\r\n", dir_clus_tmp, dir_clus));

                                                                /* ... correct clus nbr.                                */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                 fat_file_data.FileFirstClus = dir_clus;
                                 fat_file_data.FileSize      = 0u;

                                 FS_FAT_LowEntryUpdate( p_vol,
                                                        p_buf,
                                                       &fat_file_data,
                                                        DEF_YES,
                                                        p_err);
                                     if (*p_err == FS_ERR_DEV) {
                                         FSBuf_Free(p_buf);
                                         FSVol_ReleaseUnlock(p_vol);
                                         return;
                                     }
#endif
                             }
                         }

                         pos.SecNbr = end_pos.SecNbr;           /* Move to next entry.                                  */
                         pos.SecPos = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;


                     } else {
                         dot_dot = Str_Cmp_N(name, (CPU_CHAR *)"..", FS_CFG_MAX_PATH_NAME_LEN);
                         if (dot_dot == 0) {                    /* "Dot dot" entry (see Note #1b).                      */
                             dir_clus_tmp = fat_file_data.FileFirstClus;
                             if (level == 0u) {
                                 FS_TRACE_INFO(("FS_FAT_VolChk(): \"..\" entry in root dir.\r\n"));
                             } else {
                                                                /* If clus nbr bad ...                                  */
                                 if (dir_clus_tmp != dir_clus_stk[level - 1]) {
                                     FS_TRACE_INFO(("FS_FAT_VolChk(): \"..\" entry of dir in root dir with invalid clus nbr: %d (!= %d).\r\n", dir_clus_tmp, dir_clus_stk[level - 1]));

                                                                /* ... correct clus nbr.                                */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                     fat_file_data.FileFirstClus = dir_clus_stk[level - 1u];
                                     fat_file_data.FileSize      = 0u;

                                     FS_FAT_LowEntryUpdate( p_vol,
                                                            p_buf,
                                                           &fat_file_data,
                                                            DEF_YES,
                                                            p_err);
                                     if (*p_err == FS_ERR_DEV) {
                                         FSBuf_Free(p_buf);
                                         FSVol_ReleaseUnlock(p_vol);
                                         return;
                                     }
#endif
                                 }
                             }

                             pos.SecNbr = end_pos.SecNbr;       /* Move to next entry.                                  */
                             pos.SecPos = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;


                         } else {                               /* Dir.                                                 */
                             if (level < FS_FAT_CFG_VOL_CHK_MAX_LEVELS) {
                                 sec_nbr_stk[level]  = end_pos.SecNbr;
                                 sec_pos_stk[level]  = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;
                                 dir_clus_stk[level] = dir_clus;

                                 level++;

                                 dir_clus            = fat_file_data.FileFirstClus;
                                 pos.SecNbr          = FS_FAT_CLUS_TO_SEC(p_fat_data, dir_clus);
                                 pos.SecPos          = 0u;

                                 valid               = FS_FAT_IS_VALID_CLUS(p_fat_data, dir_clus);
                                 if (valid == DEF_NO) {         /* If clus not valid ...                                */
                                     FS_TRACE_INFO(("FS_FAT_VolChk(): Dir clus nbr invalid.\r\n"));

                                                                /* ... clr clus nbr  ...                                */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                     fat_file_data.FileFirstClus = 0u;
                                     fat_file_data.FileSize      = 0u;

                                     FS_FAT_LowEntryUpdate( p_vol,
                                                            p_buf,
                                                           &fat_file_data,
                                                            DEF_YES,
                                                            p_err);
                                     if (*p_err == FS_ERR_DEV) {
                                         FSBuf_Free(p_buf);
                                         FSVol_ReleaseUnlock(p_vol);
                                         return;
                                     }
#endif

                                     level--;                   /* ... & move to next entry.                            */
                                     pos.SecNbr = sec_nbr_stk[level];
                                     pos.SecPos = sec_pos_stk[level];
                                     dir_clus   = dir_clus_stk[level];


                                 } else {                       /* Chk dir.                                             */
                                     FS_FAT_ChkDir( p_vol,
                                                   &fat_file_data,
                                                    p_err);
                                     if (*p_err == FS_ERR_DEV) {
                                         FSBuf_Free(p_buf);
                                         FSVol_ReleaseUnlock(p_vol);
                                         return;
                                     }
                                 }


                             } else {                           /* Nesting too deep.                                    */
                                 FS_TRACE_DBG(("FS_FAT_VolChk(): Nesting too deep.\r\n"));
                                 pos.SecNbr = end_pos.SecNbr;   /* Move to next entry.                                  */
                                 pos.SecPos = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;
                             }
                         }
                     }



                                                                /* --------------- DIR ENTRY FILE FOUND --------------- */
                 } else {                                       /* If file ...                                          */
                     FS_TRACE_DBG(("FS_FAT_VolChk(): Found file "));
                     for (i = 0u; i < level; i++) {
                         FS_TRACE_DBG(("  "));
                     }
                     FS_TRACE_DBG(("%s\r\n", name));

                     FS_FAT_ChkFile( p_vol,                     /* ... chk file.                                        */
                                    &fat_file_data,
                                     p_err);

                     if (*p_err == FS_ERR_DEV) {
                         FSBuf_Free(p_buf);
                         FSVol_ReleaseUnlock(p_vol);
                         return;
                     }

                     pos.SecNbr = end_pos.SecNbr;               /* Move to next entry.                                  */
                     pos.SecPos = end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;
                 }
                 break;



            case FS_ERR_DEV:                                    /* ---------------------- DEV ERR --------------------- */
            default:
                 done = DEF_YES;
                 break;
        }
    }

    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
    FSVol_ReleaseUnlock(p_vol);
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_ClusChainAlloc()
*
* Description : Allocate or extend cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  First cluster of the chain from which allocation/extension will take place.
*
*               nbr_clus    Number of clusters to allocate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_DEV_FULL             Device is full (no space could be allocated).
*                               FS_ERR_NONE                 Cluster chain successfully allocated.
*                               FS_ERR_SYS_CLUS_CHAIN_END   Cluster chain ended.
*                               FS_ERR_SYS_CLUS_INVALID     Cluster chain ended with invalid cluster.
*
*                               -------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValWr()--------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValWr() for additional return error codes.
*
*                               ----------------RETURNED BY FS_FAT_JournalEnterClusChainAlloc()----------------
*                               See FS_FAT_JournalEnterClusChainAlloc() for additional return error codes.
*
*                               -------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()--------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
*                               -----------------------RETURNED BY FS_FAT_ClusFreeFind()-----------------------
*                               See FS_FAT_ClusFreeFind() for additional return error codes.
*
*                               -------------------RETURNED BY FS_FAT_ClusChainReverseDel()--------------------
*                               See FS_FAT_ClusChainReverseDel() for additional return error codes.
*
* Return(s)   : Index of the cluster from which the allocation/extension takes place.
*
* Note(s)     : (1) Uncompleted allocations are rewinded using reverse deletion. By doing so, we make sure
*                   deletion can always be completed after a potential failure (even without journaling).
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_CLUS_NBR  FS_FAT_ClusChainAlloc (FS_VOL           *p_vol,
                                        FS_BUF           *p_buf,
                                        FS_FAT_CLUS_NBR   start_clus,
                                        FS_FAT_CLUS_NBR   nbr_clus,
                                        FS_ERR           *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   rem_clus;
    FS_FAT_CLUS_NBR   cur_clus;
    FS_FAT_CLUS_NBR   next_clus;
    CPU_BOOLEAN       is_new_chain;


    p_fat_data   = (FS_FAT_DATA  *)p_vol->DataPtr;
    rem_clus     =  nbr_clus;
    is_new_chain =  DEF_NO;

    FS_TRACE_LOG(("FS_FAT_ClusChainAlloc(): Need to allocate a chain of %08X cluster from cluster (%08X).\r\n", nbr_clus, start_clus));


    if (nbr_clus == 0u) {                                       /* If no clus need to be alloc'd ...                    */
       *p_err = FS_ERR_NONE;
        return (start_clus);                                    /* ... ret successfully.                                */
    }

                                                                /* ----------------- FIND START CLUS ------------------ */
    if (start_clus == 0u) {                                     /* If new chain, find start clus.                       */
        start_clus = FS_FAT_ClusFreeFind(p_vol,
                                         p_buf,
                                         p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        is_new_chain = DEF_YES;
        rem_clus--;
        FS_TRACE_LOG(("FS_FAT_ClusChainAlloc(): The new chain will start with cluster (%08X).\r\n", start_clus));
    }

                                                                /* -------------- GET FAT ENTRY FOR CLUS -------------- */
    next_clus = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,   /* Rd next clus.                                        */
                                                       p_buf,
                                                       start_clus,
                                                       p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

    if (next_clus != 0u) {                                      /* Next clus alloc'd.                                   */

        if (next_clus < FS_FAT_MIN_CLUS_NBR) {                  /* Invalid clus nbr.                                    */
            FS_TRACE_DBG(("FS_FAT_ClusChainAlloc(): Next clus in chain after %08X is invalid (%08X).\r\n", start_clus, next_clus));
           *p_err = FS_ERR_SYS_CLUS_INVALID;
            return (0u);
        }

        if (next_clus < p_fat_data->MaxClusNbr) {               /* Next clus exists.                                    */
           *p_err = FS_ERR_NONE;
            return (next_clus);
        }

        if (next_clus < p_fat_data->FAT_TypeAPI_Ptr->ClusBad) { /* Invalid clus nbr.                                    */
            FS_TRACE_DBG(("FS_FAT_ClusChainAlloc(): Next clus in chain after %08X is invalid (%08X).\r\n", start_clus, next_clus));
           *p_err = FS_ERR_SYS_CLUS_INVALID;
            return (0u);
        }

        if (next_clus == p_fat_data->FAT_TypeAPI_Ptr->ClusBad) {/* Clus is bad.                                         */
            FS_TRACE_DBG(("FS_FAT_ClusChainAlloc(): Next clus in chain after %08X is bad (%08X).\r\n", start_clus, next_clus));
           *p_err = FS_ERR_SYS_CLUS_CHAIN_END;
            return (0u);
        }

    }                                                           /* Otherwise, clus is EOC or not alloc'd.               */


                                                                /* ------------------- ENTER JOURNAL ------------------ */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
        FS_FAT_JournalEnterClusChainAlloc(p_vol,
                                          p_buf,
                                          start_clus,
                                          is_new_chain,
                                          p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
    }
#endif


                                                                /* ----------------- ALLOC CLUS CHAIN ----------------- */
    cur_clus = start_clus;
    while (rem_clus > 0u) {

        next_clus = FS_FAT_ClusFreeFind(p_vol,                  /* Find next clus in chain.                             */
                                        p_buf,
                                        p_err);

                                                                /* ------- REWIND ALLOC IF NO MORE FREE CLUS'S -------- */
        if (next_clus == cur_clus) {
           FS_TRACE_DBG(("FS_FAT_ClusChainAlloc(): No free FAT clus could be found.\r\n"));
           FS_TRACE_DBG(("FS_FAT_ClusChainAlloc(): Reverting partial allocation.\r\n"));
           FS_FAT_ClusChainReverseDel(p_vol,                    /* Del partially alloc'd clus chain (see Note #1).      */
                                      p_buf,
                                      start_clus,
                                      is_new_chain,
                                      p_err);
           if (*p_err != FS_ERR_NONE){
               return (0u);
           }
           *p_err = FS_ERR_DEV_FULL;
            return (0u);
        }
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        FS_TRACE_LOG(("FS_FAT_ClusChainAlloc(): Next cluster in chain is (%08X).\r\n", next_clus));

        p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,           /* Update FAT chain.                                    */
                                               p_buf,
                                               cur_clus,
                                               next_clus,
                                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        cur_clus = next_clus;
        rem_clus--;
    }

    p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,               /* Update FAT chain (EOC).                              */
                                           p_buf,
                                           cur_clus,
                                           p_fat_data->FAT_TypeAPI_Ptr->ClusEOF,
                                           p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }


                                                                /* ------------------- UPDATE & RTN ------------------- */
    FS_TRACE_LOG(("FS_FAT_ClusChainAlloc(): New clus chain alloc'd: %d clusters allocated from start clus %d.\r\n", nbr_clus, start_clus));
    FS_CTR_STAT_INC(p_fat_data->StatAllocClusCtr);
    if (p_fat_data->QueryInfoValid == DEF_YES) {                /* Update query info.                                   */
        p_fat_data->QueryFreeClusCnt -= nbr_clus;
    }

   *p_err = FS_ERR_NONE;
    return (start_clus);
}
#endif


/*
*********************************************************************************************************
*                                         FS_FAT_ClusChainDel()
*
* Description : Forward delete FAT cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  First cluster of the cluster chain.
*
*               del_first   Indicates whether first cluster should be deleted :
*
*                               DEF_NO,  if first clus will be marked EOF.
*                               DEF_YES, if first clus will be marked free.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE          Cluster chain deleted.
*                               FS_ERR_INVALID_ARG   Invalid 'start_clus' parameter.
*
*                               --------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValWr()-------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValWr() for additional return error codes.
*
*                               --------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()-------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
*                               ---------------------RETURNED BY FSVol_ReleaseLocked()-------------------------
*                               See FSVol_ReleaseLocked() for additional return error codes.
*
*                               ---------------RETURNED BY FS_FAT_JournalEnterClusChainDel()-------------------
*                               See FS_FAT_JournalEnterClusChainDel() for additional return error codes.
*
*                               --------------------RETURNED BY FS_FAT_ClusChainEndFind()----------------------
*                               See FS_FAT_ClusChainEndFind() for additional return error codes.
*
*
* Return(s)   : Number of clusters deleted.
*
* Note(s)     : (1) All clusters located after 'start_clus' will be deleted, that is, until an end of cluster
*                   chain or an invalid cluster is found. In both cases, no error is returned.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_CLUS_NBR  FS_FAT_ClusChainDel (FS_VOL           *p_vol,
                                      FS_BUF           *p_buf,
                                      FS_FAT_CLUS_NBR   start_clus,
                                      CPU_BOOLEAN       del_first,
                                      FS_ERR           *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   cur_clus;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_CLUS_NBR   clus_cnt;
    FS_FAT_SEC_NBR    cur_sec;
    FS_FAT_CLUS_NBR   new_fat_entry;
    CPU_BOOLEAN       del;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_CLUS_NBR   len;
#endif


    p_fat_data  = (FS_FAT_DATA *)p_vol->DataPtr;
    del         =  del_first;
    cur_clus    =  start_clus;
   *p_err       =  FS_ERR_NONE;

                                                                /* --------------- VALIDATE START CLUS ---------------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
        FS_TRACE_DBG(("FS_FAT_ClusChainDel(): Invalid start clus.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return (0u);
    }
#endif

                                                                /* ------------------ JOURNAL ENTER ------------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
       (void)FS_FAT_ClusChainEndFind(p_vol,                     /* Cnt clus's.                                           */
                                     p_buf,
                                     start_clus,
                                    &clus_cnt,
                                     p_err);
        if (*p_err == FS_ERR_NONE) {                            /* If clus chain has an EOC ...                          */
            len = clus_cnt + 1u;                                /* ... clus chain len is the nbr of clus followed + 1 ...*/
        } else if (*p_err == FS_ERR_SYS_CLUS_INVALID) {         /* If clus chain has no EOC ...                          */
            len = clus_cnt;                                     /* ... clus chain len equals the nbr of clus followed.   */
        } else {
            return (0u);
        }

        FS_FAT_JournalEnterClusChainDel(p_vol,                  /* Add journal log.                                      */
                                        p_buf,
                                        start_clus,
                                        len,
                                        del_first,
                                        p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
    }
#endif


                                                                /* ------------------- FREE CLUS'S -------------------- */
    clus_cnt = 0u;
    do {
                                                                /* Rd next FAT entry.                                   */
        next_clus = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                           p_buf,
                                                           cur_clus,
                                                           p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        if (del == DEF_NO) {                                    /* If first clus must be preserved ...                  */
            del = DEF_YES;
            new_fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusEOF;   /*                             ... set to EOC.      */
            FS_TRACE_LOG(("FS_FAT_ClusChainDel(): FAT clus mark'd as EOC: %d.\r\n", cur_clus));

        } else {                                                /* If clus must be del'd ...                            */
            new_fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusFree;
            cur_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, cur_clus);     /*                   ... set to clus free mark.     */

            FSVol_ReleaseLocked(p_vol,                          /* #### UCFS-328                                        */
                                cur_sec,
                                p_fat_data->ClusSize_sec,
                                p_err);

            if (*p_err != FS_ERR_NONE) {
                return (0u);
            }

            if (p_fat_data->QueryInfoValid == DEF_YES) {
                p_fat_data->QueryFreeClusCnt++;
            }

            FS_CTR_STAT_INC(p_fat_data->StatFreeClusCtr);
            clus_cnt++;                                         /* Inc del clus cnt.                                    */
            FS_TRACE_LOG(("FS_FAT_ClusChainDel(): FAT clus free'd: %d.\r\n", cur_clus));
        }

        p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,           /* Wr FAT entry.                                        */
                                               p_buf,
                                               cur_clus,
                                               new_fat_entry,
                                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        cur_clus  = next_clus;                                  /* Update cur clus.                                     */

    } while (FS_FAT_IS_VALID_CLUS(p_fat_data, cur_clus) == DEF_YES);


                                                                /* ---------------- ALL CLUS'S FREE'D ----------------- */
    if (cur_clus >= p_fat_data->FAT_TypeAPI_Ptr->ClusEOF) {
        FS_TRACE_LOG(("FS_FAT_ClusChainDel(): %d FAT clus's free'd upon reaching clus chain end.\r\n", clus_cnt));
    } else {
        FS_TRACE_DBG(("FS_FAT_ClusChainDel(): %d FAT clus's free'd upon reaching invalid clus.\r\n", clus_cnt));
    }

   *p_err = FS_ERR_NONE;
    return (clus_cnt);
}
#endif


/*
*********************************************************************************************************
*                                         FS_FAT_ClusChainReverseDel()
*
* Description : Reverse delete FAT cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  First cluster of the cluster chain.
*
*               del_first   Indicates whether first cluster should be deleted :
*
*                               DEF_NO,  if first clus will be marked EOF.
*                               DEF_YES, if first clus will be marked free.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Cluster chain deleted.
*                               FS_ERR_INVALID_ARG    Invalid 'start_clus' parameter.
*
*                               -------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValWr()--------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValWr() for additional return error codes.
*
*                               ----------------------RETURNED BY FSVol_ReleaseLocked()------------------------
*                               See FSVol_ReleaseLocked() for additional return error codes.
*
*                               --------------------RETURNED BY FS_FAT_ClusChainEndFind()----------------------
*                               See FS_FAT_ClusChainEndFind() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_ClusChainReverseDel (FS_VOL           *p_vol,
                                  FS_BUF           *p_buf,
                                  FS_FAT_CLUS_NBR   start_clus,
                                  CPU_BOOLEAN       del_first,
                                  FS_ERR           *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   cur_clus;
    FS_FAT_SEC_NBR    cur_sec;
    FS_FAT_CLUS_NBR   new_fat_entry;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
    FS_FAT_CLUS_NBR   clus_cnt;
#endif


    p_fat_data  = (FS_FAT_DATA *)p_vol->DataPtr;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
    clus_cnt    =  0u;
#endif

                                                                /* --------------- VALIDATE START CLUS ---------------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
        FS_TRACE_DBG(("FS_FAT_ClusChainReverseDel(): Invalid start clus.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return;
    }
#endif

                                                                /* ------------------- FREE CLUS'S -------------------- */
    do {
                                                                /* Find chain end.                                      */
        cur_clus = FS_FAT_ClusChainEndFind(p_vol,
                                           p_buf,
                                           start_clus,
                                           DEF_NULL,
                                           p_err);
        if ((*p_err != FS_ERR_NONE) &&
            (*p_err != FS_ERR_SYS_CLUS_INVALID)) {
            return;
        }
        if ((*p_err == FS_ERR_SYS_CLUS_INVALID) &&              /* If start clus is invalid ...                         */
            ( cur_clus == 0u)) {
            cur_clus = start_clus;                              /* ... go on to make sure it is either free or EOC.     */
        }

        if ((cur_clus == start_clus) && (del_first == DEF_NO)) {    /* If start clus must be preserved ...              */
            new_fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusEOF;   /*                                 ... set to EOC.  */
        } else {
            new_fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusFree;  /* If clus must be del'd ...                        */
            FS_TRACE_LOG(("FS_FAT_ClusChainDel(): FAT clus free'd: %d.\r\n", cur_clus));/*   ... set to clus free mark. */

            cur_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, cur_clus);
            FSVol_ReleaseLocked(p_vol,                          /* #### UCFS-328                                        */
                                cur_sec,
                                p_fat_data->ClusSize_sec,
                                p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }

            if (p_fat_data->QueryInfoValid == DEF_YES) {
                p_fat_data->QueryFreeClusCnt++;
            }

            FS_CTR_STAT_INC(p_fat_data->StatFreeClusCtr);
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
            clus_cnt++;                                         /* Inc clus cnt.                                        */
#endif
        }

                                                                /* ------------------- WR FAT ENTRY ------------------- */
        p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,
                                               p_buf,
                                               cur_clus,
                                               new_fat_entry,
                                               p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

    } while ( start_clus != cur_clus);                          /* Start over until start clus has been del'd.          */


                                                                /* ---------------- ALL CLUS'S FREE'D ----------------- */
    FS_TRACE_LOG(("FS_FAT_ClusChainDel(): FAT %d clus's free'd.\r\n", clus_cnt));
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_ClusChainFollow()
*
* Description : Follow FAT cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Cluster to start following from.
*
*               len         Number of clusters to follow.
*
*               p_clus_cnt  Pointer to variable that will receive the total number of clus followed.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                        'len' clus followed.
*                               FS_ERR_SYS_CLUS_CHAIN_END_EARLY     Cluster chain ended early.
*                               FS_ERR_SYS_CLUS_INVALID             Invalid cluster found.
*                               FS_ERR_INVALID_ARG                  Invalid 'start_clus' parameter.
*
*                               --------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()-------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
* Return(s)   : Cluster number of the last valid cluster found.
*
* Note(s)     : (1) If 'len', the number of clusters to follow, is zero, the start cluster will be
*                   returned.  Otherwise, the cluster chain will be followed 'len' clusters or until chain
*                   ends, starting at cluster 'start_clus'.
*********************************************************************************************************
*/

FS_FAT_CLUS_NBR  FS_FAT_ClusChainFollow (FS_VOL           *p_vol,
                                         FS_BUF           *p_buf,
                                         FS_FAT_CLUS_NBR   start_clus,
                                         FS_FAT_CLUS_NBR   len,
                                         FS_FAT_CLUS_NBR  *p_clus_cnt,
                                         FS_ERR           *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR  *p_cnt;
    FS_FAT_CLUS_NBR   prev_clus;
    FS_FAT_CLUS_NBR   cur_clus;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_CLUS_NBR   cnt;


    p_fat_data  = (FS_FAT_DATA  *)p_vol->DataPtr;
    p_cnt       = (p_clus_cnt == DEF_NULL) ? &cnt : p_clus_cnt;
   *p_cnt       =  0u;
    prev_clus   =  0u;
    cur_clus    =  start_clus;

                                                                /* --------------- VALIDATE START CLUS ---------------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
        FS_TRACE_DBG(("FS_FAT_ClusChainFollow(): Invalid start clus.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return (0u);
    }
#endif


                                                                /* ------------------- FOLLOW CHAIN ------------------- */
    while(*p_cnt < len ) {
                                                                /* Rd next FAT entry.                                   */
        next_clus = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                           p_buf,
                                                           cur_clus,
                                                           p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

                                                                /* ------------------ VALIDATE ENTRY ------------------ */
        if (FS_FAT_IS_VALID_CLUS(p_fat_data, next_clus) == DEF_NO) {
            if (next_clus >= p_fat_data->FAT_TypeAPI_Ptr->ClusEOF) {
               *p_err = FS_ERR_SYS_CLUS_CHAIN_END_EARLY;        /* If EOC is found ...                                  */
                return (cur_clus);                              /* ... rtn cur clus nbr.                                */
            } else {
               *p_err = FS_ERR_SYS_CLUS_INVALID;                /* If invalid clus is found ...                         */
                return (prev_clus);                             /* ... rtn prev clus nbr.                               */
            }
        }

      (*p_cnt)++;
        prev_clus = cur_clus;
        cur_clus  = next_clus;
    }

                                                                /* 'len' clus followed.                                 */
   *p_err = FS_ERR_NONE;
    return (cur_clus);
}


/*
*********************************************************************************************************
*                                       FS_FAT_ClusChainEndFind()
*
* Description : Follow FAT cluster chain until the end.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Cluster to start following from.
*
*               p_clus_cnt  Pointer to variable that will receive the total number of clus followed.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                          Cluster chain followed until EOC.
*                               FS_ERR_SYS_CLUS_INVALID              Cluster chain not EOC terminated.
*
*                               --------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()-------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
*
* Return(s)   : Last allocated cluster number.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_FAT_CLUS_NBR  FS_FAT_ClusChainEndFind (FS_VOL           *p_vol,
                                          FS_BUF           *p_buf,
                                          FS_FAT_CLUS_NBR   start_clus,
                                          FS_FAT_CLUS_NBR  *p_clus_cnt,
                                          FS_ERR           *p_err)
{
    FS_FAT_CLUS_NBR  end_clus;


    end_clus = FS_FAT_ClusChainFollow(p_vol,
                                      p_buf,
                                      start_clus,
                     (FS_FAT_CLUS_NBR)-1,
                                      p_clus_cnt,
                                      p_err);
    if (*p_err == FS_ERR_SYS_CLUS_CHAIN_END_EARLY) {
        *p_err = FS_ERR_NONE;
    }

    return (end_clus);
}


/*
*********************************************************************************************************
*                                      FS_FAT_ClusChainReverseFollow()
*
* Description : Reverse follow FAT cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Cluster to start following from.
*
*               stop_clus   Cluster to stop following at.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Cluster chain reverse followed.
*                               FS_ERR_INVALID_ARG    Invalid 'start_clus' or 'stop_clus' parameter.
*
*                               -------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()--------------
*                               See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
* Return(s)   : Cluster number of the last valid cluster found.
*
* Note(s)     : (1) File allocation table is browsed backward, entry by entry, starting at the entry right
*                   before the current known first cluster entry. Since cluster chains are mostly allocated
*                   forward, this allows many clusters to be followed within a single revolution around the
*                   file allocation table.
*
*               (2) If the entry lookup returns back to its starting point after wrapping around, then no
*                   cluster points to the current target cluster and, therefore, the first cluster of the
*                   chain has been found.
*
*               (3) No error is returned in case chain following ends before the stop cluster is reached.
*********************************************************************************************************
*/

FS_FAT_CLUS_NBR  FS_FAT_ClusChainReverseFollow (FS_VOL           *p_vol,
                                                FS_BUF           *p_buf,
                                                FS_FAT_CLUS_NBR   start_clus,
                                                FS_FAT_CLUS_NBR   stop_clus,
                                                FS_ERR           *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   cur_clus;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_CLUS_NBR   target_clus;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
    FS_FAT_CLUS_NBR   found_clus_cnt;
#endif


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

                                                                /* ------------ VALIDATE START & STOP CLUS ------------ */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
        FS_TRACE_DBG(("FS_FAT_ClusChainReverseFollow(): Invalid start clus.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return (0u);
    }
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, stop_clus) == DEF_NO) {
        FS_TRACE_DBG(("FS_FAT_ClusChainReverseFollow(): Invalid stop clus.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return (0u);
    }
#endif

    target_clus    =  start_clus;
    cur_clus       =  start_clus - 1u;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
    found_clus_cnt =  0u;
#endif

                                                                /* ---------------- PERFORM FAT LOOKUP ---------------- */
    while (target_clus != stop_clus) {

        if (cur_clus < FS_FAT_MIN_CLUS_NBR) {                   /* Wrap clus nbr.                                       */
            cur_clus  = p_fat_data->MaxClusNbr - 1u;
        }
                                                                /* Chk if start has been reached (see Note #2).         */
        if (cur_clus == target_clus) {
            FS_TRACE_LOG(("FS_FAT_ClusChainReverseFollow(): Reached start of cluster chain after %d clusters.\r\n", found_clus_cnt));
            return (target_clus);
        }
                                                                /* Rd next FAT entry.                                   */
        next_clus = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                           p_buf,
                                                           cur_clus,
                                                           p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
                                                                /* Update target clus.                                  */
        if (next_clus == target_clus) {
            target_clus = cur_clus;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
            found_clus_cnt++;
#endif
        }
        cur_clus--;
    }


                                                                /* ---------------- STOP CLUS REACHED ----------------- */
    FS_TRACE_LOG(("FS_FAT_ClusChainReverseFollow(): Stop cluster reached after %d clusters.\r\n", found_clus_cnt));
   *p_err =  FS_ERR_NONE;
    return (target_clus);
}


/*
*********************************************************************************************************
*                                       FS_FAT_ClusFreeFind()
*
* Description : Find free cluster.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE        Cluster found.
*                               FS_ERR_DEV         Device access error.
*                               FS_ERR_DEV_FULL    Device is full (no space could be allocated).
*
* Return(s)   : Cluster number, if free cluster found.
*               0,              otherwise.
*
* Note(s)     : (1) In order for journaling to behave as expected, FAT entry updates must be atomic.
*                   To ensure this is the case when using FAT12, cross-boundary FAT entries must be
*                   avoided.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_CLUS_NBR  FS_FAT_ClusFreeFind (FS_VOL  *p_vol,
                                      FS_BUF  *p_buf,
                                      FS_ERR  *p_err)
{
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   fat_entry;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_CLUS_NBR   clus_cnt_chkd;
    FS_FAT_CLUS_NBR   max_nbr_clus;
    CPU_BOOLEAN       clus_ignore;
#if ((FS_FAT_CFG_FAT12_EN == DEF_ENABLED) && (FS_FAT_CFG_JOURNAL_EN == DEF_ENABLED))
    FS_SEC_SIZE       fat_offset;
    FS_SEC_SIZE       fat_sec_offset;
#endif


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    next_clus      =  p_fat_data->NextClusNbr;
    max_nbr_clus   =  p_fat_data->MaxClusNbr - FS_FAT_MIN_CLUS_NBR;
    clus_cnt_chkd  =  0u;


                                                                /* ----------------- FREE CLUS LOOKUP ----------------- */
    while (clus_cnt_chkd < max_nbr_clus) {
        if (next_clus >= p_fat_data->MaxClusNbr) {              /* Wrap clus nbr.                                       */
            next_clus  = FS_FAT_MIN_CLUS_NBR;
        }


                                                                /* Rd next FAT entry.                                   */
        fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                           p_buf,
                                                           next_clus,
                                                           p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }


                                                                /* ----------------- FREE CLUS FOUND ------------------ */
        if (fat_entry == p_fat_data->FAT_TypeAPI_Ptr->ClusFree) {   /* Chk if free clus found.                          */
            clus_ignore = DEF_NO;                                   /* Clus not ignore'd by dflt.                       */
#if ((FS_FAT_CFG_FAT12_EN == DEF_ENABLED) && (FS_FAT_CFG_JOURNAL_EN == DEF_ENABLED))
            if ((p_fat_data->FAT_Type     == 12u) &&                /* If FAT12 and journal started ...                 */
                (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_YES)) {
                fat_offset     = (FS_SEC_SIZE)next_clus + ((FS_SEC_SIZE)next_clus / 2u);
                fat_sec_offset =  fat_offset & (p_fat_data->SecSize - 1u);
                if (fat_sec_offset == p_fat_data->SecSize - 1u) {  /* ... avoid sec boundary (see Note #1) ...          */
                    FS_TRACE_LOG(("FS_FAT_ClusFreeFind(): Sec boundary clus avoided: %d.\r\n", next_clus));
                    clus_ignore = DEF_YES;
                }
            }
#endif
            if (clus_ignore == DEF_NO) {
                p_fat_data->NextClusNbr = next_clus + 1u;       /* ... else store next clus ...                         */
                FS_TRACE_LOG(("FS_FAT_ClusFreeFind(): New FAT clus alloc'd: %d.\r\n", next_clus));
               *p_err = FS_ERR_NONE;
                return (next_clus);                             /*                           ... and rtn clus.          */
            }
        }

        next_clus++;
        clus_cnt_chkd++;
    }


                                                                /* ------------------- NO CLUS FOUND ------------------ */
   *p_err = FS_ERR_DEV_FULL;
    FS_TRACE_DBG(("FS_FAT_ClusFreeFind(): No free FAT clus could be found.\r\n"));
    return (0u);
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_ClusNextGet()
*
* Description : Get next cluster after current cluster in cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Current cluster number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  FAT entry obtained.
*                               FS_ERR_SYS_CLUS_CHAIN_END    No cluster found.
*                               FS_ERR_SYS_CLUS_INVALID      Next cluster is invalid.
*
*                                                            -------- RETURNED BY FS_FAT_SecRd() --------
*                               FS_ERR_DEV                   Device access error.
*
* Return(s)   : Next cluster number, if a next cluster exists.
*               0,                   if the current cluster is the last in the cluster chain.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_FAT_CLUS_NBR  FS_FAT_ClusNextGet (FS_VOL           *p_vol,
                                     FS_BUF           *p_buf,
                                     FS_FAT_CLUS_NBR   start_clus,
                                     FS_ERR           *p_err)
{
    FS_FAT_CLUS_NBR   next_clus;
    CPU_BOOLEAN       valid;
    FS_FAT_DATA      *p_fat_data;


    p_fat_data = (FS_FAT_DATA  *)p_vol->DataPtr;
    next_clus  =  p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol, /* Rd next clus.                                        */
                                                         p_buf,
                                                         start_clus,
                                                         p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

    if (next_clus >= p_fat_data->FAT_TypeAPI_Ptr->ClusEOF) {    /* Cluster is EOC.                                      */
       *p_err = FS_ERR_SYS_CLUS_CHAIN_END;
        return (0u);
    }

    valid = FS_FAT_IS_VALID_CLUS(p_fat_data, next_clus);
    if (valid == DEF_NO) {                                      /* Cluster is invalid.                                  */
       *p_err = FS_ERR_SYS_CLUS_INVALID;
        return (0u);
    }

   *p_err = FS_ERR_NONE;
    return (next_clus);
}


/*
*********************************************************************************************************
*                                         FS_FAT_SecNextGet()
*
* Description : Get next sector in cluster chain.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_sec   Current sector number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  FAT entry obtained.
*                               FS_ERR_DEV                   Device access error.
*                               FS_ERR_SYS_CLUS_CHAIN_END    Cluster chain ended.
*                               FS_ERR_SYS_CLUS_INVALID      Cluster chain ended with invalid cluster.
*
* Return(s)   : Next sector number, if a next sector exists.
*               0,                  if the current sector is the last in the cluster chain.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_FAT_SEC_NBR  FS_FAT_SecNextGet (FS_VOL          *p_vol,
                                   FS_BUF          *p_buf,
                                   FS_FAT_SEC_NBR   start_sec,
                                   FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR    clus_sec_rem;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_SEC_NBR    next_sec;
    FS_FAT_SEC_NBR    root_dir_sec_last;
    FS_FAT_CLUS_NBR   start_clus;
    FS_FAT_DATA      *p_fat_data;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
                                                                /* ------------------ HANDLE ROOT DIR ----------------- */
    if (p_fat_data->FAT_Type != FS_FAT_FAT_TYPE_FAT32) {
        if (start_sec < p_fat_data->DataStart) {                /* If start sec outside data          ...               */
            if (start_sec >= p_fat_data->RootDirStart) {        /* ... & sec within root dir          ...               */
                root_dir_sec_last = p_fat_data->RootDirSize + p_fat_data->RootDirStart - 1u;
                if (start_sec < root_dir_sec_last) {            /* ... & sec before last root dir sec ...               */
                    next_sec = start_sec + 1u;                  /* ... rtn next sec.                                    */
                   *p_err = FS_ERR_NONE;
                    return (next_sec);
                }
            }

           *p_err = FS_ERR_DIR_FULL;
            return (0u);
        }
    }


                                                                /* ---------------- HANDLE GENERAL SEC ---------------- */
    clus_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, start_sec);

    if (clus_sec_rem != 1u) {                                   /* If more secs rem in clus ...                         */
        next_sec = start_sec + 1u;                              /* ... rtn next sec.                                    */
       *p_err = FS_ERR_NONE;
        return (next_sec);
    }

    start_clus = FS_FAT_SEC_TO_CLUS(p_fat_data, start_sec);
    next_clus  = FS_FAT_ClusNextGet(p_vol,
                                    p_buf,
                                    start_clus,
                                    p_err);
    if (*p_err != FS_ERR_NONE) {
        return (0u);
    }

    next_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, next_clus);
   *p_err = FS_ERR_NONE;
    return (next_sec);
}


/*
*********************************************************************************************************
*                                     FS_FAT_SecNextGetAlloc()
*
* Description : Get next sector in cluster chain OR allocate new one.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Current cluster number.
*
*               clr         Indicates whether sector should be cleared (upon allocation) (See Note #2):
*
*                               DEF_YES, sector MUST be cleared.
*                               DEF_NO,  sector does not need to be cleared.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                  FAT entry obtained.
*                               FS_ERR_DEV                   Device access error.
*                               FS_ERR_DEV_FULL              Device is full (no space could be allocated).
*                               FS_ERR_DIR_FULL              Directory is full (no space could be allocated).
*                               FS_ERR_SYS_CLUS_CHAIN_END    Cluster chain ended.
*                               FS_ERR_SYS_CLUS_INVALID      Cluster chain ended with invalid cluster.
*
* Return(s)   : Next sector number, if a next sector exists or one can be allocated.
*               0,                  otherwise.
*
* Note(s)     : (1) The sector clear operation will overwrite the data stored in the buffer, so the
*                   buffer MUST be flushed before this operation is performed.
*
*               (2) The sector will not be cleared if it is already allocated.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_SEC_NBR  FS_FAT_SecNextGetAlloc (FS_VOL          *p_vol,
                                        FS_BUF          *p_buf,
                                        FS_FAT_SEC_NBR   start_sec,
                                        CPU_BOOLEAN      clr,
                                        FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR    clus_sec_rem;
    FS_FAT_CLUS_NBR   next_clus;
    FS_FAT_SEC_NBR    next_sec;
    FS_FAT_SEC_NBR    root_dir_sec_last;
    FS_FAT_CLUS_NBR   start_clus;
    FS_FAT_DATA      *p_fat_data;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

                                                                /* ------------------ HANDLE ROOT DIR ----------------- */
    if (p_fat_data->FAT_Type != FS_FAT_FAT_TYPE_FAT32) {
        if (start_sec < p_fat_data->DataStart) {                /* If start sec outside data          ...               */
            if (start_sec >= p_fat_data->RootDirStart) {        /* ... & sec within root dir          ...               */
                root_dir_sec_last = p_fat_data->RootDirSize + p_fat_data->RootDirStart - 1u;
                if (start_sec < root_dir_sec_last) {            /* ... & sec before last root dir sec ...               */
                    next_sec = start_sec + 1u;                  /* ... rtn next sec.                                    */
                   *p_err = FS_ERR_NONE;
                    return (next_sec);
                }
            }

           *p_err = FS_ERR_DIR_FULL;
            return (0u);
        }
    }


                                                                /* ---------------- HANDLE GENERAL SEC ---------------- */
    clus_sec_rem = FS_FAT_CLUS_SEC_REM(p_fat_data, start_sec);

    if (clus_sec_rem != 1u) {                                   /* If more secs rem in clus ...                         */
        next_sec = start_sec + 1u;                              /* ... rtn next sec.                                    */
       *p_err = FS_ERR_NONE;
        return (next_sec);
    }

    start_clus = FS_FAT_SEC_TO_CLUS(p_fat_data, start_sec);
    next_clus  = FS_FAT_ClusNextGet(p_vol,
                                    p_buf,
                                    start_clus,
                                    p_err);
    switch (*p_err) {
        case FS_ERR_NONE:                                       /* Another clus in chain already alloc'd.               */
             clr = DEF_NO;                                      /* See Note #2.                                         */
             break;


        case FS_ERR_SYS_CLUS_CHAIN_END:                         /* EOC, alloc another cluster.                          */
             (void) FS_FAT_ClusChainAlloc(p_vol,
                                          p_buf,
                                          start_clus,
                                          1u,
                                          p_err);
             if (*p_err != FS_ERR_NONE) {
                 return (0u);
             }

             next_clus  = FS_FAT_ClusNextGet(p_vol,
                                             p_buf,
                                             start_clus,
                                             p_err);
             if (*p_err != FS_ERR_NONE) {
                 return (0u);
             }
             break;


        default:
             return (0u);                                       /* Prevent 'break NOT reachable' compiler warning.      */
    }




                                                                /* Clr clus, if necessary.                              */
    if (clr == DEF_YES) {
        FSBuf_Flush(p_buf, p_err);                              /* Flush buf (see Note #1).                             */
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        next_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, next_clus);

        FS_FAT_SecClr(p_vol,
                      p_buf->DataPtr,
                      next_sec,
                      p_fat_data->ClusSize_sec,
                      p_fat_data->SecSize,
                      FS_VOL_SEC_TYPE_DIR,                      /* DIR since it's only called with clr == DEF_YES ...   */
                      p_err);                                   /* from DirEntryPlace().                                */
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }
    }

    next_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, next_clus);
   *p_err = FS_ERR_NONE;
    return (next_sec);
}
#endif


/*
*********************************************************************************************************
*                                           FS_FAT_Query()
*
* Description : Obtain information about a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               p_info      Pointer to structure that will receive volume information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    FAT information obtained.
*                               FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_Query (FS_VOL       *p_vol,
                    FS_BUF       *p_buf,
                    FS_SYS_INFO  *p_info,
                    FS_ERR       *p_err)
{
    FS_FAT_CLUS_NBR   bad_clus_cnt;
    FS_FAT_CLUS_NBR   clus;
    FS_FAT_CLUS_NBR   fat_entry;
    FS_FAT_CLUS_NBR   free_clus_cnt;
    FS_FAT_CLUS_NBR   used_clus_cnt;
    FS_FAT_DATA      *p_fat_data;


                                                                /* ----------------- ASSIGN DFLT VAL'S ---------------- */
    p_fat_data         = (FS_FAT_DATA *)p_vol->DataPtr;
    p_info->BadSecCnt  =  0u;
    p_info->FreeSecCnt =  0u;
    p_info->UsedSecCnt =  0u;
    p_info->TotSecCnt  =  0u;



                                                                /* ----------------- CHK IF INFO CACHED --------------- */
    if (p_fat_data->QueryInfoValid == DEF_YES) {
        p_info->BadSecCnt  = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(p_fat_data->QueryBadClusCnt,  p_fat_data->ClusSizeLog2_sec);
        p_info->FreeSecCnt = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(p_fat_data->QueryFreeClusCnt, p_fat_data->ClusSizeLog2_sec);

        if (p_fat_data->MaxClusNbr >= (FS_FAT_MIN_CLUS_NBR + p_fat_data->QueryBadClusCnt + p_fat_data->QueryFreeClusCnt)) {
            used_clus_cnt = ((p_fat_data->MaxClusNbr - FS_FAT_MIN_CLUS_NBR) - p_fat_data->QueryBadClusCnt) - p_fat_data->QueryFreeClusCnt;
        } else {
            used_clus_cnt = 0u;
        }

        p_info->UsedSecCnt = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(used_clus_cnt, p_fat_data->ClusSizeLog2_sec);
        p_info->TotSecCnt  =  p_info->BadSecCnt + p_info->FreeSecCnt + p_info->UsedSecCnt;

       *p_err = FS_ERR_NONE;
        return;
    }



                                                                /* ---------- CNT NBR OF BAD/FREE/USED CLUS'S --------- */
    free_clus_cnt = 0u;
    used_clus_cnt = 0u;
    bad_clus_cnt  = 0u;
    clus          = 2u;
    while (clus < p_fat_data->MaxClusNbr) {
        fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                           p_buf,
                                                           clus,
                                                           p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (fat_entry == p_fat_data->FAT_TypeAPI_Ptr->ClusBad) {
            bad_clus_cnt++;

        } else if (fat_entry == p_fat_data->FAT_TypeAPI_Ptr->ClusFree) {
            free_clus_cnt++;

        } else {
            used_clus_cnt++;
        }

        clus++;
    }



                                                                /* ------------------ CALC SEC CNT'S ------------------ */
    p_fat_data->QueryInfoValid   =  DEF_YES;
    p_fat_data->QueryBadClusCnt  =  bad_clus_cnt;
    p_fat_data->QueryFreeClusCnt =  free_clus_cnt;

    p_info->BadSecCnt            = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(bad_clus_cnt,  p_fat_data->ClusSizeLog2_sec);
    p_info->FreeSecCnt           = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(free_clus_cnt, p_fat_data->ClusSizeLog2_sec);
    p_info->UsedSecCnt           = (FS_SEC_QTY)FS_UTIL_MULT_PWR2(used_clus_cnt, p_fat_data->ClusSizeLog2_sec);
    p_info->TotSecCnt            =  p_info->BadSecCnt + p_info->FreeSecCnt + p_info->UsedSecCnt;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_MakeBootSec()
*
* Description : Make boot sector.
*
* Argument(s) : p_temp              Pointer to temporary buffer.
*
*               p_sys_cfg           Pointer to format configuration information.
*
*               sec_size            Sector size, in octets.
*
*               size                Size of volume in sectors.
*
*               fat_size            Size of one FAT in sectors.
*
*               partition_start     partition start: number of sectors from the MBR
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_VolFmt() Note #6'.
*
*               (2) See 'FS_FAT_VolFmt() Note #7'.
*
*               (3) Avoid 'Excessive shift value' or 'Constant expression evaluates to 0' warning.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_MakeBootSec (void            *p_temp,
                          FS_FAT_SYS_CFG  *p_sys_cfg,
                          FS_SEC_SIZE      sec_size,
                          FS_FAT_SEC_NBR   size,
                          FS_FAT_SEC_NBR   fat_size,
                          FS_SEC_NBR       partition_start)
{
    CPU_INT08U  *p_temp_08;
    CPU_INT16U   sec_per_trk;
    CPU_INT16U   num_heads;
    CPU_INT32U   nbr_32;
    CPU_INT16U   nbr_16;


    p_temp_08 = (CPU_INT08U *)p_temp;


                                                                /* Boot sector & BPB structure (see Note #1).           */
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_OFF_JMPBOOT + 0u],     FS_FAT_BS_JMPBOOT_0);
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_OFF_JMPBOOT + 1u],     FS_FAT_BS_JMPBOOT_1);
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_OFF_JMPBOOT + 2u],     FS_FAT_BS_JMPBOOT_2);

    Mem_Copy((void *)&p_temp_08[FS_FAT_BS_OFF_OEMNAME],                   (void *)FS_FAT_BS_OEMNAME, (CPU_SIZE_T)8);

    MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_BYTSPERSEC],      sec_size);
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_SECPERCLUS],      p_sys_cfg->ClusSize);
    MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_RSVDSECCNT],      p_sys_cfg->RsvdAreaSize);
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_NUMFATS],         p_sys_cfg->NbrFATs);

    if  (p_sys_cfg->FAT_Type == FS_FAT_FAT_TYPE_FAT32) {
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_ROOTENTCNT],      0);
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_TOTSEC16],        0);
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_FATSZ16],         0);
        MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_TOTSEC32],        size);

    } else {
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_ROOTENTCNT],      p_sys_cfg->RootDirEntryCnt);
        if (size <= DEF_INT_16U_MAX_VAL) {
            MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_TOTSEC16],    size);
        }
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_FATSZ16],         fat_size);
        if (size > DEF_INT_16U_MAX_VAL) {
            MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_TOTSEC32],    size);
        }

    }


    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_MEDIA],           FS_FAT_BPB_MEDIA_FIXED);
    sec_per_trk = 0x3Fu;                                        /* See Note #3.                                         */
    MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_SECPERTRK],       sec_per_trk);
    num_heads   = 0xFFu;                                        /* See Note #3.                                         */
    MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_NUMHEADS],        num_heads);
    MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_OFF_HIDDSEC],         partition_start);




    if  (p_sys_cfg->FAT_Type == FS_FAT_FAT_TYPE_FAT32) {        /* FAT32 struct start at offset 36 (see Note #2).       */
        MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_FATSZ32],   fat_size);
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_EXTFLAGS],  0);
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_FSVER],     0);
        nbr_32 = FS_FAT_DFLT_ROOT_CLUS_NBR;                     /* See Note #3.                                         */
        MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_ROOTCLUS],  nbr_32);
        nbr_16 = FS_FAT_DFLT_FSINFO_SEC_NBR;                    /* See Note #3.                                         */
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_FSINFO],    nbr_16);
        nbr_16 = FS_FAT_DFLT_BKBOOTSEC_SEC_NBR;                 /* See Note #3.                                         */
        MEM_VAL_SET_INT16U_LITTLE((void *)&p_temp_08[FS_FAT_BPB_FAT32_OFF_BKBOOTSEC], nbr_16);
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_DRVNUM],     0);
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_RESERVED1],  0);
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_BOOTSIG],    FS_FAT_BS_BOOTSIG);
        MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_VOLID],      0x12345678);

        Mem_Copy((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_VOLLAB],              (void *)FS_FAT_BS_VOLLAB,            (CPU_SIZE_T)11);
        Mem_Copy((void *)&p_temp_08[FS_FAT_BS_FAT32_OFF_FILSYSTYPE],          (void *)FS_FAT_BS_FAT32_FILESYSTYPE, (CPU_SIZE_T) 8);


    } else {                                                    /* FAT12/16 struct start at offset 36 (see Note #2).    */
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_DRVNUM],     0);
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_RESERVED1],  0);
        MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_BOOTSIG],    FS_FAT_BS_BOOTSIG);
        MEM_VAL_SET_INT32U_LITTLE((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_VOLID],      0x12345678u);

        Mem_Copy((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_VOLLAB],              (void *)FS_FAT_BS_VOLLAB,            (CPU_SIZE_T)11);
        if  (p_sys_cfg->FAT_Type == FS_FAT_FAT_TYPE_FAT12) {
            Mem_Copy((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_FILSYSTYPE],      (void *)FS_FAT_BS_FAT12_FILESYSTYPE, (CPU_SIZE_T) 8);

        } else if  (p_sys_cfg->FAT_Type == FS_FAT_FAT_TYPE_FAT16) {
            Mem_Copy((void *)&p_temp_08[FS_FAT_BS_FAT1216_OFF_FILSYSTYPE],      (void *)FS_FAT_BS_FAT16_FILESYSTYPE, (CPU_SIZE_T) 8);
        } else {
            FS_TRACE_DBG(("FS_FAT_MakeBootSec: Invalid FAT_Type of arg p_sys_cfg.\r\n"));
        }
    }


                                                                /* Boot sector signature.                               */
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BOOT_SIG_LO_OFF],         FS_FAT_BOOT_SIG_LO);
    MEM_VAL_SET_INT08U_LITTLE((void *)&p_temp_08[FS_FAT_BOOT_SIG_HI_OFF],         FS_FAT_BOOT_SIG_HI);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       SYSTEM DRIVER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FS_FAT_ModuleInit()
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

void  FS_FAT_ModuleInit (FS_QTY   vol_cnt,
                         FS_QTY   file_cnt,
                         FS_QTY   dir_cnt,
                         FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


                                                                /* ----------------- CREATE INFO POOL ----------------- */
    Mem_PoolCreate(&FS_FAT_DataPool,
                    DEF_NULL,
                    0,
                    vol_cnt,
                    sizeof(FS_FAT_DATA),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FS_FAT_ModuleInit(): Could not alloc mem for FAT info: %d octets req'd.\r\n", octets_reqd));
        return;
    }



                                                                /* --------------- INIT FAT FILE MODULE --------------- */
    FS_FAT_FileModuleInit(file_cnt,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }



#ifdef  FS_DIR_MODULE_PRESENT                                   /* ---------------- INIT FAT DIR MODULE --------------- */
    if (dir_cnt != 0u) {
        FS_FAT_DirModuleInit(dir_cnt,
                             p_err);

        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#else
    (void)dir_cnt;
#endif



#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT                           /* -------------- INIT FAT JOURNAL MODULE ------------- */
    FS_FAT_JournalModuleInit(vol_cnt, p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }
#endif

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FS_FAT_VolClose()
*
* Description : Free system driver-specific structures.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to release the FAT data back to the FAT data pool.
*********************************************************************************************************
*/

void  FS_FAT_VolClose (FS_VOL  *p_vol)
{
    FS_FAT_DATA  *p_fat_data;
    FS_ERR        err;
    LIB_ERR       pool_err;


                                                                /* ----------------- FREE JOURNAL DATA ---------------- */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalExit(p_vol, &err);                            /* Free journal data.                                   */
    if (err != FS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);                                    /* Fatal err.                                           */
    }
#endif



                                                                /* ------------------- FREE FAT DATA ------------------ */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    FS_OS_Lock(&err);                                           /* Acquire FS lock (see Note #1).                       */
    if (err != FS_ERR_NONE) {
        return;
    }

    Mem_PoolBlkFree(        &FS_FAT_DataPool,                   /* Free FAT data.                                       */
                    (void *) p_fat_data,
                            &pool_err);
    FS_OS_Unlock();

    if (pool_err != LIB_MEM_ERR_NONE) {
        CPU_SW_EXCEPTION(;);                                    /* Fatal err.                                           */
    }

    p_vol->DataPtr = (void *)0;                                 /* Clr data ptr.                                        */
}


/*
*********************************************************************************************************
*                                           FS_FAT_VolFmt()
*
* Description : Format a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_sys_cfg   File system configuration.
*               ----------  Argument validated by caller.
*
*               sec_size    Sector size in octets.
*
*               size        Number of sectors in file system.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                Volume formatted successfully.
*                               FS_ERR_BUF_NONE_AVAIL      No buf available.
*                               FS_ERR_DEV                 Device access error.
*                               FS_ERR_DEV_INVALID_SIZE    Invalid device size.
*
*                                                          ------- RETURNED BY FS_FAT_GetSysCfg() -------
*                               FS_ERR_VOL_INVALID_SYS     Invalid volume for file system.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_VolOpen() Notes #1 & #2'.
*
*               (2) The minimum number of sectors in a FAT volume is calculated by :
*
*                       MinSecCnt = RsvdSecCnt + RootDirSecCnt + FATSecCnt(min) * NbrFATs + DataClusCnt(min) * ClusSize_sec + (ClusSize_sec - 1);
*
*                   where
*
*                       RsvdSecCnt    is the number of sectors in the reserved area.
*                       RootDirSecCnt is the number of sectors in the root directory.
*                       FATSecCnt     is the number of sectors in the each FAT.
*                       NbrFATs       is the number of FATs.
*                       DataSecCnt    is the number of sectors in the data area.
*
*                   (a) The number of sectors in the root directory is calculated by :
*
*                                                     RootDirEntriesCnt * 32       RootDirEntriesCnt * 32 + SecSize
*                           RootDirSecCnt = ceiling( ------------------------ ) = ----------------------------------
*                                                          BytesPerSec                           SecSize
*
*                       where
*
*                           SecSize           is the size of a sector, in octets.
*                           RootDirEntriesCnt is the number of root directory entries.
*
*                   (b) The number of sectors in each FAT is calculated by :
*
*                                                         DataClusCnt               DataClusCnt + (SecSize / BytesPerEntry) - 1
*                           FATSecCnt = ceiling( ----------------------------- ) = ---------------------------------------------
*                                                   SecSize / BytesPerEntry                   SecSize / BytesPerEntry
*
*                       where
*
*                           SecSize       is the size of a sector, in octets.
*                           BytesPerEntry is the number of bytes in one FAT entry.
*                           DataClusCnt   is the number of clusters in the data area.
*
*                   (c) For example, a minimum-size FAT32 volume, with a default number of reserved
*                       sectors (32) & number of FATs (2), with 512-byte sectors & 1 sector per
*                       cluster, would need
*
*                                                 (65535 + 1) + (512 / 4) - 1
*                           MinSecCnt = 32 + 0 + ----------------------------- * 2 + (65535 + 1) * 1 = 66592 sectors
*                                                           512 / 4
*
*                       where all multiplication & divide operations are integer divide & multiply.
*
*                       (1) [Ref 1] recommends that no volume be formatted with close to 4085 or 65525
*                           clusters, the minimum cluster counts for FAT16 & FAT32 volumes, since some
*                           FAT driver implementations do not determine properly FAT type.  In case
*                           volumes formatted with this file system suite are used with such
*                           non-compliant FAT driver implementations, no volume is formatted with
*                           4069-5001 or 65509-65541 clusters.
*
*               (3) The first sector of the first cluster is always ClusSize_sec-aligned.
*
*               (4) The number of sectors in the data area of the FAT is related to the number of sectors
*                   in the file system :
*
*                       TotSecCnt = DataSecCnt + (FATSecCnt * NbrFATs) + RootDirSecCnt + RsvdSecCnt
*
*                                                                  DataClusCnt
*                                 = DataSecCnt + ceiling(-----------------------------) * NbrFATs + RootDirSecCnt + RsvdSecCnt
*                                                            SecSize / BytesPerEntry
*
*                                                    DataSecCnt / ClusSize_sec
*                                 > DataSecCnt + (-----------------------------) * NbrFATs + RootDirSecCnt + RsvdSecCnt
*                                                    SecSize / BytesPerEntry
*
*                   so
*
*                                         (TotSecCnt - RootDirSecCnt - RsvdSecCnt) * (SecSize / BytesPerEntry)
*                       DataSecCnt < --------------------------------------------------------------------------------
*                                    (SecSize * ClusSize_sec + BytesPerEntry * NbrFATs) / (ClusSize_sec * BytesPerEntry)
*
*                                     (TotSecCnt - RootDirSecCnt - RsvdSecCnt) * (SecSize * ClusSize_sec)
*                                  < -------------------------------------------------------------------
*                                                SecSize * ClusSize_sec + BytesPerEntry * NbrFATs
*
*                   Since the numerator MAY overflow a 32-bit unsigned integer, the calculation is
*                   re-ordered & rounded up to provide an upper limit:
*
*                                           TotSecCnt - RootDirSecCnt - RsvdSecCnt
*                       DataSecCnt < (------------------------------------------------ + 1) * (SecSize * ClusSize_sec)
*                                      SecSize * ClusSize_sec + BytesPerEntry * NbrFATs
*
*               (5) The number of sectors in a FAT is calculated from the data sector count, as calculated
*                   above.  Each FAT will have sufficient entries for the data area; depending on the
*                   configuration, it MAY have extra entries or sector(s).
*
*               (6) The boot sector & BPB structure is covered in [Ref 1], Pages 9-10.
*
*                   (a) Several fields are assigned 'default' values :
*
*                       (1) BS_jmpBoot is assigned {0xEB, 0x00, 0x90}.
*                       (2) BS_OEMName is assigned "MSWIN4.1".
*                       (3) BPB_Media  is assigned 0xF8.
*
*                   (b) Several fields are always assigned 'zero' values :
*
*                       (1) BPB_SecPerTrk.
*                       (2) BPB_NumHeads.
*                       (3) BPB_HiddSec.
*
*                   (c) (1) For FAT32, several more fields are always assigned 'zero' values :
*
*                           (a) BPB_RootEntCnt.
*                           (b) BPB_TotSec16.
*                           (c) BPB_FatSz16.
*
*                           The remaining fields contain information describing the file system ;
*
*                           (a) BPB_BytsPerSec.
*                           (b) BPB_SecPerClus.
*                           (c) BPB_RsvdSecCnt.
*                           (d) BPB_NumFATs (should be 2).
*                           (e) BPB_TotSec32.
*
*                       (2) For FAT12/16, several more fields are always assigned 'zero' values :
*
*                           (a) BPB_TotSec32.
*
*                           The remaining fields contain information describing the file system :
*
*                           (a) BPB_RootEntCnt (defaults to 512).
*                           (b) BPB_TotSec16.
*                           (c) BPB_FatSz16.
*                           (d) BPB_BytsPerSec.
*                           (e) BPB_SecPerClus.
*                           (f) BPB_RsvdSecCnt (should be 1).
*                           (g) BPB_NumFATs (should be 2).
*
*               (7) Default case already invalidated by prior assignment of 'sys_cfg.FAT_Type'.  However,
*                   the default case is included an extra precaution in case 'sys_cfg.FAT_Type' is
*                   incorrectly modified.
*
*               (8) Avoid 'Excessive shift value' or 'Constant expression evaluates to 0' warning.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_VolFmt (FS_VOL       *p_vol,
                     void         *p_sys_cfg,
                     FS_SEC_SIZE   sec_size,
                     FS_SEC_QTY    size,
                     FS_ERR       *p_err)
{
    FS_BUF            *p_buf;
    FS_FAT_CLUS_NBR    data_size;
    CPU_INT08U         fat_ix;
    FS_FAT_SEC_NBR     fat_size;
    FS_FAT_SEC_NBR     fat_sec_cur;
    FS_FAT_SEC_NBR     fat1_sec_start;
    FS_FAT_SYS_CFG     sys_cfg;
    FS_FAT_SYS_CFG    *p_fat_sys_cfg;
    FS_FAT_SEC_NBR     root_dir_size;
    FS_FAT_SEC_NBR     root_dir_sec_start;
    FS_FAT_SEC_NBR     rsvd_sec_start;
    CPU_INT32U         val;
    FS_FAT_SEC_NBR     data_sec_start;
    CPU_INT32U         clus_number;
#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
    CPU_INT08U         partition_type;
    FS_ERR             err_tmp;
#endif


                                                                /* ----------------- VERIFY DISK INFO ----------------- */
    if (size == 0u) {                                           /* Verify nbr secs.                                     */
        FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid dev size: 0.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_SIZE;
        return;
    }

    if (p_sys_cfg == (FS_FAT_SYS_CFG *)0) {
        FS_FAT_GetSysCfg( sec_size,                             /* Get FAT configuration.                               */
                          size,
                         &sys_cfg,
                          p_err);

        if (*p_err != FS_ERR_NONE) {
            return;
        }
    } else {
        p_fat_sys_cfg = (FS_FAT_SYS_CFG *)p_sys_cfg;

        sys_cfg.FAT_Type        = p_fat_sys_cfg->FAT_Type;
        sys_cfg.RootDirEntryCnt = p_fat_sys_cfg->RootDirEntryCnt;
        sys_cfg.RsvdAreaSize    = p_fat_sys_cfg->RsvdAreaSize;
        sys_cfg.ClusSize        = p_fat_sys_cfg->ClusSize;
        sys_cfg.NbrFATs         = p_fat_sys_cfg->NbrFATs;

        if ((sys_cfg.ClusSize == 0u) ||
            (sys_cfg.ClusSize >  128u)) {
            FS_TRACE_DBG(("FS_FAT_VolFmt(): Cfg'd cluster size invalid: %d.\r\n", sys_cfg.ClusSize));
           *p_err = FS_ERR_VOL_INVALID_SYS;
            return;
        }

        if ((sys_cfg.ClusSize & (sys_cfg.ClusSize - 1u)) != 0u) { /* Check if ClusSize is a pwr of 2.                   */
            FS_TRACE_DBG(("FS_FAT_VolFmt(): Cfg'd cluster size invalid: %d.\r\n", sys_cfg.ClusSize));
           *p_err = FS_ERR_VOL_INVALID_SYS;
            return;
        }

        if ((sys_cfg.NbrFATs != 1u) && (sys_cfg.NbrFATs != 2u)) {
             sys_cfg.NbrFATs  = 2u;
             FS_TRACE_DBG(("FS_FAT_VolFmt(): Setting nbr FATs to 2.\r\n"));
        }

        switch (sys_cfg.FAT_Type) {
#if (FS_FAT_CFG_FAT12_EN == DEF_ENABLED)
            case FS_FAT_FAT_TYPE_FAT12:
                 if (sys_cfg.RootDirEntryCnt == 0u) {
                     sys_cfg.RootDirEntryCnt = DEF_MIN(512u, 1u + (size / 2u));
                 }
                 if (sys_cfg.RsvdAreaSize == 0u) {
                     sys_cfg.RsvdAreaSize = FS_FAT_DFLT_RSVD_SEC_CNT_FAT12;
                     FS_TRACE_DBG(("FS_FAT_VolFmt(): Setting FAT rsvd area size to default: %d.\r\n", FS_FAT_DFLT_RSVD_SEC_CNT_FAT12));
                 }
                 break;
#endif


#if (FS_FAT_CFG_FAT16_EN == DEF_ENABLED)
            case FS_FAT_FAT_TYPE_FAT16:
                 if (sys_cfg.RootDirEntryCnt == 0u) {
                     sys_cfg.RootDirEntryCnt = (CPU_INT16U)DEF_MIN(512u, 1u + (size / 2u));
                 }
                 if (sys_cfg.RsvdAreaSize == 0u) {
                     sys_cfg.RsvdAreaSize = FS_FAT_DFLT_RSVD_SEC_CNT_FAT16;
                     FS_TRACE_DBG(("FS_FAT_VolFmt(): Setting FAT rsvd area size to default: %d.\r\n", FS_FAT_DFLT_RSVD_SEC_CNT_FAT16));
                 }
                 break;
#endif


#if (FS_FAT_CFG_FAT32_EN == DEF_ENABLED)
            case FS_FAT_FAT_TYPE_FAT32:
                 if (sys_cfg.RootDirEntryCnt != 0u) {
                     sys_cfg.RootDirEntryCnt  = 0u;
                     FS_TRACE_DBG(("FS_FAT_VolFmt(): Setting root dir entry cnt to 0 for FAT32.\r\n"));
                 }
                 if (sys_cfg.RsvdAreaSize == 0u) {
                     sys_cfg.RsvdAreaSize = FS_FAT_DFLT_RSVD_SEC_CNT_FAT32;
                     FS_TRACE_DBG(("FS_FAT_VolFmt(): Setting FAT rsvd area size to default: %d.\r\n", FS_FAT_DFLT_RSVD_SEC_CNT_FAT32));
                 } else if (sys_cfg.RsvdAreaSize < FS_FAT_DFLT_BKBOOTSEC_SEC_NBR + 1u) {
                                                                /* Make sure there is enough room for backup boot sec.  */
                     FS_TRACE_DBG(("FS_FAT_VolFmt(): Cfg'd FAT rsvd area size invalid: %d.\r\n", sys_cfg.RsvdAreaSize));
                    *p_err = FS_ERR_VOL_INVALID_SYS;
                     return;
                 } else {
                                                                /* Rsvd area size cfg is legal.                         */
                 }
                 break;
#endif


            default:
                 FS_TRACE_DBG(("FS_FAT_VolFmt(): Cfg'd FAT type invalid: %d.\r\n", sys_cfg.ClusSize));
                *p_err = FS_ERR_VOL_INVALID_SYS;
                 return;
        }
    }


    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err =   FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FSVol_ReleaseLocked(p_vol,                                  /* Release vol secs.                                    */
                        0u,
                        size,
                        p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

    root_dir_size = (((FS_FAT_SEC_NBR)sys_cfg.RootDirEntryCnt * FS_FAT_SIZE_DIR_ENTRY) + (sec_size - 1u)) / sec_size;
    if(size <= root_dir_size + sys_cfg.RsvdAreaSize) {
        FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number.\r\n"));
       *p_err = FS_ERR_VOL_INVALID_SYS;
        FSBuf_Free(p_buf);
        return;
    }
    switch (sys_cfg.FAT_Type) {
#if (FS_FAT_CFG_FAT16_EN == DEF_ENABLED)
        case FS_FAT_FAT_TYPE_FAT16:                             /* ----------------- CALC FMT PARAM'S ----------------- */
                                                                /* Calc data sec cnt (see Note #4).                     */
             data_size          = ((((size - root_dir_size) - sys_cfg.RsvdAreaSize)
                                /  (((FS_FAT_SEC_NBR)sys_cfg.ClusSize * sec_size) + (FS_FAT_FAT16_ENTRY_NBR_OCTETS * (FS_FAT_SEC_NBR)sys_cfg.NbrFATs))) + 1u) * sec_size;
                                                                /* Calc fat sec cnt (see Note #5).                      */
             fat_size           = (data_size + (sec_size / FS_FAT_FAT16_ENTRY_NBR_OCTETS) - 1u)
                                / (sec_size / FS_FAT_FAT16_ENTRY_NBR_OCTETS);

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
                                                                /* Find the FAT16 type                                  */
             partition_type     = (size * sec_size) > FS_FAT_MAX_SIZE_HUGE_FAT16 ? FS_PARTITION_TYPE_FAT16_CHS_32MB_2GB :
                                                                                   FS_PARTITION_TYPE_FAT16_16_32MB;
#endif

                                                                /* Calc start sec nbr's.                                */
             rsvd_sec_start     = 0u;
             fat1_sec_start     = rsvd_sec_start + sys_cfg.RsvdAreaSize;
             root_dir_sec_start = fat1_sec_start + (fat_size * sys_cfg.NbrFATs);


             data_sec_start     = root_dir_sec_start + root_dir_size;
             clus_number        = DEF_MIN((size - data_sec_start) / sys_cfg.ClusSize, (fat_size * sec_size) / FS_FAT_FAT16_ENTRY_NBR_OCTETS);

             if(data_sec_start > size) {
                 FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                *p_err = FS_ERR_VOL_INVALID_SYS;
                 FSBuf_Free(p_buf);
                 return;
             }

             if (clus_number <= FS_FAT_MAX_NBR_CLUS_FAT12) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                 *p_err = FS_ERR_VOL_INVALID_SYS;
                  FSBuf_Free(p_buf);
                  return;
             } else if (clus_number > FS_FAT_MAX_NBR_CLUS_FAT16) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                 *p_err = FS_ERR_VOL_INVALID_SYS;
                  FSBuf_Free(p_buf);
                  return;
             } else if (clus_number <= (FS_FAT_MAX_NBR_CLUS_FAT12 + FS_FAT_CLUS_NBR_TOLERANCE)) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): clus number within unrecommended margin: %d.\r\n", clus_number));
             } else if (clus_number >  (FS_FAT_MAX_NBR_CLUS_FAT16 - FS_FAT_CLUS_NBR_TOLERANCE)) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): clus number within unrecommended margin: %d.\r\n", clus_number));
             } else {
                 ;
             }

#if (FS_TRACE_LEVEL != TRACE_LEVEL_OFF)
             FS_TRACE_INFO(("FS_FAT_VolFmt(): Creating file system: Type     : FAT16  \r\n"));
             FS_TRACE_INFO(("                                       Sec  size: %d B   \r\n", sec_size));
             FS_TRACE_INFO(("                                       Clus size: %d sec \r\n", sys_cfg.ClusSize));
             FS_TRACE_INFO(("                                       Vol  size: %d sec \r\n", size));
             FS_TRACE_INFO(("                                       # Clus   : %d     \r\n", clus_number));
             FS_TRACE_INFO(("                                       # FATs   : %d     \r\n", sys_cfg.NbrFATs));
#endif

                                                                /* --------------------- FMT DISK --------------------- */
             Mem_Clr(p_buf->DataPtr, (CPU_SIZE_T)sec_size);
             FS_FAT_MakeBootSec(p_buf->DataPtr,
                               &sys_cfg,
                                sec_size,
                                size,
                                fat_size,
                                p_vol->PartitionStart);

             FSVol_WrLockedEx(p_vol,                            /* Wr to boot sec.                                      */
                              p_buf->DataPtr,
                              0u,
                              1u,
                              FS_VOL_SEC_TYPE_MGMT,
                              p_err);

             if (*p_err != FS_ERR_NONE) {
                 *p_err  = FS_ERR_DEV;
                  FSBuf_Free(p_buf);
                  return;
             }
             break;
#endif


#if (FS_FAT_CFG_FAT32_EN == DEF_ENABLED)
        case FS_FAT_FAT_TYPE_FAT32:                             /* ----------------- CALC FMT PARAM'S ----------------- */
                                                                /* Calc data sec cnt (see Note #4).                     */
             data_size          = ((((size - root_dir_size) - sys_cfg.RsvdAreaSize)
                                /  ((sys_cfg.ClusSize * sec_size) + (FS_FAT_FAT32_ENTRY_NBR_OCTETS * (FS_FAT_SEC_NBR)sys_cfg.NbrFATs))) + 1u) * sec_size;
                                                                /* Calc fat sec cnt (see Note #5).                      */
             fat_size           = (data_size + (sec_size / FS_FAT_FAT32_ENTRY_NBR_OCTETS) - 1u)
                                / (sec_size / FS_FAT_FAT32_ENTRY_NBR_OCTETS);

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
                                                                /* FAT32 type                                           */
             partition_type     = FS_PARTITION_TYPE_FAT32_LBA;
#endif
                                                                /* Calc start sec nbr's.                                */
             rsvd_sec_start     = 0u;
             fat1_sec_start     = rsvd_sec_start + sys_cfg.RsvdAreaSize;
             root_dir_sec_start = fat1_sec_start + (fat_size * sys_cfg.NbrFATs);

             data_sec_start     = root_dir_sec_start + root_dir_size;
             clus_number        = DEF_MIN((size - data_sec_start) / sys_cfg.ClusSize, (fat_size * sec_size) / FS_FAT_FAT32_ENTRY_NBR_OCTETS);

             if(data_sec_start > size) {
                 FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                *p_err = FS_ERR_VOL_INVALID_SYS;
                 FSBuf_Free(p_buf);
                 return;
             }

             if (clus_number <= FS_FAT_MAX_NBR_CLUS_FAT16) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                 *p_err = FS_ERR_VOL_INVALID_SYS;
                  FSBuf_Free(p_buf);
                  return;
             } else if (clus_number <= (FS_FAT_MAX_NBR_CLUS_FAT16 + FS_FAT_CLUS_NBR_TOLERANCE)) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): clus number within unrecommended margin: %d.\r\n", clus_number));
             } else {
                 ;
             }

#if (FS_TRACE_LEVEL != TRACE_LEVEL_OFF)
             FS_TRACE_INFO(("FS_FAT_VolFmt(): Creating file system: Type     : FAT32  \r\n"));
             FS_TRACE_INFO(("                                       Sec  size: %d B   \r\n", sec_size));
             FS_TRACE_INFO(("                                       Clus size: %d sec \r\n", sys_cfg.ClusSize));
             FS_TRACE_INFO(("                                       Vol  size: %d sec \r\n", size));
             FS_TRACE_INFO(("                                       # Clus   : %d     \r\n", clus_number));
             FS_TRACE_INFO(("                                       # FATs   : %d     \r\n", sys_cfg.NbrFATs));
#endif

                                                                /* --------------------- FMT DISK --------------------- */
             Mem_Clr(p_buf->DataPtr, (CPU_SIZE_T)sec_size);
             FS_FAT_MakeBootSec(p_buf->DataPtr,
                               &sys_cfg,
                                sec_size,
                                size,
                                fat_size,
                                p_vol->PartitionStart);

             FSVol_WrLockedEx(p_vol,                            /* Wr to boot sec.                                      */
                              p_buf->DataPtr,
                              0u,
                              1u,
                              FS_VOL_SEC_TYPE_MGMT,
                              p_err);

             if (*p_err != FS_ERR_NONE) {
                 *p_err  = FS_ERR_DEV;
                  FSBuf_Free(p_buf);
                  return;
             }

             FSVol_WrLockedEx(p_vol,                            /* Wr to bk boot sec.                                   */
                              p_buf->DataPtr,
                              FS_FAT_DFLT_BKBOOTSEC_SEC_NBR,
                              1u,
                              FS_VOL_SEC_TYPE_MGMT,
                              p_err);

             if (*p_err != FS_ERR_NONE) {
                 *p_err  = FS_ERR_DEV;
                  FSBuf_Free(p_buf);
                  return;
             }
             break;
#endif

#if (FS_FAT_CFG_FAT12_EN == DEF_ENABLED)
        case FS_FAT_FAT_TYPE_FAT12:                             /* ----------------- CALC FMT PARAM'S ----------------- */
                                                                /* Calc data sec cnt (see Note #4).                     */
             data_size          = ((((size - root_dir_size) - sys_cfg.RsvdAreaSize)
                                /  (((FS_FAT_SEC_NBR)sys_cfg.ClusSize * sec_size) + ((FS_FAT_SEC_NBR)sys_cfg.NbrFATs + ((FS_FAT_SEC_NBR)sys_cfg.NbrFATs / 2u)))) + 1u) * sec_size;
                                                                /* Calc fat sec cnt (see Note #5).                      */
             fat_size           = (data_size + (((sec_size * 2u) + 1u) / 3u) - 1u)
                                / (((sec_size * 2u) + 1u) / 3u);

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
                                                                /* FAT12 type                                           */
             partition_type     = FS_PARTITION_TYPE_FAT12_CHS;
#endif

                                                                /* Calc start sec nbr's.                                */
             rsvd_sec_start     = 0u;
             fat1_sec_start     = rsvd_sec_start + sys_cfg.RsvdAreaSize;
             root_dir_sec_start = fat1_sec_start + (fat_size * sys_cfg.NbrFATs);

             data_sec_start     = root_dir_sec_start + root_dir_size;
             clus_number        = DEF_MIN((size - data_sec_start) / sys_cfg.ClusSize, (fat_size * sec_size * 2u) / 3u);

             if(data_sec_start > size) {
                 FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                *p_err = FS_ERR_VOL_INVALID_SYS;
                 FSBuf_Free(p_buf);
                 return;
             }

             if (clus_number > FS_FAT_MAX_NBR_CLUS_FAT12) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): Invalid clus number: %d.\r\n", clus_number));
                 *p_err = FS_ERR_VOL_INVALID_SYS;
                  FSBuf_Free(p_buf);
                  return;
             } else if (clus_number > (FS_FAT_MAX_NBR_CLUS_FAT12 - FS_FAT_CLUS_NBR_TOLERANCE)) {
                  FS_TRACE_DBG(("FS_FAT_VolFmt(): clus number within unrecommended margin: %d.\r\n", clus_number));
             } else {
                 ;
             }

#if (FS_TRACE_LEVEL != TRACE_LEVEL_OFF)
             FS_TRACE_INFO(("FS_FAT_VolFmt(): Creating file system: Type     : FAT12  \r\n"));
             FS_TRACE_INFO(("                                       Sec  size: %d B   \r\n", sec_size));
             FS_TRACE_INFO(("                                       Clus size: %d sec \r\n", sys_cfg.ClusSize));
             FS_TRACE_INFO(("                                       Vol  size: %d sec \r\n", size));
             FS_TRACE_INFO(("                                       # Clus   : %d     \r\n", clus_number));
             FS_TRACE_INFO(("                                       # FATs   : %d     \r\n", sys_cfg.NbrFATs));
#endif

                                                                /* --------------------- FMT DISK --------------------- */
             Mem_Clr(p_buf->DataPtr, (CPU_SIZE_T)sec_size);
             FS_FAT_MakeBootSec(p_buf->DataPtr,
                               &sys_cfg,
                                sec_size,
                                size,
                                fat_size,
                                p_vol->PartitionStart);


             FSVol_WrLockedEx(p_vol,                            /* Wr to boot sec.                                      */
                              p_buf->DataPtr,
                              0u,
                              1u,
                              FS_VOL_SEC_TYPE_MGMT,
                              p_err);

             if (*p_err != FS_ERR_NONE) {
                 *p_err = FS_ERR_DEV;
                  FSBuf_Free(p_buf);
                  return;
             }
             break;
#endif

        default:                                                /* See Note #7.                                         */
             FSBuf_Free(p_buf);
            *p_err = FS_ERR_DEV_INVALID_SIZE;
             return;
    }

                                                                /* --------------------- CLR FATs --------------------- */
    fat_ix      = 0u;
    fat_sec_cur = fat1_sec_start;
    while (fat_ix < sys_cfg.NbrFATs) {                          /* Clr each FAT.                                        */
        FS_FAT_SecClr(p_vol,
                      p_buf->DataPtr,
                      fat_sec_cur,
                      fat_size,
                      sec_size,
                      FS_VOL_SEC_TYPE_MGMT,
                      p_err);
        if (*p_err != FS_ERR_NONE) {
            *p_err  = FS_ERR_DEV;
             FSBuf_Free(p_buf);
             return;
        }

        fat_sec_cur += fat_size;
        fat_ix++;
    }



                                                                /* ----------------- WR FIRST FAT SEC ----------------- */
    switch (sys_cfg.FAT_Type) {                                 /* Set first entries of FAT.                            */
        case FS_FAT_FAT_TYPE_FAT12:
             val = 0x00FF8F00u | FS_FAT_BPB_MEDIA_FIXED;        /* See Note #8.                                         */
             MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + 0u), val);
             break;


        case FS_FAT_FAT_TYPE_FAT16:
             val = 0xFFF8FF00u | FS_FAT_BPB_MEDIA_FIXED;        /* See Note #8.                                         */
             MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + 0u), val);
             break;


        case FS_FAT_FAT_TYPE_FAT32:
             val = 0x0FFFFFF8u;                                 /* See Note #8.                                         */
             MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + 0u), val);
             val = 0x0FFFFF00u | FS_FAT_BPB_MEDIA_FIXED;        /* See Note #8.                                         */
             MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + 4u), val);
             val = 0x0FFFFFF8u;                                 /* See Note #8.                                         */
             MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + 8u), val); /* Clus 2 is root dir clus.   */
             break;


        default:                                                /* See Note #7.                                         */
             FSBuf_Free(p_buf);
            *p_err = FS_ERR_DEV_INVALID_SIZE;
             break;
    }

    fat_ix      = 0u;
    fat_sec_cur = fat1_sec_start;
    while (fat_ix < sys_cfg.NbrFATs) {                          /* Wr sec to each FAT.                                  */
        FSVol_WrLockedEx(p_vol,
                         p_buf->DataPtr,
                         fat_sec_cur,
                         1u,
                         FS_VOL_SEC_TYPE_MGMT,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            *p_err  = FS_ERR_DEV;
             FSBuf_Free(p_buf);
             return;
        }

        fat_ix++;
        fat_sec_cur += fat_size;
    }



                                                                /* ------------------ WR FSINFO SEC ------------------- */
    if (sys_cfg.FAT_Type == FS_FAT_FAT_TYPE_FAT32) {
        Mem_Clr(p_buf->DataPtr, (CPU_SIZE_T)sec_size);
        val = FS_FAT_FSI_LEADSIG;                               /* See Note #8.                                         */
        MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + FS_FAT_FSI_OFF_LEADSIG),    val);
        val = FS_FAT_FSI_STRUCSIG;                              /* See Note #8.                                         */
        MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + FS_FAT_FSI_OFF_STRUCSIG),   val);
        val = FS_FAT_FSI_TRAILSIG;                              /* See Note #8.                                         */
        MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + FS_FAT_FSI_OFF_TRAILSIG),   val);
                                                                /* Only one clus (for root dir) is alloc'd.             */
        MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + FS_FAT_FSI_OFF_FREE_COUNT), data_size - 1u);
                                                                /* Next free clus is clus after root dir clus.          */
        val = FS_FAT_DFLT_ROOT_CLUS_NBR + 1u;                   /* See Note #8.                                         */
        MEM_VAL_SET_INT32U_LITTLE((void *)((CPU_INT08U *)p_buf->DataPtr + FS_FAT_FSI_OFF_NXT_FREE),   val);

        FSVol_WrLockedEx(p_vol,                                 /* Wr FSINFO sec.                                       */
                         p_buf->DataPtr,
                         FS_FAT_DFLT_FSINFO_SEC_NBR,
                         1u,
                         FS_VOL_SEC_TYPE_MGMT,
                         p_err);

        if (*p_err != FS_ERR_NONE) {
            *p_err  = FS_ERR_DEV;
             FSBuf_Free(p_buf);
             return;
        }
    }
                                                                /* Update the Partition FAT type                        */

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
    if (p_vol->PartitionStart > 0u) {
        FSPartition_Update ( p_vol->DevPtr,
                             p_vol->PartitionNbr,
                             partition_type,
                            &err_tmp);
    }
#endif

                                                                /* ------------------- CLR ROOT DIR ------------------- */
    if (sys_cfg.FAT_Type == FS_FAT_FAT_TYPE_FAT32) {
        FS_FAT_SecClr(p_vol,                                    /* Clr root dir clus.                                   */
                      p_buf->DataPtr,
                      root_dir_sec_start,
                      sys_cfg.ClusSize,
                      sec_size,
                      FS_VOL_SEC_TYPE_DIR,
                      p_err);
    } else {
        FS_FAT_SecClr(p_vol,                                    /* Clr root dir.                                        */
                      p_buf->DataPtr,
                      root_dir_sec_start,
                      root_dir_size,
                      sec_size,
                      FS_VOL_SEC_TYPE_DIR,
                      p_err);
    }

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return;
    }

    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FS_FAT_VolLabelGet()
*
* Description : Get volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               label       String buffer that will receive volume label.
*               ----------  Argument validated by caller.
*
*               len_max     Size of string buffer (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Label gotten.
*                               FS_ERR_VOL_LABEL_NOT_FOUND    Volume label was not found.
*                               FS_ERR_VOL_LABEL_TOO_LONG     Volume label is too long.
*                               FS_ERR_DEV                    Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The maximum label length is 11 characters.  If the label is shorter that this, it
*                   should be padded with space characters (ASCII code point 20h) in the directory entry;
*                   those final space characters are kept in the string returned to the application.
*********************************************************************************************************
*/

void  FS_FAT_VolLabelGet (FS_VOL      *p_vol,
                          CPU_CHAR    *label,
                          CPU_SIZE_T   len_max,
                          FS_ERR      *p_err)
{
    CPU_CHAR    label_buf[FS_FAT_VOL_LABEL_LEN + 1u];
    CPU_SIZE_T  label_len;


    Mem_Clr (label_buf, sizeof(label_buf));
    FS_FAT_SFN_LabelGet(p_vol,
                        label_buf,
                        p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    label_len = Str_Len_N(label_buf, FS_FAT_VOL_LABEL_LEN);
    if (label_len > len_max) {
       *p_err = FS_ERR_VOL_LABEL_TOO_LONG;
        return;
    }

    Str_Copy_N(label, label_buf, len_max);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_VolLabelSet()
*
* Description : Set volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               label       Volume label.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
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
* Note(s)     : (1) The maximum label length is 11 characters.  If the label is shorter than this, it is
*                   padded with space characters (ASCII code point 20h) until it is 11 characters.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_VolLabelSet (FS_VOL    *p_vol,
                          CPU_CHAR  *label,
                          FS_ERR    *p_err)
{
    CPU_CHAR    label_buf[FS_FAT_VOL_LABEL_LEN + 1u];
    CPU_SIZE_T  label_len;


    label_len = Str_Len_N(label, FS_FAT_VOL_LABEL_LEN + 1u);
    if (label_len > FS_FAT_VOL_LABEL_LEN) {
       *p_err = FS_ERR_VOL_LABEL_TOO_LONG;
        return;
    }

    Mem_Set((void *)&label_buf[0], (CPU_CHAR)ASCII_CHAR_SPACE, sizeof(label_buf));
    label_buf[FS_FAT_VOL_LABEL_LEN] = (CPU_CHAR)ASCII_CHAR_NULL;

    Mem_Copy((void *)&label_buf[0], (void *)label, label_len);

    FS_FAT_SFN_LabelSet(p_vol,
                        label_buf,
                        p_err);
}
#endif


/*
*********************************************************************************************************
*                                          FS_FAT_VolOpen()
*
* Description : Detect file system on volume & initialize associated structures.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE              File system information gotten.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_INVALID_SYS       Invalid file system.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to get the FAT data from the FAT data pool.
*********************************************************************************************************
*/

void  FS_FAT_VolOpen (FS_VOL  *p_vol,
                      FS_ERR  *p_err)
{
    CPU_INT16U    boot_sig;
    FS_BUF       *p_buf;
    FS_FAT_DATA  *p_fat_data;
    void         *p_temp;
    CPU_INT08U   *p_temp_08;
    FS_ERR        err_tmp;
    LIB_ERR       pool_err;


    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_temp    = (void       *)p_buf->DataPtr;
    p_temp_08 = (CPU_INT08U *)p_temp;



                                                                /* ------------------- RD FIRST SEC ------------------- */
    FSVol_RdLockedEx(p_vol,
                     p_temp,
                     0u,
                     1u,
                     FS_VOL_SEC_TYPE_MGMT,
                     p_err);

    if (*p_err != FS_ERR_NONE) {
         FSBuf_Free(p_buf);
         return;
    }

                                                                /* Check signature in bytes 510:511 of sec.             */
    boot_sig = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + 510u));
    if (boot_sig != FS_FAT_BOOT_SIG) {
        FSBuf_Free(p_buf);
        FS_TRACE_DBG(("FS_FAT_VolOpen(): Invalid boot sec sig: 0x%04X != 0x%04X\r\n", boot_sig, FS_FAT_BOOT_SIG));
       *p_err = FS_ERR_VOL_INVALID_SYS;
        return;
    }

                                                                /* ------------------ ALLOC FAT DATA ------------------ */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock (see Note #1).                       */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

    p_fat_data = (FS_FAT_DATA *)Mem_PoolBlkGet(&FS_FAT_DataPool,/* Alloc FAT data.                                      */
                                                sizeof(FS_FAT_DATA),
                                               &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    FS_OS_Unlock();

    if (p_fat_data == (FS_FAT_DATA *)0) {
        FSBuf_Free(p_buf);
        return;
    }

    FS_FAT_DataClr(p_fat_data);                                 /* Clr FAT info.                                        */

                                                                /* --------- DETERMINE & APPLY FILE SYS PROPS --------- */
    FS_FAT_ChkBootSec(p_fat_data,
                      p_temp_08,
                      p_err);

    FSBuf_Free(p_buf);

    if (*p_err != FS_ERR_NONE) {                                /* If fat param's NOT valid ...                         */
        FS_OS_Lock(&err_tmp);
        Mem_PoolBlkFree(        &FS_FAT_DataPool,               /* ... free FAT data        ...                         */
                        (void *) p_fat_data,
                                &pool_err);
        FS_OS_Unlock();
        return;                                                 /* ... & rtn.                                           */
    }

                                                                /* ------------------ ALLOC FAT DATA ------------------ */
    p_vol->DataPtr = (void *)p_fat_data;                        /* Save FAT data in vol.                                */

#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_JournalInit(p_vol, p_err);                           /* Init journal info.                                   */

    if (*p_err != FS_ERR_NONE) {
        FS_OS_Lock(&err_tmp);
        Mem_PoolBlkFree(        &FS_FAT_DataPool,               /* ... & free FAT data.                                 */
                        (void *) p_fat_data,
                                &pool_err);
        FS_OS_Unlock();
        p_vol->DataPtr = (void *)0;
        return;
    }
#endif

                                                                /* ----------------- OUTPUT TRACE INFO ---------------- */
    FS_TRACE_INFO(("FS_FAT_VolOpen(): File system found: Type     : FAT%d \r\n",  p_fat_data->FAT_Type));
    FS_TRACE_INFO(("                                     Sec  size: %d B  \r\n",  p_fat_data->SecSize));
    FS_TRACE_INFO(("                                     Clus size: %d sec\r\n",  p_fat_data->ClusSize_sec));
    FS_TRACE_INFO(("                                     Vol  size: %d sec\r\n",  p_fat_data->VolSize));
    FS_TRACE_INFO(("                                     # Clus   : %d    \r\n", (p_fat_data->MaxClusNbr - FS_FAT_MIN_CLUS_NBR)));
    FS_TRACE_INFO(("                                     # FATs   : %d    \r\n",  p_fat_data->NbrFATs));

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FS_FAT_VolQuery()
*
* Description : Get info about file system on a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_info      Pointer to structure that will receive file system information.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE              File system information gotten.
*                               FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                               FS_ERR_DEV               Device access error.
*                               FS_ERR_VOL_INVALID_SYS   Invalid file system on volume.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_VolQuery (FS_VOL       *p_vol,
                       FS_SYS_INFO  *p_info,
                       FS_ERR       *p_err)
{
    FS_BUF       *p_buf;
    FS_FAT_DATA  *p_fat_data;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (p_fat_data == (FS_FAT_DATA *)0) {
       *p_err =   FS_ERR_VOL_INVALID_SYS;
        return;
    }

    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err =   FS_ERR_BUF_NONE_AVAIL;
        return;
    }


    FS_FAT_Query(p_vol,
                 p_buf,
                 p_info,
                 p_err);

    FSBuf_Free(p_buf);
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
*                                         FS_FAT_LowEntryUpdate()
*
* Description : Update directory entry information.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_buf           Pointer to temporary buffer.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
*               get_date        Indicates whether to get date/time to update entry :
*
*                                   DEF_YES, if entry should be updated with current date/time.
*                                   DEF_NO,  if entry should be updated with info's  date/time.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE    Directory entry updated.
*                                   FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowEntryUpdate (FS_VOL            *p_vol,
                             FS_BUF            *p_buf,
                             FS_FAT_FILE_DATA  *p_entry_data,
                             CPU_BOOLEAN        get_date,
                             FS_ERR            *p_err)
{
    CPU_INT08U      *p_dir_entry;
    CLK_DATE_TIME    stime;
    FS_FAT_DATE      date_val;
    FS_FAT_TIME      time_val;
    CPU_BOOLEAN      success;
#if (FS_FAT_CFG_JOURNAL_EN == DEF_ENABLED)
    FS_FAT_DATA     *p_fat_data;
    FS_FAT_DIR_POS   dir_end_pos;
#endif


   (void)p_vol;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */


                                                                /* ------------------- ENTER JOURNAL ------------------ */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
   p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
   if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
       dir_end_pos.SecNbr = p_entry_data->DirEndSec;
       dir_end_pos.SecPos = p_entry_data->DirEndSecPos;
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
              p_entry_data->DirEndSec,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    if (get_date == DEF_YES) {
        success = Clk_GetDateTime(&stime);                      /* Get date/time.                                       */
        if (success == DEF_YES) {
            date_val =  FS_FAT_DateFmt(&stime);
            time_val =  FS_FAT_TimeFmt(&stime);
        } else {
            date_val =  0u;
            time_val =  0u;
        }
    } else {
        date_val = p_entry_data->DateWr;
        time_val = p_entry_data->TimeWr;
    }


    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + p_entry_data->DirEndSecPos;
    MEM_VAL_SET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR),       p_entry_data->Attrib);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FSTCLUSHI),  p_entry_data->FileFirstClus >> DEF_INT_16_NBR_BITS);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FSTCLUSLO),  p_entry_data->FileFirstClus &  DEF_INT_16_MASK);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTTIME),    time_val);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTDATE),    date_val);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_LSTACCDATE), date_val);
    MEM_VAL_SET_INT32U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FILESIZE),   p_entry_data->FileSize);


    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr dir sec.                                          */
    if (*p_err != FS_ERR_NONE) {
         return;
    }
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_LowDirChkEmpty()
*
* Description : Check whether directory entry is empty.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_buf       Pointer to temporary buffer.
*               ----------  Argument validated by caller.
*
*               dir_clus    First cluster of directory.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    No device error.
*                               FS_ERR_DEV     Device access error.
*
* Return(s)   : DEF_YES, if directory is     empty.
*               DEF_NO,  if directory is NOT empty.
*
* Note(s)     : (1) The first two entries of an empty directory will be the "dot" entry & the "dot dot"
*                   entry, respectively.  Consequently, a non-empty directory will have at least three
*                   entries.
*
*                   (a) #### Check whether the first two entries ARE the "dot" & "dot dot" entries.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_BOOLEAN  FS_FAT_LowDirChkEmpty (FS_VOL           *p_vol,
                                    FS_BUF           *p_buf,
                                    FS_FAT_CLUS_NBR   dir_clus,
                                    FS_ERR           *p_err)
{
    FS_FAT_DIR_POS   dir_pos;
    FS_FAT_DIR_POS   dir_end_pos;
    CPU_INT08U       dir_entry_cnt;
    FS_FAT_DATA     *p_fat_data;
    CPU_BOOLEAN      valid;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ----------------- CHK CLUS ASSIGNED ---------------- */
    valid = FS_FAT_IS_VALID_CLUS(p_fat_data, dir_clus);
    if (valid == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return (DEF_YES);
    }

    dir_pos.SecNbr = FS_FAT_CLUS_TO_SEC(p_fat_data, dir_clus);
    dir_pos.SecPos = 0u;


                                                                /* ----------------- CHK FOR 3 ENTRIES ---------------- */
    dir_entry_cnt = 0u;
    while (dir_entry_cnt < 3u) {
        FS_FAT_FN_API_Active.NextDirEntryGet(             p_vol,
                                                          p_buf,
                                             (CPU_CHAR *) 0,
                                                         &dir_pos,
                                                         &dir_end_pos,
                                                          p_err);

        if (*p_err != FS_ERR_NONE) {
            if (*p_err == FS_ERR_EOF) {                             /* If no entry found ...                                */
                *p_err =  FS_ERR_NONE;
                 return (DEF_YES);                                  /* ... rtn empty.                                       */
            }
            return (DEF_NO);
        }

        dir_pos.SecNbr = dir_end_pos.SecNbr;
        dir_pos.SecPos = dir_end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;

        dir_entry_cnt++;
    }

    return (DEF_NO);
}
#endif


/*
*********************************************************************************************************
*                                     FS_FAT_LowDirFirstClusAdd()
*
* Description : Add first cluster to a directory.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
*               p_buf           Pointer to temporary buffer.
*               ----------      Argument validated by caller.
*
*               dir_parent_sec  Start sector of parent directory.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE        First cluster added to directory.
*
*                                                      ------------- RETURNED BY ClusAlloc() ------------
*                                   FS_ERR_DEV         Device access error.
*                                   FS_ERR_DEV_FULL    Device is full (no space could be allocated).
*
*                                                      ------- RETURNED BY FS_FAT_LowEntryUpdate() ------
*                                   FS_ERR_DEV         Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The sector clear operation will overwrite the data stored in the buffer, so the
*                   buffer MUST be flushed before this operation is performed.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LowDirFirstClusAdd (FS_VOL            *p_vol,
                                         FS_FAT_FILE_DATA  *p_entry_data,
                                         FS_BUF            *p_buf,
                                         FS_FAT_SEC_NBR     dir_parent_sec,
                                         FS_ERR            *p_err)
{
    CPU_INT08U       *p_dir_entry;
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   file_clus;
    FS_FAT_SEC_NBR    file_sec;
    FS_FAT_CLUS_NBR   dir_parent_clus;


                                                                /* ---------------- ALLOC 1ST DIR CLUS ---------------- */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
    file_clus  =  FS_FAT_ClusChainAlloc(p_vol,
                                        p_buf,
                                        0u,
                                        1u,
                                        p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    file_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, file_clus);

    FSBuf_Flush(p_buf, p_err);                                  /* Flush buf (see Note #1).                             */
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    FS_FAT_SecClr(p_vol,
                  p_buf->DataPtr,
                  file_sec,
                  p_fat_data->ClusSize_sec,
                  p_fat_data->SecSize,
                  FS_VOL_SEC_TYPE_FILE,
                  p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }


    p_entry_data->FileFirstClus = file_clus;
    p_entry_data->FileCurSec    = file_sec;


                                                                /* ------------------ WR 1ST DIR SEC ------------------ */
    FSBuf_Set(p_buf,
              file_sec,
              FS_VOL_SEC_TYPE_DIR,
              DEF_NO,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    Mem_Clr(p_buf->DataPtr, (CPU_SIZE_T)p_fat_data->SecSize);

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr;                 /* Make 'dot' entry.                                    */
    FS_FAT_SFN_DirEntryFmt((void       *) p_dir_entry,
                           (CPU_INT32U *)&FS_FAT_NameDot[0],
                                          DEF_TRUE,
                                          file_clus,
                                          DEF_NO,
                                          DEF_NO);

    p_dir_entry     +=  FS_FAT_SIZE_DIR_ENTRY;                  /* Make 'dot dot' entry.                                */
    dir_parent_clus  = (dir_parent_sec == p_fat_data->RootDirStart) ? 0u : FS_FAT_SEC_TO_CLUS(p_fat_data, dir_parent_sec);
    FS_FAT_SFN_DirEntryFmt((void        *) p_dir_entry,
                           (CPU_INT32U  *)&FS_FAT_NameDotDot[0],
                                           DEF_TRUE,
                                           dir_parent_clus,
                                           DEF_NO,
                                           DEF_NO);

    FSBuf_MarkDirty(p_buf, p_err);                              /* Write 1st sec of dir.                                 */
}
#endif


/*
*********************************************************************************************************
*                                    FS_FAT_LowFileFirstClusAdd()
*
* Description : Add first cluster to a file.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
*               p_buf           Pointer to temporary buffer.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE        First cluster added to file.
*
*                                                      ------------- RETURNED BY ClusAlloc() ------------
*                                   FS_ERR_DEV         Device access error.
*                                   FS_ERR_DEV_FULL    Device is full (no space could be allocated).
*
*                                                      ------- RETURNED BY FS_FAT_LowEntryUpdate() ------
*                                   FS_ERR_DEV         Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowFileFirstClusAdd (FS_VOL            *p_vol,
                                  FS_FAT_FILE_DATA  *p_entry_data,
                                  FS_BUF            *p_buf,
                                  FS_ERR            *p_err)
{
    FS_FAT_CLUS_NBR   clus;
    FS_FAT_SEC_NBR    sec;
    FS_FAT_DATA      *p_fat_data;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

                                                                /* Verify if dev is closed.                             */
    if (p_fat_data == (FS_FAT_DATA *)0) {
       *p_err = FS_ERR_DEV;
        return;
    }

                                                                /* -------------------- ALLOC CLUS -------------------- */
    clus = FS_FAT_ClusChainAlloc(p_vol,
                                 p_buf,
                                 0,
                                 1,
                                 p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }


                                                                /* ----------------- ASSIGN FILE INFO ----------------- */
    sec                         = FS_FAT_CLUS_TO_SEC(p_fat_data, clus);
    p_entry_data->FileFirstClus = clus;
    p_entry_data->FileCurSec    = sec;
    p_entry_data->FileCurSecPos = 0u;


                                                                /* ----------------- UPDATE DIR ENTRY ---------------- */
    FS_FAT_LowEntryUpdate(p_vol,
                          p_buf,
                          p_entry_data,
                          DEF_YES,
                          p_err);
}
#endif


/*
*********************************************************************************************************
*                                    FS_FAT_LowFileFirstClusGet()
*
* Description : Get first cluster address of a file.
*
* Argument(s) : p_vol           Pointer to volume.
*               -----           Argument validated by caller.
*
*               name_entry      Pointer to file name.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                     First cluster address obtained.
*                                   FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                                   FS_ERR_ENTRY_NOT_FILE           Entry not a file.
*                                   FS_ERR_VOL_INVALID_SEC_NBR      Cluster address invalid.
*
*                                                      ---------- RETURNED BY FS_FAT_DataSrch() ---------
*                                   FS_ERR_ENTRY_NOT_FOUND          File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND   Entry parent      NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR     Entry parent      NOT a directory.
*                                   FS_ERR_NAME_INVALID             Invalid file name or path.
*                                   FS_ERR_VOL_INVALID_SEC_NBR      Invalid sector number found in directory
*                                                                       entry.
*
* Return(s)   : Address of the first cluster of file.
*
* Note(s)     : (1) (a) Since a cluster may be assigned to a file, without any data being stored in that
*                       cluster, a directory entry for a file may hold a valid cluster number & a zero
*                       size.  However, no directory entry with a non-zero size may hold an invalid
*                       cluster number.
*
*                   (b) A directory MUST have been assigned a cluster.
*
*               (2) The cluster number in a directory is stored in two parts, a 16-bit high word & a
*                   16-bit low word.  According to [Ref 1], the high word SHOULD be 0 on FAT12/16 volumes.
*                   However, files created with major OSs do sometimes have non-zero high cluster words;
*                   to allow these files to be opened, the high cluster word (upper 16-bits of the cluster
*                   number) is discarded.
*********************************************************************************************************
*/

FS_FAT_CLUS_NBR  FS_FAT_LowFileFirstClusGet (FS_VOL    *p_vol,
                                             CPU_CHAR  *name_entry,
                                             FS_ERR    *p_err)
{
    CPU_INT08U        attrib;
    FS_FAT_DIR_POS    dir_end_pos;
    FS_FAT_SEC_NBR    dir_first_sec;
    FS_FAT_DIR_POS    dir_start_pos;
    CPU_BOOLEAN       is_dir;
    CPU_INT32U        file_size_sec;
    CPU_BOOLEAN       valid;
    CPU_BOOLEAN       invalid;
    FS_BUF           *p_buf;
    CPU_INT08U       *p_dir_entry;
    FS_FAT_DATA      *p_fat_data;
    FS_FAT_CLUS_NBR   file_first_clus;


                                                                /* --------------------- ROOT DIR --------------------- */
    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {
        FS_TRACE_DBG(("FS_FAT_LowFileFirstClusGet(): Root dir is not file.\r\n"));
       *p_err = FS_ERR_ENTRY_NOT_FILE;
        return (0u);
    }
                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return (0u);
    }

    FS_FAT_DataSrch( p_vol,                                     /* Find dir entry in data area.                         */
                     p_buf,
                     name_entry,
                    &dir_first_sec,
                    &dir_start_pos,
                    &dir_end_pos,
                     p_err);

    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return (0u);
    }

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
    attrib      = MEM_VAL_GET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR));
                                                                /* Rtn err if entry is invalid.                         */
    invalid = DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY | FS_FAT_DIRENT_ATTR_VOLUME_ID);
    if (invalid) {
        FSBuf_Free(p_buf);
        FS_TRACE_DBG(("FS_FAT_LowFileFirstClusGet(): Invalid entry: %s.\r\n", name_entry));
       *p_err = FS_ERR_ENTRY_NOT_FILE;
        return (0u);
    }
                                                                /* Rtn err if entry is vol ID.                          */
    invalid = DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID);
    if (invalid) {
        FSBuf_Free(p_buf);
        FS_TRACE_DBG(("FS_FAT_LowFileFirstClusGet(): Volume ID:     %s.\r\n", name_entry));
       *p_err = FS_ERR_ENTRY_NOT_FILE;
        return (0u);
    }

                                                                /* Sec & clus nbr of file's first sec (see Note #2).    */
    file_size_sec =  MEM_VAL_GET_INT32U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FILESIZE));
    is_dir        =  DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY);
    p_fat_data    = (FS_FAT_DATA *)p_vol->DataPtr;

    if ((file_size_sec != 0u) ||                                /* If file size is nonzero ...                          */
        (is_dir        == DEF_YES)) {                           /* ... or entry is dir     ...                          */
                                                                /* ... validate first clus.                             */
        file_first_clus =  FS_FAT_DIRENT_CLUS_NBR_GET(p_dir_entry);
        if ((p_fat_data->FAT_Type == 12u) ||
            (p_fat_data->FAT_Type == 16u)) {

            if (file_first_clus  > DEF_INT_16U_MAX_VAL) {
                FS_TRACE_DBG(("FS_FAT_LowFileFirstClusGet(): FstCluHI non-zero on FAT12/16 vol; discarding hi 16-bits.\r\n"));
                file_first_clus &= DEF_INT_16U_MAX_VAL;
            }

        }

        valid     =  FS_FAT_IS_VALID_CLUS(p_fat_data, file_first_clus);
        if (valid == DEF_NO) {
            FS_TRACE_DBG(("FS_FAT_LowFileFirstClusGet(): Invalid clus in dir entry: %d\n", file_first_clus));
           *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
            FSBuf_Free(p_buf);
            return (0u);
        }

    } else {
        file_first_clus = 0u;
    }


    FSBuf_Free(p_buf);

    return (file_first_clus);
}


/*
*********************************************************************************************************
*                                        FS_FAT_LowEntryFind()
*
* Description : Find entry for file.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to structure that will receive FAT entry data (see Note #3).
*               ----------      Argument validated by caller.
*
*               name_entry      Name of the entry.
*
*               mode            Access mode of entry.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                        Entry found.
*                                   FS_ERR_BUF_NONE_AVAIL              Buffer not available.
*                                   FS_ERR_DEV                         Device access error.
*                                   FS_ERR_DIR_FULL                    Directory is full (space could not be allocated).
*                                   FS_ERR_DEV_FULL                    Device is full (space could not be allocated).
*                                   FS_ERR_FILE_INVALID_ACCESS_MODE    Access mode is specified invalid.
*                                   FS_ERR_ENTRY_CORRUPT               File system entry is corrupt.
*                                   FS_ERR_ENTRY_EXISTS                File system entry exists.
*                                   FS_ERR_ENTRY_NOT_DIR               File system entry NOT a directory.
*                                   FS_ERR_ENTRY_NOT_EMPTY             File system entry NOT empty.
*                                   FS_ERR_ENTRY_NOT_FILE              File system entry NOT a file.
*                                   FS_ERR_ENTRY_NOT_FOUND             File system entry NOT found
*                                   FS_ERR_ENTRY_PARENT_NOT_FOUND      Entry parent NOT found.
*                                   FS_ERR_ENTRY_PARENT_NOT_DIR        Entry parent NOT a directory.
*                                   FS_ERR_ENTRY_RD_ONLY               File system entry marked read-only.
*                                   FS_ERR_NAME_INVALID                Invalid file name or path.
*
* Return(s)   : none.
*
* Note(s)     : (1) The mode flags should be the logical OR of one or more flags :
*
*                       FS_FAT_MODE_RD             Entry opened for reads.
*                       FS_FAT_MODE_WR             Entry opened for writes.
*                       FS_FAT_MODE_CREATE         Entry will be created, if necessary.
*                       FS_FAT_MODE_TRUNCATE       Entry length will be truncated to 0.
*                       FS_FAT_MODE_APPEND         All writes will be performed at EOF.
*                       FS_FAT_MODE_MUST_CREATE    Entry will be opened if & only if it does not already
*                                                      exist.
*                       FS_FAT_MODE_DEL            Entry will be deleted.
*                       FS_FAT_MODE_DIR            Entry may be a directory.
*                       FS_FAT_MODE_FILE           Entry may be a file.
*
*                   (a) Several modes are invalid :
*                       (1) Set   FS_FAT_MODE_TRUNCATE & clear FS_FAT_MODE_WR;
*                       (2) Set   FS_FAT_MODE_EXCL     & clear FS_FAT_MODE_CREATE.
*                       (3) Clear FS_FAT_MODE_RD       & clear FS_FAT_MODE_WR.
*                       (4) Clear FS_FAT_MODE_DIR      & clear FS_FAT_MODE_FILE.
*                       (5) Set   FS_FAT_MODE_DIR      & set   FS_FAT_MODE_TRUNCATE.
*                       (6) Set   FS_FAT_MODE_DEL      & clear FS_FAT_MODE_WR;
*                       (7) Set   FS_FAT_MODE_CREATE   & set   FS_FAT_MODE_FILE & set FS_FAT_MODE_DIR;
*
*               (2) (a) Since a cluster may be assigned to a file, without any data being stored in that
*                       cluster, a directory entry for a file may hold a valid cluster number & a zero
*                       size.  However, no directory entry with a non-zero size may hold an invalid
*                       cluster number.
*
*                   (b) A directory MUST have been assigned a cluster.
*
*               (3) The cluster number in a directory is stored in two parts, a 16-bit high word & a
*                   16-bit low word.  According to [Ref 1], the high word SHOULD be 0 on FAT12/16 volumes.
*                   However, files created with major OSs do sometimes have non-zero high cluster words;
*                   to allow these files to be opened, the high cluster word (upper 16-bits of the cluster
*                   number) is discarded.
*
*               (4) The FAT file data will be populated fully only if the entry if found/created.   If
*                   the entry does not exist, but the parent does, the 'DirFirstSec' member will be
*                   assigned the first sector of the would-be parent directory.
*
*               (5) An invalid cluster linked to the directory entry is tolerated if and only if the
*                   entry is going to be deleted.
*********************************************************************************************************
*/

void  FS_FAT_LowEntryFind (FS_VOL            *p_vol,
                           FS_FAT_FILE_DATA  *p_entry_data,
                           CPU_CHAR          *name_entry,
                           FS_FLAGS           mode,
                           FS_ERR            *p_err)
{
    CPU_INT08U        attrib;
    FS_FAT_DIR_POS    dir_end_pos;
    FS_FAT_SEC_NBR    dir_first_sec;
    FS_FAT_DIR_POS    dir_start_pos;
    CPU_BOOLEAN       is_dir;
    FS_FAT_CLUS_NBR   file_clus;
    FS_FAT_SEC_NBR    file_sec;
    CPU_INT32U        file_size_sec;
    CPU_BOOLEAN       valid;
    FS_BUF           *p_buf;
    CPU_INT08U       *p_dir_entry;
    FS_FAT_DATA      *p_fat_data;
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    CPU_BOOLEAN       empty;
    CPU_CHAR         *p_slash;
#endif


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE MODE ------------------ */
                                                                /* See Note #1a3.                                       */
    if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_RD | FS_FAT_MODE_WR) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return;
    }
                                                                /* See Note #1a4.                                       */
    if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_DIR | FS_FAT_MODE_FILE) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return;
    }
                                                                /* See Note #1a5.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_DIR | FS_FAT_MODE_TRUNCATE) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return;
    }
                                                                /* See Note #1a6.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_DEL) == DEF_YES) {
        if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_WR) == DEF_YES) {
           *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
            return;
        }
    }
                                                                /* See Note #1a6.                                       */
    if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_CREATE | FS_FAT_MODE_DIR | FS_FAT_MODE_FILE) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return;
    }
#endif

#if (FS_CFG_RD_ONLY_EN == DEF_ENABLED)
    if (DEF_BIT_IS_SET_ANY(mode, FS_FAT_MODE_CREATE | FS_FAT_MODE_DEL | FS_FAT_MODE_TRUNCATE | FS_FAT_MODE_WR) == DEF_YES) {
       *p_err = FS_ERR_FILE_INVALID_ACCESS_MODE;
        return;
    }
#endif


                                                                /* --------------------- ROOT DIR --------------------- */
    FS_FAT_FileDataClr(p_entry_data);
    p_entry_data->Mode = mode;

    if (name_entry[0] == (CPU_CHAR)ASCII_CHAR_NULL) {
        if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_DIR) == DEF_YES) {
            if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_MUST_CREATE) == DEF_NO) {
                p_entry_data->Attrib        =  FS_FAT_DIRENT_ATTR_DIRECTORY;
                p_fat_data                  = (FS_FAT_DATA *)p_vol->DataPtr;
                p_entry_data->FileCurSec    =  p_fat_data->RootDirStart;
                p_entry_data->FileFirstClus = (p_fat_data->RootDirStart < p_fat_data->DataStart) ? 0u : FS_FAT_SEC_TO_CLUS(p_fat_data, p_fat_data->RootDirStart);
               *p_err = FS_ERR_NONE;
            } else {
                FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Cannot create root dir.\r\n"));
               *p_err = FS_ERR_ENTRY_EXISTS;
            }
        } else {
            FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Root dir is not file.\r\n"));
           *p_err = FS_ERR_ENTRY_NOT_FILE;
        }
        return;
    }
                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    FS_FAT_DataSrch( p_vol,                                     /* Find dir entry in data area.                         */
                     p_buf,
                     name_entry,
                    &dir_first_sec,
                    &dir_start_pos,
                    &dir_end_pos,
                     p_err);


    switch (*p_err) {                                           /* Handle dir entry OR err.                             */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        case FS_ERR_ENTRY_NOT_FOUND:
                                                                /* Do nothing & rtn err if file MUST exist.             */
             p_entry_data->DirFirstSec = dir_first_sec;
             if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_CREATE | FS_FAT_MODE_MUST_CREATE) == DEF_YES) {
                 FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Entry not found: %s.\r\n", name_entry));
                 FSBuf_Free(p_buf);
                 return;
             }


                                                                /* -------------------- CREATE FILE ------------------- */
                                                                /* If file MAY be created, create file.                 */
                                                                /* Make dir entry.                                      */
             p_slash = FS_FAT_Char_LastPathSep(name_entry);
             if (p_slash != (CPU_CHAR *)0) {
                 name_entry = p_slash + 1u;
             }

             is_dir = DEF_BIT_IS_SET(mode, FS_FAT_MODE_DIR);
             FS_FAT_LowEntryCreate(p_vol,
                                   p_buf,
                                   p_entry_data,
                                   name_entry,
                                   dir_first_sec,
                                   is_dir,
                                   p_err);
             break;
#endif



        case FS_ERR_NONE:                                       /* ------------------- GET FILE INFO ------------------ */
             if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_MUST_CREATE) == DEF_YES) {
                 FSBuf_Free(p_buf);
                 FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Entry should not exist: %s.\r\n", name_entry));
                *p_err = FS_ERR_ENTRY_EXISTS;
                 return;
             }

             p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
             attrib      = MEM_VAL_GET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR));
                                                                /* Rtn err if entry is invalid.                         */
             if (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY | FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                 FSBuf_Free(p_buf);
                 FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Invalid entry: %s.\r\n", name_entry));
                *p_err = FS_ERR_ENTRY_NOT_FILE;
                 return;
             }

                                                                /* Rtn err if entry is vol ID.                          */
             if (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                 FSBuf_Free(p_buf);
                 FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Volume ID: %s.\r\n", name_entry));
                *p_err = FS_ERR_ENTRY_NOT_FILE;
                 return;
             }

             if (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
                                                                /* Rtn err if entry is dir but mode is file.            */
                 if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_DIR)  == DEF_YES) {
                     FSBuf_Free(p_buf);
                     FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Dir NOT file: %s.\r\n", name_entry));
                    *p_err = FS_ERR_ENTRY_NOT_FILE;
                     return;
                 }
             } else {
                                                                /* Rtn err if entry is file but mode is dir.            */
                 if (DEF_BIT_IS_CLR(mode, FS_FAT_MODE_FILE)  == DEF_YES) {
                     FSBuf_Free(p_buf);
                     FS_TRACE_DBG(("FS_FAT_LowEntryFind(): File NOT dir: %s.\r\n", name_entry));
                    *p_err = FS_ERR_ENTRY_NOT_DIR;
                     return;
                 }
             }

                                                                /* Rtn err if entry is rd-only but file mode is wr.     */
             if ((DEF_BIT_IS_SET(mode,   FS_FAT_MODE_WR)               == DEF_YES) &&
                 (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_READ_ONLY) == DEF_YES) &&
                 (DEF_BIT_IS_CLR(mode,   FS_FAT_MODE_DEL)              == DEF_YES)) {
                 FSBuf_Free(p_buf);
                 FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Entry is read only: %s.\r\n", name_entry));
                *p_err = FS_ERR_ENTRY_RD_ONLY;
                 return;
             }

                                                                /* Sec & clus nbr of file's dir sec.                    */
             p_entry_data->DirFirstSec    = dir_first_sec;
             p_entry_data->DirStartSec    = dir_start_pos.SecNbr;
             p_entry_data->DirStartSecPos = dir_start_pos.SecPos;
             p_entry_data->DirEndSec      = dir_end_pos.SecNbr;
             p_entry_data->DirEndSecPos   = dir_end_pos.SecPos;


             FS_FAT_FileDataInit(p_entry_data,
                                 p_dir_entry);


                                                                /* Sec & clus nbr of file's first sec (see Note #3).    */
             file_size_sec =  p_entry_data->FileSize;
             is_dir        =  DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY);
             p_fat_data    = (FS_FAT_DATA *)p_vol->DataPtr;

                                                                /* See Note #2.                                         */
             file_clus =  FS_FAT_DIRENT_CLUS_NBR_GET(p_dir_entry);
             if ((file_clus     != 0u) ||                       /* If file first clus is non zero ...                   */
                 (file_size_sec != 0u) ||                       /* ... or file has non-zero size ..                     */
                 (is_dir        == DEF_YES)) {                  /* ... or entry is dir ...                              */
                                                                /* ... validate first clus.                             */
                 if ((p_fat_data->FAT_Type == 12u) ||
                     (p_fat_data->FAT_Type == 16u)) {
                     if (file_clus  > DEF_INT_16U_MAX_VAL) {    /* See Note #3.                                         */
                         FS_TRACE_DBG(("FS_FAT_LowEntryFind(): FstCluHI non-zero on FAT12/16 vol; discarding hi 16-bits.\r\n"));
                         file_clus &= DEF_INT_16U_MAX_VAL;
                     }
                 }
                 valid     =  FS_FAT_IS_VALID_CLUS(p_fat_data, file_clus);
                 if (valid == DEF_NO) {
                     FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Invalid clus in dir entry: %d\n", file_clus));
                     if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_DEL) == DEF_NO) {     /* See Note #5.                         */
                        *p_err = FS_ERR_VOL_INVALID_SEC_NBR;
                         FSBuf_Free(p_buf);
                         return;
                     }
                 }
                 p_entry_data->FileFirstClus = file_clus;
                 file_sec                    = FS_FAT_CLUS_TO_SEC(p_fat_data, file_clus);
                 p_entry_data->FileCurSec    = file_sec;

             } else {
                 p_entry_data->FileFirstClus = 0u;
                 p_entry_data->FileCurSec    = 0u;
             }


                                                                /* -------------------- DELETE FILE ------------------- */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_DEL) == DEF_YES) {                /* Delete file (see Note #1).       */
                                                                                    /* Chk whether dir is empty.        */
                 is_dir = DEF_BIT_IS_SET(p_entry_data->Attrib, FS_FAT_DIRENT_ATTR_DIRECTORY);
                 if (is_dir == DEF_YES) {
                     empty = FS_FAT_LowDirChkEmpty(p_vol,
                                                   p_buf,
                                                   p_entry_data->FileFirstClus,
                                                   p_err);
                     if (*p_err != FS_ERR_NONE) {
                          FSBuf_Free(p_buf);
                          return;
                     }
                     if (empty == DEF_NO) {
                          FS_TRACE_DBG(("FS_FAT_LowEntryFind(): Cannot delete non-empty directory.\r\n"));
                         *p_err = FS_ERR_ENTRY_NOT_EMPTY;
                          FSBuf_Free(p_buf);
                          return;
                     }
                 }

                 FS_FAT_LowEntryDel(p_vol,
                                    p_buf,
                                    p_entry_data,
                                    p_err);
                 if (*p_err != FS_ERR_NONE) {
                      FSBuf_Free(p_buf);
                      return;
                 }


                                                                /* ------------------- TRUNCATE FILE ------------------ */
             } else  {
                 if (DEF_BIT_IS_SET(mode, FS_FAT_MODE_TRUNCATE) == DEF_YES) {       /* Truncate file (see Note #1).     */
                     if (p_entry_data->FileSize > 0u) {
                         FS_FAT_LowEntryTruncate(p_vol,
                                                 p_buf,
                                                 p_entry_data,
                                                 0u,
                                                 p_err);
                         if (*p_err != FS_ERR_NONE) {
                              FSBuf_Free(p_buf);
                              return;
                         }
                     }
                 }
             }
#endif

            *p_err = FS_ERR_NONE;
             break;

        default:
             break;
    }

    if(*p_err == FS_ERR_NONE) {
        FSBuf_Flush(p_buf, p_err);
    }

    FSBuf_Free(p_buf);
}


/*
*********************************************************************************************************
*                                       FS_FAT_LowEntryCreate()
*
* Description : Create entry (low-level).
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_buf           Pointer to temporary buffer.
*               ----------  Argument validated by caller.
*
*               p_entry_data    Pointer to variable that will receive FAT file information.
*               ----------  Argument validated by caller.
*
*               name_entry      Name of the entry.
*               ----------  Argument validated by caller.
*
*               dir_first_sec   First sector of parent directory.
*
*               is_dir          Indicates whether directory entry is entry for directory :
*
*                                   DEF_YES, entry is for directory.
*                                   DEF_NO,  entry is for file.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                                   FS_ERR_NONE                 Entry created.
*
*                                                               --- RETURNED BY FS_FAT_LowDirFirstClusAdd() ---
*                                   FS_ERR_DEV                  Device access error.
*                                   FS_ERR_DEV_FULL             Device is full (space could not be allocated).
*
*                                                               -------- RETURNED BY DirEntryCreate() ---------
*                                   FS_ERR_DIR_FULL             Directory is full (space could not be allocated).
*                                   FS_ERR_ENTRY_CORRUPT        File system entry is corrupt.
*                                   FS_ERR_NAME_INVALID         LFN name invalid.
*                                   FS_ERR_SYS_SFN_NOT_AVAIL    No SFN available.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowEntryCreate (FS_VOL            *p_vol,
                             FS_BUF            *p_buf,
                             FS_FAT_FILE_DATA  *p_entry_data,
                             CPU_CHAR          *name_entry,
                             FS_FAT_SEC_NBR     dir_first_sec,
                             CPU_BOOLEAN        is_dir,
                             FS_ERR            *p_err)
{
    CPU_INT08U       *p_dir_entry;
    FS_FAT_CLUS_NBR   file_clus;
    FS_FAT_DIR_POS    dir_start_pos;
    FS_FAT_DIR_POS    dir_end_pos;


                                                                /* ------------------ ASSIGN 1st CLUS ----------------- */
                                                                /* If dir, make 1st dir clus.                           */
    if (is_dir == DEF_YES) {
        FS_FAT_LowDirFirstClusAdd(p_vol,
                                  p_entry_data,
                                  p_buf,
                                  dir_first_sec,
                                  p_err);

        if (*p_err != FS_ERR_NONE) {
             return;
        }

        file_clus = p_entry_data->FileFirstClus;


    } else {                                                    /* Otherwise zero first clus.                           */
        file_clus = 0u;
    }




                                                                /* ----------------- CREATE DIR ENTRY ----------------- */
    dir_start_pos.SecNbr = dir_first_sec;
    dir_start_pos.SecPos = 0u;
    FS_FAT_FN_API_Active.DirEntryCreate( p_vol,
                                         p_buf,
                                         name_entry,
                                         is_dir,
                                         file_clus,
                                        &dir_start_pos,
                                        &dir_end_pos,
                                         p_err);

     if (*p_err != FS_ERR_NONE) {
          return;
     }



                                                                /* ------------------- GET FILE INFO ------------------ */
                                                                /* Populate file info.                                  */
     p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
     FS_FAT_FileDataInit(p_entry_data,
                         p_dir_entry);

     p_entry_data->DirFirstSec    = dir_first_sec;
     p_entry_data->DirStartSec    = dir_start_pos.SecNbr;
     p_entry_data->DirStartSecPos = dir_start_pos.SecPos;
     p_entry_data->DirEndSec      = dir_end_pos.SecNbr;
     p_entry_data->DirEndSecPos   = dir_end_pos.SecPos;

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_LOG)
     if (is_dir == DEF_YES) {
         FS_TRACE_LOG(("FS_FAT_LowEntryCreate(): Created directory %s\r\n", name_entry));
         FS_TRACE_LOG(("                         1st clus %08X\r\n",        file_clus));
     } else {
         FS_TRACE_LOG(("FS_FAT_LowEntryCreate(): Created file %s\r\n",      name_entry));
     }
#endif

     FS_TRACE_LOG(("                         Dir sec  %08X\r\n",     p_entry_data->DirEndSec));
     FS_TRACE_LOG(("                         Dir pos      %04X\r\n", p_entry_data->DirEndSecPos));


}
#endif


/*
*********************************************************************************************************
*                                        FS_FAT_LowEntryDel()
*
* Description : Delete entry (low-level).
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_buf           Pointer to temporary buffer.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT file information.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE             Entry deleted.
*                                   FS_ERR_DEV              Device access error.
*                                   FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowEntryDel (FS_VOL            *p_vol,
                          FS_BUF            *p_buf,
                          FS_FAT_FILE_DATA  *p_entry_data,
                          FS_ERR            *p_err)
{
    FS_FAT_DIR_POS     dir_end_pos;
    FS_FAT_DIR_POS     dir_start_pos;
    FS_FAT_DATA       *p_fat_data;
    CPU_BOOLEAN        valid;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ------------------- DEL DIR ENTRY ------------------ */
    dir_start_pos.SecNbr = p_entry_data->DirStartSec;
    dir_start_pos.SecPos = p_entry_data->DirStartSecPos;

    dir_end_pos.SecNbr   = p_entry_data->DirEndSec;
    dir_end_pos.SecPos   = p_entry_data->DirEndSecPos;

    FS_FAT_FN_API_Active.DirEntryDel(p_vol,
                                     p_buf,
                                    &dir_start_pos,
                                    &dir_end_pos,
                                     p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ------------------- DEL CLUS CHAIN ----------------- */
    valid = FS_FAT_IS_VALID_CLUS(p_fat_data, p_entry_data->FileFirstClus);
    if (valid == DEF_YES) {
        (void)FS_FAT_ClusChainDel(p_vol,
                                  p_buf,
                                  p_entry_data->FileFirstClus,
                                  DEF_YES,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
             return;
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_LowEntryRename()
*
* Description : Rename entry (low-level).
*
* Argument(s) : p_vol               Pointer to volume.
*               ----------          Argument validated by caller.
*
*               p_buf               Pointer to temporary buffer.
*               ----------          Argument validated by caller.
*
*               p_entry_data_old    Pointer to FAT entry information for the old entry.
*               ----------          Argument validated by caller.
*
*               p_entry_data_new    Pointer to FAT entry information for the new entry.
*               ----------          Argument validated by caller.
*
*               exists              Indicates whether entry exists with new name :
*
*                                       DEF_NO,  entry with new name does NOT exist.
*                                       DEF_YES, entry with new name does     exist.
*
*               name_entry_new      New name of the entry.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                 Entry renamed.
*                                       FS_ERR_DEV                  Device access error.
*                                       FS_ERR_DEV_FULL             Device is full (space could not be allocated).
*                                       FS_ERR_DIR_FULL             Directory is full (space could not be allocated).
*                                       FS_ERR_ENTRIES_TYPE_DIFF    Paths do not both specify files
*                                                                       OR directories.
*
* Return(s)   : none.
*
* Note(s)     : (1) If entry is a directory, a file with the directory name is created & then updated
*                   with the attributes of a directory.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowEntryRename (FS_VOL            *p_vol,
                             FS_BUF            *p_buf,
                             FS_FAT_FILE_DATA  *p_entry_data_old,
                             FS_FAT_FILE_DATA  *p_entry_data_new,
                             CPU_BOOLEAN        exists,
                             CPU_CHAR          *name_entry_new,
                             FS_ERR            *p_err)
{
    FS_FAT_DIR_POS   dir_end_old;
    FS_FAT_SEC_NBR   dir_first_sec;
    FS_FAT_DIR_POS   dir_start_old;
    FS_FAT_DIR_POS   dir_start_new;
    FS_FAT_DIR_POS   dir_end_new;
    FS_FAT_CLUS_NBR  target_entry_first_clus;
    FS_FAT_DATA     *p_fat_data;


    p_fat_data = (FS_FAT_DATA  *)p_vol->DataPtr;

                                                                /* ------------ REM TARGET ENTRY IF NEEDED ------------ */
    if (exists == DEF_YES) {
        target_entry_first_clus = p_entry_data_new->FileFirstClus;

        dir_start_new.SecNbr    = p_entry_data_new->DirStartSec;
        dir_start_new.SecPos    = p_entry_data_new->DirStartSecPos;
        dir_end_new.SecNbr      = p_entry_data_new->DirEndSec;
        dir_end_new.SecPos      = p_entry_data_new->DirEndSecPos;

        FS_FAT_FN_API_Active.DirEntryDel(p_vol,
                                         p_buf,
                                        &dir_start_new,
                                        &dir_end_new,
                                         p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    } else {
        target_entry_first_clus = 0u;
    }


                                                                /* ----------------- CREATE NEW ENTRY ----------------- */
    dir_first_sec = p_entry_data_new->DirFirstSec;

    FS_FAT_LowEntryCreate(p_vol,
                          p_buf,
                          p_entry_data_new,
                          name_entry_new,
                          dir_first_sec,
                          DEF_NO,
                          p_err);
    if (*p_err != FS_ERR_NONE) {                                /* If entry could not be created ...                    */
        return;                                                 /* ... rtn err.                                         */
    }


                                                                /* ----------------- UPDATE NEW ENTRY ----------------- */
    p_entry_data_new->Attrib        = p_entry_data_old->Attrib;
    p_entry_data_new->FileFirstClus = p_entry_data_old->FileFirstClus;
    p_entry_data_new->FileSize      = p_entry_data_old->FileSize;
    p_entry_data_new->DateCreate    = p_entry_data_old->DateCreate;
    p_entry_data_new->TimeCreate    = p_entry_data_old->TimeCreate;
    p_entry_data_new->DateWr        = p_entry_data_old->DateWr;
    p_entry_data_new->TimeWr        = p_entry_data_old->TimeWr;
    p_entry_data_new->DateAccess    = p_entry_data_old->DateAccess;

    FS_FAT_LowEntryUpdate(p_vol,
                          p_buf,
                          p_entry_data_new,
                          DEF_NO,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* ------------------ REM OLD ENTRY ------------------- */
    dir_start_old.SecNbr = p_entry_data_old->DirStartSec;
    dir_start_old.SecPos = p_entry_data_old->DirStartSecPos;

    dir_end_old.SecNbr   = p_entry_data_old->DirEndSec;
    dir_end_old.SecPos   = p_entry_data_old->DirEndSecPos;
    FS_FAT_FN_API_Active.DirEntryDel( p_vol,
                                      p_buf,
                                     &dir_start_old,
                                     &dir_end_old,
                                      p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* --------- DEL TARGET CLUS CHAIN IF NEEDED ---------- */
                                                                /* Clus chain del must be last operation (See Note #?)  */
    if (exists == DEF_YES) {
        if (FS_FAT_IS_VALID_CLUS(p_fat_data, target_entry_first_clus) == DEF_YES) {
            (void)FS_FAT_ClusChainDel(p_vol,
                                      p_buf,
                                      target_entry_first_clus,
                                      DEF_YES,
                                      p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_LowEntryTruncate()
*
* Description : Truncate entry (low-level).
*
* Argument(s) : p_vol                   Pointer to volume.
*               ----------              Argument validated by caller.
*
*               p_buf                   Pointer to temporary buffer.
*               ----------              Argument validated by caller.
*
*               p_entry_data            Pointer to FAT entry information.
*               ----------              Argument validated by caller.
*
*               file_size_truncated     Size of the file, in octets, after truncation.
*
*               p_err                   Pointer to variable that will receive the return error code from this function :
*               ----------              Argument validated by caller.
*
*                                           FS_ERR_NONE              Entry truncated.
*                                           FS_ERR_DEV               Device access error.
*                                           FS_ERR_ENTRY_CORRUPT     File system entry is corrupt.
*                                           FS_ERR_VOL_INVALID_OP    Invalid operation (see Note #2).
*
* Return(s)   : none.
*
* Note(s)     : (1) The file must NEVER be called on a directory.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_LowEntryTruncate (FS_VOL            *p_vol,
                               FS_BUF            *p_buf,
                               FS_FAT_FILE_DATA  *p_entry_data,
                               FS_FAT_FILE_SIZE   file_size_truncated,
                               FS_ERR            *p_err)
{
    CPU_BOOLEAN        del_first;
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_CLUS_NBR    file_first_clus;
    FS_FAT_FILE_SIZE   file_size;
    FS_FAT_FILE_SIZE   file_size_clus;
    FS_FAT_FILE_SIZE   file_size_truncated_clus;
    CPU_BOOLEAN        valid;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ----------------- UPDATE DIR ENTRY ----------------- */
    file_size       = p_entry_data->FileSize;                   /* Save file size & ...                                 */
    file_first_clus = p_entry_data->FileFirstClus;              /* ... first file clus.                                 */

                                                                /* Update dir entry's file size & first clus.           */
    p_entry_data->FileSize = file_size_truncated;
    if (file_size_truncated == 0u) {                            /* If len is zero ...                                   */
        p_entry_data->FileFirstClus = 0u;                       /* ... then unlink first clus.                          */
    }
    FS_FAT_LowEntryUpdate(p_vol,                                /* Wr new file size & first file clus to dir entry.     */
                          p_buf,
                          p_entry_data,
                          DEF_YES,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
         return;
    }



                                                                /* ------------------- DEL CLUS CHAIN ----------------- */
    valid = FS_FAT_IS_VALID_CLUS(p_fat_data, file_first_clus);
    if (valid == DEF_YES) {
        file_size_truncated_clus = (file_size_truncated  > 0u) ? (FS_UTIL_DIV_PWR2(file_size_truncated - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);
        file_size_clus           = (file_size            > 0u) ? (FS_UTIL_DIV_PWR2(file_size           - 1u, p_fat_data->ClusSizeLog2_octet) + 1u) : (0u);
        del_first                = (file_size_truncated == 0u) ?  DEF_YES : DEF_NO;

        if ((file_size_truncated_clus != file_size_clus) ||     /* If the nbr clus will chng          ...               */
            (del_first                == DEF_YES)) {            /* ... or first clus must be del'd    ...               */

            if (file_size_truncated != 0u) {
                                                                /* ... find last clus of trunc'd file ...               */
                file_first_clus = FS_FAT_ClusChainFollow(p_vol,
                                                         p_buf,
                                                         file_first_clus,
                                                         file_size_truncated_clus - 1u,
                                                         DEF_NULL,
                                                         p_err);

                if (*p_err != FS_ERR_NONE) {
                    if ((*p_err == FS_ERR_SYS_CLUS_CHAIN_END_EARLY) ||
                        (*p_err == FS_ERR_SYS_CLUS_INVALID)) {
                         *p_err =  FS_ERR_ENTRY_CORRUPT;
                    }
                    return;
                }
            }

            (void)FS_FAT_ClusChainDel(p_vol,                    /* ... & del rem clus chain.                            */
                                      p_buf,
                                      file_first_clus,
                                      del_first,
                                      p_err);
            if (*p_err != FS_ERR_NONE) {
                 return;
            }
        }
    }
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            UTILITY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            FS_FAT_DateFmt()
*
* Description : Format date for directory entry.
*
* Argument(s) : p_time      Pointer to date/time structure.
*               ----------  Argument validated by caller.
*
* Return(s)   : Formatted date for directory entry.
*
* Note(s)     : (1) The FAT date stamp encodes the date as follows :
*
*                   (a) Bits 0 -  4 : Day of month,     1-  31 inclusive.
*                   (b) Bits 5 -  8 : Month of year,    1-  12 inclusive.
*                   (c) Bits 9 - 15 : Years,         1980-2107 inclusive.
*
*               (2) According to [Ref #1], unsupported date & time fields should be set to 0.
*                   Consequently, if any illegal parameters are passed, the date is returned as 0.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_DATE  FS_FAT_DateFmt (CLK_DATE_TIME  *p_time)
{
    CPU_INT16U   date_val;
    CPU_INT16U   day;
    CPU_INT16U   month;
    CPU_INT16U   year;
    CPU_BOOLEAN  valid;


    valid = Clk_IsDateTimeValid(p_time);

    if (valid == DEF_NO) {
        return (0u);
    }

    day   = (CPU_INT16U)p_time->Day;
    month = (CPU_INT16U)p_time->Month;
    year  = (CPU_INT16U)p_time->Yr;

    if ((year  >= 1980u) && (year  <= 2107u)) {
        date_val = ((CPU_INT16U)((day         ) << 0)
                 +  (CPU_INT16U)((month       ) << 5)
                 +  (CPU_INT16U)((year - 1980u) << 9));
    } else {
        date_val = 0u;
    }

    return (date_val);
}
#endif


/*
*********************************************************************************************************
*                                            FS_FAT_TimeFmt()
*
* Description : Format time for directory entry.
*
* Argument(s) : p_time      Pointer to date/time structure.
*               ----------  Argument validated by caller.
*
* Return(s)   : Formatted time for directory entry.
*
* Note(s)     : (1) The FAT time stamp encodes the date as follows :
*
*                   (a) Bits  0 -  4 : 2-second count, 0-29 inclusive (0-58 seconds).
*                   (b) Bits  5 - 10 : Minutes,        0-59 inclusive.
*                   (c) Bits 11 - 15 : Hours,          0-23 inclusive.
*
*               (2) According to Microsoft ('FS_FAT_VolOpen() Ref #1'), unsupported date & time fields
*                   should be set to 0. Consequently, if any illegal parameters are passed, the time is
*                   returned as 0.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
FS_FAT_TIME  FS_FAT_TimeFmt (CLK_DATE_TIME  *p_time)
{
    CPU_INT16U  hour;
    CPU_INT16U  min;
    CPU_INT16U  sec;
    CPU_INT16U  time_val;


    sec  = (CPU_INT16U)p_time->Sec;
    min  = (CPU_INT16U)p_time->Min;
    hour = (CPU_INT16U)p_time->Hr;

    if ((sec < 61u) && (min < 60u) && (hour < 24u)) {
        time_val = (CPU_INT16U)((CPU_INT16U)(sec  >>  1)
                 +              (CPU_INT16U)(min  <<  5)
                 +              (CPU_INT16U)(hour << 11));
    } else {
        time_val = 0u;
    }

    return (time_val);
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_DateTimeParse()
*
* Description : Parse date & time for directory entry.
*
* Argument(s) : p_ts        Pointer to timestamp.
*               ----------  Argument validated by caller.
*
*               date        FAT-formatted date.
*
*               time        FAT-formatted time.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_DateFmt() Note #1' & 'FS_FAT_TimeFmt() Note #1'.
*
*               (2) If the date is passed as 0, or if the date is an illegal value, then the date fields
*                   of 'p_time' will be cleared.
*
*               (3) If the time is passed as 0, or if the time is an illegal value, then the time fields
*                   of 'stime' will be cleared.
*********************************************************************************************************
*/

void  FS_FAT_DateTimeParse (CLK_TS_SEC   *p_ts,
                            FS_FAT_DATE   date,
                            FS_FAT_TIME   time)
{
    CLK_DATE_TIME  stime;
    CPU_INT08U     day;
    CPU_INT08U     hour;
    CPU_INT08U     min;
    CPU_INT08U     month;
    CPU_INT08U     sec;
    CPU_INT16U     year;
    CPU_BOOLEAN    valid;


    if (date != 0u) {                                           /* See Note #2.                                         */
        day   = FS_FAT_DIRENT_DATE_GET_DAY(date);
        month = FS_FAT_DIRENT_DATE_GET_MONTH(date);
        year  = FS_FAT_DIRENT_DATE_GET_YEAR(date);

        valid = Clk_UnixDateTimeMake(&stime,
                                      year,
                                      month,
                                      day,
                                      0u,
                                      0u,
                                      0u,
                                      0u);
    } else {
        valid  = DEF_NO;
    }

    if (valid == DEF_NO) {
       *p_ts   = FS_TIME_TS_INVALID;
        return;
    }

    sec  = FS_FAT_DIRENT_TIME_GET_SEC(time);
    min  = FS_FAT_DIRENT_TIME_GET_MIN(time);
    hour = FS_FAT_DIRENT_TIME_GET_HOUR(time);

    if ((sec < 61u) && (min < 60u) && (hour < 24u)) {
        stime.Sec = sec;
        stime.Min = min;
        stime.Hr  = hour;
    } else {
        valid = DEF_NO;
    }

    if (valid == DEF_YES) {
        valid  = Clk_DateTimeToTS_Unix(p_ts,
                                      &stime);
        if (valid != DEF_YES) {
           *p_ts   = FS_TIME_TS_INVALID;
        }

    } else {
       *p_ts = FS_TIME_TS_INVALID;
    }
}


/*
*********************************************************************************************************
*                                           FS_FAT_SecClr()
*
* Description : Clear sector(s) on a volume.
*
* Argument(s) : p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
*               p_temp      Pointer to temporary buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of clear.
*
*               cnt         Number of sectors to clear.
*
*               sec_size    Sector size, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Sector(s) cleared.
*                               FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_SecClr (FS_VOL          *p_vol,
                     void            *p_temp,
                     FS_FAT_SEC_NBR   start,
                     FS_FAT_SEC_NBR   cnt,
                     FS_SEC_SIZE      sec_size,
                     FS_FLAGS         sec_type,
                     FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR  cnt_rem;
    FS_FAT_SEC_NBR  cnt_clr;
    FS_FAT_SEC_NBR  sec_per_buf;


    Mem_Clr(p_temp, FS_MaxSecSizeGet());

    cnt_rem     = cnt;
    sec_per_buf = FS_MaxSecSizeGet() / sec_size;

    while (cnt_rem > 0u) {
        cnt_clr = (cnt_rem > sec_per_buf) ? sec_per_buf : cnt_rem;

        FSVol_WrLockedEx(            p_vol,
                                     p_temp,
                         (FS_SEC_NBR)start,
                         (FS_SEC_QTY)cnt_clr,
                                     sec_type,
                                     p_err);

        if (*p_err != FS_ERR_NONE) {
            *p_err  = FS_ERR_DEV;
             break;
        }

        cnt_rem -= cnt_clr;
        start   += cnt_clr;
    }
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_Char_LastPathSep()
*
* Description : Search string for last occurrence of path separator character NOT at the end of string.
*
* Argument(s) : pstr        Pointer to string.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to last occurrence of path separator character in string, if any.
*
*               Pointer to NULL,                                                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
CPU_CHAR  *FS_FAT_Char_LastPathSep (CPU_CHAR  *pstr)
{
    CPU_CHAR     *pstr_next;
    CPU_SIZE_T    str_len;
    CPU_BOOLEAN   non_sep_found;


    pstr_next      = pstr;
    str_len        = Str_Len_N(pstr, FS_FAT_MAX_PATH_NAME_LEN);
    pstr_next     += str_len;
    non_sep_found  = DEF_NO;
    while ( ( pstr_next     != pstr) &&                         /* Srch str from end until beginning ...                */
           ((*pstr_next     != FS_FAT_PATH_SEP_CHAR) ||         /* ... until srch char found         ...                */
            ( non_sep_found == DEF_NO))) {                      /* ... but after non-srch char found.                   */
        if ((*pstr_next != FS_FAT_PATH_SEP_CHAR) &&
            (*pstr_next != ASCII_CHAR_NULL)) {
            non_sep_found = DEF_YES;
        }
        pstr_next--;
    }


    if (*pstr_next != FS_FAT_PATH_SEP_CHAR) {                   /* If srch char NOT found, str points to NULL; ...      */
         return ((CPU_CHAR *)0);                                /* ... rtn NULL.                                        */
    }

    return (pstr_next);                                         /* Else rtn ptr to found srch char.                     */
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
*                                         FS_FAT_ChkBootSec()
*
* Description : Check boot sector & calculate FAT parameters.
*
* Argument(s) : p_fat_data  Pointer to FAT data.
*               ----------  Argument validated by caller.
*
*               p_temp_08   Pointer to buffer containing boot sector.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE           File system information parsed.
*                               FS_ERR_INVALID_SYS    Invalid file system.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The first 36 bytes of the FAT boot sector are identical for FAT12/16/32 file
*                       systems :
*
*                       ---------------------------------------------------------------------------------------------------------
*                       |    JUMP INSTRUCTION TO BOOT CODE*    |             OEM NAME (in ASCII): TYPICALLY "MSWINx.y"
*                       ---------------------------------------------------------------------------------------------------------
*                                                              |    BYTES PER SECTOR**   | SEC/CLUS***|    RSVD SEC CNT****     |
*                       ---------------------------------------------------------------------------------------------------------
*                       | NBR FATs^  |    ROOT ENTRY CNT^^     | TOTAL NBR of SECTORS^^^ | MEDIA^^^^  |    FAT SIZE (in SEC)#   |
*                       ---------------------------------------------------------------------------------------------------------
*                       |    SECTORS PER TRACK    |       NBR of HEADS      |       NBR SECTORS before START OF PARTITION       |
*                       ---------------------------------------------------------------------------------------------------------
*                       |             TOTAL NBR of SECTORS^^^               |
*                       -----------------------------------------------------
*
*                       *Legal forms are
*                           boot[0] = 0xEB, boot[1] = ????, boot[2] = 0x90
*                        &
*                           boot[0] = 0xE9, boot[1] = ????, boot[2] = ????.
*
*                      **Legal values are 512, 1024, 2048 & 4096.
*
*                     ***Legal values are 2, 4, 8, 16, 32, 64 & 128.  However, the number of bytes per
*                        cluster MUST be less than or equal to 32 * 1024.
*
*                    ****For FAT12 & FAT16, this SHOULD be 1.  In any case, this MUST NOT be 0.
*
*                       ^This SHOULD be 2.
*
*                      ^^For FAT12/16, this field contains the number of directory entries in the root
*                        directory, typically 512.  For FAT32, this field should be 0.
*
*                     ^^^Either the 16-bit count (bytes 19-20) or the 32-bit count (bytes 32-35) should
*                        be non-zero & contain the total number of sectors in the file system.  The
*                        other should be zero.
*
*                    ^^^^Media type.  Legal values are 0xF0, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE
*                        & 0xFF.  Typically, 0xF0 should be used for fixed disks & 0xF8 should be used
*                        for removable disks.
*
*                       #For FAT12/16, the number of sectors in each FAT.  For FAT32, this SHOULD be 0.
*
*                   (b) The bytes 36-61 of the FAT boot sector for FAT12/16 file systems contain the
*                       following :
*
*                                                                           -----------------------------------------------------
*                                                                           | DRIVE NBR  |    0x00    |    0x29    |
*                       ---------------------------------------------------------------------------------------------------------
*                          VOLUME SERIAL NBR                   |            VOLUME LABEL (in ASCII)
*                       ---------------------------------------------------------------------------------------------------------
*                                                                                                     |
*                       ---------------------------------------------------------------------------------------------------------
                                 FILE SYSTEM TYPE LABEL (in ASCII)                                    |
*                       -------------------------------------------------------------------------------
*
*                   (c) The bytes 36-89 of the FAT boot sector for FAT32 file systems contain the
*                       following :
*
*                                                                           -----------------------------------------------------
*                                                                           |               FAT SIZE (in SECTORS)               |
*                       ---------------------------------------------------------------------------------------------------------
*                       |         FLAGS*          |     VERSION NUMBER**    |     CLUSTER NBR of FIRST CLUSTER OF ROOT DIR      |
*                       ---------------------------------------------------------------------------------------------------------
*                       |   FSINFO SECTOR NBR***  |  BACKUP BOOT RECORD**** |                       0x0000
*                       ---------------------------------------------------------------------------------------------------------
*                                              0x0000                                               0x0000                      |
*                       ---------------------------------------------------------------------------------------------------------
*                       | DRIVE NBR  |    0x00    |    0x29    |                 VOLUME SERIAL NBR                 |
*                       ---------------------------------------------------------------------------------------------------------
*                                                             VOLUME LABEL (in ASCII)
*                       ---------------------------------------------------------------------------------------------------------
*                                                 |             FILE SYSTEM TYPE LABEL (in ASCII): TYPICALLY "FAT32    "
*                       ---------------------------------------------------------------------------------------------------------
*                                                 |
*                       ---------------------------
*
*                       *If bit 7 of this is 1, then only one FAT is active; the number of this FAT is
*                        specified in bits 0-3.  Otherwise, the FAT is mirrored at runtime into all FATs.
*
*                      **File systems conforming to Microsoft's FAT documentation should contain 0x0000.
*
*                     ***Typically 1.
*
*                    ****Typically 6.
*
*                   (d) For FAT12/16/32, bytes 510 & 511 of the FAT boot sector are ALWAYS 0xAA55.
*
*               (2) (a) The total number of sectors in the file system MAY be smaller (perhaps
*                       considerably smaller) than the number of sectors in the disk.  However, the
*                       total number of sectors in the file system SHOULD NEVER be larger than the
*                       number of sectors in the disk.
*
*                   (b) For FAT32, the number of sectors in the root directory is ALWAYS 0, since the
*                       root directory lies inside the data area.
*
*                   (c) (1) For FAT12/16, the data area begins after the predetermined root directory
*                           (which immediately follows the FAT sectors).
*
*                       (2) For FAT32,    the data area begins immediately after the FAT sectors (&
*                           includes the root directory).
*
*                       (3) (a) The total number of clusters in the volume is equal to the total number
*                               of data sectors divided by the number of clusters per sector :
*
*                                                             Number of Data Sectors
*                                   Number of Clusters = -------------------------------
*                                                         Number of Clusters per Sector
*
*                           (b) Since clusters 0 & 1 do not exist, the highest cluster number is
*                               'Number of Clusters' + 1.
*
*                   (d) (1) Four areas of a FAT12/16 file system exist :
*
*                           (a) The reserved area.            ----------------------------------------------------------------
*                           (b) The FAT      area.            | Rsvd |   FAT 1   |   FAT 2   | Root |         Data           |
*                           (c) The root     directory.       | Area |   Area    |   Area    | Dir  |         Area           |
*                           (d) The data     area.            ----------------------------------------------------------------
*                                                                    ^           ^           ^      ^
*                                                                    |           |           |      |
*                                                                    |           |           |      |
*                                                 'FS_FAT_DATA.FAT1_Start'       |           |      |
*                                                                                |           |      |
*                                                             'FS_FAT_DATA.FAT2_Start'       |   'FS_FAT_DATA.DataStart'
*                                                                                            |          @ Cluster #2
*                                                                     'FS_FAT_DATA.RootDirStart'
*
*                       (2) Three areas of a FAT32 file system exist :
*
*                           (a) The reserved area.            ----------------------------------------------------------------
*                           (b) The FAT      area.            | Rsvd |   FAT 1   |   FAT 2   |            Data               |
*                           (c) The data     area.            | Area |   Area    |   Area    |            Area               |
*                                                             ----------------------------------------------------------------
*                                                                    ^           ^           ^
*                                                                    |           |           |
*                                                                    |           |           |
*                                                 'FS_FAT_DATA.FAT1_Start'       |        'FS_FAT_DATA.DataStart'
*                                                                                |               @ Cluster #2
*                                                             'FS_FAT_DATA.FAT2_Start'
*
*                           Unlike FAT12/16, the root directory is in cluster(s) within the data area.
*
*                       (3) Up to three sectors of the reserved area may be used :
*
*                           (1) THE BOOT SECTOR.  This sector, the sector 0 of the volume, contains
*                               information about the format, size & layout of the file system.
*
*                           (2) THE BACKUP BOOT SECTOR.  This sector, typically sector 1 of the volume,
*                               contains a backup copy of the boot sector.  The backup boot sector is
*                               NOT used on FAT12/16 volumes.
*
*                           (3) THE FSINFO SECTOR.  This sector, typically sector 6 of the volume, may
*                               be used to help the file system suite more quickly access the volume.
*                               The FSINFO sector is NOT used on FAT12/16 volumes.
*
*               (3) The 'ClusSize_octet' value is stored temporarily in a 32-bit variable to protect
*                   against 16-bit overflow.  However, according to Microsoft's FAT specification, all
*                   legitimate values fit within the range of 16-bit unsigned integers.
********************************************************************************************************
*/

static  void  FS_FAT_ChkBootSec (FS_FAT_DATA  *p_fat_data,
                                 CPU_INT08U   *p_temp_08,
                                 FS_ERR       *p_err)
{
    FS_SEC_SIZE      clus_size_octet;
    FS_FAT_CLUS_NBR  clus_cnt;
    FS_FAT_CLUS_NBR  clus_cnt_max_fat;

    FS_FAT_SEC_NBR   fat_size;
    CPU_INT16U       root_cnt;
    CPU_INT32U       vol_size;
#if (FS_FAT_CFG_FAT32_EN == DEF_ENABLED)
    FS_FAT_CLUS_NBR  clus_nbr;
    CPU_INT16U       sec_nbr;
    CPU_BOOLEAN      valid;
#endif


                                                                /* ---------------- RD BOOT SET PARAMS ---------------- */
    p_fat_data->SecSize = (FS_SEC_SIZE)MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_BYTSPERSEC));
    switch (p_fat_data->SecSize) {
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
             p_fat_data->SecSizeLog2 = FSUtil_Log2(p_fat_data->SecSize);
             break;

        default:
             FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid bytes/sec: %d\r\n", p_fat_data->SecSize));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }

    p_fat_data->ClusSize_sec = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_SECPERCLUS));
    switch (p_fat_data->ClusSize_sec) {
        case 1u:
        case 2u:
        case 4u:
        case 8u:
        case 16u:
        case 32u:
        case 64u:
        case 128u:
             p_fat_data->ClusSizeLog2_sec = FSUtil_Log2((CPU_INT32U)p_fat_data->ClusSize_sec);
             break;

        default:
             FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid secs/clus: %d\n", p_fat_data->ClusSize_sec));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }

                                                                /* See Note #3.                                         */
    clus_size_octet = p_fat_data->SecSize << p_fat_data->ClusSizeLog2_sec;
    switch (clus_size_octet) {
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
        case 8192u:
        case 16384u:
        case 32768u:
        case 65536u:
             p_fat_data->ClusSize_octet     = clus_size_octet;
             p_fat_data->ClusSizeLog2_octet = FSUtil_Log2(clus_size_octet);
             break;

        default:
             FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid bytes/clus: %d\n", clus_size_octet));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }

    p_fat_data->NbrFATs = MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_NUMFATS));
    switch (p_fat_data->NbrFATs) {
        case 1:
        case 2:
             break;

        default:
             FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid nbr FATs: %d\n", p_fat_data->NbrFATs));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }
                                                                /* Size of reserved area.                               */
    p_fat_data->RsvdSize    = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_RSVDSECCNT));
    p_fat_data->FAT1_Start  =  p_fat_data->RsvdSize;            /* Sec nbr of 1st FAT.                                  */

                                                                /* Size of each FAT.                                    */
    fat_size                = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_FATSZ16));
    if (fat_size           ==  0u) {
        fat_size            = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_FAT32_OFF_FATSZ32));
    }
    p_fat_data->FAT_Size    =  fat_size;

                                                                /* Size of root dir (see Notes #2b).                    */
    root_cnt                =  MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_ROOTENTCNT));
    p_fat_data->RootDirSize =  FS_UTIL_DIV_PWR2((((FS_FAT_SEC_NBR)root_cnt * 32u) + ((FS_FAT_SEC_NBR)p_fat_data->SecSize - 1u)), p_fat_data->SecSizeLog2);

                                                                /* Sec nbr's of 2nd FAT, root dir, data.                */
    if (p_fat_data->NbrFATs      == 2u) {
        p_fat_data->FAT2_Start   =  p_fat_data->RsvdSize +  p_fat_data->FAT_Size;
        p_fat_data->RootDirStart =  p_fat_data->RsvdSize + (p_fat_data->FAT_Size * 2u);
        p_fat_data->DataStart    =  p_fat_data->RsvdSize + (p_fat_data->FAT_Size * 2u) + p_fat_data->RootDirSize;
    } else {
        p_fat_data->FAT2_Start   =  0u;
        p_fat_data->RootDirStart =  p_fat_data->RsvdSize + (p_fat_data->FAT_Size * 1u);
        p_fat_data->DataStart    =  p_fat_data->RsvdSize + (p_fat_data->FAT_Size * 1u) + p_fat_data->RootDirSize;
    }

                                                                /* Size of vol (see Notes #2a).                         */
    vol_size                = (CPU_INT32U)MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_TOTSEC16));
    if (vol_size           == 0u) {
        vol_size            = MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_OFF_TOTSEC32));
    }
    p_fat_data->VolSize     = vol_size;

                                                                /* Size of data area (see Notes #2c).                   */
    p_fat_data->DataSize    = (p_fat_data->VolSize - (p_fat_data->RsvdSize + ((FS_FAT_SEC_NBR)p_fat_data->NbrFATs * p_fat_data->FAT_Size))) - p_fat_data->RootDirSize;
    p_fat_data->NextClusNbr =  2u;                              /* Next clus to alloc.                                  */
                                                                /* See Notes #2c3.                                      */
    clus_cnt                =  (FS_FAT_CLUS_NBR)FS_UTIL_DIV_PWR2(p_fat_data->DataSize, p_fat_data->ClusSizeLog2_sec);



                                                                /* --------------------- CHK FAT12 -------------------- */
    if (clus_cnt                   <=  FS_FAT_MAX_NBR_CLUS_FAT12) {
#if (FS_FAT_CFG_FAT12_EN == DEF_ENABLED)
        p_fat_data->FAT_Type        =  FS_FAT_FAT_TYPE_FAT12;
        p_fat_data->FAT_TypeAPI_Ptr = &FS_FAT_FAT12_API;
        clus_cnt_max_fat            = ((FS_FAT_CLUS_NBR)p_fat_data->FAT_Size * (FS_FAT_CLUS_NBR)p_fat_data->SecSize * 2u) / 3u;
        p_fat_data->MaxClusNbr      =  DEF_MIN(clus_cnt + FS_FAT_MIN_CLUS_NBR, clus_cnt_max_fat + FS_FAT_MIN_CLUS_NBR);
#else
        FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid file sys: FAT12\n"));
       *p_err = FS_ERR_VOL_INVALID_SYS;
        return;
#endif


                                                                /* --------------------- CHK FAT16 -------------------- */
    } else if (clus_cnt            <=  FS_FAT_MAX_NBR_CLUS_FAT16) {
#if (FS_FAT_CFG_FAT16_EN == DEF_ENABLED)
        p_fat_data->FAT_Type        =  FS_FAT_FAT_TYPE_FAT16;
        p_fat_data->FAT_TypeAPI_Ptr = &FS_FAT_FAT16_API;
        clus_cnt_max_fat            = (FS_FAT_CLUS_NBR)p_fat_data->FAT_Size * ((FS_FAT_CLUS_NBR)p_fat_data->SecSize / FS_FAT_FAT16_ENTRY_NBR_OCTETS);
        p_fat_data->MaxClusNbr      =  DEF_MIN(clus_cnt + FS_FAT_MIN_CLUS_NBR, clus_cnt_max_fat + FS_FAT_MIN_CLUS_NBR);
#else
        FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid file sys: FAT16\n"));
       *p_err = FS_ERR_VOL_INVALID_SYS;
        return;
#endif


                                                                /* --------------------- CHK FAT32 -------------------- */
    } else {
#if (FS_FAT_CFG_FAT32_EN == DEF_ENABLED)
        p_fat_data->FAT_Type        =  FS_FAT_FAT_TYPE_FAT32;
        p_fat_data->FAT_TypeAPI_Ptr = &FS_FAT_FAT32_API;
        clus_cnt_max_fat            = (FS_FAT_CLUS_NBR)p_fat_data->FAT_Size * ((FS_FAT_CLUS_NBR)p_fat_data->SecSize / FS_FAT_FAT32_ENTRY_NBR_OCTETS);
        p_fat_data->MaxClusNbr      = DEF_MIN(clus_cnt + FS_FAT_MIN_CLUS_NBR, clus_cnt_max_fat + FS_FAT_MIN_CLUS_NBR);

                                                                /* Get root dir clus nbr.                               */
        clus_nbr                    = MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_FAT32_OFF_ROOTCLUS));
        valid                       = FS_FAT_IS_VALID_CLUS(p_fat_data, clus_nbr);
        if (valid == DEF_NO) {
            FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid FAT32 root dir clus: %d\n", clus_nbr));
           *p_err = FS_ERR_VOL_INVALID_SYS;
            return;
        }
        p_fat_data->RootDirStart     = FS_FAT_CLUS_TO_SEC(p_fat_data, clus_nbr);
                                                                /* Get FSINFO sec nbr.                                  */
        sec_nbr                      = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_FAT32_OFF_FSINFO));
        if (sec_nbr != 0u) {
            p_fat_data->FS_InfoStart = (FS_FAT_SEC_NBR)sec_nbr;
        }
                                                                /* Get BKBOOT sec nbr.                                  */
        sec_nbr                      = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp_08 + FS_FAT_BPB_FAT32_OFF_BKBOOTSEC));
        if (sec_nbr != 0u) {
            p_fat_data->BPB_BkStart  = (FS_FAT_SEC_NBR)sec_nbr;
        }
#else
        FS_TRACE_DBG(("FS_FAT_ChkBootSec(): Invalid file sys: FAT32\n"));
       *p_err = FS_ERR_VOL_INVALID_SYS;
        return;
#endif
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FS_FAT_ChkDir()
*
* Description : Check a directory on a file system.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE              Directory checked successfully.
*                                   FS_ERR_BUF_NONE_AVAIL    No buffers available.
*                                   FS_ERR_DEV               Device error.
*
* Return(s)   : none.
*
* Note(s)     : (1) A directory is a special type of file.  Consequently, it must abide certain rules
*                   common to all files :
*
*                   (a) The size in the directory entry should be valid.  For a directory, the size should
*                       always be zero.
*
*                   (b) The cluster number in the directory entry should be valid or zero.
*
*                   (c) The date/times in the directory entry should be valid or zero; however, this is
*                       not checked.
*
*               (2) If an invalid cluster is linked to a directory, the last will be made EOF.
*
*               (3) If empty clusters are linked to a directory, the extra (unoccupied) clusters will
*                   be freed to release wasted volume space.
*
*                   (a) #### Free clusters.
********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
static  void  FS_FAT_ChkDir (FS_VOL            *p_vol,
                             FS_FAT_FILE_DATA  *p_entry_data,
                             FS_ERR            *p_err)
{
    CPU_BOOLEAN       end_of_dir;
    FS_FAT_CLUS_NBR   dir_clus;
    FS_FAT_CLUS_NBR   dir_clus_next;
    FS_FAT_CLUS_NBR   dir_clus_cnt_max;
    FS_FAT_CLUS_NBR   dir_clus_cnt;
#ifdef  FS_FAT_LFN_MODULE_PRESENT
    FS_FAT_SEC_NBR    dir_sec;
#endif
    CPU_BOOLEAN       valid;
    FS_BUF           *p_buf;
    FS_FAT_DATA      *p_fat_data;


                                                                /* ------------------ PREPARE FOR CHK ----------------- */
    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;



                                                                /* ------------------ CHK CLUS VALID ------------------ */
    valid      = FS_FAT_IS_VALID_CLUS(p_fat_data, p_entry_data->FileFirstClus);
    if (valid == DEF_NO) {                                      /* If clus not valid ...                                */
        FS_TRACE_INFO(("FS_FAT_ChkDir(): Dir clus nbr invalid.\r\n"));

                                                                /* ... clr clus nbr & file size in entry.               */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        p_entry_data->FileFirstClus = 0u;
        p_entry_data->FileSize      = 0u;

        FS_FAT_LowEntryUpdate(p_vol,
                              p_buf,
                              p_entry_data,
                              DEF_YES,
                              p_err);
        FSBuf_Flush(p_buf, p_err);
#endif

        FSBuf_Free(p_buf);
        return;
    }



                                                                /* ------------------ CHK CLUS CHAIN ------------------ */
                                                                /* Chk clus chain.                                      */
    dir_clus_cnt_max = p_fat_data->MaxClusNbr - FS_FAT_MIN_CLUS_NBR;
    dir_clus_cnt     = 0u;
    dir_clus         = p_entry_data->FileFirstClus;
    end_of_dir       = DEF_NO;
    while ((dir_clus_cnt <  dir_clus_cnt_max) &&                /* While max clus's have not been examined ...          */
           (end_of_dir   == DEF_NO)) {                          /* ... & dir end not found                 ...          */

                                                                /* ... follow dir clus chain.                           */
        dir_clus_next = FS_FAT_ClusNextGet(p_vol,
                                           p_buf,
                                           dir_clus,
                                           p_err);

        switch (*p_err) {
            case FS_ERR_NONE:                                   /* If valid clus found ...                              */
                 dir_clus_cnt++;                                /* ... follow chain.                                    */
                 dir_clus = dir_clus_next;
                 break;


            case FS_ERR_SYS_CLUS_CHAIN_END:
                 dir_clus_cnt++;
                 end_of_dir = DEF_YES;
                 break;


            case FS_ERR_SYS_CLUS_INVALID:                       /* If invalid clus found ...                            */
                 FS_TRACE_INFO(("FS_FAT_ChkDir(): Invalid clus linked to chain.\r\n"));

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* ... replace clus with EOF.                           */
                 (void)FS_FAT_ClusChainDel(p_vol,               /* #### UCFS-372                                        */
                                           p_buf,
                                           dir_clus,
                                           DEF_YES,
                                           p_err);
                 if (*p_err != FS_ERR_NONE) {
                     FSBuf_Free(p_buf);
                     return;
                 }
#endif
                 end_of_dir = DEF_YES;
                 break;


            case FS_ERR_DEV:
            default:
                 FSBuf_Free(p_buf);
                 return;

        }
    }

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (end_of_dir == DEF_NO) {                                 /* If EOF not found ...                                 */
        FS_TRACE_INFO(("FS_FAT_ChkDir(): No EOF in dir.\r\n"));
    }
#endif


#ifdef  FS_FAT_LFN_MODULE_PRESENT
                                                                /* ----------- CHK FOR ORPHANED LFN ENTRIES ----------- */
    dir_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, p_entry_data->FileFirstClus);
    FS_FAT_LFN_DirChk(p_vol,
                      p_buf,
                      dir_sec,
                      p_err);
#endif


    FSBuf_Free(p_buf);
}
#endif


/*
*********************************************************************************************************
*                                          FS_FAT_ChkFile()
*
* Description : Check a file on a file system.
*
* Argument(s) : p_vol           Pointer to volume.
*               ----------      Argument validated by caller.
*
*               p_entry_data    Pointer to FAT file information.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE              File checked successfully.
*                                   FS_ERR_BUF_NONE_AVAIL    No buffers available.
*                                   FS_ERR_DEV               Device error.
*
* Return(s)   : none.
*
* Note(s)     : (1) All files must abide certain rules :
*
*                   (a) The size in the directory entry should be valid.
*
*                   (b) The cluster number in the directory entry should be valid or zero.  If the size
*                       is non-zero, the cluster number MUST be valid.
*                       (1) If the cluster number is invalid, the cluster number & size in the directory
*                           entry will be cleared.
*
*                   (c) The date/times in the directory entry should be valid or zero; however, this is
*                       not checked.
*
*                   (e) The number of clusters linked to the directory MUST be sufficient to accommodate
*                       the file size.  The directory is valid if more clusters are linked.
*
*               (2) If an invalid cluster is linked to a file, the last will be made EOF.  If necessary,
*                   the file size will also be adjusted.
*
*               (3) If too few clusters are linked to a file, the file size will be adjusted.
*
*               (4) If too many clusters are linked to a file, the extra (unoccupied) clusters will be
*                   freed to release wasted volume space.
*********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
static  void  FS_FAT_ChkFile (FS_VOL            *p_vol,
                              FS_FAT_FILE_DATA  *p_entry_data,
                              FS_ERR            *p_err)
{
    CPU_BOOLEAN        end_of_file;
    FS_FAT_CLUS_NBR    file_clus;
    FS_FAT_CLUS_NBR    file_clus_next;
    FS_FAT_FILE_SIZE   file_size;
    FS_FAT_CLUS_NBR    file_clus_tot;
    FS_FAT_CLUS_NBR    file_clus_cnt;
    CPU_BOOLEAN        valid;
    FS_BUF            *p_buf;
    FS_FAT_DATA       *p_fat_data;


                                                                /* ------------------ PREPARE FOR CHK ----------------- */
    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;



                                                                /* ------------------ CHK CLUS VALID ------------------ */
    valid      = FS_FAT_IS_VALID_CLUS(p_fat_data, p_entry_data->FileFirstClus);
    if ((valid == DEF_NO) && (p_entry_data->FileFirstClus != 0u)) {     /* If clus not valid ...                        */
        FS_TRACE_INFO(("FS_FAT_ChkFile(): File clus nbr invalid.\r\n"));

                                                                /* ... clr clus nbr & file size in entry.               */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
        p_entry_data->FileFirstClus = 0u;
        p_entry_data->FileSize      = 0u;

        FS_FAT_LowEntryUpdate(p_vol,
                              p_buf,
                              p_entry_data,
                              DEF_YES,
                              p_err);

        FSBuf_Flush(p_buf, p_err);
#endif

        FSBuf_Free(p_buf);
        return;
    }



                                                                /* ---------------- CHK CLUS CHAIN LEN ---------------- */
                                                                /* Chk clus chain len.                                  */

    file_size     = p_entry_data->FileSize;
    file_clus_tot = FS_UTIL_DIV_PWR2(file_size, p_fat_data->ClusSizeLog2_octet);
    if ((file_size & (p_fat_data->ClusSize_octet - 1u)) != 0u) {
        file_clus_tot++;
    }

    file_clus_cnt = 0u;
    file_clus     = p_entry_data->FileFirstClus;
    end_of_file   = DEF_NO;
    while ((file_clus_cnt <  file_clus_tot) &&                  /* While not all clus's have been examined ...          */
           (end_of_file   == DEF_NO)) {                         /* ... & file end not found                ...          */

                                                                /* ... follow file clus chain.                          */
        file_clus_next = FS_FAT_ClusNextGet(p_vol,
                                            p_buf,
                                            file_clus,
                                            p_err);

        switch (*p_err) {
            case FS_ERR_NONE:                                   /* If valid clus found ...                              */
                 file_clus_cnt++;                               /* ... follow chain.                                    */
                 file_clus = file_clus_next;
                 break;


            case FS_ERR_SYS_CLUS_CHAIN_END:
                 file_clus_cnt++;
                                                                /* If EOF before all clus's followed ...                */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                 if (file_clus_cnt != file_clus_tot) {
                     FS_TRACE_INFO(("FS_FAT_ChkFile(): Insufficient clusters (%d < %d) linked to file.\r\n", file_clus_cnt, file_clus_tot));
                                                                /* ... correct file size.                               */

                     p_entry_data->FileSize = file_clus_cnt * p_fat_data->ClusSize_octet;

                     FS_FAT_LowEntryUpdate(p_vol,
                                           p_buf,
                                           p_entry_data,
                                           DEF_YES,
                                           p_err);
                     if (*p_err != FS_ERR_NONE) {
                         break;
                     }

                 }
#else
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
                 if (file_clus_cnt != file_clus_tot) {
                     FS_TRACE_INFO(("FS_FAT_ChkFile(): Insufficient clusters (%d < %d) linked to file.\r\n", file_clus_cnt, file_clus_tot));
                 }
#endif
#endif

                 end_of_file = DEF_YES;
                 break;


            case FS_ERR_SYS_CLUS_INVALID:                       /* If invalid clus found     ...                        */
                 FS_TRACE_INFO(("FS_FAT_ChkFile(): Invalid clus linked to chain.\r\n"));

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                                                                /* ... replace clus with EOF ...                        */
                (void)FS_FAT_ClusChainDel(p_vol,                /* #### UCFS-372                                        */
                                          p_buf,
                                          file_clus,
                                          DEF_YES,
                                          p_err);
                 if (*p_err != FS_ERR_NONE) {
                      break;
                 }

                                                                /* ... & correct file size.                             */
                 p_entry_data->FileSize = file_clus_cnt * p_fat_data->ClusSize_octet;

                 FS_FAT_LowEntryUpdate(p_vol,
                                       p_buf,
                                       p_entry_data,
                                       DEF_YES,
                                       p_err);
#endif
                 end_of_file = DEF_YES;
                 break;


            case FS_ERR_DEV:
            default:
                 FSBuf_Free(p_buf);
                 return;

        }
    }

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    if (end_of_file == DEF_NO) {                                /* If EOF not found, too many clus's in chain ...       */
        if ((file_size != 0) && (file_clus_tot != 0)) {         /* Skip misleading message for 0-byte files w/o cluster */
            FS_TRACE_INFO(("FS_FAT_ChkFile(): Extra clusters linked to file.\r\n"));
        }
    }
#endif

    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
}
#endif


/*
*********************************************************************************************************
*                                         FS_FAT_GetSysCfg()
*
* Description : Get format configuration.
*
* Argument(s) : sec_size    Sector size, in octets.
*               --------    Argument validated by caller.
*
*               size        Size of device, in sectors.
*               ----        Argument validated by caller.
*
*               p_sys_cfg   Pointer to FAT configuration that will receive configuration parameters.
*               ---------   Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE               Format configuration found.
*                               FS_ERR_VOL_INVALID_SYS    Invalid volume for file system.
*
* Return(s)   : none.
*
* Note(s)     : (1) $$$$ Verify information in FAT tables.
*
*               (2) See 'FS_FAT_VolFmt() Note #2'.
*
*               (3) A device with a large number of sectors & a large sector size may not be FAT-
*                   formattable.  In this case, an invalid bytes-per-cluster value will be computed.  If
*                   an invalid bytes-per-cluster value is computed, then an error is returned, since this
*                   may signal an error in a hardware driver.  If it these are legitimate parameters,
*                   then the volume should be formatted with a reduced number of data sectors.
*
*                   (a) $$$$ Allow user to specify reduced number of data sectors.
*
*               (4) A valid FAT table entry should always be found; however, the sectors per cluster
*                   value is checked in case the code or structures are incorrectly modified.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_GetSysCfg  (FS_SEC_SIZE      sec_size,
                                 FS_SEC_QTY       size,
                                 FS_FAT_SYS_CFG  *p_sys_cfg,
                                 FS_ERR          *p_err)
{
           FS_SEC_SIZE        clus_size_octet;
           CPU_BOOLEAN        found;
           FS_FAT_SEC_NBR     size_sec;
           CPU_INT64U         size_octets;
    const  FS_FAT_TBL_ENTRY  *p_tbl_entry;


                                                                /* ------------------ VERIFY DEV INFO ----------------- */
    switch (sec_size) {                                         /* Verify bytes per sec.                                */
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
             break;


        default:
             FS_TRACE_DBG(("FS_FAT_GetSysCfg(): Invalid sec size: %d.\n", sec_size));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }
    found = DEF_NO;

    size_sec = (FS_FAT_SEC_NBR)size;

    size_octets = (CPU_INT64U)size_sec * (CPU_INT64U)sec_size;


                                                                /* ------------------- CHK IF FAT32 ------------------- */
    if (size_octets > FS_FAT_MAX_SIZE_FAT16) {                  /* This CAN be FAT32 ... get clus size.                 */
        p_tbl_entry = &FS_FAT_TblFAT32[0];
        while ((size_sec > p_tbl_entry->VolSize) && (p_tbl_entry->VolSize != 0u)) {
            p_tbl_entry++;
        }
        if (p_tbl_entry->ClusSize != 0u) {                      /* Rtn err if sec per clus zero (see Note #4).          */
            p_sys_cfg->FAT_Type        =             FS_FAT_FAT_TYPE_FAT32;
            p_sys_cfg->RootDirEntryCnt =             FS_FAT_DFLT_ROOT_ENT_CNT_FAT32;
            p_sys_cfg->RsvdAreaSize    =             FS_FAT_DFLT_RSVD_SEC_CNT_FAT32;
            p_sys_cfg->ClusSize        = (FS_SEC_QTY)p_tbl_entry->ClusSize;
            p_sys_cfg->NbrFATs         =             2u;
            found                      =             DEF_YES;
        }
    }



    if (found == DEF_NO) {                                      /* ------------------- CHK IF FAT16 ------------------- */
        if (size_octets > FS_FAT_MAX_SIZE_FAT12) {              /* This CAN be FAT16 ... get clus size.                 */
            p_tbl_entry = &FS_FAT_TblFAT16[0];
            while ((size_sec > p_tbl_entry->VolSize) && (p_tbl_entry->VolSize != 0u)) {
                p_tbl_entry++;
            }
            if (p_tbl_entry->ClusSize != 0u) {                  /* Rtn err if sec per clus zero (see Note #4).          */
                p_sys_cfg->FAT_Type        =             FS_FAT_FAT_TYPE_FAT16;
                p_sys_cfg->RootDirEntryCnt =             FS_FAT_DFLT_ROOT_ENT_CNT_FAT16;
                p_sys_cfg->RsvdAreaSize    =             FS_FAT_DFLT_RSVD_SEC_CNT_FAT16;
                p_sys_cfg->ClusSize        = (FS_SEC_QTY)p_tbl_entry->ClusSize;
                p_sys_cfg->NbrFATs         =             2u;
                found                      =             DEF_YES;
            }
        }
    }



    if (found == DEF_NO) {                                      /* ------------------- CHK IF FAT12 ------------------- */
        p_tbl_entry = &FS_FAT_TblFAT12[0];
        while ((size_sec > p_tbl_entry->VolSize) && (p_tbl_entry->VolSize != 0u)) {
            p_tbl_entry++;
        }
        if (p_tbl_entry->ClusSize == 0u) {                  /* Rtn err if sec per clus zero (see Note #4).          */
            FS_TRACE_DBG(("FS_FAT_GetSysCfg(): No fmt cfg found.\r\n"));
           *p_err = FS_ERR_VOL_INVALID_SYS;
            return;
        }
        p_sys_cfg->FAT_Type        =             FS_FAT_FAT_TYPE_FAT12;
        p_sys_cfg->RootDirEntryCnt =             FS_FAT_DFLT_ROOT_ENT_CNT_FAT12;
        p_sys_cfg->RsvdAreaSize    =             FS_FAT_DFLT_RSVD_SEC_CNT_FAT12;
        p_sys_cfg->ClusSize        = (FS_SEC_QTY)p_tbl_entry->ClusSize;
        p_sys_cfg->NbrFATs         =             2u;
    }



                                                                /* ------------------- CHK DISK CFG ------------------- */
    clus_size_octet = (FS_SEC_SIZE)p_sys_cfg->ClusSize * sec_size;
    switch (clus_size_octet) {                                  /* Verify bytes per clus (see Note #3).                 */
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
        case 8192u:
        case 16384u:
        case 32768u:
            *p_err = FS_ERR_NONE;
             break;


        default:
             FS_TRACE_DBG(("FS_FAT_GetSysCfg(): Invalid bytes/clus: %d.\r\n", p_sys_cfg->ClusSize * sec_size));
            *p_err = FS_ERR_VOL_INVALID_SYS;
             return;
    }
}
#endif


/*
*********************************************************************************************************
*                                          FS_FAT_DataSrch()
*
* Description : Search data area for find path.
*
* Argument(s) : p_vol               Pointer to volume.
*               ----------          Argument validated by caller.
*
*               p_buf               Pointer to temporary buffer.
*               ----------          Argument validated by caller.
*
*               name_entry          Name of the entry.
*               ----------          Argument validated by caller.
*
*               p_dir_first_sec     Pointer to variable that will receive first sector of the directory
*                                   in which file was located.
*               ----------          Argument validated by caller.
*
*               p_dir_start_pos     Pointer to variable that will receive the directory position at which
*                                   the first entry is located.
*               ----------          Argument validated by caller.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the final entry is located.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                      Path found.
*                                       FS_ERR_ENTRY_NOT_FOUND           File system entry NOT found
*                                       FS_ERR_ENTRY_PARENT_NOT_FOUND    Entry parent NOT found.
*                                       FS_ERR_ENTRY_PARENT_NOT_DIR      Entry parent NOT a directory.
*                                       FS_ERR_NAME_INVALID              Invalid file name or path.
*                                       FS_ERR_VOL_INVALID_SEC_NBR       Invalid sector number found in directory
*                                                                        entry.
*
*                                                                        --- RETURNED BY FS_FAT_SFN_Process() ---
*                                                                        --- RETURNED BY FS_FAT_LFN_Process() ---
*                                       FS_ERR_NAME_INVALID              Invalid name.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*
*               (2) (a) Consecutive file separator characters are ignored.
*
*                   (b) #### Handle "dot" & "dot dot" file path components.
*********************************************************************************************************
*/

static  void  FS_FAT_DataSrch (FS_VOL          *p_vol,
                               FS_BUF          *p_buf,
                               CPU_CHAR        *name_entry,
                               FS_FAT_SEC_NBR  *p_dir_first_sec,
                               FS_FAT_DIR_POS  *p_dir_start_pos,
                               FS_FAT_DIR_POS  *p_dir_end_pos,
                               FS_ERR          *p_err)
{
    CPU_INT08U        attrib;
    FS_FAT_DIR_POS    dir_start_pos;
    FS_FAT_DIR_POS    dir_end_pos;
    FS_FAT_CLUS_NBR   dir_first_clus;
    FS_FAT_SEC_NBR    dir_first_sec;
    CPU_INT08U       *p_dir_entry;
    FS_FAT_DATA      *p_fat_data;
    CPU_CHAR          name_char;
    CPU_CHAR         *name_entry_next;


    p_dir_start_pos->SecNbr = 0u;                               /* Assign dflts.                                        */
    p_dir_start_pos->SecPos = 0u;

    p_dir_end_pos->SecNbr   = 0u;
    p_dir_end_pos->SecPos   = 0u;

   *p_dir_first_sec        =  0u;                               /* Ensure the ptr is assigned when returning.           */

                                                                /* Start srch in root dir.                              */
    dir_first_sec = ((FS_FAT_DATA *)p_vol->DataPtr)->RootDirStart;


    while (*name_entry == (CPU_CHAR)ASCII_CHAR_SPACE) {         /* Ignore leading spaces.                               */
        name_entry++;
    }

    while (DEF_ON) {
                                                                /* ------------------ FIND DIR ENTRY ------------------ */
        dir_start_pos.SecNbr = dir_first_sec;
        dir_start_pos.SecPos = 0u;

        FS_FAT_FN_API_Active.DirEntryFind( p_vol,
                                           p_buf,
                                           name_entry,
                                          &name_entry_next,
                                          &dir_start_pos,
                                          &dir_end_pos,
                                           p_err);
        if (*p_err != FS_ERR_NAME_INVALID) {
            name_entry = name_entry_next;
        }


        switch (*p_err) {
            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND:                /* ---------------- DIR ENTRY NOT FOUND --------------- */
                 name_char = *name_entry;                       /* Move past file path separator char(s) (see Note #2a).*/
                 while (name_char == FS_FAT_PATH_SEP_CHAR) {
                     name_entry++;
                     name_char = *name_entry;
                 }

                 if (*name_entry == (CPU_CHAR)ASCII_CHAR_NULL) {/* Final path component NOT found.                      */
                     *p_err           = FS_ERR_ENTRY_NOT_FOUND;
                     *p_dir_first_sec = dir_first_sec;
                 } else {                                       /* Intermediate path component NOT found.               */
                     *p_err = FS_ERR_ENTRY_PARENT_NOT_FOUND;
                 }
                 break;



            case FS_ERR_NONE:                                   /* ------------------ DIR ENTRY FOUND ----------------- */
                 name_char = *name_entry;                       /* Move past file path separator char(s) (see Note #2a).*/
                 while (name_char == FS_FAT_PATH_SEP_CHAR) {
                     name_entry++;
                     name_char = *name_entry;
                 }

                 if (*name_entry == (CPU_CHAR)ASCII_CHAR_NULL) {/* Final path component found.                          */
                     *p_err = FS_ERR_NONE;
                     *p_dir_first_sec         = dir_first_sec;
                      p_dir_start_pos->SecNbr = dir_start_pos.SecNbr;
                      p_dir_start_pos->SecPos = dir_start_pos.SecPos;
                      p_dir_end_pos->SecNbr   = dir_end_pos.SecNbr;
                      p_dir_end_pos->SecPos   = dir_end_pos.SecPos;
                      return;
                 }

                 p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
                 attrib      = MEM_VAL_GET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR));

                                                                /* Intermediate path component found.                   */
                                                                /* Rtn err if entry is vol ID.                          */
                 if (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                    *p_err = FS_ERR_ENTRY_PARENT_NOT_DIR;
                     return;
                 }
                                                                /* Rtn err if entry is NOT dir.                         */
                 if (DEF_BIT_IS_CLR(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
                    *p_err = FS_ERR_ENTRY_PARENT_NOT_DIR;
                     return;
                 }
                                                                /* Get sec & clus nbr of dir's first sec.               */
                 p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
                 dir_first_clus = FS_FAT_DIRENT_CLUS_NBR_GET(p_dir_entry);
                 if ((dir_first_clus == 0u) &&
                     ((p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT12) || (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT16))) {
                     dir_first_sec = p_fat_data->RootDirStart;  /* Clus val 0 -> root dir for FAT12/16.                 */
                 } else if (FS_FAT_IS_VALID_CLUS(p_fat_data, dir_first_clus) == DEF_NO) {
                    *p_err = FS_ERR_VOL_INVALID_SEC_NBR;        /* Invalid clus val.                                    */
                     return;
                 } else {                                       /* Valid clus val.                                      */
                     dir_first_sec = FS_FAT_CLUS_TO_SEC(p_fat_data, dir_first_clus);
                 }
                 break;


            case FS_ERR_NAME_INVALID:                           /* ------------------- INVALID NAME ------------------- */
                 break;


            case FS_ERR_DEV:                                    /* --------------------- DEV ERROR -------------------- */
                 break;


            default:
                 FS_TRACE_DBG(("FS_FAT_DataSrch(): Default case reached.\r\n"));
                 break;
        }

        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                        FS_FAT_FileDataClr()
*
* Description : Clear FAT file data structure
*
* Argument(s) : p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_FileDataClr (FS_FAT_FILE_DATA  *p_entry_data)
{
    p_entry_data->FilePos        = 0u;
    p_entry_data->FileSize       = 0u;
    p_entry_data->UpdateReqd     = DEF_NO;
    p_entry_data->Mode           = DEF_BIT_NONE;

    p_entry_data->DirFirstSec    = 0u;
    p_entry_data->DirStartSec    = 0u;
    p_entry_data->DirStartSecPos = 0u;
    p_entry_data->DirEndSec      = 0u;
    p_entry_data->DirEndSecPos   = 0u;

    p_entry_data->FileFirstClus  = 0u;
    p_entry_data->FileCurSec     = 0u;
    p_entry_data->FileCurSecPos  = 0u;

    p_entry_data->Attrib         = 0u;
    p_entry_data->DateCreate     = 0u;
    p_entry_data->TimeCreate     = 0u;
    p_entry_data->DateWr         = 0u;
    p_entry_data->TimeWr         = 0u;
    p_entry_data->DateAccess     = 0u;
}


/*
*********************************************************************************************************
*                                        FS_FAT_FileDataInit()
*
* Description : Initialize FAT file data structure with directory entry information.
*
* Argument(s) : p_entry_data    Pointer to FAT entry data.
*               ----------      Argument validated by caller.
*
*               p_dir_entry     Pointer to directory entry.
*               ----------      Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_FileDataInit (FS_FAT_FILE_DATA *p_entry_data,
                                   CPU_INT08U       *p_dir_entry)
{
    p_entry_data->FileSize   = MEM_VAL_GET_INT32U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FILESIZE));
    p_entry_data->Attrib     = MEM_VAL_GET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR));
    p_entry_data->DateCreate = MEM_VAL_GET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_CRTDATE));
    p_entry_data->TimeCreate = MEM_VAL_GET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_CRTTIME));
    p_entry_data->DateWr     = MEM_VAL_GET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTDATE));
    p_entry_data->TimeWr     = MEM_VAL_GET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTTIME));
    p_entry_data->DateAccess = MEM_VAL_GET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_LSTACCDATE));
}


/*
*********************************************************************************************************
*                                          FS_FAT_DataClr()
*
* Description : Clear FAT info structure
*
* Argument(s) : p_fat_data  Pointer to FAT info structure.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_DataClr (FS_FAT_DATA  *p_fat_data)
{
    p_fat_data->RsvdSize           =  0u;
    p_fat_data->FAT_Size           =  0u;
    p_fat_data->RootDirSize        =  0u;
    p_fat_data->VolSize            =  0u;
    p_fat_data->DataSize           =  0u;
    p_fat_data->MaxClusNbr         =  0u;

    p_fat_data->FS_InfoStart       =  0u;
    p_fat_data->BPB_BkStart        =  0u;
    p_fat_data->FAT1_Start         =  0u;
    p_fat_data->FAT2_Start         =  0u;
    p_fat_data->RootDirStart       =  0u;
    p_fat_data->DataStart          =  0u;

    p_fat_data->NextClusNbr        =  0u;

    p_fat_data->SecSize            =  0u;
    p_fat_data->SecSizeLog2        =  0u;
    p_fat_data->ClusSize_sec       =  0u;
    p_fat_data->ClusSizeLog2_sec   =  0u;
    p_fat_data->ClusSize_octet     =  0u;
    p_fat_data->ClusSizeLog2_octet =  0u;

    p_fat_data->NbrFATs            =  0u;
    p_fat_data->FAT_Type           =  0u;
    p_fat_data->FAT_TypeAPI_Ptr    = (FS_FAT_TYPE_API *)0;

    p_fat_data->QueryInfoValid     =  DEF_NO;
    p_fat_data->QueryBadClusCnt    =  0u;
    p_fat_data->QueryFreeClusCnt   =  0u;

#if (FS_CFG_CTR_STAT_EN            == DEF_ENABLED)
    p_fat_data->StatAllocClusCtr   =  0u;
    p_fat_data->StatFreeClusCtr    =  0u;
#endif
}


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of FAT module include (see Note #1).             */
