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
*                                             SD/MMC CARD
*                                     COMMON DEFINES & FUNCTIONS
*
* Filename     : fs_dev_sd.c
* Version      : V4.08.00
*********************************************************************************************************
* Reference(s) : (1) SD Card Association.  "Physical Layer Simplified Specification Version 2.00".
*                    July 26, 2006.
*
*                (2) JEDEC Solid State Technology Association.  "MultiMediaCard (MMC) Electrical
*                    Standard, High Capacity".  JESD84-B42.  July 2007.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_SD_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_mem.h>
#include  "../../Source/fs.h"
#include  "fs_dev_sd.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ CHECKSUMS (4.5) ----------------- */
#define  FS_DEV_SD_CRC7_POLY                            0x09u   /* 7-bit checksum polynomial: x7 + x3 + 1.              */


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

                                                                /* Converts CSD read_bl_len field to actual blk len.    */
static  const  CPU_INT32U  FSDev_SD_BlkLen[]         = {       512u,
                                                              1024u,
                                                              2048u};

                                                                /* Converts CSD c_size_mult field to actual mult.       */
static  const  CPU_INT32U  FSDev_SD_Mult[]           = {         4u,
                                                                 8u,
                                                                16u,
                                                                32u,
                                                                64u,
                                                               128u,
                                                               256u,
                                                               512u,
                                                              1024u};

                                                                /* Converts CSD TAAC field's time unit to 1 ns.         */
static  const  CPU_INT32U  FSDev_SD_TAAC_TimeUnit[]  = {         1u,
                                                                10u,
                                                               100u,
                                                              1000u,
                                                             10000u,
                                                            100000u,
                                                           1000000u,
                                                          10000000u};

                                                                /* Converts CSD TAAC field's time value to mult.        */
static  const  CPU_INT32U  FSDev_SD_TAAC_TimeValue[] = {         0u,
                                                                10u,
                                                                12u,
                                                                13u,
                                                                15u,
                                                                20u,
                                                                25u,
                                                                30u,
                                                                35u,
                                                                40u,
                                                                45u,
                                                                50u,
                                                                55u,
                                                                60u,
                                                                70u,
                                                                80u};

                                                                /* Converts CSD tran_speed field's rate unit to 10 kb/s.*/
static  const  CPU_INT32U  FSDev_SD_TranSpdUnit[]    = {     10000u,
                                                            100000u,
                                                           1000000u,
                                                          10000000u};

                                                                /* Converts CSD tran_speed field's time value to mult.  */
