/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                               FILE SYSTEM APPLICATION INITIALIZATION
*
*                                              TEMPLATE
*
* Filename : fs_app.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "fs_app.h"
#include  <Source/fs.h>
#include  <Source/fs_dev.h>
#include  <Source/fs_vol.h>


/*
*********************************************************************************************************
*                                               ENABLE
*********************************************************************************************************
*/

#if (APP_CFG_FS_EN == DEF_ENABLED)


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

#if (APP_CFG_FS_NAND_EN == DEF_ENABLED)

#ifdef APP_CFG_FS_NAND_CTRLR_GEN_EXT
extern  FS_NAND_CTRLR_GEN_EXT  APP_CFG_FS_NAND_CTRLR_GEN_EXT;

#ifdef APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR
extern  void  *APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR
#endif  /* APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR */

#endif  /* APP_CFG_FS_NAND_CTRLR_GEN_EXT */

#endif

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (APP_CFG_FS_RAM_EN == DEF_ENABLED)
static  CPU_INT32U  App_FS_RAM_Disk[APP_CFG_FS_RAM_SEC_SIZE * APP_CFG_FS_RAM_NBR_SECS / 4];
#endif

#if (APP_CFG_FS_NAND_EN == DEF_ENABLED)

#ifdef APP_CFG_FS_NAND_FREE_SPARE_MAP

#if (defined (APP_CFG_FS_NAND_FREE_SPARE_START) || defined (APP_CFG_FS_NAND_FREE_SPARE_END))
#error  "APP_CFG_FS_NAND_FREE_SPARE_MAP          conflicting #define in 'app_cfg.h'             "
#error  "                                       [conflicts w/APP_CFG_FS_NAND_FREE_SPARE_START]  "
#error  "                                       [       && w/APP_CFG_FS_NAND_FREE_SPARE_END  ]  "
#endif /* (defined APP_CFG_FS_NAND_FREE_SPARE_START) || (defined APP_CFG_FS_NAND_FREE_SPARE_END) */

static  const  FS_NAND_FREE_SPARE_DATA  App_FS_NAND_FreeSpareMap[] = APP_CFG_FS_NAND_FREE_SPARE_MAP;

#else /* !defined APP_CFG_FS_NAND_FREE_SPARE_MAP */
static  const  FS_NAND_FREE_SPARE_DATA  App_FS_NAND_FreeSpareMap[] = {{ APP_CFG_FS_NAND_FREE_SPARE_START, APP_CFG_FS_NAND_FREE_SPARE_LEN},
                                                                      {              (FS_NAND_PG_SIZE)-1,            (FS_NAND_PG_SIZE)-1}};
#endif /* APP_CFG_FS_NAND_FREE_SPARE_MAP */

#if (APP_CFG_FS_NAND_CTRLR_IMPL == CTRLR_GEN)
extern  const  FS_NAND_CTRLR_GEN_BSP_API  APP_CFG_FS_NAND_BSP;
#endif

#endif /* APP_CFG_FS_NAND_EN == DEF_ENABLED */

