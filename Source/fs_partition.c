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
*                               FILE SYSTEM SUITE PARTITION MANAGEMENT
*
* Filename     : fs_partition.c
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) Carrier, Brian. "File System Forensic Analysis."  NJ: Addison-Wesley, 2005.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_PARTITION_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "fs.h"
#include  "fs_type.h"
#include  "fs_partition.h"
#include  "fs_buf.h"
#include  "fs_dev.h"


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
*                                         PARTITION DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_partition {
    FS_PARTITION_NBR   Partition;                               /* Partition nbr.                                       */
    FS_SEC_NBR         CurMBR_Sec;                              /* Dev sec of cur MBR.                                  */
    FS_PARTITION_NBR   CurMBR_Pos;                              /* Pos in cur MBR sec.                                  */
    FS_SEC_NBR         PrevMBR_Sec;                             /* Dev sec of prev MBR.                                 */
    FS_PARTITION_NBR   PrevMBR_Pos;                             /* Pos in prev MBR sec.                                 */
    FS_SEC_NBR         PrevPartitionEnd;                        /* End sec of prev partition.                           */
    FS_SEC_NBR         PrimExtPartitionStart;                   /* Start sec of prim ext partition.                     */
    FS_SEC_QTY         PrimExtPartitionSize;                    /* Size of prim ext partition.                          */
    CPU_BOOLEAN        InExt;                                   /* In ext partition.                                    */
    CPU_BOOLEAN        HasErr;                                  /* Err in partition.                                    */
    FS_DEV            *DevPtr;                                  /* Ptr to dev.                                          */
} FS_PARTITION;


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

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSPartition_CalcCHS    (CPU_INT08U          *p_buf,
                                                  FS_SEC_NBR           lba);
#endif

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSPartition_GetTypeName(CPU_INT08U           type);
#endif

static  void              FSPartition_Open       (FS_DEV              *p_dev,
                                                  FS_PARTITION        *p_partition,
                                                  FS_ERR              *p_err);

static  void              FSPartition_Rd         (FS_PARTITION        *p_partition,
                                                  FS_PARTITION_ENTRY  *p_partition_entry,
                                                  FS_ERR              *p_err);