static  const  CPU_INT32U  FSDev_SD_TranSpdValue[]   = {         0u,
                                                                10u,
                                                                12u,
                                                                13u,
                                                                15u,
                                                                20u,
                                                                25u,
                                                                30u,
                                                                35u,
                                                                40u,
                                                                45u,
                                                                50u,
                                                                55u,
                                                                60u,
                                                                70u,
                                                                80u};

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
static  const  CRC_MODEL_16  FSDev_SD_ModelCRC16     = {
    0x1021u,
    0x0000u,
    DEF_NO,
    0x0000u,
   &CRC_TblCRC16_1021[0]
};
#endif


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


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FSDev_SD_ParseCSD()
*
* Description : Parse CSD data.
*
* Argument(s) : csd         Array holding CSD data.
*
*               p_sd_info   Pointer to struct into which CSD data will be parsed.
*
*               card_type   Type of card :
*
*                               FS_DEV_SD_CARDTYPE_SD_V1_X       SD card v1.x.
*                               FS_DEV_SD_CARDTYPE_SD_V2_0       SD card v2.0, standard capacity.
*                               FS_DEV_SD_CARDTYPE_SD_V2_0_HC    SD card v2.0, high capacity.
*                               FS_DEV_SD_CARDTYPE_MMC           MMC, standard capacity.
*                               FS_DEV_SD_CARDTYPE_MMC_HC        MMC, high capacity.
*
* Return(s)   : DEF_OK   if the CSD could be parsed.
*               DEF_FAIL if the CSD was illegal.
*
* Note(s)     : (1) (a) For SD v1.x & SD v2.0 standard capacity, the structure of the CSD is defined in
*                       [Ref 1], Section 5.3.2.
*
*                   (b) For MMC cards, the structure of the CSD is defined in [Ref 2], Section 8.3.
*
*                   (c) For SD v2.0 high capacity cards, the structure of the CSD is defined in [Ref 1],
*                       Section 5.3.3.
*
*               (2) The calculation of the card capacity is demonstrated in the [Ref 1], Section 5.3.2.
*                   The text does not explicitly state that the card capacity for a v1.x-compliant card
*                   shall not exceed the capacity of a 32-bit integer.
*
*               (3) According to [Ref 1], for a v1.x-compliant card, the CSD's tran_speed field "shall
*                   be always 0_0110_010b (032h)", or 25MHz.  In high-speed mode, the field value "shall
*                   be always 0_1011_010b (06Ah)", or 50MHz.
*
*               (4) According to [Ref 1], Section 4.6.2.1-2, the timeouts for high-capacity SD cards
*                   should be fixed; consequently, TSAC & NSAC should not be consulted.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_SD_ParseCSD (CPU_INT08U       csd[],
                                FS_DEV_SD_INFO  *p_sd_info,
                                CPU_INT08U       card_type)
{
    CPU_INT32U  block_len;
    CPU_INT32U  blocknr;
    CPU_INT32U  c_size;
    CPU_INT32U  c_size_mult;
    CPU_INT32U  clk_freq;
    CPU_INT32U  mult;
    CPU_INT08U  nsac;
    CPU_INT08U  taac;
    CPU_INT32U  taac_time;
    CPU_INT08U  rd_bl_len;
    CPU_INT08U  tran_spd;
    CPU_INT08U  tran_spd_unit;



    if (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0_HC) {           /* ----------- CALC DISK SIZE (SD HC CARD) ------------ */
        c_size             = (((CPU_INT32U)csd[7] & 0x3Fu) << 16)       /* Get dev size.                                */
                           | (((CPU_INT32U)csd[8] & 0xFFu) <<  8)
                           | (((CPU_INT32U)csd[9] & 0xFFu) <<  0);

        p_sd_info->BlkSize = FS_DEV_SD_BLK_SIZE;
        p_sd_info->NbrBlks = (c_size + 1u) * 1024u;



    } else {                                                    /* ----------- CALC DISK SIZE (SD STD CARD) ----------- */
        rd_bl_len          = csd[5] & DEF_NIBBLE_MASK;                  /* Get rd blk len exp.                          */

        if ((rd_bl_len < 9u) || (rd_bl_len > 11u)) {
            return (DEF_NO);
        }

        block_len          = FSDev_SD_BlkLen[rd_bl_len - 9u];           /* Calc blk len.                                */

        c_size             = (((CPU_INT32U)csd[8] & 0xC0u) >>  6)       /* Get dev size.                                */
                           | (((CPU_INT32U)csd[7] & 0xFFu) <<  2)
                           | (((CPU_INT32U)csd[6] & 0x03u) << 10);

        c_size_mult        = (((CPU_INT32U)csd[10] & 0x80u) >> 7)       /* Get dev size mult.                           */
                           | (((CPU_INT32U)csd[9]  & 0x03u) << 1);


        if (c_size_mult >= 8u) {
            return (DEF_NO);
        }

        mult               = FSDev_SD_Mult[c_size_mult];                /* Calc mult.                                   */
        blocknr            = (c_size + 1u) * mult;                      /* Calc nbr blks.                               */

        p_sd_info->BlkSize = block_len;
        p_sd_info->NbrBlks = blocknr;
    }



                                                                /* ------------------- CALC CLK SPD ------------------- */
    taac                = csd[1];                               /* TAAC data access time.                               */
    taac_time           = FSDev_SD_TAAC_TimeUnit[taac & 0x07u];
    taac_time          *= FSDev_SD_TAAC_TimeValue[(taac & 0x78u) >> 3];

    nsac                = csd[2];                               /* NSAC data access time.                               */

    tran_spd            = csd[3];                               /* Max transfer rate.                                   */
    tran_spd_unit       = tran_spd & 0x07u;
    if (tran_spd_unit > 3u) {
        return (DEF_NO);
    }
    clk_freq            = FSDev_SD_TranSpdUnit[tran_spd_unit];
    clk_freq           *= FSDev_SD_TranSpdValue[(tran_spd & 0x78u) >> 3];
    p_sd_info->ClkFreq  = clk_freq;                             /* Max clk freq, in clk/sec.                            */

    clk_freq           /= 100u;                                 /* Conv to clk/100 us.                                  */
    taac_time          /= 1000000u;                             /* Conv to 100 us.                                      */
    p_sd_info->Timeout  = (taac_time * clk_freq) + nsac;        /* Timeout, in clks.                                    */

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                         FSDev_SD_ParseCID()
*
* Description : Parse CID data.
*
* Argument(s) : cid         Array holding CID data.
*
*               p_sd_info   Pointer to struct into which CID data will be parsed.
*
*               card_type   Type of card :
*
*                               FS_DEV_SD_CARDTYPE_SD_V1_X       SD card v1.x.
*                               FS_DEV_SD_CARDTYPE_SD_V2_0       SD card v2.0, standard capacity.
*                               FS_DEV_SD_CARDTYPE_SD_V2_0_HC    SD card v2.0, high capacity.
*                               FS_DEV_SD_CARDTYPE_MMC           MMC, standard capacity.
*                               FS_DEV_SD_CARDTYPE_MMC_HC        MMC, high capacity.
*
* Return(s)   : DEF_OK   if the CID could be parsed.
*               DEF_FAIL if the CID was illegal.
*
* Note(s)     : (1) (a) For SD cards, the structure of the CID is defined in [Ref 1] Section 5.1.
*
*                   (b) For MMC cards, the structure of the CID is defined in [Ref 2] Section 8.2.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_SD_ParseCID (CPU_INT08U       cid[],
                                FS_DEV_SD_INFO  *p_sd_info,
                                CPU_INT08U       card_type)
{
    CPU_INT08U  manuf_id;
    CPU_INT16U  oem_id;
    CPU_INT32U  prod_sn;
    CPU_CHAR    prod_name[7];
    CPU_INT16U  prod_rev;
    CPU_INT16U  date;


    manuf_id =  cid[0];

    oem_id   = (CPU_INT16U)((CPU_INT16U)cid[1] << 8)
             | (CPU_INT16U)((CPU_INT16U)cid[2] << 0);

    if ((card_type == FS_DEV_SD_CARDTYPE_SD_V1_X) ||
        (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0) ||
        (card_type == FS_DEV_SD_CARDTYPE_SD_V2_0_HC)) {
        prod_sn      = ((CPU_INT32U)cid[9]  << 24)
                     | ((CPU_INT32U)cid[10] << 16)
                     | ((CPU_INT32U)cid[11] <<  8)
                     | ((CPU_INT32U)cid[12] <<  0);

        prod_name[0] = (CPU_CHAR)cid[3];
        prod_name[1] = (CPU_CHAR)cid[4];
        prod_name[2] = (CPU_CHAR)cid[5];
        prod_name[3] = (CPU_CHAR)cid[6];
        prod_name[4] = (CPU_CHAR)cid[7];
        prod_name[5] = (CPU_CHAR)'\0';
        prod_name[6] = (CPU_CHAR)'\0';

        prod_rev     = ((((CPU_INT16U)cid[8] & 0xF0u) >> 4) * 10u)
                     +   ((CPU_INT16U)cid[8] & 0x0Fu);

        date         =  (((CPU_INT16U)cid[14] & 0x0Fu) * 256u)
                     + ((((CPU_INT16U)cid[13] & 0x0Fu) << 4)
                     +  (((CPU_INT16U)cid[14] & 0xF0u) >> 4));


    } else {
        prod_sn      = ((CPU_INT32U)cid[10] << 24)
                     | ((CPU_INT32U)cid[11] << 16)
                     | ((CPU_INT32U)cid[12] <<  8)
                     | ((CPU_INT32U)cid[13] <<  0);

        prod_name[0] = (CPU_CHAR)cid[3];
        prod_name[1] = (CPU_CHAR)cid[4];
        prod_name[2] = (CPU_CHAR)cid[5];
        prod_name[3] = (CPU_CHAR)cid[6];
        prod_name[4] = (CPU_CHAR)cid[7];
        prod_name[5] = (CPU_CHAR)cid[8];
        prod_name[6] = (CPU_CHAR)'\0';

        prod_rev     = (((CPU_INT16U)cid[9] & 0x0Fu) * 10u)
                     + (((CPU_INT16U)cid[9] & 0xF0u) >> 4);

        date         = ((((CPU_INT16U)cid[14] & 0xF0u) >> 4) * 256u)
                     +   ((CPU_INT16U)cid[14] & 0x0Fu);
    }

    p_sd_info->ManufID = manuf_id;
    p_sd_info->OEM_ID  = oem_id;
    p_sd_info->ProdSN  = prod_sn;
    Mem_Copy(&p_sd_info->ProdName[0], &prod_name[0], 7u);
    p_sd_info->ProdRev = prod_rev;
    p_sd_info->Date    = date;

    return (DEF_YES);
}



/*
*********************************************************************************************************
*                                     FSDev_SD_ChkSumCalc_7Bit()
*
* Description : Calculate 7-bit Cyclic Redundancy Checksum (CRC).
*
* Argument(s) : p_data      Pointer to buffer.
*
*               size        Size of buffer, in octets.
*
* Return(s)   : 7-bit CRC, in 8-bit value.
*
* Note(s)     : (1) According to [Ref 1], Section 7.2.2 :
*
*                   (a) The CRC7 is required for the reset command, CMD0, since the card is still in SD
*                       mode when this command is received.
*
*                   (b) Despite being in SPI mode, the card always verifies the CRC for CMD8.
*
*                   (c) Otherwise, unless CMD59 is sent to enable CRC verification, the CRC7 need not
*                       be calculated.
*********************************************************************************************************
*/

CPU_INT08U  FSDev_SD_ChkSumCalc_7Bit (CPU_INT08U  *p_data,
                                      CPU_INT32U   size)
{
    CPU_INT08U  crc;
    CPU_INT08U  data;
    CPU_INT08U  mask;
    CPU_INT08U  bit_ix;


    crc = 0u;

    while (size > 0u) {
        data  = *p_data;
                                                                /* Add in uppermost bit of byte.                        */
        mask = (DEF_BIT_IS_SET(data, DEF_BIT_07) == DEF_YES) ? DEF_BIT_06 : DEF_BIT_NONE;
        if (((crc & DEF_BIT_06) ^ mask) != 0x00u) {
            crc = ((CPU_INT08U)(crc << 1)) ^ FS_DEV_SD_CRC7_POLY;
        } else {
            crc <<= 1;
        }

        crc ^= data;                                            /* Add in remaining bits of byte.                       */
        for (bit_ix = 0u; bit_ix < 7u; bit_ix++) {
            if (DEF_BIT_IS_SET(crc, DEF_BIT_06) == DEF_YES) {
                crc = ((CPU_INT08U)(crc << 1)) ^ FS_DEV_SD_CRC7_POLY;
            } else {
                crc <<= 1;
            }
        }

        size--;
        p_data++;
    }

    crc &= 0x7Fu;
    return (crc);
}


/*
*********************************************************************************************************
*                                     FSDev_SD_ChkSumCalc_16Bit()
*
* Description : Calculate 16-bit Cyclic Redundancy Checksum (CRC).
*
* Argument(s) : p_data      Pointer to buffer.
*
*               size        Size of buffer, in octets.
*
* Return(s)   : 16-bit CRC.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (FS_DEV_SD_SPI_CFG_CRC_EN == DEF_ENABLED)
CPU_INT16U  FSDev_SD_ChkSumCalc_16Bit (CPU_INT08U  *p_data,
                                       CPU_INT32U   size)
{
    CPU_INT16U  crc;
    EDC_ERR     crc_err;


    crc = CRC_ChkSumCalc_16Bit((CRC_MODEL_16 *)&FSDev_SD_ModelCRC16,
                                                p_data,
                                                size,
                                               &crc_err);

    return (crc);
}
#endif


#if (FS_TRACE_LEVEL >= TRACE_LEVEL_INFO)
/*
*********************************************************************************************************
*                                        FSDev_SD_TraceInfo()
*
* Description : Output SD trace info.
*
* Argument(s) : p_sd_info   Pointer to SD info.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_TraceInfo (FS_DEV_SD_INFO  *p_sd_info)
{
    switch (p_sd_info->CardType) {
        case FS_DEV_SD_CARDTYPE_SD_V1_X:
             FS_TRACE_INFO(("SD/MMC FOUND:  v1.x SD card\r\n"));
             break;

        case FS_DEV_SD_CARDTYPE_SD_V2_0:
             FS_TRACE_INFO(("SD/MMC FOUND:  v2.0 standard-capacity SD card\r\n"));
             break;

        case FS_DEV_SD_CARDTYPE_SD_V2_0_HC:
             FS_TRACE_INFO(("SD/MMC FOUND:  v2.0 high-capacity card\r\n"));
             break;

        case FS_DEV_SD_CARDTYPE_MMC:
             FS_TRACE_INFO(("SD/MMC FOUND:  standard-capacity MMC card\r\n"));
             break;

        case FS_DEV_SD_CARDTYPE_MMC_HC:
             FS_TRACE_INFO(("SD/MMC FOUND:  high-capacity MMC card\r\n"));
             break;

        default:
             break;
    }

    FS_TRACE_INFO(("               Blk Size       : %d bytes\r\n", p_sd_info->BlkSize));
    FS_TRACE_INFO(("               # Blks         : %d\r\n",       p_sd_info->NbrBlks));
    FS_TRACE_INFO(("               Max Clk        : %d Hz\r\n",    p_sd_info->ClkFreq));
    FS_TRACE_INFO(("               Manufacturer ID: 0x%02X\r\n",   p_sd_info->ManufID));
    FS_TRACE_INFO(("               OEM/App ID     : 0x%04X\r\n",   p_sd_info->OEM_ID));
    FS_TRACE_INFO(("               Prod Name      : %s\r\n",      &p_sd_info->ProdName[0]));
    FS_TRACE_INFO(("               Prod Rev       : %d.%d\r\n",    p_sd_info->ProdRev / 10u, p_sd_info->ProdRev % 10u));
    FS_TRACE_INFO(("               Prod SN        : 0x%08X\r\n",   p_sd_info->ProdSN));
    FS_TRACE_INFO(("               Date           : %d/%d\r\n",    p_sd_info->Date / 256u, p_sd_info->Date % 256u));
}
#endif


/*
*********************************************************************************************************
*                                         FSDev_SD_ClrInfo()
*
* Description : Clear SD info.
*
* Argument(s) : p_sd_info   Pointer to SD info.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_SD_ClrInfo (FS_DEV_SD_INFO  *p_sd_info)
{
    p_sd_info->BlkSize      = 0u;
    p_sd_info->NbrBlks      = 0u;
    p_sd_info->ClkFreq      = 0u;
    p_sd_info->Timeout      = 0u;
    p_sd_info->CardType     = FS_DEV_SD_CARDTYPE_NONE;
    p_sd_info->HighCapacity = DEF_NO;

    p_sd_info->ManufID      = 0u;
    p_sd_info->OEM_ID       = 0u;
    p_sd_info->ProdSN       = 0u;
    Mem_Set(&p_sd_info->ProdName[0], (CPU_INT08U)'\0', 7u);
    p_sd_info->ProdRev      = 0u;
    p_sd_info->Date         = 0u;
}


/*
*********************************************************************************************************
*                                        FSDev_SD_ParseEXT_CSD()
*
* Description : Parse MMC's EXT_CSD data.
*
* Argument(s) : ext_csd     Array holding EXT_CSD data.
*
*               p_sd_info   Pointer to struct into which EXT_CSD data will be parsed.
*
* Return(s)   : DEF_OK,    if the EXT_CSD could be parsed.
*               DEF_FAIL,  if the EXT_CSD was illegal.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_SD_ParseEXT_CSD (CPU_INT08U       ext_csd[],
                                    FS_DEV_SD_INFO  *p_sd_info)
{
    CPU_INT32U  size;


    size = (((CPU_INT32U)ext_csd[215]) << 24)                 /* Get dev size.                                          */
         | (((CPU_INT32U)ext_csd[214]) << 16)
         | (((CPU_INT32U)ext_csd[213]) <<  8)
         | (((CPU_INT32U)ext_csd[212]) <<  0);

    p_sd_info->NbrBlks = size;

    return (DEF_OK);
}
