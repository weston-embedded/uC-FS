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
*                                             JOURNALING
*
* Filename : fs_fat_journal.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_JOURNAL_MODULE


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
#include  "../Source/fs_file.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_journal.h"
#include  "fs_fat_lfn.h"
#include  "fs_fat_sfn.h"


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'fs_fat.h  MODULE'.
*
*           (2) See 'fs_fat_journal.h  MODULE'.
*********************************************************************************************************
*/

#ifdef   FS_FAT_MODULE_PRESENT                                  /* See Note #1.                                         */
#ifdef   FS_FAT_JOURNAL_MODULE_PRESENT                          /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_FAT_JOURNAL_MARK_ENTER                      0x6666u
#define  FS_FAT_JOURNAL_MARK_ENTER_END                  0xDDDDu

#define  FS_FAT_JOURNAL_SIG_CLUS_CHAIN_ALLOC            0x0001u
#define  FS_FAT_JOURNAL_SIG_CLUS_CHAIN_DEL              0x0002u
#define  FS_FAT_JOURNAL_SIG_ENTRY_CREATE                0x0003u
#define  FS_FAT_JOURNAL_SIG_ENTRY_UPDATE                0x0004u

#define  FS_FAT_JOURNAL_LOG_MARK_SIZE                    2u
#define  FS_FAT_JOURNAL_LOG_SIG_SIZE                     2u
#define  FS_FAT_JOURNAL_LOG_CLUS_CHAIN_ALLOC_SIZE       11u
#define  FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE  13u
#define  FS_FAT_JOURNAL_LOG_ENTRY_CREATE_SIZE           22u
#define  FS_FAT_JOURNAL_LOG_ENTRY_DEL_HEADER_SIZE       20u

#define  FS_FAT_JOURNAL_MAX_DEL_MARKER_STEP_SIZE        1000u

#define  FS_FAT_JOURNAL_FILE_NAME                      "journal.jnl"

#define  FS_FAT_JOURNAL_FILE_LEN                       (DEF_MAX((16u*1024u), FS_MaxSecSizeGet()))


/*
*********************************************************************************************************
                                    JOURNAL LOG STRUCTURE DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* ---------- CLUS CHAIN ALLOC LOG STRUCTURE ---------- */
enum  FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_STRUCTURE {
    FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_ENTER_MARK_OFFSET     = 0u,
    FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_SIG_OFFSET            = FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_ENTER_MARK_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_MARK_SIZE,

    FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_START_CLUS_OFFSET     = FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_SIG_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_SIG_SIZE,

    FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_IS_NEW_OFFSET         = FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_START_CLUS_OFFSET
                                                              + sizeof(FS_FAT_CLUS_NBR),

    FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_ENTER_END_MARK_OFFSET = FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_IS_NEW_OFFSET
                                                              + sizeof(CPU_BOOLEAN)
};


                                                                /* ----------- CLUS CHAIN DEL LOG STRUCTURE ----------- */
enum  FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_STRUCTURE {
    FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_ENTER_MARK_OFFSET       = 0u,
    FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_SIG_OFFSET              = FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_ENTER_MARK_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_MARK_SIZE,

    FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_NBR_MARKER_OFFSET       = FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_SIG_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_SIG_SIZE,

    FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_START_CLUS_OFFSET       = FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_NBR_MARKER_OFFSET
                                                              + sizeof(CPU_INT32U),

    FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_DEL_FIRST_OFFSET        = FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_START_CLUS_OFFSET
                                                              + sizeof(FS_FAT_CLUS_NBR)
};


                                                                /* ------------ ENTRY CREATE LOG STRUCTURE ------------ */
enum  FS_FAT_JOURNAL_ENTRY_CREATE_LOG_STRUCTURE {
    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_ENTER_MARK_OFFSET         = 0u,
    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_SIG_OFFSET                = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_ENTER_MARK_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_MARK_SIZE,

    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_NBR_OFFSET      = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_SIG_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_SIG_SIZE,

    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_POS_OFFSET      = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_NBR_OFFSET
                                                              + sizeof(FS_FAT_SEC_NBR),

    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_NBR_OFFSET        = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_POS_OFFSET
                                                              + sizeof(FS_SEC_SIZE),

    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_POS_OFFSET        = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_NBR_OFFSET
                                                              + sizeof(FS_FAT_SEC_NBR),

    FS_FAT_JOURNAL_ENTRY_CREATE_LOG_ENTER_END_MARK_OFFSET     = FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_POS_OFFSET
                                                              + sizeof(FS_SEC_SIZE)
};


                                                                /* ------------- ENTRY DEL LOG STRUCTURE -------------- */
enum  FS_FAT_JOURNAL_ENTRY_DEL_LOG_STRUCTURE {
    FS_FAT_JOURNAL_ENTRY_DEL_LOG_ENTER_MARK_OFFSET            = 0u,
    FS_FAT_JOURNAL_ENTRY_DEL_LOG_SIG_OFFSET                   = FS_FAT_JOURNAL_ENTRY_DEL_LOG_ENTER_MARK_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_MARK_SIZE,

    FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_NBR_OFFSET         = FS_FAT_JOURNAL_ENTRY_DEL_LOG_SIG_OFFSET
                                                              + FS_FAT_JOURNAL_LOG_SIG_SIZE,

    FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_POS_OFFSET         = FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_NBR_OFFSET
                                                              + sizeof(FS_FAT_SEC_NBR),

    FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_NBR_OFFSET           = FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_POS_OFFSET
                                                              + sizeof(FS_SEC_SIZE),

    FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_POS_OFFSET           = FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_NBR_OFFSET
                                                              + sizeof(FS_FAT_SEC_NBR),

    FS_FAT_JOURNAL_ENTRY_DEL_LOG_ENTRY_LOG_START_OFFSET       = FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_POS_OFFSET
                                                              + sizeof(FS_SEC_SIZE)
};


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

static  MEM_POOL  FS_FAT_JournalFileDataPool;


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

                                                                                /* -------- JOURNAL FILE FNCTS -------- */
static  void         FS_FAT_JournalFileCreate          (FS_VOL      *p_vol,     /* Create journal file.                 */
                                                        FS_ERR      *p_err);

                                                                                /* --------- JOURNAL LOG FNCTS -------- */
static  void         FS_FAT_JournalPeek                (FS_VOL      *p_vol,     /* Peek at next journal log.            */
                                                        FS_BUF      *p_buf,
                                                        void        *p_log,
                                                        CPU_SIZE_T   pos,
                                                        CPU_SIZE_T   len,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalPosSet              (FS_VOL      *p_vol,     /* Set journal file pos.                */
                                                        FS_BUF      *p_buf,
                                                        CPU_SIZE_T   pos,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalRd                  (FS_VOL      *p_vol,     /* Rd data from log into buf.           */
                                                        FS_BUF      *p_buf,
                                                        void        *p_log,
                                                        CPU_SIZE_T   len,
                                                        FS_ERR      *p_err);

static  CPU_INT16U   FS_FAT_JournalRd_INT16U           (FS_VOL      *p_vol,     /* Rd 16-bit int from log.              */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);

static  CPU_INT32U   FS_FAT_JournalRd_INT32U           (FS_VOL      *p_vol,     /* Rd 32-bit int from log.              */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalWr                  (FS_VOL      *p_vol,     /* Wr log.                              */
                                                        FS_BUF      *p_buf,
                                                        void        *p_log,
                                                        CPU_SIZE_T   len,
                                                        FS_ERR      *p_err);

static  CPU_SIZE_T   FS_FAT_JournalReverseScan         (FS_VOL      *p_vol,     /* Reverse scan journal.                */
                                                        FS_BUF      *p_buf,
                                                        CPU_SIZE_T   start_pos,
                                                        CPU_INT08U  *p_pattern,
                                                        CPU_SIZE_T   pattern_size,
                                                        FS_ERR      *p_err);

                                                                                /* ------- JOURNAL REPLAY FNCTS ------- */
static  void         FS_FAT_JournalReplay              (FS_VOL      *p_vol,     /* Replay journal.                      */
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalRevertClusChainAlloc(FS_VOL      *p_vol,     /* Revert clus alloc.                   */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalReplayClusChainDel  (FS_VOL      *p_vol,     /* Complete clus del.                   */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalRevertEntryCreate   (FS_VOL      *p_vol,     /* Revert dir entry creation.           */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);

static  void         FS_FAT_JournalRevertEntryUpdate   (FS_VOL      *p_vol,     /* Revert dir entry update/del.         */
                                                        FS_BUF      *p_buf,
                                                        FS_ERR      *p_err);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if (FS_FAT_JOURNAL_HEADER_SIZE_ENTRY_DEL > FS_FAT_SIZE_DIR_ENTRY)
#error  "FS_FAT_JOURNAL_HEADER_SIZE_ENTRY_DEL should be smaller than FS_FAT_SIZE_DIR_ENTRY."
#endif

/*
*********************************************************************************************************
*                                        FS_FAT_JournalOpen()
*
* Description : Open or create journal file, replaying contents to restore file system to safe state.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                        Journal opened.
*                               FS_ERR_NAME_NULL                   Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN                Volume not open.
*                               FS_ERR_VOL_JOURNAL_ALREADY_OPEN    Journal already open.
*                               FS_ERR_BUF_NONE_AVAIL              No buffer available.
*                               FS_ERR_DEV                         Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) If the journal file could not be read, it may have been only partially created.
*                   It must be fully created, with the correct length & cleared data.
*********************************************************************************************************
*/

void  FS_FAT_JournalOpen (CPU_CHAR  *name_vol,
                          FS_ERR    *p_err)
{
    FS_BUF             *p_buf;
    FS_FAT_DATA        *p_fat_data;
    FS_FAT_FILE_DATA   *p_journal_data;
    FS_VOL             *p_vol;
    FS_FAT_CLUS_NBR     clus_cnt;
    FS_FAT_CLUS_NBR     clus_end;
    FS_FAT_CLUS_NBR     fat_entry;
    CPU_BOOLEAN         journal_complete;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------- VALIDATE PTR & FILE SIZE ------------- */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == DEF_NULL) {                                 /* Validate vol name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (FS_UTIL_IS_PWR2(FS_FAT_JOURNAL_FILE_LEN) == DEF_NO) {   /* Validate journal file size.                          */
        FS_TRACE_DBG(("FS_FAT_JournalOpen(): Journal file len must be a power of 2.\r\n"));
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol may NOT be unmounted.                            */
    if (p_vol == DEF_NULL) {
        return;
    }


                                                                /* ------------------- FIND JOURNAL ------------------- */
    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;
                                                                /* If journal is already open ...                       */
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_ALREADY_OPEN;                /* ... rtn err.                                         */
        FSVol_ReleaseUnlock(p_vol);
        return;
    }

    FS_FAT_LowEntryFind(p_vol,                                 /* Find journal file, if it exists.                     */
                        p_journal_data,
                        FS_FAT_JOURNAL_FILE_NAME,
                       (FS_FILE_ACCESS_MODE_RD | FS_FAT_MODE_FILE),
                        p_err);
    switch (*p_err) {

                                                                /* ------------------ JOURNAL FOUND ------------------- */
        case FS_ERR_NONE:
             p_buf = FSBuf_Get(p_vol);
             if (p_buf == DEF_NULL) {
                *p_err = FS_ERR_BUF_NONE_AVAIL;
                 FSVol_ReleaseUnlock(p_vol);
                 return;
             }
                                                                /* Chk if journal file is complete (see Note #1).       */
             if (FS_FAT_JOURNAL_FILE_LEN >= p_fat_data->ClusSize_octet) {
                 clus_cnt = FS_UTIL_DIV_PWR2(FS_FAT_JOURNAL_FILE_LEN, p_fat_data->ClusSizeLog2_octet);
             } else {
                 clus_cnt = 1u;
             }

             journal_complete = DEF_NO;
             if (p_journal_data->FileFirstClus != 0u) {
                 clus_end = FS_FAT_ClusChainFollow(p_vol,
                                                   p_buf,
                                                   p_journal_data->FileFirstClus,
                                                   clus_cnt - 1u,
                                                   DEF_NULL,
                                                   p_err);
                 if ((*p_err != FS_ERR_NONE) &&
                     (*p_err != FS_ERR_SYS_CLUS_INVALID) &&
                     (*p_err != FS_ERR_SYS_CLUS_CHAIN_END_EARLY)) {
                     FSBuf_Free(p_buf);
                     FSVol_ReleaseUnlock(p_vol);
                     return;
                 }

                                                                /* ------------------ REPLAY JOURNAL ------------------ */
                 if (*p_err == FS_ERR_NONE) {                   /* If journal file len is as expected ...               */
                     fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                                        p_buf,
                                                                        clus_end,
                                                                        p_err);
                     if (*p_err != FS_ERR_NONE) {
                         FSBuf_Free(p_buf);
                         FSVol_ReleaseUnlock(p_vol);
                         return;
                     }
                                                                /* ... and file is EOC terminated ...                   */
                     if (fat_entry >= p_fat_data->FAT_TypeAPI_Ptr->ClusEOF) {
                         journal_complete = DEF_YES;            /*  ... then journal file is complete.                  */
                     }
                 }
             }

             if (journal_complete == DEF_YES) {                 /* If journal file is complete, replay journal ...      */
                 DEF_BIT_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN);
                 FS_FAT_JournalReplay(p_vol, p_err);
             } else {                                           /* ... else, create it.                                 */
                 FS_TRACE_DBG(("FS_FAT_JournalOpen(): Journal file incomplete. Recreating journal file.\r\n."));
                 FS_FAT_JournalFileCreate(p_vol, p_err);
             }
             FSBuf_Free(p_buf);
             break;

                                                                /* ---------------- JOURNAL NOT FOUND ----------------- */
        case FS_ERR_ENTRY_NOT_FOUND:                            /* If journal file not found ...                        */
        case FS_ERR_ENTRY_CORRUPT:                              /* ... or corrupted ...                                 */
             FS_FAT_JournalFileCreate(p_vol, p_err);            /* ... create it.                                       */
             break;

                                                                /* --------------------- DEV ERR ---------------------- */
        default:
             FS_TRACE_INFO(("FS_FAT_JournalOpen(): Device error. Journal could not be opened nor created.\r\n"));
             break;
    }


                                                                /* ------------------ JOURNAL OPENED ------------------ */
    if (*p_err == FS_ERR_NONE) {
        DEF_BIT_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN);
    } else {
        DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN);
    }
    DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START |
                                          FS_FAT_JOURNAL_STATE_REPLAY);
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalClose()
*
* Description : Close journal.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                       Journal closed.
*                               FS_ERR_NAME_NULL                  Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN               Volume not open.
*                               FS_ERR_VOL_JOURNAL_NOT_OPEN       Journal not open.
*                               FS_ERR_VOL_JOURNAL_NOT_STOPPED    Journaling not stopped.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_JournalClose (CPU_CHAR  *name_vol,
                           FS_ERR    *p_err)
{
    FS_FAT_DATA  *p_fat_data;
    FS_VOL       *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == DEF_NULL) {                                 /* Validate vol name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol may NOT be unmounted.                            */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == DEF_NULL) {
        return;
    }


                                                                /* ------------------- CLOSE JOURNAL ------------------ */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_OPEN;

    } else if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_STOPPED;

    } else {
        DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN  |
                                              FS_FAT_JOURNAL_STATE_START |
                                              FS_FAT_JOURNAL_STATE_REPLAY);
       *p_err = FS_ERR_NONE;
    }


                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalStart()
