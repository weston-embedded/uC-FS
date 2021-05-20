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
*                                         FILE SYSTEM DEVICE DRIVER
*
*                                          NAND FLASH ONFI DEVICES
*
* Filename : fs_dev_nand_part_onfi.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_NAND_ONFI_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/edc_crc.h>
#include  <fs_cfg.h>
#include  <fs_dev_nand_cfg.h>
#include  "../../../Source/fs.h"
#include  "fs_dev_nand_part_onfi.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
                                                                /* ONFI versions.                                       */
#define  FS_NAND_PART_ONFI_V30                          DEF_BIT_06
#define  FS_NAND_PART_ONFI_V23                          DEF_BIT_05
#define  FS_NAND_PART_ONFI_V22                          DEF_BIT_04
#define  FS_NAND_PART_ONFI_V21                          DEF_BIT_03
#define  FS_NAND_PART_ONFI_V20                          DEF_BIT_02
#define  FS_NAND_PART_ONFI_V10                          DEF_BIT_01

#define  FS_NAND_PART_ONFI_V_QTY                          6u

#define  FS_NAND_PART_ONFI_MAX_PARAM_PG                   3u

                                                                /* Supported ONFI features.                             */
#define  FS_NAND_PART_ONFI_FEATURE_EX_PP                DEF_BIT_07
#define  FS_NAND_PART_ONFI_FEATURE_RNDM_PG_PGM          DEF_BIT_02
#define  FS_NAND_PART_ONFI_FEATURE_BUS_16               DEF_BIT_00

                                                                /* Last data byte of parameter page covered by CRC.     */
#define  FS_NAND_PART_ONFI_PARAM_PAGE_LAST_DATA_BYTE    253u

                                                                /* Supported section types of extended parameter page.  */
#define  FS_NAND_PART_ONFI_SECTION_TYPE_UNUSED            0u
#define  FS_NAND_PART_ONFI_SECTION_TYPE_SPECIFIER         1u
#define  FS_NAND_PART_ONFI_SECTION_TYPE_ECC_INFO          2u

/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


const  CPU_INT32U  FS_NAND_PartONFI_PowerOf10[10] = {
         1u,
        10u,
       100u,
      1000u,
     10000u,
    100000u,
   1000000u,
  10000000u,
 100000000u,
1000000000u
};

const  CPU_INT16U  FS_NAND_PartONFI_SupportedVersions[FS_NAND_PART_ONFI_V_QTY] = {
  FS_NAND_PART_ONFI_V30,
  FS_NAND_PART_ONFI_V23,
  FS_NAND_PART_ONFI_V22,
  FS_NAND_PART_ONFI_V21,
  FS_NAND_PART_ONFI_V20,
  FS_NAND_PART_ONFI_V10};


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


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

#define  FS_NAND_PART_ONFI_CFG_FIELD(type, name, dflt_val)  dflt_val,
const  FS_NAND_PART_ONFI_CFG  FS_NAND_PartONFI_DfltCfg = { FS_NAND_PART_ONFI_CFG_FIELDS };
#undef   FS_NAND_PART_ONFI_CFG_FIELD


CPU_INT08U          FS_NAND_PartONFI_ParamPg[FS_NAND_PART_ONFI_PARAM_PAGE_LEN];
CPU_INT08U          FS_NAND_PartONFI_ParamPageCnt;

CPU_INT32U          FS_NAND_PartONFI_ExtParamPageLen;
CPU_INT08U         *FS_NAND_PartONFI_ExtParamPageBufPtr;
CPU_INT16U          FS_NAND_PartONFI_ExtParamPageBufLen;

FS_NAND_PART_DATA  *FS_NAND_PartONFI_ListFreePtr;

CRC_MODEL_16        FS_NAND_PartONFI_CRCModel = {
    0x8005,
    0x4F4E,
    DEF_NO,
    0x0000,
    (const CPU_INT16U *) 0
};

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                /* Open NAND part.                                      */
static  FS_NAND_PART_DATA  *FS_NAND_PartONFI_Open             (const  FS_NAND_CTRLR_API  *p_ctrlr_api,
                                                                      void               *p_ctrlr_data_v,
                                                                      void               *p_part_cfg_v,
                                                                      FS_ERR             *p_err);

                                                                /* Close NAND part.                                     */