static  void              FSPartition_RdEntry    (FS_DEV              *p_dev,
                                                  FS_PARTITION_ENTRY  *p_partition_entry,
                                                  FS_SEC_NBR           mbr_sec,
                                                  FS_PARTITION_NBR     mbr_pos,
                                                  FS_ERR              *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSPartition_Wr         (FS_PARTITION        *p_partition,
                                                  FS_PARTITION_ENTRY  *p_partition_entry,
                                                  FS_ERR              *p_err);

static  void              FSPartition_WrEntry    (FS_DEV              *p_dev,
                                                  FS_PARTITION_ENTRY  *p_partition_entry,
                                                  FS_SEC_NBR           mbr_sec,
                                                  FS_PARTITION_NBR     mbr_pos,
                                                  FS_ERR              *p_err);
#endif
#endif


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
*                                   FSPartition_GetNbrPartitions()
*
* Description : Get number of partitions on a device.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Number of partitions obtained.
*                               FS_ERR_DEV     Device access error.
*
*
* Return(s)   : Number of partitions on device.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
FS_PARTITION_NBR  FSPartition_GetNbrPartitions (FS_DEV  *p_dev,
                                                FS_ERR  *p_err)
{
    FS_PARTITION        partition;
    FS_PARTITION_ENTRY  partition_entry;
    FS_PARTITION_NBR    partition_cnt;
    CPU_INT08U          partition_type;
    FS_ERR              partition_err;


                                                                /* ------------------ OPEN PARTITION ------------------ */
    FSPartition_Open( p_dev,
                     &partition,
                     &partition_err);

    if (partition_err != FS_ERR_NONE) {
       *p_err = partition_err;
        return (0u);
    }



                                                                /* ------------ ITERATE THROUGH PARTITIONS ------------ */
    FSPartition_Rd(&partition,                                  /* Rd first partition.                                  */
                   &partition_entry,
                   &partition_err);

    partition_cnt = 0u;

    while (partition_err == FS_ERR_NONE) {                      /* While another partition exists ...                   */
        partition_type = partition_entry.Type;

        FS_TRACE_INFO(("FSPartition_GetNbrPartitions(): Partition found: %08X + %08X, TYPE = %02X (%s)\r\n", partition_entry.Start, partition_entry.Size, partition_type, FSPartition_GetTypeName(partition_type)));

        if ((partition_type != FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) &&
            (partition_type != FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) &&
            (partition_type != FS_PARTITION_TYPE_LINUX_EXT)) {
            partition_cnt++;
        }

        FSPartition_Rd(&partition,                              /* ... rd next partition.                               */
                       &partition_entry,
                       &partition_err);
    }



                                                                /* ------------------------ RTN ----------------------- */
    if ((partition_err == FS_ERR_PARTITION_INVALID) ||
        (partition_err == FS_ERR_PARTITION_INVALID_SIG) ||
        (partition_err == FS_ERR_PARTITION_ZERO)) {
         partition_err =  FS_ERR_NONE;
    }

   *p_err = partition_err;

    return (partition_cnt);
}
#endif


/*
*********************************************************************************************************
*                                          FSPartition_Add()
*
* Description : Add partition to the device.
*
* Argument(s) : p_dev           Pointer to device.
*               ----------      Argument validated by caller.
*
*               partition_size  Size, in sectors, of the partition to add.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                      Partition added.
*                                   FS_ERR_BUF_NONE_AVAIL            No buffer available.
*                                   FS_ERR_PARTITION_INVALID_SIZE    Partition size invalid.
*                                   FS_ERR_PARTITION_MAX             Maximum number of partitions created.
*                                   FS_ERR_PARTITION_NOT_FINAL       Current partition not final partition.
*                                   FS_ERR_PARTITION_NOT_FOUND       Partition entry not read.
*                                   FS_ERR_DEV                       Device access error.
*
* Return(s)   : The index of the created partition.  The first partition on the device has an index of 0.
*               If an error occurs, the function returns FS_INVALID_PARTITION_NBR.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) The final partition on the device is split into two parts, the second of which will
*                   contain 'partition_size' sectors.  If the specified partition size is larger than the
*                   final partition, then an error will be returned.
*
*               (3) If there is no valid partition on the device, then the MBR is initialized & an
*                   initial partition created.
*
*               (4) If the final partition on the volume is an active partition (e.g., it contains valid
*                   data or a file system), then this MAY cause future file system accesses to fail or
*                   data to be lost or overwritten.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN   == DEF_DISABLED)
FS_PARTITION_NBR   FSPartition_Add (FS_DEV      *p_dev,
                                    FS_SEC_QTY   partition_size,
                                    FS_ERR      *p_err)
{
    FS_PARTITION        partition;
    FS_PARTITION_ENTRY  partition_entry_last;
    FS_PARTITION_ENTRY  partition_entry;
    FS_PARTITION_NBR    partition_nbr;
    FS_PARTITION_NBR    partition_cnt;
    FS_BUF             *p_buf;
    CPU_INT08U         *p_temp_08;



#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (partition_size < 2u) {
       *p_err = FS_ERR_PARTITION_INVALID_SIZE;
        return (FS_INVALID_PARTITION_NBR);
    }
#endif

                                                                /* ------------------ OPEN PARTITION ------------------ */
    FSPartition_Open( p_dev,
                     &partition,
                      p_err);

    if (*p_err != FS_ERR_NONE) {
        return (FS_INVALID_PARTITION_NBR);
    }



                                                                /* ------------ ITERATE THROUGH PARTITIONS ------------ */
    FSPartition_Rd(&partition,                                  /* Rd first partition.                                  */
                   &partition_entry,
                    p_err);

    switch (*p_err) {
        case FS_ERR_PARTITION_INVALID:                          /* If no partition ...                                  */
        case FS_ERR_PARTITION_INVALID_SIG:
        case FS_ERR_PARTITION_ZERO:
             FSPartition_Init(p_dev,                            /* ... init partition struct.                           */
                              partition_size,
                              p_err);

             partition_nbr = (*p_err == FS_ERR_NONE) ? 0u : FS_INVALID_PARTITION_NBR;
             return (partition_nbr);


        case FS_ERR_NONE:
             break;


        case FS_ERR_BUF_NONE_AVAIL:
        case FS_ERR_DEV:
        default:
             return (FS_INVALID_PARTITION_NBR);

    }

    partition_cnt = 0u;

    while (*p_err == FS_ERR_NONE) {                             /* While another partition exists ...                   */
        partition_entry_last.Start = partition_entry.Start;
        partition_entry_last.Size  = partition_entry.Size;
        partition_entry_last.Type  = partition_entry.Type;

        if ((partition_entry.Type != FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) &&
            (partition_entry.Type != FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) &&
            (partition_entry.Type != FS_PARTITION_TYPE_LINUX_EXT)) {
            partition_cnt++;
        }

        FSPartition_Rd(&partition,                              /* ... rd next partition.                               */
                       &partition_entry,
                        p_err);
    }



                                                                /* ----------------- CREATE PARTITION ----------------- */
    switch (*p_err) {
        case FS_ERR_BUF_NONE_AVAIL:
        case FS_ERR_DEV:
             return (FS_INVALID_PARTITION_NBR);

        default:
             break;
    }

    partition_entry.Start = partition_entry_last.Start + partition_entry_last.Size;
    partition_entry.Size  = partition_size;
    partition_entry.Type  = 0u;

                                                                 /* ---------------------- GET BUF --------------------- */
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return (FS_INVALID_PARTITION_NBR);
    }



                                                                /* ------------------- FMT & WR MBR ------------------- */
                                                                /* Clear the Partition start sector to allow the format */
                                                                /* to write the correct FAT info.                       */
    p_temp_08 = (CPU_INT08U *)p_buf->DataPtr;
    Mem_Clr((void *)p_temp_08, 512u);

    FSDev_WrLocked(        p_dev,                               /* Clr 1st sec of partition.                            */
                   (void *)p_temp_08,
                           partition_entry.Start,
                           1u,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return (FS_INVALID_PARTITION_NBR);
    }


    FSPartition_Wr(&partition,
                   &partition_entry,
                    p_err);

    partition_nbr = (*p_err == FS_ERR_NONE) ? partition_cnt : FS_INVALID_PARTITION_NBR;

    FSBuf_Free(p_buf);
    return (partition_nbr);
}
#endif
#endif


