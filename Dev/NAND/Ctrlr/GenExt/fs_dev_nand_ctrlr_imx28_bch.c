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
*                     NAND DEVICE GENERIC CONTROLLER IMX28 HARDWARE ECC EXTENSION
*
* Filename : fs_dev_nand_ctrlr_imx28_bch.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_NAND_CGEN_BCH_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/ecc.h>
#include  "../../../../Source/fs_util.h"
#include  "fs_dev_nand_ctrlr_imx28_bch.h"


/*
*********************************************************************************************************
*                                             LOCAL DEFINES
*********************************************************************************************************
*/

                                                                /* ---------------- BCH REGS DEFINES ------------------ */
#define  iMX28_ADDR_BCH                                    (CPU_ADDR)(0x8000A000)

#define  iMX28_ADDR_BCH_CTRL                               (CPU_REG32 *)(iMX28_ADDR_BCH + 0x00)
#define  iMX28_REG_BCH_CTRL                                (*(iMX28_ADDR_BCH_CTRL + 0))
#define  iMX28_REG_BCH_CTRL_SET                            (*(iMX28_ADDR_BCH_CTRL + 1))
#define  iMX28_REG_BCH_CTRL_CLR                            (*(iMX28_ADDR_BCH_CTRL + 2))
#define  iMX28_REG_BCH_CTRL_TOG                            (*(iMX28_ADDR_BCH_CTRL + 3))

#define  iMX28_BIT_BCH_SFRST                               (DEF_BIT_32)
#define  iMX28_BIT_BCH_CLKGATE                             (DEF_BIT_31)
#define  iMX28_BIT_BCH_DEBUGSYNDROME                       (DEF_BIT_22)
#define  iMX28_BIT_BCH_M2M_LAYOUT_MSK                      (DEF_BIT_FIELD(2, 18))
#define  iMX28_BIT_BCH_M2M_LAYOUT(layout)                  (DEF_BIT_MASK(layout, 18) & iMX28_BIT_BCH_M2M_LAYOUT_MSK)
#define  iMX28_BIT_BCH_M2M_ENCODE                          (DEF_BIT_17)
#define  iMX28_BIT_BCH_M2M_ENABLE                          (DEF_BIT_16)
#define  iMX28_BIT_BCH_DEBUG_IRQ_EN                        (DEF_BIT_10)
#define  iMX28_BIT_BCH_IRQ_EN                              (DEF_BIT_08)
#define  iMX28_BIT_BCH_BM_ERROR_IRQ                        (DEF_BIT_03)
#define  iMX28_BIT_BCH_DEBUG_IRQ                           (DEF_BIT_02)
#define  iMX28_BIT_BCH_COMPLETE_IRQ                        (DEF_BIT_00)

#define  iMX28_REG_BCH_STATUS0                             (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x10))



#define  iMX28_REG_BCH_MODE                                (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x20))

#define  iMX28_REG_BCH_ENCODEPTR                           (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x30))
#define  iMX28_REG_BCH_DATAPTR                             (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x40))
#define  iMX28_REG_BCH_METAPTR                             (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x50))

#define  iMX28_REG_BCH_LAYOUTSELECT                        (*((CPU_REG32 *)(iMX28_ADDR_BCH + 0x70)))
#define  iMX28_BIT_BCH_LAYOUTSELECT(cs_nbr, layout)        ((layout << (layout * 2)) & (2 << (layout * 2)))

#define  iMX28_REG_BCH_FLASH0LAYOUT0                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x80))
#define  iMX28_REG_BCH_FLASH0LAYOUT1                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0x90))
#define  iMX28_REG_BCH_FLASH1LAYOUT0                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xA0))
#define  iMX28_REG_BCH_FLASH1LAYOUT1                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xB0))
#define  iMX28_REG_BCH_FLASH2LAYOUT0                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xC0))
#define  iMX28_REG_BCH_FLASH2LAYOUT1                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xD0))
#define  iMX28_REG_BCH_FLASH3LAYOUT0                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xE0))
#define  iMX28_REG_BCH_FLASH3LAYOUT1                       (*(CPU_REG32 *)(iMX28_ADDR_BCH + 0xF0))

