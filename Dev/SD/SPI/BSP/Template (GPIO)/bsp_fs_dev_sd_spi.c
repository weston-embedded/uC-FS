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
*                                       SD/MMC CARD - SPI MODE
*                                BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                           TEMPLATE (GPIO)
*
* Filename : bsp_fs_dev_sd_spi.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  FS_DEV_SD_SPI_BSP_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <Source/fs_type.h>
#include  <Source/fs_err.h>
#include  <Dev/SD/SPI/fs_dev_sd_spi.h>


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

#define  FS_DEV_BSP_SCK_CLR()               /* $$$$ Clr SCK.  */
#define  FS_DEV_BSP_SCK_SET()               /* $$$$ Set SCK.  */

#define  FS_DEV_BSP_SSEL_CLR()              /* $$$$ Clr SSEL. */
#define  FS_DEV_BSP_SSEL_SET()              /* $$$$ Set SSEL. */

#define  FS_DEV_BSP_MISO_RD()              0/* $$$$ Rd  MISO. */

#define  FS_DEV_BSP_MOSI_CLR()              /* $$$$ Clr MOSI. */
#define  FS_DEV_BSP_MOSI_SET()              /* $$$$ Set MOSI. */


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                               /* ----------- SPI API FNCTS ----------- */
static  CPU_BOOLEAN  FSDev_BSP_SPI_Open             (FS_QTY         unit_nbr); /* Open (initialize) SPI.                */

static  void         FSDev_BSP_SPI_Close            (FS_QTY         unit_nbr); /* Close (uninitialize) SPI.             */

static  void         FSDev_BSP_SPI_Lock             (FS_QTY         unit_nbr); /* Acquire SPI lock.                     */

static  void         FSDev_BSP_SPI_Unlock           (FS_QTY         unit_nbr); /* Release SPI lock.                     */

static  void         FSDev_BSP_SPI_Rd               (FS_QTY         unit_nbr,  /* Rd from SPI.                          */
                                                     void          *p_dest,
                                                     CPU_SIZE_T     cnt);

static  void         FSDev_BSP_SPI_Wr               (FS_QTY         unit_nbr,  /* Wr to SPI.                            */
                                                     void          *p_src,
                                                     CPU_SIZE_T     cnt);

static  void         FSDev_BSP_SPI_ChipSelEn        (FS_QTY         unit_nbr); /* En flash chip sel.                    */

static  void         FSDev_BSP_SPI_ChipSelDis       (FS_QTY         unit_nbr); /* Dis flash chip sel.                   */

static  void         FSDev_BSP_SPI_SetClkFreq       (FS_QTY         unit_nbr,  /* Set SPI clk freq.                     */
                                                     CPU_INT32U     freq);

                                                                               /* ------------ LOCAL FNCTS ------------ */
static  void         FSDev_BSP_Dly                  (FS_QTY       unit_nbr);


/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_SPI_API  FSDev_SD_SPI_BSP_SPI = {
    FSDev_BSP_SPI_Open,
    FSDev_BSP_SPI_Close,
    FSDev_BSP_SPI_Lock,
    FSDev_BSP_SPI_Unlock,
    FSDev_BSP_SPI_Rd,
    FSDev_BSP_SPI_Wr,
    FSDev_BSP_SPI_ChipSelEn,
    FSDev_BSP_SPI_ChipSelDis,
    FSDev_BSP_SPI_SetClkFreq,
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    FILE SYSTEM SD SPI FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        FSDev_BSP_SPI_Open()
*
* Description : Open (initialize) SPI for SD/MMC.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : DEF_OK,   if interface was opened.
*               DEF_FAIL, otherwise.
*
* Note(s)     : (1) This function will be called every time the device is opened.
*
*               (2) A SD/MMC slot is connected to the following pins :
*
*                   ----------------------------------------------
*                   |   #### NAME   |  #### PIO #  | SD/MMC NAME |
*                   ----------------------------------------------
*                   |    SSEL       |    P0[16]    | CS  (pin 1) |
*                   |    MOSI       |    P0[18]    | DI  (pin 2) |
*                   |    SCK        |    P0[15]    | CLK (pin 5) |
*                   |    MISO       |    P0[17]    | D0  (pin 7) |
*                   ----------------------------------------------
*********************************************************************************************************
*/

static  CPU_BOOLEAN  FSDev_BSP_SPI_Open (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    /* $$$$ Init GPIO. */
}


/*
*********************************************************************************************************
*                                        FSDev_BSP_SPI_Close()
*
* Description : Close (uninitialize) SPI for SD/MMC.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called every time the device is closed.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Close (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                        FSDev_BSP_SPI_Lock()
*
* Description : Acquire SPI lock.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called before the SD/MMC driver begins to access the SPI.  The
*                   application should NOT use the same bus to access another device until the matching
*                   call to 'FSDev_BSP_SPI_Unlock()' has been made.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Lock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                       FSDev_BSP_SPI_Unlock()
*
* Description : Release SPI lock.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'FSDev_BSP_SPI_Lock()' will be called before the SD/MMC driver begins to access the SPI.
*                   The application should NOT use the same bus to access another device until the
*                   matching call to this function has been made.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Unlock (FS_QTY  unit_nbr)
{
    (void)unit_nbr;
}


/*
*********************************************************************************************************
*                                         FSDev_BSP_SPI_Rd()
*
* Description : Read from SPI.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_dest    Pointer to destination memory buffer.
*
*               cnt       Number of octets to read.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Rd (FS_QTY       unit_nbr,
                                void        *p_dest,
                                CPU_SIZE_T   cnt)
{
    CPU_INT08U  *p_dest_08;
    CPU_INT08U   datum;
    CPU_INT08U   mask;
    CPU_INT32U   miso_val;
    CPU_INT08U   i;


    p_dest_08 = (CPU_INT08U *)p_dest;

    while (cnt > 0u) {
        datum = 0x00u;
        mask  = DEF_BIT_07;

        for (i = 0u; i < DEF_OCTET_NBR_BITS; i++) {
            FS_DEV_BSP_MOSI_SET();                              /* Wr bit.                                              */

            FS_DEV_BSP_SCK_CLR();                               /* Clr clk.                                             */
            FSDev_BSP_Dly(unit_nbr);

            miso_val = FS_DEV_BSP_MISO_RD();                    /* Rd bit.                                              */
            if (miso_val != 0u) {
                datum |= mask;
            }

            FS_DEV_BSP_SCK_SET();                               /* Set clk.                                             */
            FSDev_BSP_Dly(unit_nbr);

            mask >>= 1;
        }

       *p_dest_08 = datum;
        p_dest_08++;
        cnt--;
    }

    FS_DEV_BSP_SCK_CLR();                                       /* Clr clk (idle state).                                */
}