static  const  FS_CFG  App_FS_Cfg = {
    APP_CFG_FS_DEV_CNT,             /* DevCnt           */
    APP_CFG_FS_VOL_CNT,             /* VolCnt           */
    APP_CFG_FS_FILE_CNT,            /* FileCnt          */
    APP_CFG_FS_DIR_CNT,             /* DirCnt           */
    APP_CFG_FS_BUF_CNT,             /* BufCnt           */
    APP_CFG_FS_DEV_DRV_CNT,         /* DevDrvCnt        */
    APP_CFG_FS_MAX_SEC_SIZE         /* MaxSecSize       */
};


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (APP_CFG_FS_MSC_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddMSC    (void);
#endif

#if (APP_CFG_FS_IDE_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddIDE    (void);
#endif

#if (APP_CFG_FS_NAND_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddNAND   (void);
#endif

#if (APP_CFG_FS_NOR_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddNOR    (void);
#endif

#if (APP_CFG_FS_RAM_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddRAM    (void);
#endif

#if (APP_CFG_FS_SD_CARD_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddSD_Card(void);
#endif

#if (APP_CFG_FS_SD_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddSD_SPI (void);
#endif


/*
*********************************************************************************************************
*                                      LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            App_FS_Init()
*
* Description : Initialize uC/FS.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if file system suite was initialized.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) MSC device/volumes will be opened/closed dynamically by the USB Host MSC notification
*                   callback.
*********************************************************************************************************
*/

CPU_BOOLEAN  App_FS_Init (void)
{
    FS_ERR       err;
    CPU_BOOLEAN  ok;


                                                                /* ---------------------- INIT FS --------------------- */
    APP_TRACE_DBG(("\r\n"));
    APP_TRACE_DBG(("===================================================================\r\n"));
    APP_TRACE_DBG(("=                        FS INITIALIZATION                        =\r\n"));
    APP_TRACE_DBG(("===================================================================\r\n"));
    APP_TRACE_DBG(("Initializing FS...\r\n"));
    err = FS_Init((FS_CFG *)&App_FS_Cfg);
    if (err != FS_ERR_NONE) {
        APP_TRACE_DBG(("...init failed w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


#if (APP_CFG_FS_MSC_EN == DEF_ENABLED)                          /* ------------------ ADD MSC DRIVER ------------------ */
    ok = App_FS_AddMSC();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_IDE_EN == DEF_ENABLED)                          /* ---------------- ADD/OPEN IDE VOLUME --------------- */
    ok = App_FS_AddIDE();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_NAND_EN == DEF_ENABLED)                         /* ---------------- ADD/OPEN NAND VOLUME -------------- */
    ok = App_FS_AddNAND();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_NOR_EN == DEF_ENABLED)                          /* ---------------- ADD/OPEN NOR VOLUME --------------- */
    ok = App_FS_AddNOR();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_RAM_EN == DEF_ENABLED)                          /* ------------- ADD/OPEN RAM DISK VOLUME ------------- */
    ok = App_FS_AddRAM();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_SD_CARD_EN == DEF_ENABLED)                      /* --------- ADD/OPEN SD/MMC (CARDMODE) VOLUME -------- */
    ok = App_FS_AddSD_Card();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif


#if (APP_CFG_FS_SD_EN == DEF_ENABLED)                           /* ----------- ADD/OPEN SD/MMC (SPI) VOLUME ----------- */
    ok = App_FS_AddSD_SPI();

    if (ok != DEF_OK) {
        return (DEF_FAIL);
    }
#endif

    APP_TRACE_DBG(("...init succeeded.\r\n"));
    APP_TRACE_DBG(("===================================================================\r\n"));
    APP_TRACE_DBG(("===================================================================\r\n"));
    APP_TRACE_DBG(("\r\n"));

    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                           App_FS_AddMSC()
