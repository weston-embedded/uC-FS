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
*                             WINBOND W25Q SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_w25q.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) With an appropriate BSP, this device NOR PHY layer will support the following
*                devices:
*
*                    Winbond W25Q80BL/DL/DV
*                    Winbond W25Q16JV
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_W25Q_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs.h"
#include  "fs_dev_nor_w25q.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) The W25Q80 Page Program command time duration according to the datasheet is the following:
*
*               (a) W25Q80BL Page Program Time max duration is 0.8ms
*
*               (b) W25Q80DV/DL Page Program Time max duration is 3ms
*
*           (2) Fast Read(0Bh), Fast Read Dual Output(3Bh) and Fast Read Quadouput(6Bh) require to add
*               eight "Dummy" clocks after the 24-bit address is sent. The dummy clocks allow the
*               device's internal circuits additional timer for setting up the initial address. The input
*               data during the dummy clocks is "don't care". However, the IO pins should be high-impedance
*               prior to the falling edge of the first data out clock.
*********************************************************************************************************
*/

#define  FS_DEV_NOR_W25Q80                                0u
#define  FS_DEV_NOR_W25Q16                                1u

#define  FS_DEV_NOR_PHY_MANUF_ID_WINBOND               0xEFu    /* Manufacturer ID.                                     */

#define  FS_DEV_NOR_PHY_PAGE_SIZE                       256u

#define  FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms                   3u    /* See Note #1                                          */
#define  FS_DEV_NOR_PHY_MAX_64K_BLK_ERASE_ms           1000u
#define  FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms              6000u

#define  FS_DEV_NOR_W25Q_DUMMY_BYTE                    0xA5u    /* See Note #2                                          */

#define  FS_DEV_NOR_PHY_BLK_SIZE_32K             ( 32u * 1024u)
#define  FS_DEV_NOR_PHY_BLK_SIZE_64K             ( 64u * 1024u)
#define  FS_DEV_NOR_PHY_BLK_SIZE_128K            (128u * 1024u)

#define  FS_DEV_NOR_REG_RD_RETRY_MAX                   0xFFFFu

/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*********************************************************************************************************
*/
                                                                /* ------------------ READ COMMANDS ------------------- */
#define  FS_DEV_NOR_PHY_CMD_FAST_READ                   0x0Bu   /* Fast Read                                            */
                                                                /* ------------- PROGRAM & ERASE COMMANDS ------------- */
#define  FS_DEV_NOR_PHY_CMD_BLK_ERASE_32K               0x52u   /* Block Erase (32-KBytes).                             */
#define  FS_DEV_NOR_PHY_CMD_BLK_ERASE_64K               0xD8u   /* Block Erase (64-KBytes).                             */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE                  0xC7u   /* Chip erase cmd 2.                                    */
#define  FS_DEV_NOR_PHY_CMD_PAGE_PGM                    0x02u   /* Byte/Page program (1 - 256 bytes).                   */
                                                                /* ---------------- PROTECTION COMMANDS --------------- */
#define  FS_DEV_NOR_PHY_CMD_WRITE_EN                    0x06u   /* Write enable.                                        */
#define  FS_DEV_NOR_PHY_CMD_WRITE_DIS                   0x04u   /* Write disable.                                       */

                                                                /* ------------- STATUS REGISTER COMMANDS ------------- */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_READ             0x05u   /* Read status register.                                */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_WRITE            0x01u   /* Write status register byte 1.                        */
                                                                /* -------------- MISCELLANEOUS COMMANDS -------------- */
#define  FS_DEV_NOR_PHY_CMD_RD_JEDEC_ID                 0x9Fu   /* Manufacturer and Device ID Read.                     */