/*
*********************************************************************************************************
*                                         FSPartition_Find()
*
* Description : Find partition on a device.
*
* Argument(s) : p_dev               Pointer to device.
*               ----------          Argument validated by caller.
*
*               partition_nbr       Index of the partition to find.
*
*               p_partition_entry   Pointer to variable that will receive the partition information.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                    Partition found.
*                                       FS_ERR_PARTITION_INVALID_NBR   Invalid partition number.
*                                       FS_ERR_DEV                     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) An extended partition entry, essentially a link to a normal partition entry, does NOT
*                   contribute to the partition count.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
void  FSPartition_Find (FS_DEV              *p_dev,
                        FS_PARTITION_NBR     partition_nbr,
                        FS_PARTITION_ENTRY  *p_partition_entry,
                        FS_ERR              *p_err)
{
    FS_PARTITION        partition;
    FS_PARTITION_NBR    partition_cnt;
    FS_PARTITION_ENTRY  partition_entry;
    FS_SEC_QTY          partition_size;
    FS_SEC_NBR          partition_start;
    CPU_INT08U          partition_type;
    FS_ERR              partition_err;


                                                                /* ------------------- DFLT RTN VALS ------------------ */
    p_partition_entry->Start = 0u;
    p_partition_entry->Size  = 0u;
    p_partition_entry->Type  = 0u;



                                                                /* ------------------ OPEN PARTITION ------------------ */
    FSPartition_Open( p_dev,
                     &partition,
                     &partition_err);

    if (partition_err != FS_ERR_NONE) {
       *p_err = partition_err;
        return;
    }



                                                                /* ------------ ITERATE THROUGH PARTITIONS ------------ */
    FSPartition_Rd(&partition,                                  /* Rd first partition entry.                            */
                   &partition_entry,
                   &partition_err);


    partition_cnt = 0u;

    while (partition_err == FS_ERR_NONE) {
        partition_start = partition_entry.Start;
        partition_size  = partition_entry.Size;
        partition_type  = partition_entry.Type;
                                                                /* If partition is not extended partition ...           */
        if ((partition_type != FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) &&
            (partition_type != FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) &&
            (partition_type != FS_PARTITION_TYPE_LINUX_EXT)) {

            if (partition_cnt == partition_nbr) {
                p_partition_entry->Start = partition_start;
                p_partition_entry->Size  = partition_size;
                p_partition_entry->Type  = partition_type;
               *p_err = FS_ERR_NONE;
                return;
            }
            partition_cnt++;                                    /* ... inc partition cnt (see Note #2).                 */
        }

        FSPartition_Rd(&partition,                              /* If not srch partition, rd next partition entry.      */
                       &partition_entry,
                       &partition_err);
    }

   *p_err = FS_ERR_PARTITION_INVALID_NBR;
}
#endif


/*
*********************************************************************************************************
*                                      FSPartition_FindSimple()
*
* Description : Find first partition on a device.
*
* Argument(s) : p_dev               Pointer to device.
*               ----------          Argument validated by caller.
*
*               p_partition_entry   Pointer to variable that will receive the partition information.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                     Partition found.
*                                       FS_ERR_PARTITION_INVALID        Invalid partition parameters.
*                                       FS_ERR_PARTITION_INVALID_SIG    Invalid MBR signature.
*                                       FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                                       FS_ERR_DEV                      Device access error.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_DISABLED)
void  FSPartition_FindSimple (FS_DEV              *p_dev,
                              FS_PARTITION_ENTRY  *p_partition_entry,
                              FS_ERR              *p_err)
{
    FS_SEC_NBR   partition_end;
    FS_SEC_QTY   partition_size;
    FS_SEC_NBR   partition_start;
    CPU_INT08U   partition_type;
    CPU_INT16U   sig_val;
    FS_BUF      *p_buf;
    CPU_INT08U  *p_temp;


                                                                /* ------------------- DFLT RTN VALS ------------------ */
    p_partition_entry->Start = 0u;
    p_partition_entry->Size  = 0u;
    p_partition_entry->Type  = 0u;



                                                                /* --------------------- READ MBR --------------------- */
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }


    p_temp = (CPU_INT08U *)p_buf->DataPtr;

    FSDev_RdLocked(        p_dev,                               /* Rd MBR sec.                                          */
                   (void *)p_temp,
                           0u,
                           1u,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return;
    }

    sig_val = MEM_VAL_GET_INT16U_LITTLE(p_temp + 510u);         /* Check sec sig.                                       */
    if (sig_val != 0xAA55u) {
        FSBuf_Free(p_buf);
       *p_err = FS_ERR_PARTITION_INVALID_SIG;
        return;
    }



                                                                /* ------------------ READ MBR ENTRY ------------------ */
                                                                /* Rd start sec.                                        */
    partition_start = MEM_VAL_GET_INT32U_LITTLE(p_temp + FS_PARTITION_DOS_OFF_START_LBA_1      + (0u * FS_PARTITION_DOS_TBL_ENTRY_SIZE));
                                                                /* Rd partition size (see Note #2).                     */
    partition_size  = MEM_VAL_GET_INT32U_LITTLE(p_temp + FS_PARTITION_DOS_OFF_SIZE_1           + (0u * FS_PARTITION_DOS_TBL_ENTRY_SIZE));
                                                                /* Rd partition type.                                   */
    partition_type  = MEM_VAL_GET_INT08U_LITTLE(p_temp + FS_PARTITION_DOS_OFF_PARTITION_TYPE_1 + (0u * FS_PARTITION_DOS_TBL_ENTRY_SIZE));

    partition_end   = p_dev->Size - 1u;

    if ((partition_start                  >  partition_end)         ||
        (partition_start + partition_size >  partition_end   + 1u)  ||
        (partition_start + partition_size <= partition_start + 1u)) {
        FSBuf_Free(p_buf);
       *p_err = FS_ERR_PARTITION_INVALID;
        return;
    }

    FSBuf_Free(p_buf);

                                                                /* ----------------- ASSIGN ENTRY INFO ---------------- */
    p_partition_entry->Start = partition_start;
    p_partition_entry->Size  = partition_size;
    p_partition_entry->Type  = partition_type;
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                         FSPartition_Init()
*
* Description : Initialize the partition structure on the device.
*
* Argument(s) : p_dev           Pointer to device.
*               ----------      Argument validated by caller.
*
*               partition_size  Size of partition (in sectors).
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_BUF_NONE_AVAIL            No buffers available.
*                                   FS_ERR_DEV                       Device access error.
*                                   FS_ERR_PARTITION_INVALID_SIZE    Partition size invalid.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) The partition begins in the sector immediately after the MBR.
*
*               (3) The file system type is not set in the MBR record.  This can only be done once the
*                   file system type had been determined.
*
*                   (a) #### Provide mechanism for setting partition type.
*
*               (4) The CHS addresses are set as a backup addressing system, in case the disk is used on
*                   a system unaware of LBA addressing.
*
*                   (a) CHS addresses are always calculated assuming a geometry of 255 heads & 63 sectors
*                       per track.
*
*                   (b) LBA & CHS address are related by :
*
*                           LBA = (((CYLINDER * heads_per_cylinder + HEAD) * sectors_per_track) + SECTOR - 1
*
*                       so
*
*                           SECTOR   =   (LBA + 1) % sectors_per_track
*                           HEAD     =  ((LBA + 1 - SECTOR) / sectors_per_track) % heads_per_cylinder
*                           CYLINDER = (((LBA + 1 - SECTOR) / sectors_per_track) - HEAD) / heads_per_cylinder
*
*               (5) The first sector of the new partition is cleared, so that any data from a previous
*                   file system will not be used to incorrectly initialize the volume.
*
*               (6) Avoid 'Excessive shift value' or 'Constant expression evaluates to 0' warning.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSPartition_Init (FS_DEV      *p_dev,
                        FS_SEC_QTY   partition_size,
                        FS_ERR      *p_err)
{
    FS_BUF      *p_buf;
    CPU_INT08U  *p_temp_08;
    CPU_INT32U   start_lba;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (partition_size < 2u) {
       *p_err = FS_ERR_PARTITION_INVALID_SIZE;
        return;
    }
#endif

                                                                /* ---------------------- GET BUF --------------------- */
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }



                                                                /* ------------------- FMT & WR MBR ------------------- */
    p_temp_08 = (CPU_INT08U *)p_buf->DataPtr;
    Mem_Clr((void *)p_temp_08, 512u);

    FSDev_WrLocked(        p_dev,                               /* Clr 1st sec of partition.                            */
                   (void *)p_temp_08,
                           1u,
                           1u,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return;
    }

    MEM_VAL_SET_INT08U_LITTLE((void *)(p_temp_08 + FS_PARTITION_DOS_OFF_BOOT_FLAG_1),      FS_PARTITION_DOS_BOOT_FLAG);

    FSPartition_CalcCHS(               p_temp_08 + FS_PARTITION_DOS_OFF_START_CHS_ADDR_1, (FS_SEC_NBR)1);

    MEM_VAL_SET_INT08U_LITTLE((void *)(p_temp_08 + FS_PARTITION_DOS_OFF_PARTITION_TYPE_1), FS_PARTITION_TYPE_FAT32_LBA);

    FSPartition_CalcCHS(               p_temp_08 + FS_PARTITION_DOS_OFF_END_CHS_ADDR_1,    partition_size - 1u);

    start_lba = 1u;                                             /* See Note #6.                                         */
    MEM_VAL_SET_INT32U_LITTLE((void *)(p_temp_08 + FS_PARTITION_DOS_OFF_START_LBA_1),      start_lba);
    MEM_VAL_SET_INT32U_LITTLE((void *)(p_temp_08 + FS_PARTITION_DOS_OFF_SIZE_1),           partition_size - 1u);
    MEM_VAL_SET_INT16U_LITTLE((void *)(p_temp_08 + FS_PARTITION_DOS_OFF_SIG),              0xAA55u);

    FSDev_WrLocked(        p_dev,                               /* Wr MBR.                                              */
                   (void *)p_temp_08,
                           0u,
                           1u,
                           p_err);

    FSBuf_Free(p_buf);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
    }
}
#endif


