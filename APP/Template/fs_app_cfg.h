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
*                                  APPLICATION CONFIGURATION TEMPLATE
*
*
* Filename : fs_app_cfg.h
* Version  : V4.08.00
* Note(s)  : This is an example application configuration template when using the initialization code
* provided in fs_app.c. The relevant preprocessor definitions should be included in the project global
* app_cfg.h. Note that the values are demonstrative only and should be configured correctly for the
* targetted hardware.
*********************************************************************************************************
*/

#error   "This file should NOT be included. Instead, the macro definitions should be copied in app_cfg.h"


/*
*********************************************************************************************************
*                                       MODULE ENABLE / DISABLE
*********************************************************************************************************
*/

#define  APP_CFG_FS_EN               DEF_ENABLED                /* Enable/disable the file system initialization code.  */


/*
*********************************************************************************************************
*                                          FS CONFIGURATION
*
* Note(s) : This section define various prerprocessor constant used by the example initialization code
*           located in fs_app.c to configure the file system.
*********************************************************************************************************
*/

#define  APP_CFG_FS_DEV_CNT          3                          /* Maximum number of opened devices.                    */
#define  APP_CFG_FS_VOL_CNT          3                          /* Maximum number of opened volumes.                    */
#define  APP_CFG_FS_FILE_CNT         5                          /* Maximum number of opened files.                      */
#define  APP_CFG_FS_DIR_CNT          5                          /* Maximum number of opened directories.                */
#define  APP_CFG_FS_BUF_CNT          (4 * APP_CFG_FS_VOL_CNT)   /* Internal buffer count.                               */
#define  APP_CFG_FS_DEV_DRV_CNT      3                          /* Maximum number of different device drivers.          */
#define  APP_CFG_FS_WORKING_DIR_CNT  5                          /* Maximum number of active working directories.        */
#define  APP_CFG_FS_MAX_SEC_SIZE     512                        /* Maximum sector size supported.                       */

#define  APP_CFG_FS_IDE_EN           DEF_DISABLED               /* Enable/disable the IDE\CF initialization.            */
#define  APP_CFG_FS_MSC_EN           DEF_DISABLED               /* Enable/disable the MSC initialization.               */
#define  APP_CFG_FS_NAND_EN          DEF_DISABLED               /* Enable/disable the NAND initialization.              */
#define  APP_CFG_FS_NOR_EN           DEF_DISABLED               /* Enable/disable the NOR initialization.               */
#define  APP_CFG_FS_RAM_EN           DEF_ENABLED                /* Enable/disable the RAMDisk initialization.           */
#define  APP_CFG_FS_SD_EN            DEF_DISABLED               /* Enable/disable the SD (SPI) initialization.          */
#define  APP_CFG_FS_SD_CARD_EN       DEF_DISABLED               /* Enable/disable the SD (Card) initialization.         */


/*
*********************************************************************************************************
*                                    RAMDISK DRIVER CONFIGURATION
*********************************************************************************************************
*/

#define  APP_CFG_FS_RAM_NBR_SECS     1024                       /* RAMDisk size in sectors.                             */
#define  APP_CFG_FS_RAM_SEC_SIZE     512                        /* RAMDisk sector size in byte.                         */


/*
*********************************************************************************************************
*                                      NOR DRIVER CONFIGURATION
*********************************************************************************************************
*/

