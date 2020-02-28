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
*                                             IDE DEVICES
*                                BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                              TEMPLATE
*
* Filename : bsp_fs_dev_ide.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  FS_DEV_IDE_BSP_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Dev/IDE/fs_dev_ide.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

                                        /* $$$$ Define max mode supported for each supported mode type. */
#define  FS_DEV_IDE_PIO_MAX_MODE                           4u
#define  FS_DEV_IDE_DMA_MAX_MODE                           2u


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
*                                             PIO TIMINGS
*
* Note(s) : (1) True IDE mode PIO timings are given in "CF+ & CF SPECIFICATION REV. 4.1", Table 22.
*               All timings are in nanoseconds.
*********************************************************************************************************
*/

typedef  struct  fs_dev_ide_timing_pio {
    CPU_INT16U  t0;
    CPU_INT08U  t1;
    CPU_INT08U  t2_16;
    CPU_INT16U  t2_08;
    CPU_INT08U  t2i;
    CPU_INT08U  t3;
    CPU_INT08U  t4;
    CPU_INT08U  t5;
    CPU_INT08U  t6;
    CPU_INT08U  t6z;
    CPU_INT08U  t7;
    CPU_INT08U  t8;
    CPU_INT08U  t9;
    CPU_INT08U  tRD;
    CPU_INT08U  tA;
    CPU_INT16U  tB;
    CPU_INT08U  tC;
} FS_DEV_IDE_TIMING_PIO;

   /*  t0,    t1,   t2_16,   t2_8,    t2i,   t3,   t4,    t5,   t6,   t6z,    t7,    t8,    t9,  tRD,    tA,     tB,   tC */
const  FS_DEV_IDE_TIMING_PIO  FSDev_IDE_Timings_PIO[5] = {
    {600u,   70u,   165u,   290u,    0u,   60u,   30u,   50u,   5u,   30u,   90u,   60u,   20u,   0u,   35u,   1250u,   5u},
    {383u,   50u,   125u,   290u,    0u,   45u,   20u,   35u,   5u,   30u,   50u,   45u,   15u,   0u,   35u,   1250u,   5u},
    {240u,   30u,   100u,   290u,    0u,   30u,   15u,   20u,   5u,   30u,   30u,   30u,   10u,   0u,   35u,   1250u,   5u},
    {180u,   30u,    80u,    80u,   70u,   30u,   10u,   20u,   5u,   30u,    0u,    0u,   10u,   0u,   35u,   1250u,   5u},
    {120u,   25u,    70u,    70u,   25u,   20u,   10u,   20u,   5u,   30u,    0u,    0u,   10u,   0u,   35u,   1250u,   5u},
};


/*
*********************************************************************************************************
*                                        MULTIWORD DMA TIMINGS
*
* Note(s) : (1) True IDE mode Multiword DMA timings are given in "CF+ & CF SPECIFICATION REV. 4.1", Table
*               23. All timings are in nanoseconds.
*********************************************************************************************************
*/

typedef  struct  fs_dev_ide_timing_dma {
    CPU_INT16U  t0;
    CPU_INT08U  tD;
    CPU_INT08U  tE;
    CPU_INT08U  tF;
    CPU_INT08U  tG;
    CPU_INT08U  tH;
    CPU_INT08U  tI;
    CPU_INT08U  tJ;
    CPU_INT08U  tKR;
    CPU_INT08U  tKW;
    CPU_INT08U  tLR;
    CPU_INT08U  tLW;
    CPU_INT08U  tM;
    CPU_INT08U  tN;
    CPU_INT08U  tZ;
} FS_DEV_IDE_TIMING_DMA;

   /*  t0,     tD,     tE,   tF,     tG,    tH,   tI,    tJ,   tKR,    tKW,    tLR,   tLW,    tM,    tN,   tZ */
