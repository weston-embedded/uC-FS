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
*                                     USB HOST MASS STORAGE CLASS
*                                           for uC/USB-Host
*
* Filename : fs_dev_msc.c
* Version  : V4.08.00
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/USB-Host V3.10 or newer is included in the project build.
*
*            (2) REQUIREs the following uC/USB features :
*
*                (a) Host stack.
*
*                (b) Host Mass Storage Class (MSC) driver.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_MSC_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    FS_DEV_MSC_MODULE
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  "../../Source/fs.h"
#include  "../../Source/fs_vol.h"
#include  "fs_dev_msc.h"


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
*                                      MSC DEVICE INFO DATA TYPE
*********************************************************************************************************
*/

typedef  struct  fs_dev_msc_data  FS_DEV_MSC_DATA;
struct  fs_dev_msc_data {
    FS_ID             ID;
    FS_QTY            UnitNbr;

    FS_SEC_SIZE       SecSize;
    FS_SEC_QTY        Size;
    USBH_MSC_DEV     *MSC_DevPtr;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    FS_CTR            StatRdCtr;
    FS_CTR            StatWrCtr;
#endif

    FS_DEV_MSC_DATA  *NextPtr;
    FS_DEV_MSC_DATA  *PrevPtr;
};


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  const  CPU_CHAR  FSDev_MSC_Name[] = "msc";


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

static                  FS_DEV_MSC_DATA  *FSDev_MSC_ListFreePtr;
static                  FS_DEV_MSC_DATA  *FSDev_MSC_ListUsedPtr;
static  FS_DEV_MSC_EXT  FS_ID             FSDev_MSC_UnitCtr;
static  FS_DEV_MSC_EXT  FS_QTY            FSDev_MSC_UnitAllocCtr;

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
static  const  CPU_CHAR  *FSDev_MSC_NameGet (void);                         /* Get name of driver.                      */

static  void              FSDev_MSC_Init    (FS_ERR           *p_err);      /* Init driver.                             */

static  void              FSDev_MSC_Open    (FS_DEV           *p_dev,       /* Open device.                             */
                                             void             *p_dev_cfg,
                                             FS_ERR           *p_err);

static  void              FSDev_MSC_Close   (FS_DEV           *p_dev);      /* Close device.                            */

static  void              FSDev_MSC_Rd      (FS_DEV           *p_dev,       /* Read from device.                        */
                                             void             *p_dest,
                                             FS_SEC_NBR        start,
                                             FS_SEC_QTY        cnt,
                                             FS_ERR           *p_err);

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
static  void              FSDev_MSC_Wr      (FS_DEV           *p_dev,       /* Write to device.                         */
                                             void             *p_src,
                                             FS_SEC_NBR        start,
                                             FS_SEC_QTY        cnt,
                                             FS_ERR           *p_err);
#endif

static  void              FSDev_MSC_Query   (FS_DEV           *p_dev,       /* Get device info.                         */
                                             FS_DEV_INFO      *p_info,
                                             FS_ERR           *p_err);

static  void              FSDev_MSC_IO_Ctrl (FS_DEV           *p_dev,       /* Perform device I/O control.              */
                                             CPU_INT08U        opt,
                                             void             *p_data,
                                             FS_ERR           *p_err);


                                                                            /* ------------ INTERNAL FNCTS ------------ */
static  FS_DEV_MSC_DATA  *FSDev_MSC_DataGet (void);                         /* Allocate & initialize MSC data.          */

static  void              FSDev_MSC_DataFree(FS_DEV_MSC_DATA  *p_msc_data); /* Free MSC data.                           */

static  FS_DEV_MSC_DATA  *FSDev_MSC_DataFind(USBH_MSC_DEV     *p_msc_dev);  /* Find MSC data.                           */

/*
*********************************************************************************************************
*                                         INTERFACE STRUCTURE
*********************************************************************************************************
*/