/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_SR_BUSY           DEF_BIT_00            /* Erase/Write in progress                              */
#define  FS_DEV_NOR_PHY_SR_WEL            DEF_BIT_01            /* Write enable latch status.                           */
#define  FS_DEV_NOR_PHY_SR_PROT_NONE_SWP  DEF_BIT_NONE          /* All sectors are soft. unprotected.                   */
#define  FS_DEV_NOR_PHY_SR_PROT_ALL_SWP   DEF_BIT_FIELD(3u, 2u) /* All sectors are soft. protected.                     */
#define  FS_DEV_NOR_PHY_SR_TB             DEF_BIT_05            /* TOP/BOTTOM protect status.                           */
#define  FS_DEV_NOR_PHY_SR_SEC            DEF_BIT_06            /* Sector protect status.                               */
#define  FS_DEV_NOR_PHY_SR_SRP            DEF_BIT_07            /* Status register protect status.                      */

                                                                /* -------------- AVAILABLE WITH W25Q16JV ------------- */
#define  FS_NOR_PHY_STATUS_REG3_DRV1      DEF_BIT_06            /* Output driver strength for read operations.          */
#define  FS_NOR_PHY_STATUS_REG3_DRV2      DEF_BIT_05            /* Output driver strength for read operations.          */
#define  FS_NOR_PHY_STATUS_REG3_DRV_100   DEF_BIT_NONE          /* Output driver strength is 100%                       */
#define  FS_NOR_PHY_STATUS_REG3_DRV_75    DEF_BIT_MASK(1u, 6u)  /* Output driver strength is  75%                       */
#define  FS_NOR_PHY_STATUS_REG3_DRV_50    DEF_BIT_MASK(2u, 6u)  /* Output driver strength is  50%                       */
#define  FS_NOR_PHY_STATUS_REG3_DRV_25    DEF_BIT_MASK(3u, 6u)  /* Output driver strength is  25%                       */
#define  FS_NOR_PHY_STATUS_REG3_WPS       DEF_BIT_02            /* Write protect selection.                             */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_dev_nor_phy_desc {
    CPU_INT16U  DevID;
    CPU_INT32U  BlkCnt;
    CPU_INT32U  BlkSize;
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
    {0x4014u,     16u, FS_DEV_NOR_PHY_BLK_SIZE_64K},            /* W25Q80BL/DV/DL                                       */
    {0x4015u,     32u, FS_DEV_NOR_PHY_BLK_SIZE_64K},            /* W25Q16JV                                             */
};

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSDev_NOR_PHY_NameTbl[] = {
    "Winbond W25Q80",
    "Winbond W25Q16",
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

#define  FS_DEV_NOR_PHY_CMD(p_phy_data, p_src, cnt)   {   FSDev_NOR_BSP_SPI.ChipSelEn((p_phy_data)->UnitNbr);           \
                                                                                                                        \
                                                          FSDev_NOR_BSP_SPI.Wr((FS_QTY    )(p_phy_data)->UnitNbr,       \
                                                                               (void     *)(p_src),                     \
                                                                               (CPU_SIZE_T)(cnt));                      \
                                                                                                                        \
                                                          FSDev_NOR_BSP_SPI.ChipSelDis((p_phy_data)->UnitNbr);          }

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

static  void  FSDev_NOR_PHY_WrEn     (FS_QTY                unit_nbr,       /* Write enable on NOR device.              */
                                      FS_ERR               *p_err);

