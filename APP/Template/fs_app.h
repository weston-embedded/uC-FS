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
* Filename : fs_app.h
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_APP_MODULE_PRESENT
#define  FS_APP_MODULE_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_APP_MODULE
#define  FS_APP_EXT
#else
#define  FS_APP_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <app_cfg.h>
#include  <Source/fs_util.h>


/*
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*/

#ifndef  APP_CFG_FS_EN
#define  APP_CFG_FS_EN                          DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_IDE_EN
#define  APP_CFG_FS_IDE_EN                      DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_MSC_EN
#define  APP_CFG_FS_MSC_EN                      DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_NAND_EN
#define  APP_CFG_FS_NAND_EN                     DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_NOR_EN
#define  APP_CFG_FS_NOR_EN                      DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_RAM_EN
#define  APP_CFG_FS_RAM_EN                      DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_SD_CARD_EN
#define  APP_CFG_FS_SD_CARD_EN                  DEF_DISABLED
#endif

#ifndef  APP_CFG_FS_SD_EN
#define  APP_CFG_FS_SD_EN                       DEF_DISABLED
#endif


/*
*********************************************************************************************************
*                                      CONDITIONAL INCLUDE FILES
*********************************************************************************************************
*/

#if (APP_CFG_FS_EN         == DEF_ENABLED)
#include  <Source/fs.h>

#if (APP_CFG_FS_IDE_EN     == DEF_ENABLED)
#include  <Dev/IDE/fs_dev_ide.h>
#endif

#if (APP_CFG_FS_MSC_EN     == DEF_ENABLED)
#include  <Dev/MSC/fs_dev_msc.h>
#endif

#if (APP_CFG_FS_NAND_EN    == DEF_ENABLED)
#include  <Dev/NAND/fs_dev_nand.h>
#include  <Dev/NAND/Ctrlr/fs_dev_nand_ctrlr_gen.h>
#include  <Dev/NAND/Ctrlr/GenExt/fs_dev_nand_ctrlr_gen_soft_ecc.h>
#include  <Dev/NAND/Part/fs_dev_nand_part_onfi.h>
#include  <Dev/NAND/Part/fs_dev_nand_part_static.h>
#include  <Source/ecc_hamming.h>
#endif

#if (APP_CFG_FS_NOR_EN     == DEF_ENABLED)
#include  <Dev/NOR/fs_dev_nor.h>
#endif

#if (APP_CFG_FS_RAM_EN     == DEF_ENABLED)
#include  <Dev/RAMDisk/fs_dev_ramdisk.h>
#endif

#if (APP_CFG_FS_SD_CARD_EN == DEF_ENABLED)
#include  <Dev/SD/Card/fs_dev_sd_card.h>
#endif

#if (APP_CFG_FS_SD_EN      == DEF_ENABLED)
#include  <Dev/SD/SPI/fs_dev_sd_spi.h>
#endif
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/* APP_CFG_FS_NAND_PART_TYPE #DEFINES */
#define  ONFI    1
#define  STATIC  2

/* APP_CFG_FS_NAND_CTRLR_IMPL #DEFINES */
#define  CTRLR_GEN  1


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#if (APP_CFG_FS_EN == DEF_ENABLED)
CPU_BOOLEAN  App_FS_Init(void);
#endif


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/
                                                                /* ----------------- FS CONFIGURATION ----------------- */
