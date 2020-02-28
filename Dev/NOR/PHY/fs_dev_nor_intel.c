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
*                                      FILE SYSTEM DEVICE DRIVER
*
*                                          NOR FLASH DEVICES
*                             INTEL-COMPATIBLE NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_intel.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports CFI NOR flash implementing Intel command set, including :
*
*                (a) Most Intel/Numonyx devices.
*                (b) Some ST/Numonyx M28 devices.
*                (c) #### List tested devices.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_INTEL_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "../../../Source/fs.h"
#include  "fs_dev_nor_intel.h"


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

#define  FS_DEV_NOR_PHY_ALGO_INTEL                    0x0001u

#define  FS_DEV_NOR_PHY_QUERY_OFF_QRY                   0x00u
#define  FS_DEV_NOR_PHY_QUERY_OFF_ALGO                  0x03u
#define  FS_DEV_NOR_PHY_QUERY_OFF_DEV_SIZE              0x17u
#define  FS_DEV_NOR_PHY_QUERY_OFF_MULTIBYTE_WR_SIZE     0x1Au
#define  FS_DEV_NOR_PHY_QUERY_OFF_REGION_CNT            0x1Cu
#define  FS_DEV_NOR_PHY_QUERY_OFF_BLKCNT                0x1Du
#define  FS_DEV_NOR_PHY_QUERY_OFF_BLKSIZE               0x1Fu

#define  FS_DEV_NOR_PHY_MAX_PGM_us                       200u
#define  FS_DEV_NOR_PHY_MAX_BLOCK_ERASE_ms              4000u

/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*
* Note(s) : (1) For a 16-bit capable device used with a 16-bit bus, the following data/address pairs
*               constitute the standard command set.
*
*   ----------------------------------------------------------------------------------------------
*   |          COMMAND        |  LEN |                 BUS OPERATION (ADDR = DATA)               |
*   |                         |      |     1st      |     2nd      |     3rd      |     4th      |
*   ----------------------------------------------------------------------------------------------
*   |  Read Array             |  1   |  xx = 0xFF   |              |              |              |
*   |  Read Dev ID            |  2+  |  xx = 0x90   |  rd(DBA+IA)  |              |              |
*   |  CFI Query              |  2+  |  xx = 0x98   |  rd(DBA+QA)  |              |              |
*   |  Read  Status Reg       |  2   |  xx = 0x70   |  rd(xx)      |              |              |
*   |  Clear Status Reg       |  1   |  xx = 0x50   |              |              |              |
*   ----------------------------------------------------------------------------------------------
*   |  Word     Program       |  2   |  WA = 0x40   |  WA = WD     |              |              |
*   |  Buffered Program       |  2+  |  WA = 0xE8   |  WA = N - 1  |  WA = WD     |  etc.        |
*   ----------------------------------------------------------------------------------------------
*   |  Block Erase            |  2   |  BA = 0x20   |  BA = 0xD0   |              |              |
*   ----------------------------------------------------------------------------------------------
*   |  Program/Erase Suspend  |  1   |  xx = 0xB0   |              |              |              |
*   |  Program/Erase Resume   |  1   |  xx = 0xD0   |              |              |              |
*   ----------------------------------------------------------------------------------------------
*   |  Lock Block             |  2   |  BA = 0x60   |  BA = 0x01   |              |              |
*   |  Unlock Block           |  2   |  BA = 0x60   |  BA = 0xD0   |              |              |
*   |  Lockdown Block         |  2   |  BA = 0x60   |  BA = 0x2F   |              |              |
*   ----------------------------------------------------------------------------------------------
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_CMD_RD                          0xFFu
#define  FS_DEV_NOR_PHY_CMD_RD_ID                       0x90u
#define  FS_DEV_NOR_PHY_CMD_RD_CFI                      0x98u
#define  FS_DEV_NOR_PHY_CMD_RD_STATUS                   0x70u
#define  FS_DEV_NOR_PHY_CMD_CLR_STATUS                  0x50u

#define  FS_DEV_NOR_PHY_CMD_PGM_WORD                    0x40u
#define  FS_DEV_NOR_PHY_CMD_PGM_BUFD                    0xEBu

#define  FS_DEV_NOR_PHY_CMD_ERASE_SETUP                 0x20u
#define  FS_DEV_NOR_PHY_CMD_ERASE_CONFIRM               0xD0u