*
* Description : Start logging FAT operations in the journal for the specified volume.
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Journaling started.
*                               FS_ERR_NAME_NULL               Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN            Volume not open.
*                               FS_ERR_VOL_JOURNAL_NOT_OPEN    Journal not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_JournalStart (CPU_CHAR  *name_vol,
                           FS_ERR    *p_err)
{
    FS_FAT_DATA  *p_fat_data;
    FS_VOL       *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == DEF_NULL) {                                 /* Validate vol name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol may NOT be unmounted.                            */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == DEF_NULL) {
        return;
    }


                                                                /* ----------------- START JOURNALING ----------------- */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_OPEN;

    } else {
        DEF_BIT_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START);
       *p_err = FS_ERR_NONE;
    }


                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalStop()
*
* Description : Stop journaling (see Note #1).
*
* Argument(s) : name_vol    Volume name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                       Journaling stopped.
*                               FS_ERR_NAME_NULL                  Argument 'name_vol' passed a NULL pointer.
*                               FS_ERR_VOL_NOT_OPEN               Volume not open.
*                               FS_ERR_VOL_JOURNAL_NOT_OPEN       Journal not open.
*                               FS_ERR_VOL_JOURNAL_NOT_STARTED    Journaling not started.
*
* Return(s)   : none.
*
* Note(s)     : (1) Journaling should never be stopped unless volume is going to be closed. If FAT
*                   operations are performed after journal is stopped and failure occurs, file system could
*                   be left in an inconsistent state after volume remounting.
*********************************************************************************************************
*/

void  FS_FAT_JournalStop (CPU_CHAR  *name_vol,
                          FS_ERR    *p_err)
{
    FS_FAT_DATA  *p_fat_data;
    FS_VOL       *p_vol;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(;);
    }
    if (name_vol == DEF_NULL) {                                 /* Validate vol name ptr.                               */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE VOL LOCK ----------------- */
    p_vol = FSVol_AcquireLockChk(name_vol, DEF_YES, p_err);     /* Vol may NOT be unmounted.                            */
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_vol == DEF_NULL) {
        return;
    }


                                                                /* ------------------ STOP JOURNALING ----------------- */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_OPEN;

    } else if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_STARTED;

    } else {
        DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START);
       *p_err = FS_ERR_NONE;
    }


                                                                /* ----------------- RELEASE VOL LOCK ----------------- */
    FSVol_ReleaseUnlock(p_vol);
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
*                                     FS_FAT_JournalModuleInit()
*
* Description : Initialize FAT Journal module.
*
* Argument(s) : vol_cnt     Number of volumes in use.
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

