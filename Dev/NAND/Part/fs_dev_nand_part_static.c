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
*                                         NAND FLASH DEVICES
*                                STATIC CONFIGURATION PART-LAYER DRIVER
*
* Filename : fs_dev_nand_static.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_NAND_STATIC_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <fs_cfg.h>
#include  <fs_dev_nand_cfg.h>
#include  "../../../Source/fs.h"
#include  "../fs_dev_nand.h"
#include  "fs_dev_nand_part_static.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


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

#define  FS_NAND_PART_STATIC_CFG_FIELD(type, name, dflt_val)  dflt_val,
const  FS_NAND_PART_STATIC_CFG  FS_NAND_PartStatic_DfltCfg = { FS_NAND_PART_STATIC_CFG_FIELDS };
#undef   FS_NAND_PART_STATIC_CFG_FIELD

FS_NAND_PART_DATA  *FS_NAND_Part_ListFreePtr;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/
                                                                /* Get NAND part-specific data.                         */
static  FS_NAND_PART_DATA  *FS_NAND_PartStatic_Open    (const  FS_NAND_CTRLR_API  *p_ctrlr_api,
                                                        void                      *p_ctrlr_data_v,
                                                        void                      *p_part_cfg_v,
                                                        FS_ERR                    *p_err);

                                                                /* Close part layer inst.                               */
static  void                FS_NAND_PartStatic_Close   (FS_NAND_PART_DATA         *p_part_data);

                                                                /* Alloc part data struct.                              */
static  FS_NAND_PART_DATA  *FS_NAND_PartStatic_DataGet (void);

                                                                /* Free part data struct.                               */
static  void                FS_NAND_PartStatic_DataFree(FS_NAND_PART_DATA         *p_part_data);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_NAND_PART_API  FS_NAND_PartStatic = {
    FS_NAND_PartStatic_Open,                                    /* Open NAND part.                                      */
    FS_NAND_PartStatic_Close                                    /* Close NAND part.                                     */
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        FS_NAND_PartStatic_Open()
*
* Description : Open (initialize) a NAND part instance & get NAND device information.
*
* Argument(s) : p_ctrlr_api     Pointer to controller layer driver.
*               -----------     Argument validated by caller.
*
*               p_ctrlr_data_v  Pointer to controller layer data.
*               --------------  Argument validated by caller.
*
*               p_part_cfg_v    Pointer to part configuration.
*               ------------    Argument validated by caller.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_NONE                         Device driver initialized successfully.
*                                   FS_ERR_MEM_ALLOC                    Memory could not be allocated.
*
* Return(s)   : Pointer to part data, if NO errors;
*               NULL,                 otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_NAND_PART_DATA  *FS_NAND_PartStatic_Open (const  FS_NAND_CTRLR_API  *p_ctrlr_api,
                                                     void                      *p_ctrlr_data_v,
                                                     void                      *p_part_cfg_v,
                                                     FS_ERR                    *p_err)
{
    FS_NAND_PART_DATA        *p_part_data;
    FS_NAND_PART_STATIC_CFG  *p_part_cfg;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate err ptr.                                    */
        CPU_SW_EXCEPTION(DEF_NULL);
    }
    if (p_ctrlr_api == DEF_NULL) {                              /* Validate ctrlr api ptr.                              */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NULL);
    }
    if (p_ctrlr_data_v == DEF_NULL) {                           /* Validate ctrlr data ptr.                             */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NULL);
    }
    if (p_part_cfg_v == DEF_NULL) {                             /* Validate part cfg ptr.                               */
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NULL);
    }
#else
    (void)p_ctrlr_api;
    (void)p_ctrlr_data_v;