const  FS_DEV_API  FSDev_MSC = {
    FSDev_MSC_NameGet,
    FSDev_MSC_Init,
    FSDev_MSC_Open,
    FSDev_MSC_Close,
    FSDev_MSC_Rd,
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
    FSDev_MSC_Wr,
#endif
    FSDev_MSC_Query,
    FSDev_MSC_IO_Ctrl
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
*                                         FSDev_MSC_NameGet()
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

static  const  CPU_CHAR  *FSDev_MSC_NameGet (void)
{
    return (FSDev_MSC_Name);
}


/*
*********************************************************************************************************
*                                          FSDev_MSC_Init()
*
* Description : Initialize the driver.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
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

static  void  FSDev_MSC_Init (FS_ERR  *p_err)
{
    FSDev_MSC_UnitCtr      =  0u;
    FSDev_MSC_UnitAllocCtr =  0u;
    FSDev_MSC_ListFreePtr  = (FS_DEV_MSC_DATA *)0;
    FSDev_MSC_ListUsedPtr  = (FS_DEV_MSC_DATA *)0;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_MSC_Open()
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
*
*               (5) See 'FSDev_MSC_UnitAdd() Notes #2'.
*********************************************************************************************************
*/

static  void  FSDev_MSC_Open (FS_DEV  *p_dev,
                              void    *p_dev_cfg,
                              FS_ERR  *p_err)
{
    FS_SEC_SIZE       sec_size;
    FS_SEC_QTY        size;
    CPU_INT32U        usbh_size;
    USBH_ERR          err_usbh;
    USBH_MSC_DEV     *p_msc_dev;
    FS_DEV_MSC_DATA  *p_msc_data;


                                                                /* ------------------- VALIDATE CFG ------------------- */
    if (p_dev_cfg == (void *)0) {                               /* Validate dev cfg (see Note #5).                      */
        FS_TRACE_DBG(("FSDev_MSC_Open(): MSC dev cfg NULL ptr.\r\n"));
       *p_err = FS_ERR_DEV_INVALID_CFG;
        return;
    }

    p_msc_data     = (FS_DEV_MSC_DATA *)p_dev_cfg;
    p_dev->DataPtr = (void *)p_msc_data;



                                                                /* --------------------- INIT UNIT -------------------- */
    p_msc_dev = p_msc_data->MSC_DevPtr;
    err_usbh  = USBH_MSC_Init(p_msc_dev, 0u);                   /* Init MSC dev.                                        */

    if (err_usbh != USBH_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_MSC_Open(): Could not init MSC dev w/USB err: %d.\r\n", err_usbh));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    err_usbh = USBH_MSC_CapacityRd( p_msc_dev,                  /* Rd MSC capacity.                                     */
                                    0u,
                                   &usbh_size,
                                   &sec_size);

    if (err_usbh != USBH_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_MSC_Open(): Could not get MSC dev capacity w/USB err: %d.\r\n", err_usbh));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    size = usbh_size;

    p_msc_data->SecSize =  sec_size;
    p_msc_data->Size    =  size;

    FS_TRACE_INFO(("MSC DEVICE FOUND: Name    : \"msc:%d:\"\r\n", p_msc_data->UnitNbr));
    FS_TRACE_INFO(("                  Sec Size: %d bytes\r\n",    p_msc_data->SecSize));
    FS_TRACE_INFO(("                  Size    : %d secs \r\n",    p_msc_data->Size));

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          FSDev_MSC_Close()
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
*
*               (3) See 'FSDev_MSC_UnitAdd() Notes #2'.
*********************************************************************************************************
*/

static  void  FSDev_MSC_Close (FS_DEV  *p_dev)
{
    FS_DEV_MSC_DATA  *p_msc_data;


    p_msc_data = (FS_DEV_MSC_DATA *)p_dev->DataPtr;
    FSDev_MSC_DataFree(p_msc_data);
}


/*
*********************************************************************************************************
*                                           FSDev_MSC_Rd()
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

static  void  FSDev_MSC_Rd (FS_DEV      *p_dev,
                            void        *p_dest,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    CPU_INT32U        sec_size;
    USBH_ERR          err_usbh;
    USBH_MSC_DEV     *p_msc_dev;
    FS_DEV_MSC_DATA  *p_msc_data;


    p_msc_data = (FS_DEV_MSC_DATA *)p_dev->DataPtr;
    p_msc_dev  =  p_msc_data->MSC_DevPtr;
    sec_size   =  p_msc_data->SecSize;

    (void)USBH_MSC_Rd(             p_msc_dev,                   /* Rd MSC dev.                                          */
                                   0u,
                      (CPU_INT32U) start,
                      (CPU_INT16U) cnt,
                      (CPU_INT32U) sec_size,
                                   p_dest,
                                  &err_usbh);

    if (err_usbh != USBH_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_MSC_Open(): Could not rd MSC dev w/USB err: %d.\r\n", err_usbh));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    FS_CTR_STAT_ADD(p_msc_data->StatRdCtr, (FS_CTR)cnt);

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           FSDev_MSC_Wr()
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
static  void  FSDev_MSC_Wr (FS_DEV      *p_dev,
                            void        *p_src,
                            FS_SEC_NBR   start,
                            FS_SEC_QTY   cnt,
                            FS_ERR      *p_err)
{
    CPU_INT32U        sec_size;
    USBH_ERR          err_usbh;
    USBH_MSC_DEV     *p_msc_dev;
    FS_DEV_MSC_DATA  *p_msc_data;


    p_msc_data = (FS_DEV_MSC_DATA *)p_dev->DataPtr;
    p_msc_dev  =  p_msc_data->MSC_DevPtr;
    sec_size   =  p_msc_data->SecSize;

    (void)USBH_MSC_Wr(             p_msc_dev,                   /* Wr MSC dev.                                          */
                                   0u,
                      (CPU_INT32U) start,
                      (CPU_INT16U) cnt,
                      (CPU_INT32U) sec_size,
                                   p_src,
                                  &err_usbh);

    if (err_usbh != USBH_ERR_NONE) {
        FS_TRACE_DBG(("FSDev_MSC_Open(): Could not rd MSC dev w/USB err: %d.\r\n", err_usbh));
       *p_err = FS_ERR_DEV_IO;
        return;
    }

    FS_CTR_STAT_ADD(p_msc_data->StatWrCtr, (FS_CTR)cnt);

   *p_err = FS_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_MSC_Query()
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

static  void  FSDev_MSC_Query (FS_DEV       *p_dev,
                               FS_DEV_INFO  *p_info,
                               FS_ERR       *p_err)
{
    FS_DEV_MSC_DATA  *p_msc_data;


    p_msc_data      = (FS_DEV_MSC_DATA *)p_dev->DataPtr;

    p_info->SecSize =  p_msc_data->SecSize;
    p_info->Size    =  p_msc_data->Size;
    p_info->Fixed   =  DEF_NO;

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         FSDev_MSC_IO_Ctrl()
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

static  void  FSDev_MSC_IO_Ctrl (FS_DEV      *p_dev,
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
*                                      DRIVER CONTROL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSDev_MSC_DevOpen()
*
* Description : Add MSC unit.
*
* Argument(s) : p_msc_dev   Pointer to MSC device.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Unit added successfully.
*                               FS_ERR_NULL_PTR               Descriptor is NULL pointer.
*                               FS_ERR_DEV_UNIT_NONE_AVAIL    No device unit available.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout.
*
* Return(s)   : Unit number to which device has been assigned (see Note #2).
*
* Note(s)     : (1) 'FSDev_Open()' & 'FSDev_Close()' MUST NEVER be directly called by user applications
*                   for MSC devices.  Rather,  this MSC device open function & the MSC device close
*                   function should be used to open & close MSC devices.
*
*               (2) The return value should be used for form the name of the volume; for example, if the
*                   return value is 4, the volume name is "msc:4:".  A file name "file.txt" in the root
*                   directory of this volume would have the full path "msc:4\file.txt".
*********************************************************************************************************
*/

FS_QTY  FSDev_MSC_DevOpen (USBH_MSC_DEV  *p_msc_dev,
                           FS_ERR        *p_err)
{
    CPU_CHAR          name[8];
    FS_DEV_MSC_DATA  *p_msc_data;
    FS_QTY            unit_nbr;


    p_msc_data = FSDev_MSC_DataGet();                           /* Get empty info ...                                   */

    if (p_msc_data == (FS_DEV_MSC_DATA *)0) {                   /* ... rtn err if none found.                           */
       *p_err = FS_ERR_MEM_ALLOC;
        return (0u);
    }

    p_msc_data->MSC_DevPtr = p_msc_dev;
    unit_nbr               = p_msc_data->UnitNbr;

    (void)Str_Copy(&name[0], (CPU_CHAR *)"msc:x:");             /* Fmt dev name.                                        */
    name[4] = ASCII_CHAR_DIGIT_ZERO + (CPU_CHAR)unit_nbr;

    FSDev_Open(name, (void *)p_msc_data, p_err);                /* Open dev.                                            */
    if (*p_err != FS_ERR_NONE) {
         FSDev_Close(name, p_err);
        *p_err  = FS_ERR_DEV;
         return (0u);
    }

    FSVol_Open(name, name, 0u, p_err);                          /* Open vol.                                            */
    if (*p_err != FS_ERR_NONE) {
         FSVol_Close(name, p_err);
         FSDev_Close(name, p_err);
        *p_err  = FS_ERR_DEV;
         return (0u);
    }

   *p_err = FS_ERR_NONE;
    return (unit_nbr);
}


/*
*********************************************************************************************************
*                                        FSDev_MSC_DevClose()
*
* Description : Close MSC device.
*
* Argument(s) : p_msc_dev   Pointer to MSC device.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) See 'FSDev_MSC_DevOpen() Note #1'.
*********************************************************************************************************
*/

void  FSDev_MSC_DevClose (USBH_MSC_DEV  *p_msc_dev)
{
    FS_ERR            err;
    FS_QTY            unit_nbr;
    CPU_CHAR          name[8];
    FS_DEV_MSC_DATA  *p_msc_data;


    p_msc_data = FSDev_MSC_DataFind(p_msc_dev);
    if (p_msc_data == (FS_DEV_MSC_DATA *)0) {
        return;
    }

    unit_nbr = p_msc_data->UnitNbr;

    (void)Str_Copy(&name[0], (CPU_CHAR *)"msc:x:");             /* Fmt dev name.                                        */
    name[4] = ASCII_CHAR_DIGIT_ZERO + (CPU_CHAR)unit_nbr;

    FSVol_Close(name, &err);                                    /* Close vol.                                            */
    FSDev_Close(name, &err);                                    /* Close dev.                                            */
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
*                                        FSDev_MSC_DataFree()
*
* Description : Free a MSC data object.
*
* Argument(s) : p_msc_data  Pointer to a MSC data object.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  FSDev_MSC_DataFree (FS_DEV_MSC_DATA  *p_msc_data)
{
    CPU_SR_ALLOC();


    if (p_msc_data == DEF_NULL) {
        return;
    }

    CPU_CRITICAL_ENTER();
    if (p_msc_data->PrevPtr != (FS_DEV_MSC_DATA *)0) {          /* Remove from used pool.                               */
        p_msc_data->PrevPtr->NextPtr = p_msc_data->NextPtr;
    } else {
        FSDev_MSC_ListUsedPtr = p_msc_data->NextPtr;
    }

    if (p_msc_data->NextPtr != (FS_DEV_MSC_DATA *)0) {
        p_msc_data->NextPtr->PrevPtr = p_msc_data->PrevPtr;
    }


    p_msc_data->NextPtr   = FSDev_MSC_ListFreePtr;              /* Add to free pool.                                    */
    FSDev_MSC_ListFreePtr = p_msc_data;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                         FSDev_MSC_DataGet()
*
* Description : Allocate & initialize a MSC data object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a MSC data object, if NO errors.
*               Pointer to NULL,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_MSC_DATA  *FSDev_MSC_DataGet (void)
{
    LIB_ERR           alloc_err;
    CPU_SIZE_T        octets_reqd;
    FS_DEV_MSC_DATA  *p_msc_data;
    FS_ID             id;
    FS_QTY            unit_nbr;
    CPU_SR_ALLOC();


                                                                /* --------------------- ALLOC DATA ------------------- */
    CPU_CRITICAL_ENTER();
    if (FSDev_MSC_ListFreePtr == (FS_DEV_MSC_DATA *)0) {
        if (FSDev_MSC_UnitAllocCtr >= 9u) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_MSC_DataGet(): Max nbr devs alloc'd: %d\r\n", FSDev_MSC_UnitAllocCtr));
            return ((FS_DEV_MSC_DATA *)0);
        }

        p_msc_data = (FS_DEV_MSC_DATA *)Mem_HeapAlloc( sizeof(FS_DEV_MSC_DATA),
                                                       sizeof(CPU_DATA),
                                                      &octets_reqd,
                                                      &alloc_err);

        if (p_msc_data == (FS_DEV_MSC_DATA *)0) {
            CPU_CRITICAL_EXIT();
            FS_TRACE_DBG(("FSDev_MSC_DataGet(): Could not alloc mem for MSC dev data: %d octets required.\r\n", octets_reqd));
            return ((FS_DEV_MSC_DATA *)0);
        }

        (void)alloc_err;

        unit_nbr = FSDev_MSC_UnitAllocCtr;
        FSDev_MSC_UnitAllocCtr++;

    } else {
        p_msc_data            = FSDev_MSC_ListFreePtr;
        FSDev_MSC_ListFreePtr = FSDev_MSC_ListFreePtr->NextPtr;

        unit_nbr = p_msc_data->UnitNbr;
    }

    id = FSDev_MSC_UnitCtr;
    FSDev_MSC_UnitCtr++;
    CPU_CRITICAL_EXIT();


                                                                /* ---------------------- CLR DATA -------------------- */
    p_msc_data->ID        =  id;
    p_msc_data->UnitNbr   =  unit_nbr;

    p_msc_data->SecSize   =  0u;
    p_msc_data->Size      =  0u;
    p_msc_data->MSC_DevPtr = (USBH_MSC_DEV *)0;

#if (FS_CFG_CTR_STAT_EN   == DEF_ENABLED)
    p_msc_data->StatRdCtr =  0u;
    p_msc_data->StatWrCtr =  0u;
#endif


                                                                /* ------------------ ADD TO USED LIST ---------------- */
    CPU_CRITICAL_ENTER();
    p_msc_data->PrevPtr = (FS_DEV_MSC_DATA *)0;
    p_msc_data->NextPtr =  FSDev_MSC_ListUsedPtr;
    if (FSDev_MSC_ListUsedPtr != (FS_DEV_MSC_DATA *)0) {
        FSDev_MSC_ListUsedPtr->PrevPtr = p_msc_data;
    }
    FSDev_MSC_ListUsedPtr = p_msc_data;
    CPU_CRITICAL_EXIT();

    return (p_msc_data);
}


/*
*********************************************************************************************************
*                                         FSDev_MSC_DataFind()
*
* Description : Find a MSC data object.
*
* Argument(s) : p_msc_dev   Pointer to MSC device.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to a MSC data object, if found.
*               Pointer to NULL,              otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  FS_DEV_MSC_DATA  *FSDev_MSC_DataFind (USBH_MSC_DEV  *p_msc_dev)
{
    FS_DEV_MSC_DATA  *p_msc_data_list;
    FS_DEV_MSC_DATA  *p_msc_data;
    CPU_SR_ALLOC();


    p_msc_data      = (FS_DEV_MSC_DATA *)0;

    CPU_CRITICAL_ENTER();
    p_msc_data_list = FSDev_MSC_ListUsedPtr;
    while ((p_msc_data_list != (FS_DEV_MSC_DATA *)0) &&
           (p_msc_data      == (FS_DEV_MSC_DATA *)0)) {
        if (p_msc_data_list->MSC_DevPtr == p_msc_dev) {
            p_msc_data = p_msc_data_list;
        }
        p_msc_data_list = p_msc_data_list->NextPtr;
    }
    CPU_CRITICAL_EXIT();

    return (p_msc_data);
}