void  FS_FAT_JournalModuleInit (FS_QTY   vol_cnt,
                                FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


                                                                /* ----------- CREATE JOURNAL FILE DATA POOL ---------- */
    Mem_PoolCreate(&FS_FAT_JournalFileDataPool,
                    DEF_NULL,
                    0,
                    vol_cnt,
                    sizeof(FS_FAT_FILE_DATA),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FS_FAT_JournalModuleInit(): Could not alloc mem for journal file info pool: %d octets req'd.\r\n", octets_reqd));
        return;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalInit()
*
* Description : Initialize journal.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE          Journal initialized.
*                               FS_ERR_POOL_EMPTY    File info not available.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to get the journal file data from the journal file
*                   data pool.
*********************************************************************************************************
*/

void  FS_FAT_JournalInit (FS_VOL  *p_vol,
                          FS_ERR  *p_err)
{
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_DATA       *p_fat_data;
    LIB_ERR            pool_err;


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ----------------- INIT JOURNAL DATA ---------------- */
    DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN  |
                                          FS_FAT_JOURNAL_STATE_START |
                                          FS_FAT_JOURNAL_STATE_REPLAY);
    p_fat_data->JournalDataPtr = DEF_NULL;


                                                                /* -------------- ALLOC JOURNAL FILE DATA ------------- */
    FS_OS_Lock(p_err);                                          /* Acquire FS lock (see Note #1).                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_journal_data = (FS_FAT_FILE_DATA *)Mem_PoolBlkGet(&FS_FAT_JournalFileDataPool,
                                                         sizeof(FS_FAT_FILE_DATA),
                                                        &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */

    FS_OS_Unlock();

    if (p_journal_data == DEF_NULL) {
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    p_fat_data->JournalDataPtr = p_journal_data;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalExit()
*
* Description : Free journal file data memory.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE    Journal exited.
*
* Return(s)   : none.
*
* Note(s)     : (1) The file system lock MUST be held to free the journal file data back to the journal
*                   file data pool OR to free the journal buf back to the journal buf pool.
*********************************************************************************************************
*/

void  FS_FAT_JournalExit (FS_VOL  *p_vol,
                          FS_ERR  *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    LIB_ERR            pool_err;


    FS_OS_Lock(p_err);                                          /* Acquire FS lock (see Note #1).                       */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* -------------- FREE JOURNAL FILE DATA -------------- */
    p_journal_data =  p_fat_data->JournalDataPtr;               /* Free journal file data.                              */
    if (p_journal_data != DEF_NULL) {
        Mem_PoolBlkFree(        &FS_FAT_JournalFileDataPool,
                        (void *) p_journal_data,
                                &pool_err);
        if (pool_err != LIB_MEM_ERR_NONE) {
           *p_err = FS_ERR_MEM_ALLOC;
        }
    } else {
       *p_err = FS_ERR_VOL_JOURNAL_NOT_STARTED;
        FS_OS_Unlock();
        return;
    }

    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*                                         FS_FAT_JournalClr()
*
* Description : Clear a portion of the journal.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                         Journal cleared.
**                          FS_ERR_VOL_JOURNAL_FILE_INVALID     Journal file invalid.
*
*                           -------------------RETURNED BY FSBuf_Set()-------------------
*                           See FSBuf_Set() for additional return error codes.
*
*                           ---------------RETURNED BY FS_FAT_SecNextGet()---------------
*                           See FS_FAT_SecNextGet() for additional return error codes.
*
*                           ----------------RETURNED BY FSBuf_MarkDirty()----------------
*                           See FSBuf_MarkDirty() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_JournalSetPos()-------------
*                           See FS_FAT_JournalSetPos() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

void  FS_FAT_JournalClr (FS_VOL            *p_vol,
                         FS_BUF            *p_buf,
                         FS_FAT_FILE_SIZE   start_pos,
                         FS_FAT_FILE_SIZE   len,
                         FS_ERR            *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_SEC_NBR     first_sec;
    FS_FAT_SEC_NBR     cur_sec;
    FS_SEC_SIZE        cur_sec_pos;
    FS_FAT_FILE_SIZE   rem_size;
    FS_FAT_FILE_SIZE   wr_size;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;


                                                                /* ------------------- CHK JOURNAL -------------------- */
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_OPEN) == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return;
    }

    first_sec   =  FS_FAT_CLUS_TO_SEC(p_fat_data, p_journal_data->FileFirstClus);
    cur_sec     =  first_sec + FS_UTIL_DIV_PWR2(start_pos, p_fat_data->SecSizeLog2);
    cur_sec_pos = (start_pos & (p_fat_data->SecSize - 1u));
    rem_size    =  len;


                                                                /* ------------------- CLR JOURNAL -------------------- */
    while (rem_size != 0u) {
                                                                /* Update sizes.                                        */
        wr_size   = DEF_MIN(p_fat_data->SecSize - cur_sec_pos, rem_size);
        rem_size -= wr_size;
                                                                /* Set buf.                                             */
        FSBuf_Set(p_buf,
                  cur_sec,
                  FS_VOL_SEC_TYPE_FILE,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

                                                                /* Clr buf.                                             */
        Mem_Clr((void *)((CPU_INT08U *)p_buf->DataPtr + cur_sec_pos), (CPU_SIZE_T)wr_size);

                                                                /* Mark buf dirty.                                      */
        FSBuf_MarkDirty(p_buf, p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        if (rem_size != 0u) {                                   /* If sec to be cleared rem ...                         */
            cur_sec = FS_FAT_SecNextGet(p_vol,                  /*                          ... get next sec.           */
                                        p_buf,
                                        cur_sec,
                                        p_err);
            if (*p_err != FS_ERR_NONE) {
                *p_err = FS_ERR_VOL_JOURNAL_FILE_INVALID;
                return;
            }
            cur_sec_pos = 0u;                                   /* Reset sec pos.                                        */
        }
    }

    FS_TRACE_LOG(("FS_FAT_JournalClr(): %d octets cleared from position %d.\r\n", len, start_pos));
}


/*
*********************************************************************************************************
*                                      FS_FAT_JournalClrAllReset()
*
* Description : Clear entire journal and reset current position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           --------------RETURNED BY FS_FAT_JournalPosSet()-------------
*                           See FS_FAT_JournalPosSet() for additional return error codes.
*
*                           ----------------RETURNED BY FS_FAT_JournalClr()--------------
*                           See FS_FAT_JournalClr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Journal is cleared from first to last sector. This allows the journal to be
*                   atomically invalidated since a single sector clear will remove the first enter
*                   mark. If this mark is not present, journal will not be replayed (see
*                   FS_FAT_JournalReplay() notes).
*
*********************************************************************************************************
*/

void  FS_FAT_JournalClrAllReset (FS_VOL  *p_vol,
                                 FS_BUF  *p_buf,
                                 FS_ERR  *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* Clr entire journal (See Note #1)                     */
    FS_FAT_JournalClr(p_vol,
                      p_buf,
                      0u,
                      p_journal_data->FileSize,
                      p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* Reset current position.                              */
    FS_FAT_JournalPosSet(p_vol,
                         p_buf,
                         0u,
                         p_err);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalClrReset()
*
* Description : Clear journal up to current position and reset current position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           --------------RETURNED BY FS_FAT_JournalPosSet()-------------
*                           See FS_FAT_JournalPosSet() for additional return error codes.
*
*                           ----------------RETURNED BY FS_FAT_JournalClr()--------------
*                           See FS_FAT_JournalClr() for additional return error codes..
*
* Return(s)   : none.
*
* Note(s)     : (1) Journal is cleared from first to last sector. This allows the journal to be
*                   atomically invalidated since a single sector clear will remove the first enter
*                   mark. If this mark is not present, journal will not be replayed (see
*                   FS_FAT_JournalReplay() notes).
*
*********************************************************************************************************
*/

void  FS_FAT_JournalClrReset(FS_VOL  *p_vol,
                             FS_BUF  *p_buf,
                             FS_ERR  *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* Clr journal up to current position (See Note #1).    */
    FS_FAT_JournalClr(p_vol,
                      p_buf,
                      0u,
                      p_journal_data->FilePos,
                      p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* Reset current position.                              */
    FS_FAT_JournalPosSet(p_vol,
                         p_buf,
                         0u,
                         p_err);
}


/*
*********************************************************************************************************
*                                 FS_FAT_JournalEnterClusChainAlloc()
*
* Description : Append a cluster chain allocate log to the journal.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  First cluster of the chain from which allocation/extension will take place.
*
*               is_new      Indicates if the cluster chain is new :
*
*                               DEF_YES, first cluster of chain will be     allocated.
*                               DEF_NO,  first cluster of chain was already allocated.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE     Journal log entered.
*
*                               --------------RETURNED BY FS_FAT_JournalWr()-------------
*                               See FS_FAT_JournalWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_JournalEnterClusChainAlloc (FS_VOL           *p_vol,
                                         FS_BUF           *p_buf,
                                         FS_FAT_CLUS_NBR   start_clus,
                                         CPU_BOOLEAN       is_new,
                                         FS_ERR           *p_err)
{
    FS_FAT_DATA  *p_fat_data;
    CPU_INT08U    log_buf[FS_FAT_JOURNAL_LOG_CLUS_CHAIN_ALLOC_SIZE];


                                                                /* ---------------- CHK JOURNAL STATE ----------------- */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return;
    }
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_REPLAYING;
        FS_TRACE_LOG(("FS_FAT_JournalEnterClusChainAlloc(): Journal still replaying.\r\n"));
        return;
    }

                                                                /* --------------------- FORM LOG --------------------- */
    MEM_VAL_SET_INT16U_LITTLE((void *)&log_buf[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_ENTER_MARK_OFFSET],     FS_FAT_JOURNAL_MARK_ENTER);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log_buf[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_SIG_OFFSET],            FS_FAT_JOURNAL_SIG_CLUS_CHAIN_ALLOC);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log_buf[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_START_CLUS_OFFSET],     start_clus);
    MEM_VAL_SET_INT08U_LITTLE((void *)&log_buf[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_IS_NEW_OFFSET],         is_new);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log_buf[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_ENTER_END_MARK_OFFSET], FS_FAT_JOURNAL_MARK_ENTER_END);


                                                                /* ----------------- WR LOG TO JOURNAL ---------------- */
    FS_TRACE_LOG(("FS_FAT_JournalEnterClusChainAlloc(): Wr'ing log (enter) for 0x%04X.\r\n", FS_FAT_JOURNAL_SIG_CLUS_CHAIN_ALLOC));
    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log_buf[0],
                     FS_FAT_JOURNAL_LOG_CLUS_CHAIN_ALLOC_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }
}


/*
*********************************************************************************************************
*                                 FS_FAT_JournalEnterClusChainDel()
*
* Description : Append a cluster chain deletion log to the journal.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               p_buf       Pointer to temporary buffer.
*
*               start_clus  Cluster number to start deletion from.
*
*               nbr_clus    Number of clusters to delete.
*
*               del_first   Indicates whether or not the start cluster should be freed.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                         Journal log entered.
*
*                               ------------RETURNED BY FS_FAT_ClusChainFollow()--------------
*                               See FS_FAT_ClusChainFollow() for additional return error codes.
*
*                               -----------------RETURNED BY FSBuf_Flush()--------------------
*                               See FSBuf_Flush() for additional return error codes.
*
*                               --------------RETURNED BY FS_FAT_JournalWr()------------------
*                               See FS_FAT_JournalWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) A second buffer is used to prevent a single buffer from continuously switching between
*                   journal sector(s) and cluster chain sector(s). If no second buffer is available, a
*                   significant performance hit can be expected.
*********************************************************************************************************
*/

void  FS_FAT_JournalEnterClusChainDel (FS_VOL           *p_vol,
                                       FS_BUF           *p_buf,
                                       FS_FAT_CLUS_NBR   start_clus,
                                       FS_FAT_CLUS_NBR   nbr_clus,
                                       CPU_BOOLEAN       del_first,
                                       FS_ERR           *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_BUF            *p_buf2;
    FS_FAT_CLUS_NBR    marker_step;
    CPU_INT32U         nbr_marker;
    CPU_INT08U         marker_size;
    FS_SEC_SIZE        cur_sec_free_size;
    FS_SEC_SIZE        marker_log_size;
    FS_SEC_SIZE        journal_free_size;
    FS_FAT_CLUS_NBR    cur_clus;
    FS_FAT_CLUS_NBR    next_clus;
    CPU_INT08U         log[FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE];
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT32U         nbr_marker_chk;
#endif


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* ---------------- CHK JOURNAL STATE ----------------- */
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return;
    }
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_REPLAYING;
        FS_TRACE_LOG(("FS_FAT_JournalEnterClusChainDel(): Journal still replaying.\r\n"));
        return;
    }


                                                                /* - COMPUTE MARKER STEP SIZE, MARKER CNT & LOG SIZE -- */
                                                                /* Find free space size in cur sec and journal.         */
    cur_sec_free_size = p_fat_data->SecSize - p_journal_data->FileCurSecPos;
    journal_free_size = p_journal_data->FileSize - p_journal_data->FilePos;
    if (cur_sec_free_size <= (FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE + FS_FAT_JOURNAL_LOG_MARK_SIZE)) {
        marker_log_size = p_fat_data->SecSize + cur_sec_free_size       -
                         (FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE +
                          FS_FAT_JOURNAL_LOG_MARK_SIZE);
    } else {
        marker_log_size = cur_sec_free_size - (FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE + FS_FAT_JOURNAL_LOG_MARK_SIZE);
    }

    do {                                                        /* Compute marker step size.                            */
                                                                /* No bit packing used for 12-bit clus nbr.             */
        marker_size = (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT32) ? 4u : 2u;
        marker_step = (((nbr_clus * marker_size) - 1u) / marker_log_size) + 1u;
                                                                /* Log size increased until marker step is apporpriate. */
        marker_log_size += p_fat_data->SecSize;
    } while ((marker_step > FS_FAT_JOURNAL_MAX_DEL_MARKER_STEP_SIZE) &&
             (marker_log_size <= journal_free_size - (FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE +
                                                      FS_FAT_JOURNAL_LOG_MARK_SIZE)));
    if (marker_step > FS_FAT_JOURNAL_MAX_DEL_MARKER_STEP_SIZE){
        FS_TRACE_DBG(("FS_FAT_JournalEnterClusChainDel(): Unable to fit all markers in journal with step size of %d.\r\n", FS_FAT_JOURNAL_MAX_DEL_MARKER_STEP_SIZE));
        FS_TRACE_DBG(("FS_FAT_JournalEnterClusChainDel(): Step size used will be %d.\r\n", marker_step));
    }

    nbr_marker  =   (nbr_clus - 1u) / marker_step;               /* Compute nbr of markers to be log'd.                  */
    nbr_marker += (((nbr_clus - 1u) % marker_step) == 0u) ? 0u: 1u;


                                                                /* -------- LOG START, DEL FIRST & MARKER NBR --------- */
    FS_TRACE_LOG(("FS_FAT_JournalEnterClusChainDel(): Wr'ing log (enter) for 0x%04X.\r\n", FS_FAT_JOURNAL_SIG_CLUS_CHAIN_DEL));

    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_ENTER_MARK_OFFSET], FS_FAT_JOURNAL_MARK_ENTER);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_SIG_OFFSET]       , FS_FAT_JOURNAL_SIG_CLUS_CHAIN_DEL);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_NBR_MARKER_OFFSET], nbr_marker);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_START_CLUS_OFFSET], start_clus);
    MEM_VAL_SET_INT08U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_DEL_FIRST_OFFSET] , del_first);

    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }
                                                                /* Get 2nd buf if possible (see Note #1).               */
    p_buf2 = FSBuf_Get(p_vol);
    if (p_buf2 == DEF_NULL) {
         FS_TRACE_DBG(("FS_FAT_JournalEnterClusChainDel(): Unable to allocate second buffer. Clus chain deletion log will be written with a performance hit.\r\n"));
         p_buf2 = p_buf;
    }

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    nbr_marker_chk = 0u;
#endif
    cur_clus = start_clus;
   *p_err = FS_ERR_NONE;
    while (*p_err == FS_ERR_NONE) {
                                                                /* ------------ FIND NEXT MARKER CLUS NBR ------------- */
        next_clus = FS_FAT_ClusChainFollow(p_vol,
                                           p_buf2,
                                           cur_clus,
                                           marker_step,
                                           DEF_NULL,
                                           p_err);
        if ((*p_err != FS_ERR_NONE) &&
            (*p_err != FS_ERR_SYS_CLUS_CHAIN_END_EARLY) &&
            (*p_err != FS_ERR_SYS_CLUS_INVALID)) {
            if (p_buf2 != p_buf) {
                FSBuf_Free(p_buf2);
            }
            return;
        }

        if (cur_clus == next_clus) {                            /* If cur clus has already been log'd ...               */
            break;                                              /* ... then all markers have been log'd.                */
        }

        cur_clus = next_clus;                                   /* Update cur clus.                                     */

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
        nbr_marker_chk++;                                       /* Update log'd clus cnt.                               */
#endif
                                                                /* -------------------- LOG MARKER -------------------- */
        if (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT12) {
            MEM_VAL_SET_INT16U_LITTLE((void *)&log[0], (cur_clus & 0xFFF));
            marker_size = 2u;
        } else if (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT16 ) {
            MEM_VAL_SET_INT16U_LITTLE((void *)&log[0], cur_clus);
            marker_size = 2u;
        } else {
            MEM_VAL_SET_INT32U_LITTLE((void *)&log[0], cur_clus);
            marker_size = 4u;
        }
        FS_FAT_JournalWr(p_vol,
                         p_buf,
                        &log[0],
                         marker_size,
                         p_err);
        if (*p_err != FS_ERR_NONE){
           if (p_buf2 != p_buf) {
               FSBuf_Free(p_buf2);
           }
           return;
        }
    }

                                                                /* ------------------ CHK MARKER CNT ------------------ */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (nbr_marker_chk != nbr_marker) {
        FS_TRACE_DBG(("FS_FAT_JournalEnterClusChainDel(): marker number mismatch:\r\n"));
        FS_TRACE_DBG(("FS_FAT_JournalEnterClusChainDel(): %d markers expected, %d markers logged.\r\n", nbr_marker, nbr_marker_chk));
       *p_err = FS_ERR_VOL_JOURNAL_MARKER_NBR_MISMATCH;
        return;
    }
