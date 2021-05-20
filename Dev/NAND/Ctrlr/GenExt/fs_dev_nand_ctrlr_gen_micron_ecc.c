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
*                      NAND DEVICE GENERIC CONTROLLER MICRON ON-CHIP ECC EXTENSION
*
* Filename : fs_dev_nand_ctrlr_gen_micron_ecc.c
* Version  : V4.08.01
* Note(s)  : (1) This controller generic extension is compatible with some Micron
*                NAND devices with on-chip ECC hardware calculation, including:
*                    Micron 29F1G08ABADA
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_NAND_CGEN_MICRON_ECC_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "fs_dev_nand_ctrlr_gen_micron_ecc.h"


/*
*********************************************************************************************************
*                                             LOCAL DEFINES
*********************************************************************************************************
*/

#define  FS_NAND_CMD_RDSTATUS              0x70u
#define  FS_NAND_CMD_SET_FEATURES          0xEFu
#define  FS_NAND_CMD_GET_FEATURES          0xEEu
#define  FS_NAND_CMD_RDMODE                0x00u


/*
*********************************************************************************************************
*                                      STATUS REGISTER BIT DEFINES
*********************************************************************************************************
*/

#define  FS_NAND_SR_WRPROTECT              DEF_BIT_07
#define  FS_NAND_SR_BUSY                   DEF_BIT_06
#define  FS_NAND_SR_REWRITE                DEF_BIT_03
#define  FS_NAND_SR_CACHEPGMFAIL           DEF_BIT_01
#define  FS_NAND_SR_FAIL                   DEF_BIT_00


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