*
* Description : Add MSC driver.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (APP_CFG_FS_MSC_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddMSC (void)
{
    FS_ERR  err;

    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding MSC device driver ...\r\n"));
    FS_DevDrvAdd((FS_DEV_API *)&FSDev_MSC,                      /* Add MSC device driver (see Note #1).                 */
                 (FS_ERR     *)&err);
    if (err != FS_ERR_NONE) {
        APP_TRACE_DBG(("    ... could not add MSC driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }
    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                           App_FS_AddIDE()
*
* Description : Add IDE/CF volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A device error will be returned from FSDev_Open() & FSVol_Open() if the card is not
*                   present or malfunctions.  The device or volume, respectively, is still open, though
*                   the device & volume information will need to be refreshed before the medium is
*                   accessible.
*
*               (2) A volume error will be returned from FSVol_Open() if no valid file system is found
*                   on the card.  It may need to be formatted.
*********************************************************************************************************
*/

#if (APP_CFG_FS_IDE_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddIDE (void)
{
    FS_ERR  err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening IDE volume \"ide:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FSDev_IDE,                      /* Add IDE/CF device driver.                            */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN DEV --------------------- */
    FSDev_Open("ide:0:", (void *)0, &err);                      /* Open device "ide:0:".                                */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened device.\r\n"));
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             APP_TRACE_DBG(("    ...opened device (not present).\r\n"));
             return (DEF_FAIL);


        default:
             APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("ide:0:", "ide:0:", 0, &err);                    /* Open volume "ide:0:".                                */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened volume (unmounted).\r\n"));
             return (DEF_FAIL);


        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                           App_FS_AddNAND()
*
* Description : Add NAND volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) (a) A device error will be returned from FSDev_Open(), FSVol_Open(), FSDev_NOR_LowFmt()
*                       or FSVol_Fmt() if the device malfunctions.  The device may still be open;
*                       however, since NOR flash are fixed devices assumed to be always-functioning, an
*                       application change will be necessary to fully address the problem.
*
*                   (b) A low-level format invalid error will be returned from FSDev_Open() if the device
*                        is not low-level formatted.
*
*               (2) A partition-not-found error will be returned from FSVol_Open() if the device is not
*                   formatted (this will always be the situation immediately after FSDev_NAND_LowFmt()).
*********************************************************************************************************
*/

#if (APP_CFG_FS_NAND_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddNAND (void)
{
    FS_NAND_CFG                     nand_cfg     = FS_NAND_DfltCfg;
    FS_NAND_CTRLR_GEN_CFG           ctrlr_cfg    = FS_NAND_CtrlrGen_DfltCfg;
    FS_NAND_CTRLR_GEN_SOFT_ECC_CFG  soft_ecc_cfg = FS_NAND_CtrlrGen_SoftECC_DfltCfg;
#if   (APP_CFG_FS_NAND_PART_TYPE == ONFI)
    FS_NAND_PART_ONFI_CFG           part_cfg     = FS_NAND_PartONFI_DfltCfg;
#elif (APP_CFG_FS_NAND_PART_TYPE == STATIC)
    FS_NAND_PART_STATIC_CFG         part_cfg     = FS_NAND_PartStatic_DfltCfg;
#endif
    FS_ERR                          err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening NAND volume \"nand:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FS_NAND,                        /* Add NAND device driver.                              */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }

                                                                /* ------------------- CFG NAND FTL ------------------- */
    nand_cfg.CtrlrPtr          = (FS_NAND_CTRLR_API *)&FS_NAND_CtrlrGen;
    nand_cfg.CtrlrCfgPtr       = &ctrlr_cfg;

#if   (APP_CFG_FS_NAND_PART_TYPE == ONFI)
    nand_cfg.PartPtr           = (FS_NAND_PART_API *)&FS_NAND_PartONFI;
#elif (APP_CFG_FS_NAND_PART_TYPE == STATIC)
    nand_cfg.PartPtr           = (FS_NAND_PART_API *)&FS_NAND_PartStatic;
#endif /* APP_CFG_FS_NAND_PART_TYPE */
    nand_cfg.PartCfgPtr        = &part_cfg;

    nand_cfg.BSPPtr            = (FS_NAND_CTRLR_GEN_BSP_API *)&APP_CFG_FS_NAND_BSP;

#ifdef APP_CFG_FS_NAND_UB_CNT_MAX
    nand_cfg.UB_CntMax         = APP_CFG_FS_NAND_UB_CNT_MAX;
#endif /* APP_CFG_FS_NAND_UB_CNT_MAX */

                                                                /* ---------------- CFG NAND GEN CTRLR ---------------- */
#ifdef APP_CFG_FS_NAND_CTRLR_GEN_EXT
    ctrlr_cfg.CtrlrExt         = &APP_CFG_FS_NAND_CTRLR_GEN_EXT;
#ifdef APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR
    ctrlr_cfg.CtrlrExtCfg      = APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR;
#else
    ctrlr_cfg.CtrlrExtCfg      = DEF_NULL;
#endif /* APP_CFG_FS_NAND_CTRLR_GEN_EXT_CFG_PTR */
#else
    ctrlr_cfg.CtrlrExt         = &FS_NAND_CtrlrGen_SoftECC;
    ctrlr_cfg.CtrlrExtCfg      = &soft_ecc_cfg;

                                                                /* --------- CFG NAND GEN CTRLR SOFT ECC EXT ---------- */
    soft_ecc_cfg.ECC_ModulePtr = &Hamming_ECC;
#endif /* APP_CFG_FS_NAND_CTRLR_GEN_EXT */

                                                                /* ------------------ CFG NAND PART ------------------- */
    part_cfg.FreeSpareMap      = (FS_NAND_FREE_SPARE_DATA *)App_FS_NAND_FreeSpareMap;
#if (APP_CFG_FS_NAND_PART_TYPE == STATIC)
    part_cfg.BlkCnt            = APP_CFG_FS_NAND_BLK_CNT;
    part_cfg.PgPerBlk          = APP_CFG_FS_NAND_PG_PER_BLK;
    part_cfg.PgSize            = APP_CFG_FS_NAND_PG_SIZE;
    part_cfg.SpareSize         = APP_CFG_FS_NAND_SPARE_SIZE;
#ifdef APP_CFG_FS_NAND_NBR_PGM_PER_PG
    part_cfg.NbrPgmPerPg       = APP_CFG_FS_NAND_NBR_PGM_PER_PG;
#endif
    part_cfg.BusWidth          = APP_CFG_FS_NAND_BUS_WIDTH;
    part_cfg.ECC_CodewordSize  = APP_CFG_FS_NAND_ECC_CODEWORD_SIZE;
    part_cfg.ECC_NbrCorrBits   = APP_CFG_FS_NAND_ECC_NBR_CORR_BITS;
    part_cfg.DefectMarkType    = APP_CFG_FS_NAND_DEFECT_MARK_TYPE;
    part_cfg.MaxBadBlkCnt      = APP_CFG_FS_NAND_MAX_BAD_BLK_CNT;
    part_cfg.MaxBlkErase       = APP_CFG_FS_NAND_MAX_BLK_ERASE;
#endif

                                                                /* --------------------- OPEN DEV --------------------- */



    FSDev_Open("nand:0:", (void *)&nand_cfg, &err);             /* Open device "nand:0:".                               */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened device.\r\n"));
             break;


        case FS_ERR_DEV_INVALID_LOW_FMT:                        /* Low fmt invalid (see Note #1b).                      */
             APP_TRACE_DBG(("    ...opened device (not low-level formatted).\r\n"));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FS_NAND_LowFmt("nand:0:", &err);
#endif
             if (err != FS_ERR_NONE) {
                APP_TRACE_DBG(("    ...low-level format failed w/err = %d.\r\n", err));
                return (DEF_FAIL);
             }
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1a).                         */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        default:
             APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("nand:0:", "nand:0:", 0, &err);                  /* Open volume "nand:0:".                               */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;


        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened device (not formatted).\r\n"));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FSVol_Fmt("nand:0:", (void *)0, &err);
#endif
             if (err != FS_ERR_NONE) {
                APP_TRACE_DBG(("    ...format failed w/err = %d.\r\n", err));
                return (DEF_FAIL);
             }
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1a).                         */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             APP_TRACE_DBG(("    ...opened volume (unmounted) w/err = %d.\r\n", err));
             return (DEF_FAIL);


        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                           App_FS_AddNOR()
*
* Description : Add NOR volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) (a) A device error will be returned from FSDev_Open(), FSVol_Open(), FSDev_NOR_LowFmt()
*                       or FSVol_Fmt() if the device malfunctions.  The device may still be open;
*                       however, since NOR flash are fixed devices assumed to be always-functioning, an
*                       application change will be necessary to fully address the problem.
*
*                   (b) A low-level format invalid error will be returned from FSDev_Open() if the device
*                        is not low-level formatted.
*
*               (2) A partition-not-found error will be returned from FSVol_Open() if the device is not
*                   formatted (this will always be the situation immediately after FSDev_NOR_LowFmt()).
*********************************************************************************************************
*/

#if (APP_CFG_FS_NOR_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddNOR (void)
{
    FS_DEV_NOR_CFG  nor_cfg;
    FS_ERR          err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening NOR volume \"nor:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FSDev_NOR,                      /* Add NOR device driver.                               */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN DEV --------------------- */
    nor_cfg.AddrBase         =  APP_CFG_FS_NOR_ADDR_BASE;
    nor_cfg.RegionNbr        =  APP_CFG_FS_NOR_REGION_NBR;

    nor_cfg.AddrStart        =  APP_CFG_FS_NOR_ADDR_START;
    nor_cfg.DevSize          =  APP_CFG_FS_NOR_DEV_SIZE;
    nor_cfg.SecSize          =  APP_CFG_FS_NOR_SEC_SIZE;
    nor_cfg.PctRsvd          =  APP_CFG_FS_NOR_PCT_RSVD;
    nor_cfg.EraseCntDiffTh   =  APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH;

    nor_cfg.PhyPtr           = (FS_DEV_NOR_PHY_API *)APP_CFG_FS_NOR_PHY_PTR;

    nor_cfg.BusWidth         =  APP_CFG_FS_NOR_BUS_WIDTH;
    nor_cfg.BusWidthMax      =  APP_CFG_FS_NOR_BUS_WIDTH_MAX;
    nor_cfg.PhyDevCnt        =  APP_CFG_FS_NOR_PHY_DEV_CNT;
    nor_cfg.MaxClkFreq       =  APP_CFG_FS_NOR_MAX_CLK_FREQ;

    FSDev_Open("nor:0:", (void *)&nor_cfg, &err);               /* Open device "nor:0:".                                */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened device.\r\n"));
             break;


        case FS_ERR_DEV_INVALID_LOW_FMT:                        /* Low fmt invalid (see Note #1b).                      */
             APP_TRACE_DBG(("    ...opened device (not low-level formatted).\r\n"));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FSDev_NOR_LowFmt("nor:0:", &err);
#endif
             if (err != FS_ERR_NONE) {
                APP_TRACE_DBG(("    ...low-level format failed.\r\n"));
                return (DEF_FAIL);
             }
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1a).                         */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        default:
             APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("nor:0:", "nor:0:", 0, &err);                    /* Open volume "nor:0:".                                */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;


        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened device (not formatted).\r\n"));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FSVol_Fmt("nor:0:", (void *)0, &err);
#endif
             if (err != FS_ERR_NONE) {
                APP_TRACE_DBG(("    ...format failed.\r\n"));
                return (DEF_FAIL);
             }
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1a).                         */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             APP_TRACE_DBG(("    ...opened volume (unmounted).\r\n"));
             return (DEF_FAIL);


        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                           App_FS_AddRAM()
*
* Description : Add RAM disk volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A partition-not-found error will be returned from FSVol_Open() if the device is not
*                   formatted (for a RAM disk, this will typically be the situation immediately after
*                   FSDev_Open()).
*********************************************************************************************************
*/

#if (APP_CFG_FS_RAM_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddRAM (void)
{
    FS_DEV_RAM_CFG  ram_cfg;
    FS_ERR          err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening RAM disk volume \"ram:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FSDev_RAM,                      /* Add RAM disk driver.                                 */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN DEV --------------------- */
                                                                /* Assign RAM disk configuration ...                    */
    ram_cfg.SecSize =  APP_CFG_FS_RAM_SEC_SIZE;                 /* ... (a) sector size           ...                    */
    ram_cfg.Size    =  APP_CFG_FS_RAM_NBR_SECS;                 /* ... (b) disk size (in sectors)...                    */
    ram_cfg.DiskPtr = (void *)&App_FS_RAM_Disk[0];              /* ... (c) pointer to disk RAM.                         */

    FSDev_Open("ram:0:", (void *)&ram_cfg, &err);               /* Open device "ram:0:".                                */
    if (err != FS_ERR_NONE) {
        APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
        return (DEF_FAIL);
    }
    APP_TRACE_DBG(("    ...opened device.\r\n"));


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("ram:0:", "ram:0:", 0, &err);                    /* Open volume "ram:0:".                                */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;


        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened device (not formatted).\r\n"));
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
             FSVol_Fmt("ram:0:", (void *)0, &err);
#endif
             if (err != FS_ERR_NONE) {
                APP_TRACE_DBG(("    ...format failed.\r\n"));
                return (DEF_FAIL);
             }
             break;


        case FS_ERR_DEV:
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                         App_FS_AddSD_Card()
*
* Description : Add SD/MMC (CardMode) volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A device error will be returned from FSDev_Open() & FSVol_Open() if the card is not
*                   present or malfunctions.  The device or volume, respectively, is still open, though
*                   the device & volume information will need to be refreshed before the medium is
*                   accessible.
*
*               (2) A volume error will be returned from FSVol_Open() if no valid file system is found
*                   on the card.  It may need to be formatted.
*********************************************************************************************************
*/

#if (APP_CFG_FS_SD_CARD_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddSD_Card (void)
{
    FS_ERR  err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening SD/MMC (CardMode) volume \"sdcard:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FSDev_SD_Card,                  /* Add SD/MMC (CardMode) device driver.                 */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN DEV --------------------- */
    FSDev_Open("sdcard:0:", (void *)0, &err);                   /* Open device "sdcard:0:".                             */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened device.\r\n"));
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             APP_TRACE_DBG(("    ...opened device (not present).\r\n"));
             break;


        default:
             APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("sdcard:0:", "sdcard:0:", 0, &err);              /* Open volume "sdcard:0:".                             */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;

        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened volume (unmounted).\r\n"));
             return (DEF_FAIL);

        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                         App_FS_AddSD_SPI()
*
* Description : Add SD/MMC (SPI) volume.
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   if volume opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) A device error will be returned from FSDev_Open() & FSVol_Open() if the card is not
*                   present or malfunctions.  The device or volume, respectively, is still open, though
*                   the device & volume information will need to be refreshed before the medium is
*                   accessible.
*
*               (2) A volume error will be returned from FSVol_Open() if no valid file system is found
*                   on the card.  It may need to be formatted.
*********************************************************************************************************
*/

#if (APP_CFG_FS_SD_EN == DEF_ENABLED)
static  CPU_BOOLEAN  App_FS_AddSD_SPI (void)
{
    FS_ERR  err;


    APP_TRACE_DBG(("    ===========================================================    \r\n"));
    APP_TRACE_DBG(("    Adding/opening SD/MMC (SPI) volume \"sd:0:\"...\r\n"));

    FS_DevDrvAdd((FS_DEV_API *)&FSDev_SD_SPI,                   /* Add SD/MMC (SPI) device driver.                      */
                 (FS_ERR     *)&err);
    if ((err != FS_ERR_NONE) &&
        (err != FS_ERR_DEV_DRV_ALREADY_ADDED)) {
        APP_TRACE_DBG(("    ...could not add driver w/err = %d\r\n\r\n", err));
        return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN DEV --------------------- */
    FSDev_Open("sd:0:", (void *)0, &err);                       /* Open device "sd:0:".                                 */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened device.\r\n"));
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
             APP_TRACE_DBG(("    ...opened device (not present).\r\n"));
             break;


        default:
             APP_TRACE_DBG(("    ...opening device failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }


                                                                /* --------------------- OPEN VOL --------------------- */
    FSVol_Open("sd:0:", "sd:0:", 0u, &err);                     /* Open volume "sd:0:".                                 */
    switch (err) {
        case FS_ERR_NONE:
             APP_TRACE_DBG(("    ...opened volume (mounted).\r\n"));
             break;


        case FS_ERR_DEV:                                        /* Device error (see Note #1).                          */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_PARTITION_NOT_FOUND:                        /* Volume error (see Note #2).                          */
             APP_TRACE_DBG(("    ...opened volume (unmounted).\r\n"));
             return (DEF_FAIL);


        default:
             APP_TRACE_DBG(("    ...opening volume failed w/err = %d.\r\n\r\n", err));
             return (DEF_FAIL);
    }

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                             ENABLE END
*********************************************************************************************************
*/

#endif