#endif

                                                                /* ---------------- LOG ENTER END MARK ---------------- */
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[0], FS_FAT_JOURNAL_MARK_ENTER_END);
    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_MARK_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        if (p_buf2 != p_buf) {
            FSBuf_Free(p_buf2);
        }
        return;
    }

                                                                /* ---------------- ALL MARKERS LOG'D ----------------- */
    if (p_buf2 != p_buf) {                                      /* Free 2nd buf.                                        */
        FSBuf_Free(p_buf2);
    }
}


/*
*********************************************************************************************************
*                                  FS_FAT_JournalEnterEntryCreate()
*
* Description : Append a directory entry creation log to the journal
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to entry start position.
*
*               p_dir_end_pos       Pointer to entry end position.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE     Journal log entered.
*
*                                       --------------RETURNED BY FS_FAT_JournalWr()-------------
*                                       See FS_FAT_JournalWr() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_JournalEnterEntryCreate (FS_VOL          *p_vol,
                                      FS_BUF          *p_buf,
                                      FS_FAT_DIR_POS  *p_dir_start_pos,
                                      FS_FAT_DIR_POS  *p_dir_end_pos,
                                      FS_ERR          *p_err)
{
    FS_FAT_DATA  *p_fat_data;
    CPU_INT08U    log[FS_FAT_JOURNAL_LOG_ENTRY_CREATE_SIZE];


                                                                /* ---------------- CHK JOURNAL STATE ----------------- */
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return;
    }
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_REPLAYING;
        FS_TRACE_LOG(("FS_FAT_JournalEnterEntryCreate(): Journal still replaying.\r\n"));
        return;
    }

                                                                /* ---------------- LOG SEC NBR & POS ----------------- */
    FS_TRACE_LOG(("FS_FAT_JournalEnterEntryCreate(): Wr'ing log (enter) for 0x%04X.\r\n", FS_FAT_JOURNAL_SIG_ENTRY_CREATE));

    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_ENTER_MARK_OFFSET],     FS_FAT_JOURNAL_MARK_ENTER);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_SIG_OFFSET],            FS_FAT_JOURNAL_SIG_ENTRY_CREATE);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_NBR_OFFSET],  p_dir_start_pos->SecNbr);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_POS_OFFSET],  p_dir_start_pos->SecPos);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_NBR_OFFSET],    p_dir_end_pos->SecNbr);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_POS_OFFSET],    p_dir_end_pos->SecPos);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_ENTER_END_MARK_OFFSET], FS_FAT_JOURNAL_MARK_ENTER_END);

    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_ENTRY_CREATE_SIZE,
                     p_err);
}


/*
*********************************************************************************************************
*                                    FS_FAT_JournalEnterEntryUpdate()
*
* Description : Append a directory entry update/deletion log to the journal
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to entry start position.
*
*               p_dir_end_pos       Pointer to entry end position.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE             Journal log entered.
*                                       FS_ERR_ENTRY_CORRUPT    File system entry is corrupted.
*
*                                       -----------------RETURNED BY FSBuf_Set()------------------
*                                       See FSBuf_Set() for additional return error codes.
*
*                                       ----------------RETURNED BY FSBuf_Flush()-----------------
*                                       See FSBuf_Flush() for additional return error codes.
*
*                                       --------------RETURNED BY FS_FAT_JournalWr()--------------
*                                       See FS_FAT_JournalWr() for additional return error codes.
*
*                                       --------------RETURNED BY FS_FAT_SecNextGet()-------------
*                                       See FS_FAT_SecNextGet() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) A second buffer is used to prevent a single buffer from continuously switching between
*                   journal sector(s) and directory entry sector(s). If no second buffer is available, a
*                   significant performance hit can be expected.
*
*               (2) The sector number gotten or allocated from the FAT should be valid. These checks are
*                   effectively redundant.
*********************************************************************************************************
*/

