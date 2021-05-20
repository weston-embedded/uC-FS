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
*                                      FILE SYSTEM DEVICE DRIVER
*
*                                          NOR FLASH DEVICES
*                             ATMEL AT45 SERIAL NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_at45.c
* Version  : V4.08.01
*********************************************************************************************************
* Note(s)  : (1) Support for binary page size (ie. 512, 1024... bytes pages) is not implemented. Contact
*                Micrium if this capability is required.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_NOR_AT45_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs.h"
#include  "../../../Source/fs_util.h"
#include  "fs_dev_nor_at45.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_MAX_BLK_ERASE_ms               150u    /* Blk erase timeout in ms.                             */
#define  FS_DEV_NOR_PHY_MAX_PG_PRGM_ms                 50u     /* Page program timeout in ms.                          */
#define  FS_DEV_NOR_PHY_MAX_PG_TRAN_ms                 10u     /* Main page to buf transfer timeout in ms.             */

#define  FS_DEV_NOR_PHY_MANUF_ID_ATMEL                 0x1Fu   /* Manufacturer ID.                                     */


/*
*********************************************************************************************************
*                                           COMMAND DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ READ COMMANDS ------------------- */
#define  FS_DEV_NOR_PHY_CMD_MEM_PAGE_READ              0xD2u    /* Read page.                                           */
#define  FS_DEV_NOR_PHY_CMD_MEM_PAGE_BUF1_TRAN         0x53u    /* Page to buf 1 transfer.                              */
#define  FS_DEV_NOR_PHY_CMD_MEM_PAGE_BUF2_TRAN         0x55u    /* Page to buf 2 transfer.                              */

                                                                /* ------------- PROGRAM & ERASE COMMANDS ------------- */
#define  FS_DEV_NOR_PHY_CMD_PAGE_ERASE                 0x81u    /* Page erase.                                          */
#define  FS_DEV_NOR_PHY_CMD_BLOCK_ERASE                0x50u    /* Blk erase.                                           */
#define  FS_DEV_NOR_PHY_CMD_SECTOR_ERASE               0x7Cu    /* Sec erase.                                           */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_00              0xC7u    /* Chip erase byte 1.                                   */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_01              0x94u    /* Chip erase byte 2.                                   */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_02              0x80u    /* Chip erase byte 3.                                   */
#define  FS_DEV_NOR_PHY_CMD_CHIP_ERASE_03              0x9Au    /* Chip erase byte 4.                                   */
#define  FS_DEV_NOR_PHY_CMD_BUF1_WRITE                 0x84u    /* Buf 1 write.                                         */
#define  FS_DEV_NOR_PHY_CMD_BUF2_WRITE                 0x87u    /* Buf 2 write.                                         */
#define  FS_DEV_NOR_PHY_CMD_BUF1_MEM_PAGE_PRGM         0x88u    /* Buf 1 to main page without erase.                    */
#define  FS_DEV_NOR_PHY_CMD_BUF2_MEM_PAGE_PRGM         0x89u    /* Buf 2 to main page without erase.                    */

                                                                /* -------------- MISCELLANEOUS COMMANDS -------------- */
#define  FS_DEV_NOR_PHY_CMD_MANUF_DEV_ID_READ          0x9Fu    /* Manufacturer and Device ID Rd.                       */
#define  FS_DEV_NOR_PHY_CMD_STATUS_REG_READ            0xD7u    /* Rd status reg.                                       */


