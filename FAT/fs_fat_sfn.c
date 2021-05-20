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
*                                       SHORT FILE NAME SUPPORT
*
* Filename : fs_fat_sfn.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_FAT_SFN_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <Source/clk.h>
#include  "../Source/fs.h"
#include  "../Source/fs_buf.h"
#include  "../Source/fs_vol.h"
#include  "fs_fat.h"
#include  "fs_fat_journal.h"
#include  "fs_fat_sfn.h"


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
*                                           LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                                    /* --------- FILE NAME API -------- */
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void         FS_FAT_SFN_DirEntryCreate    (FS_VOL             *p_vol,       /* Create SFN dir entry.            */
                                                   FS_BUF             *p_buf,
                                                   CPU_CHAR           *name,
                                                   CPU_BOOLEAN         is_dir,
                                                   FS_FAT_CLUS_NBR     file_first_clus,
                                                   FS_FAT_DIR_POS     *p_dir_start_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);

static  void         FS_FAT_SFN_DirEntryCreateAt  (FS_VOL             *p_vol,       /* Create SFN dir entry at pos.     */
                                                   FS_BUF             *p_buf,
                                                   CPU_CHAR           *name,
                                                   FS_FILE_NAME_LEN    name_len,
                                                   CPU_INT32U          name_8_3[],
                                                   CPU_BOOLEAN         is_dir,
                                                   FS_FAT_CLUS_NBR     file_first_clus,
                                                   FS_FAT_DIR_POS     *p_dir_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);

static  void         FS_FAT_SFN_DirEntryDel       (FS_VOL             *p_vol,       /* Delete SFN dir entry.            */
                                                   FS_BUF             *p_buf,
                                                   FS_FAT_DIR_POS     *p_dir_start_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);
#endif

static  void         FS_FAT_SFN_DirEntryFind      (FS_VOL             *p_vol,       /* Search dir for SFN dir entry.    */
                                                   FS_BUF             *p_buf,
                                                   CPU_CHAR           *name,
                                                   CPU_CHAR          **p_name_next,
                                                   FS_FAT_DIR_POS     *p_dir_start_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);


static  void         FS_FAT_SFN_NextDirEntryGet   (FS_VOL             *p_vol,       /* Get next dir entry in dir.       */
                                                   FS_BUF             *p_buf,
                                                   void               *name,
                                                   FS_FAT_DIR_POS     *p_dir_start_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);


                                                                                    /* ---------- LOCAL FNCTS --------- */
static  void         FS_FAT_SFN_Parse             (void               *p_dir_entry, /* Parse SFN.                       */
                                                   CPU_CHAR           *name,
                                                   CPU_BOOLEAN         name_lower_case,
                                                   CPU_BOOLEAN         ext_lower_case);