void  FS_FAT_JournalEnterEntryUpdate (FS_VOL          *p_vol,
                                      FS_BUF          *p_buf,
                                      FS_FAT_DIR_POS  *p_dir_start_pos,
                                      FS_FAT_DIR_POS  *p_dir_end_pos,
                                      FS_ERR          *p_err)
{
    FS_BUF          *p_buf2;
    FS_FAT_DATA     *p_fat_data;
    FS_FAT_DIR_POS   dir_cur_pos;
    CPU_INT08U      *p_dir_entry;
    FS_FAT_SEC_NBR   dir_next_sec;
    CPU_BOOLEAN      dir_sec_valid;
    CPU_INT08U       log[FS_FAT_SIZE_DIR_ENTRY];


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;


                                                                /* ---------------- CHK JOURNAL STATE ----------------- */
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_START) == DEF_NO) {
       *p_err = FS_ERR_NONE;
        return;
    }
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_YES) {
       *p_err = FS_ERR_VOL_JOURNAL_REPLAYING;
        FS_TRACE_LOG(("FS_FAT_JournalEnterEntryUpdate(): Journal still replaying.\r\n"));
        return;
    }


                                                                /* ---------------- LOG SEC NBR & POS ----------------- */
    FS_TRACE_LOG(("FS_FAT_JournalEnterEntryUpdate(): Wr'ing log (enter) for 0x%04X.\r\n", FS_FAT_JOURNAL_SIG_ENTRY_UPDATE));

    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_ENTER_MARK_OFFSET],    FS_FAT_JOURNAL_MARK_ENTER);
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_SIG_OFFSET],           FS_FAT_JOURNAL_SIG_ENTRY_UPDATE);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_NBR_OFFSET], p_dir_start_pos->SecNbr);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_POS_OFFSET], p_dir_start_pos->SecPos);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_NBR_OFFSET],   p_dir_end_pos->SecNbr);
    MEM_VAL_SET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_POS_OFFSET],   p_dir_end_pos->SecPos);

    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_ENTRY_DEL_HEADER_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* Get 2nd buf if possible (see Note #1).               */
    p_buf2 = FSBuf_Get(p_vol);
    if (p_buf2 == DEF_NULL) {
        FS_TRACE_DBG(("FS_FAT_JournalEnterEntryUpdate(): Unable to allocate second buffer. Dir entry update log will be written with a performance hit.\r\n"));
        p_buf2 = p_buf;
    }


                                                                /* ---------- ADD ALL DIR ENTRIES TO JOURNAL ---------- */
    dir_cur_pos   = *p_dir_start_pos;
    dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    while ( (dir_sec_valid      == DEF_YES) &&                  /* While sec is valid (see Note #2)  ...                */
           ((dir_cur_pos.SecNbr != p_dir_end_pos->SecNbr) ||    /* ... &     end sec     not reached ...                */
            (dir_cur_pos.SecPos <= p_dir_end_pos->SecPos))) {   /* ...    OR end sec pos not reached.                   */

                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {
            dir_next_sec = FS_FAT_SecNextGet(p_vol,             /* Get next sec in dir.                                 */
                                             p_buf2,
                                             dir_cur_pos.SecNbr,
                                             p_err);
            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                     FS_TRACE_INFO(("FS_FAT_JournalEnterEntryUpdate(): Unexpected end to dir after sec %d.\r\n", dir_cur_pos.SecNbr));
                    *p_err = FS_ERR_ENTRY_CORRUPT;
                     if (p_buf2 != p_buf) {
                         FSBuf_Free(p_buf2);
                     }
                     return;

                case FS_ERR_DEV:
                default:
                     if (p_buf2 != p_buf) {
                         FSBuf_Free(p_buf2);
                     }
                     return;
            }
                                                                /* Chk sec validity (see Note #2).                      */
            dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_next_sec);

                                                                /* Update cur sec.                                      */
            dir_cur_pos.SecNbr = dir_next_sec;
            dir_cur_pos.SecPos = 0u;

        }

        p_dir_entry = (CPU_INT08U *)p_buf2->DataPtr;

        FSBuf_Set(p_buf2,
                  dir_cur_pos.SecNbr,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            if (p_buf2 != p_buf) {
                FSBuf_Free(p_buf2);
            }
            return;
        }

                                                                /* -------------------- LOG ENTRY --------------------- */
        Mem_Copy(&log, (CPU_INT08U *)p_buf2->DataPtr + dir_cur_pos.SecPos, FS_FAT_SIZE_DIR_ENTRY);

        FS_FAT_JournalWr(p_vol,
                         p_buf,
                        &log[0],
                         FS_FAT_SIZE_DIR_ENTRY,
                         p_err);

                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
        dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
        p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
    }


                                                                /* ---------------- LOG ENTER END MARK ---------------- */
    MEM_VAL_SET_INT16U_LITTLE((void *)&log[0], FS_FAT_JOURNAL_MARK_ENTER_END);

    FS_FAT_JournalWr(p_vol,
                     p_buf,
                    &log,
                     FS_FAT_JOURNAL_LOG_MARK_SIZE,
                     p_err);

                                                                /* ---------------- ALL ENTRIES LOG'D ----------------- */
    if (p_buf2 != p_buf) {                                      /* Free 2nd buf.                                        */
        FSBuf_Free(p_buf2);
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
*                                     FS_FAT_JournalFileCreate()
*
* Description : Create journal file.
*
* Argument(s) : p_vol       Pointer to volume
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   Journal file created.
*                               FS_ERR_BUF_NONE_AVAIL         Buffer not available.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalFileCreate (FS_VOL  *p_vol,
                                        FS_ERR  *p_err)
{
    FS_BUF            *p_buf;
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_SEC_NBR     sec;
    FS_FAT_SEC_NBR     sec_cnt;
    FS_FAT_SEC_NBR     sec_nbr;
    FS_FAT_TIME        time_val;
    FS_FAT_DATE        date_val;
    CLK_DATE_TIME      date_time;
    CPU_INT08U        *p_dir_entry;
    CPU_BOOLEAN        success;


    p_fat_data     =  (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =   p_fat_data->JournalDataPtr;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------ VALIDATE JOURNAL FILE LEN ------------- */
    if (FS_UTIL_IS_PWR2(FS_FAT_JOURNAL_FILE_LEN) == DEF_NO) {
         FS_TRACE_DBG(("FS_FAT_JournalFileCreate(): Journal file len must be a power of 2.\r\n"));
        *p_err = FS_ERR_INVALID_CFG;
         return;
    }
#endif


                                                                /* ---------------- CREATE OR FIND FILE --------------- */
    FS_FAT_LowEntryFind(p_vol,                                  /* Create journal file.                                 */
                        p_journal_data,
                        FS_FAT_JOURNAL_FILE_NAME,
                       (FS_FILE_ACCESS_MODE_RD | FS_FAT_MODE_FILE | FS_FAT_MODE_CREATE | FS_FAT_MODE_TRUNCATE),
                        p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ----------------- ALLOC FIRST CLUS ----------------- */
    p_buf = FSBuf_Get(p_vol);
    if (p_buf == DEF_NULL) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    if (p_journal_data->FileFirstClus == 0u) {                  /* If no clus alloc'd to file ...                       */
        FS_FAT_LowFileFirstClusAdd(p_vol,                       /* ... alloc clus.                                      */
                                   p_journal_data,
                                   p_buf,
                                   p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }
    }

    FSBuf_Flush(p_buf, p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

                                                                /* ---------------- ALLOC OTHER CLUS's ---------------- */
    sec     = FS_FAT_CLUS_TO_SEC(p_fat_data, p_journal_data->FileFirstClus);
    sec_cnt = FS_FAT_JOURNAL_FILE_LEN >> p_fat_data->SecSizeLog2;
    sec_nbr = 0u;
    while (sec_nbr < sec_cnt) {
        FS_FAT_SecClr(p_vol,                                    /* Clr file clus.                                       */
                      p_buf->DataPtr,
                      sec,
                      1u,
                      p_vol->DevPtr->SecSize,
                      FS_VOL_SEC_TYPE_FILE,
                      p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }

        sec_nbr++;


        if (sec_nbr < sec_cnt) {                                /* Get next file sec.                                   */
            sec = FS_FAT_SecNextGetAlloc(p_vol,
                                         p_buf,
                                         sec,
                                         DEF_NO,
                                         p_err);
            if (*p_err != FS_ERR_NONE) {
                FSBuf_Free(p_buf);
                return;
            }
        }

        FSBuf_Flush(p_buf, p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }
    }


                                                                /* --------------- UPDATE FILE ATTRIB's --------------- */
    p_journal_data->FileSize  = FS_FAT_JOURNAL_FILE_LEN;        /* Update file len.                                     */
    p_journal_data->Attrib   |= FS_FAT_DIRENT_ATTR_HIDDEN;      /* Update file attrib's.                                */

    FSBuf_Set(p_buf,                                            /* Rd dir sec.                                          */
              p_journal_data->DirEndSec,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    success = Clk_GetDateTime(&date_time);                      /* Get date/time.                                       */
    if (success == DEF_YES) {
        date_val =  FS_FAT_DateFmt(&date_time);
        time_val =  FS_FAT_TimeFmt(&date_time);
    } else {
        date_val =  0u;
        time_val =  0u;
    }

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + p_journal_data->DirEndSecPos;
    MEM_VAL_SET_INT08U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_ATTR),       p_journal_data->Attrib);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FSTCLUSHI),  p_journal_data->FileFirstClus >> DEF_INT_16_NBR_BITS);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FSTCLUSLO),  p_journal_data->FileFirstClus &  DEF_INT_16_MASK);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTTIME),    time_val);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_WRTDATE),    date_val);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_LSTACCDATE), date_val);
    MEM_VAL_SET_INT32U_LITTLE((void *)(p_dir_entry + FS_FAT_DIRENT_OFF_FILESIZE),   p_journal_data->FileSize);

    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr dir sec.                                          */
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalPeek()
*
* Description : 'Peek' at arbitrarily located log in journal, without incrementing the file position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_log   Pointer to variable that will receive read data.
*
*               pos     Position where to start reading.
*
*               len     Size of data to be read in octets.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE     Data read from journal.
*
*                           -------------RETURNED BY FS_FAT_JournalSetPos()--------------
*                           See FS_FAT_JournalSetPos() for additional return error codes.
*
*                           ---------------RETURNED BY FS_FAT_JournalRd()----------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalPeek (FS_VOL      *p_vol,
                                  FS_BUF      *p_buf,
                                  void        *p_log,
                                  CPU_SIZE_T   pos,
                                  CPU_SIZE_T   len,
                                  FS_ERR      *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_FILE_SIZE   cur_file_pos;
    FS_ERR             temp_err;


    p_fat_data     = (FS_FAT_DATA  *)p_vol->DataPtr;
    p_journal_data = p_fat_data->JournalDataPtr;

                                                                /* ------------------- SAVE CUR POS ------------------- */
    cur_file_pos = p_journal_data->FilePos;

                                                                /* --------------------- SET POS ---------------------- */
    FS_FAT_JournalPosSet(p_vol,
                         p_buf,
                         pos,
                         p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* -------------------- RD JOURNAL -------------------- */
    FS_FAT_JournalRd(p_vol,
                     p_buf,
                     p_log,
                     len,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_FAT_JournalPosSet(p_vol,                             /* In case of err, rewind journal pos.                  */
                             p_buf,
                             cur_file_pos,
                            &temp_err);
        (void)temp_err;                                        /* Always rtn rd err.                                   */
        return;
    }

                                                                /* ------------------- RESTORE POS -------------------- */
    FS_FAT_JournalPosSet(p_vol,
                         p_buf,
                         cur_file_pos,
                         p_err);
}


/*
*********************************************************************************************************
*                                       FS_FAT_JournalPosSet()
*
* Description : Set journal file position, along with corresponding sector number and sector position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               pos     Journal file position to set.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE           Position set.
*                           FS_ERR_INVALID_ARG    Position greater than file size.
*
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalPosSet (FS_VOL      *p_vol,
                                    FS_BUF      *p_buf,
                                    CPU_SIZE_T   pos,
                                    FS_ERR      *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_SEC_NBR     cur_sec;
    FS_FAT_SEC_NBR     cur_sec_cnt;
    FS_FAT_SEC_NBR     sec_cnt;
    FS_FAT_SEC_NBR     sec_ix;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* --------------------- CHK ARGS --------------------- */
    if (pos > FS_FAT_JOURNAL_FILE_LEN) {
       *p_err = FS_ERR_INVALID_ARG;
        return;
    }
#endif

    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* ------------------- FIND SEC NBR ------------------- */
    sec_cnt     = FS_UTIL_DIV_PWR2(pos, p_fat_data->SecSizeLog2);
    cur_sec_cnt = FS_UTIL_DIV_PWR2(p_journal_data->FilePos, p_fat_data->SecSizeLog2);
    cur_sec     = FS_FAT_CLUS_TO_SEC(p_fat_data, p_journal_data->FileFirstClus);

    if (sec_cnt != cur_sec_cnt) {                              /* If new pos is not in cur sec ...                      */
        for (sec_ix = 0; sec_ix < sec_cnt; sec_ix++) {         /* ... find new sec nbr ...                              */
            cur_sec = FS_FAT_SecNextGet(p_vol,
                                        p_buf,
                                        cur_sec,
                                        p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }
    } else {
        cur_sec = p_journal_data->FileCurSec;                   /* ... else do not change sec nbr.                      */
    }

                                                                /* --------------------- SET POS ---------------------- */
    p_journal_data->FilePos       =  pos;
    p_journal_data->FileCurSec    =  cur_sec;
    p_journal_data->FileCurSecPos = (pos & (p_fat_data->SecSize - 1u));

   *p_err = FS_ERR_NONE;
}

/*
*********************************************************************************************************
*                                        FS_FAT_JournalRd()
*
* Description : Read log from journal & increment file position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_log   Pointer to buffer that will receive log data.
*
*               len     Size of data to be read in octets.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_JOURNAL_FULL     Journal full.
*                           FS_ERR_NONE             Log written to journal.
*
*                           ------------------RETURNED BY FSBuf_Set()-----------------
*                           See FSBuf_Set() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_SecNextGet()-------------
*                           See FS_FAT_SecNextGet() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalRd (FS_VOL      *p_vol,
                                FS_BUF      *p_buf,
                                void        *p_log,
                                CPU_SIZE_T   len,
                                FS_ERR      *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_FILE_SIZE   file_pos_end;
    CPU_SIZE_T         rd_size;
    CPU_SIZE_T         rem_size;
    FS_SEC_SIZE        cur_sec_pos;
    FS_FAT_SEC_NBR     cur_sec;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* ----------------- CHK IF VALID POS ----------------- */
    file_pos_end   = p_journal_data->FilePos + len;
    if (file_pos_end > FS_FAT_JOURNAL_FILE_LEN) {
        FS_TRACE_DBG(("FS_FAT_JournalRd(): Attempt to rd beyond journal file end.\r\n"));
       *p_err = FS_ERR_INVALID_ARG;
        return;
    }


    rem_size    = len;
    cur_sec     = p_journal_data->FileCurSec;
    cur_sec_pos = p_journal_data->FileCurSecPos;
    do {
                                                                /* --------------------- READ SEC --------------------- */
                                                                /* Compute next rd chunk size.                          */
        rd_size = DEF_MIN((p_fat_data->SecSize - cur_sec_pos), rem_size);
        FSBuf_Set(p_buf,
                  cur_sec,
                  FS_VOL_SEC_TYPE_FILE,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

                                                                /* ----------------- COPY TO DEST BUF ----------------- */
        Mem_Copy((void *)  p_log,
                 (void *)((CPU_INT08U *)p_buf->DataPtr + cur_sec_pos),
                           rd_size);

                                                                /* ----------- UPDATE SEC POS AND REM SIZE ------------ */
        cur_sec_pos = (cur_sec_pos + rd_size) & (p_fat_data->SecSize - 1u);
        rem_size -= rd_size;


                                                                /* ------------------- GET NEXT SEC ------------------- */
        if ((cur_sec_pos == 0u) && (rem_size != 0u)) {          /* If we crossed sec boundary amd data rem ...          */
            cur_sec = FS_FAT_SecNextGet(p_vol,                  /* ... find next sec.                                   */
                                        p_buf,
                                        cur_sec,
                                        p_err);
            if (*p_err != FS_ERR_NONE) {
                return;
            }
        }

    } while (rem_size != 0u);

                                                                /* -------------------- UPDATE POS -------------------- */
    p_journal_data->FilePos       = file_pos_end;
    p_journal_data->FileCurSec    = cur_sec;
    p_journal_data->FileCurSecPos = cur_sec_pos;
}


/*
*********************************************************************************************************
*                                    FS_FAT_JournalRd_INTxxU()
*
* Description : Read 16-/32-bit value from journal & increment file position.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE     Journal read.
*
*                           ---------------RETURNED BY FS_FAT_JournalRd()-------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
* Return(s)   : 16/32-bit value.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  FS_FAT_JournalRd_INT16U (FS_VOL  *p_vol,
                                             FS_BUF  *p_buf,
                                             FS_ERR  *p_err)
{
    CPU_INT16U  val;
    CPU_INT08U  log[2u];


    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     2u,
                     p_err);

    val = MEM_VAL_GET_INT16U_LITTLE(&log[0]);
    return (val);
}


static  CPU_INT32U  FS_FAT_JournalRd_INT32U (FS_VOL  *p_vol,
                                             FS_BUF  *p_buf,
                                             FS_ERR  *p_err)
{
    CPU_INT32U  val;
    CPU_INT08U  log[4u];


    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     4u,
                     p_err);

    val = MEM_VAL_GET_INT32U_LITTLE(&log[0]);
    return (val);
}


/*
*********************************************************************************************************
*                                        FS_FAT_JournalWr()
*
* Description : Write log to journal.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_log   Pointer to log buffer.
*
*               len     Length of log, in octets.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                         Log written to journal.
*                           FS_ERR_JOURNAL_FULL                 Journal full.
*                           FS_ERR_VOL_JOURNAL_FILE_INVALID     Journal file invalid.
*
*                           ------------------RETURNED BY FSBuf_Set()------------------
*                           See FSBuf_Set() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_SecNextGet()--------------
*                           See FS_FAT_SecNextGet() for additional return error codes.
*
*                           ---------------RETURNED BY FSBuf_MarkDirty()---------------
*                           See FSBuf_MarkDirty() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalWr (FS_VOL      *p_vol,
                                FS_BUF      *p_buf,
                                void        *p_log,
                                CPU_SIZE_T   len,
                                FS_ERR      *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_FAT_FILE_SIZE   file_pos_end;
    CPU_SIZE_T         wr_size;
    CPU_SIZE_T         rem_size;
    FS_SEC_SIZE        cur_sec_pos;
    FS_FAT_SEC_NBR     cur_sec;


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* -------------- CHK IF WR OUT OF BOUNDS ------------- */
    file_pos_end   = p_journal_data->FilePos + len;
    if (file_pos_end > FS_FAT_JOURNAL_FILE_LEN) {
       *p_err = FS_ERR_VOL_JOURNAL_FULL;
        return;
    }


    rem_size    = len;
    cur_sec     = p_journal_data->FileCurSec;
    cur_sec_pos = p_journal_data->FileCurSecPos;
    do {
                                                                /* Compute next rd chunk size.                          */
        wr_size  = DEF_MIN((p_fat_data->SecSize - cur_sec_pos), rem_size);

        FSBuf_Set(p_buf,                                        /* Set buf.                                             */
                  cur_sec,
                  FS_VOL_SEC_TYPE_FILE,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

                                                                /* ----------------- WR LOG TO JOURNAL ---------------- */
                                                                /* Copy log into buf.                                   */
        Mem_Copy((CPU_INT08U *)p_buf->DataPtr + cur_sec_pos,
                 (void *)p_log,
                  wr_size);
                                                                /* Udpate cur sec pos & rem size.                       */
        cur_sec_pos = (cur_sec_pos + wr_size) & (p_fat_data->SecSize - 1u);
        rem_size -= wr_size;

        FSBuf_MarkDirty(p_buf, p_err);                          /* Mark buf as dirty.                                   */
        if (*p_err != FS_ERR_NONE) {
            return;
        }

                                                                /* ------------------- GET NEXT SEC ------------------- */
        if ((cur_sec_pos == 0u) && (rem_size > 0)) {            /* If we crossed sec boundary and data rem ...          */
            cur_sec = FS_FAT_SecNextGet(p_vol,                  /* ... get next sec.                                    */
                                        p_buf,
                                        cur_sec,
                                        p_err);
            if (*p_err != FS_ERR_NONE) {                        /* No next sec in file ...                              */
               *p_err = FS_ERR_VOL_JOURNAL_FILE_INVALID;
                return;
            }
        }

    } while(rem_size > 0);

                                                                /* -------------------- UPDATE POS -------------------- */
    p_journal_data->FilePos       = file_pos_end;
    p_journal_data->FileCurSec    = cur_sec;
    p_journal_data->FileCurSecPos = cur_sec_pos;
}


/*
*********************************************************************************************************
*                                       FS_FAT_JournalReverseScan()
*
* Description : Scan journal backward, looking for given pattern.
*
* Argument(s) : p_vol           Pointer to volume.
*
*               p_buf           Pointer to temporary buffer.
*
*               start_pos       Position to start scan at.
*
*               pattern         Pattern to look for.
*
*               pattern_size    Pattern size in octets.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                          Pattern found.
*                                   FS_ERR_VOL_JOURNAL_LOG_INCOMPLETE    Pattern not found.
*
*                                   -------------RETURNED BY FS_FAT_JournalPeek()--------------
*                                   See FS_FAT_JournalPeek() for additional return error codes.
*
* Return(s)   : Start position of first pattern occurence if match occurs. Zero if no match.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_SIZE_T  FS_FAT_JournalReverseScan (FS_VOL      *p_vol,
                                               FS_BUF      *p_buf,
                                               CPU_SIZE_T   start_pos,
                                               CPU_INT08U  *p_pattern,
                                               CPU_SIZE_T   pattern_size,
                                               FS_ERR      *p_err)
{
    CPU_INT08U   octet_val;
    CPU_INT08U  *p_pattern_pos;
    CPU_INT08U  *p_pattern_start;
    CPU_INT08U  *p_pattern_end;
    CPU_SIZE_T   cur_pos;


                                                                /* ----------------- SET PARTTERN POS ----------------- */
    p_pattern_start = (CPU_INT08U *)p_pattern;
    p_pattern_end   = p_pattern_start + pattern_size - 1u;
    p_pattern_pos   = p_pattern_end;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (start_pos > FS_FAT_JOURNAL_FILE_LEN - 1u) {
        *p_err = FS_ERR_INVALID_ARG;
         return (0u);
    }
#endif

    cur_pos = start_pos + 1u;
    while (cur_pos != 0u) {
                                                                /* ---------------- PEEK AT NEXT OCTET ---------------- */
        cur_pos--;
        FS_FAT_JournalPosSet(p_vol,                             /* Set pos.                                             */
                             p_buf,
                             cur_pos,
                             p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

        FS_FAT_JournalRd(p_vol,                                 /* Rd journal.                                          */
                         p_buf,
                         &octet_val,
                         1u,
                         p_err);
        if (*p_err != FS_ERR_NONE) {
            return (0u);
        }

                                                                /* ------------------- CHK IF MATCH ------------------- */
        if (octet_val == *p_pattern_pos) {                      /* If octet match occurs ...                            */
            if (p_pattern_pos == p_pattern_start) {             /* ... if pattern match occurs ...                      */
                return cur_pos;                                 /* ... rtn cur pos OR ...                               */
            }                                                   /* ... if no pattern match occurs ...                   */
            p_pattern_pos--;                                    /* ... look for next octet.                             */
        } else {                                                /* If no octet match occurs ...                         */
            p_pattern_pos = p_pattern_end;                      /* ... reset cur pattern pos.                           */
        }
    }

                                                                /* --------------------- NO MATCH --------------------- */
   *p_err = FS_ERR_VOL_JOURNAL_LOG_INCOMPLETE;
    return (0u);
}


/*
*********************************************************************************************************
*                                       FS_FAT_JournalReplay()
*
* Description : Replay journal. Revert partially completed top level FAT operations involving cluster
*               chain allocation and/or directory entry creation/deletion. Top level FAT operations
*               involving cluster chain deletion will be completed.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                         Journal replayed.
*                           FS_ERR_BUF_NONE_AVAIL               No buffer available.
*
*
*                           --------------RETURNED BY FS_FAT_JournalRevertEntryUpdate()---------------
*                           See FS_FAT_JournalRevertEntryUpdate() for additional return error codes.
*
*                           --------------------RETURNED BY FS_FAT_JournalPeek()----------------------
*                           See FS_FAT_JournalPeek() for additional return error codes.
*
*                           -------------------RETURNED BY FS_FAT_JournalSetPos()---------------------
*                           See FS_FAT_JournalSetPos() for additional return error codes.
*
*                           ------------------------RETURNED BY FSBuf_Flush()-------------------------
*                           See FSBuf_Flush() for additional return error codes.
*
*                           ---------------------RETURNED BY FS_FAT_JournalClr()----------------------
*                           See FS_FAT_JournalClr() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_JournalRevertEntryCreate()---------------
*                           See FS_FAT_JournalRevertEntryCreate() for additional return error codes.
*
*                           -------------RETURNED BY FS_FAT_JournalRevertClusChainAlloc()-------------
*                           See FS_FAT_JournalRevertClusChainAlloc() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_JournalReplayClusChainDel()--------------
*                           See FS_FAT_JournalReplayClusChainDel() for additional return error codes.
*
*                           -----------------RETURNED BY FS_FAT_JournalReverseScan()------------------
*                           See FS_FAT_JournalReverseScan() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) If the first 16-bit word read from the journal is not an enter mark, the journal is
*                   clear or has been partially cleared. This indicates that the last top level FAT operation
*                   previously performed has been completed. Therefore, no operation needs to be replayed/
*                   reverted and the journal is cleared.
*
*               (2) Since reverting a cluster chain deletion basically involves logging all deleted clusters,
*                   it is practically impossible to implement without huge performance penalty.
*
*                   (a) Cluster chain deletion is the only low level FAT operation that is completed upon replay.
*                       To accomodate this exception, the FAT layer is designed so that cluster chain deletion
*                       is always the last low level operation performed inside the containing top level
*                       operation.
*
*                   (b) Since the journal is replayed backward, the clus chain deletion log is the
*                       first log to be parsed. Once cluster chain deletion is completed, the containing
*                       top level operation is also completed (see Note 2a). The journal is then cleared
*                       and the replay is aborted.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalReplay (FS_VOL  *p_vol,
                                    FS_ERR  *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    FS_BUF            *p_buf;
    CPU_INT16U         mark;
    CPU_INT16U         sig;
    CPU_INT08U         buf[4u];
    CPU_SIZE_T         journal_pos;
    CPU_BOOLEAN        clus_chain_deleted;


    p_fat_data     = (FS_FAT_DATA  *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;

                                                                /* ------------------- REPLAY START ------------------- */
    DEF_BIT_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY);


    p_buf = FSBuf_Get(p_vol);                                   /* Get buf.                                             */
    if (p_buf == DEF_NULL) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }
                                                                /* ---------- CHK IF JOURNAL HAS BEEN CLR'D ----------- */
    FS_FAT_JournalPeek(p_vol,                                   /* See Note #1.                                         */
                       p_buf,
                      &buf[0],
                       0u,
                       FS_FAT_JOURNAL_LOG_MARK_SIZE,
                       p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

    mark = MEM_VAL_GET_INT16U_LITTLE(&buf[0]);
    if (mark != FS_FAT_JOURNAL_MARK_ENTER) {                    /* If first enter mark is not found ...                 */
                                                                /* ... then journal has been (partially) clr'd.         */
        FS_FAT_JournalClrAllReset(p_vol,                        /* Make sure journal is completely clr'd ...            */
                                  p_buf,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
        FSBuf_Flush(p_buf, p_err);
        FSBuf_Free(p_buf);
        DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY);
        return;                                                 /* ... and abort replay.                                */
    }


                                                                /* ----- FIND LAST COMPLETE ENTRY ENTER END MARK ------ */
    MEM_VAL_SET_INT16U_LITTLE(&buf[0], FS_FAT_JOURNAL_MARK_ENTER_END);
    journal_pos = FS_FAT_JournalReverseScan(p_vol,
                                            p_buf,
                                            FS_FAT_JOURNAL_FILE_LEN - 1u,
                                           &buf[0],
                                            FS_FAT_JOURNAL_LOG_MARK_SIZE,
                                            p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_VOL_JOURNAL_LOG_INCOMPLETE)) {
        FSBuf_Free(p_buf);
        return;
    }

    if (*p_err == FS_ERR_VOL_JOURNAL_LOG_INCOMPLETE) {          /* If no enter end mark is found ...                    */
                                                                /* ... then journal does not contain a complete entry.  */
        FS_FAT_JournalClrAllReset(p_vol,                        /* Make sure journal is completely clr'd ...            */
                                  p_buf,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
        FSBuf_Flush(p_buf, p_err);
        FSBuf_Free(p_buf);
        DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY);
        return;                                                 /* ... and abort replay.                                */
    }

    clus_chain_deleted = DEF_NO;
    while ((journal_pos > 0u) &&
           (clus_chain_deleted == DEF_NO)) {

                                                                /* --------------- FIND NEXT LOG ENTRY ---------------- */
        MEM_VAL_SET_INT16U_LITTLE(&buf[0], FS_FAT_JOURNAL_MARK_ENTER);
        journal_pos = FS_FAT_JournalReverseScan(p_vol,
                                                p_buf,
                                                journal_pos,
                                               &buf[0],
                                                FS_FAT_JOURNAL_LOG_MARK_SIZE,
                                                p_err);
        if ((*p_err != FS_ERR_NONE) &&
            (*p_err != FS_ERR_VOL_JOURNAL_LOG_INCOMPLETE)) {
            FSBuf_Free(p_buf);
            return;
        }

                                                                /* --------- SET POS AT LOG ENTRY ENTER MARK ---------- */
        FS_FAT_JournalPosSet(p_vol,
                             p_buf,
                             journal_pos,
                             p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }

                                                                /* --------------------- GET SIG ---------------------- */
        FS_FAT_JournalPeek(p_vol,                               /* Enter mark presence already asserted by prev scan ...*/
                           p_buf,
                          &buf[0],
                           p_journal_data->FilePos + FS_FAT_JOURNAL_LOG_MARK_SIZE,      /* ... so skip enter mark.      */
                           FS_FAT_JOURNAL_LOG_SIG_SIZE,
                           p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }

        sig = MEM_VAL_GET_INT16U_LITTLE(&buf[0]);


                                                                /* ---------------------- REPLAY ---------------------- */
        switch (sig) {
            case FS_FAT_JOURNAL_SIG_CLUS_CHAIN_ALLOC:
                 FS_FAT_JournalRevertClusChainAlloc(p_vol,      /* Revert clus chain alloc.                             */
                                                    p_buf,
                                                    p_err);
                 break;

            case FS_FAT_JOURNAL_SIG_CLUS_CHAIN_DEL:
                 FS_FAT_JournalReplayClusChainDel(p_vol,        /* Complete clus chain del (See Note #2a).              */
                                                  p_buf,
                                                  p_err);
                 clus_chain_deleted = DEF_YES;                  /* Clus chain has been del'd (See Note #2b).            */
                 break;

            case FS_FAT_JOURNAL_SIG_ENTRY_CREATE:
                 FS_FAT_JournalRevertEntryCreate(p_vol,         /* Revert dir entry creation.                           */
                                                 p_buf,
                                                 p_err);
                 break;

            case FS_FAT_JOURNAL_SIG_ENTRY_UPDATE:
                 FS_FAT_JournalRevertEntryUpdate(p_vol,         /* Revert dir entry update/del.                         */
                                                 p_buf,
                                                 p_err);
                 break;

            default:
                 FS_TRACE_LOG(("FS_FAT_JournalReplay(): Unknown journal sig: 0x%04X\r\n", sig));
                *p_err = FS_ERR_NONE;
                 break;
        }
    }
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* -------------------- CLR JOURNAL ------------------- */
    FS_FAT_JournalClrAllReset(p_vol,
                              p_buf,
                              p_err);
    if (*p_err != FS_ERR_NONE) {
        FSBuf_Free(p_buf);
        return;
    }

                                                                /* ----------------- FLUSH & FREE BUF ----------------- */
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);

                                                                /* ------------------- REPLAY DONE -------------------- */
    DEF_BIT_CLR(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY);
}