#ifndef  APP_CFG_FS_EN
#error  "APP_CFG_FS_EN                            not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_EN != DEF_DISABLED) && \
        (APP_CFG_FS_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_EN                      illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "




#elif   (APP_CFG_FS_EN == DEF_ENABLED)
                                                                /* Device count.                                        */
#ifndef  APP_CFG_FS_DEV_CNT
#error  "APP_CFG_FS_DEV_CNT                       not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_DEV_CNT < 1u)
#error  "APP_CFG_FS_DEV_CNT                 illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif


                                                                /* Volume count.                                        */
#ifndef  APP_CFG_FS_VOL_CNT
#error  "APP_CFG_FS_VOL_CNT                       not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_VOL_CNT < 1u)
#error  "APP_CFG_FS_VOL_CNT                 illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif


                                                                /* File count.                                          */
#ifndef  APP_CFG_FS_FILE_CNT
#error  "APP_CFG_FS_FILE_CNT                      not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_FILE_CNT < 1u)
#error  "APP_CFG_FS_FILE_CNT                illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif


                                                                /* Directory count.                                     */
#ifndef  APP_CFG_FS_DIR_CNT
#error  "APP_CFG_FS_DIR_CNT                       not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0]                              "

#elif   (APP_CFG_FS_DIR_CNT < 0u)
#error  "APP_CFG_FS_DIR_CNT                 illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0]                              "
#endif


                                                                /* Buffer count.                                        */
#ifndef  APP_CFG_FS_BUF_CNT
#error  "APP_CFG_FS_BUF_CNT                       not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_BUF_CNT < (2u * APP_CFG_FS_VOL_CNT))
#error  "APP_CFG_FS_BUF_CNT                 illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif


                                                                /* Device driver count.                                 */
#ifndef  APP_CFG_FS_DEV_DRV_CNT
#error  "APP_CFG_FS_DEV_DRV_CNT                   not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_DEV_DRV_CNT < 1u)
#error  "APP_CFG_FS_DEV_DRV_CNT             illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif


                                                                /* Maximum sector size.                                 */
#ifndef  APP_CFG_FS_MAX_SEC_SIZE
#error  "APP_CFG_FS_MAX_SEC_SIZE                  not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "

#elif  ((APP_CFG_FS_MAX_SEC_SIZE !=  512u) && \
        (APP_CFG_FS_MAX_SEC_SIZE != 1024u) && \
        (APP_CFG_FS_MAX_SEC_SIZE != 2048u) && \
        (APP_CFG_FS_MAX_SEC_SIZE != 4096u))
#error  "APP_CFG_FS_MAX_SEC_SIZE            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "
#endif


                                                                /* ------------- IDE DRIVER CONFIGURATION ------------- */
#ifndef  APP_CFG_FS_IDE_EN
#error  "APP_CFG_FS_IDE_EN                        not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_IDE_EN != DEF_DISABLED) && \
        (APP_CFG_FS_IDE_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_IDE_EN                  illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "
#endif


                                                                /* ------------- MSC DRIVER CONFIGURATION ------------- */
#ifndef  APP_CFG_FS_MSC_EN
#error  "APP_CFG_FS_MSC_EN                        not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_MSC_EN != DEF_DISABLED) && \
        (APP_CFG_FS_MSC_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_MSC_EN                  illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "
#endif


                                                                /* ------------- NAND DRIVER CONFIGURATION ------------ */
#ifndef  APP_CFG_FS_NAND_EN
#error  "APP_CFG_FS_NAND_EN                       not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_NAND_EN != DEF_DISABLED) && \
        (APP_CFG_FS_NAND_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_NAND_EN                 illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "


#elif   (APP_CFG_FS_NAND_EN == DEF_ENABLED)

#if     (APP_CFG_FS_NAND_CTRLR_IMPL != CTRLR_GEN)
#error  "APP_CFG_FS_NAND_CTRLR_IMPL         illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be CTRLR_GEN]                         "
#endif

#if    ((APP_CFG_FS_NAND_PART_TYPE != ONFI) && \
        (APP_CFG_FS_NAND_PART_TYPE != STATIC))
#error  "APP_CFG_FS_NAND_PART_TYPE          illegally or not #define'd in 'app_cfg.h'   "
#error  "                                   [MUST be ONFI  ]"
#error  "                                   [     || STATIC]"
#endif

#ifndef  APP_CFG_FS_NAND_FREE_SPARE_MAP
#ifndef  APP_CFG_FS_NAND_FREE_SPARE_START
#error  "APP_CFG_FS_NAND_FREE_SPARE_START   not #define'd in 'app_cfg.h'                "
#endif

#ifndef  APP_CFG_FS_NAND_FREE_SPARE_LEN
#error  "APP_CFG_FS_NAND_FREE_SPARE_LEN     not #define'd in 'app_cfg.h'                "
#endif
#endif  /* APP_CFG_FS_NAND_FREE_SPARE_MAP */

#if     (APP_CFG_FS_NAND_PART_TYPE == STATIC)

#ifndef  APP_CFG_FS_NAND_BLK_CNT
#error  "APP_CFG_FS_NAND_BLK_CNT            not #define'd in 'app_cfg.h'                "
#elif  ((APP_CFG_FS_NAND_BLK_CNT <  0) || \
        (APP_CFG_FS_NAND_BLK_CNT >= 65535))
#error  "APP_CFG_FS_NAND_BLK_CNT            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0    ]                          "
#error  "                                   [     && <= 65535]                          "
#endif

#if    ((APP_CFG_FS_NAND_BUS_WIDTH != 8u ) && \
        (APP_CFG_FS_NAND_BUS_WIDTH != 16u))
