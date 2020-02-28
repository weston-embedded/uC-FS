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
*                                          NOR FLASH DEVICES
*                              AMD-COMPATIBLE NOR PHYSICAL-LAYER DRIVER
*
* Filename : fs_dev_nor_amd_1x16.h
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Supports CFI NOR flash implementing AMD command set, including :
*
*                (a) Most AMD & Spansion devices.
*                (b) Most ST/Numonyx M29 devices.
*                (c) #### List tested devices.
*
*            (2) Fast programming command "write to buffer & program", supported by many flash
*                implementing AMD command set, is used in this driver if the "Maximum number of bytes
*                in a multi-byte write" (in the CFI device geometry definition) is non-zero.
*
*                (a) Some flash implementing AMD command set have non-zero multi-byte write size but
*                    do NOT support the "write to buffer & program" command.  Often, these devices
*                    will support alternate fast programming methods (e.g., "double word program" or
*                   "quadruple word program").  This driver MUST be modified for those devices, to
*                    ignore the multi-byte write size in the CFI information (see 'FSDev_NOR_PHY_Open()
*                    Note #2'.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_DEV_NOR_AMD_1X16_PRESENT
#define  FS_DEV_NOR_AMD_1X16_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_DEV_NOR_AMD_1X16_MODULE
#define  FS_DEV_NOR_AMD_1X16_EXT
#else
#define  FS_DEV_NOR_AMD_1X16_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../../Source/fs_dev.h"
#include  "../fs_dev_nor.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/


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

extern  const  FS_DEV_NOR_PHY_API  FSDev_NOR_AMD_1x16;


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


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of NOR AMD module include.                       */