/*
*********************************************************************************************************
*                                         FSDev_BSP_SPI_Wr()
*
* Description : Write to SPI.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               p_src     Pointer to source memory buffer.
*
*               cnt       Number of octets to write.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Wr (FS_QTY       unit_nbr,
                                void        *p_src,
                                CPU_SIZE_T   cnt)
{
    CPU_INT08U  *p_src_08;
    CPU_INT08U   datum;
    CPU_INT08U   mask;
    CPU_INT32U   miso_val;
    CPU_INT08U   i;


    p_src_08 = (CPU_INT08U *)p_src;

    while (cnt > 0u) {
        datum = *p_src_08;
        mask  =  DEF_BIT_07;

        for (i = 0u; i < DEF_OCTET_NBR_BITS; i++) {
            if (DEF_BIT_IS_SET(datum, mask) == DEF_YES) {       /* Wr bit.                                              */
                FS_DEV_BSP_MOSI_SET();
            } else {
                FS_DEV_BSP_MOSI_CLR();
            }

            FS_DEV_BSP_SCK_CLR();                               /* Clr clk.                                             */
            FSDev_BSP_Dly(unit_nbr);

            miso_val = FS_DEV_BSP_MISO_RD();                    /* Rd bit.                                              */
            (void)miso_val;

            FS_DEV_BSP_SCK_SET();                               /* Set clk.                                             */
            FSDev_BSP_Dly(unit_nbr);

            mask >>= 1;
        }

        p_src_08++;
        cnt--;
    }

    FS_DEV_BSP_SCK_CLR();                                       /* Clr clk (idle state).                                */
}


/*
*********************************************************************************************************
*                                      FSDev_BSP_SPI_ChipSelEn()
*
* Description : Enable SD/MMC chip select.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) The chip select is typically 'active low'; to enable the card, the chip select pin
*                   should be cleared.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_ChipSelEn (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    FS_DEV_BSP_SCK_CLR();
    FS_DEV_BSP_SSEL_CLR();
}


/*
*********************************************************************************************************
*                                     FSDev_BSP_SPI_ChipSelDis()
*
* Description : Disable SD/MMC chip select.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
* Return(s)   : none.
*
* Note(s)     : (1) The chip select is typically 'active low'; to disable the card, the chip select pin
*                   should be set.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_ChipSelDis (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    FS_DEV_BSP_SSEL_SET();
    FS_DEV_BSP_SCK_CLR();
}


/*
*********************************************************************************************************
*                                     FSDev_BSP_SPI_SetClkFreq()
*
* Description : Set SPI clock frequency.
*
* Argument(s) : unit_nbr  Unit number of SD/MMC card.
*
*               freq      Clock frequency, in Hz.
*
* Return(s)   : none.
*
* Note(s)     : (1) The effective clock frequency MUST be no more than 'freq'.  If the frequency cannot be
*                   configured equal to 'freq', it should be configured less than 'freq'.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_SetClkFreq (FS_QTY      unit_nbr,
                                        CPU_INT32U  freq)
{
    (void)unit_nbr;
    (void)freq;

    /* $$$$ Calculate # clocks to delay in FSDev_BSP_Dly(). */
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           FSDev_BSP_Dly()
*
* Description : Delay between SPI clock pulses.
*
* Argument(s) : unit_nbr  Unit number of NAND.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_BSP_SPI_Dly (FS_QTY  unit_nbr)
{
    (void)unit_nbr;

    /* $$$$ Delay for certain # clks. */
}