/*
*********************************************************************************************************
*                                        FSPartition_Update()
*
* Description : Update partition on a device.
*
* Argument(s) : p_dev           Pointer to device.
*               ----------      Argument validated by caller.
*
*               partition_nbr   Index of the partition to update.
*
*               partition_type  New type of the partition.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE                    Partition found.
*                                   FS_ERR_PARTITION_INVALID_NBR   Invalid partition number.
*                                   FS_ERR_DEV                     Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN   == DEF_DISABLED)
void  FSPartition_Update (FS_DEV            *p_dev,
                          FS_PARTITION_NBR   partition_nbr,
                          CPU_INT08U         partition_type,
                          FS_ERR            *p_err)
{
    FS_PARTITION        partition;
    FS_PARTITION_NBR    partition_cnt;
    FS_PARTITION_ENTRY  partition_entry;
    FS_ERR              partition_err;


                                                                /* ------------------ OPEN PARTITION ------------------ */
    FSPartition_Open( p_dev,
                     &partition,
                     &partition_err);

    if (partition_err != FS_ERR_NONE) {
       *p_err = partition_err;
        return;
    }


                                                                /* ------------ ITERATE THROUGH PARTITIONS ------------ */
    FSPartition_Rd(&partition,
                   &partition_entry,
                   &partition_err);

    partition_cnt = 0u;

    while (partition_err == FS_ERR_NONE) {
        if ((partition_entry.Type != FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) &&
            (partition_entry.Type != FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) &&
            (partition_entry.Type != FS_PARTITION_TYPE_LINUX_EXT)) {

            if (partition_cnt == partition_nbr) {
                partition_entry.Type = partition_type;

                FSPartition_WrEntry( p_dev,                     /* Wr updated entry.                                    */
                                    &partition_entry,
                                     partition.PrevMBR_Sec,
                                     partition.PrevMBR_Pos,
                                     p_err);
                return;
            }
            partition_cnt++;
        }

        FSPartition_Rd(&partition,
                       &partition_entry,
                       &partition_err);
    }

   *p_err = FS_ERR_PARTITION_INVALID_NBR;
}
#endif
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
*                                        FSPartition_CalcCHS()
*
* Description : Calculate CHS address.
*
* Argument(s) : p_buf       Pointer to buffer in which CHS address will be stored.
*
*               lba         LBA address.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSPartition_CalcCHS (CPU_INT08U  *p_buf,
                                   FS_SEC_NBR   lba)
{
    CPU_INT16U  cylinder;
    CPU_INT08U  head;
    CPU_INT08U  sector;


    sector   = (CPU_INT08U)(  (lba + 1u)          % FS_PARTITION_DOS_CHS_SECTORS_PER_TRK);
    head     = (CPU_INT08U)( ((lba + 1u - sector) / FS_PARTITION_DOS_CHS_SECTORS_PER_TRK)         % FS_PARTITION_DOS_CSH_HEADS_PER_CYLINDER);
    cylinder = (CPU_INT16U)((((lba + 1u - sector) / FS_PARTITION_DOS_CHS_SECTORS_PER_TRK) - head) / FS_PARTITION_DOS_CSH_HEADS_PER_CYLINDER);
   *p_buf    = head;
    p_buf++;
   *p_buf    = sector | (CPU_INT08U)((cylinder & 0x300u) >> 8);
    p_buf++;
   *p_buf    = (CPU_INT08U)cylinder & DEF_INT_08_MASK;
}
#endif