/*
*********************************************************************************************************
*                                     STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_DEV_NOR_PHY_SR_PGSIZE                 DEF_BIT_00            /* Page size.                                          */
#define  FS_DEV_NOR_PHY_SR_PROT                   DEF_BIT_01            /* Sec prot status.                                    */
#define  FS_DEV_NOR_PHY_SR_COMP                   DEF_BIT_06            /* Comp result.                                        */
#define  FS_DEV_NOR_PHY_SR_RDY_BSY                DEF_BIT_07            /* RDY/BSY status.                                     */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  fs_dev_nor_phy_desc {
    CPU_INT32U  DevID;
    CPU_INT32U  PgSize;
    CPU_INT32U  DF_PgBit;
    CPU_INT32U  BlkSize;
    CPU_INT32U  SecSize;
    CPU_INT32U  PgCnt;
    CPU_BOOLEAN BinPg;
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
/*     DevID   PgSize  DF_PgBit BlkSize  SecSize  PgCnt  BinPg */
    {  0x22u,  256u,   9u,      8u,      128u,    512u , DEF_NO}, /* AT45DB011D */
    {  0x23u,  256u,   9u,      8u,      128u,    1024u, DEF_NO}, /* AT45DB021D */
    {  0x24u,  256u,   9u,      8u,      256u,    2048u, DEF_NO}, /* AT45DB041D */
    {  0x25u,  256u,   9u,      8u,      256u,    4096u, DEF_NO}, /* AT45DB081D */
    {  0x26u,  512u,   10u,     8u,      256u,    4096u, DEF_NO}, /* AT45DB161D */
    {  0x27u,  512u,   10u,     8u,      128u,    8192u, DEF_NO}, /* AT45DB321D */
    {  0x28u, 1024u,   11u,     8u,      256u,    8192u, DEF_NO}, /* AT45DB642D */
    {     0u,    0u,    0u,     0u,        0u,       0u, DEF_NO}
};

static  const  FS_DEV_NOR_PHY_DESC  FSDev_NOR_PHY_DevTblBin[] = {
/*     DevID   PgSize  DF_PgBit BlkSize  SecSize  PgCnt  BinPg */
    {  0x22u,  256u,   9u,      8u,      128u,    512u , DEF_YES}, /* AT45DB011D */
    {  0x23u,  256u,   9u,      8u,      128u,    1024u, DEF_YES}, /* AT45DB021D */
    {  0x24u,  256u,   9u,      8u,      256u,    2048u, DEF_YES}, /* AT45DB041D */
    {  0x25u,  256u,   9u,      8u,      256u,    4096u, DEF_YES}, /* AT45DB081D */
    {  0x26u,  512u,   10u,     8u,      256u,    4096u, DEF_YES}, /* AT45DB161D */
    {  0x27u,  512u,   10u,     8u,      128u,    8192u, DEF_YES}, /* AT45DB321D */
    {  0x28u, 1024u,   11u,     8u,      256u,    8192u, DEF_YES}, /* AT45DB642D */
    {     0u,    0u,    0u,     0u,        0u,       0u, DEF_NO}
};

#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  const  CPU_CHAR  *FSDev_NOR_PHY_NameTbl[] = {
    "Atmel AT45DB011D",
    "Atmel AT45DB021D",
    "Atmel AT45DB041D",
    "Atmel AT45DB081D",
    "Atmel AT45DB161D",
    "Atmel AT45DB321D",
    "Atmel AT45DB642D",
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


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                            /* -------- NOR PHY INTERFACE FNCTS ------- */
static  void        FSDev_NOR_PHY_Open     (FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Open NOR device.                       */
                                            FS_ERR               *p_err);

static  void        FSDev_NOR_PHY_Close    (FS_DEV_NOR_PHY_DATA  *p_phy_data);/* Close NOR device.                      */

static  void        FSDev_NOR_PHY_Rd       (FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Read from NOR device.                  */
                                            void                 *p_dest,
                                            CPU_INT32U            start,
                                            CPU_INT32U            cnt,
                                            FS_ERR               *p_err);

static  void        FSDev_NOR_PHY_Wr       (FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Write to NOR device.                   */
                                            void                 *p_src,
                                            CPU_INT32U            start,
                                            CPU_INT32U            cnt,
                                            FS_ERR               *p_err);

static  void        FSDev_NOR_PHY_EraseBlk (FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Erase block on NOR device.             */
                                            CPU_INT32U            start,
                                            CPU_INT32U            size,
                                            FS_ERR               *p_err);

