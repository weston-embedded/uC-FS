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
*                               NAND FLASH DEVICES - GENERIC CONTROLLER
*                                BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                              TEMPLATE
*
* Filename : bsp_fs_dev_nand_ctrlr_gen.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <lib_def.h>
#include  <Dev/NAND/Ctrlr/fs_dev_nand_ctrlr_gen.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          REGISTER DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        REGISTER BIT DEFINES
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


/*
*********************************************************************************************************
*                                            LOCAL MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_Open         (FS_ERR        *p_err);

static  void  FS_NAND_BSP_Close        (void);

static  void  FS_NAND_BSP_ChipSelEn    (void);

static  void  FS_NAND_BSP_ChipSelDis   (void);

static  void  FS_NAND_BSP_CmdWr        (CPU_INT08U    *p_cmd,
                                        CPU_SIZE_T     cnt,
                                        FS_ERR        *p_err);

static  void  FS_NAND_BSP_AddrWr       (CPU_INT08U    *p_addr,
                                        CPU_SIZE_T     cnt,
                                        FS_ERR        *p_err);

static  void  FS_NAND_BSP_DataWr       (void          *p_src,
                                        CPU_SIZE_T     cnt,
                                        CPU_INT08U     width,
                                        FS_ERR        *p_err);

static  void  FS_NAND_BSP_DataRd       (void          *p_dest,
                                        CPU_SIZE_T     cnt,
                                        CPU_INT08U     width,
                                        FS_ERR        *p_err);

static  void  FS_NAND_BSP_WaitWhileBusy(void          *poll_fnct_arg,
                                        CPU_BOOLEAN  (*poll_fnct)(void  *p_arg),
                                        CPU_INT32U     to_us,
                                        FS_ERR        *p_err);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*
* Note(s) : (1) (a) The name given to this structure must be unique to avoid conflicts with other BSPs.
*
*               (b) When initializing the generic controller layer, you must declare this structure
*                   as a 'extern const FS_NAND_CTRLR_GEN_BSP_API' in the source file containing
*                   the call to FSDev_Open(). This value should be assigned to the member .BSPPtr
*                   of the FS_NAND_CFG structure you pass to the function FSDev_Open().
*********************************************************************************************************
*/

const  FS_NAND_CTRLR_GEN_BSP_API  FS_NAND_BSP_TEMPLATE = {  /* &&&& Replace this name (see Note #1).                */
    FS_NAND_BSP_Open,
    FS_NAND_BSP_Close,
    FS_NAND_BSP_ChipSelEn,
    FS_NAND_BSP_ChipSelDis,
    FS_NAND_BSP_CmdWr,
    FS_NAND_BSP_AddrWr,
    FS_NAND_BSP_DataWr,
    FS_NAND_BSP_DataRd,
    FS_NAND_BSP_WaitWhileBusy
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
*                                         FS_NAND_BSP_Open()
*
* Description : Open (initialize) bus for NAND.
*
* Argument(s) : p_err  Pointer to variable that will receive the return error code from this function :
*
*                          FS_ERR_NONE  No error.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called every time the device is opened.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_Open (FS_ERR  *p_err)
{
                                                                /* &&&& Setup the bus/controller.                       */
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FS_NAND_BSP_Close()
*
* Description : Close (uninitialize) bus for NAND.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called every time the device is closed.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_Close (void)
{
                                                                /* &&&& Optional uninit.                                */
}


/*
*********************************************************************************************************
*                                       FS_NAND_BSP_ChipSelEn()
*
* Description : Enable NAND chip select.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : (1) If you are sharing the bus/hardware with other software, this function MUST get an
*                   exclusive resource lock and configure any peripheral that could have been setup
*                   differently by the other software.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_ChipSelEn (void)
{
                                                                /* &&&& En chip sel & optionally acquire lock.          */
}


/*
*********************************************************************************************************
*                                      FS_NAND_BSP_ChipSelDis()
*
* Description : Disable NAND chip select.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : (1) If you are sharing the bus/hardware with other software, this function MUST release
*                   the exclusive resource lock obtained in function FS_NAND_BSP_ChipSelEn().
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_ChipSelDis (void)
{
                                                                /* &&&& Dis chip sel & optionally release lock.         */
}


/*
*********************************************************************************************************
*                                        FS_NAND_BSP_AddrWr()
*
* Description : Write address to NAND.
*
* Argument(s) : p_addr  Pointer to buffer that holds address.
*
*               cnt     Number of octets to write (size of the address).
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE  No error.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function must send 'cnt' octets on the data bus of the NAND. The pins of this
*                   bus are usually labeled IO0_0 - IO7_0 or DQ0_0 - DQ7_0 in the datasheet of the
*                   NAND flash, as specified by the ONFI 3.0 specification. The ALE (Address Latch Enable)
*                   pin must also be set high to indicate the data is an address.
*********************************************************************************************************
*/