static  void                FS_NAND_PartONFI_Close            (       FS_NAND_PART_DATA  *p_part_data);

                                                                /* Parse ONFI parameter page.                           */
static  void                FS_NAND_PartONFI_ParamPageParse   (       FS_NAND_PART_DATA  *p_part_data,
                                                                      FS_ERR             *p_err);

                                                                /* Parse ONFI extended parameter page.                  */
static  void                FS_NAND_PartONFI_ExtParamPageParse(       FS_NAND_PART_DATA  *p_part_data,
                                                                      CPU_INT08U         *p_page,
                                                                      CPU_INT16U          page_len,
                                                                      FS_ERR             *p_err);

                                                                /* Get part data structure container.                   */
static  FS_NAND_PART_DATA  *FS_NAND_PartONFI_DataGet          (       void);

                                                                /* Free part data structure container.                  */
static  void                FS_NAND_PartONFI_DataFree         (       FS_NAND_PART_DATA  *p_part_data);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_NAND_PART_API  FS_NAND_PartONFI = {
    FS_NAND_PartONFI_Open,                                      /* Open NAND Part.                                      */
    FS_NAND_PartONFI_Close                                      /* Close NAND part.                                     */
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      FS_NAND_PartONFI_Open()
*
* Description : Open (initialize) the part-specific layer for ONFI devices.
*
* Argument(s) : p_ctrlr_api     Pointer to the NAND controller API.
*
*               p_ctrlr_data    Pointer to the NAND controller data structure.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE                                 Part open successfully.
*                               FS_ERR_NULL_PTR                             Argument is a null pointer.
*                               FS_ERR_MEM_ALLOC                            Part data could not be alloc'ed.
*
*                                                                           --- RETURNED BY FS_NAND_PartONFI_ParamPageParse() ---
*                               FS_ERR_DEV_NAND_ONFI_INVALID_PARAM_PAGE     Invalid parameter page.
*                               FS_ERR_DEV_NAND_ONFI_VER_NOT_SUPPORTED      ONFI version not supported.
*
* Return(s)   : Pointer to a NAND part data object, if NO errors.
*               Null pointer                      , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_NAND_PART_DATA  *FS_NAND_PartONFI_Open  (const  FS_NAND_CTRLR_API  *p_ctrlr_api,
                                                           void               *p_ctrlr_data_v,
                                                           void               *p_part_cfg_v,
                                                           FS_ERR             *p_err)
{
    FS_NAND_PART_DATA                 *p_part_data;
    FS_NAND_RD_PARAM_PG_IO_CTRL_DATA   param_pg;
    FS_NAND_PART_ONFI_CFG             *p_cfg;
    CPU_DATA                           ix;
    CPU_INT08U                         sig_cnt;
    CPU_INT16U                         chk_crc;
    CPU_INT16U                         calc_crc;
    EDC_ERR                            err;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate error pointer.                              */
        CPU_SW_EXCEPTION((FS_NAND_PART_DATA *)0u);
    }
    if (p_ctrlr_api == DEF_NULL) {                              /* Validate pointer to controller API.                  */
       *p_err = FS_ERR_NULL_PTR;
        return ((FS_NAND_PART_DATA *)0u);
    }
    if (p_ctrlr_data_v == DEF_NULL) {                           /* Validate pointer to controller data structure.       */
       *p_err = FS_ERR_NULL_PTR;
        return ((FS_NAND_PART_DATA *)0u);
    }
#endif


                                                                /* ----------------- ALLOC PART DATA ------------------ */
    p_part_data = FS_NAND_PartONFI_DataGet();
    if (p_part_data == (FS_NAND_PART_DATA *)0u) {
       *p_err = FS_ERR_MEM_ALLOC;
        return ((FS_NAND_PART_DATA *)0u);
    }


                                                                /* --------------- READ PARAMETER PAGE ---------------- */
                                                                /* Set read parameters.                                 */
    param_pg.RelAddr =  0u;
    param_pg.DataPtr = &FS_NAND_PartONFI_ParamPg[0u];
    param_pg.ByteCnt =  FS_NAND_PART_ONFI_PARAM_PAGE_LEN;

                                                                /* Init CRC values to start the loop.                   */
    calc_crc = 0u;
    chk_crc  = 1u;
    ix       = 0u;

                                                                /* Find a valid parameter page (matching CRC).          */
    while ((calc_crc != chk_crc) &&
           (ix        < FS_NAND_PART_ONFI_MAX_PARAM_PG)) {
        ix++;
                                                                /* Read an instance of the parameter page.              */
        p_ctrlr_api->IO_Ctrl(p_ctrlr_data_v, FS_DEV_IO_CTRL_NAND_PARAM_PG_RD, &param_pg, p_err);
        if (*p_err != FS_ERR_NONE) {
            FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Could not recover parameter page from ONFI device.\r\n"));
            return ((FS_NAND_PART_DATA *)0u);
        }


                                                                /* --------- VERIFY PARAMETER PAGE SIGNATURE ---------- */
                                                                /* Signature is valid if 2 or more bytes are valid.     */
        sig_cnt = 0;
        if (FS_NAND_PartONFI_ParamPg[0] == 'O') {
            sig_cnt += 1;
        }
        if (FS_NAND_PartONFI_ParamPg[1] == 'N') {
            sig_cnt += 1;
        }
        if (FS_NAND_PartONFI_ParamPg[2] == 'F') {
            sig_cnt += 1;
        }
        if (FS_NAND_PartONFI_ParamPg[3] == 'I') {
            sig_cnt += 1;
        }
                                                                /* If sig invalid, last parameter page has been read.   */
        if (sig_cnt < 2) {
            FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Could not recover a valid parameter page from ONFI device.\r\n"));
           *p_err = FS_ERR_DEV_INVALID;
        } else {
                                                                /* If sig valid, verify the CRC.                        */
            chk_crc   = FS_NAND_PartONFI_ParamPg[254];
            chk_crc  |= (CPU_INT16U)((CPU_INT16U)FS_NAND_PartONFI_ParamPg[255] << DEF_INT_08_NBR_BITS);

            calc_crc = CRC_ChkSumCalc_16Bit (&FS_NAND_PartONFI_CRCModel,
                                             &FS_NAND_PartONFI_ParamPg[0],
                                             FS_NAND_PART_ONFI_PARAM_PAGE_LAST_DATA_BYTE + 1u,
                                             &err);
            if (err != EDC_CRC_ERR_NONE) {
                FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Invalid CRC argument.\r\n"));
               *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
                return ((FS_NAND_PART_DATA *)0u);
            }

                                                                /* Increment the parameter page address.                */
            param_pg.RelAddr += FS_NAND_PART_ONFI_PARAM_PAGE_LEN;

        }

    }

    if (calc_crc != chk_crc) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Could not recover parameter page from ONFI device.\r\n"));
        return ((FS_NAND_PART_DATA *)0u);
    }

                                                                /* Parse the parameter page.                            */
    FS_NAND_PartONFI_ParamPageParse(p_part_data, p_err);


                                                                /* Find an extended parameter page if present.          */
    if (*p_err == FS_ERR_DEV_NAND_ONFI_EXT_PARAM_PAGE) {
        *p_err  = FS_ERR_NONE;
                                                                /* Set buffer to read the extended parameter page.      */
        FS_NAND_PartONFI_ExtParamPageBufPtr = &FS_NAND_PartONFI_ParamPg[0];
        FS_NAND_PartONFI_ExtParamPageBufLen =  FS_NAND_PART_ONFI_PARAM_PAGE_LEN;

        param_pg.RelAddr = FS_NAND_PartONFI_ParamPageCnt * FS_NAND_PART_ONFI_PARAM_PAGE_LEN;
        param_pg.ByteCnt = FS_NAND_PartONFI_ExtParamPageLen;
        param_pg.DataPtr = FS_NAND_PartONFI_ExtParamPageBufPtr;

        if (param_pg.ByteCnt > FS_NAND_PartONFI_ExtParamPageBufLen) {
            FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): The buffer used for the Extended Parameter Page is too small.\r\n"));
           *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
            return ((FS_NAND_PART_DATA *)0u);
        }

        calc_crc = 0u;
        chk_crc  = 1u;
                                                                /* Find a valid extended parameter page.                */
        while (chk_crc != calc_crc) {

            p_ctrlr_api->IO_Ctrl(p_ctrlr_data_v, FS_DEV_IO_CTRL_NAND_PARAM_PG_RD, &param_pg, p_err);
            if (*p_err != FS_ERR_NONE) {
                FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Could not recover extended parameter page from ONFI device.\r\n"));
                return ((FS_NAND_PART_DATA *)0u);
            }
                                                                /* --------------- VERIFY PARAM PG SIG ---------------- */
                                                                /* Signature is valid if 2 or more bytes are valid.     */
            sig_cnt = 0;
            if (FS_NAND_PartONFI_ExtParamPageBufPtr[2] == 'E') {
                sig_cnt += 1;
            }
            if (FS_NAND_PartONFI_ExtParamPageBufPtr[3] == 'P') {
                sig_cnt += 1;
            }
            if (FS_NAND_PartONFI_ExtParamPageBufPtr[4] == 'P') {
                sig_cnt += 1;
            }
            if (FS_NAND_PartONFI_ExtParamPageBufPtr[5] == 'S') {
                sig_cnt += 1;
            }
                                                                /* If sig invalid, last parameter page has been read.   */
            if (sig_cnt < 2) {
                FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Could not recover a valid extended parameter page from ONFI device.\r\n"));
               *p_err = FS_ERR_DEV_INVALID;
                return ((FS_NAND_PART_DATA *)0u);
            } else {
                                                                /* If sig valid, verify the CRC.                        */
                chk_crc  = FS_NAND_PartONFI_ExtParamPageBufPtr[0];
                chk_crc |= (CPU_INT16U)((CPU_INT16U)FS_NAND_PartONFI_ExtParamPageBufPtr[1] << DEF_INT_08_NBR_BITS);

                calc_crc = CRC_ChkSumCalc_16Bit (&FS_NAND_PartONFI_CRCModel,
                                                 &FS_NAND_PartONFI_ExtParamPageBufPtr[2],
                                                  FS_NAND_PartONFI_ExtParamPageLen - 2,
                                                 &err);
                if (err != EDC_CRC_ERR_NONE) {
                    FS_TRACE_DBG(("FS_NAND_PartONFI_Open(): Invalid CRC argument.\r\n"));
                   *p_err = FS_ERR_DEV_INVALID_LOW_PARAMS;
                    return ((FS_NAND_PART_DATA *)0u);
                }

                                                                /* Increment the extended parameter page address.       */
                param_pg.RelAddr += FS_NAND_PartONFI_ExtParamPageLen;

            }

        }

                                                                /* Parse the extended parameter page.                   */
        FS_NAND_PartONFI_ExtParamPageParse(p_part_data,
                                           FS_NAND_PartONFI_ExtParamPageBufPtr,
                                           FS_NAND_PartONFI_ExtParamPageLen,
                                           p_err);

        if (*p_err != FS_ERR_NONE) {
           *p_err = FS_ERR_DEV_INVALID;
            return ((FS_NAND_PART_DATA *)0u);
        }

    } else {
        if (*p_err != FS_ERR_NONE) {
           *p_err = FS_ERR_DEV_INVALID;
            return ((FS_NAND_PART_DATA *)0u);
        }
    }

                                                                /* Set other ONFI parameters.                           */
    p_cfg = (FS_NAND_PART_ONFI_CFG *)p_part_cfg_v;

    p_part_data->FreeSpareMap     = p_cfg->FreeSpareMap;

    return (p_part_data);
}