#define  iMX28_BIT_BCH_LAYOUT0_NBLOCKS_MSK                 (DEF_BIT_FIELD(8, 24))
#define  iMX28_BIT_BCH_LAYOUT0_NBLOCKS(nbr)                (DEF_BIT_MASK(nbr, 24) & iMX28_BIT_BCH_LAYOUT0_NBLOCKS_MSK)
#define  iMX28_BIT_BCH_LAYOUT0_METASIZE_MSK                (DEF_BIT_FIELD(8, 16))
#define  iMX28_BIT_BCH_LAYOUT0_METASIZE(size)              (DEF_BIT_MASK(size, 16) & iMX28_BIT_BCH_LAYOUT0_METASIZE_MSK)
#define  iMX28_BIT_BCH_LAYOUT0_ECC0_MSK                    (DEF_BIT_FIELD(4, 12))
#define  iMX28_BIT_BCH_LAYOUT0_ECC0(ecc)                   (DEF_BIT_MASK(ecc, 12) & iMX28_BIT_BCH_LAYOUT0_ECC0_MSK)
#define  iMX28_BIT_BCH_LAYOUT0_DAT0SZ_MSK                  (DEF_BIT_FIELD(12, 0))
#define  iMX28_BIT_BCH_LAYOUT0_DAT0SZ(size)                (DEF_BIT_MASK(size, 0) & iMX28_BIT_BCH_LAYOUT0_DAT0SZ_MSK)

#define  iMX28_BIT_BCH_LAYOUT1_PAGESZ_MSK                  (DEF_BIT_FIELD(16, 16))
#define  iMX28_BIT_BCH_LAYOUT1_PAGESZ(size)                (DEF_BIT_MASK(size, 16) & iMX28_BIT_BCH_LAYOUT1_PAGESZ_MSK)
#define  iMX28_BIT_BCH_LAYOUT1_ECCN_MSK                    (DEF_BIT_FIELD(4, 12))
#define  iMX28_BIT_BCH_LAYOUT1_ECCN(ecc)                   (DEF_BIT_MASK(ecc, 12) & iMX28_BIT_BCH_LAYOUT1_ECCN_MSK)
#define  iMX28_BIT_BCH_LAYOUT1_DATANSZ_MSK                 (DEF_BIT_FIELD(12, 0))
#define  iMX28_BIT_BCH_LAYOUT1_DATANSZ(size)               (DEF_BIT_MASK(size, 0) & iMX28_BIT_BCH_LAYOUT1_DATANSZ_MSK)


#define  iMX28_ADDR_BCH_DEBUG0                             (CPU_REG32 *)(iMX28_ADDR_BCH + 0x100)
#define  iMX28_REG_BCH_DEBUG0_SET                          (*(iMX28_ADDR_BCH_DEBUG0 + 1))
#define  iMX28_REG_BCH_DEBUG0_CLR                          (*(iMX28_ADDR_BCH_DEBUG0 + 2))
#define  iMX28_REG_BCH_DEBUG0_TOG                          (*(iMX28_ADDR_BCH_DEBUG0 + 3))

