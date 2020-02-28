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
*                             ATMEL AT25 SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_at25.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_AT25_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs.h"
#include  "fs_dev_nor_at25.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_PAGE_SIZE                        256u

#define  FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms                   3u
#define  FS_DEV_NOR_PHY_MAX_4K_BLK_ERASE_ms             200u
#define  FS_DEV_NOR_PHY_MAX_32K_BLK_ERASE_ms            600u
#define  FS_DEV_NOR_PHY_MAX_64K_BLK_ERASE_ms            950u
#define  FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms             40000u


#define  FS_DEV_NOR_PHY_MANUF_ID_ATMEL                 0x1Fu   /* Manufacturer ID.                                     */

/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*********************************************************************************************************
*/


                                                                /* ------------------ READ COMMANDS ------------------ */
#define  FS_DEV_NOR_PHY_CMD_ARRAY_HI_READ               0x1Bu   /* Read Array (High Frequency, Up to 100MHz).           */
#define  FS_DEV_NOR_PHY_CMD_ARRAY_READ                  0x0Bu   /* Read Array (Up to 85MHz).                            */
#define  FS_DEV_NOR_PHY_CMD_ARRAY_LO_READ               0x03u   /* Read Array (Low Frequency, Up to 50MHz).             */

                                                                /* ------------- PROGRAM & ERASE COMMANDS ------------- */
#define  FS_DEV_NOR_PHY_CMD_4K_BLOCK_ERASE              0x20u   /* Block Erase (4-KBytes).                              */
#define  FS_DEV_NOR_PHY_CMD_32K_BLOCK_ERASE             0x52u   /* Block Erase (32-KBytes).                             */
#define  FS_DEV_NOR_PHY_CMD_64K_BLOCK_ERASE             0xD8u   /* Block Erase (64-KBytes).                             */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_01               0x60u   /* Chip erase cmd 1.                                    */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_02               0xC7u   /* Chip erase cmd 2.                                    */
#define  FS_DEV_NOR_PHY_CMD_BYTE_PAGE_PGM               0x02u   /* Byte/Page program (1 - 256 bytes).                   */
#define  FS_DEV_NOR_PHY_CMD_PGM_ERASE_SUSPEND           0xB0u   /* Program/erase suspend.                               */
#define  FS_DEV_NOR_PHY_CMD_PGM_ERASE_RESUME            0xD0u   /* Program/erase resume.                                */

                                                                /* ---------------- PROTECTION COMMANDS --------------- */
#define  FS_DEV_NOR_PHY_CMD_WRITE_EN                    0x06u   /* Write enable.                                        */
#define  FS_DEV_NOR_PHY_CMD_WRITE_DIS                   0x04u   /* Write disable.                                       */
#define  FS_DEV_NOR_PHY_CMD_SEC_PROTECT                 0x36u   /* Protect sector.                                      */
#define  FS_DEV_NOR_PHY_CMD_SEC_UNPROTECT               0x39u   /* Unprotect sector.                                    */
#define  FS_DEV_NOR_PHY_CMD_SEC_PROTECT_REG_READ        0x3Cu   /* Read sector protection registers.                    */

                                                                /* ------------- STATUS REGISTER COMMANDS ------------- */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_READ             0x05u   /* Read status register.                                */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_BYTE_01_WRITE    0x01u   /* Write status register byte 1.                        */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_BYTE_02_WRITE    0x31u   /* Write status register byte 2.                        */
                                                                /* -------------- MISCELLANEOUS COMMANDS -------------- */
#define  FS_DEV_NOR_PHY_CMD_RESET                       0xF0u   /* Reset.                                               */
#define  FS_DEV_NOR_PHY_CMD_MANUF_DEV_ID_READ           0x9Fu   /* Manufacturer and Device ID Read.                     */
#define  FS_DEV_NOR_PHY_CMD_DEEP_PWR_DOWN               0xB9u   /* Deep Power-down.                                     */
#define  FS_DEV_NOR_PHY_CMD_RESUME_DEEP_PWR_DOWN        0xABu   /* Resume from Deep Power-down                          */