#define  FS_DEV_NOR_PHY_CMD_SUSPEND                     0xB0u
#define  FS_DEV_NOR_PHY_CMD_RESUME                      0xD0u

#define  FS_DEV_NOR_PHY_CMD_LOCK_SETUP                  0x60u
#define  FS_DEV_NOR_PHY_CMD_LOCK_CONFIRM                0x01u
#define  FS_DEV_NOR_PHY_CMD_UNLOCK_CONFIRM              0xD0u
#define  FS_DEV_NOR_PHY_CMD_LOCKDOWN_CONFIRM            0x2Fu

#define  FS_DEV_NOR_PHY_DATA_UNLOCK1                    0x60u
#define  FS_DEV_NOR_PHY_DATA_UNLOCK2                    0xD0u

/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_REG_DWS                     DEF_BIT_07  /* Device Write Status.                                 */
#define  FS_DEV_NOR_PHY_REG_ESS                     DEF_BIT_06  /* Erase Suspend Status.                                */
#define  FS_DEV_NOR_PHY_REG_ES                      DEF_BIT_05  /* Erase Status.                                        */
#define  FS_DEV_NOR_PHY_REG_PS                      DEF_BIT_04  /* Program Status.                                      */
#define  FS_DEV_NOR_PHY_REG_BLS                     DEF_BIT_03  /* Block Lock Status.                                   */
#define  FS_DEV_NOR_PHY_REG_PSS                     DEF_BIT_02  /* Program Suspend Status.                              */


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
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                    /* ---- NOR PHY INTERFACE FNCTS --- */
static  void         FSDev_NOR_PHY_Open     (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Open NOR device.                 */
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_Close    (FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Close NOR device.                */

static  void         FSDev_NOR_PHY_Rd       (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Read from NOR device.            */
                                             void                 *p_dest,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_Wr       (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Write to NOR device.             */
                                             void                 *p_src,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_EraseBlk (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Erase block on NOR device.       */
                                             CPU_INT32U            start,
                                             CPU_INT32U            size,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_IO_Ctrl  (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Perform NOR device I/O control.  */
                                             CPU_INT08U            opt,
                                             void                 *p_data,
                                             FS_ERR               *p_err);


                                                                                    /* ---------- LOCAL FNCTS --------- */
static  void         FSDev_NOR_PHY_EraseChip(FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Erase NOR device.                */
                                             FS_ERR               *p_err);

static  CPU_BOOLEAN  FSDev_NOR_PHY_PollFnct (FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Poll fnct.                       */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_Intel_1x16 = {
    FSDev_NOR_PHY_Open,
    FSDev_NOR_PHY_Close,
    FSDev_NOR_PHY_Rd,
    FSDev_NOR_PHY_Wr,
    FSDev_NOR_PHY_EraseBlk,
    FSDev_NOR_PHY_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                               NOR PHYSICAL DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        FSDev_NOR_PHY_Open()
*
* Description : Open (initialize) a NOR device instance & get NOR device information.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Device driver initialized successfully.
*                               FS_ERR_DEV_INVALID_CFG         Device configuration specified invalid.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Several members of 'p_phy_data' may need to be used/assigned :
*
*                   (a) 'BlkCnt' & 'BlkSize' MUST be assigned the block count & block size of the device,
*                       respectively.
*
*                   (b) 'RegionNbr' specifies the block region that will be used.  'AddrRegionStart' MUST
*                       be assigned the start address of this block region.
*
*                   (c) 'DataPtr' may store a pointer to any driver-specific data.
*
*                   (d) 'UnitNbr' is the unit number of the NOR device.
*
*                   (e) 'BusWidth', 'BusWidthMax' & 'PhyDevCnt' specify the bus configuration.
*                       'AddrBase' specifies the base address of the NOR flash memory.
*
*               (2) If multi-byte write size is non-zero, NOR flash is assumed to support the "buffered
*                   program".  However, some flash implementing Intel command set may have non-zero
*                   multi-byte write size but do NOT support the "buffered program" command.  When used
*                   with such devices, this driver must be modified to ignore the multi-byte write size
*                   in the CFI information.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Open (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                  FS_ERR               *p_err)
{
    CPU_ADDR     addr_region_start;
    CPU_INT16U   algo;
    CPU_INT32U   blk_cnt;
    CPU_INT08U   blk_region_cnt;
    CPU_INT32U   blk_size;
    CPU_INT08U   datum_08;
    CPU_INT16U   datum_16;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    CPU_INT32U   dev_size;
    CPU_INT08U   dev_size_log;
#endif
    CPU_SIZE_T   i;
    CPU_INT32U   mult_size;
    CPU_INT08U   mult_size_log;
    CPU_BOOLEAN  ok;
    CPU_INT08U   query_info[40];
    CPU_INT08U   region_nbr;


                                                                /* ---------------------- INIT HW --------------------- */
    ok = FSDev_NOR_BSP_Open(p_phy_data->UnitNbr,
                            p_phy_data->AddrBase,
                            p_phy_data->BusWidth,
                            p_phy_data->PhyDevCnt);
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return;
    }



                                                                /* ------------------- RD QUERY INFO ------------------ */
                                                                /* Enter query mode.                                    */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD_CFI);

    i = 0u;
    while (i < sizeof(query_info)) {                            /* Rd query info.                                       */
        datum_16      =  FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + 0x20u + (i * 2u));
        datum_08      = (CPU_INT08U)datum_16;
        query_info[i] =  datum_08;

        i++;
    }

                                                                /* Reset dev.                                           */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);



                                                                /* ----------------- VERIFY QUERY INFO ---------------- */
    if ((query_info[0x00] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_Q) ||               /* Verify ID string.                */
        (query_info[0x01] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_R) ||
        (query_info[0x02] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_Y)) {
        FS_TRACE_INFO(("NOR PHY Intel 1x16: Query string wrong: (0x%02X != 0x%02X) OR (0x%02X != 0x%02X) OR (0x%02X != 0x%02X)\r\n", query_info[0x00], ASCII_CHAR_LATIN_UPPER_Q,
                                                                                                                                     query_info[0x01], ASCII_CHAR_LATIN_UPPER_R,
                                                                                                                                     query_info[0x02], ASCII_CHAR_LATIN_UPPER_Y));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    algo = MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_ALGO]);  /* Cmd set algo.                    */
    if (algo != FS_DEV_NOR_PHY_ALGO_INTEL) {
        FS_TRACE_INFO(("NOR PHY Intel 1x16: Algo wrong: %04X != %04X.\r\n", algo, FS_DEV_NOR_PHY_ALGO_INTEL));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    dev_size_log = query_info[FS_DEV_NOR_PHY_QUERY_OFF_DEV_SIZE];                   /* Dev size.                        */
    if ((dev_size_log <= 10u) || (dev_size_log >= 32u)) {
        FS_TRACE_INFO(("NOR PHY Intel 1x16: Device size wrong: (%d <= 10) || (%d >= 32).\r\n", dev_size_log, dev_size_log));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
    dev_size       = 1uL << dev_size_log;
#endif

    blk_region_cnt = query_info[FS_DEV_NOR_PHY_QUERY_OFF_REGION_CNT];               /* Blk reg info.                    */
    if (blk_region_cnt == 0u) {
        FS_TRACE_INFO(("NOR PHY Intel 1x16: Blk region cnt wrong: (%d == 0).\r\n", blk_region_cnt));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
    if (p_phy_data->RegionNbr > blk_region_cnt) {
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

                                                                                    /* Blk cnt & size.                  */
    addr_region_start = p_phy_data->AddrBase;
    region_nbr        = 0u;
    while (region_nbr < p_phy_data->RegionNbr) {
        blk_cnt            = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKCNT  + (4u * region_nbr)]);
        blk_cnt           +=  1u;
        blk_size           = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKSIZE + (4u * region_nbr)]);
        blk_size          *=  256u;

        addr_region_start +=  blk_cnt * blk_size;

        region_nbr++;
    }
    blk_cnt   = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKCNT  + (4u * region_nbr)]);
    blk_cnt  +=  1u;
    blk_size  = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKSIZE + (4u * region_nbr)]);
    blk_size *=  256u;

