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
*                             SST SST25 SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_sst25.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports SST's SST25 serial NOR flash memories, as described in various datasheets
*                at SST (http://www.sst.com).  This driver has been tested with or should work with
*                the following devices :
*
*                    SST25VF512A                           [*]
*                    SST25VF512B
*                    SST25VF010/SST25VF010A                [*]
*                    SST25VF020/SST25VF020A/SST25LF020A    [*]
*                    SST25WF020B
*                    SST25VF040A/SST25LF040A
*                    SST25VF040B                           [*]
*                    SST25WF040B
*                    SST25VF080B                           [*]
*                    SST25VF080
*                    SST25VF016B                           [*]
*                    SST25VF032B                           [*]
*
*                          [*} Devices tested
*
*            (2) SST's SST25 family of serial NOR flash memory are programmed on a word (2 B) basis
*                & erased on a sector (4 kB) or block (32 kB) basis.  The revision A devices &
*                revision B devices differ slightly :
*
*                (a) Both have an Auto-Address Increment (AAI) programming mode.  In revision A
*                    devices, the programming is performed byte-by-byte.  In revision B devices, the
*                    programming is performed word-by-word.
*
*                (b) Revision B devices can also be erased on 64 kB block basis.
*
*                (c) Revision B devices support a command to read a JEDEC-compatible ID.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_SST25_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "../../../Source/fs.h"
#include  "fs_dev_nor_sst25.h"


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

#define  FS_DEV_NOR_PHY_MANUF_ID_SST                    0xBFu   /* Manufacturer ID.                                     */


                                                                /* ----------------- OPERATION TIMING ----------------- */
#define  FS_DEV_NOR_PHY_MAX_SECTOR_ERASE_ms               25u
#define  FS_DEV_NOR_PHY_MAX_BLOCK_ERASE_ms                25u
#define  FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms                 50u
#define  FS_DEV_NOR_PHY_MAX_BYTE_PGM_us                   10u

#define  FS_DEV_NOR_PHY_MAX_FREQ_NORMAL             25000000uL
#define  FS_DEV_NOR_PHY_MAX_FREQ_FAST               50000000uL


                                                                /* ------------------- DEVICE FAMILY ------------------ */
#define  FS_DEV_NOR_PHY_FAMILY_NONE                        0u
#define  FS_DEV_NOR_PHY_FAMILY_SST25A                      1u
#define  FS_DEV_NOR_PHY_FAMILY_SST25B                      2u
#define  FS_DEV_NOR_PHY_FAMILY_SST25C                      3u



/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*********************************************************************************************************
*/
                                                                /* ----------- SST25 A, B & C INSTRUCTIONS ------------ */
#define  FS_DEV_NOR_PHY_INSTR_RD                        0x03u   /* Rd data bytes.                                       */
#define  FS_DEV_NOR_PHY_INSTR_FAST_RD                   0x0Bu   /* Rd data bytes at higher speed.                       */
#define  FS_DEV_NOR_PHY_INSTR_SE                        0x20u   /* 4-kB sec erase.                                      */
#define  FS_DEV_NOR_PHY_INSTR_BE32                      0x52u   /* 32-kB blk  erase.                                    */
#define  FS_DEV_NOR_PHY_INSTR_CE                        0x60u   /* Chip erase.                                          */
#define  FS_DEV_NOR_PHY_INSTR_BP                        0x02u   /* Byte pgm.                                            */
#define  FS_DEV_NOR_PHY_INSTR_RDSR                      0x05u   /* Rd status.                                           */
#define  FS_DEV_NOR_PHY_INSTR_EWSR                      0x50u   /* En wr status reg.                                    */
#define  FS_DEV_NOR_PHY_INSTR_WRSR                      0x01u   /* Wr status reg.                                       */
#define  FS_DEV_NOR_PHY_INSTR_WREN                      0x06u   /* Wr en.                                               */
#define  FS_DEV_NOR_PHY_INSTR_WRDI                      0x04u   /* Wr dis.                                              */
#define  FS_DEV_NOR_PHY_INSTR_RDID                      0x90u   /* Rd ID.                                               */

                                                                /* ------------- SST25 B & C INSTRUCTIONS ------------- */
#define  FS_DEV_NOR_PHY_INSTR_BE64                      0xD8u   /* 64-kB blk  erase.                                    */
#define  FS_DEV_NOR_PHY_INSTR_RDJEDECID                 0x9Fu   /* Rd JEDEC ID.                                         */

                                                                /* ------------- SST25 A-ONLY INSTRUCTIONS ------------ */
#define  FS_DEV_NOR_PHY_INSTR_AIIP                      0xAFu   /* Auto addr inc byte-pgm.                              */

                                                                /* ------------- SST25 B-ONLY INSTRUCTIONS ------------ */
#define  FS_DEV_NOR_PHY_INSTR_AIIWP                     0xADu   /* Auto addr inc word-pgm.                              */
#define  FS_DEV_NOR_PHY_INSTR_EBSY                      0x70u   /* En RY/BY# out.                                       */
#define  FS_DEV_NOR_PHY_INSTR_DBSY                      0x80u   /* Disable RY/BY# out.                                  */

                                                                /* ------------- SST25 C-ONLY INSTRUCTIONS ------------ */
#define  FS_DEV_NOR_PHY_INSTR_PAGE_PGM                  0x02u   /* Page-pgm (auto addr inc).                            */
#define  FS_DEV_NOR_PHY_INSTR_FAST_RD_DUAL_IO           0xBBu   /* Fast-rd dual IO.                                     */
#define  FS_DEV_NOR_PHY_INSTR_FAST_RD_DUAL_OUT          0x3Bu   /* Fast-rd dual out.                                    */
#define  FS_DEV_NOR_PHY_INSTR_DUAL_IN_PAGE_PGM          0xA2u   /* Dual in page pgm.                                    */
#define  FS_DEV_NOR_PHY_INSTR_EN_HOLD_PIN               0xAAu   /* En hold pin fuctionality of the RST/HOLD pin.        */
#define  FS_DEV_NOR_PHY_INSTR_RD_SID                    0x88u   /* Rd security ID.                                      */
#define  FS_DEV_NOR_PHY_INSTR_PGM_SID                   0xA5u   /* Pgm user security ID.                                */
#define  FS_DEV_NOR_PHY_INSTR_LOCKOUT_SID               0x85u   /* Lock security ID pgm.                                */


/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_SR_BPL                      DEF_BIT_07  /* Blk protection lock-down.                            */
#define  FS_DEV_NOR_PHY_SR_AAI                      DEF_BIT_06  /* Auto addr inc pgm status.                            */
#define  FS_DEV_NOR_PHY_SR_BP3                      DEF_BIT_05  /* Blk protect bit 3.                                   */
#define  FS_DEV_NOR_PHY_SR_BP2                      DEF_BIT_04  /* Blk protect bit 2.                                   */
#define  FS_DEV_NOR_PHY_SR_BP1                      DEF_BIT_03  /* Blk protect bit 1.                                   */
#define  FS_DEV_NOR_PHY_SR_BP0                      DEF_BIT_02  /* Blk protect bit 0.                                   */
#define  FS_DEV_NOR_PHY_SR_WEL                      DEF_BIT_01  /* Wr enable latch.                                     */
#define  FS_DEV_NOR_PHY_SR_BUSY                     DEF_BIT_00  /* Busy.                                                */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_dev_nor_phy_desc {
    CPU_INT08U  DevID;
    CPU_INT32U  BlkCnt;
    CPU_INT32U  BlkSize;
    CPU_INT08U  Family;
} FS_DEV_NOR_PHY_DESC;


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