/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_SR_RDY_BSY                DEF_BIT_00            /* Ready/Busy status.                                  */
#define  FS_DEV_NOR_PHY_SR_WEL                    DEF_BIT_01            /* Write enable latch status.                          */
#define  FS_DEV_NOR_PHY_SR_EPE                    DEF_BIT_05            /* Erase/program error.                                */
#define  FS_DEV_NOR_PHY_SR_PROT_NONE_SWP          DEF_BIT_NONE          /* All sectors are soft. unprotected.                  */
#define  FS_DEV_NOR_PHY_SR_PROT_SOME_SWP          DEF_BIT_FIELD(1u, 2u) /* Some sectors are soft. protected.                   */
#define  FS_DEV_NOR_PHY_SR_PROT_ALL_SWP           DEF_BIT_FIELD(3u, 2u) /* All sectors are soft. protected.                    */

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
    {  0x45u,     16u, 64u * 1024u},
    {  0x46u,     32u, 64u * 1024u},
    {  0x47u,     64u, 64u * 1024u},
    {     0u,      0u,          0u}
};

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSDev_NOR_PHY_NameTbl[] = {
    "Atmel AT25DF081",
    "Atmel AT25DF161",
    "Atmel AT25DF321",
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

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_AT25 = {
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
    CPU_INT08U   id[4];
    CPU_INT08U   id_manuf;
    CPU_INT08U   id_dev;
    CPU_INT08U   cmd[2];
    CPU_INT08U   sr;
    CPU_BOOLEAN  ok;

    if (p_phy_data->AddrBase != 0u) {
        p_phy_data->AddrBase  = 0u;
        FS_TRACE_DBG(("NOR PHY AT25: \'AddrBase\' = 0x%08X != 0x00000000: Corrected.\r\n", p_phy_data->AddrBase));
    }

    if (p_phy_data->RegionNbr != 0u) {
        p_phy_data->RegionNbr  = 0u;
        FS_TRACE_DBG(("NOR PHY AT25: \'RegionNbr\' = %d != 0: Corrected.\r\n", p_phy_data->RegionNbr));
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
    cmd[0] = FS_DEV_NOR_PHY_CMD_MANUF_DEV_ID_READ;

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &id[0], 4u);      /* Rd 4-byte ID.                                        */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ------------------ VALIDATE DEV ID ----------------- */
    id_manuf = id[0];
    id_dev   = id[1];

    if (id_manuf != FS_DEV_NOR_PHY_MANUF_ID_ATMEL) {            /* Validate manuf ID.                                   */
        FS_TRACE_INFO(("NOR PHY AT25: Unrecognized manuf ID: 0x%02X != 0x%02X.\r\n", id_manuf, FS_DEV_NOR_PHY_MANUF_ID_ATMEL));
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
        FS_TRACE_INFO(("NOR PHY AT25: Unrecognized dev ID: 0x%02X.\r\n", id_dev));
       *p_err = FS_ERR_DEV_IO;
        return;
    }



                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY AT25: Recognized device: Part nbr: %s\r\n", FSDev_NOR_PHY_NameTbl[desc_nbr]));
    FS_TRACE_INFO(("                                 Blk cnt : %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt));
    FS_TRACE_INFO(("                                 Blk size: %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize));
    FS_TRACE_INFO(("                                 Manuf ID : 0x%02X\r\n", id_manuf));
    FS_TRACE_INFO(("                                 Dev   ID : 0x%02X\r\n", id_dev));

    p_phy_data->BlkCnt          =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkCnt;
    p_phy_data->BlkSize         =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize;
    p_phy_data->AddrRegionStart =  0u;
    p_phy_data->DataPtr         = (void *)&FSDev_NOR_PHY_DevTbl[desc_nbr];      /* Save AT25 desc.                      */


                                                                /* --------------- GLOBAL SEC UNPROTECT --------------- */
                                                                /* ----------------------- EN WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_WRITE_EN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], 1u);                /* Wr cmd.                                              */

                                                                /* ----------------------- SR WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_BYTE_01_WRITE;
    cmd[1] = 0u;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], sizeof(cmd))        /* Wr cmd.                                              */

                                                                /* ---------------------- CHK SR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                       */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    if (DEF_BIT_IS_SET_ANY(sr, FS_DEV_NOR_PHY_SR_PROT_ALL_SWP)) {
        *p_err = FS_ERR_DEV_IO;                                 /* Some/All Sectors are Soft protected.                 */
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
* Note(s)     : (1) When using operational code FS_DEV_NOR_PHY_CMD_ARRAY_READ (0x0Bh), a single dummy byte
*                   must be clocked in to the device after the Three Address bytes.
*
*********************************************************************************************************
*/

static  void  FSDev_NOR_PHY_Rd (FS_DEV_NOR_PHY_DATA  *p_phy_data,
                                void                 *p_dest,
                                CPU_INT32U            start,
                                CPU_INT32U            cnt,
                                FS_ERR               *p_err)
{
    CPU_INT08U  cmd[4];
    CPU_INT08U  dont_care[1];


                                                                /* ---------------------- RD DEV ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_ARRAY_READ;
    cmd[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr((FS_QTY      )p_phy_data->UnitNbr,     /* Wr cmd.                                              */
                         (void       *)&cmd[0],
                         (CPU_SIZE_T  )sizeof(cmd));
    FSDev_NOR_BSP_SPI.Wr((FS_QTY      )p_phy_data->UnitNbr,     /* See Note #1.                                         */
                         (void       *)&dont_care[0],
                         (CPU_SIZE_T  )sizeof(dont_care));
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
    CPU_INT08U    cmd_erase;
    CPU_INT08U    cmd[4];
    CPU_INT08U    sr;
    CPU_BOOLEAN   done;
    CPU_INT32U    timeout;


    switch (size) {
        case 4096u:                                             /* 4KBytes Block erase.                                 */
             cmd_erase = FS_DEV_NOR_PHY_CMD_4K_BLOCK_ERASE;
             timeout   = FS_DEV_NOR_PHY_MAX_4K_BLK_ERASE_ms * 2u;
             break;


        case 32768u:                                            /* 32KBytes Block erase.                                */
             cmd_erase = FS_DEV_NOR_PHY_CMD_32K_BLOCK_ERASE;
             timeout   = FS_DEV_NOR_PHY_MAX_32K_BLK_ERASE_ms * 2u;
             break;


        case 65536u:                                            /* 64KBytes Block erase.                                */
             cmd_erase = FS_DEV_NOR_PHY_CMD_64K_BLOCK_ERASE;
             timeout   = FS_DEV_NOR_PHY_MAX_64K_BLK_ERASE_ms * 2u;
             break;


        default:                                                /* Invalid size.                                        */
            *p_err = FS_ERR_DEV_IO;
             return;
    }

                                                                /* ----------------------- EN WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_WRITE_EN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], 1u);                /* Wr cmd.                                              */

                                                                /* -------------------- ERASE BLOCK-------------------- */
    cmd[0] =  cmd_erase;
    cmd[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], sizeof(cmd));       /* Wr cmd.                                              */


                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */


    done = DEF_NO;
    while (done == DEF_NO) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */
        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_RDY_BSY);

        if (done == DEF_NO) {
            FS_OS_Dly_ms(1u);
        }
    }
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ---------------------- DIS WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_WRITE_DIS;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], 1u);                /* Wr cmd.                                              */

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
    CPU_INT08U   cmd;
    CPU_INT08U   sr;
    CPU_BOOLEAN  done;
    CPU_INT32U   timeout;


                                                                /* ----------------------- EN WR ---------------------- */
    cmd = FS_DEV_NOR_PHY_CMD_WRITE_EN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);                   /* Wr cmd.                                              */

                                                                /* --------------------- ERASE CHIP ------------------- */
    cmd = FS_DEV_NOR_PHY_CMD_CHIP_ERASE_01;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);                   /* Wr cmd.                                              */

                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    cmd = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd, 1u);        /* Wr cmd.                                              */

    done    = DEF_NO;
    timeout = FS_DEV_NOR_PHY_MAX_CHIP_ERASE_ms * 2u;
    while ((timeout > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */
        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_RDY_BSY);

        if (done == DEF_NO) {
            FS_OS_Dly_ms(1u);
        }
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                       */



                                                                /* ---------------------- DIS WR ---------------------- */
    cmd = FS_DEV_NOR_PHY_CMD_WRITE_DIS;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd, 1u);                   /* Wr cmd.                                              */

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
    CPU_INT08U   cmd[4];
    CPU_INT08U   sr;
    CPU_BOOLEAN  done;
    CPU_INT32U   timeout;


                                                                /* ----------------------- EN WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_WRITE_EN;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], 1u);                /* Wr cmd.                                              */

                                                                /* -------------------- CHK SR WEL -------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);         /* Rd status reg.                                       */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    if (DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_WEL)) {
       *p_err = FS_ERR_DEV_IO;
        return;
    }

                                                                /* ---------------------- WR DEV ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_BYTE_PAGE_PGM;
    cmd[1] = (CPU_INT08U)((start >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[2] = (CPU_INT08U)((start >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
    cmd[3] = (CPU_INT08U)((start >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr((FS_QTY    )p_phy_data->UnitNbr,       /* Wr cmd.                                              */
                         (void     *)&cmd[0],
                         (CPU_SIZE_T)sizeof(cmd));
    FSDev_NOR_BSP_SPI.Wr((FS_QTY    )p_phy_data->UnitNbr,       /* Wr data.                                             */
                         (void     *)p_src,
                         (CPU_SIZE_T)cnt);
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */



                                                                /* ----------------- WAIT WHILE WR'ING ---------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */

    done    = DEF_NO;
    timeout = FS_DEV_NOR_PHY_MAX_PAGE_PGM_ms * 1000u;
    while ((timeout > 0u) && (done == DEF_NO)) {
        FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &sr, 1u);     /* Rd status reg.                                       */
        timeout--;
        done = DEF_BIT_IS_CLR(sr, FS_DEV_NOR_PHY_SR_RDY_BSY);
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */


                                                                /* ---------------------- DIS WR ---------------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_WRITE_DIS;
    FS_DEV_NOR_PHY_CMD(p_phy_data, &cmd[0], 1u);                /* Wr cmd.                                              */

    if (done == DEF_NO) {
       *p_err = FS_ERR_DEV_TIMEOUT;
        return;
    }

   *p_err = FS_ERR_NONE;
}