#define  APP_CFG_FS_NOR_ADDR_BASE             0x64000000u       /* Base address of flash.                               */
#define  APP_CFG_FS_NOR_REGION_NBR            0u                /* Block region within flash.                           */
#define  APP_CFG_FS_NOR_ADDR_START            0x64000000u       /* Start address of block region within NOR.            */
#define  APP_CFG_FS_NOR_DEV_SIZE              0x01000000u       /* Device size in bytes.                                */
#define  APP_CFG_FS_NOR_SEC_SIZE              512u              /* Sector size.                                         */
#define  APP_CFG_FS_NOR_PCT_RSVD              10u               /* Reserved area in percent.                            */
#define  APP_CFG_FS_NOR_ERASE_CNT_DIFF_TH     40u               /* Erase count difference threshold.                    */
#define  APP_CFG_FS_NOR_PHY_PTR              &FSDev_NOR_AMD_1x16 /* PHY pointer.                                        */
#define  APP_CFG_FS_NOR_BUS_WIDTH             16u               /* Bus width.                                           */
#define  APP_CFG_FS_NOR_BUS_WIDTH_MAX         16u               /* Maximum bus width.                                   */
#define  APP_CFG_FS_NOR_PHY_DEV_CNT           1u                /* Number of interleaved devices.                       */
#define  APP_CFG_FS_NOR_MAX_CLK_FREQ          0u                /* Maximum clock frequency for serial flash.            */

/*
*********************************************************************************************************
*                                      NAND DRIVER CONFIGURATION
*
* Note(s) : Only include one of either the ONFI or Static configuration.
*********************************************************************************************************
*/

                                                                 /* ----------------------- ONFI ---------------------- */
#define  APP_CFG_FS_NAND_CTRLR_IMPL                CTRLR_GEN     /* Controller implementation.                          */
#define  APP_CFG_FS_NAND_PART_TYPE                 ONFI          /* Part type.                                          */

#define  APP_CFG_FS_NAND_BSP                       FS_NAND_BSP_XXX /* NAND BSP.                                         */
#define  APP_CFG_FS_NAND_FREE_SPARE_START          1u            /* Start of available free spare area.                 */
#define  APP_CFG_FS_NAND_FREE_SPARE_LEN            63u           /* Length of available free spare area.                */


                                                                 /* ---------------------- STATIC --------------------- */
#define  APP_CFG_FS_NAND_CTRLR_IMPL                CTRLR_GEN     /* Controller implementation.                          */
#define  APP_CFG_FS_NAND_PART_TYPE                 STATIC        /* Part type.                                          */

#define  APP_CFG_FS_NAND_BSP                       FS_NAND_BSP_XXX /* NAND BSP.                                         */

#define  APP_CFG_FS_NAND_BLK_CNT                   2048u         /* Total block count.                                  */
#define  APP_CFG_FS_NAND_BUS_WIDTH                 8u            /* Bus width.                                          */
#define  APP_CFG_FS_NAND_DEFECT_MARK_TYPE          DEFECT_PG_L_1_OR_N_PG_1_OR_2 /* Manufacturer defect mark type.       */
#define  APP_CFG_FS_NAND_ECC_CODEWORD_SIZE         528u          /* Size of ECC codewords.                              */
#define  APP_CFG_FS_NAND_ECC_NBR_CORR_BITS         1u            /* Maximum number of correctable bits.                 */
#define  APP_CFG_FS_NAND_FREE_SPARE_START          1u            /* Start of available free spare area.                 */
#define  APP_CFG_FS_NAND_FREE_SPARE_LEN            63u           /* Length of available free spare area.                */
#define  APP_CFG_FS_NAND_MAX_BAD_BLK_CNT           40u           /* Maximum number of bad blocks.                       */
#define  APP_CFG_FS_NAND_PG_PER_BLK                4u            /* Pages per block.                                    */
#define  APP_CFG_FS_NAND_PG_SIZE                   2048u         /* Page size not including the spare area.             */
#define  APP_CFG_FS_NAND_SPARE_SIZE                64u           /* Total size of the spare area.                       */



/*
*********************************************************************************************************
*                                    SD CARD DRIVER CONFIGURATION
*
* Note(s) : No configuration is required for this driver.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     SD SPI DRIVER CONFIGURATION
*
* Note(s) : No configuration is required for this driver.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       MSC DRIVER CONFIGURATION
*
* Note(s) : No configuration is required for this driver.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     IDE/CF DRIVER CONFIGURATION
*
* Note(s) : No configuration is required for this driver.
*********************************************************************************************************
*/
