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
*                                       LONG FILE NAME SUPPORT
*
* Filename : fs_fat_lfn.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_LFN_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_cfg_fs.h"
#include  "../Source/fs_unicode.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_journal.h"
#include  "fs_fat_lfn.h"
#include  "fs_fat_sfn.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat_lfn.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_LFN_MODULE_PRESENT                              /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_DIRENT_LFN_NBR_CHARS                      13u

#define  FS_FAT_LFN_CHAR_EMPTY_HI                       0xFFu
#define  FS_FAT_LFN_CHAR_EMPTY_LO                       0xFFu

#define  FS_FAT_LFN_CHAR_FINAL_HI                       0x00u
#define  FS_FAT_LFN_CHAR_FINAL_LO                       0x00u

#define  FS_FAT_LFN_OFF_SEQNUMBER                          0u
#define  FS_FAT_LFN_OFF_NAMEFIRST                          1u
#define  FS_FAT_LFN_OFF_ATTR                              11u
#define  FS_FAT_LFN_OFF_RES                               12u
#define  FS_FAT_LFN_OFF_CHKSUM                            13u
#define  FS_FAT_LFN_OFF_NAMESECOND                        14u
#define  FS_FAT_LFN_OFF_FSTCLUSLO                         26u
#define  FS_FAT_LFN_OFF_NAMETHIRD                         28u


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT32U  FS_FAT_SFN_TAIL;

#if (FS_CFG_UTF8_EN == DEF_ENABLED)
typedef  CPU_WCHAR   FS_FAT_LFN_CHAR;
#else
typedef  CPU_CHAR    FS_FAT_LFN_CHAR;
#endif


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
*                                           LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                                        /* ------- FILE NAME API ------ */
                                                                                        /* Create LFN dir entry.        */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void               FS_FAT_LFN_DirEntryCreate          (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               CPU_CHAR               *name,
                                                               CPU_BOOLEAN             is_dir,
                                                               FS_FAT_CLUS_NBR         file_first_clus,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);

                                                                                        /* Create LFN dir entry at pos. */
static  void               FS_FAT_LFN_DirEntryCreateAt        (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               CPU_CHAR               *name,
                                                               FS_FILE_NAME_LEN        name_len,
                                                               CPU_INT32U              name_8_3[],
                                                               CPU_BOOLEAN             is_dir,
                                                               FS_FAT_CLUS_NBR         file_first_clus,
                                                               FS_FAT_DIR_POS         *p_dir_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);

                                                                                        /* Delete LFN dir entry.        */
static  void               FS_FAT_LFN_DirEntryDel             (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);
#endif
                                                                                        /* Srch dir for LFN dir entry.  */
static  void               FS_FAT_LFN_DirEntryFind            (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               CPU_CHAR               *name,
                                                               CPU_CHAR              **p_name_next,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);

                                                                                        /* Get next dir entry in dir.   */
static  void               FS_FAT_LFN_NextDirEntryGet         (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               void                   *name,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);


                                                                                        /* -------- LOCAL FNCTS ------- */
                                                                                        /* Check name as LFN.           */
static  void               FS_FAT_LFN_Chk                     (CPU_CHAR               *name,
                                                               FS_FILE_NAME_LEN       *p_name_len,
                                                               FS_FILE_NAME_LEN       *p_name_len_octet,
                                                               FS_ERR                 *p_err);

                                                                                        /* Calculate check sum for SFN. */
static  CPU_INT08U         FS_FAT_LFN_ChkSumCalc              (void                   *name_8_3);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                                                 /* Format dir entry for LFN.    */
static  void               FS_FAT_LFN_DirEntryFmt             (void                   *p_dir_entry,
                                                               FS_FAT_LFN_CHAR        *name,
                                                               FS_FILE_NAME_LEN        char_cnt,
                                                               CPU_INT08U              chk_sum,
                                                               FS_FAT_DIR_ENTRY_QTY    lfn_nbr,
                                                               CPU_BOOLEAN             is_final);
#endif

                                                                                        /* Find dir entries for LFN.    */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void               FS_FAT_LFN_DirEntryPlace           (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               FS_FAT_DIR_ENTRY_QTY    dir_entry_cnt,
                                                               FS_FAT_SEC_NBR          dir_start_sec,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);
#endif

                                                                                        /* Find length of LFN entry.    */
static  FS_FILE_NAME_LEN   FS_FAT_LFN_Len                     (void                   *p_dir_entry);

                                                                                        /* Parse LFN.                   */
static  void               FS_FAT_LFN_Parse                   (void                   *p_dir_entry,
                                                               FS_FAT_LFN_CHAR        *name,
                                                               FS_FILE_NAME_LEN        name_len);

                                                                                        /* Parse full LFN.              */
static  void               FS_FAT_LFN_ParseFull               (void                   *p_dir_entry,
                                                               FS_FAT_LFN_CHAR        *name);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                                                 /* Allocate SFN for LFN.        */
static  void               FS_FAT_LFN_SFN_Alloc               (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               CPU_CHAR               *name,
                                                               CPU_INT32U              name_8_3[],
                                                               FS_FAT_SEC_NBR          dir_sec,
                                                               FS_ERR                 *p_err);

                                                                                        /* Format SFN from LFN.         */
static  FS_FILE_NAME_LEN   FS_FAT_LFN_SFN_Fmt                 (CPU_CHAR               *name,
                                                               CPU_INT32U              name_8_3[],
                                                               FS_ERR                 *p_err);
#endif

                                                                                        /* Parse SFN into LFN buffer.   */
static  void               FS_FAT_LFN_SFN_Parse               (void                   *p_dir_entry,
                                                               FS_FAT_LFN_CHAR        *name,
                                                               CPU_BOOLEAN             name_lower_case,
                                                               CPU_BOOLEAN             ext_lower_case);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                                                 /* Format SFN tail.             */
static  void               FS_FAT_LFN_SFN_FmtTail             (CPU_INT32U              name_8_3[],
                                                               FS_FILE_NAME_LEN        char_cnt,
                                                               FS_FAT_SFN_TAIL         tail_nbr);

                                                                                        /* Find max tail val.           */
static  FS_FAT_SFN_TAIL    FS_FAT_LFN_SFN_DirEntryFindMax     (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               CPU_INT32U              name_8_3[],
                                                               FS_FAT_SEC_NBR          dir_start_sec,
                                                               FS_ERR                 *p_err);

                                                                                        /* Find max tail val in sector. */
static  FS_FAT_SFN_TAIL    FS_FAT_LFN_SFN_DirEntryFindMaxInSec(void                   *p_temp,
                                                               FS_SEC_SIZE             sec_size,
                                                               CPU_INT32U              name_8_3[],
                                                               FS_ERR                 *p_err);
#endif

static  FS_FILE_NAME_LEN   FS_FAT_LFN_StrLen_N                (FS_FAT_LFN_CHAR        *p_str,
                                                               FS_FILE_NAME_LEN        len_max);


static  CPU_INT32S         FS_FAT_LFN_StrCmpIgnoreCase_N      (CPU_CHAR               *p1_str,
                                                               FS_FAT_LFN_CHAR        *p2_str,
                                                               FS_FILE_NAME_LEN        len_max);

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)                                              /* Check dir entries.           */
static  CPU_INT08U         FS_FAT_LFN_DirEntriesChk           (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_FAT_DIR_POS         *p_dir_end_pos,
                                                               FS_ERR                 *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)                                                 /* Delete dir entry.             */
static  void               FS_FAT_LFN_DirEntryFree            (FS_VOL                 *p_vol,
                                                               FS_BUF                 *p_buf,
                                                               FS_FAT_DIR_POS         *p_dir_start_pos,
                                                               FS_ERR                 *p_err);