#ifndef  NOR_NO_BUF_PGM                                                            /* Multi-byte wr size (see Note #2).*/
    mult_size_log = query_info[FS_DEV_NOR_PHY_QUERY_OFF_MULTIBYTE_WR_SIZE];
    if ((mult_size_log == 0u) || (mult_size_log >= 32u)) {
        mult_size = 0u;
    } else {
        mult_size = 1uL << mult_size_log;
    }
#else
    mult_size = 0u;
#endif


                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY Intel 1x16: Dev size: %d\r\n",     dev_size));
    FS_TRACE_INFO(("                    Algo    : 0x%04X\r\n", algo));
    FS_TRACE_INFO(("                    Blk cnt : %d\r\n",     blk_cnt));
    FS_TRACE_INFO(("                    Blk size: %d\r\n",     blk_size));

    p_phy_data->BlkCnt          =  blk_cnt;
    p_phy_data->BlkSize         =  blk_size;
    p_phy_data->DataPtr         = (void *)0;
    p_phy_data->AddrRegionStart =  addr_region_start;

    p_phy_data->WrMultSize      =  mult_size;
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_PHY_Close()
*
* Description : Close (uninitialize) a NOR device instance.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Close (FS_DEV_NOR_PHY_DATA  *p_phy_data)
{
    FSDev_NOR_BSP_Close(p_phy_data->UnitNbr);
    p_phy_data->DataPtr = (void *)0;
}