/*
*********************************************************************************************************
*                                      FSPartition_GetTypeName()
*
* Description : Get name of partition type.
*
* Argument(s) : type        Partition type.
*
* Return(s)   : Pointer to partition name string.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSPartition_GetTypeName (CPU_INT08U  type)
{
    const  CPU_CHAR  *p_type_name;

    switch (type) {
        case FS_PARTITION_TYPE_FAT12_CHS:
        case FS_PARTITION_TYPE_HID_FAT12_CHS:
             p_type_name = FS_PARTITION_DOS_NAME_FAT12;
             break;


        case FS_PARTITION_TYPE_FAT16_16_32MB:
        case FS_PARTITION_TYPE_FAT16_CHS_32MB_2GB:
        case FS_PARTITION_TYPE_FAT16_LBA_32MB_2GB:
        case FS_PARTITION_TYPE_HID_FAT16_16_32MB_CHS:
        case FS_PARTITION_TYPE_HID_FAT16_CHS_32MB_2GB:
        case FS_PARTITION_TYPE_HID_FAT16_LBA_32MB_2GB:
             p_type_name = FS_PARTITION_DOS_NAME_FAT16;
             break;


        case FS_PARTITION_TYPE_FAT32_CHS:
        case FS_PARTITION_TYPE_FAT32_LBA:
        case FS_PARTITION_TYPE_HID_CHS_FAT32:
        case FS_PARTITION_TYPE_HID_LBA_FAT32:
             p_type_name = FS_PARTITION_DOS_NAME_FAT32;
             break;


        case FS_PARTITION_TYPE_CHS_MICROSOFT_EXT:
        case FS_PARTITION_TYPE_LBA_MICROSOFT_EXT:
        case FS_PARTITION_TYPE_LINUX_EXT:
             p_type_name = FS_PARTITION_DOS_NAME_EXT;
             break;


        default:
             p_type_name = FS_PARTITION_DOS_NAME_OTHER;
             break;
    }

    return (p_type_name);
}
#endif
#endif


/*
*********************************************************************************************************
*                                         FSPartition_Open()
*
* Description : Open partition for reading.
*
* Argument(s) : p_dev           Pointer to device.
*               ----------      Argument validated by caller.
*
*               p_partition     Pointer to partition.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE    Partition opened.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/


#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
static  void  FSPartition_Open (FS_DEV        *p_dev,
                                FS_PARTITION  *p_partition,
                                FS_ERR        *p_err)
{
    p_partition->Partition             = 0u;
    p_partition->CurMBR_Sec            = 0u;
    p_partition->CurMBR_Pos            = 0u;
    p_partition->PrevMBR_Sec           = 0u;
    p_partition->PrevMBR_Pos           = 0u;
    p_partition->PrevPartitionEnd      = 0u;
    p_partition->PrimExtPartitionStart = 0u;
    p_partition->PrimExtPartitionSize  = 0u;
    p_partition->InExt                 = DEF_NO;
    p_partition->HasErr                = DEF_NO;
    p_partition->DevPtr                = p_dev;

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSPartition_Rd()
*
* Description : Read a partition entry.
*
* Argument(s) : p_partition         Pointer to partition.
*               ----------          Argument validated by caller.
*
*               p_partition_entry   Pointer to partition entry.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                     Partition entry read.
*                                       FS_ERR_PARTITION_ZERO           Partition entry is 'zero'.
*                                       FS_ERR_PARTITION_INVALID        Partition entry is invalid.
*                                       FS_ERR_PARTITION_INVALID_SIG    Invalid MBR signature.
*                                       FS_ERR_DEV                      Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) The start sector of a partition entry may be relative to one of three device
*                   locations :
*
*                   (a) The start sector for a primary partition or for the primary extended partition is
*                       relative to the start of the device (sector 0).  The entries for this partition
*                       are located in the "primary" MBR.
*
*                   (b) The start sector for a secondary file system partition is relative to the start
*                       of the partition.
*
*                   (c) The start sector for a secondary extended partition is relative to the start of
*                       the primary extended partition.
*
*               (3) (a) The "primary" MBR may hold as many as 4 partition entries.
*
*                   (b) "Secondary" MBRs may hold as many as 2 partition entries.  If two entries are
*                       present, the first will specify a file system & the second will point to the next
*                       extended partition.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
static  void  FSPartition_Rd (FS_PARTITION        *p_partition,
                              FS_PARTITION_ENTRY  *p_partition_entry,
                              FS_ERR              *p_err)
{
    FS_SEC_NBR           partition_end_prev;
    FS_SEC_NBR           partition_end;
    FS_PARTITION_ENTRY   partition_entry;
    FS_SEC_QTY           partition_size;
    FS_SEC_NBR           partition_start;
    CPU_INT08U           partition_type;
    FS_DEV              *p_dev;


                                                                /* ---------------- CLR PARTITION ENTRY --------------- */
    p_partition_entry->Start = 0u;
    p_partition_entry->Size  = 0u;
    p_partition_entry->Type  = 0u;

                                                                /* See Note #3.                                         */
    if (((p_partition->CurMBR_Pos == 4u) && (p_partition->InExt == DEF_NO)) ||
        ((p_partition->CurMBR_Pos == 2u) && (p_partition->InExt == DEF_YES))) {
       *p_err = FS_ERR_PARTITION_INVALID;
        return;
    }

    if (p_partition->HasErr == DEF_YES) {
       *p_err = FS_ERR_PARTITION_INVALID;
        return;
    }



                                                                /* --------------------- RD ENTRY --------------------- */
    p_dev = p_partition->DevPtr;

    FSPartition_RdEntry( p_dev,
                        &partition_entry,
                         p_partition->CurMBR_Sec,
                         p_partition->CurMBR_Pos,
                         p_err);

    if (*p_err != FS_ERR_NONE) {
        return;
    }

    partition_size  = partition_entry.Size;
    partition_start = partition_entry.Start;
    partition_type  = partition_entry.Type;

    if ((partition_size  == 0u) &&
        (partition_start == 0u) &&
        (partition_type  == 0u)) {
        p_partition->HasErr = DEF_YES;
       *p_err = FS_ERR_PARTITION_ZERO;
        return;
    }



                                                                /* ----------------- HANDLE MBR ENTRY ----------------- */
    if (p_partition->InExt == DEF_YES) {
                                                                /* Move to next partition entry.                        */
        if ((partition_type == FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LINUX_EXT)) {
                                                                /* See Note #2b.                                        */
            partition_start += p_partition->PrimExtPartitionStart;
        } else {
            partition_start += p_partition->CurMBR_Sec;         /* See Note #2c.                                        */
        }

                                                                /* Validate partition.                                  */
        partition_end      = p_partition->PrimExtPartitionStart + p_partition->PrimExtPartitionSize;
        partition_end_prev = p_partition->PrevPartitionEnd;
        if ((partition_end_prev               >= partition_start)    ||
            (partition_start                  >  partition_end)      ||
            (partition_start + partition_size >  partition_end + 1u) ||
            (partition_size                   <= 1u)                 ||
            (partition_start                  <= 1u)) {
            p_partition->HasErr = DEF_YES;
           *p_err = FS_ERR_PARTITION_INVALID;
            return;
        }

                                                                /* Move to next partition entry.                        */
        p_partition->PrevMBR_Sec = p_partition->CurMBR_Sec;
        p_partition->PrevMBR_Pos = p_partition->CurMBR_Pos;

        if ((partition_type == FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LINUX_EXT)) {
            p_partition->CurMBR_Sec = partition_start;
            p_partition->CurMBR_Pos = 0u;
        } else {
            p_partition->PrevPartitionEnd = partition_start + partition_size - 1u;
            p_partition->CurMBR_Pos++;
        }

    } else {
                                                                /* Validate partition.                                  */
                                                                /* See Note #2a.                                        */
        partition_end      = p_dev->Size;
        partition_end_prev = p_partition->PrevPartitionEnd;
        if ((partition_end_prev               >= partition_start)    ||
            (partition_start                  >  partition_end)      ||
            (partition_start + partition_size >  partition_end + 1u) ||
            (partition_size                   <  1u)) {
            p_partition->HasErr = DEF_YES;
           *p_err = FS_ERR_PARTITION_INVALID;
            return;
        }

                                                                /* Move to next partition entry.                        */
        p_partition->PrevMBR_Sec = p_partition->CurMBR_Sec;
        p_partition->PrevMBR_Pos = p_partition->CurMBR_Pos;

        if ((partition_type == FS_PARTITION_TYPE_CHS_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LBA_MICROSOFT_EXT) ||
            (partition_type == FS_PARTITION_TYPE_LINUX_EXT)) {
                                                                /* Entry is first in sec MBR.                           */
            p_partition->CurMBR_Sec            = partition_start;
            p_partition->CurMBR_Pos            = 0u;
            p_partition->PrimExtPartitionStart = partition_start;
            p_partition->PrimExtPartitionSize  = partition_size;
            p_partition->InExt                 = DEF_YES;
        } else {
                                                                /* Entry is next in prim MBR (see Note #3a).            */
            p_partition->PrevPartitionEnd = partition_start + partition_size - 1u;
            p_partition->CurMBR_Pos++;
        }

    }

    p_partition->Partition++;



                                                                /* ----------------- ASSIGN ENTRY INFO ---------------- */
    p_partition_entry->Start = partition_start;
    p_partition_entry->Size  = partition_size;
    p_partition_entry->Type  = partition_type;
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        FSPartition_RdEntry()
*
* Description : Read a partition entry.
*
* Argument(s) : p_dev               Pointer to device.
*               ----------          Argument validated by caller.
*
*               p_partition_entry   Pointer to partition entry.
*               ----------          Argument validated by caller.
*
*               mbr_sec             MBR sector.
*
*               mbr_pos             Position in MBR sector.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                     Partition entry read.
*                                       FS_ERR_BUF_NONE_AVAIL           No buffer available.
*                                       FS_ERR_PARTITION_INVALID_SIG    Invalid MBR signature.
*                                       FS_ERR_DEV                      Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
static  void  FSPartition_RdEntry (FS_DEV              *p_dev,
                                   FS_PARTITION_ENTRY  *p_partition_entry,
                                   FS_SEC_NBR           mbr_sec,
                                   FS_PARTITION_NBR     mbr_pos,
                                   FS_ERR              *p_err)
{
    FS_SEC_QTY   partition_size;
    FS_SEC_NBR   partition_start;
    CPU_INT08U   partition_type;
    CPU_INT16U   sig_val;
    FS_BUF      *p_buf;
    CPU_INT08U  *p_temp;


                                                                /* ---------------- CLR PARTITION ENTRY --------------- */
    p_partition_entry->Start = 0u;
    p_partition_entry->Size  = 0u;
    p_partition_entry->Type  = 0u;




                                                                /* ---------------------- RD MBR ---------------------- */
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_temp = (CPU_INT08U *)p_buf->DataPtr;

    FSDev_RdLocked(        p_dev,                               /* Rd MBR sec.                                          */
                   (void *)p_temp,
                           mbr_sec,
                           1u,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return;
    }

                                                                /* Check sec sig.                                       */
    sig_val = MEM_VAL_GET_INT16U_LITTLE((void *)(p_temp + 510u));
    if (sig_val != 0xAA55u) {
        FS_TRACE_DBG(("FSPartition_RdEntry(): Invalid partition sig: 0x%04X != 0xAA55.\r\n", sig_val));
        FSBuf_Free(p_buf);
       *p_err = FS_ERR_PARTITION_INVALID_SIG;
        return;
    }



                                                                /* ------------------ READ MBR ENTRY ------------------ */
                                                                /* Rd start sec.                                        */
    partition_start = MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_START_LBA_1      + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)));
                                                                /* Rd partition size (see Note #2).                     */
    partition_size  = MEM_VAL_GET_INT32U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_SIZE_1           + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)));
                                                                /* Rd partition type.                                   */
    partition_type  = MEM_VAL_GET_INT08U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_PARTITION_TYPE_1 + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)));
    FS_TRACE_DBG(("FSPartition_RdEntry(): Found possible partition: Start: %d sector\r\n",  partition_start));
    FS_TRACE_DBG(("                                                 Size : %d sectors\r\n", partition_size));
    FS_TRACE_DBG(("                                                 Type : %02X\r\n",       partition_type));
    FSBuf_Free(p_buf);



                                                                /* ----------------- ASSIGN ENTRY INFO ---------------- */
    p_partition_entry->Start = partition_start;
    p_partition_entry->Size  = partition_size;
    p_partition_entry->Type  = partition_type;
   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSPartition_Wr()