#endif
#endif

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_FAT_FN_API  FS_FAT_LFN_API = {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_FAT_LFN_DirEntryCreate,
    FS_FAT_LFN_DirEntryCreateAt,
    FS_FAT_LFN_DirEntryDel,
#endif
    FS_FAT_LFN_DirEntryFind,
    FS_FAT_LFN_NextDirEntryGet,
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FS_FAT_LFN_DirChk()
*
* Description : Check entries in directory.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               dir_sec     Directory sector in which entries should be checked.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    Directory checked.
*                               FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
void  FS_FAT_LFN_DirChk (FS_VOL          *p_vol,
                         FS_BUF          *p_buf,
                         FS_FAT_SEC_NBR   dir_sec,
                         FS_ERR          *p_err)
{
    FS_FAT_DIR_POS   dir_cur_pos;
    FS_FAT_DIR_POS   dir_end_pos;
    CPU_INT08U       entry_cnt;
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_FAT_DATA     *p_fat_data;
    FS_FAT_DIR_POS   dir_free_pos;
#endif


    dir_cur_pos.SecNbr =  dir_sec;
    dir_cur_pos.SecPos =  0u;

    do {
        entry_cnt = FS_FAT_LFN_DirEntriesChk( p_vol,
                                              p_buf,
                                             &dir_cur_pos,
                                             &dir_end_pos,
                                              p_err);

        switch (*p_err) {
            case FS_ERR_NONE:
                 return;

            case FS_ERR_DEV:
                 return;

            case FS_ERR_SYS_LFN_ORPHANED:
                 FS_TRACE_DBG(("FS_FAT_LFN_DirChk(): Orphaned LFN entries: %d entries starting at %08X.%04X\r\n", entry_cnt, dir_end_pos.SecNbr, dir_end_pos.SecPos));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
                 p_fat_data         = (FS_FAT_DATA *)p_vol->DataPtr;

                 dir_free_pos.SecNbr = dir_end_pos.SecNbr;
                 dir_free_pos.SecPos = dir_end_pos.SecPos;
                 while (entry_cnt > 0) {
                                                                /* Free dir entry.                                      */
                     FS_FAT_LFN_DirEntryFree( p_vol,
                                              p_buf,
                                             &dir_free_pos,
                                              p_err);
                     if (*p_err != FS_ERR_NONE) {
                         return;
                     }

                     entry_cnt--;

                                                                /* Rd next dir sec.                                     */
                     if (entry_cnt > 0) {
                                                                /* Move to next entry.                                  */
                         dir_free_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
                         if (dir_free_pos.SecPos == p_fat_data->SecSize) {
                             dir_free_pos.SecNbr =  FS_FAT_SecNextGet(p_vol,
                                                                      p_buf,
                                                                      dir_free_pos.SecNbr,
                                                                      p_err);

                             switch (*p_err) {
                                 case FS_ERR_NONE:
                                      break;

                                 case FS_ERR_SYS_CLUS_CHAIN_END:
                                 case FS_ERR_SYS_CLUS_INVALID:
                                     *p_err = FS_ERR_NONE;
                                      return;

                                 case FS_ERR_DEV:
                                 default:
                                      return;
                             }

                             dir_free_pos.SecPos = 0u;
                         }

                     }
                 }
#endif
                 break;

            default:
                 FS_TRACE_DBG(("FS_FAT_LFN_DirChk(): Default case reached.\r\n"));
                 return;
        }

        dir_cur_pos.SecNbr = dir_end_pos.SecNbr;                /* Start checking at del'd dir entries.                 */
        dir_cur_pos.SecPos = dir_end_pos.SecPos;

    } while (entry_cnt != 0);                                   /* While invalid entries found.                         */

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       FILE NAME API FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     FS_FAT_LFN_DirEntryCreate()
*
* Description : Create LFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name                Name of the entry.
*
*               is_dir              Indicates whether directory entry is entry for directory :
*
*                                       DEF_YES, entry is for directory.
*                                       DEF_NO,  entry is for file.
*
*               file_first_clus     First cluster in entry.
*
*               p_dir_start_pos     Pointer to directory position at which entry should be created;
*                                   variable that will receive the directory position at which the first
*                                   entry_cnt (the first LFN entry or the SFN entry) was created.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the final entry (the SFN entry) was created.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                 Directory entry created.
*                                       FS_ERR_DEV                  Device access error.
*                                       FS_ERR_DIR_FULL             Directory is full (space could not be allocated).
*                                       FS_ERR_DEV_FULL             Device is full (space could not be allocated).
*                                       FS_ERR_ENTRY_CORRUPT        File system entry is corrupt.
*
*                                                                   --- RETURNED BY FS_FAT_LFN_SFN_Alloc() ---
*                                       FS_ERR_NAME_INVALID         LFN name invalid.
*                                       FS_ERR_SYS_SFN_NOT_AVAIL    No SFN available.
*
*                                                                   ----- RETURNED BY FS_FAT_JournalExitDirEntryCreate() -----
*                                       FS_ERR_BUF_NONE_AVAIL       Buffer not available.
*                                       FS_ERR_JOURNAL_FULL         Journal full.
*
* Return(s)   : none.
*
* Note(s)     : (1) ???? If name is a valid SFN, create only SFN.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryCreate (FS_VOL           *p_vol,
                                         FS_BUF           *p_buf,
                                         CPU_CHAR         *name,
                                         CPU_BOOLEAN       is_dir,
                                         FS_FAT_CLUS_NBR   file_first_clus,
                                         FS_FAT_DIR_POS   *p_dir_start_pos,
                                         FS_FAT_DIR_POS   *p_dir_end_pos,
                                         FS_ERR           *p_err)
{
    FS_FAT_DIR_POS        dir_cur_pos;
    FS_FAT_DIR_ENTRY_QTY  dir_entry_cnt;
    CPU_BOOLEAN           ext_lower_case;
    CPU_BOOLEAN           formed;
    CPU_INT32U            name_8_3[4];
    FS_FILE_NAME_LEN      name_len;
    FS_FILE_NAME_LEN      name_len_octet;
    CPU_BOOLEAN           name_lower_case;


    p_dir_end_pos->SecNbr = 0u;                                 /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos = 0u;



                                                                /* ----------------- FIND DIR ENTRIES ----------------- */
    FS_FAT_LFN_Chk( name,                                       /* Chk whether valid LFN.                               */
                   &name_len,
                   &name_len_octet,
                    p_err);

    if (*p_err != FS_ERR_NONE) {
         return;
    }

                                                                /* Calc nbr of LFN dir entries.                         */
    dir_entry_cnt = (FS_FAT_DIR_ENTRY_QTY)((name_len + FS_FAT_DIRENT_LFN_NBR_CHARS - 1u) / FS_FAT_DIRENT_LFN_NBR_CHARS);

    FS_FAT_LFN_DirEntryPlace( p_vol,                            /* Find consecutive dir entries for LFN & SFN.          */
                              p_buf,
                             (dir_entry_cnt + 1u),
                              p_dir_start_pos->SecNbr,
                             &dir_cur_pos,
                              p_err);


    switch (*p_err) {
        case FS_ERR_NONE:
             break;

        case FS_ERR_DEV_FULL:
        case FS_ERR_DIR_FULL:
        case FS_ERR_ENTRY_CORRUPT:
             FS_TRACE_DBG(("FS_FAT_LFN_DirEntryCreate(): No dir entries avail.\r\n"));
             return;

        case FS_ERR_DEV:
             return;

        default:
             FS_TRACE_DBG(("FS_FAT_LFN_DirEntryCreate(): Default case reached.\r\n"));
             return;
    }


                                                                /* --------------------- ALLOC SFN -------------------- */
    formed = DEF_NO;
    if (name_len_octet == name_len) {                           /* If name contains only 8-bit characters ...           */
        FS_FAT_SFN_Create( name,                                /* ... try to form SFN directly from LFN  ...           */
                          &name_8_3[0],
                          &name_lower_case,
                          &ext_lower_case,
                           p_err);

        if (*p_err == FS_ERR_NONE) {
            FS_FAT_SFN_API.DirEntryCreate(p_vol,                /* ... create only SFN.                                 */
                                          p_buf,
                                          name,
                                          is_dir,
                                          file_first_clus,
                                          p_dir_start_pos,
                                          p_dir_end_pos,
                                          p_err);
            return;

        } else {
            if (*p_err == FS_ERR_NAME_MIXED_CASE) {
                formed = DEF_YES;
            }
        }
    }

    if (formed == DEF_NO) {                                     /* If SFN could NOT be formed directly ...              */
        FS_FAT_LFN_SFN_Alloc( p_vol,                            /* ... use LFN generation.                              */
                              p_buf,
                              name,
                             &name_8_3[0],
                              p_dir_start_pos->SecNbr,
                              p_err);

        if (*p_err != FS_ERR_NONE) {
             return;
        }
    }



                                                                /* ------------------- WR DIR ENTRY ------------------- */
    FS_FAT_LFN_DirEntryCreateAt( p_vol,
                                 p_buf,
                                 name,
                                 name_len,
                                &name_8_3[0],
                                 is_dir,
                                 file_first_clus,
                                &dir_cur_pos,
                                 p_dir_end_pos,
                                 p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;               /* Rtn start pos.                                       */
    p_dir_start_pos->SecPos = dir_cur_pos.SecPos;
}
#endif


/*
*********************************************************************************************************
*                                    FS_FAT_LFN_DirEntryCreateAt()
*
* Description : Create LFN directory entry at specified position.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name                Name of the entry.
*
*               name_len            Entry name length, in characters.
*
*               name_8_3            Entry SFN.
*
*               is_dir              Indicates whether directory entry is entry for directory :
*
*                                       DEF_YES, entry is for directory.
*                                       DEF_NO,  entry is for file.
*
*               file_first_clus     First cluster in entry.
*
*               p_dir_pos           Pointer to directory position at which entry should be created.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the final entry (the SFN entry) was created.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE             Directory entry created.
*                                       FS_ERR_DEV              Device access error.
*                                       FS_ERR_DIR_FULL         Directory is full (space could not be allocated).
*                                       FS_ERR_DEV_FULL         Device is full (space could not be allocated).
*                                       FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryCreateAt (FS_VOL            *p_vol,
                                           FS_BUF            *p_buf,
                                           CPU_CHAR          *name,
                                           FS_FILE_NAME_LEN   name_len,
                                           CPU_INT32U         name_8_3[],
                                           CPU_BOOLEAN        is_dir,
                                           FS_FAT_CLUS_NBR    file_first_clus,
                                           FS_FAT_DIR_POS    *p_dir_pos,
                                           FS_FAT_DIR_POS    *p_dir_end_pos,
                                           FS_ERR            *p_err)
{
    FS_FAT_DATA           *p_fat_data;
    CPU_INT08U            *p_dir_entry;
    FS_FILE_NAME_LEN       char_cnt;
    FS_FILE_NAME_LEN       char_cnt_wr;
    CPU_INT08U             chk_sum;
    FS_FAT_DIR_POS         dir_cur_pos;
    FS_FAT_DIR_ENTRY_QTY   dir_entry_cnt;
    CPU_BOOLEAN            is_final;
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_WCHAR              name_wc[FS_FAT_MAX_FILE_NAME_LEN + 1u];
    CPU_CHAR              *p_name_rtn;
#endif
    FS_FAT_LFN_CHAR       *p_name_create;
#ifdef FS_FAT_JOURNAL_MODULE_PRESENT
    CPU_INT32U             dir_entry_ix;
#endif


    p_dir_end_pos->SecNbr =  0u;                                /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos =  0u;

    p_fat_data            = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ---------------- WR LFN DIR ENTRIES ---------------- */
    char_cnt       =  name_len;

#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    p_name_rtn     =  name;
    (void)MB_StrToWC(&name_wc[0],
                     &p_name_rtn,
                      FS_FAT_MAX_FILE_NAME_LEN);
    p_name_create  = &name_wc[0];
#else
    p_name_create  =  name;
#endif

                                                                /* Calc nbr of LFN dir entries.                         */
    dir_entry_cnt  = (FS_FAT_DIR_ENTRY_QTY)((char_cnt + FS_FAT_DIRENT_LFN_NBR_CHARS - 1u) / FS_FAT_DIRENT_LFN_NBR_CHARS);
    p_name_create += char_cnt;                                  /* Move to end of file name.                            */
    is_final       = DEF_YES;
    chk_sum        = FS_FAT_LFN_ChkSumCalc((void *)name_8_3);

    char_cnt_wr    = char_cnt % FS_FAT_DIRENT_LFN_NBR_CHARS;    /* Calc nbr chars for final LFN dir entry.              */
    if (char_cnt_wr == 0u) {
        char_cnt_wr =  FS_FAT_DIRENT_LFN_NBR_CHARS;
    }


#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {

                                                                /* ------------- CALC NEW ENTRY LAST POS -------------- */
        dir_cur_pos.SecNbr =  p_dir_pos->SecNbr;
        dir_cur_pos.SecPos =  p_dir_pos->SecPos;

        for (dir_entry_ix = 0u; dir_entry_ix < dir_entry_cnt; dir_entry_ix++) {
            dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;

            if (dir_cur_pos.SecPos == p_fat_data->SecSize) {
                dir_cur_pos.SecNbr = FS_FAT_SecNextGet(p_vol,   /* Get next dir sec.                                    */
                                                       p_buf,
                                                       dir_cur_pos.SecNbr,
                                                       p_err);
                switch (*p_err) {
                    case FS_ERR_NONE:
                         break;

                    case FS_ERR_DIR_FULL:
                    case FS_ERR_DEV_FULL:
                         FS_TRACE_DBG(("FS_FAT_LFN_DirEntryCreateAt(): No dir entries avail.\r\n"));
                         return;

                    case FS_ERR_SYS_CLUS_CHAIN_END:
                    case FS_ERR_SYS_CLUS_INVALID:
                        *p_err = FS_ERR_ENTRY_CORRUPT;
                         return;

                    case FS_ERR_DEV:
                    default:
                         return;
                }
                dir_cur_pos.SecPos = 0u;
            }
        }
                                                                /* ------------------- ENTER JOURNAL ------------------ */
        FS_FAT_JournalEnterEntryCreate(p_vol,
                                       p_buf,
                                       p_dir_pos,
                                      &dir_cur_pos,
                                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#endif

    dir_cur_pos.SecNbr =  p_dir_pos->SecNbr;                    /* Reset cur dir pos.                                   */
    dir_cur_pos.SecPos =  p_dir_pos->SecPos;

    FSBuf_Set(p_buf,                                            /* Rd dir sec of init dir entry.                        */
              dir_cur_pos.SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_entry        = (CPU_INT08U *)p_buf->DataPtr + dir_cur_pos.SecPos;

    while (dir_entry_cnt >= 1u) {
        p_name_create -= char_cnt_wr;
        char_cnt      -= char_cnt_wr;

        FS_FAT_LFN_DirEntryFmt((void *)p_dir_entry,             /* Fmt new LFN entry.                                   */
                                       p_name_create,
                                       char_cnt_wr,
                                       chk_sum,
                                       dir_entry_cnt,
                                       is_final);


        char_cnt_wr = FS_FAT_DIRENT_LFN_NBR_CHARS;
        is_final    = DEF_NO;
        dir_entry_cnt--;

        p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
        dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;

        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {
            FSBuf_MarkDirty(p_buf, p_err);                      /* Wr updated dir sec.                                  */
            if (*p_err != FS_ERR_NONE) {
                return;
            }

                                                                /* Get next dir sec.                                    */
            dir_cur_pos.SecNbr = FS_FAT_SecNextGet(p_vol,
                                                   p_buf,
                                                   dir_cur_pos.SecNbr,
                                                   p_err);

            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_DIR_FULL:
                case FS_ERR_DEV_FULL:
                     FS_TRACE_DBG(("FS_FAT_LFN_DirEntryCreateAt(): No dir entries avail.\r\n"));
                     return;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                    *p_err = FS_ERR_ENTRY_CORRUPT;
                     return;

                case FS_ERR_DEV:
                default:
                     return;
            }

            FSBuf_Set(p_buf,                                    /* Rd new dir sec.                                      */
                      dir_cur_pos.SecNbr,
                      FS_VOL_SEC_TYPE_DIR,
                      DEF_YES,
                      p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }

            p_dir_entry        = (CPU_INT08U *)p_buf->DataPtr;
            dir_cur_pos.SecPos = 0u;
        }
    }


                                                                /* ----------------- WR SFN DIR ENTRY ----------------- */
    FS_FAT_SFN_DirEntryFmt((void *)p_dir_entry,                 /* Make SFN dir entry.                                  */
                                   name_8_3,
                                   is_dir,
                                   file_first_clus,
                                   DEF_NO,
                                   DEF_NO);

    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr updated dir sec.                                  */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_end_pos->SecNbr = dir_cur_pos.SecNbr;
    p_dir_end_pos->SecPos = dir_cur_pos.SecPos;
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_LFN_DirEntryDel()
*
* Description : Delete LFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to directory position at which the first entry (the first LFN
*                                   entry or the SFN entry) is located.
*
*               p_dir_end_pos       Pointer to directory position at which the final entry (the SFN
*                                   entry) is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE    Directory entry deleted.
*                                       FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryDel (FS_VOL          *p_vol,
                                      FS_BUF          *p_buf,
                                      FS_FAT_DIR_POS  *p_dir_start_pos,
                                      FS_FAT_DIR_POS  *p_dir_end_pos,
                                      FS_ERR          *p_err)
{
    FS_FAT_DATA     *p_fat_data;
    FS_FAT_DIR_POS   dir_cur_pos;
    CPU_INT08U      *p_dir_entry;
    FS_FAT_SEC_NBR   dir_next_sec;
    CPU_BOOLEAN      dir_sec_valid;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
        FS_FAT_JournalEnterEntryUpdate(p_vol,
                                       p_buf,
                                       p_dir_start_pos,
                                       p_dir_end_pos,
                                       p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#endif


                                                                /* -------------------- RD DIR SEC -------------------- */
    p_dir_entry         = (CPU_INT08U  *)p_buf->DataPtr;
    p_dir_entry        +=  p_dir_start_pos->SecPos;

    dir_cur_pos.SecNbr  =  p_dir_start_pos->SecNbr;
    dir_cur_pos.SecPos  =  p_dir_start_pos->SecPos;
    dir_sec_valid       =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    FSBuf_Set(p_buf,
              dir_cur_pos.SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    while ( (dir_sec_valid      == DEF_YES) &&                  /* While sec is valid (see Note #1)  ...                */
           ((dir_cur_pos.SecNbr != p_dir_end_pos->SecNbr) ||    /* ... &     end sec     not reached ...                */
            (dir_cur_pos.SecPos <= p_dir_end_pos->SecPos))) {   /* ...    OR end sec pos not reached.                   */

                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {
                                                                /* Get next sec in dir.                                 */
            dir_next_sec = FS_FAT_SecNextGet(p_vol,
                                             p_buf,
                                             dir_cur_pos.SecNbr,
                                             p_err);

            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                     FS_TRACE_INFO(("FS_FAT_LFN_DirEntryDel(): Unexpected end to dir while del'ing entry (0x%08X).\r\n", dir_cur_pos.SecNbr));
                    *p_err = FS_ERR_ENTRY_CORRUPT;
                     return;

                case FS_ERR_DEV:
                default:
                     return;
            }

                                                                /* Chk sec validity (see Note #1).                      */
            dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_next_sec);

            dir_cur_pos.SecNbr = dir_next_sec;
            dir_cur_pos.SecPos = 0u;

            if (dir_sec_valid == DEF_YES) {
                p_dir_entry = (CPU_INT08U *)p_buf->DataPtr;

                FSBuf_Set(p_buf,
                          dir_cur_pos.SecNbr,
                          FS_VOL_SEC_TYPE_DIR,
                          DEF_YES,
                          p_err);
                if (*p_err != FS_ERR_NONE) {
                    return;
                }
            }
        }

        if (dir_sec_valid == DEF_YES) {
                                                                /* ------------------- CLR DIR ENTRY ------------------ */
#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
            Mem_Clr((void *)p_dir_entry,
                            FS_FAT_SIZE_DIR_ENTRY);
#endif
            p_dir_entry[0] = FS_FAT_DIRENT_NAME_ERASED_AND_FREE;/* Mark LFN dir entry as free.                          */

            FSBuf_MarkDirty(p_buf, p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }



                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
            dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
            p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
        }
    }




                                                                /* ---------------- ALL DIR SECS FOUND ---------------- */
    if (dir_sec_valid == DEF_NO) {                              /* Invalid sec found (see Note #1).                     */
        FS_TRACE_DBG(("FS_FAT_LFN_DirEntryDel(): Invalid sec gotten: %d.\r\n", dir_cur_pos.SecNbr));
       *p_err = FS_ERR_ENTRY_CORRUPT;
    }
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_LFN_DirEntryFind()
*
* Description : Search directory for LFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name                Name of the entry (see Note #2).
*
*               p_name_next         Pointer to variable that will receive pointer to character following
*                                   entry name (see Note #2).
*
*               p_dir_start_pos     Pointer to directory position at which search should start; variable
*                                   will receive the directory position at which the first entry (the
*                                   first LFN entry or the SFN entry) is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the final entry (the SFN entry) is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                       Directory entry found.
*                                       FS_ERR_DEV                        Device access error.
*                                       FS_ERR_SYS_DIR_ENTRY_NOT_FOUND    Directory entry not found.
*
*                                                                         --- RETURNED BY FS_FAT_LFN/SFN_Chk ---
*                                       FS_ERR_NAME_INVALID               File name is illegal.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*
*               (2) The file name 'name' MAY not be NULL-terminated; it may be followed by a path
*                   separator character.  Only the characters until the first NULL or path separator
*                   character should be used in the comparison.
*
*               (3) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_DirEntryFind (FS_VOL           *p_vol,
                                       FS_BUF           *p_buf,
                                       CPU_CHAR         *name,
                                       CPU_CHAR        **p_name_next,
                                       FS_FAT_DIR_POS   *p_dir_start_pos,
                                       FS_FAT_DIR_POS   *p_dir_end_pos,
                                       FS_ERR           *p_err)
{
    CPU_BOOLEAN        chk_sfn;
    FS_FAT_DIR_POS     dir_cur_pos;
    FS_FAT_DIR_POS     dir_end_pos;
    CPU_BOOLEAN        dir_sec_valid;
    CPU_BOOLEAN        ext_lower_case;
    CPU_INT32U         name_8_3[3];
    FS_FILE_NAME_LEN   name_len;
    FS_FILE_NAME_LEN   name_len_octet;
    FS_FILE_NAME_LEN   name_len_found;
    FS_FAT_LFN_CHAR    name_lfn[FS_FAT_MAX_FILE_NAME_LEN + 1u];
    CPU_INT32S         name_lfn_cmp;
    CPU_BOOLEAN        name_lower_case;
    CPU_INT32U         name_word;
    CPU_INT08U        *p_dir_entry;
    FS_FAT_DATA       *p_fat_data;


    p_dir_end_pos->SecNbr = 0u;                                 /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos = 0u;

    FS_FAT_SFN_Chk(name,                                        /* Chk if valid SFN.                                    */
                  &name_len,
                   p_err);

    if (*p_err == FS_ERR_NONE) {
#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr((void *)name_8_3, sizeof(name_8_3));
#endif
        FS_FAT_SFN_Create(name,                                 /* Create 8.3 name.                                     */
                         &name_8_3[0],
                         &name_lower_case,
                         &ext_lower_case,
                          p_err);

        if (*p_err == FS_ERR_NAME_MIXED_CASE) {
            *p_err  = FS_ERR_NONE;
        }
    }
    chk_sfn = (*p_err == FS_ERR_NONE) ? DEF_YES : DEF_NO;       /* Cmp with SFNs if valid SFN.                          */


    FS_FAT_LFN_Chk( name,                                       /* Chk if valid LFN.                                    */
                   &name_len,
                   &name_len_octet,
                    p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_name_next = name;
         return;
    }

   *p_name_next        =  name + name_len_octet;

    p_fat_data         = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_cur_pos.SecNbr =  p_dir_start_pos->SecNbr;
    dir_cur_pos.SecPos =  p_dir_start_pos->SecPos;
    dir_sec_valid      =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #3).                    */
        FS_FAT_LFN_NextDirEntryGet(         p_vol,
                                            p_buf,
                                   (void *)&name_lfn[0],
                                           &dir_cur_pos,
                                           &dir_end_pos,
                                            p_err);

        switch (*p_err) {
            case FS_ERR_NONE:
                 break;

            case FS_ERR_EOF:
            case FS_ERR_DIR_FULL:
                *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;
                 return;

            case FS_ERR_DEV:
                 return;

            default:
                 FS_TRACE_DBG(("FS_FAT_LFN_DirEntryFind(): Default case reached.\r\n"));
                 return;
        }



                                                                /* ---------------------- CHK LFN --------------------- */
        p_dir_entry    = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
        name_len_found = FS_FAT_LFN_StrLen_N(name_lfn, FS_FAT_MAX_FILE_NAME_LEN);
        if (name_len == name_len_found) {                       /* See Note #2.                                         */
            name_lfn_cmp = FS_FAT_LFN_StrCmpIgnoreCase_N(name, &name_lfn[0], name_len);
            if (name_lfn_cmp == 0) {
                p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;
                p_dir_start_pos->SecPos = dir_cur_pos.SecPos;
                p_dir_end_pos->SecNbr   = dir_end_pos.SecNbr;
                p_dir_end_pos->SecPos   = dir_end_pos.SecPos;
                return;
            }
        }



                                                                /* ---------------------- CHK SFN --------------------- */
        if (chk_sfn == DEF_YES) {
            name_word = MEM_VAL_GET_INT32U((void *)(p_dir_entry + 0u));
            if (name_word == name_8_3[0]) {
                name_word = MEM_VAL_GET_INT32U((void *)(p_dir_entry + 4u));
                if (name_word == name_8_3[1]) {
                    name_word  = MEM_VAL_GET_INT32U((void *)(p_dir_entry + 8u));
                   #if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE)
                    name_word &= 0x00FFFFFFu;
                   #else
                    name_word &= 0xFFFFFF00u;
                   #endif
                    if (name_word == name_8_3[2]) {
                        p_dir_start_pos->SecNbr = dir_end_pos.SecNbr;
                        p_dir_start_pos->SecPos = dir_end_pos.SecPos;
                        p_dir_end_pos->SecNbr   = dir_end_pos.SecNbr;
                        p_dir_end_pos->SecPos   = dir_end_pos.SecPos;
                        return;
                    }
                }
            }
        }



                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
        dir_cur_pos.SecNbr = dir_end_pos.SecNbr;
        dir_cur_pos.SecPos = dir_end_pos.SecPos + FS_FAT_SIZE_DIR_ENTRY;
                                                                /* Chk sec validity (see Note #3).                      */
        dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_end_pos.SecNbr);
    }


                                                                /* Invalid sec found (see Note #3).                     */
    FS_TRACE_DBG(("FS_FAT_LFN_DirEntryFind(): Invalid sec gotten: %d.\r\n", dir_end_pos.SecNbr));
   *p_err = FS_ERR_ENTRY_CORRUPT;
}


/*
*********************************************************************************************************
*                                    FS_FAT_LFN_NextDirEntryGet()
*
* Description : Get next LFN dir entry from dir.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name                String that will receive entry name.
*                                       OR
*                                   Pointer to NULL.
*
*               p_dir_start_pos     Pointer to directory position at which search should start; variable
*                                   will receive the directory position at which the first entry (the
*                                   first LFN entry or the SFN entry) is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the final entry (the SFN entry) is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE    Directory entry gotten.
*                                       FS_ERR_DEV     Device access error.
*                                       FS_ERR_EOF     End of directory reached.
*
* Return(s)   : none.
*
* Note(s)     : (1) The lower nibble of the first byte of a LFN directory entry is the index of that
*                   entry in the group of LFN entries for the SFN.  The first LFN entry (i.e., the one
*                   located in the lowest position of the lowest sector) contains the final characters
*                   of the LFN & has the highest index.  Additionally, the first LFN entry will have the
*                   sixth bit of the first byte set.
*
*               (2) The checksum of a LFN becomes invalid if the SFN is modified or deleted by a LFN-
*                   ignorant system.  In the case of an invalid checksum, the SFN is the correct name.
*
*               (3) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*
*               (4) Windows stores case information for the short file name in the NTRes byte of the
*                   directory entry :
*                   (a) If bit 3 is set, the name characters are lower case.
*                   (b) If bit 4 is set, the extension characters are lower case.
*
*                   Names of entries created by systems that do not appropriate set NTRes bits upon entry
*                   creation will not be returned in the intended case; however, since all file name
*                   comparisons are caseless, driver operation is unaffected.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_NextDirEntryGet (FS_VOL           *p_vol,
                                          FS_BUF           *p_buf,
                                          void             *name,
                                          FS_FAT_DIR_POS   *p_dir_start_pos,
                                          FS_FAT_DIR_POS   *p_dir_end_pos,
                                          FS_ERR           *p_err)
{
    CPU_INT08U         attrib;
    CPU_INT08U         chksum;
    CPU_INT08U         chksum_calc;
    FS_FLAGS           chksum_correctly_set;
    FS_FAT_DIR_POS     dir_cur_pos;
    FS_FAT_SEC_NBR     dir_next_sec;
    CPU_BOOLEAN        dir_sec_valid;
    CPU_BOOLEAN        ext_lower_case;
    CPU_BOOLEAN        has_lfn;
    CPU_BOOLEAN        lfn_seq_nbr_invalid;
    CPU_INT08U         lfn_entry_cnt;
    CPU_BOOLEAN        lfn_entry_first;
    CPU_INT08U         lfn_seq_nbr;
    FS_FILE_NAME_LEN   lfn_len;
    FS_FILE_NAME_LEN   lfn_pos;
    CPU_BOOLEAN        long_name;
    CPU_INT08U         name_char;
    CPU_BOOLEAN        name_lower_case;
    CPU_INT08U         ntres;
    FS_FAT_LFN_CHAR   *p_name_char;
    FS_FAT_DATA       *p_fat_data;
    CPU_INT08U        *p_dir_entry;



    p_dir_end_pos->SecNbr =  0u;                                /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos =  0u;

    chksum                =  0u;
    chksum_correctly_set  =  DEF_NO;

    lfn_entry_cnt         =  0u;
    has_lfn               =  DEF_NO;

    p_name_char           = (FS_FAT_LFN_CHAR *)name;
    p_fat_data            = (FS_FAT_DATA     *)p_vol->DataPtr;
    p_dir_entry           = (CPU_INT08U      *)p_buf->DataPtr;
    p_dir_entry          +=  p_dir_start_pos->SecPos;

    dir_cur_pos.SecNbr    =  p_dir_start_pos->SecNbr;
    dir_cur_pos.SecPos    =  p_dir_start_pos->SecPos;
    dir_sec_valid         =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    if (dir_sec_valid == DEF_NO) {
                                                                /* Invalid sec found (see Note #3).                     */
        FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): Invalid sec gotten: %d.\r\n", dir_cur_pos.SecNbr));
       *p_err = FS_ERR_ENTRY_CORRUPT;
        return;
    }

    FSBuf_Set(p_buf,
              dir_cur_pos.SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    while (DEF_YES) {
                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {        /* If whole sector has been read                        */
                                                                /* Get next sector                                     */
            dir_next_sec = FS_FAT_SecNextGet(p_vol,
                                             p_buf,
                                             dir_cur_pos.SecNbr,
                                             p_err);

            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                case FS_ERR_DIR_FULL:
                     p_dir_end_pos->SecNbr = dir_cur_pos.SecNbr;
                     p_dir_end_pos->SecPos = dir_cur_pos.SecPos;
                    *p_err = FS_ERR_EOF;
                     return;

                case FS_ERR_DEV:
                default:
                     return;
            }

                                                                /* Chk sec validity (see Note #3).                      */
            dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_next_sec);

            dir_cur_pos.SecNbr = dir_next_sec;
            dir_cur_pos.SecPos = 0u;

            if (dir_sec_valid == DEF_YES) {
                p_dir_entry = (CPU_INT08U *)p_buf->DataPtr;

                FSBuf_Set(p_buf,
                          dir_cur_pos.SecNbr,
                          FS_VOL_SEC_TYPE_DIR,
                          DEF_YES,
                          p_err);

                if (*p_err != FS_ERR_NONE) {
                    return;
                }

            } else {
                                                                /* Invalid sec found (see Note #3).                     */
                FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): Invalid sec gotten: %d.\r\n", dir_cur_pos.SecNbr));
               *p_err = FS_ERR_ENTRY_CORRUPT;
                return;
            }

        }


                                                                /* ------------- CHK IF LAST ENTRY IN DIR ------------- */
        name_char = *p_dir_entry;
        if (name_char == FS_FAT_DIRENT_NAME_FREE) {             /* If dir entry free & all subsequent entries free ...  */
            p_dir_end_pos->SecNbr = dir_cur_pos.SecNbr;
            p_dir_end_pos->SecPos = dir_cur_pos.SecPos;
           *p_err = FS_ERR_EOF;                                 /* Could not find another entry                         */
            return;                                             /* ... rtn NULL ptr.                                    */
        }





        if (name_char == FS_FAT_DIRENT_NAME_ERASED_AND_FREE) {  /* If dir entry free but not last.                      */

            lfn_entry_cnt = 0u;
            has_lfn       = DEF_NO;

        } else {                                                /* If dir entry NOT free.                               */

            attrib = *(p_dir_entry + FS_FAT_LFN_OFF_ATTR);
            long_name = FS_FAT_DIRENT_ATTR_IS_LONG_NAME(attrib);/* LFN attribute should always be 0x0F                  */

                                                                /* --------------------- LFN ENTRY -------------------- */
            if (long_name == DEF_YES) {                         /* If dir entry is LFN entry                            */

                                                                /* Sequence number of lfn entry                         */
                lfn_seq_nbr = (name_char & FS_FAT_DIRENT_NAME_LONG_ENTRY_MASK);

                lfn_seq_nbr_invalid = (lfn_seq_nbr == 0u) || (lfn_seq_nbr > 20u);
                if (lfn_seq_nbr_invalid == DEF_YES) {           /* If sequence number is not valid                      */
                    lfn_entry_cnt = 0u;
                    has_lfn       = DEF_NO;
                    FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): LFN entry has an invalid sequence number: %d.\r\n", lfn_seq_nbr));
                } else {
                                                                /* 1st LFN entry.                                       */
                                                                /* 1st entry to be read is also the last LFN entry      */
                                                                /* Bit 7 of the sequence number should be set           */
                    lfn_entry_first = DEF_BIT_IS_SET(name_char, FS_FAT_DIRENT_NAME_LAST_LONG_ENTRY);
                    if (lfn_entry_first == DEF_YES) {

                        has_lfn              =   DEF_NO;
                        lfn_entry_cnt        =   lfn_seq_nbr;
                        chksum               = *(p_dir_entry + FS_FAT_LFN_OFF_CHKSUM);
                        chksum_correctly_set =   DEF_YES;

                                                                /* Number of character in this LFN entry                */
                        lfn_len = FS_FAT_LFN_Len((void *)p_dir_entry);
                                                                /* Start position of theses characters in the file name */
                        lfn_pos = ((FS_FILE_NAME_LEN)lfn_entry_cnt - 1u) * FS_FAT_DIRENT_LFN_NBR_CHARS;

                                                                /* If file name is shorter than or equal to max length  */
                        if (lfn_len + lfn_pos <= FS_FAT_MAX_FILE_NAME_LEN) {

                                                                /* If p_name_char is NOT a NULL pointer                 */
                            if (p_name_char != (FS_FAT_LFN_CHAR *)0) {
                                                                /* Parse LFN entry into LFN name buf.                   */
#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
                                Mem_Clr((void *)p_name_char, FS_FAT_MAX_FILE_NAME_LEN);
#endif
                                FS_FAT_LFN_Parse((void *) p_dir_entry,
                                                         &p_name_char[lfn_pos],
                                                          lfn_len);
                                p_name_char[lfn_pos + lfn_len] =(FS_FAT_LFN_CHAR)ASCII_CHAR_NULL;
                            }
                            lfn_entry_cnt--;

                                                                /* If all LFN entries have been read successfully       */
                            if (lfn_pos == 0u) {
                                has_lfn = DEF_YES;
                            }
                            p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;
                            p_dir_start_pos->SecPos = dir_cur_pos.SecPos;

                                                                /* If file name is longer than max length               */
                        } else {

                            FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): LFN w/len %d > MAX_LEN = %d found.\r\n", lfn_len + lfn_pos, FS_FAT_MAX_FILE_NAME_LEN));
                            lfn_entry_cnt = 0u;

                        }


                                                                /* LFN entry, not 1st.                                  */
                    } else if (lfn_entry_cnt == lfn_seq_nbr) {
                        lfn_pos = ((FS_FILE_NAME_LEN)lfn_entry_cnt - 1u) * FS_FAT_DIRENT_LFN_NBR_CHARS;

                                                                /* If p_name_char is NOT a NULL pointer                 */
                        if (p_name_char != (FS_FAT_LFN_CHAR *)0) {
                                                                /* Parse LFN entry into LFN name buf.                   */
                            FS_FAT_LFN_ParseFull((void *) p_dir_entry,
                                                         &p_name_char[lfn_pos]);
                        }
                        lfn_entry_cnt--;

                                                                /* If all LFN entries have been read successfully       */
                        if (lfn_pos == 0u) {
                            has_lfn = DEF_YES;
                        }

                                                                /* Orphaned LFN entry?                                  */
                                                                /* If LFN entry sequence number invalid                 */
                    } else {
                        FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): Orphaned LFN entries? starting at %08X.%04X\r\n", dir_cur_pos.SecNbr, dir_cur_pos.SecPos));
                        lfn_entry_cnt = 0u;
                        has_lfn       = DEF_NO;
                    }

                }


                                                                /* --------------------- SFN ENTRY -------------------- */
                                                                /* If dir entry is SFN entry                            */
            } else if (DEF_BIT_IS_CLR(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                if (has_lfn == DEF_YES) {                       /* If SFN entry has matching LFN entries                */

                    chksum_calc     = FS_FAT_LFN_ChkSumCalc((void *)p_dir_entry);

                    if (chksum_correctly_set == DEF_YES) {
                        if (chksum_calc != chksum) {
                            FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): LFN chksum mismatch for LFN %s: %d != %d.\r\n", (CPU_CHAR *)p_name_char, chksum_calc, chksum));
                            has_lfn = DEF_NO;
                        }
                    } else {
                        FS_TRACE_DBG(("FS_FAT_LFN_NextDirEntryGet(): Orphaned LFN entries? starting at %08X.%04X\r\n", dir_cur_pos.SecNbr, dir_cur_pos.SecPos));
                    }

                }

                if (has_lfn == DEF_NO) {                        /* If SFN entry does not have matching LFN entries      */
                    p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;
                    p_dir_start_pos->SecPos = dir_cur_pos.SecPos;

                    if (p_name_char != (FS_FAT_LFN_CHAR *)0) {
                                                                /* Apply case (see Note #4).                            */
                        ntres           = MEM_VAL_GET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_NTRES));
                        name_lower_case = DEF_BIT_IS_SET(ntres, FS_FAT_DIRENT_NTRES_NAME_LOWER_CASE);
                        ext_lower_case  = DEF_BIT_IS_SET(ntres, FS_FAT_DIRENT_NTRES_EXT_LOWER_CASE);
#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
                        Mem_Clr((void *)p_name_char, FS_FAT_MAX_FILE_NAME_LEN);
#endif
                        FS_FAT_LFN_SFN_Parse((void *)p_dir_entry,
                                                     p_name_char,
                                                     name_lower_case,
                                                     ext_lower_case);
                    }
                }
                p_dir_end_pos->SecNbr = dir_cur_pos.SecNbr;
                p_dir_end_pos->SecPos = dir_cur_pos.SecPos;
               *p_err = FS_ERR_NONE;
                return;
                                                                /* If entry is not a file entry                         */
            } else {
                lfn_entry_cnt = 0u;
                has_lfn       = DEF_NO;
            }

        }



                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
        dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
        p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;

    }


}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FS_FAT_LFN_Chk()
*
* Description : Check whether file name is valid LFN.
*
* Argument(s) : name                Name of the entry.
*
*               p_name_len          Pointer to variable that will receive the length of the file name, in characters.
*
*               p_name_len_octet    Pointer to variable that will receive the length of the file name, in octets.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE            File name valid.
*                                       FS_ERR_NAME_INVALID    File name invalid.
*
* Return(s)   : none.
*
* Note(s)     : (1) If 'name' is not NULL, then it must point to an array of 'FS_FAT_MAX_FILE_NAME_LEN + 1'
*                   characters.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_Chk (CPU_CHAR          *name,
                              FS_FILE_NAME_LEN  *p_name_len,
                              FS_FILE_NAME_LEN  *p_name_len_octet,
                              FS_ERR            *p_err)
{
    CPU_BOOLEAN       is_legal;
    FS_FAT_LFN_CHAR   name_char;
    CPU_SIZE_T        name_char_len;
    FS_FILE_NAME_LEN  name_len;
    FS_FILE_NAME_LEN  name_len_octet;


    is_legal         = DEF_YES;
    name_len         = 0u;
    name_len_octet   = 0u;
   *p_name_len       = 0u;
   *p_name_len_octet = 0u;


                                                                /* ------------------- CHK INIT CHAR ------------------ */
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    name_char_len = MB_CharToWC(&name_char,
                                 name,
                                 MB_MAX_LEN);

    if (name_char_len > MB_MAX_LEN) {
        name_char = (FS_FAT_LFN_CHAR)ASCII_CHAR_NULL;
    }
#else
    name_char_len = 1u;
    name_char     = *name;
#endif

    switch (name_char) {                                        /* Rtn err if first char is ' ', '\0', '\'.             */
        case ASCII_CHAR_SPACE:
        case ASCII_CHAR_NULL:
        case FS_FAT_PATH_SEP_CHAR:
             FS_TRACE_DBG(("FS_FAT_LFN_Process(): Invalid first char.\n"));
            *p_err = FS_ERR_NAME_INVALID;
             return;

        default:
             break;
    }


                                                                /* ------------------- PROCESS PATH ------------------- */
    while ((name_char != (CPU_CHAR)ASCII_CHAR_NULL     ) &&     /* Process str until NULL char ...                      */
           (name_char != (CPU_CHAR)FS_FAT_PATH_SEP_CHAR) &&     /* ... or path sep        char ...                      */
           (is_legal  ==  DEF_YES                      )) {     /* ... or illegal name is found.                        */

        if (name_len <= FS_FAT_MAX_FILE_NAME_LEN) {
            is_legal = FS_FAT_IS_LEGAL_LFN_CHAR(name_char);
            if (is_legal == DEF_YES) {
                name_len_octet += (FS_FILE_NAME_LEN)name_char_len;
                name_len++;
            } else {
                FS_TRACE_DBG(("FS_FAT_LFN_Process(): Invalid character in file name: %c (0x%02X).\r\n", name_char, name_char));
                is_legal = DEF_NO;
            }
        } else {                                                /* Name illegal if file name too long.                  */
            FS_TRACE_DBG(("FS_FAT_LFN_Process(): File name too long.\r\n"));
            is_legal = DEF_NO;
        }
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
        name         += name_char_len;

        name_char_len = MB_CharToWC(&name_char,
                                     name,
                                     MB_MAX_LEN);

        if (name_char_len > MB_MAX_LEN) {
            name_char = (FS_FAT_LFN_CHAR)ASCII_CHAR_NULL;
        }
#else
        name++;
        name_char = *name;
#endif
    }


                                                                /* ------------------- ASSIGN & RTN ------------------- */
    if (is_legal == DEF_NO) {                                   /* Rtn err if illegal name.                             */
       *p_err = FS_ERR_NAME_INVALID;
        return;
    }

   *p_name_len       = name_len;
   *p_name_len_octet = name_len_octet;
   *p_err            = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FS_FAT_LFN_ChkSumCalc()
*
* Description : Calculate LFN checksum.
*
* Argument(s) : name_8_3    Entry SFN.
*
* Return(s)   : LFN checksum
*
* Note(s)     : (1) The checksum is calculated according to the algorithm given in [Ref 1], page 28.
*********************************************************************************************************
*/

static  CPU_INT08U  FS_FAT_LFN_ChkSumCalc (void  *name_8_3)
{
    CPU_INT08U   chk_sum;
    CPU_INT16U   name_8_3_len;
    CPU_INT08U  *name_8_3_08;


    chk_sum      = 0u;
    name_8_3_len = 11u;
    name_8_3_08  = (CPU_INT08U *)name_8_3;

    while (name_8_3_len >= 1u) {
        chk_sum = ((DEF_BIT_IS_SET(chk_sum, DEF_BIT_00) == DEF_YES) ? DEF_BIT_07 : DEF_BIT_NONE) + (chk_sum >> 1) + *name_8_3_08;
        name_8_3_08++;
        name_8_3_len--;
    }

    return (chk_sum);
}


/*
*********************************************************************************************************
*                                      FS_FAT_LFN_DirEntryFmt()
*
* Description : Make directory entry for LFN entry.
*
* Argument(s) : p_dir_entry     Pointer to buffer that will receive the directory entry.
*
*               name            Entry name.
*
*               char_cnt        Number of characters to write.
*
*               chk_sum         Checksum of LFN entry.
*
*               lfn_nbr         LFN sequence number.
*
*               is_final        Indicates whether LFN entry is the final LFN entry :
*
*                                   DEF_YES, if the LFN entry is     the final LFN entry.
*                                   DEF_NO,  if the LFN entry is NOT the final LFN entry.
*
* Return(s)   : none.
*
* Note(s)     : (1) The LFN directory entry format is specified in [Ref. 1], Page 28 :
*
*                   (a) The 0th byte contains the sequence number of the LFN entry in the LFN, starting
*                       at 1.  The final LFN entry for the LFN (which is stored in the lowest on-disk
*                       location) has its 6th bit set.
*
*                   (b) Up to 13 Unicode characters are stored in each LFN entry.  If the final LFN entry
*                       for the LFN has fewer than 13 characters (neglecting a NULL character), then a
*                       NULL character is placed following the final LFN character.  Remaining character
*                       slots, if any, are filled with 0xFFFF.
*
*                       ---------------------------------------------------------------------------------
*                       |    |   #1    |   #2    |   #3    |   #4    |   #5    |    |    |    |   #6    |
*                       ---------------------------------------------------------------------------------
*                       |   #7    |   #8    |   #9    |   #10   |   #11   |         |   #12   |   #13   |
*                       ---------------------------------------------------------------------------------
*
*                   (c) The 11th byte contains the LFN entry attributes, 0x0F.
*
*                   (d) The 12th, 27th & 28th bytes are NULL.
*
*                   (e) The 13th byte contains the LFN entry checksum.  (See 'FS_FAT_LFN_ChkSumCalc()
*                       Note #2').
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryFmt (void                  *p_dir_entry,
                                      FS_FAT_LFN_CHAR       *name,
                                      FS_FILE_NAME_LEN       char_cnt,
                                      CPU_INT08U             chk_sum,
                                      FS_FAT_DIR_ENTRY_QTY   lfn_nbr,
                                      CPU_BOOLEAN            is_final)
{
    FS_FILE_NAME_LEN   char_cnt_wr;
    CPU_INT08U         marker;
    CPU_INT08U         i;
    CPU_INT08U        *p_dir_entry_08;


    char_cnt_wr    = 0u;
    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;


                                                                /* ----------------- LONG ENTRY MARKER ---------------- */
    marker         = (is_final == DEF_YES) ? (lfn_nbr | FS_FAT_DIRENT_NAME_LAST_LONG_ENTRY)
                                           : (lfn_nbr);
   *p_dir_entry_08 = marker;
    p_dir_entry_08++;



                                                                /* ----------------- NAME CHARS 1 - 5 ----------------- */
    for (i = 1u; i <= 5u; i++) {
        if (char_cnt_wr > char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_HI;
            p_dir_entry_08++;
        } else if (char_cnt_wr == char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_HI;
            p_dir_entry_08++;
        } else {
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
#else
           *p_dir_entry_08 = (CPU_INT08U)*name;
            p_dir_entry_08++;
           *p_dir_entry_08 = (CPU_INT08U) ASCII_CHAR_NULL;
            p_dir_entry_08++;
#endif
            name++;
        }
        char_cnt_wr++;
    }



                                                                /* ---------------- DIR ENTRY ATTRIB'S ---------------- */
   *p_dir_entry_08 = FS_FAT_DIRENT_ATTR_LONG_NAME;              /* Long file name.                                      */
    p_dir_entry_08++;
   *p_dir_entry_08 = 0x00u;                                     /* NULL byte.                                           */
    p_dir_entry_08++;
   *p_dir_entry_08 = chk_sum;                                   /* Checksum.                                            */
    p_dir_entry_08++;



                                                                /* ----------------- NAME CHARS 6 - 11 ---------------- */
    for (i = 6u; i <= 11u; i++) {
        if (char_cnt_wr > char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_HI;
            p_dir_entry_08++;
        } else if (char_cnt_wr == char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_HI;
            p_dir_entry_08++;
        } else {
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
#else
           *p_dir_entry_08 = (CPU_INT08U)*name;
            p_dir_entry_08++;
           *p_dir_entry_08 = (CPU_INT08U) ASCII_CHAR_NULL;
            p_dir_entry_08++;
#endif
            name++;
        }
        char_cnt_wr++;
    }



                                                                /* -------------------- NULL BYTES -------------------- */
   *p_dir_entry_08 = 0x00u;
    p_dir_entry_08++;
   *p_dir_entry_08 = 0x00u;
    p_dir_entry_08++;



                                                                /* ---------------- NAME CHARS 12 - 13 ---------------- */
    for (i = 12u; i <= 13u; i++) {
        if (char_cnt_wr > char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_EMPTY_HI;
            p_dir_entry_08++;
        } else if (char_cnt_wr == char_cnt) {
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_LO;
            p_dir_entry_08++;
           *p_dir_entry_08 = FS_FAT_LFN_CHAR_FINAL_HI;
            p_dir_entry_08++;
        } else {
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
           *p_dir_entry_08 = ((CPU_INT08U)(*name >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK);
            p_dir_entry_08++;
#else
           *p_dir_entry_08 = (CPU_INT08U)*name;
            p_dir_entry_08++;
           *p_dir_entry_08 = (CPU_INT08U) ASCII_CHAR_NULL;
            p_dir_entry_08++;
#endif
            name++;
        }
        char_cnt_wr++;
    }
}
#endif


/*
*********************************************************************************************************
*                                     FS_FAT_LFN_DirEntryPlace()
*
* Description : Find consecutive directory entries for placement of LFN.
*
* Argument(s) : p_vol           Pointer to volume.
*
*               p_buf           Pointer to temporary buffer.
*
*               dir_entry_cnt   Number of consecutive directory entries required.
*
*               dir_start_sec   Directory sector in which entries should be located.
*
*               p_dir_end_pos   Pointer to variable that will receive the directory position at which
*                               entries can be located.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE             Directory entries found.
*                                   FS_ERR_DEV              Device access error.
*                                   FS_ERR_DIR_FULL         Directory is full (space could not be allocated).
*                                   FS_ERR_DEV_FULL         Device is full (space could not be allocated).
*                                   FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*
*               (2) The sector number gotten or allocated from the FAT should be valid.  These checks are
*                   effectively redundant.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryPlace (FS_VOL                *p_vol,
                                        FS_BUF                *p_buf,
                                        FS_FAT_DIR_ENTRY_QTY   dir_entry_cnt,
                                        FS_FAT_SEC_NBR         dir_start_sec,
                                        FS_FAT_DIR_POS        *p_dir_end_pos,
                                        FS_ERR                *p_err)
{
    CPU_BOOLEAN            chk;
    FS_FAT_DIR_POS         dir_cur_pos;
    FS_FAT_DIR_POS         dir_cur_start_pos;
    FS_FAT_DIR_ENTRY_QTY   dir_entry_free_cnt;
    CPU_BOOLEAN            dir_sec_valid;
    CPU_INT08U            *p_dir_entry;
    FS_FAT_DATA           *p_fat_data;


    p_dir_end_pos->SecNbr    =  0u;
    p_dir_end_pos->SecPos    =  0u;

    p_fat_data               = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_entry_free_cnt       =  0u;
    dir_cur_pos.SecNbr       =  dir_start_sec;
    dir_cur_pos.SecPos       =  0u;
    dir_cur_start_pos.SecNbr =  0u;
    dir_cur_start_pos.SecPos =  0u;
    dir_sec_valid            =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);



    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #2).                    */
                                                                /* -------------------- CHK JOURNAL ------------------- */
        chk = DEF_YES;
                                                                /* -------------------- RD DIR SEC -------------------- */
        if (chk == DEF_YES) {
            FSBuf_Set(p_buf,
                      dir_cur_pos.SecNbr,
                      FS_VOL_SEC_TYPE_DIR,
                      DEF_YES,
                      p_err);
            if (*p_err != FS_ERR_NONE) {
                 return;
            }



                                                                /* ----------- CNT CONSECUTIVE EMPTY ENTRIES ---------- */
            dir_cur_pos.SecPos = 0u;
            p_dir_entry        = (CPU_INT08U *)p_buf->DataPtr;
            while (dir_cur_pos.SecPos < p_fat_data->SecSize) {

                if ((*p_dir_entry == FS_FAT_DIRENT_NAME_ERASED_AND_FREE) ||
                    (*p_dir_entry == FS_FAT_DIRENT_NAME_FREE)) {   /* Free entry found ... inc cnt.                    */

                    if (dir_entry_free_cnt == 0u) {
                        dir_cur_start_pos.SecNbr = dir_cur_pos.SecNbr;
                        dir_cur_start_pos.SecPos = dir_cur_pos.SecPos;
                    }

                    dir_entry_free_cnt++;

                    if (dir_entry_free_cnt >= dir_entry_cnt) {
                        p_dir_end_pos->SecNbr = dir_cur_start_pos.SecNbr;
                        p_dir_end_pos->SecPos = dir_cur_start_pos.SecPos;
                        return;
                    }
                } else {                                            /* Occupied entry ... reset cnt.                    */
                    dir_entry_free_cnt = 0u;
                }

                p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
                dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
            }
        }



                                                                /* -------------------- GET NEXT SEC ------------------ */
        dir_cur_pos.SecNbr = FS_FAT_SecNextGetAlloc(p_vol,
                                                    p_buf,
                                                    dir_cur_pos.SecNbr,
                                                    DEF_YES,
                                                    p_err);

        switch (*p_err) {
            case FS_ERR_NONE:
                                                                /* Chk sec validity (see Note #2).                      */
                 dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);
                 break;

            case FS_ERR_DIR_FULL:
            case FS_ERR_DEV_FULL:
                 return;

            case FS_ERR_SYS_CLUS_CHAIN_END:
            case FS_ERR_SYS_CLUS_INVALID:
                *p_err = FS_ERR_ENTRY_CORRUPT;
                 return;

            case FS_ERR_DEV:
            default:
                 return;
        }
    }


                                                                /* Invalid sec found (see Note #2).                     */
    FS_TRACE_DBG(("FS_FAT_LFN_DirEntryPlace(): Invalid sec alloc'd: %d.\r\n", dir_cur_pos.SecNbr));
   *p_err = FS_ERR_ENTRY_CORRUPT;
    return;
}
#endif