/*
*********************************************************************************************************
*                                         FSDev_NOR_PHY_Rd()
*
* Description : Read from a NOR device & store data in buffer.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               p_dest      Pointer to destination buffer.
*
*               start       Start address of read (relative to start of device).
*
*               cnt         Number of octets to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Octets read successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Rd (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                void                 *p_dest,
                                CPU_INT32U            start,
                                CPU_INT32U            cnt,
                                FS_ERR               *p_err)
{
                                                                /* Make certain NOR is in read mode.                    */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);

    FSDev_NOR_BSP_Rd_16( p_phy_data->UnitNbr,                   /* Copy data to buf.                                    */
                         p_dest,
                        (p_phy_data->AddrBase + start),
                         cnt / 2u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_NOR_PHY_Wr()
*
* Description : Write data to a NOR device from a buffer.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               p_src       Pointer to source buffer.
*
*               start       Start address of write (relative to start of device).
*
*               cnt         Number of octets to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Octets written successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Wr (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                void                 *p_src,
                                CPU_INT32U            start,
                                CPU_INT32U            cnt,
                                FS_ERR               *p_err)
{
    CPU_INT32U    cnt_words;
    CPU_INT16U    datum;
    CPU_BOOLEAN   err;
    CPU_INT08U   *p_src_08;
    CPU_BOOLEAN   ok;
    CPU_INT16U    status;


    p_src_08  = (CPU_INT08U *)p_src;
    cnt_words =  cnt / 2u;

    while (cnt_words > 0u) {
                                                                /* ------------------ PGM WORD IN DEV ----------------- */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_DATA_UNLOCK1);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_DATA_UNLOCK2);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_CMD_PGM_WORD);
        datum = MEM_VAL_GET_INT16U((void *)p_src_08);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, datum);



                                                                /* --------------- WAIT FOR PGM TO FINISH ------------- */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD_STATUS);
        ok = FSDev_NOR_BSP_WaitWhileBusy(p_phy_data->UnitNbr,
                                         p_phy_data,
                                         FSDev_NOR_PHY_PollFnct,
                                         FS_DEV_NOR_PHY_MAX_PGM_us);



                                                                /* -------------------- CHK FOR ERR ------------------- */
        if (ok != DEF_OK) {                                     /* Rtn err if pgm timed out.                            */
           *p_err = FS_ERR_DEV_TIMEOUT;
            FS_TRACE_DBG(("NOR PHY Intel 1x16: Pgm timeout.\r\n"));
            return;
        }

                                                                /* Rd status reg.                                       */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD_STATUS);
        status = FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase);
        err    = DEF_BIT_IS_SET_ANY(status, FS_DEV_NOR_PHY_REG_ES | FS_DEV_NOR_PHY_REG_PS | FS_DEV_NOR_PHY_REG_BLS);
        if (err == DEF_YES) {
                                                                /* Clr err.                                             */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_CLR_STATUS);
                                                                /* Rtn to rd mode.                                      */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);
           *p_err = FS_ERR_DEV_TIMEOUT;
            FS_TRACE_DBG(("NOR PHY Intel 1x16: Pgm err: %02X.\r\n", status));
            return;

        }

                                                                /* Rtn to rd mode.                                      */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);

        p_src_08 += 2u;
        start    += 2u;
        cnt_words--;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PHY_EraseBlk()