*
* Description : Write a partition entry.
*
* Argument(s) : p_partition         Pointer to partition.
*               ----------          Argument validated by caller.
*
*               p_partition_entry   Pointer to partition entry containing size & type of new partition.
*                                   The start sector & size will be updated, if necessary, to reflect the
*                                   characteristics of the created partition.
*               ----------          Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE                   Partition entry written.
*                                       FS_ERR_BUF_NONE_AVAIL         No buffer available.
*                                       FS_ERR_PARTITION_MAX          Maximum number of partitions created.
*                                       FS_ERR_PARTITION_NOT_FINAL    Current partition not final partition.
*                                       FS_ERR_DEV                    Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) (a) See 'FSPartition_Rd() Note #3'.
*
*                   (b) If the MBR has the maximum number of partition entries, then no new partition
*                       entry may be created.
*
*                   (c) A partition can only be created after the final valid partition/partition entry.
*                       In this case, an error will ALWAYS have occurred previously while reading the
*                       partition structure.
*
*               (3) (a) If the partition is at the 0th entry of an extended partition, then that
*                       partition must have an illegal signature, a zero first entry or an invalid
*                       first entry.  In this case, that extended partition's first sector should be
*                       fully re-initialized.  The size should be checked against the previous MBR
*                       sector/entry as well.
*
*                       (1) If the previous MBR sector/entry is corrupt, e.g., the start sector/size are
*                           really invalid, then no partition will be created.
*
*                   (b) If the partition is at the 1st entry of an extended partition, then a new
*                       extended partition will be created (space permitting).
*
*                       (1) If the 1st entry is not 'zero', then no partition will be created.
*
*                   (c) The case in which the position is past the 1st entry of an extended partition is
*                       included for completeness.  This should NEVER be reached.
*
*                   (d) If the partition is within the primary partition, then a new partition entry
*                       will be created.
*
*                       (1) If this is the 4th entry, then no more partitions can be created on the
*                           device.
*
*                       (2) If the partition size in the partition entry passed to this function is too
*                           large or zero, it will be set to the largest possible size.
*
*                       (3) A partition MUST contain at least one sector.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN   == DEF_DISABLED)
static  void  FSPartition_Wr (FS_PARTITION        *p_partition,
                              FS_PARTITION_ENTRY  *p_partition_entry,
                              FS_ERR              *p_err)
{
    FS_SEC_NBR           partition_end_prev;
    FS_SEC_NBR           partition_end;
    FS_PARTITION_ENTRY   partition_entry;
    FS_SEC_QTY           partition_size;
    FS_SEC_NBR           partition_start;
    FS_DEV              *p_dev;


                                                                /* ------------ CHK FOR PARTITION LOCATION ------------ */
                                                                /* See Notes #2, #3b1.                                  */
    if (((p_partition->CurMBR_Pos == 4u) && (p_partition->InExt == DEF_NO)) ||
        ((p_partition->CurMBR_Pos == 2u) && (p_partition->InExt == DEF_YES))) {
       *p_err = FS_ERR_PARTITION_MAX;
        return;
    }

    if (p_partition->HasErr == DEF_NO) {                        /* See Note #2c.                                        */
       *p_err = FS_ERR_PARTITION_NOT_FINAL;
        return;
    }


    p_dev = p_partition->DevPtr;

    if (p_partition->InExt == DEF_YES) {                        /* --------------- CREATE SEC PARTITION --------------- */
        if (p_partition->CurMBR_Pos == 0u) {                    /* See Note #3a.                                        */
            FSPartition_RdEntry( p_dev,
                                &partition_entry,
                                 p_partition->PrevMBR_Sec,
                                 p_partition->PrevMBR_Pos,
                                 p_err);
            return;


        } else if (p_partition->CurMBR_Pos == 1u) {
            FSPartition_RdEntry( p_dev,
                                &partition_entry,
                                 p_partition->CurMBR_Sec,
                                 p_partition->CurMBR_Pos,
                                 p_err);

            if (*p_err != FS_ERR_NONE) {
                return;
            }

            if ((partition_entry.Size  != 0u) ||                /* See Note #3a2.                                       */
                (partition_entry.Start != 0u) ||
                (partition_entry.Type  != 0u)) {
                FS_TRACE_DBG(("FSPartition_Wr(): CANNOT CREATE EXT PARTITION. INVALID PARTITION ENTRY EXISTS: \r\n"));
                FS_TRACE_DBG(("                      Size  = %d\r\n", partition_entry.Size));
                FS_TRACE_DBG(("                      Start = %d\r\n", partition_entry.Start));
                FS_TRACE_DBG(("                      Type  = %d\r\n", partition_entry.Type));
               *p_err = FS_ERR_PARTITION_INVALID;
                return;
            }

        } else {                                                /* See Note #3c.                                        */
           *p_err = FS_ERR_PARTITION_INVALID;
            return;
        }



    } else {                                                    /* --------------- CREATE PRIM PARTITION -------------- */
        partition_end_prev = p_partition->PrevPartitionEnd;
        partition_end      = p_dev->Size;
        partition_size     = p_partition_entry->Size;
        partition_start    = partition_end_prev + 1u;

        if (partition_start > partition_end) {
           *p_err = FS_ERR_PARTITION_INVALID;
            return;
        }
                                                                /* See Note #3d.                                        */
        if ((partition_size                        == 0u)              ||
            (partition_start + partition_size <= partition_start + 1u) ||
            (partition_start + partition_size <= partition_size  + 1u) ||
            (partition_start + partition_size >  partition_end   + 1u)) {
             partition_size = partition_end - partition_start;
        }

        if (partition_size < 2u) {                              /* See Note #3d3.                                       */
            FS_TRACE_DBG(("FSPartition_Wr(): CANNOT CREATE PARTITION.  PARTITION WOULD BE TOO SMALL:\r\n"));
            FS_TRACE_DBG(("                      Size  = %d\r\n", partition_size));
            FS_TRACE_DBG(("                      Start = %d\r\n", partition_start));
            FS_TRACE_DBG(("                      End   = %d\r\n", partition_end));
           *p_err = FS_ERR_PARTITION_INVALID;
            return;
        }

        p_partition_entry->Start = partition_start;
        p_partition_entry->Size  = partition_size;

        FSPartition_WrEntry(p_dev,
                            p_partition_entry,
                            p_partition->CurMBR_Sec,
                            p_partition->CurMBR_Pos,
                            p_err);
    }
}
#endif
#endif