/*
*********************************************************************************************************
*                                          FS_FAT_LFN_Len()
*
* Description : Calculate length of LFN name in LFN entry.
*
* Argument(s) : p_dir_entry     Pointer to directory entry.
*
* Return(s)   : Number of characters in name.
*
* Note(s)     : (1) Length does not include final NULL (0x0000) character
*********************************************************************************************************
*/

static  FS_FILE_NAME_LEN  FS_FAT_LFN_Len (void  *p_dir_entry)
{
    CPU_INT08U         byte1;
    CPU_INT08U         byte2;
    CPU_INT08U         i;
    FS_FILE_NAME_LEN   lfn_len;
    CPU_INT08U        *p_dir_entry_08;


    lfn_len        = 0u;
    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;
    p_dir_entry_08++;

    for (i = 1u; i <= 5u; i++) {                                /* Chk chars 1-5.                                       */
        byte1 = *p_dir_entry_08;
        p_dir_entry_08++;
        byte2 = *p_dir_entry_08;
        p_dir_entry_08++;
        if ((byte1 == FS_FAT_LFN_CHAR_FINAL_HI) && (byte2 == FS_FAT_LFN_CHAR_FINAL_LO)) {
            return (lfn_len);
        }
        lfn_len++;
    }

    p_dir_entry_08 += 3u;
    for (i = 6u; i <= 11u; i++) {                               /* Chk chars 6-11.                                      */
        byte1 = *p_dir_entry_08;
        p_dir_entry_08++;
        byte2 = *p_dir_entry_08;
        p_dir_entry_08++;
        if ((byte1 == FS_FAT_LFN_CHAR_FINAL_HI) && (byte2 == FS_FAT_LFN_CHAR_FINAL_LO)) {
            return (lfn_len);
        }
        lfn_len++;
    }

    p_dir_entry_08 += 2u;
    for (i = 12u; i <= 13u; i++) {                              /* Chk chars 12-13.                                     */
        byte1 = *p_dir_entry_08;
        p_dir_entry_08++;
        byte2 = *p_dir_entry_08;
        p_dir_entry_08++;
        if ((byte1 == FS_FAT_LFN_CHAR_FINAL_HI) && (byte2 == FS_FAT_LFN_CHAR_FINAL_LO)) {
            return (lfn_len);
        }
        lfn_len++;
    }

    return (lfn_len);
}