typedef  struct  micron_ecc_data  MICRON_ECC_DATA;
struct micron_ecc_data {
    FS_NAND_CTRLR_GEN_BSP_API   *BSP_Ptr;
    MICRON_ECC_DATA             *NextPtr;
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

MICRON_ECC_DATA  *FS_NAND_CtrlrGen_MicronECC_ListFreePtr;


/*
*********************************************************************************************************
*                                       LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void              Init       (FS_ERR                  *p_err);

static  void             *Open       (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                                      void                    *p_ext_cfg,
                                      FS_ERR                  *p_err);

static  void              Close      (void                    *p_ext_data);

static  void              RdStatusChk(void                    *p_ext_data,
                                      FS_ERR                  *p_err);

static  MICRON_ECC_DATA  *DataGet    (void);

static  void              DataFree   (MICRON_ECC_DATA         *p_ext_data);


/*
*********************************************************************************************************
*                            NAND GENERIC CTRLR MICRON HARDWARE ECC EXTENSION
*********************************************************************************************************
*/

FS_NAND_CTRLR_GEN_EXT  FS_NAND_CtrlrGen_MicronECC =  {
    Init,                                                       /* Init().                                              */
    Open,                                                       /* Open().                                              */
    Close,                                                      /* Close().                                             */
    DEF_NULL,                                                   /* Setup().                                             */
    RdStatusChk,                                                /* RdStatusChk().                                       */
    DEF_NULL,                                                   /* ECC_Calc().                                          */
    DEF_NULL,                                                   /* ECC_Verify().                                        */
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
* Description : Initialize NAND Controller Generic Micron On-Chip ECC extension.
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function :
*               -----   Argument validated by caller.
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
    FS_NAND_CtrlrGen_MicronECC_ListFreePtr = DEF_NULL;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                                Open()
*
* Description : Open extension module instance and enables the on-chip ECC hardware module.
*
* Argument(s) : p_ctrlr_data    Pointer to NAND Controller Generic data structure.
*               ------------    Argument validated by caller.
*
*               p_ext_cfg       Pointer to (unused) extension configuration structure.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               -----           Argument validated by caller.
*
*                                   FS_ERR_MEM_ALLOC    Failed to allocate memory for extension data.
*
* Return(s)   : Pointer to Micron ECC extension data.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  *Open (FS_NAND_CTRLR_GEN_DATA  *p_ctrlr_data,
                     void                    *p_ext_cfg,
                     FS_ERR                  *p_err)
{
    MICRON_ECC_DATA            *p_micron_ecc_data;
    FS_NAND_CTRLR_GEN_BSP_API  *p_bsp_api;
    CPU_INT08U                  cmd;
    CPU_INT08U                  addr;
    CPU_INT08U                  internal_ecc_data[4];


    (void)p_ext_cfg;
                                                                /* --------------- ALLOC AND INIT DATA ---------------- */
    p_micron_ecc_data = DataGet();
    if (p_micron_ecc_data == DEF_NULL) {
       *p_err = FS_ERR_MEM_ALLOC;
        return (DEF_NULL);
    }

    p_micron_ecc_data->BSP_Ptr = p_ctrlr_data->BSP_Ptr;

    p_bsp_api = p_micron_ecc_data->BSP_Ptr;

                                                                /* ------------------ ENABLE HW ECC ------------------- */
    cmd  = FS_NAND_CMD_SET_FEATURES;
    addr = 0x90u;
    internal_ecc_data[0] = 0x08u;
    internal_ecc_data[1] = 0x00u;
    internal_ecc_data[2] = 0x00u;
    internal_ecc_data[3] = 0x00u;

    p_bsp_api->ChipSelEn();

    FS_ERR_CHK_RTN(p_bsp_api->CmdWr( &cmd, 1u, p_err),
                   p_bsp_api->ChipSelDis(),
                   p_micron_ecc_data);
    FS_ERR_CHK_RTN(p_bsp_api->AddrWr(&addr, 1u, p_err),
                   p_bsp_api->ChipSelDis(),
                   p_micron_ecc_data);
    p_bsp_api->DataWr(&internal_ecc_data[0], 4u, 8u, p_err);

    p_bsp_api->WaitWhileBusy(p_ctrlr_data,                          /*    Wait until rdy.    */
                             FS_NAND_CtrlrGen_PollFnct,
                             (CPU_INT32U)5000u,
                             p_err);
    if (*p_err != FS_ERR_NONE) {
        p_bsp_api->ChipSelDis();
        FS_TRACE_DBG(("(fs_nand_ctrlr_gen_ext) Open(): Timeout occurred when sending command.\r\n"));
        return (p_micron_ecc_data);
    }

    p_bsp_api->ChipSelDis();

                                                                /* ------------------ CHECK HW ECC ------------------- */
    cmd = FS_NAND_CMD_GET_FEATURES;

    p_bsp_api->ChipSelEn();

    FS_ERR_CHK_RTN(p_bsp_api->CmdWr(&cmd, 1u, p_err),
                   p_bsp_api->ChipSelDis(),
                   p_micron_ecc_data);
    FS_ERR_CHK_RTN(p_bsp_api->AddrWr(&addr, 1u, p_err),
                   p_bsp_api->ChipSelDis(),
                   p_micron_ecc_data);

    p_bsp_api->WaitWhileBusy(p_ctrlr_data,                          /*    Wait until rdy.    */
                             FS_NAND_CtrlrGen_PollFnct,
                             (CPU_INT32U)5000u,
                             p_err);
    if (*p_err != FS_ERR_NONE) {
        p_bsp_api->ChipSelDis();
        FS_TRACE_DBG(("(fs_nand_ctrlr_gen_ext) Open(): Timeout occurred when sending command.\r\n"));
        return (p_micron_ecc_data);
    }

    p_bsp_api->DataRd(&internal_ecc_data[0], 4u, 8u, p_err);

    p_bsp_api->ChipSelDis();

    if (internal_ecc_data[0] != 0x08){
        FS_TRACE_DBG(("(fs_nand_ctrlr_gen_ext) Open(): Failed to enable on-chip ECC.\r\n"));
        return (p_micron_ecc_data);
    }


    return (p_micron_ecc_data);
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
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  Close (void  *p_ext_data)
{
    DataFree((MICRON_ECC_DATA *)p_ext_data);
}


/*
*********************************************************************************************************
*                                             RdStatusChk()
*
* Description : Check NAND page read operation status for ECC errors.
*
* Argument(s) : p_ext_data  Pointer to extension data.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               -----       Argument validated by caller.
*
*                               FS_ERR_ECC_CORR     A correctable read error occurred, data has been corrected.
*                               FS_ERR_ECC_UNCORR   An uncorrectable read error occurred.
*
* Return(s)   : none.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  RdStatusChk (void    *p_ext_data,
                           FS_ERR  *p_err)
{
    MICRON_ECC_DATA  *p_micron_ecc_data;
    CPU_INT08U        sr;
    CPU_INT08U        cmd;


    p_micron_ecc_data = (MICRON_ECC_DATA *)p_ext_data;


    cmd = FS_NAND_CMD_RDSTATUS;
    FS_ERR_CHK(p_micron_ecc_data->BSP_Ptr->CmdWr(&cmd, 1u, p_err), ;);
    FS_ERR_CHK(p_micron_ecc_data->BSP_Ptr->DataRd(&sr, 1u, 8u, p_err), ;);

    if (DEF_BIT_IS_SET(sr, FS_NAND_SR_FAIL) == DEF_YES) {
       *p_err = FS_ERR_ECC_UNCORR;
    } else {
        if (DEF_BIT_IS_SET(sr, FS_NAND_SR_REWRITE) == DEF_YES) {
           *p_err = FS_ERR_ECC_CRITICAL_CORR;
        }
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

static  MICRON_ECC_DATA  *DataGet (void)
{
    MICRON_ECC_DATA  *p_micron_ecc_data;
    CPU_SIZE_T        octets_reqd;
    LIB_ERR           alloc_err;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FS_NAND_CtrlrGen_MicronECC_ListFreePtr == (MICRON_ECC_DATA *)0) {
        p_micron_ecc_data = (MICRON_ECC_DATA *)Mem_HeapAlloc(sizeof(MICRON_ECC_DATA),
                                                             sizeof(CPU_INT32U),
                                                            &octets_reqd,
                                                            &alloc_err);
        if (p_micron_ecc_data == DEF_NULL) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("(fs_nand_ctrlr_gen_micron_ecc) DataGet(): Could not alloc mem for "
                          "NAND Generic Ctrlr Micron ECC data: %d octets required.\r\n",
                          octets_reqd));
            return (DEF_NULL);
        }
        (void)alloc_err;

    } else {
        p_micron_ecc_data                      = FS_NAND_CtrlrGen_MicronECC_ListFreePtr;
        FS_NAND_CtrlrGen_MicronECC_ListFreePtr = FS_NAND_CtrlrGen_MicronECC_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();

    return (p_micron_ecc_data);
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

static  void  DataFree (MICRON_ECC_DATA  *p_ext_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
                                                                /* Add to free pool.                                    */
    p_ext_data->NextPtr                    = FS_NAND_CtrlrGen_MicronECC_ListFreePtr;
    FS_NAND_CtrlrGen_MicronECC_ListFreePtr = p_ext_data;
    CPU_CRITICAL_EXIT();
}