/*
*********************************************************************************************************
*                                 FS_FAT_JournalRevertClusChainAlloc()
*
* Description : Revert cluster chain allocation based on journal log information.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                           Cluster chain allocation reverted.
*                           FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG    Invalid log args.
*
*                           ------------------RETURNED BY FS_FAT_JournalRd()-------------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
*                           -------------RETURNED BY FS_FAT_ClusChainReverseDel()--------------
*                           See FS_FAT_ClusChainReverseDel() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Enter mark, enter end mark and signature are validated by FS_FAT_JournalReplay().
*                   This function is called only if the corresponding journal log is complete.
*
*               (2) Log arguments are strictly validated.  If any error exists, the journal replay will
*                   be aborted.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalRevertClusChainAlloc (FS_VOL  *p_vol,
                                                  FS_BUF  *p_buf,
                                                  FS_ERR  *p_err)
{
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    FS_FAT_DATA      *p_fat_data;
#endif
    FS_FAT_CLUS_NBR   start_clus;
    CPU_BOOLEAN       del_first;
    CPU_INT08U        log[FS_FAT_JOURNAL_LOG_CLUS_CHAIN_ALLOC_SIZE];


                                                                /* ------------------ PARSE LOG ARGS ------------------ */
    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_CLUS_CHAIN_ALLOC_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* Ignore marks & sig (See Note #1).                    */
    start_clus = (FS_FAT_CLUS_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_START_CLUS_OFFSET]);
    del_first  = (CPU_BOOLEAN    )MEM_VAL_GET_INT08U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_ALLOC_LOG_IS_NEW_OFFSET]);


                                                                /* ---------- VALIDATE LOG ARGS (see Note #2) --------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
        return;
    }

    if ((del_first != DEF_YES) && (del_first != DEF_NO)) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
        return;
    }
#endif


                                                                /* ------------------- REVERT ALLOC ------------------- */
    FS_TRACE_LOG(("FS_FAT_JournalRevertClusChainAlloc(): Reverting op for 0x%04X.\r\n", FS_FAT_JOURNAL_SIG_CLUS_CHAIN_ALLOC));
    FS_FAT_ClusChainReverseDel(p_vol,
                               p_buf,
                               start_clus,
                               del_first,
                               p_err);
    if ((*p_err == FS_ERR_SYS_CLUS_INVALID)  &&                 /* If clus chain creation was not even started ...          */
        ( del_first == DEF_YES)) {                              /* ... then clus invalid err is expected.                   */
        *p_err = FS_ERR_NONE;
    }
}