/*
*********************************************************************************************************
*                                         FS_FAT_LFN_Parse()
*
* Description : Read characters of LFN name in LFN entry.
*
* Argument(s) : p_dir_entry     Pointer to directory entry.
*
*               name            String that will receive entry name characters.
*
*               name_len        Number of characters in directory entry.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_LFN_DirEntryFmt() Note #2'.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_Parse (void              *p_dir_entry,
                                FS_FAT_LFN_CHAR   *name,
                                FS_FILE_NAME_LEN   name_len)
{
    CPU_INT08U         i;
    CPU_INT08U        *p_dir_entry_08;
    FS_FILE_NAME_LEN   name_len_rem;
    FS_FAT_LFN_CHAR    name_char;


    name_len_rem   =  name_len;
    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;
    p_dir_entry_08++;

#if (FS_CFG_UTF8_EN == DEF_ENABLED)                             /* ----------------- PARSE AS UNICODE ----------------- */
    for (i = 1u; i <= 5u; i++) {                                /* Parse chars 1-5.                                     */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }

    p_dir_entry_08 += 3u;
    for (i = 6u; i <= 11u; i++) {                               /* Parse chars 6-11.                                    */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }

    p_dir_entry_08 += 2u;
    for (i = 12u; i <= 13u; i++) {                              /* Chk chars 12-13.                                     */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }


#else                                                           /* ------------------ PARSE AS ASCII ------------------ */
    for (i = 1u; i <= 5u; i++) {                                /* Parse chars 1-5.                                     */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }

    p_dir_entry_08 += 3u;
    for (i = 6u; i <= 11u; i++) {                               /* Parse chars 6-11.                                    */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }

    p_dir_entry_08 += 2u;
    for (i = 12u; i <= 13u; i++) {                              /* Chk chars 12-13.                                     */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
        if (name_len_rem == 1u) {
            return;
        }
        name_len_rem--;
    }
#endif
}


/*
*********************************************************************************************************
*                                       FS_FAT_LFN_ParseFull()
*
* Description : Read all characters of LFN name in LFN entry.
*
* Argument(s) : p_dir_entry     Pointer to directory entry.
*
*               name            String that will receive entry name characters.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_LFN_DirEntryFmt() Note #2'.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_ParseFull (void             *p_dir_entry,
                                    FS_FAT_LFN_CHAR  *name)
{
    CPU_INT08U         i;
    CPU_INT08U        *p_dir_entry_08;
    FS_FAT_LFN_CHAR    name_char;


    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;
    p_dir_entry_08++;

#if (FS_CFG_UTF8_EN == DEF_ENABLED)                             /* ----------------- PARSE AS UNICODE ----------------- */
    for (i = 1u; i <= 5u; i++) {                                /* Parse chars 1-5.                                     */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }

    p_dir_entry_08 += 3u;
    for (i = 6u; i <= 11u; i++) {                               /* Parse chars 6-11.                                    */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }

    p_dir_entry_08 += 2u;
    for (i = 12u; i <= 13u; i++) {                              /* Chk chars 12-13.                                     */
        name_char       = ((FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 0u) << (0u * DEF_OCTET_NBR_BITS))
                        |  (FS_FAT_LFN_CHAR)((FS_FAT_LFN_CHAR)*(p_dir_entry_08 + 1u) << (1u * DEF_OCTET_NBR_BITS)));
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }


#else                                                           /* ------------------ PARSE AS ASCII ------------------ */
    for (i = 1u; i <= 5u; i++) {                                /* Parse chars 1-5.                                     */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }

    p_dir_entry_08 += 3u;
    for (i = 6u; i <= 11u; i++) {                               /* Parse chars 6-11.                                    */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }

    p_dir_entry_08 += 2u;
    for (i = 12u; i <= 13u; i++) {                              /* Chk chars 12-13.                                     */
        name_char       = (FS_FAT_LFN_CHAR)*p_dir_entry_08;
       *name            = name_char;
        name++;
        p_dir_entry_08 += 2u;
    }
#endif
}


