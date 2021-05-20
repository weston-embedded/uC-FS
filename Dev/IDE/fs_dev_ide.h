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
*                                             IDE DEVICES
*
* Filename     : fs_dev_ide.h
* Version      : V4.08.01
*********************************************************************************************************
* Reference(s) : (1) CompactFlash Association.  "CF+ and CompactFlash Specification Revision 4.1".
*                    002/16/07.
*
*                (2) ANSI.  "AT Attachment with Packet Interface - 6 -- (ATA/ATAPI-6)."
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  FS_DEV_IDE_PRESENT
#define  FS_DEV_IDE_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   FS_DEV_IDE_MODULE
#define  FS_DEV_IDE_EXT
#else
#define  FS_DEV_IDE_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/fs_dev.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            MODE DEFINES
*
* Note(s) : (1) (a) True IDE PIO timing modes 0, 1, 2, 3 & 4 are defined in [Ref 2].  Additional CF-
*                   specific PIO timing modes 5 & 6 are defined by [Ref 1].  Timings for all modes are
*                   given in [Ref 1], Table 21.
*
*               (b) True IDE Multiword DMA timing modes 0, 1 & 2 are defined by [Ref 2].  Additional CF-
*                   specific multiword DMA timing modes 3 & 4 are defined by [Ref 1].  Timings for all
*                   modes are given in [Ref 1], Table 21.
*
*               (c) Ultra DMA timing modes 0, 1, 2, 3, 4, 5 & 6 are defined by [Ref 2].
*
*           (2) Multiword DMA is NEVER supported for CF cards accessed in PC Card Memory or PC Card I/O
*               interface Mode.  (See [Ref 1] Table 58.)
*
*           (3) ALL IDE device & host controllers MUST support PIO mode.
*********************************************************************************************************
*/

#define  FS_DEV_IDE_MODE_TYPE_PIO                 DEF_BIT_03    /* PIO mode.                                            */
#define  FS_DEV_IDE_MODE_TYPE_DMA                 DEF_BIT_05    /* Multiword DMA mode.                                  */
#define  FS_DEV_IDE_MODE_TYPE_UDMA                DEF_BIT_06    /* Ultra DMA mode.                                      */

/*
*********************************************************************************************************
*                                          REGISTER DEFINES
*
* Note(s) : (1) See [Ref 1], Section 6.1.5 'CF-ATA Registers'.  The register define values are the
*               offsets for contiguous I/O or memory mapped addressing.
*
*           (2) The register define values may NOT be the appropriate bus/memory offset.  [Ref 1], Tables
*               4 'Pin Assignments and Pin Type', 47 'Memory Mapped Decoding' & 48 'True IDE Mode I/O
*               Decoding', in addition to MCU/MPU datasheets and hardware schematics should be used to
*               determine the correct offset.
*
*               (a) For a primary   I/O mapped drive, the offsets may be 0x1F0-0x1F7, 0x3F6-0x3F7.
*               (b) For a secondary I/O mapped drive, the offsets may be 0x170-0x177, 0x376-0x377.
*               (c) In contiguous I/O & memory mapped addressing, several duplicate registers are available :
*                   (1) Duplicate even data @ 0x08
*                   (2) Duplicate odd  data @ 0x09
*                   (3) Duplicate error/features @ 0x0D
*               (d) In true IDE mode, ...
*                   (1) the registers DATA, ERR/FR, SC, SN, CYL, CYH, DH & STATUS/CMD are accessed at
*                       locations 0x00-0x07 with nCE1 active & nCE2 inactive.
*                   (2) the registers ALTSTATUS/DEVCTRL & DRVADDR are accessed at locations 0x06-0x07
*                       with nCE1 inactive & nCE2 active.
*
*           (3) The SN, CYL & CYH registers are also called the LBA_LOW, LBA_MID & LBA_HIGH registers.
*********************************************************************************************************
*/