/*
*********************************************************************************************************
*                                 FS_FAT_JournalReplayClusChainDel()
*
* Description : Complete cluster chain deletion based on journal log information.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                           Journal log replayed.
*                           FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG    Invalid log args.
*
*                           -----------------------RETURNED BY FS_FAT_JournalPeek()-----------------------
*                           See FS_FAT_JournalPeek() for additional return error codes.
*
*                           ----------------------RETURNED BY FS_FAT_ClusChainDel()-----------------------
*                           See FS_FAT_ClusChainDel() for additional return error codes.
*
*                           -----------------RETURNED BY FS_FAT_ClusChainReverseFollow()------------------
*                           See FS_FAT_ClusChainReverseFollow() for additional return error codes.
*
*                           -------------RETURNED BY p_fat_data->FAT_TypeAPI_Ptr->ClusValRd()-------------
*                           See p_fat_data->FAT_TypeAPI_Ptr->ClusValRd() for additional return error codes.
*
*                           -----------------------RETURNED BY FS_FAT_JournalRd()--------------------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) Enter mark, enter end mark and signature are validated by FS_FAT_JournalReplay().
*                   This function is called only if the corresponding journal log is complete.
*
*               (2) Log arguments are strictly validated.  If any error exists, the journal replay will
*                   be aborted.
*
*               (3) The start clus is handled separatly from the rest of the cluster chain for 2 different
*                   reasons, in 2 different scenarios:
*
*                       (a) In case a valid marker is found: if FAT12 is used, a FAT entry may span across
*                           2 sectors. This could lead to corrupted entry if a failure occurs between the 2
*                           sector write needed for the entry update. Although no such entry will be used when
*                           journaling is enabled (see FS_FAT_ClusFreeFind() notes) and mounting journaled
*                           volume under another host is strongly discouraged, the integrity of the end of
*                           cluster mark is enforced here for extra safety.
*
*                       (b) In case no markers were logged: if no markers were logged, the start cluster is
*                           the only cluster to be deleted. Since there are no markers to start first cluster
*                           lookup from, start cluster needs to be treated separatly.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalReplayClusChainDel (FS_VOL  *p_vol,
                                                FS_BUF  *p_buf,
                                                FS_ERR  *p_err)
{
    FS_FAT_DATA       *p_fat_data;
    FS_FAT_FILE_DATA  *p_journal_data;
    CPU_BOOLEAN        del;
    CPU_BOOLEAN        del_first;
    CPU_BOOLEAN        valid_marker_found;
    FS_FAT_CLUS_NBR    first_clus;
    FS_FAT_CLUS_NBR    start_clus;
    FS_FAT_CLUS_NBR    cur_marker;
    FS_FAT_CLUS_NBR    fat_entry;
    FS_FAT_CLUS_NBR    nbr_marker;
    FS_FAT_FILE_SIZE   end_mark_pos;
    CPU_INT08U         log[FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE];
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT32U         mark;
#endif


    p_fat_data     = (FS_FAT_DATA *)p_vol->DataPtr;
    p_journal_data =  p_fat_data->JournalDataPtr;


                                                                /* ------------------ PARSE LOG ARGS ------------------ */
    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_CLUS_CHAIN_DEL_HEADER_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }
                                                                /* Ignore marks & sig (see Note #1).                    */
    nbr_marker = (FS_FAT_CLUS_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_NBR_MARKER_OFFSET]);
    start_clus = (FS_FAT_CLUS_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_START_CLUS_OFFSET]);
    del_first  = (CPU_BOOLEAN    )MEM_VAL_GET_INT08U_LITTLE((void *)&log[FS_FAT_JOURNAL_CLUS_CHAIN_DEL_LOG_DEL_FIRST_OFFSET]);


                                                                /* ---------- VALIDATE LOG ARGS (see Note #2) --------- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (nbr_marker > p_fat_data->MaxClusNbr - FS_FAT_MIN_CLUS_NBR) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
        return;
    }

    if (FS_FAT_IS_VALID_CLUS(p_fat_data, start_clus) == DEF_NO) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
        return;
    }

    if ((del_first != DEF_YES) && (del_first != DEF_NO)) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
        return;
    }
#endif

                                                                /* ------------ COMPUTE ENTER END MARK POS ------------ */
    end_mark_pos = p_journal_data->FilePos + nbr_marker *
                  (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT32 ? 4u : 2u);


                                                                /* ---- CHK ENTER END MARK LOCATION (see Note #2) ----- */
#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (end_mark_pos > p_journal_data->FileSize) {
       *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;             /* End mark pos is invalid.                             */
        return;
    }
                                                                /* Peek at enter end mark.                              */
    FS_FAT_JournalPeek(p_vol,
                       p_buf,
                      &log[0],
                       end_mark_pos,
                       FS_FAT_JOURNAL_LOG_MARK_SIZE,
                       p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

   mark = MEM_VAL_GET_INT16U_LITTLE(&log[0]);
   if (mark != FS_FAT_JOURNAL_MARK_ENTER_END) {                 /* If no end mark where one is expected ...             */
      *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;              /* ... rtn err.                                         */
       return;
   }
#endif

                                                                /* ------------- FIND FIRST VALID MARKER -------------- */
   valid_marker_found = DEF_NO;
   while ((p_journal_data->FilePos < end_mark_pos) &&
          (valid_marker_found == DEF_NO)) {

       if (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT12) {
           cur_marker  = FS_FAT_JournalRd_INT16U(p_vol, p_buf, p_err);
           cur_marker &= 0xFFFu;
       } else if (p_fat_data->FAT_Type == FS_FAT_FAT_TYPE_FAT16 ) {
           cur_marker  = FS_FAT_JournalRd_INT16U(p_vol, p_buf, p_err);
       } else {
           cur_marker  = FS_FAT_JournalRd_INT32U(p_vol, p_buf, p_err);
       }
       if (*p_err != FS_ERR_NONE) {
           return;
       }

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ---------- VALIDATE LOG ARGS (see Note #2) --------- */
       if (FS_FAT_IS_VALID_CLUS(p_fat_data, cur_marker) == DEF_NO) {
          *p_err = FS_ERR_VOL_JOURNAL_LOG_INVALID_ARG;
           return;
       }
#endif
                                                                /* ------------------- RD FAT ENTRY ------------------- */
       fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,
                                                          p_buf,
                                                          cur_marker,
                                                          p_err);
       if (*p_err != FS_ERR_NONE) {
           return;
       }
                                                                /* Chk if valid marker found.                           */
       if ((FS_FAT_IS_VALID_CLUS(p_fat_data, fat_entry) == DEF_YES) ||
           (fat_entry >= p_fat_data->FAT_TypeAPI_Ptr->ClusEOF)) {
           valid_marker_found = DEF_YES;
       }
   }

                                                                /* -------------- NO VALID MARKER FOUND --------------- */

   if ((valid_marker_found == DEF_NO) && (nbr_marker > 0u)) {   /* If no valid marker found among log'd markers ...     */
       return;                                                  /* ... everything has already been del'd.               */
   }


                                                                /* --- VALID MARKER FOUND OR NO MARKERS WERE LOG'D ---- */
                                                                /* ------------------ CHK START CLUS ------------------ */
                                                                /* See Note #3a & 3b.                                   */
   fat_entry = p_fat_data->FAT_TypeAPI_Ptr->ClusValRd(p_vol,    /* Rd start of chain entry.                             */
                                                      p_buf,
                                                      start_clus,
                                                      p_err);
   if (*p_err != FS_ERR_NONE) {
       return;
   }

   if ((fat_entry < p_fat_data->FAT_TypeAPI_Ptr->ClusEOF) &&   /* If start clus has not been mark'd as EOC ...         */
       (del_first == DEF_NO)) {                                 /* ... while it should be ...                           */
       p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,            /*                        ... mark it as EOC.           */
                                              p_buf,
                                              start_clus,
                                              p_fat_data->FAT_TypeAPI_Ptr->ClusEOF,
                                              p_err);
       if (*p_err != FS_ERR_NONE) {
           return;
       }
   }

   if ((fat_entry != p_fat_data->FAT_TypeAPI_Ptr->ClusFree) &&  /* If start clus has not been mark'd as free ...        */
       (del_first == DEF_YES)) {                                /* ... while it should be ...                           */
       p_fat_data->FAT_TypeAPI_Ptr->ClusValWr(p_vol,            /*                        ... mark it as free.          */
                                              p_buf,
                                              start_clus,
                                              p_fat_data->FAT_TypeAPI_Ptr->ClusFree,
                                              p_err);
       if (*p_err != FS_ERR_NONE) {
           return;
       }
   }

   if (nbr_marker == 0u) {                                      /* If no markers were log'd ...                         */
       return;                                                  /* nothing to do beyond start clus handling.            */
   }


                                                                /* ---------- FIND BROKEN CHAIN START & DEL ----------- */
   first_clus = FS_FAT_ClusChainReverseFollow(p_vol,            /* Find rem clus chain start ...                        */
                                              p_buf,
                                              cur_marker,
                                              start_clus,
                                              p_err);
   if (*p_err != FS_ERR_NONE) {
       return;
   }

   del = (first_clus == start_clus) ? del_first : DEF_YES;      /* ... and del.                                         */
   (void)FS_FAT_ClusChainDel(p_vol,
                             p_buf,
                             first_clus,
                             del,
                             p_err);
}