const  FS_DEV_IDE_TIMING_DMA  FSDev_IDE_Timings_DMA[3] = {
    {480u,   215u,   150u,   5u,   100u,   20u,   0u,   20u,   50u,   215u,   120u,   40u,   50u,   15u,  20u},
    {150u,    80u,    60u,   5u,    30u,   15u,   0u,    5u,   50u,    50u,    40u,   40u,   30u,   10u,  25u},
    {120u,    70u,    50u,   5u,    20u,   10u,   0u,    5u,   25u,    25u,    35u,   35u,   25u,   10u,  25u},
};


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


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      FILE SYSTEM IDE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_Open()
*
* Description : Open (initialize) hardware.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : DEF_OK,   if interface was opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function will be called every time the device is opened.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_IDE_BSP_Open (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_Close()
*
* Description : Close (uninitialize) hardware.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called EVERY time the device is closed.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_Close (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_Lock()
*
* Description : Acquire IDE card bus lock.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called before the IDE driver begins to access the IDE bus.
*                   The application should NOT use the same bus to access another device until the
*                   matching call to 'FSDev_IDE_BSP_Unlock()' has been made.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_Lock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                       FSDev_IDE_BSP_Unlock()
*
* Description : Release IDE bus lock.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'FSDev_IDE_BSP_Lock()' will be called before the IDE driver begins to access the IDE
*                   bus.  The application should NOT use the same bus to access another device until the
*                   matching call to this function has been made.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_Unlock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_Reset()
*
* Description : Hardware-reset IDE device.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_Reset (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_RegRd()
*
* Description : Read from IDE device register.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               reg         Register to read :
*
*                               FS_DEV_IDE_REG_ERR          Error Register
*                               FS_DEV_IDE_REG_SC           Sector Count Registers
*                               FS_DEV_IDE_REG_SN           Sector Number Registers
*                               FS_DEV_IDE_REG_CYL          Cylinder Low Registers
*                               FS_DEV_IDE_REG_CYH          Cylinder High Registers
*                               FS_DEV_IDE_REG_DH           Card/Drive/Head Register
*                               FS_DEV_IDE_REG_STATUS       Status Register
*                               FS_DEV_IDE_REG_ALTSTATUS    Alternate Status
*
* Return(s)   : Register value.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  FSDev_IDE_BSP_RegRd (FS_QTY      unit_nbr,
                                 CPU_INT08U  reg)
{
    CPU_INT08U  reg_val;


    (void)unit_nbr;
    (void)reg;

    reg_val = 0u;       /* $$$$ Read register */
    return (reg_val);
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_RegWr()
*
* Description : Write to IDE device register.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               reg         Register to write :
*
*                               FS_DEV_IDE_REG_FR         Features Register
*                               FS_DEV_IDE_REG_SC         Sector Count Register
*                               FS_DEV_IDE_REG_SN         Sector Number Register
*                               FS_DEV_IDE_REG_CYL        Cylinder Low Register
*                               FS_DEV_IDE_REG_CYH        Cylinder High Register
*                               FS_DEV_IDE_REG_DH         Card/Drive/Head Register
*                               FS_DEV_IDE_REG_CMD        Command  Register
*                               FS_DEV_IDE_REG_DEVCTRL    Device Control
*
*               val         Value to write into register.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_RegWr (FS_QTY      unit_nbr,
                           CPU_INT08U  reg,
                           CPU_INT08U  val)
{
    (void)unit_nbr;
    (void)reg;
    (void)val;
}


/*
*********************************************************************************************************
*                                        FSDev_IDE_BSP_CmdWr()
*
* Description : Write 7-byte command to IDE device registers.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               cmd         Array holding command.
*
* Return(s)   : none.
*
* Note(s)     : (1) The 7-byte of the command should be written to IDE device registers as follows :
*
*                       REG_FR  = Byte 0
*                       REG_SC  = Byte 1
*                       REG_SN  = Byte 2
*                       REG_CYL = Byte 3
*                       REG_CYH = Byte 4
*                       REG_DH  = Byte 5
*                       REG_CMD = Byte 6
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_CmdWr (FS_QTY      unit_nbr,
                           CPU_INT08U  cmd[])
{
    (void)unit_nbr;
    (void)cmd;
}


/*
*********************************************************************************************************
*                                       FSDev_IDE_BSP_DataRd()
*
* Description : Read data from IDE device.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               p_dest      Pointer to destination memory buffer.
*
*               cnt         Number of octets to read.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_DataRd (FS_QTY       unit_nbr,
                            void        *p_dest,
                            CPU_SIZE_T   cnt)
{
    (void)unit_nbr;
    (void)p_dest;
    (void)cnt;
}


/*
*********************************************************************************************************
*                                       FSDev_IDE_BSP_DataWr()
*
* Description : Write data to IDE device.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               p_src       Pointer to source memory buffer.
*
*               cnt         Number of octets to write.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_DataWr (FS_QTY       unit_nbr,
                            void        *p_src,
                            CPU_SIZE_T   cnt)
{
    (void)unit_nbr;
    (void)p_src;
    (void)cnt;
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_BSP_DMA_Start()
*
* Description : Setup DMA for command (initialize channel).
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               p_data      Pointer to memory buffer.
*
*               cnt         Number of octets to transfer.
*
*               mode_type   Transfer mode type :
*
*                               FS_DEV_IDE_MODE_TYPE_DMA     Multiword DMA mode.
*                               FS_DEV_IDE_MODE_TYPE_UDMA    Ultra-DMA mode.
*
*               rd          Indicates whether transfer is read or write :
*
*                               DEF_YES    Transfer is read.
*                               DEF_NO     Transfer is write.
*
* Return(s)   : none.
*
* Note(s)     : (1) DMA setup occurs before the command is executed (in 'FSDev_IDE_BSP_CmdWr()'.
*					Afterwards, data transmission completion must be confirmed (in 'FSDev_IDE_BSP_DMA_End()')
*					before the driver checks the command status.
*
*               (2) If the return value of 'FSDev_IDE_BSP_GetModesSupported()' does NOT include
*                  'FS_DEV_IDE_MODE_TYPE_DMA' or 'FS_DEV_IDE_MODE_TYPE_UDMA', this function need not
*                   be implemented; it will never be called.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_DMA_Start (FS_QTY        unit_nbr,
                               void         *p_data,
                               CPU_SIZE_T    cnt,
                               FS_FLAGS      mode_type,
                               CPU_BOOLEAN   rd)
{
    (void)unit_nbr;
    (void)p_data;
    (void)cnt;
    (void)mode_type;
    (void)rd;
}


/*
*********************************************************************************************************
*                                       FSDev_IDE_BSP_DMA_End()
*
* Description : End DMA transfer (& uninitialize channel).
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               p_data      Pointer to memory buffer.
*
*               cnt         Number of octets that should have been transfered.
*
*               rd          Indicates whether transfer was read or write :
*
*                               DEF_YES    Transfer was read.
*                               DEF_NO     Transfer was write.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDev_IDE_BSP_DMA_Start()  Note #1'.
*
*               (2) When this function returns, the host controller should be setup to transmit commands
*                   in PIO mode.
*
*               (3) If the return value of 'FSDev_IDE_BSP_GetModesSupported()' does NOT include
*                  'FS_DEV_IDE_MODE_TYPE_DMA' or 'FS_DEV_IDE_MODE_TYPE_UDMA', this function need not
*                   be implemented; it will never be called.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_DMA_End (FS_QTY        unit_nbr,
                             void         *p_data,
                             CPU_SIZE_T    cnt,
                             CPU_BOOLEAN   rd)
{
    (void)unit_nbr;
    (void)p_data;
    (void)cnt;
    (void)rd;
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_BSP_GetDrvNbr()
*
* Description : Get IDE drive number.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : Drive number (0 or 1).
*
* Note(s)     : (1) Two IDE devices may be accessed on the same bus by setting the DRV bit of the
*                   drive/head register.  If the bit should be clear, this function should return 0; if
*                   the bit should be set, this function should return 1.
*********************************************************************************************************
*/

CPU_INT08U  FSDev_IDE_BSP_GetDrvNbr (FS_QTY  unit_nbr)
{
    CPU_INT08U  drv_nbr;


    drv_nbr = (unit_nbr == 0u) ? 0u : 1u;
    return (drv_nbr);
}


/*
*********************************************************************************************************
*                                  FSDev_IDE_BSP_GetModesSupported()
*
* Description : Get supported transfer modes.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : Bit-mapped variable indicating supported transfer mode(s); should be the bitwise OR of
*               one or more of :
*
*                   FS_DEV_IDE_MODE_TYPE_PIO     PIO mode supported.
*                   FS_DEV_IDE_MODE_TYPE_DMA     Multiword DMA mode supported.
*                   FS_DEV_IDE_MODE_TYPE_UDMA    Ultra-DMA mode supported.
*
* Note(s)     : (1) See 'fs_dev_ide.h  MODE DEFINES'.
*********************************************************************************************************
*/

FS_FLAGS  FSDev_IDE_BSP_GetModesSupported (FS_QTY  unit_nbr)
{
    FS_FLAGS  mode_types;


    (void)unit_nbr;

    mode_types = FS_DEV_IDE_MODE_TYPE_PIO | FS_DEV_IDE_MODE_TYPE_DMA;
    return (mode_types);
}


/*
*********************************************************************************************************
*                                       FSDev_IDE_BSP_SetMode()
*
* Description : Set transfer mode timings.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
*               mode_type   Transfer mode type :
*
*                               FS_DEV_IDE_MODE_TYPE_PIO     PIO mode.
*                               FS_DEV_IDE_MODE_TYPE_DMA     Multiword DMA mode.
*                               FS_DEV_IDE_MODE_TYPE_UDMA    Ultra-DMA mode.
*
*               mode        Transfer mode, between 0 and maximum mode supported for mode type by device
*                           (inclusive).
*
* Return(s)   : Mode selected; should be between 0 and 'mode', inclusive.
*
* Note(s)     : (1) If DMA is supported, two transfer modes will be setup.  The first will be a PIO mode;
*                   the second will be a Multiword DMA or Ultra-DMA mode.  Thereafter, the host controller
*                   or bus interface must be capable of both PIO and DMA transfers.
*********************************************************************************************************
*/

CPU_INT08U  FSDev_IDE_BSP_SetMode (FS_QTY      unit_nbr,
                                   FS_FLAGS    mode_type,
                                   CPU_INT08U  mode)
{
    CPU_INT08U  mode_sel;


    (void)unit_nbr;

    switch (mode_type) {
        case FS_DEV_IDE_MODE_TYPE_DMA:                          /* ----------------- CFG DMA TIMINGS ------------------ */
             mode_sel = DEF_MAX(mode, FS_DEV_IDE_DMA_MAX_MODE);

             /* $$$$ Set multiword DMA mode timings */

             break;


        case FS_DEV_IDE_MODE_TYPE_PIO:                          /* ------------------ CFG PIO TIMINGS ----------------- */
        default:
             mode_sel = DEF_MAX(mode, FS_DEV_IDE_PIO_MAX_MODE);

             /* $$$$ Set PIO mode timings */

             break;
    }

    return (mode_sel);
}


/*
*********************************************************************************************************
*                                      FSDev_IDE_BSP_Dly400_ns()
*
* Description : Delay for 400 ns.
*
* Argument(s) : unit_nbr    Unit number of IDE device.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_IDE_BSP_Dly400_ns (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}