/*
*********************************************************************************************************
*                                            LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef  struct  bch_data  BCH_DATA;
struct bch_data {
           CPU_INT32U   Ecc0;
           CPU_INT32U   EccN;
           CPU_INT32U   NbrBlk;
           CPU_INT32U   DataBlkSize;
           CPU_INT32U   PgSize;
           CPU_INT32U   SpareSize;
           CPU_INT32U   EccWordCnt;
           CPU_INT32U   EccByteCnt;
           CPU_INT32U   EccRefreshThres;
           CPU_INT08U  *BufPtr;
           BCH_DATA    *NextPtr;
};


/*
*********************************************************************************************************
*                                              LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#define  FS_NAND_CTRLR_GEN_BCH_CFG_FIELD(type, name, dflt_val)  dflt_val,
const  FS_NAND_CTRLR_GEN_BCH_CFG  FS_NAND_CtrlrGen_BCH_DlftCfg = { FS_NAND_CTRLR_GEN_BCH_CFG_FIELDS };
#undef   FS_NAND_CTRLR_GEN_SOFT_ECC_CFG_FIELD

BCH_DATA  *FS_NAND_CtrlrGen_BCH_ListFreePtr;


/*
*********************************************************************************************************
*                                       LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void              Init      (FS_ERR                  *p_err);

static  void             *Open      (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                                     void                    *p_ext_cfg,
                                     FS_ERR                  *p_err);

static  void              Close     (void                    *p_ext_data);

static  FS_NAND_PG_SIZE   Setup     (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                                     void                    *p_ext_data,
                                     FS_ERR                  *p_err);

static  void              ECC_Calc  (void                    *p_ext_data,
                                     void                    *p_sec_buf,
                                     void                    *p_oos_buf,
                                     FS_NAND_PG_SIZE          oos_size,
                                     FS_ERR                  *p_err);

static  void              ECC_Verify(void                    *p_ext_data,
                                     void                    *p_sec_buf,
                                     void                    *p_oos_buf,
                                     FS_NAND_PG_SIZE          oos_size,
                                     FS_ERR                  *p_err);

static  BCH_DATA         *DataGet   (void);

static  void              DataFree  (BCH_DATA                *p_ext_data);


/*
*********************************************************************************************************
*                               NAND GENERIC CTRLR SOFTWARE ECC EXTENSION
*********************************************************************************************************
*/

FS_NAND_CTRLR_GEN_EXT  FS_NAND_CtrlrGen_BCH =  {
    Init,                                                       /* Init().                                              */
    Open,                                                       /* Open().                                              */
    Close,                                                      /* Close().                                             */
    Setup,                                                      /* Setup().                                             */
    DEF_NULL,                                                   /* RdStatusChk().                                       */
    ECC_Calc,                                                   /* ECC_Calc().                                          */
    ECC_Verify,                                                 /* ECC_Verify().                                        */
};


/*
*********************************************************************************************************
*                                       LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                                Init()
*
* Description : Initialize NAND generic controller software ECC extension.
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function :
*               -----   Argument validated by caller.
*
*                           FS_ERR_NONE    Extension module successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  Init (FS_ERR  *p_err)
{
    CPU_SR_ALLOC();

    (void)p_err;
    CPU_CRITICAL_ENTER();
    FS_NAND_CtrlrGen_BCH_ListFreePtr = DEF_NULL;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                                Open()
*
* Description : Open extension module instance.
*
* Argument(s) : p_ctrlr_data    Pointer to NAND Controller Generic data structure.
*               ------------    Argument validated by caller.
*
*               p_ext_cfg       Pointer to software ECC extension configuration structure.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_MEM_ALLOC    Failed to allocate memory for extension data.
*                                   FS_ERR_NONE         Extension module instance successfully opened.
*
* Return(s)   : Pointer to extension data.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  *Open (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                     void                    *p_ext_cfg,
                     FS_ERR                  *p_err)
{
    FS_NAND_CTRLR_GEN_BCH_CFG  *p_bch_cfg;
    BCH_DATA                   *p_bch_data;


    (void)p_ctrlr_data;

#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE ARGS ------------------- */
    if (p_ext_cfg == DEF_NULL) {                                /* Validate cfg ptr.                                    */
        CPU_SW_EXCEPTION(DEF_NULL;);
    }
#endif


    p_bch_cfg = (FS_NAND_CTRLR_GEN_BCH_CFG *)p_ext_cfg;


    p_bch_data = DataGet();                                     /* --------------- ALLOC AND INIT DATA ---------------- */
    if (p_bch_data == DEF_NULL) {
       *p_err = FS_ERR_MEM_ALLOC;
        return (DEF_NULL);
    }

    if(p_bch_cfg->Ecc0 > 0xAu) {
        FS_TRACE_DBG(("(fs_dev_nand_ctrlr_bch) Open: ECC0 must be 0xA or smaller.\r\n"));
       *p_err  = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return (DEF_NULL);
    }

    if(p_bch_cfg->EccN > 0xAu) {
        FS_TRACE_DBG(("(fs_dev_nand_ctrlr_bch) Open: ECCN must be 0xA or smaller.\r\n"));
       *p_err  = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return (DEF_NULL);
    }

    if(p_bch_cfg->Ecc0 != p_bch_cfg->EccN) {
        FS_TRACE_DBG(("(fs_dev_nand_ctrlr_bch) Open: ECC0 must be equal to ECCN.\r\n"));
       *p_err  = FS_ERR_DEV_INVALID_LOW_PARAMS;
        return (DEF_NULL);
    }

    p_bch_data->Ecc0            = p_bch_cfg->Ecc0;
    p_bch_data->EccN            = p_bch_cfg->EccN;
    p_bch_data->EccRefreshThres = p_bch_cfg->EccThres;


    return (p_bch_data);
}