#error  "APP_CFG_FS_NAND_BUS_WIDTH          illegally or not #define'd in 'app_cfg.h'   "
#error  "                                   [MUST be 8u ]                               "
#error  "                                   [     || 16u]                               "
#endif

#ifndef  APP_CFG_FS_NAND_DEFECT_MARK_TYPE
#error  "APP_CFG_FS_NAND_DEFECT_MARK_TYPE   not #define'd in 'app_cfg.h'                "
#elif  ((APP_CFG_FS_NAND_DEFECT_MARK_TYPE <  0) || \
        (APP_CFG_FS_NAND_DEFECT_MARK_TYPE >= 6))
#error  "APP_CFG_FS_NAND_DEFECT_MARK_TYPE   illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be FS_NAND_DEFECT_MARK_TYPE enum val] "
#endif

#ifndef  APP_CFG_FS_NAND_ECC_CODEWORD_SIZE
#error  "APP_CFG_FS_NAND_ECC_CODEWORD_SIZE  not #define'd in 'app_cfg.h'                "
#elif   ((APP_CFG_FS_NAND_ECC_CODEWORD_SIZE > 8192) || \
         (APP_CFG_FS_NAND_ECC_CODEWORD_SIZE < 0))
#error  "APP_CFG_FS_NAND_ECC_CODEWORD_SIZE  illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be > 0    ]                           "
#error  "                                   [     && <= 8192]                           "
#endif

#ifndef  APP_CFG_FS_NAND_ECC_NBR_CORR_BITS
#error  "APP_CFG_FS_NAND_ECC_NBR_CORR_BITS  not #define'd in 'app_cfg.h'                "
#elif   (APP_CFG_FS_NAND_ECC_NBR_CORR_BITS != 1)
#error  "APP_CFG_FS_NAND_ECC_NBR_CORR_BITS  illegally #define'd in 'app_cfg.h'          "
#error  "                                          [MUST be 1]                          "
#endif

#ifndef  APP_CFG_FS_NAND_MAX_BAD_BLK_CNT
#error  "APP_CFG_FS_NAND_MAX_BAD_BLK_CNT    not #define'd in 'app_cfg.h'                "
#elif   (APP_CFG_FS_NAND_MAX_BAD_BLK_CNT < 0)
#error  "APP_CFG_FS_NAND_MAX_BAD_BLK_CNT    illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0]                              "
#endif

#ifndef  APP_CFG_FS_NAND_MAX_BLK_ERASE
#error  "APP_CFG_FS_NAND_MAX_BLK_ERASE      not #define'd in 'app_cfg.h'                "
#elif   (APP_CFG_FS_NAND_MAX_BLK_ERASE < 0)
#error  "APP_CFG_FS_NAND_MAX_BLK_ERASE      illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0]                              "
#endif

#ifndef  APP_CFG_FS_NAND_PG_PER_BLK
#error  "APP_CFG_FS_NAND_PG_PER_BLK         not #define'd in 'app_cfg.h'                "
#elif  ((APP_CFG_FS_NAND_PG_PER_BLK <= 0) || \
        (APP_CFG_FS_NAND_PG_PER_BLK >  65535))
#error  "APP_CFG_FS_NAND_PG_PER_BLK         illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be > 0    ]                           "
#error  "                                   [     && < 65536]                           "
#endif

#ifndef  APP_CFG_FS_NAND_PG_SIZE
#error  "APP_CFG_FS_NAND_PG_SIZE            not #define'd in 'app_cfg.h'                "
#elif  ((APP_CFG_FS_NAND_PG_SIZE <= 0)     || \
        (APP_CFG_FS_NAND_PG_SIZE >  65535) || \
        (FS_UTIL_IS_PWR2(APP_CFG_FS_NAND_PG_SIZE) == DEF_NO))