/*
*********************************************************************************************************
*                                       FS_FAT_LFN_SFN_Alloc()
*
* Description : Allocate SFN.
*
* Argument(s) : p_vol           Pointer to volume.
*
*               p_buf           Pointer to temporary buffer.
*
*               name            Name of the entry.
*
*               name_8_3        Array that will receive the entry SFN.
*
*               p_dir_entry     Pointer to directory entry.
*
*               dir_sec         Directory sector in which file will be located.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                 SFN allocated.
*                                   FS_ERR_SYS_SFN_NOT_AVAIL    No SFN is available.
*
*                                                               ---- RETURNED BY FS_FAT_SFN_DirSrch() ---
*                                   FS_ERR_DEV                  Device access error.
*
*                                                               ---- RETURNED BY FS_FAT_LFN_SFN_Fmt() ---
*                                   FS_ERR_NAME_INVALID         LFN name invalid.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FS_FAT_SFN_Create() Note #1'.
*
*               (2) The basis SFN is formed according to the rules outlined in [Ref 1] (see
*                   'FS_FAT_LFN_SFN_Fmt() Note #2'), with the tail generation as outlined therein as well
*                   (see 'FS_FAT_LFN_SFN_FmtTail() Note #2').
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_SFN_Alloc (FS_VOL          *p_vol,
                                    FS_BUF          *p_buf,
                                    CPU_CHAR        *name,
                                    CPU_INT32U       name_8_3[],
                                    FS_FAT_SEC_NBR   dir_sec,
                                    FS_ERR          *p_err)
{
    CPU_INT16U       char_cnt;
    FS_FAT_DIR_POS   dir_start_pos;
    FS_FAT_DIR_POS   dir_end_pos;
    FS_FAT_SFN_TAIL  tail_nbr;
    FS_FAT_SFN_TAIL  tail_max;



                                                                /* ------------------- FMT INIT NAME ------------------ */
    char_cnt = FS_FAT_LFN_SFN_Fmt(name, name_8_3, p_err);       /* Fmt SFN from LFN.                                    */
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ------------------- FIND MAX SFN ------------------- */
    tail_max = FS_FAT_LFN_SFN_DirEntryFindMax(p_vol,
                                              p_buf,
                                              name_8_3,
                                              dir_sec,
                                              p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (tail_max == 0u) {
        tail_nbr =  1u;
    } else {
        if (tail_max < 999999u) {
            tail_nbr = tail_max + 1u;



        } else {                                                /* -------------- SRCH FOR FREE SFN TAIL -------------- */
            tail_nbr = 1u;

            while (tail_nbr <= 999999u) {
                FS_FAT_LFN_SFN_FmtTail(name_8_3, char_cnt, tail_nbr);

                                                                /* Srch dir for SFN.                                    */
                dir_start_pos.SecNbr = dir_sec;
                dir_start_pos.SecPos = 0u;
                FS_FAT_SFN_SFN_Find( p_vol,
                                     p_buf,
                                     name_8_3,
                                    &dir_start_pos,
                                    &dir_end_pos,
                                     p_err);

                if (*p_err == FS_ERR_SYS_DIR_ENTRY_NOT_FOUND) {
                    break;
                }

                switch (*p_err) {
                    case FS_ERR_NONE:                           /* SFN found, so collision.                             */
                         break;

                    case FS_ERR_DEV:
                         return;

                    default:
                         FS_TRACE_LOG(("FS_FAT_LFN_SFN_Alloc(): Default case reached.\r\n"));
                         return;
                }

                tail_nbr++;                                     /* Inc tail nbr.                                        */
            }

            if (tail_nbr > 999999u) {
               *p_err = FS_ERR_SYS_SFN_NOT_AVAIL;
                return;
            }
        }
    }



                                                                /* ------------------ SFN TAIL FOUND ------------------ */
    FS_FAT_LFN_SFN_FmtTail(name_8_3, char_cnt, tail_nbr);       /* Add tail to SFN.                                     */

    FS_TRACE_LOG(("FS_FAT_LFN_SFN_Alloc(): SFN %s ASSIGNED TO\r\n", (CPU_CHAR *)name_8_3));
    FS_TRACE_LOG(("                            %s\r\n",             (CPU_CHAR *)name));
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FS_FAT_LFN_SFN_Fmt()
*
* Description : Format SFN from LFN.
*
* Argument(s) : name        Name of the entry.
*
*               name_8_3    Array that will receive the entry SFN.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE            SFN formatted.
*                               FS_ERR_NAME_INVALID    LFN name invalid.
*
* Return(s)   : The number of characters in the basis SFN.
*
* Note(s)     : (1) See 'FS_FAT_SFN_Create() Note #1'.
*
*               (2) The basis SFN is formed according to the rules outlined in Microsoft's
*                   'FAT: General Overview of On-Disk Format' :
*
*                   (a) The file name is converted to upper case.
*
*                   (b) If the character is not a valid SFN character, then it is replaced by a '_'.
*
*                   (c) Leading & embedded spaces are skipped.
*
*                   (d) Leading periods are skipped.  The first embedded period ends the primary portion
*                       of the basis name.
*
*                       (1) The final period separates the base name from its extension.  Once the
*                           final period is reached, the following characters will be parsed to form
*                           the extension.
*
*                   (e) The base name may contain as many as 8 characters.
*
*                   (f) The extension may contain as many as 3 characters.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_FILE_NAME_LEN  FS_FAT_LFN_SFN_Fmt (CPU_CHAR    *name,
                                              CPU_INT32U   name_8_3[],
                                              FS_ERR      *p_err)
{
    FS_FILE_NAME_LEN   char_cnt;
    FS_FILE_NAME_LEN   char_cnt_ext;
    FS_FILE_NAME_LEN   i;
    CPU_BOOLEAN        is_legal;
    CPU_BOOLEAN        is_done;
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_WCHAR          name_char;
    CPU_SIZE_T         name_char_len;
#else
    CPU_CHAR           name_char;
#endif
    CPU_CHAR          *p_period;
    CPU_CHAR           name_8_3_08[12];


                                                                /* -------------------- CLR SFN BUF ------------------- */
    for (i = 0u; i < 11u; i++) {                                /* 'Clear' 8.3 file name buffer.                        */
        name_8_3_08[i] = (CPU_INT08U)ASCII_CHAR_SPACE;
    }
    name_8_3_08[11] = (CPU_INT08U)ASCII_CHAR_NULL;             /* 'Clear' final byte of 8.3 file name (see Note #1).  */

    p_period     =  Str_Char_Last_N(name, FS_CFG_MAX_FILE_NAME_LEN, (CPU_CHAR)ASCII_CHAR_FULL_STOP);
    if (p_period == name) {                                     /* If period not found in name ...                      */
        p_period = (CPU_CHAR *)0;                               /* ... set ptr to period invalid.                       */
    }



                                                                /* -------------- COPY PRIMARY FILE NAME -------------- */
    i             =  0u;
    char_cnt      =  0u;
    is_done       =  DEF_NO;
    is_legal      =  DEF_YES;
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    name_char_len = MB_CharToWC( &name_char,
                                  name,
                                  MB_MAX_LEN);
    if (name_char_len > MB_MAX_LEN) {
        name_char = (CPU_WCHAR)ASCII_CHAR_NULL;
    }
#else
    name_char     = *name;
#endif
    while ((is_done  == DEF_NO) &&                              /* While parse is not done ...                          */
           (is_legal == DEF_YES)) {                             /* ... & name is still legal.                           */
        switch (name_char) {
            case ASCII_CHAR_SPACE:                              /* Skip embedded spaces (see Notes #2c).                */
                 break;


            case ASCII_CHAR_FULL_STOP:                          /* End at first embedded periods (see Notes #2d).       */
                 is_done = DEF_YES;
                 break;


            case ASCII_CHAR_NULL:                               /* Exit loop if NULL char or path sep char reached.     */
            case FS_FAT_PATH_SEP_CHAR:
                 is_done = DEF_YES;
                 break;


            default:                                            /* Chk for legal file name char.                        */
                 is_legal = FS_FAT_IS_LEGAL_SFN_CHAR(name_char);
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
                 if (name_char > DEF_INT_08U_MAX_VAL) {         /* Chk for non-8 bit char.                              */
                     is_legal = DEF_NO;
                 }
#endif
                 if (is_legal == DEF_YES) {
                     name_8_3_08[i] = ASCII_TO_UPPER(name_char);            /* See Notes #2a.                           */
                     i++;
                 } else {
                     is_legal = FS_FAT_IS_LEGAL_LFN_CHAR(name_char);
                     if (is_legal == DEF_YES) {
                        name_8_3_08[i] = (CPU_CHAR)ASCII_CHAR_LOW_LINE;     /* See Notes #2b.                           */
                        i++;
                     } else {
                         FS_TRACE_DBG(("FS_FAT_LFN_SFN_Fmt(): Invalid character in file name: %c (0x%02X).\r\n", name_char, name_char));
                     }
                 }
                 char_cnt++;
                 break;
        }

        if ((is_done  == DEF_NO) &&                             /* If name not fully parsed ...                         */
            (is_legal == DEF_YES)) {                            /* ... & name is still legal.                           */
            if (char_cnt >= FS_FAT_SFN_NAME_MAX_NBR_CHAR) {     /* See Notes #2e.                                       */
                is_done = DEF_YES;
            } else {
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
                name        +=  name_char_len;
                name_char_len = MB_CharToWC(&name_char,
                                             name,
                                             MB_MAX_LEN);

                if (name_char_len > MB_MAX_LEN) {
                    name_char = (CPU_WCHAR)ASCII_CHAR_NULL;
                }
#else
                name++;
                name_char = *name;
#endif
            }
        }
    }

    if (is_legal == DEF_NO) {
       *p_err = FS_ERR_NAME_INVALID;
        return (0u);
    }



                                                                /* ------------------ COPY EXTENSION ------------------ */
    i += FS_FAT_SFN_NAME_MAX_NBR_CHAR - char_cnt;
    if (p_period != (CPU_CHAR *)0) {                            /* If there is a period in file name, copy ext.         */
        p_period++;
        char_cnt_ext  = 0u;
        is_done       = DEF_NO;
        is_legal      = DEF_YES;
#if (FS_CFG_UTF8_EN  == DEF_ENABLED)
        name_char_len = MB_CharToWC(&name_char,
                                     p_period,
                                     MB_MAX_LEN);
        if ((name_char_len == (CPU_SIZE_T)-2) ||
            (name_char_len == (CPU_SIZE_T)-1)) {
            name_char = (CPU_WCHAR)ASCII_CHAR_NULL;
        }
#else
        name_char     = *p_period;
#endif
        while ((is_done  == DEF_NO) &&
               (is_legal == DEF_YES)) {
            switch (name_char) {
                case ASCII_CHAR_SPACE:                          /* Skip embedded spaces (see Notes #2c).                */
                     break;


                case ASCII_CHAR_NULL:                           /* Exit loop if NULL char or path sep char reached.     */
                case FS_FAT_PATH_SEP_CHAR:
                     is_done = DEF_YES;
                     break;


                default:                                        /* Chk for legal file name char.                        */
                     is_legal = FS_FAT_IS_LEGAL_SFN_CHAR(name_char);
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
                     if (name_char > DEF_INT_08U_MAX_VAL) {     /* Chk for non-8 bit char.                              */
                         is_legal = DEF_NO;
                     }
#endif
                     if (is_legal == DEF_YES) {
                         name_8_3_08[i] = ASCII_TO_UPPER(name_char);            /* See Notes #2a.                       */
                         i++;
                     } else {
                         is_legal = FS_FAT_IS_LEGAL_LFN_CHAR(name_char);
                         if (is_legal == DEF_YES) {
                             name_8_3_08[i] = (CPU_INT08U)ASCII_CHAR_LOW_LINE;  /* See Notes #2b.                       */
                             i++;
                         } else {
                            FS_TRACE_DBG(("FS_FAT_LFN_SFN_Fmt(): Invalid character in file ext: %c (0x%02X).\r\n", name_char, name_char));
                         }
                     }
                     char_cnt_ext++;
                     break;
            }

            if ((is_done  == DEF_NO) &&                         /* If name not fully parsed ...                         */
                (is_legal == DEF_YES)) {                        /* ... & name is still legal.                           */
                if (char_cnt_ext >= FS_FAT_SFN_EXT_MAX_NBR_CHAR) {      /* See Notes #2f.                               */
                    is_done = DEF_YES;
                } else {
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
                    p_period     += name_char_len;
                    name_char_len = MB_CharToWC(&name_char,
                                                 p_period,
                                                 MB_MAX_LEN);

                    if ((name_char_len == (CPU_SIZE_T)-2) ||
                        (name_char_len == (CPU_SIZE_T)-1)) {
                        name_char = (CPU_WCHAR)ASCII_CHAR_NULL;
                    }
#else
                    p_period++;
                    name_char = *p_period;
#endif
                }
            }
        }
    }



                                                                /* ------------------------ RTN ----------------------- */
    if (is_legal == DEF_NO) {                                   /* Rtn err if illegal name.                             */
       *p_err = FS_ERR_NAME_INVALID;
        return (0u);
    }

    name_8_3[0] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[0]);
    name_8_3[1] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[4]);
    name_8_3[2] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[8]);

   *p_err = FS_ERR_NONE;
    return (char_cnt);
}
#endif



/*
*********************************************************************************************************
*                                       FS_FAT_LFN_SFN_Parse()
*
* Description : Read characters of SFN name in SFN entry.
*
* Argument(s) : p_dir_entry         Pointer to directory entry.
*
*               name                String that will receive entry name.
*
*               name_lower_case     Indicates whether name characters will be returned in lower case :
*                                       DEF_YES, name characters will be returned in lower case.
*                                       DEF_NO,  name characters will be returned in upper case.
*
*               ext_lower_case      Indicates whether extension characters will be returned in lower case :
*                                       DEF_YES, extension characters will be returned in lower case.
*                                       DEF_NO,  extension characters will be returned in upper case.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_LFN_SFN_Parse (void             *p_dir_entry,
                                    FS_FAT_LFN_CHAR  *name,
                                    CPU_BOOLEAN       name_lower_case,
                                    CPU_BOOLEAN       ext_lower_case)
{
    FS_FILE_NAME_LEN   ix;
    FS_FILE_NAME_LEN   len;
    CPU_INT08U        *p_dir_entry_08;


    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;

                                                                /* ---------------------- RD NAME --------------------- */
    for (len = FS_FAT_SFN_NAME_MAX_NBR_CHAR; len > 0u; len--) { /* Calc name len.                                       */
        if (p_dir_entry_08[len - 1u] != (CPU_INT08U)ASCII_CHAR_SPACE) {
            break;
        }
    }

    if (len != 0u) {                                            /* Copy name.                                           */
        for (ix = 0u; ix < len; ix++) {
           *name = (name_lower_case == DEF_YES) ? ASCII_TO_LOWER((CPU_CHAR)p_dir_entry_08[ix])
                                                :                          p_dir_entry_08[ix];
            name++;
        }
    }



                                                                /* ---------------------- RD EXT ---------------------- */
    for (len = FS_FAT_SFN_EXT_MAX_NBR_CHAR; len > 0u; len--) {  /* Calc ext len.                                        */
        if (p_dir_entry_08[len + FS_FAT_SFN_NAME_MAX_NBR_CHAR - 1u] != (CPU_INT08U)ASCII_CHAR_SPACE) {
            break;
        }
    }

    if (len != 0u) {                                            /* Copy ext.                                            */
       *name = (FS_FAT_LFN_CHAR)ASCII_CHAR_FULL_STOP;
        name++;

        for (ix = 0u; ix < len; ix++) {
           *name = (ext_lower_case == DEF_YES) ? ASCII_TO_LOWER((CPU_CHAR)p_dir_entry_08[ix + FS_FAT_SFN_NAME_MAX_NBR_CHAR])
                                               :                          p_dir_entry_08[ix + FS_FAT_SFN_NAME_MAX_NBR_CHAR];
            name++;
        }
    }

   *name = (FS_FAT_LFN_CHAR)ASCII_CHAR_NULL;                    /* End NULL char.                                       */
}