#endif

    p_part_data = FS_NAND_PartStatic_DataGet();
    if (p_part_data == DEF_NULL) {
        FS_TRACE_DBG(("FS_NAND_PartStatic_Open: Could not alloc mem for part data.\r\n"));
       *p_err = FS_ERR_MEM_ALLOC;
        return (DEF_NULL);
    }
    p_part_cfg = (FS_NAND_PART_STATIC_CFG *)p_part_cfg_v;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE CFG ------------------- */
                                                                /* Validate bus width.                                  */
    if ((p_part_cfg->BusWidth != 8u) && (p_part_cfg->BusWidth != 16u)) {
        FS_TRACE_DBG(("FS_NAND_PartStatic_Open(): Invalid bus width: %d. Must be 8 or 16.\r\n", p_part_cfg->BusWidth));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return (DEF_NULL);
    }

                                                                /* Validate defect mark len.                            */
    if (p_part_cfg->DefectMarkType >= DEFECT_MARK_TYPE_NBR) {
        FS_TRACE_DBG(("FS_NAND_PartStatic_Open(): Invalid defect mark type: %d.", p_part_cfg->DefectMarkType));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return (DEF_NULL);
    }

    if (p_part_cfg->FreeSpareMap == DEF_NULL) {
        FS_TRACE_DBG(("FS_NAND_PartStatic_Open(): Invalid Free Spare Map pointer(NULL).\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return (DEF_NULL);
    }
#endif

                                                                /* Cfg part data.                                       */
    p_part_data->BlkCnt           = p_part_cfg->BlkCnt;
    p_part_data->PgPerBlk         = p_part_cfg->PgPerBlk;
    p_part_data->PgSize           = p_part_cfg->PgSize;
    p_part_data->SpareSize        = p_part_cfg->SpareSize;
    p_part_data->NbrPgmPerPg      = p_part_cfg->NbrPgmPerPg;
    p_part_data->BusWidth         = p_part_cfg->BusWidth;
    p_part_data->ECC_NbrCorrBits  = p_part_cfg->ECC_NbrCorrBits;
    p_part_data->ECC_CodewordSize = p_part_cfg->ECC_CodewordSize;
    p_part_data->DefectMarkType   = p_part_cfg->DefectMarkType;
    p_part_data->MaxBadBlkCnt     = p_part_cfg->MaxBadBlkCnt;
    p_part_data->MaxBlkErase      = p_part_cfg->MaxBlkErase;
    p_part_data->FreeSpareMap     = p_part_cfg->FreeSpareMap;

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

static  void  FS_NAND_PartStatic_Close (FS_NAND_PART_DATA  *p_part_data)
{
    FS_NAND_PartStatic_DataFree(p_part_data);
}

/*
*********************************************************************************************************
*                                     FS_NAND_PartStatic_DataFree()
*
* Description : Free a NAND part data object.
*
* Argument(s) : p_part_data     Pointer to a NAND part data object.
*               -----------     Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FS_NAND_PartStatic_DataFree (FS_NAND_PART_DATA  *p_part_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_part_data->NextPtr = FS_NAND_Part_ListFreePtr;            /* Add to free pool.                                    */
    FS_NAND_Part_ListFreePtr  = p_part_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                  FS_NAND_PartStatic_DataGet()
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

static  FS_NAND_PART_DATA  *FS_NAND_PartStatic_DataGet (void)
{
    LIB_ERR             alloc_err;
    CPU_SIZE_T          octets_reqd;
    FS_NAND_PART_DATA  *p_part_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_NAND_Part_ListFreePtr == DEF_NULL) {
        p_part_data = (FS_NAND_PART_DATA *)Mem_HeapAlloc(sizeof(FS_NAND_PART_DATA),
                                                         sizeof(CPU_INT32U),
                                                        &octets_reqd,
                                                        &alloc_err);
        if (p_part_data == DEF_NULL) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FS_NAND_Part_DataGet(): Could not alloc mem for NAND part data: %d octets required.\r\n", octets_reqd));
            return (DEF_NULL);
        }
        (void)alloc_err;

        FS_NAND_PartStatic_UnitCtr++;

    } else {
        p_part_data              = FS_NAND_Part_ListFreePtr;
        FS_NAND_Part_ListFreePtr = FS_NAND_Part_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    p_part_data->NextPtr = DEF_NULL;

    return (p_part_data);
}