static  FS_SEC_SIZE  FS_FAT_SFN_DirEntryFindInSec (void               *p_temp,      /* Search sec for SFN dir entry.    */
                                                   FS_SEC_SIZE         sec_size,
                                                   CPU_INT32U          name_8_3[],
                                                   FS_ERR             *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void         FS_FAT_SFN_DirEntryPlace     (FS_VOL             *p_vol,       /* Place dir entry in dir.          */
                                                   FS_BUF             *p_buf,
                                                   FS_FAT_SEC_NBR      dir_start_sec,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);

static  FS_SEC_SIZE  FS_FAT_SFN_DirEntryPlaceInSec(void               *p_temp,      /* Search sec for empty dir entry.  */
                                                   FS_SEC_SIZE         sec_size,
                                                   FS_ERR             *p_err);
#endif

static  void         FS_FAT_SFN_LabelFind         (FS_VOL             *p_vol,       /* Search dir for label (vol ID).   */
                                                   FS_BUF             *p_buf,
                                                   FS_FAT_DIR_POS     *p_dir_start_pos,
                                                   FS_FAT_DIR_POS     *p_dir_end_pos,
                                                   FS_ERR             *p_err);

static  FS_SEC_SIZE  FS_FAT_SFN_LabelFindInSec    (void               *p_temp,      /* Search sec for label (vol ID).   */
                                                   FS_SEC_SIZE         sec_size,
                                                   FS_ERR             *p_err);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_FAT_FN_API  FS_FAT_SFN_API = {
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FS_FAT_SFN_DirEntryCreate,
    FS_FAT_SFN_DirEntryCreateAt,
    FS_FAT_SFN_DirEntryDel,
#endif
    FS_FAT_SFN_DirEntryFind,
    FS_FAT_SFN_NextDirEntryGet,
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
*                                          FS_FAT_SFN_Chk()
*
* Description : Check whether file name is valid SFN.
*
* Argument(s) : name        Entry name/path.
*
*               p_name_len  Pointer to variable that will receive the length of the entry name, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE            File name valid.
*                               FS_ERR_NAME_INVALID    File name invalid.
*
* Return(s)   : none.
*
* Note(s)     : (1) If LFN support is enabled, then a return error will force an attempt to parse the
*                   name as a LFN, in which real name errors will be determined.
*********************************************************************************************************
*/

void  FS_FAT_SFN_Chk (CPU_CHAR          *name,
                      FS_FILE_NAME_LEN  *p_name_len,
                      FS_ERR            *p_err)
{
    CPU_BOOLEAN       has_period;
    CPU_BOOLEAN       is_legal;
    FS_FILE_NAME_LEN  len;
    CPU_CHAR          name_char;
    FS_FILE_NAME_LEN  name_len;


    is_legal   = DEF_YES;
    has_period = DEF_NO;
    len        = 0u;
    name_len   = 0u;
   *p_name_len = 0u;


                                                                /* ------------------- CHK INIT CHAR ------------------ */
    name_char = *name;
    switch (name_char) {                                        /* Rtn err if first char is '.', ' ', '\0', '\'.        */
        case ASCII_CHAR_FULL_STOP:
        case ASCII_CHAR_SPACE:
        case ASCII_CHAR_NULL:
        case FS_FAT_PATH_SEP_CHAR:
            #ifndef  FS_FAT_LFN_MODULE_PRESENT                  /* See Note #1.                                         */
             FS_TRACE_DBG(("FS_FAT_SFN_Chk(): Invalid first char.\r\n"));
            #endif
            *p_err = FS_ERR_NAME_INVALID;
             return;

        default:
             break;
    }


                                                                /* ------------------- PROCESS PATH ------------------- */
    while ((name_char != (CPU_CHAR)ASCII_CHAR_NULL     ) &&     /* Process str until NULL char ...                      */
           (name_char != (CPU_CHAR)FS_FAT_PATH_SEP_CHAR) &&     /* ... or path sep        char ...                      */
           (is_legal  ==  DEF_YES                      )) {     /* ... or illegal name is found.                        */
        if (name_char == (CPU_CHAR)ASCII_CHAR_FULL_STOP) {
            if (has_period == DEF_NO) {
                has_period  = DEF_YES;
                len         = 0u;
                name_len++;
            } else {                                            /* If 2nd period ... illegal name.                      */
               #ifndef  FS_FAT_LFN_MODULE_PRESENT               /* See Note #1.                                         */
                FS_TRACE_DBG(("FS_FAT_SFN_Chk(): Period (full stop) in extension.\r\n"));
               #endif
                is_legal = DEF_NO;
            }
        } else {
            if (((has_period == DEF_YES) && (len < FS_FAT_SFN_EXT_MAX_NBR_CHAR )) ||
                ((has_period == DEF_NO ) && (len < FS_FAT_SFN_NAME_MAX_NBR_CHAR))) {
                is_legal = FS_FAT_IS_LEGAL_SFN_CHAR(name_char);
                if (is_legal == DEF_YES) {
                    len++;
                    name_len++;
                } else {
                   #ifndef  FS_FAT_LFN_MODULE_PRESENT           /* See Note #1.                                         */
                    FS_TRACE_DBG(("FS_FAT_SFN_Chk(): Invalid character in file name: %c (0x%02X).\r\n", name_char, name_char));
                   #endif
                    is_legal = DEF_NO;
                }
            } else {                                            /* If file name or extension too long ... illegal name. */
               #ifndef  FS_FAT_LFN_MODULE_PRESENT               /* See Note #1.                                         */
                FS_TRACE_DBG(("FS_FAT_SFN_Chk(): File name too long.\r\n"));
               #endif
                is_legal = DEF_NO;
            }
        }
        name++;
        name_char = *name;
    }


                                                                /* ------------------- ASSIGN & RTN ------------------- */
    if (is_legal == DEF_NO) {                                   /* Rtn err if illegal name.                             */
       *p_err = FS_ERR_NAME_INVALID;
        return;
    }

   *p_name_len = name_len;
   *p_err      = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      FS_FAT_SFN_DirEntryFmt()
*
* Description : Make directory entry for SFN entry.
*
* Argument(s) : p_dir_entry         Pointer to directory entry.
*
*               name_8_3            Entry SFN.
*
*               is_dir              Indicates whether entry is for directory :
*
*                                       DEF_TRUE    Entry is for directory.
*                                       DEF_FALSE   Entry is for file.
*
*               file_first_clus     First cluster in file.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_SFN_DirEntryFmt (void             *p_dir_entry,
                              CPU_INT32U        name_8_3[],
                              CPU_BOOLEAN       is_dir,
                              FS_FAT_CLUS_NBR   file_first_clus,
                              CPU_BOOLEAN       name_lower_case,
                              CPU_BOOLEAN       ext_lower_case)
{
    CPU_INT16U     date_val;
    CLK_DATE_TIME  stime;
    CPU_INT08U     ntres;
    CPU_INT16U     time_val;
    CPU_INT08U    *p_dir_entry_08;
    CPU_BOOLEAN    ok;


    ok = Clk_GetDateTime(&stime);                               /* Get date/time.                                       */
    if (ok == DEF_YES) {
        time_val =  FS_FAT_TimeFmt(&stime);
        date_val =  FS_FAT_DateFmt(&stime);
    } else {
        time_val =  0u;
        date_val =  0u;
    }

    p_dir_entry_08 = (CPU_INT08U *)p_dir_entry;

                                                                /* Octets 0-10: SFN.                                    */
    MEM_VAL_SET_INT32U((void *)(p_dir_entry_08 + 0u), name_8_3[0]);
    MEM_VAL_SET_INT32U((void *)(p_dir_entry_08 + 4u), name_8_3[1]);
    MEM_VAL_SET_INT32U((void *)(p_dir_entry_08 + 8u), name_8_3[2]);
    p_dir_entry_08 += 11u;

                                                                /* Octet  11:    Attrib.                                */
    if (is_dir == DEF_YES) {
       *p_dir_entry_08 = FS_FAT_DIRENT_ATTR_DIRECTORY;
        p_dir_entry_08++;
    } else {
       *p_dir_entry_08 = FS_FAT_DIRENT_ATTR_NONE;
        p_dir_entry_08++;
    }

                                                               /* 12:    NT reserved info.                             */
    ntres = DEF_BIT_NONE;
    if (name_lower_case == DEF_YES) {
        ntres |= FS_FAT_DIRENT_NTRES_NAME_LOWER_CASE;
    }
    if (ext_lower_case == DEF_YES) {
        ntres |= FS_FAT_DIRENT_NTRES_EXT_LOWER_CASE;
    }
   *p_dir_entry_08 =  ntres;
    p_dir_entry_08++;

   *p_dir_entry_08 =  0u;                                       /* 13:    Creation time, 10ths of a sec.                */
    p_dir_entry_08++;
                                                                /* 14-15: Creation time.                                */
   *p_dir_entry_08 = (CPU_INT08U)(time_val        >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(time_val        >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 16-17: Creation date.                                */
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 18-19: Last access date.                             */
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 20-21: Hi word, entry's 1st clus nbr.                */
   *p_dir_entry_08 = (CPU_INT08U)(file_first_clus >> (DEF_INT_08_NBR_BITS * 2u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(file_first_clus >> (DEF_INT_08_NBR_BITS * 3u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 22-23: Last write  time.                             */
   *p_dir_entry_08 = (CPU_INT08U)(time_val        >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(time_val        >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 24-25: Last wrote  date.                             */
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(date_val        >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
                                                                /* 26-27: Lo word, entry's 1st clus nbr.                */
   *p_dir_entry_08 = (CPU_INT08U)(file_first_clus >> (DEF_INT_08_NBR_BITS * 0u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 = (CPU_INT08U)(file_first_clus >> (DEF_INT_08_NBR_BITS * 1u)) & DEF_INT_08_MASK;
    p_dir_entry_08++;
   *p_dir_entry_08 =  0u;                                       /* 28-31: File size in bytes.                           */
    p_dir_entry_08++;
   *p_dir_entry_08 =  0u;
    p_dir_entry_08++;
   *p_dir_entry_08 =  0u;
    p_dir_entry_08++;
   *p_dir_entry_08 =  0u;
}
#endif


/*
*********************************************************************************************************
*                                         FS_FAT_SFN_Create()
*
* Description : Create 8.3 SFN from path component for comparison with data from volume.
*
* Argument(s) : name                Name of the entry.
*
*               name_8_3            Array that will receive entry SFN.
*
*               p_name_lower_case   Pointer to variable that will receive indicator of whether name
*                                   characters will be returned in lower case :
*                                       DEF_YES, name characters will be returned in lower case.
*                                       DEF_NO,  name characters will be returned in upper case.
*
*               p_ext_lower_case    Pointer to variable that will receive indicator of whether extension
*                                   characters will be returned in lower case :
*                                       DEF_YES, extension characters will be returned in lower case.
*                                       DEF_NO,  extension characters will be returned in upper case.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE               File SFN is legal.
*                                       FS_ERR_NAME_INVALID       File SFN is illegal.
*                                       FS_ERR_NAME_MIXED_CASE    File SFN is mixed case.
*
* Return(s)   : none.
*
* Note(s)     : (1) (a) The 8.3 file name, 'p_name_8_3' is maintained as an array of three four-byte
*                       so that file name search comparison may be efficiently executed.
*
*                   (b) The final byte of the final name--the byte in the 3rd word of 'p_name_8_3' at
*                       the highest memory locations is cleared to 0 since the 12th byte of FAT
*                       directory entries is NOT part of the file name.
*********************************************************************************************************
*/

void  FS_FAT_SFN_Create (CPU_CHAR     *name,
                         CPU_INT32U    name_8_3[],
                         CPU_BOOLEAN  *p_name_lower_case,
                         CPU_BOOLEAN  *p_ext_lower_case,
                         FS_ERR       *p_err)
{
    CPU_BOOLEAN       ext_lower_case;
    CPU_BOOLEAN       has_period;
    CPU_BOOLEAN       has_upper;
    FS_FILE_NAME_LEN  i;
    CPU_BOOLEAN       is_legal;
    FS_FILE_NAME_LEN  len;
    CPU_BOOLEAN       lower;
    CPU_BOOLEAN       mixed;
    CPU_CHAR          name_char;
    CPU_CHAR          name_8_3_08[12];
    CPU_BOOLEAN       name_lower_case;
    CPU_BOOLEAN       upper;


    ext_lower_case  = DEF_NO;
    has_period      = DEF_NO;
    has_upper       = DEF_NO;
    is_legal        = DEF_YES;
    len             = 0u;
    mixed           = DEF_NO;
    name_lower_case = DEF_NO;

    for (i = 0u; i < 11u; i++) {                                /* 'Clear' 8.3 file name buffer.                        */
        name_8_3_08[i] = (CPU_CHAR)ASCII_CHAR_SPACE;
    }
    name_8_3_08[11] = (CPU_CHAR)ASCII_CHAR_NULL;                /* 'Clear' final byte of 8.3 file name (see Note #1b).  */



                                                                /* ------------------- CHK INIT CHAR ------------------ */
    name_char = *name;
    switch (name_char) {                                        /* Rtn err if first char is '.', ' ', '\0', '\'.        */
        case ASCII_CHAR_FULL_STOP:
        case ASCII_CHAR_SPACE:
        case ASCII_CHAR_NULL:
        case FS_FAT_PATH_SEP_CHAR:
            *p_err = FS_ERR_NAME_INVALID;
             return;

        default:
             break;
    }


                                                                /* ------------------- PROCESS PATH ------------------- */
    i = 0u;
    while ((name_char != (CPU_CHAR)ASCII_CHAR_NULL     ) &&     /* Process str until NULL char ...                      */
           (name_char != (CPU_CHAR)FS_FAT_PATH_SEP_CHAR) &&     /* ... or path sep        char ...                      */
           (is_legal  ==  DEF_YES                      )) {     /* ... or illegal name is found.                        */
        if (name_char == (CPU_CHAR)ASCII_CHAR_FULL_STOP) {
            if (has_period == DEF_NO) {                         /* Entering extension ...                               */
                i          += FS_FAT_SFN_NAME_MAX_NBR_CHAR - len;                  /* ... 'len' will ALWAYS be <= 8.    */
                has_period  = DEF_YES;
                has_upper   = DEF_NO;
                len         = 0u;
            } else {                                            /* If 2nd period ... illegal name.                      */
                is_legal = DEF_NO;
            }
        } else {
            if (((has_period == DEF_YES) && (len < FS_FAT_SFN_EXT_MAX_NBR_CHAR )) ||
                ((has_period == DEF_NO ) && (len < FS_FAT_SFN_NAME_MAX_NBR_CHAR))) {
                is_legal = FS_FAT_IS_LEGAL_SFN_CHAR(name_char);
                if (is_legal == DEF_YES) {
                    lower = ASCII_IS_LOWER(name_char);
                    if (lower == DEF_YES) {
                        if (has_upper == DEF_YES) {
                            mixed = DEF_YES;
                        } else {
                            if (has_period == DEF_YES) {
                                ext_lower_case = DEF_YES;
                            } else {
                                name_lower_case = DEF_YES;
                            }
                        }
                    } else {
                        upper = ASCII_IS_UPPER(name_char);
                        if (upper == DEF_YES) {
                            has_upper = DEF_YES;
                            if (((has_period == DEF_YES) && (ext_lower_case  == DEF_YES)) ||
                                ((has_period == DEF_NO)  && (name_lower_case == DEF_YES))) {
                                mixed = DEF_YES;
                            }
                        }
                    }
                    name_8_3_08[i] = ASCII_TO_UPPER(name_char);
                    i++;
                    len++;
                } else {
                    is_legal = DEF_NO;
                }
            } else {                                            /* If file name or extension too long ... illegal name. */
                is_legal = DEF_NO;
            }
        }
        name++;
        name_char = *name;
    }


                                                                /* ------------------- ASSIGN & RTN ------------------- */
    if (is_legal == DEF_NO) {                                   /* Rtn err if illegal name.                             */
       *p_err = FS_ERR_NAME_INVALID;
        return;
    }

    name_8_3[0] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[0]);
    name_8_3[1] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[4]);
    name_8_3[2] = MEM_VAL_GET_INT32U((void *)&name_8_3_08[8]);

    if (mixed == DEF_YES) {
        name_lower_case = DEF_NO;
        ext_lower_case  = DEF_NO;
    }

   *p_name_lower_case = name_lower_case;
   *p_ext_lower_case  = ext_lower_case;

   *p_err = (mixed == DEF_YES) ? FS_ERR_NAME_MIXED_CASE : FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_SFN_SFN_Find()
*
* Description : Search directory for SFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name_8_3            Entry SFN.
*
*               p_dir_start_pos     Pointer to directory position at which search should start; variable
*                                   will receive the directory position at which the entry is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                       Directory entry found.
*                                       FS_ERR_DEV                        Device access error.
*                                       FS_ERR_SYS_DIR_ENTRY_NOT_FOUND    Directory entry not found.
*
* Return(s)   : none.
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

void  FS_FAT_SFN_SFN_Find (FS_VOL          *p_vol,
                           FS_BUF          *p_buf,
                           CPU_INT32U       name_8_3[],
                           FS_FAT_DIR_POS  *p_dir_start_pos,
                           FS_FAT_DIR_POS  *p_dir_end_pos,
                           FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR   dir_cur_sec;
    CPU_BOOLEAN      dir_sec_valid;
    FS_SEC_SIZE      dir_end_sec_pos;
    FS_FAT_DATA     *p_fat_data;


    p_fat_data    = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_cur_sec   =  p_dir_start_pos->SecNbr;
    dir_sec_valid =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #2).                    */
        FSBuf_Set(p_buf,
                  dir_cur_sec,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        dir_end_sec_pos = FS_FAT_SFN_DirEntryFindInSec(p_buf->DataPtr,
                                                       p_fat_data->SecSize,
                                                       name_8_3,
                                                       p_err);

        switch (*p_err) {
            case FS_ERR_NONE:                                   /* Entry with matching name found.                      */
                 p_dir_start_pos->SecNbr = dir_cur_sec;
                 p_dir_start_pos->SecPos = dir_end_sec_pos;

                 p_dir_end_pos->SecNbr   = p_dir_start_pos->SecNbr;
                 p_dir_end_pos->SecPos   = p_dir_start_pos->SecPos;
                 return;


            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND:                /* No entry with matching name exists.                  */
                 return;


            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET:            /* Entry with matching name MAY exist in later sec.     */
                 dir_cur_sec = FS_FAT_SecNextGet(p_vol,
                                                 p_buf,
                                                 dir_cur_sec,
                                                 p_err);

                 switch (*p_err) {
                     case FS_ERR_NONE:                          /* More secs exist in dir.                              */
                                                                /* Chk sec validity (see Note #3).                      */
                          dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);
                          break;


                     case FS_ERR_SYS_CLUS_CHAIN_END:            /* No more secs exist in dir.                           */
                     case FS_ERR_SYS_CLUS_INVALID:
                     case FS_ERR_DIR_FULL:
                         *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;
                          return;


                     case FS_ERR_DEV:
                     default:
                          return;
                 }
                 break;


            case FS_ERR_DEV:
                 return;


            default:
                 FS_TRACE_DBG(("FS_FAT_SFN_SFN_Find(): Default case reached.\r\n"));
                 return;
        }
    }


                                                                /* Invalid sec found (see Note #3).                     */
    FS_TRACE_DBG(("FS_FAT_SFN_SFN_Find(): Invalid sec gotten: %d.\r\n", dir_cur_sec));
   *p_err = FS_ERR_ENTRY_CORRUPT;
}


/*
*********************************************************************************************************
*                                        FS_FAT_SFN_LabelGet()
*
* Description : Get volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               label       String buffer that will receive volume label.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   Label gotten.
*                               FS_ERR_VOL_LABEL_NOT_FOUND    Volume label was not found.
*                               FS_ERR_DEV                    Device access error.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FS_FAT_SFN_LabelGet (FS_VOL    *p_vol,
                           CPU_CHAR  *label,
                           FS_ERR    *p_err)
{
    FS_BUF          *p_buf;
    FS_FAT_DIR_POS   dir_end_pos;
    FS_FAT_DIR_POS   dir_start_pos;
    FS_FAT_SEC_NBR   dir_start_sec;
    CPU_CHAR        *p_dir_entry;


    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    dir_start_sec = ((FS_FAT_DATA *)p_vol->DataPtr)->RootDirStart;



                                                                /* -------------------- FIND LABEL -------------------- */
    dir_start_pos.SecNbr = dir_start_sec;
    dir_start_pos.SecPos = 0u;

    FS_FAT_SFN_LabelFind( p_vol,                                /* Find label ...                                       */
                          p_buf,
                         &dir_start_pos,
                         &dir_end_pos,
                          p_err);

    if (*p_err != FS_ERR_NONE) {
        if (*p_err == FS_ERR_SYS_DIR_ENTRY_NOT_FOUND) {         /*           ... if dir entry not found ...             */
            *p_err =  FS_ERR_VOL_LABEL_NOT_FOUND;               /*                                      ... rtn err.    */
        }
        FSBuf_Free(p_buf);
        return;
    }


                                                                /* ----------------- COPY LABEL TO BUF ---------------- */
    p_dir_entry = (CPU_CHAR *)p_buf->DataPtr + dir_end_pos.SecPos;
    Str_Copy_N(label, (CPU_CHAR *)p_dir_entry, FS_FAT_VOL_LABEL_LEN);
    FSBuf_Free(p_buf);
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_FAT_SFN_LabelSet()
*
* Description : Set volume label.
*
* Argument(s) : p_vol       Pointer to volume.
*
*               label       Volume label.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                 Label set.
*                               FS_ERR_DEV                  Device access error.
*                               FS_ERR_DIR_FULL             Directory is full (space could not be allocated).
*                               FS_ERR_DEV_FULL             Device is full (space could not be allocated).
*                               FS_ERR_ENTRY_CORRUPT        File system entry is corrupt.
*                               FS_ERR_VOL_LABEL_INVALID    Volume label is invalid.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FS_FAT_SFN_LabelSet (FS_VOL    *p_vol,
                           CPU_CHAR  *label,
                           FS_ERR    *p_err)
{
    FS_BUF            *p_buf;
    FS_FAT_DIR_POS     dir_end_pos;
    FS_FAT_DIR_POS     dir_start_pos;
    FS_FAT_SEC_NBR     dir_start_sec;
    CPU_INT08U        *p_dir_entry;
    FS_FILE_NAME_LEN   i;
    CPU_BOOLEAN        is_legal;
    CPU_CHAR           label_buf[FS_FAT_VOL_LABEL_LEN + 1u];
    CPU_CHAR           label_char;
    FS_FILE_NAME_LEN   len;


                                                                /* ------------------- PROCESS LABEL ------------------ */
    for (i = 0u; i < 11u; i++) {                                /* 'Clear' label buf.                                   */
        label_buf[i] = (CPU_CHAR)ASCII_CHAR_SPACE;
    }
    label_buf[11] = (CPU_CHAR)ASCII_CHAR_NULL;                  /* 'Clear' final byte of label buf.                     */

    label_char = *label;
    is_legal   = DEF_YES;
    len        = 0u;
    i          =  0u;
    while ((label_char != (CPU_CHAR)ASCII_CHAR_NULL     ) &&    /* Process str until NULL char ...                      */
           (is_legal   ==  DEF_YES                      )) {    /* ... or illegal label is found.                       */
        if (len < FS_FAT_VOL_LABEL_LEN) {
            is_legal = FS_FAT_IS_LEGAL_VOL_LABEL_CHAR(label_char);
            if (is_legal == DEF_YES) {
                label_buf[i] = ASCII_TO_UPPER(label_char);
                i++;
                len++;
            } else {
                is_legal = DEF_NO;
            }
        } else {                                                /* If label too long ... illegal label.                 */
            is_legal = DEF_NO;
        }
        label++;
        label_char = *label;
    }

    if (is_legal == DEF_NO) {                                   /* Rtn err if illegal label.                             */
       *p_err = FS_ERR_VOL_LABEL_INVALID;
        return;
    }


                                                                /* -------------------- FIND LABEL -------------------- */
    p_buf = FSBuf_Get(p_vol);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    dir_start_sec = ((FS_FAT_DATA *)p_vol->DataPtr)->RootDirStart;

    dir_start_pos.SecNbr = dir_start_sec;
    dir_start_pos.SecPos = 0u;

    FS_FAT_SFN_LabelFind( p_vol,                                /* Find label ...                                       */
                          p_buf,
                         &dir_start_pos,
                         &dir_end_pos,
                          p_err);

    if (*p_err == FS_ERR_DEV) {
        FSBuf_Free(p_buf);
        return;
    }

    if (*p_err != FS_ERR_NONE) {                                /* If not found ...                                     */
        FS_FAT_SFN_DirEntryPlace( p_vol,                        /*              ... find place for entry.               */
                                  p_buf,
                                  dir_start_sec,
                                 &dir_end_pos,
                                  p_err);
        if (*p_err != FS_ERR_NONE) {
            FSBuf_Free(p_buf);
            return;
        }
    }


                                                                /* ----------------- COPY LABEL TO BUF ---------------- */
    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_end_pos.SecPos;
    Mem_Clr((void *)p_dir_entry, FS_FAT_SIZE_DIR_ENTRY);
    Mem_Copy((void *)p_dir_entry, (void *)label_buf, FS_FAT_VOL_LABEL_LEN);
    p_dir_entry[FS_FAT_DIRENT_OFF_ATTR] = FS_FAT_DIRENT_ATTR_VOLUME_ID;
    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr dir entry.                                        */
    FSBuf_Flush(p_buf, p_err);
    FSBuf_Free(p_buf);
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
*                                     FS_FAT_SFN_DirEntryCreate()
*
* Description : Create SFN directory entry.
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
*                                   variable that will receive the directory position at which the entry
*                                   was created.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the entry was created.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE             Directory entry created.
*                                       FS_ERR_DEV              Device access error.
*                                       FS_ERR_DIR_FULL         Directory is full (space could not be allocated).
*                                       FS_ERR_DEV_FULL         Device is full (space could not be allocated).
*                                       FS_ERR_ENTRY_CORRUPT    File system entry is corrupt.
*                                       FS_ERR_NAME_INVALID     SFN name invalid.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_SFN_DirEntryCreate (FS_VOL           *p_vol,
                                         FS_BUF           *p_buf,
                                         CPU_CHAR         *name,
                                         CPU_BOOLEAN       is_dir,
                                         FS_FAT_CLUS_NBR   file_first_clus,
                                         FS_FAT_DIR_POS   *p_dir_start_pos,
                                         FS_FAT_DIR_POS   *p_dir_end_pos,
                                         FS_ERR           *p_err)
{
    FS_FAT_DIR_POS   dir_cur_pos;
    CPU_INT08U      *p_dir_entry;
    CPU_BOOLEAN      ext_lower_case;
    CPU_INT32U       name_8_3[3];
    CPU_BOOLEAN      name_lower_case;
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    FS_FAT_DATA     *p_fat_data;
#endif


    p_dir_end_pos->SecNbr = 0u;                                 /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos = 0u;


                                                                /* --------------------- MAKE SFN --------------------- */
    FS_FAT_SFN_Create( name,
                      &name_8_3[0],
                      &name_lower_case,
                      &ext_lower_case,
                       p_err);

    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_NAME_MIXED_CASE)) {
         *p_err  = FS_ERR_NAME_INVALID;
          return;
    }


                                                                /* ------------------ FIND DIR ENTRY ------------------ */
    FS_FAT_SFN_DirEntryPlace( p_vol,
                              p_buf,
                              p_dir_start_pos->SecNbr,
                             &dir_cur_pos,
                              p_err);

    switch (*p_err) {
        case FS_ERR_NONE:
             break;

        case FS_ERR_DIR_FULL:
        case FS_ERR_DEV_FULL:
        case FS_ERR_ENTRY_CORRUPT:
             FS_TRACE_DBG(("FS_FAT_SFN_DirEntryCreate(): No dir entries avail.\r\n"));
             return;

        case FS_ERR_DEV:
             return;

        default:
             FS_TRACE_DBG(("FS_FAT_SFN_DirEntryCreate(): Default case reached.\r\n"));
             return;
    }


                                                                /* ------------------- ENTER JOURNAL ------------------ */
#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
    if (DEF_BIT_IS_SET(p_fat_data->JournalState, FS_FAT_JOURNAL_STATE_REPLAY) == DEF_NO) {
        FS_FAT_JournalEnterEntryCreate(p_vol,
                                       p_buf,
                                      &dir_cur_pos,
                                      &dir_cur_pos,
                                      p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }
    }
#endif

    FSBuf_Set(p_buf,
              dir_cur_pos.SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }


                                                                /* ----------------- WR SFN DIR ENTRY ----------------- */
    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + dir_cur_pos.SecPos;

    FS_FAT_SFN_DirEntryFmt(p_dir_entry,                         /* Create dir entry.                                    */
                           name_8_3,
                           is_dir,
                           file_first_clus,
                           name_lower_case,
                           ext_lower_case);
    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr dir entry.                                        */
    if (*p_err != FS_ERR_NONE) {
        return;
    }


    p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;               /* Rtn start pos.                                       */
    p_dir_start_pos->SecPos = dir_cur_pos.SecPos;

    p_dir_end_pos->SecNbr   = dir_cur_pos.SecNbr;               /* Rtn end   pos.                                       */
    p_dir_end_pos->SecPos   = dir_cur_pos.SecPos;
}
#endif


/*
*********************************************************************************************************
*                                    FS_FAT_SFN_DirEntryCreateAt()
*
* Description : Create SFN directory entry at specified position.
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
*                                   entry was created.
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
static  void  FS_FAT_SFN_DirEntryCreateAt (FS_VOL            *p_vol,
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
    CPU_INT08U  *p_dir_entry;


   (void)name;                                                  /*lint --e{550} Suppress "Symbol not accessed".         */
   (void)name_len;
   (void)p_vol;

    p_dir_end_pos->SecNbr = 0u;
    p_dir_end_pos->SecPos = 0u;
    p_dir_entry           = (CPU_INT08U *)p_buf->DataPtr + p_dir_pos->SecPos;



                                                                /* ----------------- WR SFN DIR ENTRY ----------------- */
    FSBuf_Set(p_buf,                                            /* Rd sec.                                              */
              p_dir_pos->SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FS_FAT_SFN_DirEntryFmt(p_dir_entry,                         /* Create dir entry.                                    */
                           name_8_3,
                           is_dir,
                           file_first_clus,
                           DEF_NO,
                           DEF_NO);

    FSBuf_MarkDirty(p_buf, p_err);                              /* Wr dir entry.                                        */
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_end_pos->SecNbr = p_dir_pos->SecNbr;
    p_dir_end_pos->SecPos = p_dir_pos->SecPos;
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_SFN_DirEntryDel()
*
* Description : Delete SFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to directory position at which entry is located.
*
*               p_dir_end_pos       Pointer to directory position at which entry is located (see Note #1).
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE    Directory entry deleted.
*                                       FS_ERR_DEV     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Parameters for the start & end directory positions are provided for LFN deletion,
*                   since several directory entries may specify a single file.  'p_dir_start_pos' &
*                   'p_dir_end_pos' SHOULD specify the exact same position for a SFN.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FS_FAT_SFN_DirEntryDel (FS_VOL          *p_vol,
                                      FS_BUF          *p_buf,
                                      FS_FAT_DIR_POS  *p_dir_start_pos,
                                      FS_FAT_DIR_POS  *p_dir_end_pos,
                                      FS_ERR          *p_err)
{
    CPU_INT08U   *p_dir_entry;
#if (FS_FAT_CFG_JOURNAL_EN == DEF_ENABLED)
    FS_FAT_DATA  *p_fat_data;
#endif


    (void)p_dir_start_pos;                                      /*lint --e{550} Suppress "Symbol not accessed".         */
    (void)p_vol;


#ifdef  FS_FAT_JOURNAL_MODULE_PRESENT
    p_fat_data = (FS_FAT_DATA *)p_vol->DataPtr;
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
    FSBuf_Set(p_buf,                                            /* Rd sec.                                              */
              p_dir_end_pos->SecNbr,
              FS_VOL_SEC_TYPE_DIR,
              DEF_YES,
              p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    p_dir_entry = (CPU_INT08U *)p_buf->DataPtr + p_dir_end_pos->SecPos;


                                                                /* ------------------ FREE SFN ENTRY ------------------ */
#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    Mem_Clr((void     *)p_dir_entry,
            (CPU_SIZE_T)FS_FAT_SIZE_DIR_ENTRY);
#endif

    p_dir_entry[0] = FS_FAT_DIRENT_NAME_ERASED_AND_FREE;


                                                                /* ------------------ UPDATE DIR SEC ------------------ */
    FSBuf_MarkDirty(p_buf, p_err);
}
#endif


/*
*********************************************************************************************************
*                                      FS_FAT_SFN_DirEntryFind()
*
* Description : Search directory for SFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               name                Name of the entry.
*
*               p_name_next         Pointer to variable that will receive pointer to character following
*                                   entry name.
*
*               p_dir_start_pos     Pointer to directory position at which search should start; variable
*                                   will receive the directory position at which the entry is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                       Directory entry found.
*
*                                                                         --- RETURNED BY FS_FAT_SFN_SFN_Find ---
*                                       FS_ERR_DEV                        Device access error.
*                                       FS_ERR_SYS_DIR_ENTRY_NOT_FOUND    Directory entry not found.
*
*                                                                         ---- RETURNED BY FS_FAT_SFN_API.Chk ---
*                                       FS_ERR_NAME_INVALID               File name is illegal.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_FAT_SFN_DirEntryFind (FS_VOL           *p_vol,
                                       FS_BUF           *p_buf,
                                       CPU_CHAR         *name,
                                       CPU_CHAR        **p_name_next,
                                       FS_FAT_DIR_POS   *p_dir_start_pos,
                                       FS_FAT_DIR_POS   *p_dir_end_pos,
                                       FS_ERR           *p_err)
{
    CPU_BOOLEAN        ext_lower_case;
    CPU_INT32U         name_8_3[3];
    FS_FILE_NAME_LEN   name_len;
    CPU_BOOLEAN        name_lower_case;


    p_dir_end_pos->SecNbr = 0u;                                 /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos = 0u;
   *p_name_next           = name;

    FS_FAT_SFN_Chk( name,                                       /* Chk if valid SFN.                                    */
                   &name_len,
                    p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

    FS_FAT_SFN_Create( name,                                    /* Form SFN entry for comparison.                       */
                      &name_8_3[0],
                      &name_lower_case,
                      &ext_lower_case,
                       p_err);
    if ((*p_err != FS_ERR_NONE) &&
        (*p_err != FS_ERR_NAME_MIXED_CASE)) {
        return;
    }

   *p_name_next = name + name_len;

    FS_FAT_SFN_SFN_Find(p_vol,
                        p_buf,
                        name_8_3,
                        p_dir_start_pos,
                        p_dir_end_pos,
                        p_err);
}


/*
*********************************************************************************************************
*                                    FS_FAT_SFN_NextDirEntryGet()
*
* Description : Get next SFN dir entry from dir.
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
*                                   will receive the directory position at which the entry is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE    Directory entry gotten.
*                                       FS_ERR_DEV     Device access error.
*                                       FS_ERR_EOF     End of directory reached.
*
* Return(s)   : none.
*
* Note(s)     : (1) The sector number gotten from the FAT should be valid.  These checks are effectively
*                   redundant.
*
*               (2) Windows stores case information for the short file name in the NTRes byte of the
*                   directory entry :
*                   (a) If bit 3 is set, the name characters are lower case.
*                   (b) If bit 4 is set, the extension characters are lower case.
*
*                   Names of entries created by systems that do not appropriate set NTRes bits upon entry
*                   creation will not be returned in the intended case; however, since all file name
*                   comparisons are caseless, driver operation is unaffected.
*********************************************************************************************************
*/

static  void  FS_FAT_SFN_NextDirEntryGet (FS_VOL          *p_vol,
                                          FS_BUF          *p_buf,
                                          void            *name,
                                          FS_FAT_DIR_POS  *p_dir_start_pos,
                                          FS_FAT_DIR_POS  *p_dir_end_pos,
                                          FS_ERR          *p_err)
{
    CPU_INT08U       data_08;
    FS_FAT_DIR_POS   dir_cur_pos;
    FS_FAT_SEC_NBR   dir_next_sec;
    CPU_BOOLEAN      dir_sec_valid;
    CPU_INT08U       fat_attrib;
    FS_FAT_DATA     *p_fat_data;
    CPU_INT08U      *p_temp_08;
    CPU_BOOLEAN      ext_lower_case;
    CPU_BOOLEAN      name_lower_case;
    CPU_INT08U       ntres;


    p_dir_end_pos->SecNbr =  0u;                                /* Dflt dir end pos.                                    */
    p_dir_end_pos->SecPos =  0u;

    p_fat_data            = (FS_FAT_DATA *)p_vol->DataPtr;

    dir_cur_pos.SecNbr    =  p_dir_start_pos->SecNbr;
    dir_cur_pos.SecPos    =  p_dir_start_pos->SecPos;
    dir_sec_valid         =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_pos.SecNbr);

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #1).                    */
                                                                /* ------------------- RD FIRST DIR SEC ---------------- */
        FSBuf_Set(p_buf,
                  dir_cur_pos.SecNbr,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        p_temp_08  = (CPU_INT08U *)p_buf->DataPtr;
        p_temp_08 += dir_cur_pos.SecPos;

        while (dir_cur_pos.SecPos < p_fat_data->SecSize) {
            data_08 = *p_temp_08;

            if (data_08 == FS_FAT_DIRENT_NAME_ERASED_AND_FREE) {/* ------------------ DIR ENTRY FREE ------------------ */
                ;

            } else if (data_08 == FS_FAT_DIRENT_NAME_FREE) {    /* ------------ ALL SUBSEQUENT ENTRIES FREE ----------- */
                p_dir_end_pos->SecNbr = dir_cur_pos.SecNbr;
                p_dir_end_pos->SecPos = dir_cur_pos.SecPos;
               *p_err = FS_ERR_EOF;
                return;

            } else {                                            /* ---------------- DIR ENTRY NOT FREE ---------------- */
                fat_attrib = MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp_08 + FS_FAT_DIRENT_OFF_ATTR));
                if (DEF_BIT_IS_CLR(fat_attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                    if (name != (void *)0) {
                                                                /* Apply case (see Note #2).                            */
                        ntres           = MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp_08 + FS_FAT_DIRENT_OFF_NTRES));
                        name_lower_case = DEF_BIT_IS_SET(ntres, FS_FAT_DIRENT_NTRES_NAME_LOWER_CASE);
                        ext_lower_case  = DEF_BIT_IS_SET(ntres, FS_FAT_DIRENT_NTRES_EXT_LOWER_CASE);
                        FS_FAT_SFN_Parse((void        *)p_temp_08,
                                         (CPU_CHAR    *)name,
                                         (CPU_BOOLEAN  )name_lower_case,
                                         (CPU_BOOLEAN  )ext_lower_case);
                    }
                    p_dir_start_pos->SecNbr = dir_cur_pos.SecNbr;
                    p_dir_start_pos->SecPos = dir_cur_pos.SecPos;
                    p_dir_end_pos->SecNbr   = dir_cur_pos.SecNbr;
                    p_dir_end_pos->SecPos   = dir_cur_pos.SecPos;
                   *p_err = FS_ERR_NONE;
                    return;
                }
            }
            p_temp_08          += FS_FAT_SIZE_DIR_ENTRY;
            dir_cur_pos.SecPos += FS_FAT_SIZE_DIR_ENTRY;
        }



                                                                /* ------------------ RD NEXT DIR SEC ----------------- */
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
                 return;

            default:
                 FS_TRACE_DBG(("FS_FAT_SFN_NextDirEntryGet(): Default case reached: %d.\r\n", *p_err));
                 return;
        }

                                                                /* Chk sec validity (see Note #1).                      */
        dir_sec_valid      = FS_FAT_IS_VALID_SEC(p_fat_data, dir_next_sec);

        dir_cur_pos.SecNbr = dir_next_sec;
        dir_cur_pos.SecPos = 0u;
    }


                                                                /* Invalid sec found (see Note #1).                     */
    FS_TRACE_DBG(("FS_FAT_SFN_NextDirEntryGet(): Invalid sec gotten: %d.\r\n", dir_cur_pos.SecNbr));
   *p_err = FS_ERR_ENTRY_CORRUPT;
    return;
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
*                                         FS_FAT_SFN_Parse()
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

static  void  FS_FAT_SFN_Parse (void         *p_dir_entry,
                                CPU_CHAR     *name,
                                CPU_BOOLEAN   name_lower_case,
                                CPU_BOOLEAN   ext_lower_case)
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
                                                :                (CPU_CHAR)p_dir_entry_08[ix];
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
       *name = (CPU_CHAR)ASCII_CHAR_FULL_STOP;
        name++;

        for (ix = 0u; ix < len; ix++) {
           *name = (ext_lower_case == DEF_YES) ? ASCII_TO_LOWER((CPU_CHAR)p_dir_entry_08[ix + FS_FAT_SFN_NAME_MAX_NBR_CHAR])
                                               :                (CPU_CHAR)p_dir_entry_08[ix + FS_FAT_SFN_NAME_MAX_NBR_CHAR];
            name++;
        }
    }

   *name = (CPU_CHAR)ASCII_CHAR_NULL;                           /* End NULL char.                                       */
}


/*
*********************************************************************************************************
*                                   FS_FAT_SFN_DirEntryFindInSec()
*
* Description : Search directory sector to find SFN directory entry.
*
* Argument(s) : p_temp      Pointer to buffer of directory sector.
*
*               sec_size    Sector size, in octets.
*
*               name_8_3    Entry SFN.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                           Directory entry found.
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND        Directory entry NOT found & all
*                                                                         subsequent entries are free
*                                                                         (so search SHOULD end).
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET    Directory entry NOT found, but
*                                                                         non-free subsequent entries
*                                                                         may exist).
*
* Return(s)   : Location of directory entry within buffer.
*
* Note(s)     : (1) (a) The 'p_temp' pointer is 4-byte aligned since all sectors are 4-byte aligned.
*
*                   (b) All pointers to directory entries are 4-byte aligned since all directory entries
*                       lie at a offset multiple of 32 (the size of a directory entry) from the beginning
*                       of a sector.
*********************************************************************************************************
*/

static  FS_SEC_SIZE  FS_FAT_SFN_DirEntryFindInSec (void         *p_temp,
                                                   FS_SEC_SIZE   sec_size,
                                                   CPU_INT32U    name_8_3[],
                                                   FS_ERR       *p_err)
{
    CPU_INT08U    data_08;
    CPU_INT32U    data_32;
    CPU_INT32U    name0;
    CPU_INT32U    name1;
    CPU_INT32U    name2;
    FS_SEC_SIZE   sec_pos;
    CPU_INT32U   *p_buf_32;


    name0    = name_8_3[0];
    name1    = name_8_3[1];
    name2    = name_8_3[2];

    p_buf_32 = (CPU_INT32U *)p_temp;                             /* See Note #1.                                         */

    sec_pos  = 0u;
    while (sec_pos < sec_size) {
        data_32 = *p_buf_32;
       #if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE)
        data_08 = (CPU_INT08U)(data_32 >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
       #else
        data_08 = (CPU_INT08U)(data_32 >> (3u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
       #endif

        if (data_08 == FS_FAT_DIRENT_NAME_FREE) {               /* If dir entry free & all subsequent entries free ...  */
           *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;             /* ... rtn err.                                         */
            return (0u);
        }

        if (data_08 != FS_FAT_DIRENT_NAME_ERASED_AND_FREE) {    /* If dir entry NOT free.                               */
            if (data_32 == name0) {
                data_32 = *(p_buf_32 + 1);
                if (data_32 == name1) {
                    data_32  = *(p_buf_32 + 2);
                   #if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE)
                    data_32 &= 0x00FFFFFFuL;
                   #else
                    data_32 &= 0xFFFFFF00uL;
                   #endif
                    if (data_32 == name2) {                     /* Match found.                                         */
                       *p_err = FS_ERR_NONE;
                        return (sec_pos);
                    }
                }
            }
        }

        p_buf_32 += FS_FAT_SIZE_DIR_ENTRY / 4u;
        sec_pos  += FS_FAT_SIZE_DIR_ENTRY;
    }

   *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET;                 /* If dir entry NOT found ... rtn err.                  */
    return (0u);
}


/*
*********************************************************************************************************
*                                     FS_FAT_SFN_DirEntryPlace()
*
* Description : Find directory entry for placement of SFN.
*
* Argument(s) : p_vol           Pointer to volume.
*
*               p_buf           Pointer to temporary buffer.
*
*               dir_start_sec   Directory sector in which entry should be located.
*
*               p_dir_end_pos   Pointer to variable that will receive the directory position at which the
*                               entry can be located.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE             Directory entry found.
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
static  void  FS_FAT_SFN_DirEntryPlace (FS_VOL          *p_vol,
                                        FS_BUF          *p_buf,
                                        FS_FAT_SEC_NBR   dir_start_sec,
                                        FS_FAT_DIR_POS  *p_dir_end_pos,
                                        FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR         dir_cur_sec;
    FS_SEC_SIZE            dir_end_sec_pos;
    CPU_BOOLEAN            dir_sec_valid;
    FS_FAT_DATA           *p_fat_data;


    p_dir_end_pos->SecNbr =  0u;
    p_dir_end_pos->SecPos =  0u;

    p_fat_data            = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_cur_sec           =  dir_start_sec;
    dir_sec_valid         =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);


    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #2).                    */

                                                                /* --------------------- RD DIR SEC ------------------- */
        FSBuf_Set(p_buf,
                  dir_cur_sec,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }


                                                                /* ------------- FIND EMPTY DIR ENTRY SLOT ------------ */
        dir_end_sec_pos = FS_FAT_SFN_DirEntryPlaceInSec(p_buf->DataPtr,
                                                        p_fat_data->SecSize,
                                                        p_err);



        switch (*p_err) {
            case FS_ERR_NONE:                                   /* ---------------- PLACE ENTRY IN SLOT --------------- */
                 FS_TRACE_LOG(("FS_FAT_SFN_DirEntryPlace(): Free dir entry found at %d of sec %d\r\n", dir_end_sec_pos / FS_FAT_SIZE_DIR_ENTRY, dir_cur_sec));

                 p_dir_end_pos->SecNbr = dir_cur_sec;
                 p_dir_end_pos->SecPos = dir_end_sec_pos;
                 return;



            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET:            /* ----------------- GET NEXT DIR SEC ----------------- */
                 dir_cur_sec = FS_FAT_SecNextGetAlloc(p_vol,
                                                      p_buf,
                                                      dir_cur_sec,
                                                      DEF_TRUE,
                                                      p_err);
                 switch (*p_err) {
                     case FS_ERR_NONE:
                                                                /* Chk sec validity (see Note #2).                      */
                          dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);
                          break;

                     case FS_ERR_DIR_FULL:
                     case FS_ERR_DEV_FULL:
                          return;

                     case FS_ERR_SYS_CLUS_CHAIN_END:
                     case FS_ERR_SYS_CLUS_INVALID:
                         *p_err = FS_ERR_ENTRY_CORRUPT;
                          return;

                     case FS_ERR_DEV:
                          return;

                     default:
                          FS_TRACE_DBG(("FS_FAT_SFN_DirEntryPlace(): Default case reached: %d.\r\n", *p_err));
                          return;
                 }
                 break;



            default:
                FS_TRACE_DBG(("FS_FAT_SFN_DirEntryPlace(): Default case reached: %d.\r\n", *p_err));
                return;
        }
    }


                                                                /* Invalid sec found (see Note #2).                     */
    FS_TRACE_DBG(("FS_FAT_SFN_DirEntryPlace(): Invalid sec alloc'd: %d.\r\n", dir_cur_sec));
   *p_err = FS_ERR_ENTRY_CORRUPT;
}
#endif


/*
*********************************************************************************************************
*                                   FS_FAT_SFN_DirEntryPlaceInSec()
*
* Description : Search directory sector to find empty directory entry.
*
* Argument(s) : p_temp      Pointer to buffer of directory sector.
*
*               sec_size    Sector size, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                           Empty directory entry found.
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET    Empty directory entry NOT found yet.
*
* Return(s)   : Location of directory entry within buffer.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  FS_SEC_SIZE  FS_FAT_SFN_DirEntryPlaceInSec (void         *p_temp,
                                                    FS_SEC_SIZE   sec_size,
                                                    FS_ERR       *p_err)
{
    CPU_INT08U    data_08;
    FS_SEC_SIZE   dir_sec_pos;
    CPU_INT08U   *p_temp_08;


    p_temp_08   = (CPU_INT08U *)p_temp;
    dir_sec_pos = 0u;

    while (dir_sec_pos < sec_size) {
        data_08 = *p_temp_08;
        if ((data_08 == FS_FAT_DIRENT_NAME_ERASED_AND_FREE) ||  /* If dir entry free ...                                */
            (data_08 == FS_FAT_DIRENT_NAME_FREE))  {
           *p_err = FS_ERR_NONE;
            return (dir_sec_pos);                               /*                   ... rtn dir entry loc.             */
        }
        p_temp_08   += FS_FAT_SIZE_DIR_ENTRY;
        dir_sec_pos += FS_FAT_SIZE_DIR_ENTRY;
    }

   *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET;                 /* If dir entry free NOT found ... rtn err.             */
    return (0u);
}
#endif


/*
*********************************************************************************************************
*                                       FS_FAT_SFN_LabelFind()
*
* Description : Search directory for SFN directory entry.
*
* Argument(s) : p_vol               Pointer to volume.
*
*               p_buf               Pointer to temporary buffer.
*
*               p_dir_start_pos     Pointer to directory position at which search should start; variable
*                                   will receive the directory position at which the entry is located.
*
*               p_dir_end_pos       Pointer to variable that will receive the directory position at which
*                                   the entry is located.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       FS_ERR_NONE                       Label found.
*                                       FS_ERR_DEV                        Device access error.
*                                       FS_ERR_SYS_DIR_ENTRY_NOT_FOUND    Label not found.
*
* Return(s)   : none.
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

static  void  FS_FAT_SFN_LabelFind (FS_VOL          *p_vol,
                                    FS_BUF          *p_buf,
                                    FS_FAT_DIR_POS  *p_dir_start_pos,
                                    FS_FAT_DIR_POS  *p_dir_end_pos,
                                    FS_ERR          *p_err)
{
    FS_FAT_SEC_NBR   dir_cur_sec;
    CPU_BOOLEAN      dir_sec_valid;
    FS_SEC_SIZE      dir_end_sec_pos;
    FS_FAT_DATA     *p_fat_data;


    p_fat_data    = (FS_FAT_DATA *)p_vol->DataPtr;
    dir_cur_sec   =  p_dir_start_pos->SecNbr;
    dir_sec_valid =  FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);

    while (dir_sec_valid == DEF_YES) {                          /* While sec is valid (see Note #2).                    */
        FSBuf_Set(p_buf,
                  dir_cur_sec,
                  FS_VOL_SEC_TYPE_DIR,
                  DEF_YES,
                  p_err);
        if (*p_err != FS_ERR_NONE) {
            return;
        }

        dir_end_sec_pos = FS_FAT_SFN_LabelFindInSec(p_buf->DataPtr,
                                                    p_fat_data->SecSize,
                                                    p_err);

        switch (*p_err) {
            case FS_ERR_NONE:                                   /* Entry with matching name found.                      */
                 p_dir_start_pos->SecNbr = dir_cur_sec;
                 p_dir_start_pos->SecPos = dir_end_sec_pos;

                 p_dir_end_pos->SecNbr   = p_dir_start_pos->SecNbr;
                 p_dir_end_pos->SecPos   = p_dir_start_pos->SecPos;
                 return;


            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND:                /* No entry with matching name exists.                  */
                 return;


            case FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET:            /* Entry with matching name MAY exist in later sec.     */
                 dir_cur_sec = FS_FAT_SecNextGet(p_vol,
                                                 p_buf,
                                                 dir_cur_sec,
                                                 p_err);

                 switch (*p_err) {
                     case FS_ERR_NONE:                          /* More secs exist in dir.                              */
                                                                /* Chk sec validity (see Note #3).                      */
                          dir_sec_valid = FS_FAT_IS_VALID_SEC(p_fat_data, dir_cur_sec);
                          break;


                     case FS_ERR_SYS_CLUS_CHAIN_END:            /* No more secs exist in dir.                           */
                     case FS_ERR_SYS_CLUS_INVALID:
                         *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;
                          return;


                     case FS_ERR_DEV:
                     default:
                          return;
                 }
                 break;


            case FS_ERR_DEV:
                 return;


            default:
                 FS_TRACE_DBG(("FS_FAT_SFN_SFN_Find(): Default case reached.\r\n"));
                 return;
        }
    }


                                                                /* Invalid sec found (see Note #3).                     */
    FS_TRACE_DBG(("FS_FAT_SFN_LabelFind(): Invalid sec gotten: %d.\r\n", dir_cur_sec));
   *p_err = FS_ERR_ENTRY_CORRUPT;
}


/*
*********************************************************************************************************
*                                     FS_FAT_SFN_LabelFindInSec()
*
* Description : Search directory sector to find label (volume ID).
*
* Argument(s) : p_temp      Pointer to buffer of directory sector.
*
*               sec_size    Sector size, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                           Label found.
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND        Label NOT found & all subsequent
*                                                                         entries are free (so search
*                                                                         SHOULD end).
*                               FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET    Label NOT found, but non-free
*                                                                         subsequent entries may exist).
*
* Return(s)   : Location of label within buffer.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_SEC_SIZE  FS_FAT_SFN_LabelFindInSec (void         *p_temp,
                                                FS_SEC_SIZE   sec_size,
                                                FS_ERR       *p_err)
{
    CPU_INT08U    attrib;
    CPU_INT08U   *p_buf_08;
    CPU_INT08U    data_08;
    FS_SEC_SIZE   sec_pos;


    p_buf_08 = (CPU_INT08U *)p_temp;

    sec_pos  = 0u;
    while (sec_pos < sec_size) {
        data_08 = *p_buf_08;

        if (data_08 == FS_FAT_DIRENT_NAME_FREE) {               /* If dir entry free & all subsequent entries free ...  */
           *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND;             /* ... rtn err.                                         */
            return (0u);
        }

        if (data_08 != FS_FAT_DIRENT_NAME_ERASED_AND_FREE) {    /* If dir entry NOT free.                               */
            attrib  = *(p_buf_08 + FS_FAT_DIRENT_OFF_ATTR);

            if ((attrib & FS_FAT_DIRENT_ATTR_LONG_NAME_MASK) != FS_FAT_DIRENT_ATTR_LONG_NAME) {
                if (DEF_BIT_IS_SET(attrib, FS_FAT_DIRENT_ATTR_VOLUME_ID) == DEF_YES) {
                    if (DEF_BIT_IS_CLR(attrib, FS_FAT_DIRENT_ATTR_DIRECTORY) == DEF_YES) {
                       *p_err = FS_ERR_NONE;
                        return (sec_pos);
                    }
                }
            }
        }

        p_buf_08 += FS_FAT_SIZE_DIR_ENTRY;
        sec_pos  += FS_FAT_SIZE_DIR_ENTRY;
    }

   *p_err = FS_ERR_SYS_DIR_ENTRY_NOT_FOUND_YET;                 /* If dir entry NOT found ... rtn err.                  */
    return (0u);
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'MODULE  Note #1' & 'fs_fat.h  MODULE  Note #1'.
*********************************************************************************************************
*/

#endif                                                          /* End of FAT SFN module include (see Note #1).         */