static  void        FSDev_NOR_PHY_IO_Ctrl  (FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Perform NOR device I/O control.        */
                                            CPU_INT08U            opt,
                                            void                 *p_data,
                                            FS_ERR               *p_err);


                                                                            /* -------------- LOCAL FNCTS ------------- */
static  void        FSDev_NOR_PHY_EraseChip(FS_DEV_NOR_PHY_DATA  *p_phy_data, /* Erase NOR device.                      */
                                            FS_ERR               *p_err);

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_NOR_PHY_API  FSDev_NOR_AT45 = {
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
    CPU_INT08U   id_manuf;
    CPU_INT08U   id_dev;
    CPU_INT08U   id[4];
    CPU_INT08U   cmd[2];
    CPU_BOOLEAN  bin_pg;
    CPU_BOOLEAN  ok;


                                                                /* ---------------------- INIT HW --------------------- */
    ok = FSDev_NOR_BSP_SPI.Open(p_phy_data->UnitNbr);
    if (ok != DEF_OK) {                                         /* If HW could not be init'd ...                        */
       *p_err = FS_ERR_DEV_INVALID_UNIT_NBR;                    /* ... rtn err.                                         */
        return;
    }

    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);
    FS_OS_Dly_ms(20u);

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
        FS_TRACE_INFO(("NOR PHY AT45: Unrecognized manuf ID: 0x%02X != 0x%02X.\r\n", id_manuf, FS_DEV_NOR_PHY_MANUF_ID_ATMEL));
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
        FS_TRACE_INFO(("NOR PHY AT45: Unrecognized dev ID: 0x%02X.\r\n", id_dev));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

                                                                /* ------------------ READ STATUS REG ----------------- */
    cmd[0] = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr, &cmd[0], 1u);     /* Wr cmd.                                              */
    FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr, &id[0], 1u);      /* Rd 1-byte status.                                    */
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    bin_pg = DEF_NO;
    if(DEF_BIT_IS_SET(id[0], FS_DEV_NOR_PHY_SR_PGSIZE)) {       /* Check that dev is NOT configured with binary pages.  */
        bin_pg = DEF_YES;
    }

    if(bin_pg == DEF_YES) {
        FS_TRACE_INFO(("NOR PHY AT45: Binary page size not supported. See fs_dev_nor_at45.c for details."));
       *p_err = FS_ERR_DEV_IO;                                  /* Binary page size not supported.                      */
        return;
    }

                                                                /* ------------------- SAVE PHY INFO ------------------ */
    FS_TRACE_INFO(("NOR PHY AT45: Recognized device: Part nbr: %s\r\n", FSDev_NOR_PHY_NameTbl[desc_nbr]));
    FS_TRACE_INFO(("                                 Pg cnt : %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].PgCnt));
    FS_TRACE_INFO(("                                 Pg size: %d\r\n", FSDev_NOR_PHY_DevTbl[desc_nbr].PgSize));
    if(bin_pg == DEF_YES) {
        FS_TRACE_INFO(("                                 Pg type: Binary\r\n"));
    } else {
        FS_TRACE_INFO(("                                 Pg type: DataFlash\r\n"));
    }
    FS_TRACE_INFO(("                                 Manuf ID : 0x%02X\r\n", id_manuf));
    FS_TRACE_INFO(("                                 Dev   ID : 0x%02X\r\n", id_dev));


    p_phy_data->BlkCnt          =  FSDev_NOR_PHY_DevTbl[desc_nbr].PgCnt / FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize;
    p_phy_data->BlkSize         =  FSDev_NOR_PHY_DevTbl[desc_nbr].BlkSize * FSDev_NOR_PHY_DevTbl[desc_nbr].PgSize;
    p_phy_data->AddrRegionStart =  0u;

    if(bin_pg == DEF_YES) {
        p_phy_data->DataPtr     = (void *)&FSDev_NOR_PHY_DevTblBin[desc_nbr];   /* Save AT45 desc.                      */
    } else {
        p_phy_data->DataPtr     = (void *)&FSDev_NOR_PHY_DevTbl[desc_nbr];      /* Save AT45 desc.                      */
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
    CPU_INT08U            cmd[4];
    CPU_INT08U            dummy[4];
    CPU_INT32U            pg_ix;
    CPU_INT32U            pg_off;
    CPU_INT32U            cur_addr;
    CPU_INT32U            dev_addr;
    CPU_INT32U            rd_cnt;
    CPU_INT32U            cnt_rem;
    FS_DEV_NOR_PHY_DESC  *p_dev_desc;
    CPU_INT08U           *p_dest_08;


    p_dev_desc = (FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;
    p_dest_08  = (CPU_INT08U *)p_dest;

    cur_addr = start;
    cnt_rem  = cnt;
    if(p_dev_desc->BinPg == DEF_YES) {
       *p_err = FS_ERR_DEV_IO;                                  /* Binary page size not supported.                      */
        return;
    } else {
        while(cnt_rem > 0u) {
            pg_ix  = cur_addr / p_dev_desc->PgSize;
            pg_off = cur_addr % p_dev_desc->PgSize;
            rd_cnt = p_dev_desc->PgSize - pg_off;
            rd_cnt = (rd_cnt > cnt_rem) ? cnt_rem : rd_cnt;

            dev_addr  = pg_off;
            dev_addr += pg_ix << p_dev_desc->DF_PgBit;

            cmd[0] = FS_DEV_NOR_PHY_CMD_MEM_PAGE_READ;
            cmd[1] = (CPU_INT08U)((dev_addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[2] = (CPU_INT08U)((dev_addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[3] = (CPU_INT08U)((dev_addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

            FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);   /* En chip sel.                                         */
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr cmd.                                              */
                                 &cmd[0],
                                  4u);
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr dummy bytes.                                      */
                                 &dummy[0],
                                  4u);
            FSDev_NOR_BSP_SPI.Rd(p_phy_data->UnitNbr,           /* Rd data.                                             */
                                 p_dest_08,
                                 rd_cnt);
            FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);  /* Dis chip sel.                                        */

            cnt_rem   -= rd_cnt;
            cur_addr  += rd_cnt;
            p_dest_08 += rd_cnt;
        }
    }

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
    CPU_INT08U            cmd[4];
    CPU_INT08U            sr;
    CPU_INT32U            timeout;
    CPU_INT32U            pg_ix;
    CPU_INT32U            pg_off;
    CPU_INT32U            cur_addr;
    CPU_INT32U            dev_addr;
    CPU_INT32U            wr_cnt;
    CPU_INT32U            cnt_rem;
    FS_DEV_NOR_PHY_DESC  *p_dev_desc;
    CPU_INT08U           *p_src_08;


    p_dev_desc = (FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;
    p_src_08   = (CPU_INT08U *)p_src;

    cur_addr = start;
    cnt_rem  = cnt;
    while(cnt_rem > 0u) {
        if(p_dev_desc->BinPg == DEF_YES) {
           *p_err = FS_ERR_DEV_IO;                              /* Binary page size not supported.                      */
            return;
        } else {
            pg_ix  = cur_addr / p_dev_desc->PgSize;
            pg_off = cur_addr % p_dev_desc->PgSize;
            wr_cnt = p_dev_desc->PgSize - pg_off;
            wr_cnt = (wr_cnt > cnt_rem) ? cnt_rem : wr_cnt;

            dev_addr  = pg_off;
            dev_addr += pg_ix << p_dev_desc->DF_PgBit;

            if(wr_cnt < p_dev_desc->PgSize) {                   /* Partial page programming. Prepare R-M-W op.          */
                cmd[0] = FS_DEV_NOR_PHY_CMD_MEM_PAGE_BUF1_TRAN;
                cmd[1] = (CPU_INT08U)((dev_addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
                cmd[2] = (CPU_INT08U)((dev_addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
                cmd[3] = (CPU_INT08U)((dev_addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

                FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr); /* En chip sel.                                       */
                FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,        /* Wr cmd.                                            */
                                     &cmd[0],
                                      4u);
                FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);/* Dis chip sel.                                      */

                                                                 /* ------------- WAIT WHILE TRANSFERING -------------- */
                timeout = FS_DEV_NOR_PHY_MAX_PG_TRAN_ms;
                cmd[0]  = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
                FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);/* En chip sel.                                        */
                while(timeout != 0u) {
                    FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,   /* Wr cmd.                                             */
                                         &cmd[0],
                                          1u);
                    FSDev_NOR_BSP_SPI.Rd( p_phy_data->UnitNbr,   /* Rd 1-byte status.                                   */
                                         &sr,
                                          1u);
                    if(DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_RDY_BSY)) {
                        break;
                    }

                    FS_OS_Dly_ms(1u);
                    timeout--;
                }
                FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);/* Dis chip sel.                                      */

                if(timeout == 0) {
                   *p_err = FS_ERR_DEV_TIMEOUT;
                    return;
                }
            }

            cmd[0] = FS_DEV_NOR_PHY_CMD_BUF1_WRITE;
            cmd[1] = (CPU_INT08U)((dev_addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[2] = (CPU_INT08U)((dev_addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[3] = (CPU_INT08U)((dev_addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

            FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);   /* En chip sel.                                         */
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr cmd.                                              */
                                 &cmd[0],
                                  4u);
            FSDev_NOR_BSP_SPI.Wr(p_phy_data->UnitNbr,           /* Wr data.                                             */
                                 p_src_08,
                                 wr_cnt);
            FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);  /* Dis chip sel.                                        */


            cmd[0] = FS_DEV_NOR_PHY_CMD_BUF1_MEM_PAGE_PRGM;
            FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);   /* En chip sel.                                         */
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr cmd.                                              */
                                 &cmd[0],
                                  4u);
            FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);  /* Dis chip sel.                                        */


                                                                /* ---------------- WAIT WHILE WR'ING ----------------- */
            timeout = FS_DEV_NOR_PHY_MAX_PG_PRGM_ms;
            cmd[0]  = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
            FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);   /* En chip sel.                                         */
            while(timeout > 0u) {
                FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,      /* Wr cmd.                                              */
                                     &cmd[0],
                                      1u);
                FSDev_NOR_BSP_SPI.Rd( p_phy_data->UnitNbr,      /* Rd 1-byte status.                                    */
                                     &sr,
                                      1u);
                if(DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_RDY_BSY)) {
                    break;
                }

                FS_OS_Dly_ms(1u);
                timeout--;
            }
            FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);  /* Dis chip sel.                                        */


            if(timeout == 0) {
               *p_err = FS_ERR_DEV_TIMEOUT;
                return;
            }


            cnt_rem  -= wr_cnt;
            cur_addr += wr_cnt;
            p_src_08 += wr_cnt;
        }
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
    CPU_INT08U           cmd[4];
    CPU_INT08U           sr;
    CPU_INT32U           timeout;
    CPU_INT32U           blk_ix;
    CPU_INT32U           blk_addr;
    CPU_INT32U           blk_size_log2;
    FS_DEV_NOR_PHY_DESC *p_dev_desc;


    p_dev_desc = (FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;

    if(size != p_dev_desc->PgSize * p_dev_desc->BlkSize) {
       *p_err = FS_ERR_DEV_IO;                                  /* Only blk's should be erased.                         */
        return;
    }

    blk_size_log2 = FSUtil_Log2(p_dev_desc->BlkSize);

    blk_ix   = start / (p_dev_desc->PgSize * p_dev_desc->BlkSize);
    blk_addr = blk_ix << blk_size_log2;

    if(p_dev_desc->BinPg == DEF_YES) {
       *p_err = FS_ERR_DEV_IO;                                  /* Binary page size not supported.                      */
        return;
    } else {
        blk_addr = blk_addr << p_dev_desc->DF_PgBit;
        cmd[0] = FS_DEV_NOR_PHY_CMD_BLOCK_ERASE;
        cmd[1] = (CPU_INT08U)((blk_addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        cmd[2] = (CPU_INT08U)((blk_addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
        cmd[3] = (CPU_INT08U)((blk_addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

        FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);       /* En chip sel.                                         */
        FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,              /* Wr cmd.                                              */
                             &cmd[0],
                              4u);
        FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);      /* Dis chip sel.                                        */
    }

                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
    timeout = FS_DEV_NOR_PHY_MAX_BLK_ERASE_ms;
    cmd[0]  = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
    FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);           /* En chip sel.                                         */
    while(timeout > 0u) {
        FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,              /* Wr cmd.                                              */
                             &cmd[0],
                              1u);
        FSDev_NOR_BSP_SPI.Rd( p_phy_data->UnitNbr,              /* Rd 1-byte status.                                    */
                             &sr,
                              1u);
        if(DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_RDY_BSY)) {
            break;
        }

        FS_OS_Dly_ms(1u);
        timeout--;

    }
    FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);          /* Dis chip sel.                                        */

    if(timeout == 0) {
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
    CPU_INT08U            cmd[4];
    CPU_INT08U            sr;
    CPU_INT32U            timeout;
    CPU_INT32U            blk_ix;
    CPU_INT32U            blk_addr;
    CPU_INT32U            blk_cnt;
    CPU_INT32U            blk_size_log2;
    FS_DEV_NOR_PHY_DESC  *p_dev_desc;


    p_dev_desc = (FS_DEV_NOR_PHY_DESC *)p_phy_data->DataPtr;

    blk_size_log2 = FSUtil_Log2(p_dev_desc->BlkSize);
    blk_cnt       = p_dev_desc->PgCnt / p_dev_desc->BlkSize;

    for(blk_ix = 0u; blk_ix < blk_cnt; blk_ix++) {
        blk_addr = blk_ix << blk_size_log2;

        if(p_dev_desc->BinPg == DEF_YES) {
           *p_err = FS_ERR_DEV_IO;                              /* Binary page size not supported.                      */
            return;
        } else {
            blk_addr = blk_addr << p_dev_desc->DF_PgBit;
            cmd[0] = FS_DEV_NOR_PHY_CMD_BLOCK_ERASE;
            cmd[1] = (CPU_INT08U)((blk_addr >> (2u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[2] = (CPU_INT08U)((blk_addr >> (1u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);
            cmd[3] = (CPU_INT08U)((blk_addr >> (0u * DEF_OCTET_NBR_BITS)) & DEF_OCTET_MASK);

            FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);   /* En chip sel.                                         */
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr cmd.                                              */
                                 &cmd[0],
                                  4u);
            FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);  /* Dis chip sel.                                        */
        }

                                                                /* --------------- WAIT WHILE ERASE'ING --------------- */
        timeout = FS_DEV_NOR_PHY_MAX_BLK_ERASE_ms;
        cmd[0]  = FS_DEV_NOR_PHY_CMD_STATUS_REG_READ;
        FSDev_NOR_BSP_SPI.ChipSelEn(p_phy_data->UnitNbr);       /* En chip sel.                                         */
        while(timeout > 0u) {
            FSDev_NOR_BSP_SPI.Wr( p_phy_data->UnitNbr,          /* Wr cmd.                                              */
                                 &cmd[0],
                                  1u);
            FSDev_NOR_BSP_SPI.Rd( p_phy_data->UnitNbr,          /* Rd 1-byte status.                                    */
                                 &sr,
                                  1u);
            if(DEF_BIT_IS_SET(sr, FS_DEV_NOR_PHY_SR_RDY_BSY)) {
                break;
            }

            FS_OS_Dly_ms(1u);
            timeout--;
        }

        FSDev_NOR_BSP_SPI.ChipSelDis(p_phy_data->UnitNbr);      /* Dis chip sel.                                        */

        if(timeout == 0u) {
           *p_err = FS_ERR_DEV_TIMEOUT;
            return;
        }
    }

    *p_err = FS_ERR_NONE;
}
