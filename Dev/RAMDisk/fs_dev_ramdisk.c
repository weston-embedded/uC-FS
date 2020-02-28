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
*                                              RAM DISK
*
* Filename : fs_dev_ramdisk.h
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_RAM_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  "../../Source/fs.h"
#include  "fs_dev_ramdisk.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       RAM DISK DATA DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_ram_data  FS_DEV_RAM_DATA;
struct  fs_dev_ram_data {
    FS_QTY            UnitNbr;

    FS_SEC_SIZE       SecSize;
    FS_SEC_QTY        Size;
    void             *DiskPtr;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR            StatRdCtr;
    FS_CTR            StatWrCtr;
#endif

    FS_DEV_RAM_DATA  *NextPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  const  CPU_CHAR  FSDev_RAM_Name[] = "ram";


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

static  FS_DEV_RAM_DATA  *FSDev_RAM_ListFreePtr;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                            /* ---------- DEV INTERFACE FNCTS --------- */
static  const  CPU_CHAR  *FSDev_RAM_NameGet (void);                         /* Get name of driver.                      */

static  void              FSDev_RAM_Init    (FS_ERR           *p_err);      /* Init driver.                             */

static  void              FSDev_RAM_Open    (FS_DEV           *p_dev,       /* Open device.                             */
                                             void             *p_dev_cfg,
                                             FS_ERR           *p_err);

static  void              FSDev_RAM_Close   (FS_DEV           *p_dev);      /* Close device.                            */

static  void              FSDev_RAM_Rd      (FS_DEV           *p_dev,       /* Read from device.                        */
                                             void             *p_dest,
                                             FS_SEC_NBR        start,
                                             FS_SEC_QTY        cnt,
                                             FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_RAM_Wr      (FS_DEV           *p_dev,       /* Write to device.                         */
                                             void             *p_src,
                                             FS_SEC_NBR        start,
                                             FS_SEC_QTY        cnt,
                                             FS_ERR           *p_err);
#endif

static  void              FSDev_RAM_Query   (FS_DEV           *p_dev,       /* Get device info.                         */
                                             FS_DEV_INFO      *p_info,
                                             FS_ERR           *p_err);

static  void              FSDev_RAM_IO_Ctrl (FS_DEV           *p_dev,       /* Perform device I/O control.              */
                                             CPU_INT08U        opt,
                                             void             *p_data,
                                             FS_ERR           *p_err);

                                                                            /* -------------- LOCAL FNCTS ------------- */
static  void              FSDev_RAM_DataFree(FS_DEV_RAM_DATA  *p_ram_data); /* Free RAM data.                           */

static  FS_DEV_RAM_DATA  *FSDev_RAM_DataGet (void);                         /* Allocate & initialize RAM data.          */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_API  FSDev_RAM = {
    FSDev_RAM_NameGet,
    FSDev_RAM_Init,
    FSDev_RAM_Open,
    FSDev_RAM_Close,
    FSDev_RAM_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_RAM_Wr,
#endif
    FSDev_RAM_Query,
    FSDev_RAM_IO_Ctrl
};


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DRIVER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSDev_RAM_NameGet()
*
* Description : Return name of device driver.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to string which holds device driver name.
*
* Note(s)     : (1) The name MUST NOT include the ':' character.
*
*               (2) The name MUST be constant; each time this function is called, the same name MUST be
*                   returned.
*********************************************************************************************************
*/

static  const  CPU_CHAR  *FSDev_RAM_NameGet (void)
{
    return (FSDev_RAM_Name);
}


/*
*********************************************************************************************************
*                                          FSDev_RAM_Init()
*
* Description : Initialize the driver.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE    Device driver initialized successfully.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function should initialize any structures, tables or variables that are common
*                   to all devices or are used to manage devices accessed with the driver.  This function
*                   SHOULD NOT initialize any devices; that will be done individually for each with
*                   device driver's 'Open()' function.
*********************************************************************************************************
*/

static  void  FSDev_RAM_Init (FS_ERR  *p_err)
{
    FSDev_RAM_UnitCtr     =  0u;
    FSDev_RAM_ListFreePtr = (FS_DEV_RAM_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_RAM_Open()
*
* Description : Open (initialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to open.
*               ----------  Argument validated by caller.
*
*               p_dev_cfg   Pointer to device configuration.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device opened successfully.
*                               FS_ERR_DEV_ALREADY_OPEN        Device is already opened.
*                               FS_ERR_DEV_INVALID_CFG         Device configuration specified invalid.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_INVALID_LOW_PARAMS  Device low-level device parameters invalid.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*                               FS_ERR_MEM_ALLOC               Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should NEVER be
*                   called when a device is already open.
*
*               (2) Some drivers may need to track whether a device has been previously opened
*                   (indicating that the hardware has previously been initialized).
*
*               (3) This function will be called EVERY time the device is opened.
*
*               (4) See 'FSDev_Open() Notes #3'.
*********************************************************************************************************
*/

static  void  FSDev_RAM_Open (FS_DEV  *p_dev,
                              void    *p_dev_cfg,
                              FS_ERR  *p_err)
{
    FS_DEV_RAM_DATA  *p_ram_data;
    FS_DEV_RAM_CFG   *p_ram_cfg;


                                                                /* ------------------- VALIDATE CFG ------------------- */
    if (p_dev_cfg == (void *)0) {                               /* Validate dev cfg.                                    */
        FS_TRACE_DBG(("FSDev_RAM_Open(): RAM disk cfg NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    p_ram_cfg = (FS_DEV_RAM_CFG *)p_dev_cfg;

    switch (p_ram_cfg->SecSize) {                               /* Validate sec size.                                   */
        case 512u:
        case 1024u:
        case 2048u:
        case 4096u:
             break;

        default:
            FS_TRACE_DBG(("FSDev_RAM_Open(): Invalid RAM disk sec size: %d.\r\n", p_ram_cfg->SecSize));
           *p_err = FS_ERR_DEV_INVALID_CFG;
            return;
    }

    if (p_ram_cfg->Size < 1u) {                                 /* Validate size.                                       */
        FS_TRACE_DBG(("FSDev_RAM_Open(): Invalid RAM disk size: %d.\r\n", p_ram_cfg->Size));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    if (p_ram_cfg->DiskPtr == (void *)0) {                      /* Validate disk ptr.                                   */
        FS_TRACE_DBG(("FSDev_RAM_Open(): RAM disk area specified as NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }



                                                                /* --------------------- INIT UNIT -------------------- */
    p_ram_data = FSDev_RAM_DataGet();
    if (p_ram_data == (FS_DEV_RAM_DATA *)0) {
       *p_err = FS_ERR_MEM_ALLOC;
        return;
    }

    p_ram_data->UnitNbr =  p_dev->UnitNbr;
    p_ram_data->SecSize =  p_ram_cfg->SecSize;
    p_ram_data->Size    =  p_ram_cfg->Size;
    p_ram_data->DiskPtr =  p_ram_cfg->DiskPtr;
    p_dev->DataPtr      = (void *)p_ram_data;

    FS_TRACE_INFO(("RAM DISK FOUND: Name    : \"ram:%d:\"\r\n", p_ram_data->UnitNbr));
    FS_TRACE_INFO(("                Sec Size: %d bytes\r\n",    p_ram_data->SecSize));
    FS_TRACE_INFO(("                Size    : %d secs \r\n",    p_ram_data->Size));


    *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_RAM_Close()
*
* Description : Close (uninitialize) a device instance.
*
* Argument(s) : p_dev       Pointer to device to close.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) This function will be called EVERY time the device is closed.
*********************************************************************************************************
*/

static  void  FSDev_RAM_Close (FS_DEV  *p_dev)
{
    FS_DEV_RAM_DATA  *p_ram_data;


    p_ram_data = (FS_DEV_RAM_DATA *)p_dev->DataPtr;
    FSDev_RAM_DataFree(p_ram_data);
}


/*
*********************************************************************************************************
*                                           FSDev_RAM_Rd()
*
* Description : Read from a device & store data in buffer.
*
* Argument(s) : p_dev       Pointer to device to read from.
*               ----------  Argument validated by caller.
*
*               p_dest      Pointer to destination buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of read.
*
*               cnt         Number of sectors to read.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Sector(s) read.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

static  void  FSDev_RAM_Rd (FS_DEV      *p_dev,
                            void        *p_dest,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    FS_DEV_RAM_DATA  *p_ram_data;
    FS_SEC_QTY        i;
    CPU_SIZE_T        cnt_octets;
    CPU_INT08U       *p_dest_08;
    CPU_INT08U       *p_sec_08;


    p_ram_data = (FS_DEV_RAM_DATA *)p_dev->DataPtr;

    p_dest_08  = (CPU_INT08U *) p_dest;
    p_sec_08   = (CPU_INT08U *)(p_ram_data->DiskPtr) + (start * p_ram_data->SecSize);
    cnt_octets = (CPU_SIZE_T  )(p_ram_data->SecSize);

    for (i = 0u; i < cnt; i++) {
        Mem_Copy(p_dest_08,
                 p_sec_08,
                 cnt_octets);

        p_dest_08 += cnt_octets;
        p_sec_08  += cnt_octets;
    }

    FS_CTR_STAT_ADD(p_ram_data->StatRdCtr, (FS_CTR)cnt);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSDev_RAM_Wr()
*
* Description : Write data to a device from a buffer.
*
* Argument(s) : p_dev       Pointer to device to write to.
*               ----------  Argument validated by caller.
*
*               p_src       Pointer to source buffer.
*               ----------  Argument validated by caller.
*
*               start       Start sector of write.
*
*               cnt         Number of sectors to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void  FSDev_RAM_Wr (FS_DEV      *p_dev,
                            void        *p_src,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    FS_DEV_RAM_DATA  *p_ram_data;
    FS_SEC_QTY        i;
    CPU_SIZE_T        cnt_octets;
    CPU_INT08U       *p_sec_08;
    CPU_INT08U       *p_src_08;


    p_ram_data = (FS_DEV_RAM_DATA *)p_dev->DataPtr;

    p_src_08   = (CPU_INT08U *) p_src;
    p_sec_08   = (CPU_INT08U *)(p_ram_data->DiskPtr) + (start * p_ram_data->SecSize);
    cnt_octets = (CPU_SIZE_T  )(p_ram_data->SecSize);

    for (i = 0u; i < cnt; i++) {
        Mem_Copy(p_sec_08,
                 p_src_08,
                 cnt_octets);

        p_src_08 += cnt_octets;
        p_sec_08 += cnt_octets;
    }

    FS_CTR_STAT_ADD(p_ram_data->StatWrCtr, (FS_CTR)cnt);

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_RAM_Query()
*
* Description : Get information about a device.
*
* Argument(s) : p_dev       Pointer to device to query.
*               ----------  Argument validated by caller.
*
*               p_info      Pointer to structure that will receive device information.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device information obtained.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*********************************************************************************************************
*/

static  void  FSDev_RAM_Query (FS_DEV       *p_dev,
                               FS_DEV_INFO  *p_info,
                               FS_ERR       *p_err)
{
    FS_DEV_RAM_DATA  *p_ram_data;


    p_ram_data      = (FS_DEV_RAM_DATA *)p_dev->DataPtr;

    p_info->SecSize =  p_ram_data->SecSize;
    p_info->Size    =  p_ram_data->Size;
    p_info->Fixed   =  DEF_YES;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_RAM_IO_Ctrl()
*
* Description : Perform device I/O control operation.
*
* Argument(s) : p_dev       Pointer to device to control.
*               ----------  Argument validated by caller.
*
*               opt         Control command.
*
*               p_data      Buffer which holds data to be used for operation.
*                              OR
*                           Buffer in which data will be stored as a result of operation.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Control operation performed successfully.
*                               FS_ERR_DEV_INVALID_IO_CTRL     I/O control operation unknown to driver.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Tracking whether a device is open is not necessary, because this should ONLY be
*                   called when a device is open.
*
*               (2) Defined I/O control operations are :
*
*                   (a) FS_DEV_IO_CTRL_REFRESH           Refresh device.               [*]
*                   (b) FS_DEV_IO_CTRL_LOW_FMT           Low-level format device.      [*]
*                   (c) FS_DEV_IO_CTRL_LOW_MOUNT         Low-level mount device.       [*]
*                   (d) FS_DEV_IO_CTRL_LOW_UNMOUNT       Low-level unmount device.     [*]
*                   (e) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level compact device.     [*]
*                   (f) FS_DEV_IO_CTRL_LOW_DEFRAG        Low-level defragment device.  [*]
*                   (g) FS_DEV_IO_CTRL_SEC_RELEASE       Release data in sector.       [*]
*                   (h) FS_DEV_IO_CTRL_PHY_RD            Read  physical device.        [*]
*                   (i) FS_DEV_IO_CTRL_PHY_WR            Write physical device.        [*]
*                   (j) FS_DEV_IO_CTRL_PHY_RD_PAGE       Read  physical device page.   [*]
*                   (k) FS_DEV_IO_CTRL_PHY_WR_PAGE       Write physical device page.   [*]
*                   (l) FS_DEV_IO_CTRL_PHY_ERASE_BLK     Erase physical device block.  [*]
*                   (m) FS_DEV_IO_CTRL_PHY_ERASE_CHIP    Erase physical device.        [*]
*
*                           [*] NOT SUPPORTED
*********************************************************************************************************
*/

static  void  FSDev_RAM_IO_Ctrl (FS_DEV      *p_dev,
                                 CPU_INT08U   opt,
                                 void        *p_data,
                                 FS_ERR      *p_err)
{
   (void)p_dev;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */
   (void)opt;
   (void)p_data;


                                                                /* ------------------ PERFORM I/O CTL ----------------- */
   *p_err = FS_ERR_DEV_INVALID_IO_CTRL;
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
*                                        FSDev_RAM_DataFree()
*
* Description : Free a RAM data object.
*
* Argument(s) : p_ram_data  Pointer to a RAM data object.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_RAM_DataFree (FS_DEV_RAM_DATA  *p_ram_data)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_ram_data->NextPtr   = FSDev_RAM_ListFreePtr;
    FSDev_RAM_ListFreePtr = p_ram_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         FSDev_RAM_DataGet()
*
* Description : Allocate & initialize a RAM data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a RAM data object, if NO errors.
*               Pointer to NULL,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_RAM_DATA  *FSDev_RAM_DataGet (void)
{
    LIB_ERR           alloc_err;
    CPU_SIZE_T        octets_reqd;
    FS_DEV_RAM_DATA  *p_ram_data;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FSDev_RAM_ListFreePtr == (FS_DEV_RAM_DATA *)0) {
        p_ram_data = (FS_DEV_RAM_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_RAM_DATA),
                                                       sizeof(CPU_INT32U),
                                                      &octets_reqd,
                                                      &alloc_err);
        if (p_ram_data == (FS_DEV_RAM_DATA *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_RAM_DataGet(): Could not alloc mem for RAM disk data: %d octets required.\r\n", octets_reqd));
            return ((FS_DEV_RAM_DATA *)0);
        }
        (void)alloc_err;

        FSDev_RAM_UnitCtr++;


    } else {
        p_ram_data            = FSDev_RAM_ListFreePtr;
        FSDev_RAM_ListFreePtr = FSDev_RAM_ListFreePtr->NextPtr;
    }
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    p_ram_data->SecSize   =  0u;
    p_ram_data->Size      =  0u;
    p_ram_data->DiskPtr   = (void *)0;

#if (FS_CFG_CTR_STAT_EN   == DEF_ENABLED)
    p_ram_data->StatRdCtr =  0u;
    p_ram_data->StatWrCtr =  0u;
#endif

    p_ram_data->NextPtr   = (FS_DEV_RAM_DATA *)0;

    return (p_ram_data);
}