static void  FS_NAND_BSP_AddrWr (CPU_INT08U  *p_addr,
                                 CPU_SIZE_T   cnt,
                                 FS_ERR      *p_err)
{
    (void)p_addr;
    (void)cnt;

                                                                /* &&&& Wr addr on data bus with ALE set high.          */
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FS_NAND_BSP_CmdWr()
*
* Description : Write command to NAND.
*
* Argument(s) : p_cmd  Pointer to buffer that holds command.
*
*               cnt    Number of octets to write.
*
*               p_err  Pointer to variable that will receive the return error code from this function :
*
*                          FS_ERR_NONE  No error.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function must send 'cnt' octets on the data bus of the NAND. The pins of this
*                   bus are usually labeled IO0_0 - IO7_0 or DQ0_0 - DQ7_0 in the datasheet of the
*                   NAND flash, as specified by the ONFI 3.0 specification. The CLE (Command Latch Enable)
*                   pin must also be set high to indicate the data is an address.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_CmdWr (CPU_INT08U  *p_cmd,
                                 CPU_SIZE_T   cnt,
                                 FS_ERR      *p_err)
{
    (void)p_cmd;
    (void)cnt;

                                                                /* &&&& Wr cmd on data bus with CLE set high.           */
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_NAND_BSP_DataWr()
*
* Description : Write data to NAND.
*
* Argument(s) : p_src  Pointer to source memory buffer.
*
*               cnt    Number of octets to write.
*
*               width  Write access width, in bits.
*
*               p_err  Pointer to variable that will receive the return error code from this function :
*
*                          FS_ERR_NONE  No error.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function must send 'cnt' octets on the data bus of the NAND. The pins of this
*                   bus are usually labeled IO0_0 - IO15_0 or DQ0_0 - DQ15_0 in the datasheet of the
*                   NAND flash, as specified by the ONFI 3.0 specification. The CLE (Command Latch Enable)
*                   and ALE (Address Latch Enabled) pins must both be set low.
*
*               (2) When calling this function, the generic controller layer has already sent the proper
*                   address and command to the NAND Flash. The purpose of this function is only to write
*                   data at a specific address.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_DataWr (void        *p_src,
                                  CPU_SIZE_T   cnt,
                                  CPU_INT08U   width,
                                  FS_ERR      *p_err)
{
    (void)p_src;
    (void)cnt;
    (void)width;

                                                                /* &&&& Wr data on data bus with CLE and ALE set low.   */
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FS_NAND_BSP_DataRd()
*
* Description : Read data from NAND.
*
* Argument(s) : p_dest  Pointer to destination memory buffer.
*
*               cnt     Number of octets to read.
*
*               width   Read access width, in bits.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE  No error.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function must read 'cnt' octets on the data bus of the NAND. The pins of this
*                   bus are usually labeled IO0_0 - IO15_0 or DQ0_0 - DQ15_0 in the datasheet of the
*                   NAND flash, as specified by the ONFI 3.0 specification.
*
*               (2) When calling this function, the generic controller layer has already sent the proper
*                   address and command to the NAND Flash. The purpose of this function is only to read
*                   data at a specific address.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_DataRd (void        *p_dest,
                                  CPU_SIZE_T   cnt,
                                  CPU_INT08U   width,
                                  FS_ERR      *p_err)
{
    (void)p_dest;
    (void)cnt;
    (void)width;

                                                                /* &&&& Rd data on data bus.                            */
   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     FS_NAND_BSP_WaitWhileBusy()
*
* Description : Wait while NAND is busy.
*
* Argument(s) : poll_fnct_arg  Pointer to argument that must be passed to the poll_fnct, if needed.
*
*               poll_fnct      Pointer to function to poll, if there is no hardware ready/busy signal.
*
*               to_us          Timeout, in microseconds.
*
*               p_err          Pointer to variable that will receive the return error code from this function :
*
*                                  FS_ERR_NONE         No error.
*                                  FS_ERR_NULL_PTR     Null poll function pointer.
*                                  FS_ERR_DEV_TIMEOUT  Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) This template shows how to implement this function using the 'poll_fnct'. However,
*                   reading the appropriate register should be more efficient, if available. In this
*                   case, you should replace all the code in the function.
*********************************************************************************************************
*/

static  void  FS_NAND_BSP_WaitWhileBusy (void          *poll_fnct_arg,
                                         CPU_BOOLEAN  (*poll_fnct)(void  *p_arg),
                                         CPU_INT32U     to_us,
                                         FS_ERR        *p_err)
{
    CPU_INT32U   time_cur_us;
    CPU_INT32U   time_start_us;
    CPU_BOOLEAN  rdy;


    if (poll_fnct == DEF_NULL) {                                /* Validate poll fnct ptr.                              */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }

    time_cur_us   = 0u;                                         /* $$$$ GET CURRENT TIME, IN MICROSECONDS.              */
    time_start_us = time_cur_us;

    while (time_cur_us - time_start_us < to_us) {
        rdy = poll_fnct(poll_fnct_arg);
        if (rdy == DEF_OK) {
           *p_err = FS_ERR_NONE;
            return;
        }
        time_cur_us = 0u;                                       /* $$$$ GET CURRENT TIME, IN MICROSECONDS.              */
    }

    rdy = poll_fnct(poll_fnct_arg);                             /* Try again to ensure poll at or after timeout.        */
    if (rdy == DEF_OK) {
       *p_err = FS_ERR_NONE;
        return;
    }

   *p_err = FS_ERR_DEV_TIMEOUT;
    return;
}