#error  "APP_CFG_FS_NAND_PG_SIZE            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be > 0    ]                           "
#error  "                                   [     && < 65536]                           "
#error  "                                   [     && POW2   ]                           "
#endif

#ifndef  APP_CFG_FS_NAND_SPARE_SIZE
#error  "APP_CFG_FS_NAND_SPARE_SIZE         not #define'd in 'app_cfg.h'                "
#elif  ((APP_CFG_FS_NAND_SPARE_SIZE < 0) || \
        (APP_CFG_FS_NAND_SPARE_SIZE > 65535))
#error  "APP_CFG_FS_NAND_SPARE_SIZE         illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 0    ]                          "
#error  "                                   [     && <  65536]                          "
#endif

#endif  /* APP_CFG_FS_NAND_PART_TYPE == STATIC */

#endif  /* APP_CFG_FS_NAND_EN == DEF_ENABLED) */

                                                                /* ------------- NOR DRIVER CONFIGURATION ------------- */
#ifndef  APP_CFG_FS_NOR_EN
#error  "APP_CFG_FS_NOR_EN                        not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_NOR_EN != DEF_DISABLED) && \
        (APP_CFG_FS_NOR_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_NOR_EN                  illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif   (APP_CFG_FS_NOR_EN == DEF_ENABLED)

                                                                /* Base address.                                        */
#ifndef  APP_CFG_FS_NOR_ADDR_BASE
#error  "APP_CFG_FS_NOR_ADDR_BASE                 not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Region number.                                       */
#ifndef  APP_CFG_FS_NOR_REGION_NBR
#error  "APP_CFG_FS_NOR_REGION_NBR                not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Start address.                                       */
#ifndef  APP_CFG_FS_NOR_ADDR_START
#error  "APP_CFG_FS_NOR_ADDR_START                not #define'd in 'app_cfg.h'          "

#elif   (APP_CFG_FS_NOR_ADDR_START < APP_CFG_FS_NOR_ADDR_BASE)
#error  "APP_CFG_FS_NOR_ADDR_START          illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= APP_CFG_FS_NOR_ADDR_BASE]       "
#endif

                                                                /* Device size.                                         */
#ifndef  APP_CFG_FS_NOR_DEV_SIZE
#error  "APP_CFG_FS_NOR_DEV_SIZE                  not #define'd in 'app_cfg.h'          "

#elif   (APP_CFG_FS_NOR_DEV_SIZE < 1u)
#error  "APP_CFG_FS_NOR_DEV_SIZE            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif

                                                                /* Sector size.                                         */
#ifndef  APP_CFG_FS_NOR_SEC_SIZE
#error  "APP_CFG_FS_NOR_SEC_SIZE                  not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "

#elif  ((APP_CFG_FS_NOR_SEC_SIZE !=  512u) && \
        (APP_CFG_FS_NOR_SEC_SIZE != 1024u) && \
        (APP_CFG_FS_NOR_SEC_SIZE != 2048u) && \
        (APP_CFG_FS_NOR_SEC_SIZE != 4096u))
#error  "APP_CFG_FS_NOR_SEC_SIZE            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "
#endif

                                                                /* Percentage of device reserved.                       */
#ifndef  APP_CFG_FS_NOR_PCT_RSVD
#error  "APP_CFG_FS_NOR_PCT_RSVD                 not #define'd in 'app_cfg.h'           "

#elif  ((APP_CFG_FS_NOR_PCT_RSVD < FS_DEV_NOR_PCT_RSVD_MIN) || \
        (APP_CFG_FS_NOR_PCT_RSVD > FS_DEV_NOR_PCT_RSVD_MAX))
#error  "APP_CFG_FS_NOR_PCT_RSVD           illegally #define'd in 'app_cfg.h'           "
#error  "                                  [MUST be >= FS_DEV_NOR_PCT_RSVD_MIN]         "
#error  "                                  [     && <= FS_DEV_NOR_PCT_RSVD_MAX]         "
#endif

                                                                /* Erase count differential threshold.                  */
#ifndef  APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH
#error  "APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH        not #define'd in 'app_cfg.h'           "