*
* Description : Erase block of NOR device.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               start       Start address of block (relative to start of device).
*
*               size        Size of block, in octets.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              Block erased successfully.
*                               FS_ERR_DEV_INVALID_OP    Invalid operation for device.
*                               FS_ERR_DEV_IO            Device I/O error.
*                               FS_ERR_DEV_TIMEOUT       Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_EraseBlk (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                      CPU_INT32U            start,
                                      CPU_INT32U            size,
                                      FS_ERR               *p_err)
{
    CPU_BOOLEAN  err;
    CPU_BOOLEAN  ok;
    CPU_INT16U   status;


    (void)size;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */


                                                                /* --------------------- ERASE BLK -------------------- */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_DATA_UNLOCK1);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_DATA_UNLOCK2);

    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_CMD_ERASE_SETUP);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + start, FS_DEV_NOR_PHY_CMD_ERASE_CONFIRM);



                                                                /* -------------- WAIT FOR ERASE TO FINISH ------------ */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD_STATUS);
    ok = FSDev_NOR_BSP_WaitWhileBusy(p_phy_data->UnitNbr,
                                     p_phy_data,
                                     FSDev_NOR_PHY_PollFnct,
                                     FS_DEV_NOR_PHY_MAX_BLOCK_ERASE_ms * 1000uL);



                                                                /* -------------------- CHK FOR ERR ------------------- */
    if (ok != DEF_OK) {                                         /* Rtn err if pgm timed out.                            */
       *p_err = FS_ERR_DEV_TIMEOUT;
        FS_TRACE_DBG(("NOR PHY Intel 1x16: Erase timeout.\r\n"));

    } else {
                                                                /* Rd status reg.                                       */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD_STATUS);
        status = FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase);
        err    = DEF_BIT_IS_SET_ANY(status, FS_DEV_NOR_PHY_REG_ES | FS_DEV_NOR_PHY_REG_PS | FS_DEV_NOR_PHY_REG_BLS);
        if (err == DEF_YES) {
                                                                /* Clr err.                                             */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_CLR_STATUS);
                                                                /* Rtn to rd mode.                                      */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);
           *p_err = FS_ERR_DEV_TIMEOUT;
            FS_TRACE_DBG(("NOR PHY Intel 1x16: Erase err: %02X.\r\n", status));

        } else {
                                                                /* Rtn to rd mode.                                      */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);
           *p_err = FS_ERR_NONE;
        }
    }
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_IO_Ctrl()
*
* Description : Perform NOR device I/O control operation.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               opt         Control command.
*
*               p_data      Buffer which holds data to be used for operation.
*                              OR
*                           Buffer in which data will be stored as a result of operation.
*
*               p_err       Pointer to variable that will receive the return the error code from this function :
*
*                               FS_ERR_NONE                   Control operation performed successfully.
*                               FS_ERR_DEV_INVALID_IO_CTRL    I/O control operation unknown to driver.
*                               FS_ERR_DEV_INVALID_OP         Invalid operation for device.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Defined I/O control operations are :
*
*                   (a) FS_DEV_IO_CTRL_PHY_ERASE_CHIP    Erase entire chip.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_IO_Ctrl (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                     CPU_INT08U            opt,
                                     void                 *p_data,
                                     FS_ERR               *p_err)
{
    (void)p_data;                                               /*lint --e{550} Suppress "Symbol not accessed".         */

    switch (opt) {
        case FS_DEV_IO_CTRL_PHY_ERASE_CHIP:                     /* -------------------- ERASE CHIP -------------------- */
             FSDev_NOR_PHY_EraseChip(p_phy_data, p_err);
             break;


        case FS_DEV_IO_CTRL_REFRESH:
        default:                                                /* --------------- UNSUPPORTED I/O CTRL --------------- */
            *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
             break;
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
*                                      FSDev_NOR_PHY_EraseChip()
*
* Description : Erase NOR device.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE              Block erased successfully.
*                               FS_ERR_DEV_INVALID_OP    Invalid operation for device.
*                               FS_ERR_DEV_IO            Device I/O error.
*                               FS_ERR_DEV_TIMEOUT       Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_EraseChip (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                       FS_ERR               *p_err)
{
    (void)p_phy_data;                                           /*lint --e{550} Suppress "Symbol not accessed".         */

   *p_err = FS_ERR_DEV_INVALID_OP;
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PHY_PollFnct()
*
* Description : Poll function.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
* Return(s)   : DEF_YES, if operation is complete or error occurred.
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_NOR_PHY_PollFnct (FS_DEV_NOR_PHY_DATA  *p_phy_data)
{
    CPU_BOOLEAN  rdy;
    CPU_INT16U   status;


    status = FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase);
    rdy    = DEF_BIT_IS_SET(status, FS_DEV_NOR_PHY_REG_DWS);

    return (rdy);
}