/*
*********************************************************************************************************
*                                      FS_FAT_LFN_SFN_FmtTail()
*
* Description : Format SFN tail.
*
* Argument(s) : name_8_3    Entry SFN.
*
*               char_cnt    Number of characters in base SFN.
*
*               tail_nbr    Number to use for tail.
*
* Return(s)   : none.
*
* Note(s)     : (1) The numeric tail is formatted according to the numeric-tail generation algorithm
*                   in [Ref 1] :
*
*                   (a) The numeric tail is a string '~n' where 'n' is a decimal number between 1 &
*                       999999.
*
*                   (b) This function MUST never be called with 'tail_nbr' > 999999; a check for this is
*                       merely added for completeness.
*
*               (2) If the numeric tail can be appended to the base SFN WITHOUT overflowing the
*                   8-character limit, then it is appended; otherwise, the final characters of the
*                   8-character SFN will be the numeric tail (thus overwriting as few of the name
*                   characters as possible).
*
*                   (a) For example, if the name encoded in 'name_8_3' is 'abcd.txt' & 'tail_nbr'
*                       is 12, then the resulting SFN will be 'abcd~12.txt'.
*
*                   (b) For example, if the name encoded in 'name_8_3' is 'abcd.txt' & 'tail_nbr'
*                       is 12345, then the resulting SFN will be 'ab~12345.txt'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_LFN_SFN_FmtTail (CPU_INT32U         name_8_3[],
                                      FS_FILE_NAME_LEN   char_cnt,
                                      FS_FAT_SFN_TAIL    tail_nbr)
{
    CPU_INT16U        char_cnt_tail;
    FS_FILE_NAME_LEN  char_ix;
    CPU_INT08U        name_8_3_08[12];
    CPU_INT08U        tail_nbr_dig;


                                                                /* --------------- CALC NBR TAIL CHAR's --------------- */
    if (tail_nbr < 10u) {
        char_cnt_tail = 1u;
    } else if (tail_nbr < 100u) {
        char_cnt_tail = 2u;
    } else if (tail_nbr < 1000u) {
        char_cnt_tail = 3u;
    } else if (tail_nbr < 10000u) {
        char_cnt_tail = 4u;
    } else if (tail_nbr < 100000u) {
        char_cnt_tail = 5u;
    } else if (tail_nbr < 1000000u) {
        char_cnt_tail = 6u;
    } else {
        FS_TRACE_DBG(("FS_FAT_LFN_SFN_FmtTail(): Tail nbr %d > 999999.\r\n", tail_nbr));
        return;                                                 /* See Notes #1b.                                       */
    }


                                                                /* ------------------- FMT TAIL STR ------------------- */
                                                                /* See Notes #2.                                        */
    char_ix = 0u;
    if (FS_FAT_SFN_NAME_MAX_NBR_CHAR >= char_cnt +  char_cnt_tail + 1u) {
        char_ix +=  char_cnt;
    } else {
        char_ix += (FS_FAT_SFN_NAME_MAX_NBR_CHAR - 1u) - char_cnt_tail;
    }

    MEM_VAL_SET_INT32U((void *)&name_8_3_08[0], name_8_3[0]);
    MEM_VAL_SET_INT32U((void *)&name_8_3_08[4], name_8_3[1]);
    MEM_VAL_SET_INT32U((void *)&name_8_3_08[8], name_8_3[2]);

    name_8_3_08[char_ix] = (CPU_INT08U)ASCII_CHAR_TILDE;
    char_ix += char_cnt_tail;

    while (char_cnt_tail > 0u) {
        tail_nbr_dig          = (CPU_INT08U)(tail_nbr % 10u);
        name_8_3_08[char_ix]  =  tail_nbr_dig + (CPU_INT08U)ASCII_CHAR_DIGIT_ZERO;
        tail_nbr             /=  DEF_NBR_BASE_DEC;
        char_ix--;
        char_cnt_tail--;
    }

    name_8_3[0] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[0]);
    name_8_3[1] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[4]);
    name_8_3[2] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[8]);
}
#endif


/*
*********************************************************************************************************
*                                  FS_FAT_LFN_SFN_DirEntryFindMax()
*
* Description : Search directory for find SFN directory entries with the same stem & return max tail
*               value.
*
* Argument(s) : p_vol           Pointer to volume.
*
*               p_buf           Pointer to temporary buffer.
*
*               name_8_3        Entry SFN.
*
*               dir_start_sec   Directory sector at which search should start.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                       Directory entry found.
*                                   FS_ERR_DEV                        Device access error.
*                                   FS_ERR_SYS_DIR_ENTRY_NOT_FOUND    Directory entry not found.
*
* Return(s)   : Maximum tail value, if any is found.
*               0,                  otherwise.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all buffers are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*
*               (2) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_FAT_SFN_TAIL  FS_FAT_LFN_SFN_DirEntryFindMax (FS_VOL          *p_vol,
                                                         FS_BUF          *p_buf,
                                                         CPU_INT32U       name_8_3[],
                                                         FS_FAT_SEC_NBR   dir_start_sec,
                                                         FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR    dir_cur_sec;
    CPU_BOOLEAN       dir_sec_valid;
    FS_FAT_SFN_TAIL   max;
    FS_FAT_SFN_TAIL   max_temp;
    FS_FAT_DATA      *p_fat_data;


    p_fat_data    = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_cur_sec   =  dir_start_sec;
    dir_sec_valid =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);
    max           =  0u;

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #2).                    */
        FSBuf_Set(p_buf,
                  dir_cur_sec,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        max_temp = FS_FAT_LFN_SFN_DirEntryFindMaxInSec(p_buf->DataPtr,
                                                       p_fat_data->SecSize,
                                                       name_8_3,
                                                       p_err);

        max      = DEF_MAX(max, max_temp);

        switch (*p_err) {
            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND:                /* No entry with matching name exists.                  */
                 return (max);

            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET:            /* Entry with matching name MAY exist in later sec.     */
                 dir_cur_sec = FS_FAT_SecNextGet(p_vol,
                                                 p_buf,
                                                 dir_cur_sec,
                                                 p_err);

                 switch (*p_err) {
                     case FS_ERR_NONE:                          /* More secs exist in dir.                              */
                                                                /* Chk sec validity (see Note #2).                      */
                          dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);
                          break;


                     case FS_ERR_SYS_CLUS_CHAIN_END:            /* No more secs exist in dir.                           */
                     case FS_ERR_SYS_CLUS_INVALID:
                         *p_err = FS_ERR_NONE;
                          return (max);


                     case FS_ERR_DEV:
                     default:
                          return (0u);
                 }
                 break;

            case FS_ERR_DEV:
                 return (0u);

            default:
                 FS_TRACE_DBG(("FS_FAT_LFN_SFN_DirEntryFindMax(): Default case reached.\r\n"));
                 return (0u);
        }
    }


                                                                /* Invalid sec found (see Note #2).                     */
    FS_TRACE_DBG(("FS_FAT_LFN_SFN_DirEntryFindMax(): Invalid sec gotten: %d.\r\n", dir_cur_sec));
   *p_err = FS_ERR_ENTRY_CORRUPT;
    return (0u);
}
#endif


/*
*********************************************************************************************************
*                                FS_FAT_LFN_SFN_DirEntryFindMaxInSec()
*
* Description : Search directory sector to find SFN directory entry with the same stem & return max tail
*               value.
*
* Argument(s) : p_temp      Pointer to buffer of directory sector.
*
*               sec_size    Sector size, in octets.
*
*               name_8_3    Entry SFN.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND        All subsequent entries are free
*                                                                         (so search SHOULD end).
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET    Non-free subsequent entries may exist.
*
* Return(s)   : Maximum tail value, if any is found.
*               0,                  otherwise.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_FAT_SFN_TAIL  FS_FAT_LFN_SFN_DirEntryFindMaxInSec (void         *p_temp,
                                                              FS_SEC_SIZE   sec_size,
                                                              CPU_INT32U    name_8_3[],
                                                              FS_ERR       *p_err)
{
    CPU_INT08U        cmp_len;
    CPU_INT08U        data_08;
    CPU_BOOLEAN       dig;
    CPU_BOOLEAN       space;
    FS_FAT_SFN_TAIL   max;
    FS_FAT_SFN_TAIL   max_temp;
    CPU_INT08U        name_char;
    CPU_INT08U        name_cmp_char;
    FS_SEC_SIZE       sec_pos;
    CPU_INT08U       *p_buf_08;
    CPU_INT08U       *p_name;
    CPU_INT08U       *p_name_cmp;
    CPU_INT08U        name_8_3_08[12];



    MEM_VAL_SET_INT32U((void *)&name_8_3_08[0], name_8_3[0]);
    MEM_VAL_SET_INT32U((void *)&name_8_3_08[4], name_8_3[1]);
    MEM_VAL_SET_INT32U((void *)&name_8_3_08[8], name_8_3[2]);

    p_name   =  name_8_3_08;
    max      =  0u;
    sec_pos  =  0u;
    p_buf_08 = (CPU_INT08U *)p_temp;
    while (sec_pos < sec_size) {
        data_08 = *p_buf_08;

        if (data_08 == FS_FAT_DIRENT_NAME_FREE) {               /* If dir entry free & all subsequent entries free ...  */
           *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;
            return (max);                                       /* ... rtn cur max.                                     */
        }

        if (data_08 != FS_FAT_DIRENT_NAME_ERASED_AND_FREE) {    /* If dir entry NOT free.                               */
            p_name_cmp    =  p_buf_08;
            cmp_len       =  0u;
            name_cmp_char = *p_name_cmp;
            name_char     = *p_name;
            while ((name_cmp_char == name_char) &&              /* Cmp first 6 chars of name & dir entry.               */
                   ( cmp_len      <  FS_FAT_SFN_MAX_STEM_LEN)) {
                p_name_cmp   +=  sizeof(CPU_INT08U);
                p_name       +=  sizeof(CPU_INT08U);
                cmp_len      +=  sizeof(CPU_INT08U);
                name_cmp_char = *p_name_cmp;
                name_char     = *p_name;
            }

            if (cmp_len > 0u) {                                 /* Matching char(s) found.                              */
                name_cmp_char = *p_name_cmp;
                if (name_cmp_char == (CPU_INT08U)ASCII_CHAR_TILDE) {    /* Names match up to tilde.                     */
                    p_name_cmp++;                                       /* Move past tilde.                             */
                    max_temp = 0u;
                    while (cmp_len < FS_FAT_SFN_NAME_MAX_NBR_CHAR - 1u) {

                        name_cmp_char  = *p_name_cmp;
                                                                        /* Chk if tail char is dig.                     */
                        dig   = ASCII_IS_DIG((CPU_CHAR)name_cmp_char);
                        space = ASCII_IS_SPACE((CPU_CHAR)name_cmp_char);
                        if (dig == DEF_NO) {
                            if(space == DEF_NO) {
                                max_temp = 0u;                          /* Invalid tail found.                          */
                            }
                            break;
                        }

                        max_temp   *=  10u;
                        max_temp   += (FS_FAT_SFN_TAIL)name_cmp_char - (FS_FAT_SFN_TAIL)ASCII_CHAR_DIG_ZERO;
                        cmp_len    +=  sizeof(CPU_INT08U);
                        p_name_cmp +=  sizeof(CPU_INT08U);
                    }

                    max = DEF_MAX(max, max_temp);
                }
            }

            p_name = name_8_3_08;
        }

        p_buf_08 += FS_FAT_SIZE_DIR_ENTRY;
        sec_pos  += FS_FAT_SIZE_DIR_ENTRY;
    }

   *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET;                 /* If dir entry NOT found ... rtn NULL ptr.             */
    return (max);
}
#endif