#elif  ((APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH < FS_DEV_NOR_ERASE_CNT_DIFF_TH_MIN) || \
        (APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH > FS_DEV_NOR_ERASE_CNT_DIFF_TH_MAX))
#error  "APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH  illegally #define'd in 'app_cfg.h'           "
#error  "                                  [MUST be >= FS_DEV_NOR_ERASE_CNT_DIFF_TH_MIN]"
#error  "                                  [     && <= FS_DEV_NOR_ERASE_CNT_DIFF_TH_MAX]"
#endif

                                                                /* NOR physical-layer driver pointer.                   */
#ifndef  APP_CFG_FS_NOR_PHY_PTR
#error  "APP_CFG_FS_NOR_PHY_PTR                   not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Bus width.                                           */
#ifndef  APP_CFG_FS_NOR_BUS_WIDTH
#error  "APP_CFG_FS_NOR_BUS_WIDTH                 not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Maximum bus width.                                   */
#ifndef  APP_CFG_FS_NOR_BUS_WIDTH_MAX
#error  "APP_CFG_FS_NOR_BUS_WIDTH_MAX             not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Physical device count.                               */
#ifndef  APP_CFG_FS_NOR_PHY_DEV_CNT
#error  "APP_CFG_FS_NOR_PHY_DEV_CNT               not #define'd in 'app_cfg.h'          "
#endif

                                                                /* Maximum clock frequency.                             */
#ifndef  APP_CFG_FS_NOR_MAX_CLK_FREQ
#error  "APP_CFG_FS_NOR_MAX_CLK_FREQ              not #define'd in 'app_cfg.h'          "
#endif
#endif


                                                                /* ------------- RAM DRIVER CONFIGURATION ------------- */
#ifndef  APP_CFG_FS_RAM_EN
#error  "APP_CFG_FS_RAM_EN                        not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_RAM_EN != DEF_DISABLED) && \
        (APP_CFG_FS_RAM_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_RAM_EN                  illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif   (APP_CFG_FS_RAM_EN == DEF_ENABLED)
                                                                /* Number of sectors.                                   */
#ifndef  APP_CFG_FS_RAM_NBR_SECS
#error  "APP_CFG_FS_RAM_NBR_SECS                  not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "

#elif   (APP_CFG_FS_RAM_NBR_SECS < 1u)
#error  "APP_CFG_FS_RAM_NBR_SECS            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be >= 1]                              "
#endif

                                                                /* Sector size.                                         */
#ifndef  APP_CFG_FS_RAM_SEC_SIZE
#error  "APP_CFG_FS_RAM_SEC_SIZE                  not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "

#elif  ((APP_CFG_FS_RAM_SEC_SIZE !=  512) && \
        (APP_CFG_FS_RAM_SEC_SIZE != 1024) && \
        (APP_CFG_FS_RAM_SEC_SIZE != 2048) && \
        (APP_CFG_FS_RAM_SEC_SIZE != 4096))
#error  "APP_CFG_FS_RAM_SEC_SIZE            illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  512]                              "
#error  "                                   [     || 1024]                              "
#error  "                                   [     || 2048]                              "
#error  "                                   [     || 4096]                              "
#endif
#endif


                                                                /* ---------- SD (CARD) DRIVER CONFIGURATION ---------- */
#ifndef  APP_CFG_FS_SD_CARD_EN
#error  "APP_CFG_FS_SD_CARD_EN                    not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_SD_CARD_EN != DEF_DISABLED) && \
        (APP_CFG_FS_SD_CARD_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_SD_CARD_EN              illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "
#endif


                                                                /* ----------- SD (SPI) DRIVER CONFIGURATION ---------- */
#ifndef  APP_CFG_FS_SD_EN
#error  "APP_CFG_FS_SD_EN                         not #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "

#elif  ((APP_CFG_FS_SD_EN != DEF_DISABLED) && \
        (APP_CFG_FS_SD_EN != DEF_ENABLED ))
#error  "APP_CFG_FS_SD_EN                   illegally #define'd in 'app_cfg.h'          "
#error  "                                   [MUST be  DEF_DISABLED]                     "
#error  "                                   [     ||  DEF_ENABLED ]                     "
#endif

#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of FS app module include.                        */