/*
*********************************************************************************************************
*                                                Close()
*
* Description : Close extension module instance.
*
* Argument(s) : p_ext_data  Pointer to extension data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_NONE     Extension module instance successfully closed.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  Close (void  *p_ext_data)
{
    DataFree((BCH_DATA *)p_ext_data);
}


/*
*********************************************************************************************************
*                                                Setup()
*
* Description : Setup software ECC extension module in accordance with generic controller's data.
*
* Argument(s) : p_ctrlr_data    Pointer to NAND generic controller data.
*               ------------    Argument validated by caller.
*
*               p_ext_data      Pointer to extension data.
*               ----------      Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_INVALID_LOW_PARAMS   Invalid low level parameters.
*
* Return(s)   : Size required storage space for ECC per sector.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  FS_NAND_PG_SIZE  Setup (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                                void                    *p_ext_data,
                                FS_ERR                  *p_err)
{
    BCH_DATA    *p_bch_data;
    CPU_INT32U   bit_cnt;
    CPU_INT32U   byte_cnt;


    p_bch_data = (BCH_DATA *)p_ext_data;


    p_bch_data->PgSize = p_ctrlr_data->PartDataPtr->PgSize;
    p_bch_data->SpareSize = p_ctrlr_data->PartDataPtr->SpareSize;

    p_bch_data->DataBlkSize = 512;
    p_bch_data->NbrBlk = p_bch_data->PgSize / p_bch_data->DataBlkSize;

    p_bch_data->EccWordCnt = p_bch_data->Ecc0 * 2;
    bit_cnt = (p_bch_data->Ecc0 * 2 * 13 * (p_bch_data->NbrBlk + 1));
    byte_cnt = bit_cnt / 8u;
    if(bit_cnt % 8 != 0u) {
        byte_cnt++;
    }

    *p_err = FS_ERR_NONE;

    return (byte_cnt);
}


/*
*********************************************************************************************************
*                                              ECC_Calc()
*
* Description : Calculate ECC code words for sector and OOS data.
*
* Argument(s) : p_ext_data  Pointer to extension data.
*
*               p_sec_buf   Pointer to sector data.
*
*               p_oos_buf   Pointer to OOS data.
*
*               oos_size    Size of OOS data to protect in octets, excluding storage space for ECC code words.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_DEV_INVALID_ECC  An error occured during ECC code words calculation.
*                               FS_ERR_NONE             ECC code words successfully calculated.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  ECC_Calc (void             *p_ext_data,
                        void             *p_sec_buf,
                        void             *p_oos_buf,
                        FS_NAND_PG_SIZE   oos_size,
                        FS_ERR           *p_err)
{
    BCH_DATA         *p_bch_data;
    FS_NAND_PG_SIZE   oos_chk_size_rem;
    CPU_DATA          ix;
    CPU_DATA          blk_ix;
    CPU_INT32U        temp;
    CPU_SIZE_T        offset_src_octet;
    CPU_DATA          offset_src_bit;
    CPU_SIZE_T        offset_dest_octet;
    CPU_DATA          offset_dest_bit;
    CPU_DATA          bit_ix;


    p_bch_data       = (BCH_DATA *)p_ext_data;

    oos_chk_size_rem = oos_size;

    iMX28_REG_BCH_CTRL = 0;
    iMX28_REG_BCH_CTRL_CLR = iMX28_BIT_BCH_COMPLETE_IRQ;

    iMX28_REG_BCH_LAYOUTSELECT = iMX28_BIT_BCH_LAYOUTSELECT(0, 0);


    iMX28_REG_BCH_FLASH0LAYOUT0 = iMX28_BIT_BCH_LAYOUT0_NBLOCKS(p_bch_data->NbrBlk)  |
                                  iMX28_BIT_BCH_LAYOUT0_METASIZE(oos_chk_size_rem) |
                                  iMX28_BIT_BCH_LAYOUT0_ECC0(p_bch_data->Ecc0)     |
                                  iMX28_BIT_BCH_LAYOUT0_DAT0SZ(0);

    iMX28_REG_BCH_FLASH0LAYOUT1 = iMX28_BIT_BCH_LAYOUT1_PAGESZ(p_bch_data->PgSize + p_bch_data->SpareSize) |
                                  iMX28_BIT_BCH_LAYOUT1_ECCN(p_bch_data->EccN)           |
                                  iMX28_BIT_BCH_LAYOUT1_DATANSZ(p_bch_data->DataBlkSize);

    iMX28_REG_BCH_ENCODEPTR     = (CPU_ADDR)p_bch_data->BufPtr;
    iMX28_REG_BCH_DATAPTR       = (CPU_ADDR)p_sec_buf;
    iMX28_REG_BCH_METAPTR       = (CPU_ADDR)p_oos_buf;

    iMX28_REG_BCH_CTRL          = iMX28_BIT_BCH_M2M_LAYOUT(0) |
                                  iMX28_BIT_BCH_M2M_ENCODE    |
                                  iMX28_BIT_BCH_M2M_ENABLE;

    while (DEF_BIT_IS_SET(iMX28_REG_BCH_CTRL, iMX28_BIT_BCH_COMPLETE_IRQ) == DEF_NO) {}


    offset_src_octet = oos_chk_size_rem;
    offset_src_bit   = 0;
    offset_dest_octet = oos_chk_size_rem;
    offset_dest_bit   = 0;

    for(blk_ix = 0; blk_ix < p_bch_data->NbrBlk + 1u; blk_ix++) {
        for(ix = 0; ix < p_bch_data->EccWordCnt; ix++) {
            temp = FSUtil_ValUnpack32( p_bch_data->BufPtr,
                                      &offset_src_octet,
                                      &offset_src_bit,
                                       13u);

            FSUtil_ValPack32((CPU_INT08U *)p_oos_buf,
                                          &offset_dest_octet,
                                          &offset_dest_bit,
                                           temp,
                                           13u);
        }

        bit_ix = offset_src_octet * 8 + offset_src_bit;
        bit_ix += p_bch_data->DataBlkSize * 8;
        offset_src_octet = bit_ix / 8;
        offset_src_bit   = bit_ix % 8;
    }

    *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             ECC_Verify()
*
* Description : Verify sector and OOS data against ECC code words and correct data if needed/possible.
*
* Argument(s) : p_ext_data  Pointer to extension data.
*
*               p_sec_buf   Pointer to sector data.
*
*               p_oos_buf   Pointer to OOS data.
*
*               oos_size    Size of OOS data to protect in octets, excluding storage space for ECC code words.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_ECC_CORRECTABLE      Sector and OOS data have been successfully corrected.
*                               FS_ERR_ECC_UNCORRECTABLE    Sector and OOS data are corrupted and can't be corrected.
*                               FS_ERR_INVALID_ARG          Invalid parameters have been passed to ECC module.
*                               FS_ERR_NONE                 Sector and OOS data successfully verified.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  ECC_Verify (void                        *p_ext_data,
                          void                        *p_sec_buf,
                          void                        *p_oos_buf,
                          FS_NAND_PG_SIZE              oos_size,
                          FS_ERR                      *p_err)
{
    BCH_DATA         *p_bch_data;
    FS_NAND_PG_SIZE   oos_chk_size_rem;
    CPU_DATA          ix;
    CPU_DATA          blk_ix;
    CPU_INT08U       *p_sec_buf_08;
    CPU_INT08U       *p_oos_buf_08;
    CPU_INT32U        temp;
    CPU_SIZE_T        offset_src_octet;
    CPU_DATA          offset_src_bit;
    CPU_SIZE_T        offset_dest_octet;
    CPU_DATA          offset_dest_bit;
    CPU_DATA          sec_ix;
    CPU_DATA          stat_offset;
    CPU_DATA          max_err_cnt;


    p_bch_data       = (BCH_DATA *)p_ext_data;
    p_sec_buf_08     = (CPU_INT08U *)p_sec_buf;
    p_oos_buf_08     = (CPU_INT08U *)p_oos_buf;

    oos_chk_size_rem = oos_size;


    offset_src_octet  = oos_chk_size_rem;
    offset_src_bit    = 0;
    offset_dest_octet = oos_chk_size_rem;
    offset_dest_bit   = 0;
    sec_ix            = 0;

    for(ix = 0; ix < oos_size; ix++) {
        p_bch_data->BufPtr[ix] = p_oos_buf_08[ix];
    }

    for(ix = 0; ix < p_bch_data->EccWordCnt; ix++) {
        temp = FSUtil_ValUnpack32((CPU_INT08U *)p_oos_buf,
                                               &offset_src_octet,
                                               &offset_src_bit,
                                                13u);

        FSUtil_ValPack32( p_bch_data->BufPtr,
                         &offset_dest_octet,
                         &offset_dest_bit,
                          temp,
                          13u);

    }

    for(blk_ix = 0; blk_ix < p_bch_data->NbrBlk; blk_ix++) {

        for(ix = sec_ix; ix < p_bch_data->DataBlkSize + sec_ix; ix++) {
            temp = p_sec_buf_08[ix];

            FSUtil_ValPack32( p_bch_data->BufPtr,
                             &offset_dest_octet,
                             &offset_dest_bit,
                              temp,
                              8u);
        }

        sec_ix += p_bch_data->DataBlkSize;

        for(ix = 0; ix < p_bch_data->EccWordCnt; ix++) {
            temp = FSUtil_ValUnpack32((CPU_INT08U *)p_oos_buf,
                                                   &offset_src_octet,
                                                   &offset_src_bit,
                                                    13u);

            FSUtil_ValPack32( p_bch_data->BufPtr,
                             &offset_dest_octet,
                             &offset_dest_bit,
                              temp,
                              13u);

        }

    }

    iMX28_REG_BCH_CTRL = 0;
    iMX28_REG_BCH_CTRL_CLR = iMX28_BIT_BCH_COMPLETE_IRQ;

    iMX28_REG_BCH_LAYOUTSELECT = iMX28_BIT_BCH_LAYOUTSELECT(0, 0);


    iMX28_REG_BCH_FLASH0LAYOUT0 = iMX28_BIT_BCH_LAYOUT0_NBLOCKS(p_bch_data->NbrBlk)  |
                                  iMX28_BIT_BCH_LAYOUT0_METASIZE(oos_chk_size_rem) |
                                  iMX28_BIT_BCH_LAYOUT0_ECC0(p_bch_data->Ecc0)     |
                                  iMX28_BIT_BCH_LAYOUT0_DAT0SZ(0);

    iMX28_REG_BCH_FLASH0LAYOUT1 = iMX28_BIT_BCH_LAYOUT1_PAGESZ(p_bch_data->PgSize + p_bch_data->SpareSize) |
                                  iMX28_BIT_BCH_LAYOUT1_ECCN(p_bch_data->EccN)           |
                                  iMX28_BIT_BCH_LAYOUT1_DATANSZ(p_bch_data->DataBlkSize);

    iMX28_REG_BCH_ENCODEPTR     = (CPU_ADDR)p_bch_data->BufPtr;
    iMX28_REG_BCH_DATAPTR       = (CPU_ADDR)p_sec_buf;
    iMX28_REG_BCH_METAPTR       = (CPU_ADDR)p_oos_buf;

    iMX28_REG_BCH_CTRL          = iMX28_BIT_BCH_M2M_LAYOUT(0) |
                                  iMX28_BIT_BCH_M2M_ENABLE;


    while (DEF_BIT_IS_SET(iMX28_REG_BCH_CTRL, iMX28_BIT_BCH_COMPLETE_IRQ) == DEF_NO) {}

    stat_offset = (oos_size + p_bch_data->EccByteCnt) / 4u;
    if((oos_size + p_bch_data->EccByteCnt) % 4u != 0u) {
        stat_offset++;
    }

    stat_offset = stat_offset * 4u;
    max_err_cnt = 0u;

    for(ix = 0u; ix < p_bch_data->NbrBlk + 1u; ix++) {
        if(p_oos_buf_08[ix + stat_offset] == 0xFE) {
//            FS_TRACE_DBG(("fs_dev_nand_ctrlr_bch) ECC_Verify: Block %u uncorrectable. \r\n", ix));
        } else if((p_oos_buf_08[ix + stat_offset] <= 20u) && (p_oos_buf_08[ix + stat_offset] != 0)) {
            if(p_oos_buf_08[ix + stat_offset] > max_err_cnt) {
                max_err_cnt = p_oos_buf_08[ix + stat_offset];
            }

//            FS_TRACE_DBG(("fs_dev_nand_ctrlr_bch) ECC_Verify: Block %u corrected %u bits. \r\n", ix, p_oos_buf_08[ix + stat_offset]));
        }
    }

    if(DEF_BIT_IS_SET(iMX28_REG_BCH_STATUS0, 1 << 2) == DEF_YES)  {
        *p_err = FS_ERR_ECC_UNCORR;
    } else if(DEF_BIT_IS_SET(iMX28_REG_BCH_STATUS0, 1 << 3) == DEF_YES) {
        if(max_err_cnt >= p_bch_data->EccRefreshThres) {
            *p_err = FS_ERR_ECC_CRITICAL_CORR;
        } else {
            *p_err = FS_ERR_ECC_CORR;
        }
    } else {
        *p_err = FS_ERR_NONE;
    }


}


/*
*********************************************************************************************************
*                                               DataGet()
*
* Description : Allocate & initialize extension data.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to extension data, if NO errors.
*               DEF_NULL,                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  BCH_DATA  *DataGet (void)
{
    BCH_DATA       *p_bch_data;
    CPU_SIZE_T      octets_reqd;
    LIB_ERR         alloc_err;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_NAND_CtrlrGen_BCH_ListFreePtr == (BCH_DATA *)0) {
        p_bch_data = (BCH_DATA *)Mem_HeapAlloc(sizeof(FS_NAND_CTRLR_GEN_DATA),
                                               sizeof(CPU_INT32U),
                                               &octets_reqd,
                                               &alloc_err);
        if (p_bch_data == DEF_NULL) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("(fs_nand_ctrlr_gen_soft_ecc) DataGet(): Could not alloc mem for "
                          "NAND Generic Ctrlr BCH data: %d octets required.\r\n",
                          octets_reqd));
            return (DEF_NULL);
        }

        p_bch_data->BufPtr = (CPU_INT08U *)Mem_HeapAlloc(4096+224,
                                           sizeof(CPU_INT32U),
                                           &octets_reqd,
                                           &alloc_err);
        if (p_bch_data == DEF_NULL) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("(fs_nand_ctrlr_gen_soft_ecc) DataGet(): Could not alloc mem for "
                          "NAND Generic Ctrlr BCH buffer: %d octets required.\r\n",
                          octets_reqd));
            return (DEF_NULL);
        }
        (void)alloc_err;

    } else {
        p_bch_data                       = FS_NAND_CtrlrGen_BCH_ListFreePtr;
        FS_NAND_CtrlrGen_BCH_ListFreePtr = FS_NAND_CtrlrGen_BCH_ListFreePtr->NextPtr;
    }




    CPU_CRITICAL_EXIT();

    return (p_bch_data);
}


/*
*********************************************************************************************************
*                                              DataFree()
*
* Description : Free extension data.
*
* Argument(s) : p_ext_data  Pointer to extension data.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  DataFree (BCH_DATA  *p_ext_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
                                                                /* Add to free pool.                                    */
    p_ext_data->NextPtr                  = FS_NAND_CtrlrGen_BCH_ListFreePtr;
    FS_NAND_CtrlrGen_BCH_ListFreePtr = p_ext_data;
    CPU_CRITICAL_EXIT();
}