static  void  FSDev_NOR_PHY_CmplWait (FS_QTY                unit_nbr,
                                      CPU_INT32U            dur_max,
                                      FS_ERR               *p_err);

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_W25Q = {
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
*                               FS_ERR_DEV_WR_PROT             Device is write protected.
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
*                   (e) (1) 'MaxClkFreq' specifies the maximum SPI clock frequency.
*
*
*                       (2) 'BusWidth', 'BusWidthMax' & 'PhyDevCnt' specify the bus configuration.
*                           'AddrBase' specifies the base address of the NOR flash memory.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Open (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                  FS_ERR               *p_err)
{
    CPU_INT08U   desc_nbr;
    CPU_BOOLEAN  found;
    CPU_INT08U   id[3u];
    CPU_INT08U   id_manuf;
    CPU_INT16U   id_dev;
    CPU_INT08U   cmd[3u];
    CPU_INT08U   sr;
    CPU_BOOLEAN  ok;

    if (p_phy_data->AddrBase != 0u) {
        p_phy_data->AddrBase  = 0u;
        FS_TRACE_DBG(("NOR PHY W25Q: \'AddrBase\' = 0x%08X != 0x00000000: Corrected.\r\n", p_phy_data->AddrBase));
    }

    if (p_phy_data->RegionNbr != 0u) {
        p_phy_data->RegionNbr  = 0u;
        FS_TRACE_DBG(("NOR PHY W25Q: \'RegionNbr\' = %d != 0: Corrected.\r\n", p_phy_data->RegionNbr));
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
    cmd[0] = FS_DEV_NOR_PHY_CMD_RD_JEDEC_ID;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &id[0], 3u);      /* Rd 3-byte JEDEC ID.                                  */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ------------------ VALIDATE DEV ID ----------------- */
    id_manuf = id[0];
    id_dev   = (id[1] << DEF_OCTET_NBR_BITS) + id[2];

    if (id_manuf != FS_DEV_NOR_PHY_MANUF_ID_WINBOND) {         /* Validate manuf ID.                                   */
        FS_TRACE_INFO(("NOR PHY W25Q: Unrecognized manuf ID: 0x%02X != 0x%02X.\r\n", id_manuf, FS_DEV_NOR_PHY_MANUF_ID_WINBOND));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    desc_nbr = 0u;                                              /* Lookup dev params in dev tbl.                        */
    found    = DEF_NO;
    for (desc_nbr = 0u;
         desc_nbr < sizeof(FSDev_NOR_PHY_DevTbl)/sizeof(*FSDev_NOR_PHY_DevTbl);
         desc_nbr++) {
        if (FSDev_NOR_PHY_DevTbl[desc_nbr].DevID == id_dev) {
            found = DEF_YES;
            break;
        }
    }

    if (found == DEF_NO) {
        FS_TRACE_INFO(("NOR PHY W25Q: Unrecognized dev ID: 0x%02X.\r\n", id_dev));
       *p_err = FS_ERR_DEV_IO;
        return;
    }



                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY W25Q: Recognized device:  Part nbr: %s\r\n", FSDev_NOR_PHY_NameTbl[desc_nbr]));
    FS_TRACE_INFO(("                                  Blk cnt : %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt));
    FS_TRACE_INFO(("                                  Blk size: %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize));
    FS_TRACE_INFO(("                                  Manuf ID : 0x%02X\r\n", id_manuf));
    FS_TRACE_INFO(("                                  Dev   ID : 0x%02X\r\n", id_dev));

    p_phy_data->BlkCnt          =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt;
    p_phy_data->BlkSize         =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize;
    p_phy_data->AddrRegionStart =  0u;
    p_phy_data->DataPtr         = (void *)&FSDev_NOR_PHY_DevTbl[desc_nbr];      /* Save GEN desc.                      */


                                                                /* --------------- GLOBAL SEC UNPROTECT --------------- */
                                                                /* ----------------------- EN WR ---------------------- */
    FSDev_NOR_PHY_WrEn(p_phy_data->UnitNbr, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* ----------------------- SR WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_WRITE;               /* Only non-volatile bits are affected.                 */
    cmd[1] = 0u;                                                /* Status register 1 value                              */
    cmd[2] = 0u;                                                /* Status register 2 value                              */
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], sizeof(cmd))        /* Wr cmd.                                              */

    FS_OS_Dly_ms(20u);                                          /* Device write status reg requires at least 10-15 ms   */
                                                                /* ---------------------- CHK SR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                       */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    if (DEF_BIT_IS_SET_ANY(sr, FS_DEV_NOR_PHY_SR_PROT_ALL_SWP)) {
        *p_err = FS_ERR_DEV_WR_PROT;                            /* Some/All Sectors are Soft protected.                 */
        return;
    }

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
* Note(s)     : (1) When using operational code FS_DEV_NOR_PHY_CMD_FAST_READ (0x0Bh), eight "dummy" clocks
*                   must be added after the Three Address bytes. The dummy clocks allow the device's
*                   internal circuits additional time for setting up the initial address. The input data
*                   during the dummy clocks is "don't care". However, the IO0 pin should be high-impedance
*                   prior to the falling edge of the first data out clock.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Rd (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                void                 *p_dest,
                                CPU_INT32U            start,
                                CPU_INT32U            cnt,
                                FS_ERR               *p_err)
{
    CPU_INT08U  cmd[5u];


                                                                /* ---------------------- RD DEV ---------------------- */
    cmd[0u] = FS_DEV_NOR_PHY_CMD_FAST_READ;
                                                                /* Cfg. 3-Byte address[A23-A0].Sending MSB first        */
    cmd[1u] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2u] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3u] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[4u] = FS_DEV_NOR_W25Q_DUMMY_BYTE;                       /* See Note #1.                                         */

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr((FS_QTY      )p_phy_data->UnitNbr,     /* Wr cmd.                                              */
                         (void       *)&cmd[0u],
                         (CPU_SIZE_T  )sizeof(cmd));
    FSDev_NOR_BSP_SPI.Rd((FS_QTY      )p_phy_data->UnitNbr,     /* Rd data.                                             */
                         (void       *)p_dest,
                         (CPU_SIZE_T  )cnt);
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

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
    CPU_INT08U   cmd_erase;
    CPU_INT08U   cmd[4u];
    CPU_INT32U   timeout;


    switch (size) {
        case FS_DEV_NOR_PHY_BLK_SIZE_64K:                       /* 64KBytes Block erase.                               */
             cmd_erase = FS_DEV_NOR_PHY_CMD_BLK_ERASE_64K;
             timeout   = FS_DEV_NOR_PHY_MAX_64K_BLK_ERASE_ms;
             break;

        default:                                                /* Invalid size.                                        */
            *p_err = FS_ERR_DEV_IO;
             return;
    }

                                                                /* ----------------------- EN WR ---------------------- */
    FSDev_NOR_PHY_WrEn(p_phy_data->UnitNbr, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* -------------------- ERASE BLOCK-------------------- */
    cmd[0] =  cmd_erase;
                                                                /* Cfg. 3-Byte address[A23-A0].Sending MSB first        */
    cmd[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], sizeof(cmd));       /* Wr cmd.                                              */


                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    FSDev_NOR_PHY_CmplWait(p_phy_data->UnitNbr, timeout, p_err);

    return;
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
    (void)p_data;

    switch (opt) {
        case FS_DEV_IO_CTRL_PHY_ERASE_CHIP:                     /* -------------------- ERASE CHIP -------------------- */
             FSDev_NOR_PHY_EraseChip(p_phy_data, p_err);
             break;


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
*                                     FSDev_NOR_PHY_EraseChip()
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
    CPU_INT08U  cmd;

                                                                /* ----------------------- EN WR ---------------------- */
    FSDev_NOR_PHY_WrEn(p_phy_data->UnitNbr, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* --------------------- ERASE CHIP ------------------- */
    cmd = FS_DEV_NOR_PHY_CMD_CHIP_ERASE;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);                   /* Wr cmd.                                              */

                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    FSDev_NOR_PHY_CmplWait(p_phy_data->UnitNbr,
                           FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms,
                           p_err);

    return;
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
    CPU_INT08U  cmd[4u];


                                                                /* ----------------------- EN WR ---------------------- */
    FSDev_NOR_PHY_WrEn(p_phy_data->UnitNbr, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

                                                                /* ---------------------- WR DEV ---------------------- */
    cmd[0u] = FS_DEV_NOR_PHY_CMD_PAGE_PGM;
                                                                /* Cfg. 3-Byte address[A23-A0].Sending MSB first        */
    cmd[1u] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2u] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3u] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr((FS_QTY    )p_phy_data->UnitNbr,       /* Wr cmd.                                              */
                         (void     *)&cmd[0u],
                         (CPU_SIZE_T)sizeof(cmd));
    FSDev_NOR_BSP_SPI.Wr((FS_QTY    )p_phy_data->UnitNbr,       /* Wr data.                                             */
                         (void     *)p_src,
                         (CPU_SIZE_T)cnt);
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */


                                                                /* ----------------- WAIT WHILE WR'ING ---------------- */
    FSDev_NOR_PHY_CmplWait(p_phy_data->UnitNbr,
                           FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms,
                           p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FSDev_NOR_PHY_WrEn()
*
* Description : Set Write Enable Latch on flash device (See Note#1)
*
* Argument(s) : unit_nbr    Pointer to NOR phy data.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Page written successfully.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Write Enable Latch must be set every time prior to executing these flash commands:
*                   Page Program(PP), Quad PP, Sector Erase, Block Erase(32k&64k), Chip Erase, and
*                   Write Status Register.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_WrEn (FS_QTY   unit_nbr,
                                  FS_ERR  *p_err)
{
    CPU_BOOLEAN  done;
    CPU_INT08U   reg_val;
    CPU_INT32U   retry;


    *p_err = FS_ERR_NONE;
                                                                /* Send WR EN cmd to flash dev.                         */
    reg_val = FS_DEV_NOR_PHY_CMD_WRITE_EN;
    FSDev_NOR_BSP_SPI.ChipSelEn(unit_nbr);
    FSDev_NOR_BSP_SPI.Wr(unit_nbr, (void *)&reg_val, 1u);
    FSDev_NOR_BSP_SPI.ChipSelDis(unit_nbr);

                                                                /* Test WEL bit to ensure write enable latch en in dev. */
    reg_val = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(unit_nbr);                      /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(unit_nbr, (void *)&reg_val, 1u);

    retry = FS_DEV_NOR_REG_RD_RETRY_MAX;
    done  = DEF_NO;
    while ((retry > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(unit_nbr, &reg_val, 1u);           /* Rd status reg.                                       */
        retry--;
        done = DEF_BIT_IS_SET(reg_val, FS_DEV_NOR_PHY_SR_WEL);
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(unit_nbr);                     /* Dis chip sel.                                        */

    if (done == DEF_NO) {
       *p_err = FS_ERR_DEV_TIMEOUT;
    }
}


/*
*********************************************************************************************************
*                                      FSDev_NOR_PHY_CmplWait()
*
* Description : Wait for write operation (program, erase or write status register) to complete.
*
* Argument(s) : unit_nbr    Pointer to NOR phy data.
*
*               dur_max     Maximum duration of operation MUST be in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE           Page written successfully.
*                               FS_ERR_DEV_TIMEOUT    Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_CmplWait (FS_QTY       unit_nbr,
                                      CPU_INT32U   dur_max,
                                      FS_ERR      *p_err)
{
    CPU_BOOLEAN  done;
    CPU_INT08U   reg_val;


    *p_err = FS_ERR_NONE;
                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    reg_val = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(unit_nbr);                      /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(unit_nbr, &reg_val, 1u);               /* Wr cmd.                                              */

    done    = DEF_NO;

    while ((done == DEF_NO) && (dur_max > 0)) {
        FSDev_NOR_BSP_SPI.Rd(unit_nbr, &reg_val, 1u);           /* Rd status reg.                                       */
        dur_max--;
        done = DEF_BIT_IS_CLR(reg_val, FS_DEV_NOR_PHY_SR_BUSY);

        if (done == DEF_NO) {
            FS_OS_Dly_ms(1u);
        }
    }
    FSDev_NOR_BSP_SPI.ChipSelDis(unit_nbr);                     /* Dis chip sel.                                        */

    if (done == DEF_NO) {
       *p_err = FS_ERR_DEV_TIMEOUT;
    }
}
