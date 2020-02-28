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
*                      ST MICROELECTRONICS M25 SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_stm25.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports Numonyx/ST's M25 & M45 serial NOR flash memories, as described in various
*                datasheets at Numonyx (http://www.numonyx.com).  This driver has been tested with
*                or should work with the following devices :
*
*                    M25P05-A [*]     M25PE10 [*]     M25PX80     M45PE10
*                    M25P10-A         M25PE20 [*]     M25PX16     M45PE20
*                    M25P20   [*]     M25PE40 [*]     M25PX32     M45PE40
*                    M25P40   [*]     M25PE80 [*]     M25PX64     M45PE80
*                    M25P80           M25PE16 [*]                 M45PE16
*                    M25P16   [*]
*                    M25P32   [*]
*                    M25P64   [*]
*                    M25P128
*
*                         [*} Devices tested
*
*            (2) STMicroelectronics offers several families of serial NOR flash memory, including :
*
*                (a) M25P-series  devices are programmed on a page (256 B) basis & erased on a sector
*                    (32 or 64 KB) basis.
*
*                (b) M25PE-series devices are programmed on a page (256 B) basis & erased on a page,
*                    subsector (4 KB) or sector (64 KB) basis.
*
*                (c) M25PX-series devices are programmed on a page (256 B) basis & erased on a
*                    subsector (4 KB) or sector (64 KB) basis.  These devices support dual I/O read
*                    & program operations.
*
*                (d) M45PE-series devices are programmed on a page (256 B) basis & erased on a sector
*                    (64 KB) basis.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_STM25_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs.h"
#include  "fs_dev_nor_stm25.h"


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

#define  FS_DEV_NOR_PHY_PAGE_SIZE                        256u
#define  FS_DEV_NOR_PHY_PAGE_MASK                       0xFFu

#define  FS_DEV_NOR_PHY_MANUF_ID_ST                     0x20u   /* Manufacturer ID.                                     */


                                                                /* ----------------- OPERATION TIMING ----------------- */
#define  FS_DEV_NOR_PHY_MAX_PAGE_ERASE_ms                 20u
#define  FS_DEV_NOR_PHY_MAX_SUBSECTOR_ERASE_ms           150u
#define  FS_DEV_NOR_PHY_MAX_SECTOR_ERASE_ms             5000u
#define  FS_DEV_NOR_PHY_MAX_BULK_ERASE_ms              10000u
#define  FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms                    5u

#define  FS_DEV_NOR_PHY_MAX_FREQ_NORMAL             25000000uL


                                                                /* ------------------- DEVICE FAMILY ------------------ */
#define  FS_DEV_NOR_PHY_FAMILY_NONE                        0u
#define  FS_DEV_NOR_PHY_FAMILY_M25P                        1u
#define  FS_DEV_NOR_PHY_FAMILY_M25PE                       2u
#define  FS_DEV_NOR_PHY_FAMILY_M45PE                       3u
#define  FS_DEV_NOR_PHY_FAMILY_M25PX                       4u

/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*********************************************************************************************************
*/
                                                                /*                                M25P M25PE M45PE M25PX*/
#define  FS_DEV_NOR_PHY_INSTR_WREN                      0x06u   /* Write enable.                    Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_WRDI                      0x04u   /* Write disable.                   Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_RDID                      0x9Fu   /* Read identification.             Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_RDSR                      0x05u   /* Read status register.            Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_WRSR                      0x01u   /* Write status register.           Y     Y     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_RDLR                      0xE8u   /* Read lock register.              -     Y     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_WRLR                      0xE5u   /* Write lock register.             -     Y     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_ROTP                      0x4Bu   /* Read OTP.                        -     -     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_POTP                      0x42u   /* Program OTP.                     -     -     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_READ                      0x03u   /* Read data bytes.                 Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_FAST_READ                 0x0Bu   /* Read data bytes at higher speed. Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_DOFR                      0x3Bu   /* Dual output fast read.           -     -     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_PW                        0x0Au   /* Page write.                      -     Y     Y     - */
#define  FS_DEV_NOR_PHY_INSTR_PP                        0x02u   /* Page program.                    Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_DIFP                      0xA2u   /* Dual input fast program.         -     -     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_PE                        0xDBu   /* Page erase.                      -     Y     Y     - */
#define  FS_DEV_NOR_PHY_INSTR_SSE                       0x20u   /* Subsector erase.                 -     Y     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_SE                        0xD8u   /* Sector erase.                    Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_BE                        0xC7u   /* Bulk erase.                      Y     Y     -     Y */
#define  FS_DEV_NOR_PHY_INSTR_RES                       0xABu   /* Read electronic signature.       Y     -     -     - */
#define  FS_DEV_NOR_PHY_INSTR_DP                        0xB9u   /* Deep power-down.                 Y     Y     Y     Y */
#define  FS_DEV_NOR_PHY_INSTR_RDP                       0xABu   /* Release from deep power-down.    Y     Y     Y     Y */
                                                                /*                                M25P M25PE M45PE M25PX*/

/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_SR_SRWD                     DEF_BIT_07  /* Status register write disable.                       */
#define  FS_DEV_NOR_PHY_SR_TB                       DEF_BIT_05  /* Top/bottom.                                          */
#define  FS_DEV_NOR_PHY_SR_BP2                      DEF_BIT_04  /* Block protect bit 2.                                 */
#define  FS_DEV_NOR_PHY_SR_BP1                      DEF_BIT_03  /* Block protect bit 1.                                 */
#define  FS_DEV_NOR_PHY_SR_BP0                      DEF_BIT_02  /* Block protect bit 0.                                 */
#define  FS_DEV_NOR_PHY_SR_WEL                      DEF_BIT_01  /* Write enable latch.                                  */
#define  FS_DEV_NOR_PHY_SR_WIP                      DEF_BIT_00  /* Write in progress.                                   */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_dev_nor_phy_desc {
    CPU_INT16U  DevID;
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
    {0x2010u,      2u,    32768u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2011u,      4u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2012u,      4u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2013u,      8u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2014u,     16u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2015u,     32u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2016u,     64u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2017u,    128u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x2018u,     64u,   262144u,     FS_DEV_NOR_PHY_FAMILY_M25P },
    {0x8011u,      2u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PE},
    {0x8012u,      4u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PE},
    {0x8013u,      8u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PE},
    {0x8014u,     16u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PE},
    {0x8015u,     32u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PE},
    {0x7114u,     16u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PX},
    {0x7115u,     32u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PX},
    {0x7116u,     64u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PX},
    {0x7117u,    128u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M25PX},
    {0x4011u,      2u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M45PE},
    {0x4012u,      4u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M45PE},
    {0x4013u,      8u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M45PE},
    {0x4014u,     16u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M45PE},
    {0x4015u,     32u,    65536u,     FS_DEV_NOR_PHY_FAMILY_M45PE},
    {     0u,      0u,        0u,     FS_DEV_NOR_PHY_FAMILY_NONE }
};

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSDev_NOR_PHY_NameTbl[] = {
    "ST M25P05-A",
    "ST M25P10-A",
    "ST M25P20",
    "ST M25P40",
    "ST M25P80",
    "ST M25P16",
    "ST M25P32",
    "ST M25P64",
    "ST M25P128",
    "ST M25PE10",
    "ST M25PE20",
    "ST M25PE40",
    "ST M25PE80",
    "ST M25PE16",
    "ST M25PX80",
    "ST M25PX16",
    "ST M25PX32",
    "ST M25PX64",
    "ST M45PE10",
    "ST M45PE20",
    "ST M45PE40",
    "ST M45PE80",
    "ST M45PE16",
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

                                                                            /* -------- NOR PHY INTERFACE FNCTS ------- */
static  void  FSDev_NOR_PHY_Open     (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Open NOR device.                         */
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_Close    (FS_DEV_NOR_PHY_DATA  *p_phy_data);    /* Close NOR device.                        */

static  void  FSDev_NOR_PHY_Rd       (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Read from NOR device.                    */
                                      void                 *p_dest,
                                      CPU_INT32U            start,
                                      CPU_INT32U            cnt,
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_Wr       (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Write to NOR device.                     */
                                      void                 *p_src,
                                      CPU_INT32U            start,
                                      CPU_INT32U            cnt,
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_EraseBlk (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Erase block on NOR device.               */
                                      CPU_INT32U            start,
                                      CPU_INT32U            size,
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_IO_Ctrl  (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Perform NOR device I/O control.          */
                                      CPU_INT08U            opt,
                                      void                 *p_data,
                                      FS_ERR               *p_err);


                                                                            /* -------------- LOCAL FNCTS ------------- */
static  void  FSDev_NOR_PHY_EraseChip(FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Erase NOR device.                        */
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_WrPage   (FS_DEV_NOR_PHY_DATA  *p_phy_data,     /* Write page on NOR device.                */
                                      void                 *p_src,
                                      CPU_INT32U            start,
                                      CPU_INT32U            cnt,
                                      FS_ERR               *p_err);

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_STM25 = {
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
    CPU_INT08U   id[3];
    CPU_INT08U   id_manuf;
    CPU_INT16U   id_dev;
    CPU_INT08U   instr;
    CPU_BOOLEAN  ok;


    if (p_phy_data->AddrBase != 0u) {
        p_phy_data->AddrBase  = 0u;
        FS_TRACE_DBG(("NOR PHY STM25: \'AddrBase\' = 0x%08X != 0x00000000: Corrected.\r\n", p_phy_data->AddrBase));
    }

    if (p_phy_data->RegionNbr != 0u) {
        p_phy_data->RegionNbr  = 0u;
        FS_TRACE_DBG(("NOR PHY STM25: \'RegionNbr\' = %d != 0: Corrected.\r\n", p_phy_data->RegionNbr));
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



                                                                /* --------------------- RD DEV ID -------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_RDID;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);               /* En chip sel.                                     */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr, 1u);          /* Wr cmd.                                          */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &id[0], 3u);          /* Rd 3-byte ID.                                    */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);              /* Dis chip sel.                                    */



                                                                /* ------------------ VALIDATE DEV ID ----------------- */
    id_manuf =  id[0];
    id_dev   = ((CPU_INT16U)id[1] << DEF_OCTET_NBR_BITS) | (CPU_INT16U)id[2];

    if (id_manuf != FS_DEV_NOR_PHY_MANUF_ID_ST) {               /* Validate manuf ID.                                   */
        FS_TRACE_INFO(("NOR PHY STM25: Unrecognized manuf ID: 0x%02X != 0x%02X.\r\n", id_manuf, FS_DEV_NOR_PHY_MANUF_ID_ST));
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
        FS_TRACE_INFO(("NOR PHY STM25: Unrecognized dev ID: 0x%04X.\r\n", id_dev));
       *p_err = FS_ERR_DEV_IO;
        return;
    }



                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY STM25: Recognized device: Part nbr: %s\r\n", FSDev_NOR_PHY_NameTbl[desc_nbr]));
    FS_TRACE_INFO(("                                  Blk cnt : %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt));
    FS_TRACE_INFO(("                                  Blk size: %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize));
    FS_TRACE_INFO(("                                  ID      : 0x%02X 0x%04X\r\n", id_manuf, id_dev));

    p_phy_data->BlkCnt          =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt;
    p_phy_data->BlkSize         =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize;
    p_phy_data->AddrRegionStart =  0u;
    p_phy_data->DataPtr         = (void *)&FSDev_NOR_PHY_DevTbl[desc_nbr];      /* Save ST M25 desc.                    */
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
    instr[0] =  FS_DEV_NOR_PHY_INSTR_FAST_READ;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[4] =  0xFFu;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);                               /* En chip sel.                     */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 5u);                       /* Wr cmd.                          */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, p_dest, (CPU_SIZE_T)cnt);             /* Rd data.                         */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);                              /* Dis chip sel.                    */

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
    CPU_INT32U   start_mod;
    CPU_INT32U   cnt_wr;
    CPU_INT08U  *p_src_08;


                                                                /* --------------- WR INIT PARTIAL PAGE --------------- */
    p_src_08  = (CPU_INT08U *)p_src;
    start_mod =  start % FS_DEV_NOR_PHY_PAGE_SIZE;
    if (start_mod > 0u) {
        cnt_wr = DEF_MIN(FS_DEV_NOR_PHY_PAGE_SIZE - start_mod, cnt);
        FSDev_NOR_PHY_WrPage(p_phy_data,
                             p_src_08,
                             start,
                             cnt_wr,
                             p_err);

        if (*p_err != FS_ERR_NONE) {
            return;
        }

        p_src_08 += cnt_wr;
        cnt      -= cnt_wr;
        start    += cnt_wr;
    }

                                                                /* -------------- WR FULL/PARTIAL PAGES --------------- */
    while (cnt > 0u) {
        cnt_wr = DEF_MIN(FS_DEV_NOR_PHY_PAGE_SIZE, cnt);
        FSDev_NOR_PHY_WrPage(p_phy_data,
                             p_src_08,
                             start,
                             cnt_wr,
                             p_err);

        if (*p_err != FS_ERR_NONE) {
            return;
        }

        p_src_08 += cnt_wr;
        cnt      -= cnt_wr;
        start    += cnt_wr;
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
    const  FS_DEV_NOR_PHY_DESC  *p_desc;
    CPU_INT08U                   instr_erase;
    CPU_INT08U                   instr[4];
    CPU_INT08U                   sr;
    CPU_INT32U                   timeout;
    CPU_BOOLEAN                  done;


                                                                /* ----------------- VALIDATE BLK SIZE ---------------- */
    p_desc = (const FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;

    if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_M25P) {
        if (size != p_desc->BlkSize) {
           *p_err = FS_ERR_DEV_INVALID_OP;
            return;
        }
        instr_erase = FS_DEV_NOR_PHY_INSTR_SE;
        timeout     = FS_DEV_NOR_PHY_MAX_SECTOR_ERASE_ms * 2u;

    } else {
        switch (size) {
            case 256u:                                          /* Page erase.                                          */
                 if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_M25PX) {
                    *p_err = FS_ERR_DEV_INVALID_OP;
                     return;
                 }
                 instr_erase = FS_DEV_NOR_PHY_INSTR_PE;
                 timeout     = FS_DEV_NOR_PHY_MAX_PAGE_ERASE_ms * 2u;
                 break;


            case 4096u:                                         /* Subsector erase.                                     */
                 if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_M45PE) {
                    *p_err = FS_ERR_DEV_INVALID_OP;
                     return;
                 }
                 instr_erase = FS_DEV_NOR_PHY_INSTR_SSE;
                 timeout     = FS_DEV_NOR_PHY_MAX_SUBSECTOR_ERASE_ms * 2u;
                 break;


            case 65536u:                                        /* Sector erase.                                        */
                 instr_erase = FS_DEV_NOR_PHY_INSTR_SE;
                 timeout     = FS_DEV_NOR_PHY_MAX_SECTOR_ERASE_ms * 2u;
                 break;


            default:                                            /* Invalid size.                                        */
                *p_err = FS_ERR_DEV_IO;
                 return;
        }
    }



                                                                /* ----------------------- EN WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */



                                                                /* --------------------- ERASE SEC -------------------- */
    instr[0] =   instr_erase;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 4u);              /* Wr cmd.                                              */



                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 1u);   /* Wr cmd.                                              */



    done = DEF_NO;
    while ((timeout > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */

        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_WIP);

        if (done == DEF_NO) {
            FS_OS_Dly_ms(1u);
        }
    }
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);                  /* Dis chip sel.                                */



                                                                /* ---------------------- DIS WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */

    if (done == DEF_NO) {
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
    const  FS_DEV_NOR_PHY_DESC  *p_desc;
    CPU_INT08U                   instr;
    CPU_INT08U                   sr;
    CPU_INT32U                   timeout;
    CPU_BOOLEAN                  done;


                                                                /* ------------------- VALIDATE DEV ------------------- */
    p_desc = (const FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;
    if (p_desc->Family == FS_DEV_NOR_PHY_FAMILY_M45PE) {
       *p_err = FS_ERR_DEV_INVALID_OP;
        return;
    }



                                                                /* ----------------------- EN WR ---------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */



                                                                /* --------------------- ERASE SEC -------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_BE;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */



                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    instr = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr, 1u);      /* Wr cmd.                                              */



    timeout = FS_DEV_NOR_PHY_MAX_BULK_ERASE_ms * 2u;
    done    = DEF_NO;
    while ((timeout > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */
        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_WIP);

        if (done == DEF_NO) {
            FS_OS_Dly_ms(1u);
        }
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);                  /* Dis chip sel.                                */



                                                                /* ---------------------- DIS WR ---------------------- */
    instr = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr, 1u);                 /* Wr cmd.                                              */

    if (done == DEF_NO) {
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       FSDev_NOR_PHY_WrPage()
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
*                               FS_ERR_NONE           Page written successfully.
*                               FS_ERR_DEV_IO         Device I/O error.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_WrPage (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                    void                 *p_src,
                                    CPU_INT32U            start,
                                    CPU_INT32U            cnt,
                                    FS_ERR               *p_err)
{
    CPU_INT08U   instr[4];
    CPU_INT08U   sr;
    CPU_INT32U   timeout;
    CPU_BOOLEAN  done;



                                                                /* ----------------------- EN WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WREN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */



                                                                /* -------------------- CHK SR WEL -------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 1u);   /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                       */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ---------------------- WR DEV ---------------------- */
    instr[0] =  FS_DEV_NOR_PHY_INSTR_PP;
    instr[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    instr[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 4u);   /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr,  p_src, cnt);     /* Wr data.                                             */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ----------------- WAIT WHILE WR'ING ---------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_RDSR;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */

    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &instr[0], 1u);   /* Wr cmd.                                              */


    timeout = FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms * 1000u;
    done    = DEF_NO;
    while ((timeout > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */
        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_WIP);
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ---------------------- DIS WR ---------------------- */
    instr[0] = FS_DEV_NOR_PHY_INSTR_WRDI;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &instr[0], 1u);              /* Wr cmd.                                              */


    if (done == DEF_NO) {
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

   *p_err = FS_ERR_NONE;
}