/*
*********************************************************************************************************
*                                        FS_FAT_LFN_StrLen_N()
*
* Description : Calculate length of a string, up to a maximum number of characters.
*
* Argument(s) : p_str       Pointer to string.
*
*               len_max     Maximum number of characters to search.
*
* Return(s)   : Length of string; number of characters in string before terminating NULL character.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_FILE_NAME_LEN  FS_FAT_LFN_StrLen_N (FS_FAT_LFN_CHAR   *p_str,
                                               FS_FILE_NAME_LEN   len_max)
{
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_SIZE_T  len;


    len = WC_StrLen_N(p_str, (CPU_SIZE_T)len_max);
    return ((FS_FILE_NAME_LEN)len);
#else
    CPU_SIZE_T  len;


    len = Str_Len_N(p_str, (CPU_SIZE_T)len_max);
    return ((FS_FILE_NAME_LEN)len);
#endif
}


/*
*********************************************************************************************************
*                                   FS_FAT_LFN_StrCmpIgnoreCase_N()
*
* Description : Determine if two strings are identical for up to a maximum number of characters, ignoring
*               case.
*
* Argument(s) : p1_str      Pointer to first  string.
*
*               p2_str      Pointer to second string (possibly a wide-character string).
*
*               len_max     Maximum number of character octets from 'p1_str' to compare.
*
* Return(s)   : 0,              if strings are identical.
*
*               Negative value, if 'p1_str' is less    than 'p2_str'.
*
*               Positive value, if 'p1_str' is greater than 'p2_str'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32S  FS_FAT_LFN_StrCmpIgnoreCase_N (CPU_CHAR          *p1_str,
                                                   FS_FAT_LFN_CHAR   *p2_str,
                                                   FS_FILE_NAME_LEN   len_max)
{
#if (FS_CFG_UTF8_EN == DEF_ENABLED)
    CPU_WCHAR   char1;
    CPU_WCHAR   char2;
    CPU_INT32S  cmp_val;
    CPU_SIZE_T  cmp_len;
    CPU_SIZE_T  char_len;


    cmp_len  = 0u;
    char_len = MB_CharToWC(&char1,
                            p1_str,
                            MB_MAX_LEN);
    if (char_len <= MB_MAX_LEN) {
        char1 = WC_CharToCasefold(char1);
    }
    char2 = *p2_str;
    char2 =  WC_CharToCasefold(char2);

    while ((char1    ==  char2)        &&                       /* Cmp strs until non-matching chars ...                */
           (char2    != (CPU_WCHAR )0) &&                       /* ... or NULL chars                 ...                */
           (char_len != (CPU_SIZE_T)0) &&                       /* ... or NULL ptr(s) found                             */
           (char_len >  (CPU_SIZE_T)0) &&
           (cmp_len  <  (CPU_SIZE_T)len_max)) {                 /* ... or max nbr chars cmp'd.                          */
        p1_str  += char_len;
        p2_str++;
        cmp_len++;

        char_len = MB_CharToWC(&char1,
                                p1_str,
                                MB_MAX_LEN);
        if (char_len <= MB_MAX_LEN) {
            char1 = WC_CharToCasefold(char1);
        }
        char2 = *p2_str;
        char2 =  WC_CharToCasefold(char2);
    }


    if (cmp_len == len_max) {                                   /* If strs identical for len nbr of chars, ...          */
        return ((CPU_INT32S)0);                                 /* ... rtn 0.                                           */
    }

    if ((char_len == 0u) ||                                     /* If NULL char(s) found, ...                           */
        (char_len >  MB_MAX_LEN)) {
        if (char2 == (CPU_WCHAR)0) {
            cmp_val = 0;                                        /* ... strs identical; rtn 0.                           */
        } else {
            cmp_val = (CPU_INT32S)0 - (CPU_INT32S)char2;
        }

    }  else if (char1 != char2) {                               /* If strs NOT identical, ...                           */
         cmp_val = (CPU_INT32S)char1 - (CPU_INT32S)char2;       /* ... calc & rtn char diff.                            */

    } else {                                                    /* Else ...                                             */
         cmp_val = 0;                                           /* ... strs identical; rtn 0.                           */
    }

    return (cmp_val);
#else
    CPU_INT16S  cmp_val;


    cmp_val = Str_CmpIgnoreCase_N(p1_str, p2_str, (CPU_SIZE_T)len_max);
    return ((CPU_INT32S)cmp_val);
#endif
}


/*
*********************************************************************************************************
*                                     FS_FAT_LFN_DirEntriesChk()
*
* Description : Check next LFN directory entries from dir.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to directory position at which should search should start.
*
*               p_dir_end_pos       Pointer to variable that will receive directory position at which
*                                   first orphaned LFN entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                All directory entries checked.
*                                       FS_ERR_SYS_LFN_ORPHANED    Orphaned LFN(s) found.
*                                       FS_ERR_DEV                 Device access error.
*
* Return(s)   : The number of orphaned LFN entries that should be deleted, if orphaned entries found.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*
*               (2) The lower nibble of the first byte of a LFN directory entry is the index of that
*                   entry in the group of LFN entries for the SFN.  The first LFN entry (i.e., the one
*                   located in the lowest position of the lowest sector) contains the final characters
*                   of the LFN & has the highest index.  Additionally, the first LFN entry will have the
*                   sixth bit of the first byte set.
*
*                   (a) If the LFN entry order is disrupted, that LFN entry & previous entries should
*                       be deleted.
*
*                   (b) If a LFN entry is found without a first LFN entry having been found, the LFN
*                       entry should be deleted.
*
*                   (c) No LFN entry should contain an index of 0.
*
*               (3) The checksum of a LFN becomes invalid if the SFN is modified or deleted by a LFN-
*                   ignorant system.  In the case of an invalid checksum, the SFN is the correct name &
*                   the LFN entries should be deleted.
*
*               (4) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
static  CPU_INT08U  FS_FAT_LFN_DirEntriesChk (FS_VOL          *p_vol,
                                              FS_BUF          *p_buf,
                                              FS_FAT_DIR_POS  *p_dir_start_pos,
                                              FS_FAT_DIR_POS  *p_dir_end_pos,
                                              FS_ERR          *p_err)
{
    CPU_INT08U       attrib;
    CPU_INT08U       chksum;
    CPU_INT08U       chksum_calc;
    FS_FAT_DIR_POS   dir_lfn_pos;
    FS_FAT_DIR_POS   dir_cur_pos;
    CPU_BOOLEAN      dir_sec_valid;
    CPU_INT08U       lfn_entry_cnt;
    CPU_INT08U       lfn_entry_cnt_tot;
    CPU_BOOLEAN      lfn_entry_first;
    CPU_INT08U       lfn_entry_ix;
    CPU_BOOLEAN      long_name;
    CPU_BOOLEAN      has_lfn;
    CPU_INT08U       name_char;
    FS_FAT_DATA     *p_fat_data;
    CPU_INT08U      *p_dir_entry;


    p_dir_end_pos->SecNbr =  0u;                                /* Assign dflts.                                        */
    p_dir_end_pos->SecPos =  0u;

    chksum                =  0u;
    lfn_entry_cnt         =  0u;
    lfn_entry_cnt_tot     =  0u;
    has_lfn               =  DEF_NO;

    p_fat_data            = (FS_FAT_DATA *)p_vol->DataPtr;
    p_dir_entry           = (CPU_INT08U  *)p_buf->DataPtr;
    p_dir_entry          +=  p_dir_start_pos->SecPos;

    dir_cur_pos.SecNbr    =  p_dir_start_pos->SecNbr;
    dir_cur_pos.SecPos    =  p_dir_start_pos->SecPos;
    dir_sec_valid         =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    FSBuf_Set(p_buf,
              dir_cur_pos.SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
         return (0u);
    }

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #4).                    */
                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {
            dir_cur_pos.SecNbr =  FS_FAT_SecNextGet(p_vol,
                                                    p_buf,
                                                    dir_cur_pos.SecNbr,
                                                    p_err);

            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                    *p_err = FS_ERR_NONE;
                     return (0u);

                case FS_ERR_DEV:
                default:
                     return (0u);
            }

                                                                /* Chk sec validity (see Note #4).                      */
            dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);
            if (dir_sec_valid == DEF_YES) {
                dir_cur_pos.SecPos = 0u;
                p_dir_entry        = (CPU_INT08U *)p_buf->DataPtr;

                FSBuf_Set(p_buf,
                          dir_cur_pos.SecNbr,
                          FS_VOL_SEC_TYPE_DIR,
                          DEF_YES,
                          p_err);

                if (*p_err != FS_ERR_NONE) {
                    return (0u);
                }
            }
        }


        if (dir_sec_valid == DEF_YES) {
            name_char = *p_dir_entry;
                                                                /* --------------------- FREE ENTRY ------------------- */
            if ((name_char == FS_FAT_DIRENT_NAME_ERASED_AND_FREE) ||
                (name_char == FS_FAT_DIRENT_NAME_FREE)) {       /* If dir entry free.                                   */

                if (lfn_entry_cnt >= 1u) {                      /* Disrupted LFN entry order (see Note #2a).            */
                    p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                    p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                   *p_err = FS_ERR_SYS_LFN_ORPHANED;
                    return (lfn_entry_cnt_tot - lfn_entry_cnt);

                } else {
                    if (has_lfn == DEF_YES) {                   /* LFN entry without SFN entry.                         */
                        p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                        p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                       *p_err = FS_ERR_SYS_LFN_ORPHANED;
                        return (lfn_entry_cnt_tot);
                    }
                }

                if (name_char == FS_FAT_DIRENT_NAME_FREE) {     /* If all subsequent entries free ...                   */
                   *p_err = FS_ERR_NONE;
                    return (0u);                                /* ... rtn zero.                                        */
                }

                has_lfn       = DEF_NO;
                lfn_entry_cnt = 0u;


            } else {                                            /* If dir entry NOT free.                               */
                attrib = *(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR);

                                                                /* --------------------- LFN ENTRY -------------------- */
                long_name = FS_FAT_DIRENT_ATTR_IS_LONG_NAME(attrib);
                if (long_name == DEF_YES) {
                                                                /* See Note #2.                                         */
                    lfn_entry_ix = (name_char & FS_FAT_DIRENT_NAME_LONG_ENTRY_MASK);
                    if (lfn_entry_ix > 0u) {
                                                                /* 1st LFN entry.                                       */
                        lfn_entry_first = DEF_BIT_IS_SET(name_char, FS_FAT_DIRENT_NAME_LAST_LONG_ENTRY);
                        if (lfn_entry_first == DEF_YES) {

                            if (lfn_entry_cnt >= 1u) {          /* Disrupted LFN entry order (see Note #2a).            */
                                p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                                p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                               *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                return (lfn_entry_cnt_tot - lfn_entry_cnt);

                            } else {
                                if (has_lfn == DEF_YES) {       /* LFN entry without SFN entry.                         */
                                    p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                                    p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                                   *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                    return (lfn_entry_cnt_tot);
                                }
                            }

                            has_lfn       =   DEF_NO;
                            lfn_entry_cnt =   lfn_entry_ix;
                            chksum        = *(p_dir_entry + FS_FAT_LFN_OFF_CHKSUM);

                            if (lfn_entry_cnt >= 1u) {          /* Parse LFN entry into LFN name buf.                   */
                                lfn_entry_cnt      -= 1u;

                                dir_lfn_pos.SecNbr  = dir_cur_pos.SecNbr;
                                dir_lfn_pos.SecPos  = dir_cur_pos.SecPos;

                                lfn_entry_cnt_tot   = lfn_entry_cnt;

                                if (lfn_entry_cnt == 0) {
                                    has_lfn = DEF_YES;
                                }
                            }

                                                                /* LFN entry, not 1st.                                  */
                        } else if (lfn_entry_cnt == lfn_entry_ix) {
                            lfn_entry_cnt--;
                            if (lfn_entry_cnt == 0u) {
                                has_lfn = DEF_YES;
                            }

                                                                /* Orphaned LFN entry?                                  */
                        } else {
                            if (lfn_entry_cnt != 0u) {          /* Disrupted LFN entry order (see Note #2a).            */
                                p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                                p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                               *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                return (lfn_entry_cnt_tot - lfn_entry_cnt);

                            } else if (has_lfn == DEF_YES) {    /* LFN entry without SFN entry.                         */
                                p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                                p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                               *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                return (lfn_entry_cnt_tot);

                            } else {                            /* LFN entry without first LFN entry (see Note #2b).    */
                                p_dir_end_pos->SecNbr  = dir_cur_pos.SecNbr;
                                p_dir_end_pos->SecPos  = dir_cur_pos.SecPos;
                               *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                return ((CPU_INT08U)1);
                            }
                        }

                                                                /* Invalid LFN entry? (see Note #2c)                    */
                    } else {
                        has_lfn       = DEF_NO;
                        lfn_entry_cnt = 0u;
                    }


                                                                /* --------------------- SFN ENTRY -------------------- */
                } else if (DEF_BIT_IS_CLR(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                    if (lfn_entry_cnt != 0u) {                  /* Disrupted LFN entry order (see Note #2a).            */
                        p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                        p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                       *p_err = FS_ERR_SYS_LFN_ORPHANED;
                        return (lfn_entry_cnt_tot - lfn_entry_cnt);

                    } else {
                        if (has_lfn == DEF_YES) {
                            chksum_calc = FS_FAT_LFN_ChkSumCalc((void *)p_dir_entry);

                            if (chksum_calc != chksum) {        /* Invalid checksum (see Note #3).                          */
                                FS_TRACE_DBG(("FS_FAT_LFN_DirEntriesChk(): Chksum mismatch.\r\n"));
                                p_dir_end_pos->SecNbr = dir_lfn_pos.SecNbr;
                                p_dir_end_pos->SecPos = dir_lfn_pos.SecPos;
                               *p_err = FS_ERR_SYS_LFN_ORPHANED;
                                return (lfn_entry_cnt_tot);
                            }
                        }
                    }

                    has_lfn       = DEF_NO;                     /* Valid entry found, reset for next.                       */
                    lfn_entry_cnt = 0u;


                } else {
                    has_lfn       = DEF_NO;
                    lfn_entry_cnt = 0u;
                }
            }



                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
            dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
            p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
        }
    }


                                                                /* Invalid sec found (see Note #4).                     */
    FS_TRACE_DBG(("FS_FAT_LFN_DirEntriesChk(): Invalid sec gotten: %d.\r\n", dir_cur_pos.SecNbr));
   *p_err = FS_ERR_ENTRY_CORRUPT;
    return (0u);
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_LFN_DirEntryFree()
*
* Description : Free directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to directory position at which entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE    Directory entry freed.
*                                       FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_FAT_CFG_VOL_CHK_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN     == DEF_DISABLED)
static  void  FS_FAT_LFN_DirEntryFree (FS_VOL          *p_vol,
                                       FS_BUF          *p_buf,
                                       FS_FAT_DIR_POS  *p_dir_start_pos,
                                       FS_ERR          *p_err)
{
    CPU_INT08U  *p_dir_entry;


    (void)p_vol;

                                                                /* -------------------- RD DIR SEC -------------------- */
    FSBuf_Set(p_buf,
              p_dir_start_pos->SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + p_dir_start_pos->SecPos;



                                                                /* -------------------- FREE ENTRY -------------------- */
    Mem_Clr((void       *)p_dir_entry,                          /* Mark dir entry as free.                              */
            (CPU_SIZE_T  )FS_FAT_SIZE_DIR_ENTRY);
    p_dir_entry[0] = FS_FAT_DIRENT_NAME_ERASED_AND_FREE;



                                                                /* ------------------ UPDATE DIR SEC ------------------ */
    FSBuf_MarkDirty(p_buf, p_err);
}
#endif
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_fat.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of FAT LFN module include (see Note #1).         */