/*
*********************************************************************************************************
*                                      FS_NAND_PartStatic_Close()
*
* Description : Close NAND part instance.
*
* Argument(s) : p_part_data     Pointer to NAND part data object.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  FS_NAND_PartONFI_Close (FS_NAND_PART_DATA  *p_part_data)
{
    FS_NAND_PartONFI_DataFree(p_part_data);
}


/*
*********************************************************************************************************
*                                   FS_NAND_PartONFI_ParamPageParse()
*
* Description : Parse the ONFI parameter page.
*
* Argument(s) : p_part_data     Pointer to a NAND part data object.
*               -----------     Argument validated by caller.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_DEV_NAND_ONFI_INVALID_PARAM_PAGE     Invalid parameter page.
*                                   FS_ERR_DEV_NAND_ONFI_VER_NOT_SUPPORTED      ONFI version not supported.
*                                   FS_ERR_NONE                                 Parameter page parsed successfully.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_NAND_PartONFI_ParamPageParse (FS_NAND_PART_DATA  *p_part_data,
                                               FS_ERR             *p_err)
{
    CPU_BOOLEAN  is_ver_supported;
    CPU_BOOLEAN  has_ext_pp;
    CPU_BOOLEAN  is_bus_16;
    CPU_BOOLEAN  is_invalid;
    CPU_INT08U   value;
    CPU_INT08U   multiplier;
    CPU_INT08U   lun_nbr;
    CPU_INT16U   version;
    CPU_INT32U   pg_size;
    CPU_INT32U   max_size;
    CPU_INT32U   nb_pg_per_blk;
    CPU_INT32U   blk_cnt;
    CPU_INT64U   max_blk_erase;
    CPU_DATA     ix;


   *p_err = FS_ERR_NONE;
                                                                /* -------------- REV INFO AND FEATURES --------------- */
                                                                /* Validate ONFI version.                               */
    MEM_VAL_COPY_GET_INT16U_LITTLE(&version, &FS_NAND_PartONFI_ParamPg[4]);

    is_invalid = DEF_BIT_IS_SET(version, DEF_BIT_00);
    if (is_invalid == DEF_YES) {
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }

    ix = 0;
    is_ver_supported = DEF_NO;
    while ((is_ver_supported != DEF_YES) &&
           (ix < FS_NAND_PART_ONFI_V_QTY)){
        is_ver_supported = DEF_BIT_IS_SET(version, FS_NAND_PartONFI_SupportedVersions[ix]);
        ix++;
    }

    if (is_ver_supported == DEF_YES) {
        version = FS_NAND_PartONFI_SupportedVersions[ix - 1u];
    } else {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): ONFI version not supported by this driver.\r\n"));
       *p_err = FS_ERR_DEV_INVALID;
        return;
    }


                                                                /* Identify features.                                   */
    has_ext_pp = DEF_BIT_IS_SET(FS_NAND_PartONFI_ParamPg[6], FS_NAND_PART_ONFI_FEATURE_EX_PP);

    is_bus_16  = DEF_BIT_IS_SET(FS_NAND_PartONFI_ParamPg[6], FS_NAND_PART_ONFI_FEATURE_BUS_16);
    if (is_bus_16 == DEF_YES) {
        p_part_data->BusWidth = 16u;
    } else {
        p_part_data->BusWidth =  8u;
    }

                                                                /* Extended parameter page length.                      */
    if (has_ext_pp == DEF_YES) {
        FS_NAND_PartONFI_ExtParamPageLen  = FS_NAND_PartONFI_ParamPg[12];
        FS_NAND_PartONFI_ExtParamPageLen |= (CPU_INT16U)((CPU_INT16U)FS_NAND_PartONFI_ParamPg[13] << DEF_INT_08_NBR_BITS);
        FS_NAND_PartONFI_ExtParamPageLen *= 16u;
    }

                                                                /* Number of parameter pages.                           */
    if (version >= FS_NAND_PART_ONFI_V21) {
        FS_NAND_PartONFI_ParamPageCnt = FS_NAND_PartONFI_ParamPg[14];
    } else {
        FS_NAND_PartONFI_ParamPageCnt = 3u;
    }


                                                                /* ----------- IDENTIFY MEMORY ORGANIZATION ----------- */
                                                                /* Page size.                                           */
    MEM_VAL_COPY_GET_INT32U_LITTLE(&pg_size, &FS_NAND_PartONFI_ParamPg[80]);
    max_size = (FS_NAND_PG_SIZE) -1;
    if (pg_size > max_size) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): Page size does not fit in container type FS_NAND_PG_SIZE.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
    p_part_data->PgSize = pg_size;

                                                                /* Spare size.                                          */
    MEM_VAL_COPY_GET_INT16U_LITTLE(&(p_part_data->SpareSize), &FS_NAND_PartONFI_ParamPg[84]);

                                                                /* Nb of pages per blk.                                 */
    MEM_VAL_COPY_GET_INT32U_LITTLE(&nb_pg_per_blk, &FS_NAND_PartONFI_ParamPg[92]);
    max_size  = (FS_NAND_PG_PER_BLK_QTY) -1;
    if (nb_pg_per_blk > max_size) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): Nb of pages per blk does not fit in container type FS_NAND_PG_PER_BLK_QTY.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
    p_part_data->PgPerBlk = nb_pg_per_blk;

                                                                /* Nb of block per logical unit.                        */
    MEM_VAL_COPY_GET_INT32U_LITTLE(&blk_cnt, &FS_NAND_PartONFI_ParamPg[96]);
    max_size = (FS_NAND_BLK_QTY) - 1;
    if (blk_cnt > max_size) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): Blk cnt does not fit in container type FS_NAND_BLK_QTY.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
                                                                /* Nb of logical units.                                 */
    lun_nbr = FS_NAND_PartONFI_ParamPg[100];

    p_part_data->BlkCnt = blk_cnt * lun_nbr;

                                                                /* Max nb of bad blocks per logical unit.               */
    MEM_VAL_COPY_GET_INT16U_LITTLE(&(p_part_data->MaxBadBlkCnt), &FS_NAND_PartONFI_ParamPg[103]);
    p_part_data->MaxBadBlkCnt *= lun_nbr;

                                                                /* Max programming operations per block.                */
    value          = FS_NAND_PartONFI_ParamPg[105];
    multiplier     = FS_NAND_PartONFI_ParamPg[106];
    if (multiplier > 9u) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse: Max blk erase count larger than supported.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
    max_blk_erase     = value * FS_NAND_PartONFI_PowerOf10[multiplier];
    max_size          = (CPU_INT32U) - 1;
    if (max_blk_erase > max_size) {
        FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): Max blk erase cnt does not fit in container type CPU_INT32U.\r\n"));
       *p_err = FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS;
        return;
    }
    p_part_data->MaxBlkErase = max_blk_erase;

                                                                /* Max nb of partial page programming.                  */
    p_part_data->NbrPgmPerPg = FS_NAND_PartONFI_ParamPg[110];

                                                                /* ECC correction needed.                               */
    p_part_data->ECC_NbrCorrBits = FS_NAND_PartONFI_ParamPg[112];
    if (p_part_data->ECC_NbrCorrBits == 0xFFu) {

        p_part_data->ECC_CodewordSize = 0u;
        if (has_ext_pp == DEF_FALSE) {
            FS_TRACE_DBG(("FS_NAND_PartONFI_ParamPageParse(): ECC located in extended param pg, but dev does not have one.\r\n"));
           *p_err = FS_ERR_DEV_INVALID;
            return;
        } else {
           *p_err = FS_ERR_DEV_NAND_ONFI_EXT_PARAM_PAGE;
        }

    } else {
        p_part_data->ECC_CodewordSize = 528u;
    }

                                                                /* Factory defect mark type.                            */
    if (version >= FS_NAND_PART_ONFI_V21) {
        p_part_data->DefectMarkType = DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0;
    } else {
                                                                /* #### NAND drv not compatible with 'any loc'.         */
        p_part_data->DefectMarkType = DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0;
    }
}