static  const  FS_DEV_NOR_PHY_DESC  FSDev_NOR_PHY_DevTbl[] = {
    {0x48u,      16u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25A},
    {0x01u,      16u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x49u,      32u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25A},
    {0x02u,      32u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x43u,      64u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25A},
    {0x03u,      64u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x44u,     128u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25A},
    {0x8Du,     128u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x04u,     128u,      4096u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x8Eu,      32u,     32768u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x05u,      32u,     32768u,     FS_DEV_NOR_PHY_FAMILY_SST25A},
    {0x41u,      64u,     32768u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x4Au,     128u,     32768u,     FS_DEV_NOR_PHY_FAMILY_SST25B},
    {0x4Bu,     128u,     65536u,     FS_DEV_NOR_PHY_FAMILY_SST25C},
    {   0u,       0u,         0u,     FS_DEV_NOR_PHY_FAMILY_NONE  }
};

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSDev_NOR_PHY_NameTbl[] = {
    "SST SST25VF512A",
    "SST SST25WF512B",
    "SST SST25VF010/VF010A",
    "SST SST25WF010B",
    "SST SST25VF020/VF020A/LF020A",
    "SST SST25WF020B",
    "SST SST25VF040A/LF040A",
    "SST SST25VF040B",
    "SST SST25WF040B",
    "SST SST25VF080B",
    "SST SST25WF080",
    "SST SST25VF016B",
    "SST SST25VF032B",
    "SST SST25VF064C",
    ""
};
#endif


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