/*
*********************************************************************************************************
*                                  FS_FAT_JournalRevertEntryCreate()
*
* Description : Revert directory entry creation based on journal log information.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE     Directory entry creation reverted.
*
*                           --------------------RETURNED BY FS_FAT_JournalRd()--------------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
*                           --------------RETURNED BY FS_FAT_FN_API_Active.DirEntryDel()-------------
*                           See FS_FAT_FN_API_Active.DirEntryDel() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)     : (1) A LFN can require as many as
*
*                               [  FS_FAT_MAX_FILE_NAME_LEN   ]
*                           ceil[-----------------------------] = ceil(255/13) = 20
*                               [ FS_FAT_DIRENT_LFN_NBR_CHARS ]
*
*                       separate directory entries.  Each directory entry is 32-bytes, so a single file
*                       name can occupy as many as 640 bytes of a directory & up to one full cluster
*                       plus part of another may be consumed.  Consequently, as many as two clusters may
*                       be allocated for the directory entry.
*
*               (2) Enter mark, enter end mark and signature are validated by FS_FAT_JournalReplay().
*                   This function is called only if the corresponding journal log is complete.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalRevertEntryCreate (FS_VOL  *p_vol,
                                               FS_BUF  *p_buf,
                                               FS_ERR  *p_err)
{
    FS_FAT_DIR_POS  dir_start_pos;
    FS_FAT_DIR_POS  dir_end_pos;
    CPU_INT08U      log[FS_FAT_JOURNAL_LOG_ENTRY_CREATE_SIZE];


                                                                /* ------------------ PARSE LOG ARGS ------------------ */
    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_ENTRY_CREATE_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }
                                                                /* Ignore marks & sig (see Note #2).                    */
    dir_start_pos.SecNbr = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_NBR_OFFSET]);
    dir_start_pos.SecPos = (FS_SEC_SIZE   )MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_START_SEC_POS_OFFSET]);
    dir_end_pos.SecNbr   = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_NBR_OFFSET]);
    dir_end_pos.SecPos   = (FS_SEC_SIZE   )MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_CREATE_LOG_END_SEC_POS_OFFSET]);


                                                                /* ------------------ DEL DIR ENTRY ------------------- */
    FS_FAT_FN_API_Active.DirEntryDel(p_vol,
                                     p_buf,
                                    &dir_start_pos,
                                    &dir_end_pos,
                                     p_err);
    if (*p_err != FS_ERR_NONE) {
        FS_TRACE_DBG(("FS_FAT_JournalRevertEntryCreate(): Unable to delete dir entry."));
    }
}


/*
*********************************************************************************************************
*                                   FS_FAT_JournalRevertEntryUpdate()
*
* Description : Revert directory entry update/deletion based on journal log information.
*
* Argument(s) : p_vol   Pointer to volume.
*
*               p_buf   Pointer to temporary buffer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                         Directory entry deletion reverted.
*
*                           -------------------------RETURNED BY FSBuf_Set()--------------------------
*                           See FSBuf_Set() for additional return error codes.
*
*                           ------------------------RETURNED BY FSBuf_Flush()-------------------------
*                           See FSBuf_Flush() for additional return error codes.
*
*                           --------------------RETURNED BY FS_FAT_JournalRd()---------------------
*                           See FS_FAT_JournalRd() for additional return error codes.
*
*                           ---------------------RETURNED BY FS_FAT_SecNextGet()----------------------
*                           See FS_FAT_SecNextGet() for additional return error codes.
*
*                           ----------------------RETURNED BY FSBuf_MarkDirty()-----------------------
*                           See FSBuf_MarkDirty() for additional return error codes.
*
* Return(s)   : none.
*
* Note(s)       (1) Enter mark, enter end mark and signature are validated by FS_FAT_JournalReplay().
*                   This function is called only if the corresponding journal log is complete.
*
*               (2) A second buffer is used to prevent a single buffer from continuously switching between
*                   journal sector(s) and directory entry sector(s). If no second buffer is available, the
*                   journal log operation will still be performed at the expense of a significant performance
*                   penalty.
*
*               (3) The sector number gotten or allocated from the FAT should be valid. These checks are
*                   effectively redundant.
*********************************************************************************************************
*/

static  void  FS_FAT_JournalRevertEntryUpdate (FS_VOL  *p_vol,
                                               FS_BUF  *p_buf,
                                               FS_ERR  *p_err)
{
    FS_FAT_DATA     *p_fat_data;
    FS_BUF          *p_buf2;
    CPU_INT08U      *p_dir_entry;
    FS_FAT_DIR_POS   dir_start_pos;
    FS_FAT_DIR_POS   dir_end_pos;
    FS_FAT_DIR_POS   dir_cur_pos;
    CPU_BOOLEAN      dir_sec_valid;
    FS_FAT_SEC_NBR   dir_next_sec;
    CPU_INT08U       log[FS_FAT_SIZE_DIR_ENTRY];


    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;

                                                                /* ------------------ PARSE LOG ARGS ------------------ */
    FS_FAT_JournalRd(p_vol,
                     p_buf,
                    &log[0],
                     FS_FAT_JOURNAL_LOG_ENTRY_DEL_HEADER_SIZE,
                     p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* Ignore marks & sig (see Note #1).                    */
    dir_start_pos.SecNbr = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_NBR_OFFSET]);
    dir_start_pos.SecPos = (FS_SEC_SIZE   )MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_START_SEC_POS_OFFSET]);
    dir_end_pos.SecNbr   = (FS_FAT_SEC_NBR)MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_NBR_OFFSET]);
    dir_end_pos.SecPos   = (FS_SEC_SIZE   )MEM_VAL_GET_INT32U_LITTLE((void *)&log[FS_FAT_JOURNAL_ENTRY_DEL_LOG_END_SEC_POS_OFFSET]);


                                                                /* Get 2nd buf if possible (see Note #2).               */
    p_buf2 = FSBuf_Get(p_vol);
    if (p_buf2 == DEF_NULL) {
        FS_TRACE_DBG(("FS_FAT_JournalRevertEntryUpdate(): Unable to allocate second buffer. Dir entry update will be reverted with a performance hit.\r\n"));
        p_buf2 = p_buf;
    }

    dir_cur_pos   = dir_start_pos;
    dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    while ( (dir_sec_valid      == DEF_YES) &&                  /* While sec is valid (see Note #3)  ...                */
           ((dir_cur_pos.SecNbr != dir_end_pos.SecNbr) ||       /* ... &     end sec     not reached ...                */
            (dir_cur_pos.SecPos <= dir_end_pos.SecPos))) {      /* ...    OR end sec pos not reached.                   */


                                                                /* --------------------- RD ENTRY ----------------------*/
        FS_FAT_JournalRd(p_vol,
                         p_buf,
                        &log[0],
                         FS_FAT_SIZE_DIR_ENTRY,
                         p_err);
        if (*p_err != FS_ERR_NONE) {
            if (p_buf2 != p_buf) {
                FSBuf_Free(p_buf2);
            }
            return;
        }

                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
        if (dir_cur_pos.SecPos == p_fat_data->SecSize) {

            dir_next_sec = FS_FAT_SecNextGet(p_vol,             /* Get next sec in dir.                                 */
                                             p_buf2,
                                             dir_cur_pos.SecNbr,
                                             p_err);
            switch (*p_err) {
                case FS_ERR_NONE:
                     break;

                case FS_ERR_SYS_CLUS_CHAIN_END:
                case FS_ERR_SYS_CLUS_INVALID:
                     FS_TRACE_INFO(("FS_FAT_JournalRevertEntryUpdate(): Unexpected end to dir after sec %d.\r\n", dir_cur_pos.SecNbr));
                    *p_err = FS_ERR_ENTRY_CORRUPT;
                     if (p_buf2 != p_buf) {
                         FSBuf_Free(p_buf2);
                     }
                     return;

                case FS_ERR_DEV:
                default:
                     if (p_buf2 != p_buf) {
                         FSBuf_Free(p_buf2);
                     }
                     return;
            }
                                                                /* Chk sec validity (see Note #3).                      */
            dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_next_sec);

            dir_cur_pos.SecNbr = dir_next_sec;
            dir_cur_pos.SecPos = 0u;
        }

        p_dir_entry = (CPU_INT08U *)p_buf2->DataPtr;

        if (dir_sec_valid == DEF_YES) {

            FSBuf_Set(p_buf2,                                   /* Set buf.                                             */
                      dir_cur_pos.SecNbr,
                      FS_VOL_SEC_TYPE_DIR,
                      DEF_YES,
                      p_err);
            if (*p_err != FS_ERR_NONE) {
                if (p_buf2 != p_buf) {
                    FSBuf_Free(p_buf2);
                }
                return;
            }
        }
                                                                /* Wr dir entry in buf.                                 */
        Mem_Copy(p_dir_entry + dir_cur_pos.SecPos, &log[0], FS_FAT_SIZE_DIR_ENTRY);

        FSBuf_MarkDirty(p_buf2, p_err);                         /* Mark buf as dirty.                                   */
        if (*p_err != FS_ERR_NONE) {
            if (p_buf2 != p_buf) {
                FSBuf_Free(p_buf2);
            }
            return;
        }
                                                                /* ---------------- MOVE TO NEXT ENTRY ---------------- */
        dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
        p_dir_entry        += FS_FAT_SIZE_DIR_ENTRY;
    }


                                                                /* ---------------- ENTRY DEL REVERT'D ---------------- */
    if (p_buf2 != p_buf) {                                      /* Free 2nd buf.                                        */
        FSBuf_Flush(p_buf2, p_err);
        FSBuf_Free(p_buf2);
    }
}


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/


#endif                                                          /* End of journaling module include (see Note #2).      */
#endif                                                          /* End of FAT        module include (see Note #1).      */
