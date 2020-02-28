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
*                                 SST SST39 NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_sst39.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports SST's SST39 Multi-Purpose Flash memories, as described in various
*                datasheets at SST (http://www.sst.com).  This driver has been tested with or should
*                work with the following devices :
*
*                    SST39LF200A/VF200A
*                    SST39LF400A/VF400A
*                    SST39LF800A/VF800A
*
*                    SST39VF1601/WF1601
*                    SST39VF3201        [*]
*                    SST39VF3202
*                    SST39VF6401
*                    SST39VF6402
*
*                    SST39SF010A
*                    SST39SF020A
*                    SST39SF040
*
*                    SST39WF1681
*                    SST39WF1682
*
*                    SST39WF800A
*                    SST39WF800B
*
*                    SST39LF512/VF512
*                    SST39LF010/VF010
*                    SST39LF020/VF020
*                    SST39LF040/VF040
*
*                          [*} Devices tested
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_SST39_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "../../../Source/fs.h"
#include  "fs_dev_nor_sst39.h"


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

#define  FS_DEV_NOR_PHY_MANUF_ID_SST                  0x00BFu   /* Manufacturer ID.                                     */

#define  FS_DEV_NOR_PHY_ALGO_SST                      0x0701u
#define  FS_DEV_NOR_PHY_ALGO_SST_B                    0x0002u

#define  FS_DEV_NOR_PHY_QUERY_OFF_QRY                   0x00u
#define  FS_DEV_NOR_PHY_QUERY_OFF_ALGO                  0x03u
#define  FS_DEV_NOR_PHY_QUERY_OFF_DEV_SIZE              0x17u
#define  FS_DEV_NOR_PHY_QUERY_OFF_REGION_CNT            0x1Cu
#define  FS_DEV_NOR_PHY_QUERY_OFF_BLKCNT                0x1Du
#define  FS_DEV_NOR_PHY_QUERY_OFF_BLKSIZE               0x1Fu

#define  FS_DEV_NOR_PHY_MAX_SECTOR_ERASE_ms               25u
#define  FS_DEV_NOR_PHY_MAX_BLOCK_ERASE_ms                25u
#define  FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms                 50u
#define  FS_DEV_NOR_PHY_MAX_PGM_us                        10u

/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*
* Note(s) : (1) For a 16-bit capable device used with a 16-bit bus, the following data/address pairs
*               constitute the standard command set.
*
*   ------------------------------------------------------------------------------------------------------------
*   |    COMMAND     | LEN |                        BUS OPERATION (ADDR = DATA)                                |
*   |                |     |     1st     |     2nd     |     3rd     |     4th     |     5th     |     6th     |
*   ------------------------------------------------------------------------------------------------------------
*   | Program        | 4   |  5555 = AA  |  2AAA = 55  |  5555 = A0  |  WA   = WD  |             |             |
*   | Chip Erase     | 6   |  5555 = AA  |  2AAA = 55  |  5555 = 80  |  5555 = AA  |  2AAA = 55  |  5555 = 10  |
*   | Block Erase    | 6   |  5555 = AA  |  2AAA = 55  |  5555 = 80  |  5555 = AA  |  2AAA = 55  |  BA   = 50  |
*   | Sector Erase   | 6   |  5555 = AA  |  2AAA = 55  |  5555 = 80  |  5555 = AA  |  2AAA = 55  |  SA   = 30  |
*   | Erase/Pgm Susp | 1   |  X    = B0  |             |             |             |             |             |
*   | Erase/Pgm Res  | 1   |  X    = 30  |             |             |             |             |             |
*   | Read sw ID     | 3   |  5555 = AA  |  2AAA = 55  |  5555 = 90  |             |             |             |
*   | Read CFI Query | 3   |  5555 = AA  |  2AAA = 55  |  5555 = 98  |             |             |             |
*   | Read/reset     | 3   |  5555 = AA  |  2AAA = 55  |  X    = F0  |             |             |             |
*   | Read/reset     | 1   |  X    = F0  |             |             |             |             |             |
*   ------------------------------------------------------------------------------------------------------------
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_ADDR_UNLOCK1_16               0x5555u
#define  FS_DEV_NOR_PHY_ADDR_UNLOCK2_16               0x2AAAu
#define  FS_DEV_NOR_PHY_ADDR_CFI_QUERY_16             0x5555u

#define  FS_DEV_NOR_PHY_DATA_UNLOCK1                    0xAAu
#define  FS_DEV_NOR_PHY_DATA_UNLOCK2                    0x55u

#define  FS_DEV_NOR_PHY_CMD_ERASE_CHIP                  0x10u
#define  FS_DEV_NOR_PHY_CMD_RESUME                      0x30u
#define  FS_DEV_NOR_PHY_CMD_ERASE_SEC                   0x30u
#define  FS_DEV_NOR_PHY_CMD_ERASE_BLK                   0x50u
#define  FS_DEV_NOR_PHY_CMD_ERASE_SETUP                 0x80u
#define  FS_DEV_NOR_PHY_CMD_RD_SW_ID                    0x90u
#define  FS_DEV_NOR_PHY_CMD_CFI_QUERY                   0x98u
#define  FS_DEV_NOR_PHY_CMD_PGM                         0xA0u
#define  FS_DEV_NOR_PHY_CMD_SUSPEND                     0xB0u
#define  FS_DEV_NOR_PHY_CMD_RD                          0xF0u

/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_REG_DATAPOLLING         DEF_BIT_07
#define  FS_DEV_NOR_PHY_REG_TOGGLE              DEF_BIT_06
#define  FS_DEV_NOR_PHY_REG_ALTTOGGLE           DEF_BIT_02


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

const  FS_DEV_NOR_PHY_API  FSDev_NOR_SST39_1x16 = {
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
*               (2) (a) SST39 devices are monolithic; they consist of one region with uniformly-sized
*                       blocks.
*
*                   (b) SST39 device are divided into large 'blocks' & smaller 'sectors'.  The two regions
*                       described in the CFI information are NOT two continguous block regions; rather,
*                       they are alternate geometries.  The first specifies the quantity & size of the
*                       sectors; the second, the quantity & size of the blocks.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Open (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                  FS_ERR               *p_err)
{
    CPU_INT16U    algo;
    CPU_INT32U    blk_cnt;
    CPU_INT32U    blk_size;
    CPU_INT08U    blk_region_cnt;
    CPU_INT08U    datum_08;
    CPU_INT16U    datum_16;
#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    CPU_INT32U    dev_size;
    CPU_INT08U    dev_size_log;
#endif
    CPU_SIZE_T    i;
    CPU_BOOLEAN   ok;
    CPU_INT08U    query_info[40];


                                                                /* -------------------- CHK PARAMS -------------------- */
    if (p_phy_data->RegionNbr != 0u) {                          /* Validate region nbr (see Note #2a).                  */
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }



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
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK1);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK2_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK2);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_CMD_CFI_QUERY);

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
    if ((query_info[0x00] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_Q) ||                   /* Verify ID string.            */
        (query_info[0x01] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_R) ||
        (query_info[0x02] != (CPU_INT08U)ASCII_CHAR_LATIN_UPPER_Y)) {
        FS_TRACE_INFO(("NOR PHY PHY: Query string wrong: (0x%02X != 0x%02X) OR (0x%02X != 0x%02X) OR (0x%02X != 0x%02X)\r\n", query_info[0x00], ASCII_CHAR_LATIN_UPPER_Q,
                                                                                                                              query_info[0x01], ASCII_CHAR_LATIN_UPPER_R,
                                                                                                                              query_info[0x02], ASCII_CHAR_LATIN_UPPER_Y));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    algo = MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_ALGO]);      /* Cmd set algo.                */
    if ((algo != FS_DEV_NOR_PHY_ALGO_SST) &&
        (algo != FS_DEV_NOR_PHY_ALGO_SST_B)) {
        FS_TRACE_INFO(("NOR PHY SST39: Algo wrong: %04X != %04X (or %04X for newer devices).\r\n", algo, FS_DEV_NOR_PHY_ALGO_SST, FS_DEV_NOR_PHY_ALGO_SST_B));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    dev_size_log = query_info[FS_DEV_NOR_PHY_QUERY_OFF_DEV_SIZE];                       /* Dev size.                    */
    if ((dev_size_log <= 10u) || (dev_size_log >= 32u)) {
        FS_TRACE_INFO(("NOR PHY SST39: Device size wrong: (%d <= 10) || (%d >= 32).\r\n", dev_size_log, dev_size_log));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
    dev_size       = 1uL << dev_size_log;
#endif

    blk_region_cnt = query_info[FS_DEV_NOR_PHY_QUERY_OFF_REGION_CNT];                   /* Blk reg info.                */
    if (blk_region_cnt == 0u) {
        FS_TRACE_INFO(("NOR PHY SST39: Blk region cnt wrong: (%d == 0).\r\n", blk_region_cnt));
       *p_err = FS_ERR_DEV_IO;
        return;
    }
                                                                                        /* Blk cnt/size (see Note #2b). */
    blk_cnt   = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKCNT  + 4u]);
    blk_cnt  +=  1u;
    blk_size  = (CPU_INT32U)MEM_VAL_GET_INT16U((void *)&query_info[FS_DEV_NOR_PHY_QUERY_OFF_BLKSIZE + 4u]);
    blk_size *=  256u;


                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY SST39: Dev size: %d\r\n",     dev_size));
    FS_TRACE_INFO(("               Algo    : 0x%04X\r\n", algo));
    FS_TRACE_INFO(("               Blk cnt : %d\r\n",     blk_cnt));
    FS_TRACE_INFO(("               Blk size: %d\r\n",     blk_size));

    p_phy_data->BlkCnt          =  blk_cnt;
    p_phy_data->BlkSize         =  blk_size;
    p_phy_data->DataPtr         = (void *)0;
    p_phy_data->AddrRegionStart =  p_phy_data->AddrBase;
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
                        (cnt + 1u) / 2u);

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
    CPU_BOOLEAN   ok;
    CPU_INT08U   *p_src_08;
    CPU_INT16U    src_datum;


    p_src_08  = (CPU_INT08U *)p_src;
    cnt_words =  cnt / 2u;

    while (cnt_words > 0u) {
                                                                /* ------------------ PGM WORD IN DEV ----------------- */
        src_datum = MEM_VAL_GET_INT16U((void *)p_src_08);

        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK1);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK2_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK2);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_CMD_PGM);
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase +  start,                                src_datum);



                                                                /* --------------- WAIT FOR PGM TO FINISH ------------- */
        ok = FSDev_NOR_BSP_WaitWhileBusy(p_phy_data->UnitNbr,
                                         p_phy_data,
                                         FSDev_NOR_PHY_PollFnct,
                                         FS_DEV_NOR_PHY_MAX_PGM_us);

        if (ok != DEF_OK) {                                     /* Rtn err if pgm timed out.                            */
                                                                /* Reset dev.                                           */
            FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);
           *p_err = FS_ERR_DEV_TIMEOUT;
            FS_TRACE_DBG(("NOR PHY SST39: Pgm timeout.\r\n"));
            return;
        }

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
    CPU_BOOLEAN  ok;


    (void)size;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */


                                                                /* --------------------- ERASE BLK -------------------- */
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK1);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK2_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK2);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_CMD_ERASE_SETUP);

    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK1_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK1);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase + (FS_DEV_NOR_PHY_ADDR_UNLOCK2_16 * 2u), FS_DEV_NOR_PHY_DATA_UNLOCK2);
    FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase +  start,                                FS_DEV_NOR_PHY_CMD_ERASE_BLK);



                                                                /* -------------- WAIT FOR ERASE TO FINISH ------------ */
    ok = FSDev_NOR_BSP_WaitWhileBusy(p_phy_data->UnitNbr,
                                     p_phy_data,
                                     FSDev_NOR_PHY_PollFnct,
                                     FS_DEV_NOR_PHY_MAX_BLOCK_ERASE_ms * 1000u);

    if (ok != DEF_OK) {                                         /* Rtn err if pgm timed out.                            */
                                                                /* Reset dev.                                           */
        FSDev_NOR_BSP_WrWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase, FS_DEV_NOR_PHY_CMD_RD);
       *p_err  = FS_ERR_DEV_TIMEOUT;
        FS_TRACE_DBG(("NOR PHY SST39: Erase timeout.\r\n"));
        return;
    }

   *p_err = FS_ERR_NONE;
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
    CPU_BOOLEAN  toggle1;
    CPU_BOOLEAN  toggle2;
    CPU_INT16U   word1;
    CPU_INT16U   word2;


    word1   = FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase);
    word2   = FSDev_NOR_BSP_RdWord_16(p_phy_data->UnitNbr, p_phy_data->AddrBase);

    toggle1 = DEF_BIT_IS_SET(word1, FS_DEV_NOR_PHY_REG_TOGGLE);
    toggle2 = DEF_BIT_IS_SET(word2, FS_DEV_NOR_PHY_REG_TOGGLE);

    rdy     = (toggle1 == toggle2) ? DEF_YES : DEF_NO;

    return (rdy);
}