#define  FS_DEV_NOR_PHY_CMD(p_phy_data, p_src, cnt)   {   FSDev_NOR_BSP_SPI.ChipSelEn((p_phy_data)->UnitNbr);         \
                                                                                                                      \
                                                          FSDev_NOR_BSP_SPI.Wr((p_phy_data)->UnitNbr,                 \
                                                                               (p_src),                               \
                                                                               (cnt));                                \
                                                                                                                      \
                                                          FSDev_NOR_BSP_SPI.ChipSelDis((p_phy_data)->UnitNbr);        }


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                                    /* ---- NOR PHY INTERFACE FNCTS --- */
static  void         FSDev_NOR_PHY_Open     (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Open NOR device.                 */
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_Close    (FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Close NOR device.                */

static  void         FSDev_NOR_PHY_Rd       (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Rd from NOR device.              */
                                             void                 *p_dest,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_Wr       (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Wr to NOR device.                */
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

static  void         FSDev_NOR_PHY_WrByte   (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Wr to NOR device.                */
                                             void                 *p_src,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_WrWord   (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Wr to NOR device.                */
                                             void                 *p_src,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  void         FSDev_NOR_PHY_WrBuf    (FS_DEV_NOR_PHY_DATA  *p_phy_data,      /* Wr to NOR device.                */
                                             void                 *p_src,
                                             CPU_INT32U            start,
                                             CPU_INT32U            cnt,
                                             FS_ERR               *p_err);

static  CPU_BOOLEAN  FSDev_NOR_PHY_WaitErase(FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Wait while dev erase.            */

static  CPU_BOOLEAN  FSDev_NOR_PHY_WaitWr   (FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Wait while dev wr.               */

static  CPU_BOOLEAN  FSDev_NOR_PHY_IsRdy    (FS_DEV_NOR_PHY_DATA  *p_phy_data);     /* Test busy & wr en flag.          */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_SST25 = {
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
*                   (e) 'MaxClkFreq' specifies the maximum SPI clock frequency.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Open (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                  FS_ERR               *p_err)
{
    CPU_INT08U   desc_nbr;
    CPU_BOOLEAN  found;
    CPU_INT08U   id[2];
    CPU_INT08U   id_manuf;
    CPU_INT16U   id_dev;
    CPU_INT08U   instr[4];
    CPU_BOOLEAN  ok;


    if (p_phy_data->AddrBase != 0u) {
        p_phy_data->AddrBase  = 0u;
        FS_TRACE_DBG(("NOR PHY SST25: \'AddrBase\' = 0x%08X != 0x00000000: Corrected.\r\n", p_phy_data->AddrBase));
    }

    if (p_phy_data->RegionNbr != 0u) {
        p_phy_data->RegionNbr  = 0u;
        FS_TRACE_DBG(("NOR PHY SST25: \'RegionNbr\' = %d != 0: Corrected.\r\n", p_phy_data->RegionNbr));
    }



                                                                /* ---------------------- INIT HW --------------------- */
    ok = FSDev_NOR_BSP_SPI.Open(p_phy_data->UnitNbr);
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return;
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);
    FS_OS_Dly_ms(10u);

    FSDev_NOR_BSP_SPI.SetClkFreq(p_phy_data->UnitNbr, p_phy_data->MaxClkFreq);

                                                                /* --------------- WR DIS (EXIT AAI WR) --------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WRDI;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 1u);   /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

                                                                /* --------------------- RD DEV ID -------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_RDID;
    instr[1] = 0x00u;
    instr[2] = 0x00u;
    instr[3] = 0x00u;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 4u);   /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &id[0], 2u);      /* Rd 2-byte ID.                                        */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ------------------ VALIDATE DEV ID ----------------- */
    id_manuf = id[0];
    id_dev   = id[1];

    if (id_manuf != FS_DEV_NOR_PHY_MANUF_ID_SST) {              /* Validate manuf ID.                                   */
        FS_TRACE_INFO(("NOR PHY SST25: Unrecognized manuf ID: 0x%02X != 0x%02X.\r\n", id_manuf, FS_DEV_NOR_PHY_MANUF_ID_SST));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    desc_nbr = 0u;                                              /* Lookup dev params in dev tbl.                        */
    found    = DEF_NO;
    while ((FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize != 0u) && (found == DEF_NO)) {
        if (FSDev_NOR_PHY_DevTbl[desc_nbr].DevID == id_dev) {
            found = DEF_YES;
        } else {
            desc_nbr++;
        }
    }

    if (found == DEF_NO) {
        FS_TRACE_INFO(("NOR PHY SST25: Unrecognized dev ID: 0x%02X.\r\n", id_dev));
       *p_err = FS_ERR_DEV_IO;
        return;
    }


                                                                /* -------------------- DIS WR PROT ------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_EWSR;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */


    instr[0] = FS_DEV_NOR_PHY_INSTR_WRSR;
    instr[1] = 0x00u;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 2u);              /* Wr cmd.                                              */



                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY SST25: Recognized device: Part nbr: %s\r\n", FSDev_NOR_PHY_NameTbl[desc_nbr]));
    FS_TRACE_INFO(("                                  Blk cnt : %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt));
    FS_TRACE_INFO(("                                  Blk size: %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize));
    FS_TRACE_INFO(("                                  ID      : 0x%02X 0x%02X\r\n", id_manuf, id_dev));

    p_phy_data->BlkCnt          =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt;
    p_phy_data->BlkSize         =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize;
    p_phy_data->AddrRegionStart =  0u;
    p_phy_data->DataPtr         = (void *)&FSDev_NOR_PHY_DevTbl[desc_nbr];          /* Save SST SST25 desc.             */
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
    FSDev_NOR_BSP_SPI.Close(p_phy_data->UnitNbr);
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
    CPU_INT08U  instr[5];


                                                                /* ---------------------- RD DEV ---------------------- */
    instr[0] =  FS_DEV_NOR_PHY_INSTR_FAST_RD;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[4] =  0xFFu;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);                   /* En chip sel.                                 */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 5u);           /* Wr cmd.                                      */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, p_dest, (CPU_SIZE_T)cnt); /* Rd data.                                     */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);                  /* Dis chip sel.                                */

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
    const  FS_DEV_NOR_PHY_DESC  *p_desc;
    CPU_BOOLEAN                  busy;


    p_desc = (const FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;


                                                                /* ---------------------- WR DATA --------------------- */
    if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_SST25A) {
        FSDev_NOR_PHY_WrByte(p_phy_data, p_src, start, cnt, p_err);
    } else if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_SST25B) {
        FSDev_NOR_PHY_WrWord(p_phy_data, p_src, start, cnt, p_err);
    } else {
        FSDev_NOR_PHY_WrBuf(p_phy_data, p_src, start, cnt, p_err);
    }


    busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                    /* Wait while wr completes ...                          */
    if (busy == DEF_YES) {                                      /* ... if dev still busy   ...                          */
       *p_err = FS_ERR_DEV_TIMEOUT;                             /* ... rtn timeout err.                                 */
        return;
    }
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
    const  FS_DEV_NOR_PHY_DESC  *p_desc;
    CPU_INT08U                   instr_erase;
    CPU_INT08U                   instr[4];
    CPU_BOOLEAN                  busy;


                                                                /* ----------------- VALIDATE BLK SIZE ---------------- */
    p_desc = (const FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;
    switch (size) {
        case 4096u:                                             /* Sector erase.                                        */
             instr_erase = FS_DEV_NOR_PHY_INSTR_SE;
             break;


        case 32768u:                                            /* 32-KB blk erase.                                     */
             instr_erase = FS_DEV_NOR_PHY_INSTR_BE32;
             break;


        case 65536u:                                            /* 64-KB blk erase (only B devs).                       */
             if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_SST25A) {
                *p_err = FS_ERR_DEV_INVALID_OP;
                 return;
             }
             instr_erase = FS_DEV_NOR_PHY_INSTR_BE64;
             break;


        default:                                                /* Invalid size.                                        */
            *p_err = FS_ERR_DEV_IO;
             return;
    }



                                                                /* ----------------------- EN WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */



                                                                /* --------------------- ERASE BLK -------------------- */
    instr[0] =   instr_erase;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 4u);              /* Wr cmd.                                              */

    busy = FSDev_NOR_PHY_WaitErase(p_phy_data);                 /* Wait while erasing.                                  */



                                                                /* ---------------------- DIS WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */

    if (busy == DEF_YES) {
       *p_err = FS_ERR_DEV_TIMEOUT;
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
    CPU_INT08U   instr;
    CPU_BOOLEAN  busy;


                                                                /* ----------------------- EN WR ---------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */



                                                                /* -------------------- ERASE CHIP -------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_CE;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */

    busy = FSDev_NOR_PHY_WaitErase(p_phy_data);                 /* Wait while erasing.                                  */



                                                                /* ---------------------- DIS WR ---------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */

    if (busy == DEF_YES) {
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_WrByte()
*
* Description : Write data to a NOR device from a buffer (using byte AAI).
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

static  void  FSDev_NOR_PHY_WrByte (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                    void                 *p_src,
                                    CPU_INT32U            start,
                                    CPU_INT32U            cnt,
                                    FS_ERR               *p_err)
{
    CPU_INT08U  *p_src_08;
    CPU_INT08U   instr[5];
    CPU_BOOLEAN  busy;
    CPU_INT08U   cmd;

                                                                /* ------------------- START AII PRM ------------------ */
    p_src_08 = (CPU_INT08U *)p_src;

                                                                /* Wr en.                                               */
    cmd = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

    instr[0] =   FS_DEV_NOR_PHY_INSTR_AIIP;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[4] =  *p_src_08;

    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 5u);              /* Wr cmd & first data byte.                            */

    busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                    /* Wait during wr        ...                            */
    if (busy == DEF_YES) {                                      /* ... if dev still busy ...                            */
       *p_err = FS_ERR_DEV_TIMEOUT;                             /* ... rtn timeout err.                                 */
        return;
    }

    p_src_08 += sizeof(CPU_INT08U);
    cnt      -= sizeof(CPU_INT08U);


                                                                /* ---------------------- WR DATA --------------------- */
    while (cnt > 0u) {
        instr[1]  = *p_src_08;

        FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 2u);          /* Wr cmd & data byte.                                  */

        busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                /* Wait during wr        ...                            */
        if (busy == DEF_YES) {                                  /* ... if dev still busy ...                            */
           *p_err = FS_ERR_DEV_TIMEOUT;                         /* ... rtn timeout err.                                 */
            return;
        }

        p_src_08 += sizeof(CPU_INT08U);
        cnt      -= sizeof(CPU_INT08U);
    }

                                                                /* Wr dis.                                              */
    cmd = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_WrWord()
*
* Description : Write data to a NOR device from a buffer (using word AAI).
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

static  void  FSDev_NOR_PHY_WrWord (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                    void                 *p_src,
                                    CPU_INT32U            start,
                                    CPU_INT32U            cnt,
                                    FS_ERR               *p_err)
{
    CPU_INT08U  *p_src_08;
    CPU_INT08U   instr[6];
    CPU_BOOLEAN  busy;
    CPU_INT08U   cmd;

                                                                /* ------------------- START AII PRM ------------------ */
    p_src_08 = (CPU_INT08U *)p_src;
    busy     =  DEF_NO;

                                                                /* Wr en.                                               */
    cmd = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

    if (cnt >= 2u) {
        instr[0]  =   FS_DEV_NOR_PHY_INSTR_AIIWP;
        instr[1]  = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[2]  = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[3]  = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[4]  = *p_src_08;
        p_src_08 +=  sizeof(CPU_INT08U);
        instr[5]  = *p_src_08;
        p_src_08 +=  sizeof(CPU_INT08U);
        cnt      -=  sizeof(CPU_INT16U);
        start    +=  sizeof(CPU_INT16U);

        FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 6u);          /* Wr cmd & first data word.                            */

        busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                /* Wait during wr        ...                            */
        if (busy == DEF_YES) {                                  /* ... if dev still busy ...                            */
           *p_err = FS_ERR_DEV_TIMEOUT;                         /* ... rtn timeout err.                                 */
            return;
        }



                                                                /* ---------------------- WR DATA --------------------- */
        while (cnt >= 2u) {
            instr[1]  = *p_src_08;
            p_src_08 +=  sizeof(CPU_INT08U);
            instr[2]  = *p_src_08;
            p_src_08 +=  sizeof(CPU_INT08U);
            cnt      -=  sizeof(CPU_INT16U);
            start    +=  sizeof(CPU_INT16U);

            FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 3u);      /* Wr cmd & data word.                                  */

            busy = FSDev_NOR_PHY_WaitWr(p_phy_data);            /* Wait during wr        ...                            */
            if (busy == DEF_YES) {                              /* ... if dev still busy ...                            */
               *p_err = FS_ERR_DEV_TIMEOUT;                     /* ... rtn timeout err.                                 */
                return;
            }
        }
    }



                                                                /* ------------------- WR LAST DATUM ------------------ */
    if (cnt > 0u) {
        instr[0] =   FS_DEV_NOR_PHY_INSTR_AIIP;
        instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[4] = *p_src_08;


        FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 5u)           /* Wr cmd & data word.                                  */

        busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                /* Wait during wr        ...                            */
        if (busy == DEF_YES) {                                  /* ... if dev still busy ...                            */
           *p_err = FS_ERR_DEV_TIMEOUT;                         /* ... rtn timeout err.                                 */
            return;
        }
    }

                                                                /* Wr dis.                                              */
    cmd = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_WrBuf()
*
* Description : Write data to a NOR device from a buffer (using page program).
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

static  void  FSDev_NOR_PHY_WrBuf (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                   void                 *p_src,
                                   CPU_INT32U            start,
                                   CPU_INT32U            cnt,
                                   FS_ERR               *p_err)
{
    CPU_INT32U   start_page_rem_space;
    CPU_INT32U   start_page_byte_nbr;
    CPU_INT32U   addr;
    CPU_INT08U  *p_src_08;
    CPU_INT08U   instr[4];
    CPU_INT08U   cmd;
    CPU_BOOLEAN  busy;


                                                                /* ------------------ START PGM PAGE ------------------ */
    addr     =  start;
    p_src_08 = (CPU_INT08U  *)p_src;
    busy     =  DEF_NO;

                                                                /* Determine nbr of bytes to be written in start page.  */
    start_page_rem_space = 256 - (addr & 0xFF);
    start_page_byte_nbr  = (cnt < start_page_rem_space) ? cnt : start_page_rem_space;

                                                                /* Wr en.                                               */
    cmd = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

                                                                /* Gather cmd & start addr.                             */
    instr[0]  =  FS_DEV_NOR_PHY_INSTR_PAGE_PGM;
    instr[1]  = (CPU_INT08U)((addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2]  = (CPU_INT08U)((addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3]  = (CPU_INT08U)((addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

                                                                /* Wr cmd & start addr.                                 */
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, instr, 4u);
                                                                /* --------------- WR FIRST PARTIAL PAGE -------------- */

                                                                /* Wr data to first page.                               */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, p_src_08, start_page_byte_nbr);
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);

    busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                    /* Wait during wr        ...                            */
    if (busy == DEF_YES) {                                      /* ... if dev still busy ...                            */
       *p_err = FS_ERR_DEV_TIMEOUT;                             /* ... rtn timeout err.                                 */
        return;
    }
                                                                /* Inc/dec cnt, addr & src ptr.                         */
    cnt      -= start_page_byte_nbr;
    p_src_08 += start_page_byte_nbr;
    addr     += start_page_byte_nbr;

                                                                /* -------------- WR FOLLOWING FULL PAGES ------------- */
    while (cnt >= 256){
                                                                /* Fmt addr.                                            */
        instr[1]  = (CPU_INT08U)((addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[2]  = (CPU_INT08U)((addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[3]  = (CPU_INT08U)((addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

                                                                /* Wr en.                                               */
        cmd = FS_DEV_NOR_PHY_INSTR_WREN;
        FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

                                                                /* Wr cmd & addr.                                       */
        FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);
        FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, instr, 4u);
                                                                /* Wr 256 bytes.                                        */
        FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, p_src_08, 256u);
        FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);

        busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                /* Wait during wr        ...                            */
        if (busy == DEF_YES) {                                  /* ... if dev still busy ...                            */
           *p_err = FS_ERR_DEV_TIMEOUT;                         /* ... rtn timeout err.                                 */
            return;
        }
                                                                /* Inc/dec cnt, addr & src ptr.                         */
        cnt      -= 256u;
        p_src_08 += 256u;
        addr     += 256u;
    }
                                                                /* --------------- WR FINAL PARTIAL PAGE -------------- */
    if (cnt > 0)
    {                                                           /* Fmt addr.                                            */
        instr[1]  = (CPU_INT08U)((addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[2]  = (CPU_INT08U)((addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        instr[3]  = (CPU_INT08U)((addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

                                                                /* Wr en.                                               */
        cmd = FS_DEV_NOR_PHY_INSTR_WREN;
        FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

                                                                /* Wr cmd & addr.                                       */
        FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);
        FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, instr, 4u);
                                                                /* Wr rem bytes.                                        */
        FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, p_src_08, cnt);
        FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);

        busy = FSDev_NOR_PHY_WaitWr(p_phy_data);                /* Wait during wr        ...                            */
        if (busy == DEF_YES) {                                  /* ... if dev still busy ...                            */
           *p_err = FS_ERR_DEV_TIMEOUT;                         /* ... rtn timeout err.                                 */
            return;
        }
    }

                                                                /* Wr dis.                                              */
    cmd = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_WaitWr()
*
* Description : Wait during NOR device write.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
* Return(s)   : DEF_YES, if timeout has occured.
*               DEF_NO,  if the device operation has finished.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_NOR_PHY_WaitWr (FS_DEV_NOR_PHY_DATA  *p_phy_data)
{
    CPU_INT08U   instr;
    CPU_BOOLEAN  busy;
    CPU_BOOLEAN  rdy;


    instr = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);               /* En chip sel.                                     */

    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr, 1u);          /* Wr cmd.                                          */

                                                                    /* Wait for wr completion.                          */
    rdy = FSDev_NOR_BSP_SPI_WaitWhileBusy(p_phy_data->UnitNbr,
                                          p_phy_data,
                                          FSDev_NOR_PHY_IsRdy,
                                          2000u);

    busy = (rdy == DEF_FAIL ? DEF_YES : DEF_NO);

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);              /* Dis chip sel.                                    */

    return (busy);
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_IsReady()
*
* Description : Polls busy & write enable flags to ensure write process is done.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
* Return(s)   : DEF_YES, if the device is rdy.
*               DEF_NO,  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_NOR_PHY_IsRdy(FS_DEV_NOR_PHY_DATA  *p_phy_data)
{
    CPU_INT08U   instr;
    CPU_INT08U   sr;
    CPU_BOOLEAN  rdy;
    CPU_BOOLEAN  busy;


    instr = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr, 1u);      /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                       */

    busy  =  DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_BUSY |        /* Chk busy & wr en flags.                              */
                               FS_DEV_NOR_PHY_SR_WEL);

    rdy   = (busy == DEF_NO ? DEF_YES : DEF_NO);

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    return (rdy);
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PHY_WaitErase()
*
* Description : Wait during NOR device block/chip erase.
*
* Argument(s) : p_phy_data  Pointer to NOR phy data.
*
* Return(s)   : DEF_YES, if the device is still busy.
*               DEF_NO,  if the device operation has finished.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_NOR_PHY_WaitErase (FS_DEV_NOR_PHY_DATA  *p_phy_data)
{
    CPU_INT08U   instr;
    CPU_INT08U   sr;
    CPU_INT32U   timeout;
    CPU_BOOLEAN  busy;


    instr = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);               /* En chip sel.                                     */

    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr, 1u);          /* Wr cmd.                                          */

    timeout = FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms;
    busy    = DEF_YES;
    while ((timeout > 0u) && (busy == DEF_YES)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                   */
        timeout--;
        busy = DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_WEL | FS_DEV_NOR_PHY_SR_BUSY);

        if (busy == DEF_YES) {
            FS_OS_Dly_ms(1u);
        }
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);              /* Dis chip sel.                                    */

    return (busy);
}