#define  FS_DEV_IDE_REG_DATA                            0x00u   /* Data Register.                                       */
#define  FS_DEV_IDE_REG_ERR                             0x01u   /* Error Register (read).                               */
#define  FS_DEV_IDE_REG_FR                              0x01u   /* Features Register (write).                           */
#define  FS_DEV_IDE_REG_SC                              0x02u   /* Sector Count  Register.                              */
#define  FS_DEV_IDE_REG_SN                              0x03u   /* Sector Number Register.                              */
#define  FS_DEV_IDE_REG_CYL                             0x04u   /* Cylinder Low  Register.                              */
#define  FS_DEV_IDE_REG_CYH                             0x05u   /* Cylinder High Register.                              */
#define  FS_DEV_IDE_REG_DH                              0x06u   /* Card/Drive/Head Register.                            */
#define  FS_DEV_IDE_REG_STATUS                          0x07u   /* Status Register (read).                              */
#define  FS_DEV_IDE_REG_CMD                             0x07u   /* Command Register (write).                            */

#define  FS_DEV_IDE_REG_ALTSTATUS                       0x0Eu   /* Alternate Status Register (read).                    */
#define  FS_DEV_IDE_REG_DEVCTRL                         0x0Eu   /* Device Control Register (write).                     */


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

extern          const  FS_DEV_API  FSDev_IDE;
FS_DEV_IDE_EXT         FS_CTR      FSDev_IDE_UnitCtr;


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
*                                         FUNCTION PROTOTYPES
*                                 DEFINED IN BSP'S 'bsp_fs_dev_ide.c'
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_IDE_BSP_Open             (FS_QTY        unit_nbr);   /* Open (initialize) hardware.                  */

void         FSDev_IDE_BSP_Close            (FS_QTY        unit_nbr);   /* Close (uninitialize) hardware.               */

void         FSDev_IDE_BSP_Lock             (FS_QTY        unit_nbr);   /* Acquire IDE bus lock.                        */

void         FSDev_IDE_BSP_Unlock           (FS_QTY        unit_nbr);   /* Release IDE bus lock.                        */

void         FSDev_IDE_BSP_Reset            (FS_QTY        unit_nbr);   /* Hardware-reset IDE device.                   */


CPU_INT08U   FSDev_IDE_BSP_RegRd            (FS_QTY        unit_nbr,    /* Read data from IDE device register.          */
                                             CPU_INT08U    reg);

void         FSDev_IDE_BSP_RegWr            (FS_QTY        unit_nbr,    /* Write data to IDE device register.           */
                                             CPU_INT08U    reg,
                                             CPU_INT08U    val);

void         FSDev_IDE_BSP_CmdWr            (FS_QTY        unit_nbr,    /* Write cmd to IDE device.                     */
                                             CPU_INT08U    cmd[]);


void         FSDev_IDE_BSP_DataRd           (FS_QTY        unit_nbr,    /* Read data from IDE device.                   */
                                             void         *p_dest,
                                             CPU_SIZE_T    cnt);

void         FSDev_IDE_BSP_DataWr           (FS_QTY        unit_nbr,    /* Write data to IDE device.                    */
                                             void         *p_src,
                                             CPU_SIZE_T    cnt);


void         FSDev_IDE_BSP_DMA_Start        (FS_QTY        unit_nbr,    /* Setup DMA for command (initialize channel).  */
                                             void         *p_data,
                                             CPU_SIZE_T    cnt,
                                             FS_FLAGS      mode_type,
                                             CPU_BOOLEAN   rd);

void         FSDev_IDE_BSP_DMA_End          (FS_QTY        unit_nbr,    /* End DMA transfer (& uninitialize channel).   */
                                             void         *p_data,
                                             CPU_SIZE_T    cnt,
                                             CPU_BOOLEAN   rd);


CPU_INT08U   FSDev_IDE_BSP_GetDrvNbr        (FS_QTY        unit_nbr);   /* Get IDE drive number.                        */

FS_FLAGS     FSDev_IDE_BSP_GetModesSupported(FS_QTY        unit_nbr);   /* Get supported transfer modes.                */

CPU_INT08U   FSDev_IDE_BSP_SetMode          (FS_QTY        unit_nbr,    /* Set transfer mode.                           */
                                             FS_FLAGS      mode_type,
                                             CPU_INT08U    mode);

void         FSDev_IDE_BSP_Dly400_ns        (FS_QTY        unit_nbr);   /* Delay for 400 ns.                            */


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

#endif                                                          /* End of IDE module include.                           */