/*
*********************************************************************************************************
*                                   FS_NAND_PartONFI_ExtParamPageParse()
*
* Description : Parse the ONFI extended parameter page.
*
* Argument(s) : p_part_data     Pointer to a NAND part data object.
*               -----------     Argument validated by caller.
*
*               p_page          Pointer to extended parameter page buffer.
*               ------          Argument validated by caller.
*
*               page_len        Extended parameter page length.
*               --------        Argument validated by caller.
*
*               p_err           Pointer to variable that will receive return the error code from this function :
*               -----           Argument validated by caller.
*
*                               FS_ERR_NONE                                 Parameter page parsed successfully.
*                               FS_ERR_DEV_NAND_ONFI_INVALID_PARAM_PAGE     Invalid parameter page.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_NAND_PartONFI_ExtParamPageParse (FS_NAND_PART_DATA  *p_part_data,
                                                  CPU_INT08U         *p_page,
                                                  CPU_INT16U          page_len,
                                                  FS_ERR             *p_err)
{
    CPU_INT08U   section_type;
    CPU_INT08U   section_len;
    CPU_INT08U   type_addr;
    CPU_INT08U   section_addr;
    CPU_BOOLEAN  is_pg_parsed;
    CPU_BOOLEAN  ecc_scheme_modified;
    CPU_INT32U   ix;
    CPU_INT08U   type_specifier_section_addr;
    CPU_INT08U   type_specifier_section_len;
    CPU_BOOLEAN  extra_sections_specified;


    type_addr                = 16;
    section_addr             = 32;
    section_type             = FS_NAND_PART_ONFI_SECTION_TYPE_SPECIFIER;
    is_pg_parsed             = DEF_FALSE;
    ecc_scheme_modified      = DEF_FALSE;
    extra_sections_specified = DEF_FALSE;


                                                                /* Parse the first 8 sections of the extended param pg. */
    while (is_pg_parsed == DEF_FALSE) {
                                                                /* Get section type and length.                         */
        section_type = p_page[type_addr];
        section_len  = p_page[type_addr + 1u] * 16u;

        switch (section_type) {
            case FS_NAND_PART_ONFI_SECTION_TYPE_UNUSED:         /* End parsing if unused sections are reached.          */
                 is_pg_parsed = DEF_TRUE;
                 break;

            case FS_NAND_PART_ONFI_SECTION_TYPE_SPECIFIER:      /* Get extra sections specifiers.                       */
                 type_specifier_section_addr = section_type;
                 type_specifier_section_len  = section_len;
                 extra_sections_specified    = DEF_TRUE;
                 break;

            case FS_NAND_PART_ONFI_SECTION_TYPE_ECC_INFO:       /* Get additionnal ECC info.                            */
                 if (ecc_scheme_modified == DEF_FALSE) {
                     p_part_data->ECC_NbrCorrBits  = p_page[section_addr];
                     p_part_data->ECC_CodewordSize = p_page[section_addr + 1u];
                     ecc_scheme_modified           = DEF_TRUE;
                 } else {
                     FS_TRACE_DBG(("FS_NAND_PartONFI_ExtParamPageParse(): Driver only support one ECC scheme.\r\n"));
                    *p_err = FS_ERR_DEV_NAND_ECC_NOT_SUPPORTED;
                     return;
                 }
                 break;

            default:
                 FS_TRACE_DBG(("FS_NAND_PartONFI_ExtParamPageParse(): Invalid section type in extended parameter page: %d.\r\n", section_type));
                *p_err = FS_ERR_DEV_INVALID;
                 return;
        }

                                                                /* Increment section address.                           */
        section_addr += section_len;
        type_addr    += 2u;

                                                                /* Detect end of extended parameter page.               */
        if (section_addr > page_len) {
            is_pg_parsed = DEF_TRUE;
        }
        if (type_addr > 30u) {
            is_pg_parsed = DEF_TRUE;
        }

    }

                                                               /* Process extra sections of extended parameter page.    */
    if (type_addr > 30u) {
        if (extra_sections_specified == DEF_FALSE) {
            FS_TRACE_DBG(("FS_NAND_PartONFI_ExtParamPageParse(): Extended parameter page extra sections not specified.\r\n"));
           *p_err = FS_ERR_DEV_INVALID;
            return;
        }
        for (ix = 0u; ix < (type_specifier_section_len / 2u); ix ++) {
            section_type = p_page[type_specifier_section_addr];
            section_len  = p_page[type_specifier_section_addr + 1u] * 16u;

            switch (section_type) {
                case FS_NAND_PART_ONFI_SECTION_TYPE_UNUSED:
                     break;

                case FS_NAND_PART_ONFI_SECTION_TYPE_ECC_INFO:
                     if (ecc_scheme_modified == DEF_FALSE) {
                         p_part_data->ECC_NbrCorrBits  = p_page[section_addr];
                         p_part_data->ECC_CodewordSize = p_page[section_addr + 1u];
                         ecc_scheme_modified           = DEF_TRUE;
                     } else {
                         FS_TRACE_DBG(("FS_NAND_PartONFI_ExtParamPageParse(): Driver only support one ECC scheme.\r\n"));
                        *p_err = FS_ERR_DEV_NAND_ECC_NOT_SUPPORTED;
                         return;
                     }
                     break;

                default:
                     FS_TRACE_DBG(("FS_NAND_PartONFI_ExtParamPageParse(): Invalid section type in extended parameter page: %d.\r\n", section_type));
                    *p_err = FS_ERR_DEV_INVALID;
                     return;
            }

            type_specifier_section_addr += 1u;
        }
    }

}