/*
*********************************************************************************************************
*                                        FSPartition_WrEntry()
*
* Description : Write a partition entry.
*
* Argument(s) : p_dev               Pointer to device.
*               ----------          Argument validated by caller.
*
*               p_partition_entry   Pointer to partition entry.
*               ----------          Argument validated by caller.
*
*               mbr_sec             MBR sector.
*               -------             Argument validated by caller.
*
*               mbr_pos             Position in MBR sector.
*               -------             Argument validated by caller.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*               ----------          Argument validated by caller.
*
*                                       FS_ERR_NONE              Partition entry written.
*                                       FS_ERR_BUF_NONE_AVAIL    No buffer available.
*                                       FS_ERR_DEV               Device access error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN   == DEF_DISABLED)
static  void  FSPartition_WrEntry (FS_DEV              *p_dev,
                                   FS_PARTITION_ENTRY  *p_partition_entry,
                                   FS_SEC_NBR           mbr_sec,
                                   FS_PARTITION_NBR     mbr_pos,
                                   FS_ERR              *p_err)
{
    FS_BUF      *p_buf;
    CPU_INT08U  *p_temp;


                                                                /* ---------------------- RD MBR ---------------------- */
    p_buf = FSBuf_Get((FS_VOL *)0);
    if (p_buf == (FS_BUF *)0) {
       *p_err = FS_ERR_BUF_NONE_AVAIL;
        return;
    }

    p_temp = (CPU_INT08U *)p_buf->DataPtr;

    FSDev_RdLocked(        p_dev,                               /* Rd MBR sec.                                          */
                   (void *)p_temp,
                           mbr_sec,
                           1u,
                           p_err);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
         FSBuf_Free(p_buf);
         return;
    }

    MEM_VAL_SET_INT16U_LITTLE((void *)(p_temp + 510u), 0xAA55u);/* Set sec sig.                                         */



                                                                /* ------------------- WR MBR ENTRY ------------------- */
    MEM_VAL_SET_INT08U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_BOOT_FLAG_1      + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)), FS_PARTITION_DOS_BOOT_FLAG);

    FSPartition_CalcCHS(               p_temp + FS_PARTITION_DOS_OFF_START_CHS_ADDR_1 + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE),  p_partition_entry->Start);

    MEM_VAL_SET_INT08U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_PARTITION_TYPE_1 + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)), p_partition_entry->Type);

    FSPartition_CalcCHS(               p_temp + FS_PARTITION_DOS_OFF_END_CHS_ADDR_1   + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE),  p_partition_entry->Start + p_partition_entry->Size - 1u);

    MEM_VAL_SET_INT32U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_START_LBA_1      + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)), p_partition_entry->Start);
    MEM_VAL_SET_INT32U_LITTLE((void *)(p_temp + FS_PARTITION_DOS_OFF_SIZE_1           + (mbr_pos * FS_PARTITION_DOS_TBL_ENTRY_SIZE)), p_partition_entry->Size);

    FSDev_WrLocked(         p_dev,                              /* Wr MBR sec.                                          */
                   (void  *)p_temp,
                            mbr_sec,
                            1u,
                            p_err);

    FSBuf_Free(p_buf);

    if (*p_err != FS_ERR_NONE) {
        *p_err  = FS_ERR_DEV;
    }
}
#endif
#endif