/*
*********************************************************************************************************
*                                     FS_NAND_PartONFI_DataFree()
*
* Description : Free a NAND part data object.
*
* Argument(s) : p_part_data   Pointer to a nand part data object.
*               -----------   Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_NAND_PartONFI_DataFree (FS_NAND_PART_DATA  *p_part_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
                                                                /* Add to free pool.                                    */
    p_part_data->NextPtr         = FS_NAND_PartONFI_ListFreePtr;
    FS_NAND_PartONFI_ListFreePtr = p_part_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                     FS_NAND_PartONFI_DataGet()
*
* Description : Allocate & initialize a NAND part data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a NAND part data object, if NO errors.
*               Null pointer                      , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_NAND_PART_DATA  *FS_NAND_PartONFI_DataGet (void)
{
    LIB_ERR             alloc_err;
    CPU_SIZE_T          octets_reqd;
    FS_NAND_PART_DATA  *p_nand_part_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_NAND_PartONFI_ListFreePtr == (FS_NAND_PART_DATA *)0) {
        p_nand_part_data = (FS_NAND_PART_DATA *)Mem_HeapAlloc(sizeof(FS_NAND_PART_DATA),
                                                              sizeof(CPU_INT32U),
                                                             &octets_reqd,
                                                             &alloc_err);
        if (p_nand_part_data == (FS_NAND_PART_DATA *)0u) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FS_NAND_Part_DataGet(): Could not alloc mem for NAND part data: %d octets required.\r\n", octets_reqd));
            return ((FS_NAND_PART_DATA *)0u);
        }
        (void)alloc_err;

        FS_NAND_PartONFI_UnitCtr++;


    } else {
        p_nand_part_data             = FS_NAND_PartONFI_ListFreePtr;
        FS_NAND_PartONFI_ListFreePtr = FS_NAND_PartONFI_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    p_nand_part_data->NextPtr          = (FS_NAND_PART_DATA *)0u;

    p_nand_part_data->BlkCnt           = 0;
    p_nand_part_data->PgPerBlk         = 0;
    p_nand_part_data->PgSize           = 0;
    p_nand_part_data->SpareSize        = 0;
    p_nand_part_data->NbrPgmPerPg      = 0;
    p_nand_part_data->BusWidth         = 0;
    p_nand_part_data->ECC_NbrCorrBits  = 0;
    p_nand_part_data->ECC_CodewordSize = 0;
    p_nand_part_data->DefectMarkType   = DEFECT_SPARE_L_1_PG_1_OR_N_ALL_0;
    p_nand_part_data->MaxBadBlkCnt     = 0;
    p_nand_part_data->MaxBlkErase      = 0;
    p_nand_part_data->FreeSpareMap     = 0;


    return (p_nand_part_data);
}

