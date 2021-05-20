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
*                                 FILE SYSTEM SUITE DEVICE MANAGEMENT
*
* Filename : fs_dev.c
* Version  : V4.08.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_DEV_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>
#include  <lib_ascii.h>
#include  <lib_mem.h>
#include  <lib_str.h>
#include  "fs.h"
#include  "fs_buf.h"
#include  "fs_dev.h"
#include  "fs_partition.h"


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

static  FS_DEV_API  **FS_DevDrvTbl;                             /* Tbl of dev drv.                                      */
static  FS_QTY        FS_DevDrvCntMax;                          /* Max nbr of dev drv.                                  */

static  MEM_POOL      FSDev_Pool;                               /* Pool of devices.                                     */
static  FS_DEV      **FSDev_Tbl;                                /* Tbl of dev.                                          */

static  FS_QTY        FSDev_DevCntMax;                          /* Maximum number of open devices.                      */
static  FS_QTY        FSDev_Cnt;


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/

#define  FS_DEV_IS_VALID_SIZE(size)            (((size) > 0u) ? (DEF_YES) : (DEF_NO))
#define  FS_DEV_IS_VALID_SEC_SIZE(sec_size) (( ((sec_size) <= FS_MaxSecSizeGet())              && \
                                              (((sec_size) ==  512u) || ((sec_size) == 1024u)  || \
                                               ((sec_size) == 2048u) || ((sec_size) == 4096u)))             ? (DEF_YES) : (DEF_NO))



/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  FS_DEV_API  *FS_DevDrvFind  (const  CPU_CHAR  *name_dev);   /* Find device driver by name.                      */

static  void         FSDev_HandleErr(       FS_DEV    *p_dev,   /* Handle error from device access.                     */
                                            FS_ERR     err);


                                                                /* ------------------- NAME PARSING ------------------- */
static  CPU_CHAR    *FSDev_NameParse(       CPU_CHAR  *name_full,   /* Extract name & unit number of device.            */
                                            CPU_CHAR  *name_dev,
                                            FS_QTY    *p_unit_nbr);


                                                                /* ------------- DEVICE OBJECT MANAGEMENT ------------- */
static  void         FSDev_ObjClr   (       FS_DEV    *p_dev);  /* Clear    a device object.                            */

static  FS_DEV      *FSDev_ObjFind  (       CPU_CHAR  *name_dev);   /* Find a device object by name.                */

static  void         FSDev_ObjFree  (       FS_DEV    *p_dev);  /* Free     a device object.                            */

static  FS_DEV      *FSDev_ObjGet   (       void);              /* Allocate a device object.                            */


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           FS_DevDrvAdd()
*
* Description : Add a device driver to the file system.
*
* Argument(s) : p_dev_drv   Pointer to a device driver.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                        Device driver added.
*                               FS_ERR_NULL_PTR                    Argument 'p_dev_drv' passed a NULL pointer.
*                               FS_ERR_DEV_DRV_ALREADY_ADDED       Device driver already added.
*                               FS_ERR_DEV_DRV_INVALID_NAME        Device driver name invalid.
*                               FS_ERR_DEV_DRV_NO_TBL_POS_AVAIL    No device driver table position available.
*
* Return(s)   : none.
*
* Note(s)     : (1) The 'NameGet()' device driver interface function MUST return a valid name :
*
*                   (a) The name must be unique (e.g., a name that is not returned by any other device
*                       driver);
*                   (b) The name must NOT include any of the characters ':', '\' or '/';
*                   (c) The name must contain fewer than FS_CFG_MAX_DEV_DRV_NAME_LEN characters;
*                   (d) The name must NOT be an empty string.
*
*               (2) The 'Init()' device driver interface function is called to initialize driver
*                   structures & any hardware for detecting the presence of devices (for a removable
*                   medium).
*********************************************************************************************************
*/

void  FS_DevDrvAdd (FS_DEV_API  *p_dev_drv,
                    FS_ERR      *p_err)
{
           CPU_SIZE_T    str_len;
           CPU_CHAR     *p_colon;
           FS_DEV_API   *p_dev_drv_dup;
    const  CPU_CHAR     *name_dev;
           FS_QTY        ix;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE ARGS ------------------- */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (p_dev_drv == (FS_DEV_API *)0) {                         /* Validate dev drv ptr.                                */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE FS LOCK ------------------ */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* -------------------- TEST NAME --------------------- */
    name_dev = p_dev_drv->NameGet();                            /* Get dev drv name (see Note #1).                      */
    if (name_dev == (CPU_CHAR *)0) {                            /* Rtn if name is NULL ptr.                             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_INVALID_NAME;
        return;
    }

    str_len = Str_Len_N(name_dev, FS_CFG_MAX_DEV_DRV_NAME_LEN + 1u);
    if ((str_len <                          1u) ||              /* Rtn if name is empty str (see Note #1d) ...          */
        (str_len > FS_CFG_MAX_DEV_DRV_NAME_LEN)) {              /* ... or name is too long  (see Note #1c).             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_INVALID_NAME;
        return;
    }

    p_colon = Str_Char_N(name_dev, FS_CFG_MAX_DEV_DRV_NAME_LEN, FS_CHAR_DEV_SEP);
    if (p_colon != (CPU_CHAR *)0) {                             /* Rtn if name contains ':' (see Note #1b).             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_INVALID_NAME;
        return;
    }

    p_colon = Str_Char_N(name_dev, FS_CFG_MAX_DEV_DRV_NAME_LEN, FS_CHAR_PATH_SEP_ALT);
    if (p_colon != (CPU_CHAR *)0) {                             /* Rtn if name contains '/' (see Note #1b).             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_INVALID_NAME;
        return;
    }

    p_colon = Str_Char_N(name_dev, FS_CFG_MAX_DEV_DRV_NAME_LEN, FS_CHAR_PATH_SEP);
    if (p_colon != (CPU_CHAR *)0) {                             /* Rtn if name contains '\' (see Note #1b).             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_INVALID_NAME;
        return;
    }


                                                                /* ------------------ TEST DUP NAME ------------------- */
    p_dev_drv_dup = FS_DevDrvFind(name_dev);                    /* Chk if name dup'd (see Note #1a).                    */
    if (p_dev_drv_dup != (FS_DEV_API *)0) {
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_DRV_ALREADY_ADDED;
        return;
    }

                                                                /* ------------ FIND EMPTY SLOT IN DRV TBL ------------ */
    for (ix = 0u; ix < FS_DevDrvCntMax; ix++) {
        if (FS_DevDrvTbl[ix] == DEF_NULL) {
            break;
        }
    }
    if (ix >= FS_DevDrvCntMax) {
        FS_OS_Unlock();
        FS_TRACE_INFO(("FS_DevDrvAdd(): Could not alloc drv from pool. Opened driver limit reached.\r\n"));
       *p_err = FS_ERR_DEV_DRV_NONE_AVAIL;
        return;
    }


                                                                /* ---------------- INIT & ADD TO TBL ----------------- */
    p_dev_drv->Init(p_err);                                     /* Init dev drv (see Note #2).                          */
    if (*p_err != FS_ERR_NONE) {
        FS_OS_Unlock();
        return;
    }
    FS_DevDrvTbl[ix] = p_dev_drv;                                /* Add name to tbl.                                     */



                                                                /* ----------------- RELEASE FS LOCK ------------------ */
    FS_OS_Unlock();

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                              FSDev_Close()
*
* Description : Close & free a device.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE             Device closed successfully.
*                               FS_ERR_DEV_FIXED        Device is fixed (cannot be closed).
*                               FS_ERR_DEV_NOT_OPEN     Device is not open.
*                               FS_ERR_NAME_NULL        Argument 'p_name_dev' passed a NULL pointer.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_Close (CPU_CHAR  *name_dev,
                   FS_ERR    *p_err)
{
    FS_DEV       *p_dev;
    FS_CTR        ref_cnt;
    CPU_BOOLEAN   dev_lock_ok;



#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_Acquire(name_dev);
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }

    ref_cnt = p_dev->RefCnt;                                    /* Check if volume is open on device                    */
    if (ref_cnt > 2u) {
       *p_err = FS_ERR_DEV_VOL_OPEN;                            /* ... rtn err.                                         */
        FSDev_Release(p_dev);
        return;
    }

    dev_lock_ok = FSDev_Lock(p_dev);
    if (dev_lock_ok == DEF_NO) {
        FSDev_Release(p_dev);
       *p_err = FS_ERR_OS_LOCK;
        return;
    }



                                                                /* ------------------- VALIDATE DEV ------------------- */
    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:
        case FS_DEV_STATE_PRESENT:
        case FS_DEV_STATE_LOW_FMT_VALID:
             break;


        case FS_DEV_STATE_CLOSED:                               /* Dev is already closing or not rdy ...                */
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
             FSDev_ReleaseUnlock(p_dev);                        /* ... nothing can be done.                             */
            *p_err = FS_ERR_DEV_NOT_OPEN;
             return;
    }


                                                                /* --------------------- CLOSE DEV --------------------- */
    p_dev->DevDrvPtr->Close(p_dev);                             /* Close dev.                                            */

    p_dev->State = FS_DEV_STATE_CLOSING;



                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
    FSDev_Release(p_dev);                                       /* Release dev ref init.                                */

   *p_err = FS_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        FSDev_GetNbrPartitions()
*
* Description : Get number of partitions on a device.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Number of partitions obtained.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*
*                                                              ------- RETURNED BY FSDev_AcquireLockChk() -------
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*
*                                                              --- RETURNED BY FSPartition_GetNbrPartitions() ---
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*
* Return(s)   : Number of partitions on the device, if no error was encountered.
*               Zero,                               otherwise.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in internal functions called by
*                      'FSPartition_GetNbrPartitions()'.
*********************************************************************************************************
*/

#if   (FS_CFG_PARTITION_EN == DEF_ENABLED)
FS_PARTITION_NBR  FSDev_GetNbrPartitions (CPU_CHAR  *name_dev,
                                          FS_ERR    *p_err)
{
    FS_DEV            *p_dev;
    FS_PARTITION_NBR   nbr_partitions;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION((FS_PARTITION_NBR)0);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return ((FS_PARTITION_NBR)0);
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return ((FS_PARTITION_NBR)0);
    }



                                                                /* ---------------- GET NBR PARTITIONS ---------------- */
    nbr_partitions = FSPartition_GetNbrPartitions(p_dev, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);



                                                                /* ------------------------ RTN ----------------------- */
    return (nbr_partitions);
}
#endif


/*
*********************************************************************************************************
*                                           FSDev_IO_Ctrl()
*
* Description : Perform device I/O control operation.
*
* Argument(s) : name_dev    Device name.
*
*               opt         Control command.
*
*               p_data      Buffer which holds data to be used for operation.
*                              OR
*                           Buffer in which data will be stored as a result of operation.
*
*               p_err       Pointer to variable that will receive return the error code from this function :
*
*                               FS_ERR_NONE                    Control operation performed successfully.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*
*                                                              --- RETURNED BY FSDev_AcquireLockChk() ---
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*
*                                                              ----- RETURNED BY DEV DRV's IO_CTRL() ----
*                               FS_ERR_DEV_INVALID_IO_CTRL     I/O control operation unknown to driver.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : none.
*
* Note(s)     : (1) Defined I/O control operations are :
*
*                                                        ---------------- GENERIC OPTIONS ---------------
*                   (a) FS_DEV_IO_CTRL_REFRESH:          Refresh device.
*                   (b) FS_DEV_IO_CTRL_LOW_FMT           Low-level format device.
*                   (c) FS_DEV_IO_CTRL_LOW_MOUNT         Low-level mount device.
*                   (d) FS_DEV_IO_CTRL_LOW_UNMOUNT       Low-level unmount device.
*                   (e) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level compact device.
*                   (f) FS_DEV_IO_CTRL_LOW_COMPACT       Low-level defragment device.
*                   (g) FS_DEV_IO_CTRL_SEC_RELEASE       Release data in sector.
*                   (h) FS_DEV_IO_CTRL_PHY_RD            Read  physical device.
*                   (i) FS_DEV_IO_CTRL_PHY_WR            Write physical device.
*                   (j) FS_DEV_IO_CTRL_PHY_RD_PAGE       Read  physical device page.
*                   (k) FS_DEV_IO_CTRL_PHY_WR_PAGE       Write physical device page.
*                   (l) FS_DEV_IO_CTRL_PHY_ERASE_BLK     Erase physical device block.
*                   (m) FS_DEV_IO_CTRL_PHY_ERASE_CHIP    Erase physical device.
*
*                                                        ---------- SD-DRIVER SPECIFIC OPTIONS ----------
*                   (n) FS_DEV_IO_CTRL_SD_QUERY          Get info about SD/MMC card.
*                   (o) FS_DEV_IO_CTRL_SD_RD_CID         Read SD/MMC card Card ID register.
*                   (p) FS_DEV_IO_CTRL_SD_RD_CSD         Read SD/MMC card Card-Specific Data register.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*********************************************************************************************************
*/

void  FSDev_IO_Ctrl (CPU_CHAR    *name_dev,
                     CPU_INT08U   opt,
                     void        *p_data,
                     FS_ERR      *p_err)
{
    FS_DEV       *p_dev;
    CPU_BOOLEAN   lock_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_Acquire(name_dev);

    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }

    lock_success = FSDev_Lock(p_dev);
    if (lock_success != DEF_YES) {
        FSDev_Release(p_dev);
       *p_err = FS_ERR_OS_LOCK;
        return;
    }



                                                                /* ------------------- VALIDATE DEV ------------------- */
    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:
        case FS_DEV_STATE_PRESENT:
        case FS_DEV_STATE_LOW_FMT_VALID:
             break;


        case FS_DEV_STATE_CLOSED:                               /* Dev closed or not rdy ...                            */
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
             FSDev_ReleaseUnlock(p_dev);                        /* ... so nothing can be done.                          */
            *p_err = FS_ERR_DEV_NOT_OPEN;
             return;
    }



                                                                /* ----------------- PERFORM I/O CTRL ----------------- */
    p_dev->DevDrvPtr->IO_Ctrl(p_dev,
                              opt,
                              p_data,
                              p_err);

    FSDev_HandleErr(p_dev, *p_err);                             /* See Note #2.                                         */



                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}


/*
*********************************************************************************************************
*                                            FSDev_Open()
*
* Description : Open a device.
*
* Argument(s) : name_dev    Device name.  See Note #1.
*
*               p_dev_cfg   Pointer to device configuration.
*               ---------   Validated in device driver 'Open()' function.
*
*               p_err       Pointer to variable that will receive the return error code from this function
*                           (see Note #2) :
*
*                               FS_ERR_NONE                     Device opened successfully.
*                               FS_ERR_DEV_ALREADY_OPEN         Device is already open.
*                               FS_ERR_DEV_INVALID_NAME         Specified device name not valid.
*                               FS_ERR_DEV_INVALID_SEC_SIZE     Invalid device sector size.
*                               FS_ERR_DEV_INVALID_SIZE         Invalid device size.
*                               FS_ERR_DEV_NONE_AVAIL           No devices available.
*                               FS_ERR_DEV_UNKNOWN              Unknown device error.
*                               FS_ERR_NAME_NULL                Argument 'name_dev' passed a NULL pointer.
*
*                                                               ------ RETURNED BY DEV DRV's Open() -----
*                               FS_ERR_DEV_INVALID_LOW_FMT      Device needs to be low-level formatted.
*                               FS_ERR_DEV_INVALID_UNIT_NBR     Device unit number is invalid.
*                               FS_ERR_DEV_IO                   Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT          Device is not present.
*                               FS_ERR_DEV_TIMEOUT              Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Valid device names follow the pattern
*
*                       <dev_drv_name>:<unit>:
*
*                   where   <dev_drv_name> is the name of the device driver, identical to the string
*                                          returned by the device driver's 'NameGet()' function.
*
*                           <unit>         is the unit number of the device instance.
*
*                   (a) A name is invalid if ...
*
*                       (1) ... it cannot be parsed :
*
*                           (a) Device name is empty string (e.g., first character is colon or NULL).
*                           (b) Device name too long.
*                           (c) No first  colon found.
*                           (d) Device driver name too long.
*                           (e) No second colon found.
*                           (f) Illegal unit number.
*                           (g) Consecutive colons found.
*
*                       (2) ... it includes text after a valid device name.
*
*                       (3) ... it is an empty string.
*
*                       (4) ... device driver could not be found with matching name.
*
*               (2) The return error code from this function SHOULD always be checked by the calling
*                   application to determine if the device was successfully opened.  Repeated calls to
*                   'FSDev_Open()' resulting in errors that do not indicate failure to open (like
*                   FS_ERR_DEV_INVALID_LOW_FMT) without matching 'FSDev_Close()' calls may exhaust the
*                   supply of device structures.
*
*                   (a) If FS_ERR_NONE is returned, then the device has been added to the file system &
*                       is immediately accessible.
*
*                   (b) If FS_DEV_INVALID_LOW_FMT is returned, then the device has been added to the file
*                       system & is present, but needs to be low-level formatted.
*
*                   (c) If FS_ERR_DEV_NOT_PRESENT, FS_ERR_DEV_IO or FS_ERR_DEV_TIMEOUT is returned, then
*                       the device has been added to the file system, though it is probably not present.
*                       The device will need to be either closed & re-added, or refreshed.
*
*                   (d) If FS_ERR_DEV_INVALID_NAME, FS_ERR_DEV_INVALID_SEC_SIZE, FS_ERR_DEV_INVALID_SIZE,
*                       FS_ERR_DEV_INVALID_UNIT_NBR or FS_ERR_DEV_NONE_AVAIL is returned, then the device
*                       has NOT been added to the file system.
*
*                   (e) If FS_ERR_DEV_UNKNOWN is returned, then the device driver is in an indeterminate
*                       state.  The system MAY need to be restarted & the device driver should be
*                       examined for errors.  The device has NOT been added to the file system.
*********************************************************************************************************
*/

void  FSDev_Open (CPU_CHAR  *name_dev,
                  void      *p_dev_cfg,
                  FS_ERR    *p_err)
{
    FS_DEV       *p_dev;
    FS_DEV_API   *p_dev_drv;
    FS_DEV_INFO   dev_info;
    CPU_CHAR      name_dev_copy[FS_CFG_MAX_DEV_NAME_LEN + 1u];
    CPU_CHAR     *name_file;
    FS_ERR        query_err;
    FS_QTY        unit_nbr;
    CPU_BOOLEAN   lock_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ------------------ PARSE DEV NAME ------------------ */
    name_file = FSDev_NameParse( name_dev,                      /* Parse dev name.                                      */
                                 name_dev_copy,
                                &unit_nbr);

    if (name_file == (CPU_CHAR *)0) {                           /* Name could not be parsed (see Note #1a1).            */
       *p_err = FS_ERR_DEV_INVALID_NAME;
        return;
    }

    if (*name_file != (CPU_CHAR)ASCII_CHAR_NULL) {              /* Name inc's text after good dev name (see Note #1a2). */
        *p_err = FS_ERR_DEV_INVALID_NAME;
         return;
    }

    if (name_dev_copy[0] == (CPU_CHAR)ASCII_CHAR_NULL) {        /* Dev name is empty str (see Note #1a3).               */
       *p_err = FS_ERR_DEV_INVALID_NAME;
        return;
    }



                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(p_err);
    if (*p_err != FS_ERR_NONE) {
         return;
    }

    p_dev = FSDev_ObjFind(name_dev);
    if (p_dev != (FS_DEV *)0) {                                 /* If dev already open ...                              */
        FS_OS_Unlock();

        if (p_dev->State == FS_DEV_STATE_LOW_FMT_VALID) {       /* Chk if dev has been low level fmt'd.                 */
           *p_err = FS_ERR_DEV_ALREADY_OPEN;
        } else {
           *p_err = FS_ERR_DEV_INVALID_LOW_FMT;                 /* Return err that will prompt for a low level fmt.     */
        }
        return;
    }



                                                                /* ------------------- FIND DEV DRV ------------------- */
    p_dev_drv = FS_DevDrvFind(&name_dev_copy[0]);
    if (p_dev_drv == (FS_DEV_API *)0) {                         /* If dev drv NOT found (see Note #1a4) ...             */
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_INVALID_NAME;                        /* ... rtn err.                                         */
        return;
    }



                                                                /* ------------------- ALLOCATE DEV ------------------- */
    p_dev = FSDev_ObjGet();                                     /* Get dev.                                             */
    if (p_dev == (FS_DEV *)0) {
        FS_OS_Unlock();
       *p_err = FS_ERR_DEV_NONE_AVAIL;
        return;
    }

    p_dev->DevDrvPtr         = p_dev_drv;
    p_dev->UnitNbr           = unit_nbr;
    p_dev->State             = FS_DEV_STATE_OPENING;
    p_dev->RefCnt            = 1u;
    (void)Str_Copy_N(&p_dev->Name[0], name_dev, FS_CFG_MAX_DEV_NAME_LEN);
    FS_OS_Unlock();



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    lock_success = FSDev_Lock(p_dev);
    if (lock_success != DEF_YES) {
       *p_err = FS_ERR_OS_LOCK;
        return;
    }



                                                                /* --------------------- OPEN DEV --------------------- */
    p_dev_drv->Open(p_dev, p_dev_cfg, p_err);                   /* Open dev.                                            */


    switch (*p_err) {
        case FS_ERR_NONE:                                       /* See Note #2a.                                        */
             p_dev_drv->Query(p_dev, &dev_info, &query_err);    /* Get dev info.                                        */
             if (query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 break;
             }
             p_dev->Fixed = dev_info.Fixed;

                                                                /* Validate size & sec size.                            */
             if (FS_DEV_IS_VALID_SIZE(dev_info.Size) == DEF_YES) {
                 if (FS_DEV_IS_VALID_SEC_SIZE(dev_info.SecSize) == DEF_YES) {
                     p_dev->State   = FS_DEV_STATE_LOW_FMT_VALID;
                     p_dev->Size    = dev_info.Size;
                     p_dev->SecSize = dev_info.SecSize;
                 } else {
                     FS_TRACE_INFO(("FSDev_Open(): invalid device sector size: %u\r\n", dev_info.SecSize));
                    *p_err = FS_ERR_DEV_INVALID_SEC_SIZE;
                 }
             } else {
                *p_err = FS_ERR_DEV_INVALID_SIZE;
             }
             break;


        case FS_ERR_DEV_INVALID_LOW_FMT:                        /* Invalid low fmt (see Note #2b).                      */
        case FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS:
        case FS_ERR_DEV_CORRUPT_LOW_FMT:
             p_dev_drv->Query(p_dev, &dev_info, &query_err);
             if (query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 break;
             }
             p_dev->Fixed = dev_info.Fixed;
             p_dev->State = FS_DEV_STATE_PRESENT;
             break;


        case FS_ERR_DEV_NOT_PRESENT:                            /* Device access err (see Notes #2c).                   */
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
             p_dev_drv->Query(p_dev, &dev_info, &query_err);
             if (query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 break;
             }
             p_dev->Fixed = dev_info.Fixed;
             p_dev->State = FS_DEV_STATE_OPEN;
             break;


        case FS_ERR_DEV_INVALID_UNIT_NBR:                       /* See Notes #2d.                                       */
        case FS_ERR_DEV_INVALID_CFG:
        case FS_ERR_DEV_INVALID_LOW_PARAMS:
        case FS_ERR_DEV_ALREADY_OPEN:
        case FS_ERR_MEM_ALLOC:
             break;


        default:
            *p_err = FS_ERR_DEV_UNKNOWN;
             break;
    }



                                                                /* ------------------- HANDLE ERROR ------------------- */
    switch (*p_err) {
        case FS_ERR_NONE:
        case FS_ERR_DEV_INVALID_LOW_FMT:
        case FS_ERR_DEV_INCOMPATIBLE_LOW_PARAMS:
        case FS_ERR_DEV_CORRUPT_LOW_FMT:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
             FSDev_Unlock(p_dev);                               /* Unlock dev but keep dev ref.                         */
             break;


        case FS_ERR_DEV_INVALID_UNIT_NBR:
        case FS_ERR_DEV_INVALID_SEC_SIZE:
        case FS_ERR_DEV_INVALID_SIZE:
        case FS_ERR_DEV_ALREADY_OPEN:
        case FS_ERR_DEV_INVALID_CFG:
        case FS_ERR_DEV_UNKNOWN:
        case FS_ERR_MEM_ALLOC:
        default:
             p_dev_drv->Close(p_dev);
             p_dev->State = FS_DEV_STATE_CLOSING;
             FSDev_ReleaseUnlock(p_dev);
             break;
    }
}


/*
*********************************************************************************************************
*                                          FSDev_PartitionAdd()
*
* Description : Add a partition to a device.
*
* Argument(s) : name_dev        Device name.
*
*               partition_size  Size, in sectors, of the partition to add.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Partition added.
*                                   FS_ERR_NAME_NULL                 Argument 'name_dev' passed a NULL pointer.
*
*                                                                    --- RETURNED BY FSDev_AcquireLockChk() ---
*                                   FS_ERR_DEV_NOT_OPEN              Device is not open.
*                                   FS_ERR_DEV_NOT_PRESENT           Device is not present.
*                                   FS_ERR_DEV_INVALID_LOW_FMT       Device needs to be low-level formatted.
*
*                                                                    ------ RETURNED BY FSPartition_Add() -----
*                                   FS_ERR_DEV_IO                    Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT               Device timeout error.
*                                   FS_ERR_INVALID_SIG               Invalid MBR signature.
*                                   FS_ERR_INVALID_PARTITION         Invalid partition.
*                                   FS_ERR_PARTITION_INVALID_SIZE    Partition size invalid.
*
* Return(s)   : The index of the created partition.  The first partition on the device has an index of 0.
*               If an error occurs, the function returns FS_INVALID_PARTITION_NBR.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in local functions called by
*                      'FSPartition_Add()'.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN   == DEF_DISABLED)
FS_PARTITION_NBR   FSDev_PartitionAdd (CPU_CHAR    *name_dev,
                                       FS_SEC_QTY   partition_size,
                                       FS_ERR      *p_err)
{
    FS_DEV            *p_dev;
    FS_PARTITION_NBR   partition_nbr;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION((FS_PARTITION_NBR)0);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return ((FS_PARTITION_NBR)0);
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return ((FS_PARTITION_NBR)0);
    }



                                                                /* ------------------- ADD PARTITION ------------------ */
    partition_nbr = FSPartition_Add(p_dev, partition_size, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);

    return (partition_nbr);
}
#endif
#endif


/*
*********************************************************************************************************
*                                          FSDev_PartitionFind()
*
* Description : Find a partition on a device.
*
* Argument(s) : name_dev            Device name.
*
*               partition_nbr       Index of the partition to find.
*
*               p_partition_entry   Pointer to variable that will receive the partition information.
*
*               p_err               Pointer to variable that will receive the return error code from this
*                                   function :
*
*                                       FS_ERR_NONE                      Partition found.
*                                       FS_ERR_NAME_NULL                 Argument 'name_dev' passed a NULL pointer.
*                                       FS_ERR_NULL_PTR                  Argument 'p_partition_entry' passed
*                                                                            a NULL pointer.
*
*                                                                        --- RETURNED BY FSDev_AcquireLockChk() ---
*                                       FS_ERR_DEV_NOT_OPEN              Device is not open.
*                                       FS_ERR_DEV_NOT_PRESENT           Device is not present.
*                                       FS_ERR_DEV_INVALID_LOW_FMT       Device needs to be low-level formatted.
*
*                                                                        ----- RETURNED BY FSPartition_Find() -----
*                                       FS_ERR_PARTITION_NOT_FOUND       Partition not found.
*                                       FS_ERR_INVALID_SIG               Invalid MBR signature.
*                                       FS_ERR_PARTITION_INVALID_NBR     Invalid partition number.
*                                       FS_ERR_DEV_INVALID_LOW_FMT       Device needs to be low-level formatted.
*                                       FS_ERR_DEV_IO                    Device I/O error.
*                                       FS_ERR_DEV_TIMEOUT               Device timeout error.
*                                       FS_ERR_DEV_NOT_PRESENT           Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in internal functions called by
*                      'FSPartition_Find()'.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
void  FSDev_PartitionFind (CPU_CHAR            *name_dev,
                           FS_PARTITION_NBR     partition_nbr,
                           FS_PARTITION_ENTRY  *p_partition_entry,
                           FS_ERR              *p_err)
{
    FS_DEV  *p_dev;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_partition_entry == (FS_PARTITION_ENTRY *)0) {         /* Validate partition entry ptr.                        */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ------------------- DFLT RTN VARS ------------------ */
   p_partition_entry->Start = 0u;
   p_partition_entry->Size  = 0u;
   p_partition_entry->Type  = 0u;



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return;
    }



                                                                /* ------------------ FIND PARTITION ------------------ */
#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
    FSPartition_Find(p_dev,
                     partition_nbr,
                     p_partition_entry,
                     p_err);
#else
    FSPartition_FindSimple(p_dev,
                           p_partition_entry,
                           p_err);
#endif

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_PartitionInit()
*
* Description : Initialize the partition structure on a device.
*
* Argument(s) : name_dev        Device name.
*
*               partition_size  Size of partition, in sectors.
*                                   OR
*                               0, if partition will occupy entire device.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   FS_ERR_NONE                      Partition found.
*                                   FS_ERR_DEV_VOL_OPEN              Volume open on device.
*                                   FS_ERR_NAME_NULL                 Argument 'name_dev' passed a NULL pointer.
*                                   FS_ERR_PARTITION_INVALID_SIZE    Partition size invalid.
*
*                                                                    --- RETURNED BY FSDev_AcquireLockChk() ---
*                                   FS_ERR_DEV_NOT_OPEN              Device is not open.
*                                   FS_ERR_DEV_NOT_PRESENT           Device is not present.
*                                   FS_ERR_DEV_INVALID_LOW_FMT       Device needs to be low-level formatted.
*
*                                                                    ----- RETURNED BY FSPartition_Init() -----
*                                   FS_ERR_DEV_IO                    Device I/O error.
*                                   FS_ERR_DEV_TIMEOUT               Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Function blocked if a volume is open on the device.  All volume (& files) MUST be
*                   closed prior to initializing the partition structure, since it will obliterate any
*                   existing file system.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in internal functions called by
*                      'FSPartition_Init()'.
*********************************************************************************************************
*/

#if (FS_CFG_PARTITION_EN == DEF_ENABLED)
#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_PartitionInit (CPU_CHAR    *name_dev,
                           FS_SEC_QTY   partition_size,
                           FS_ERR      *p_err)
{
    FS_DEV  *p_dev;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return;
    }

    if (p_dev->VolCnt != (FS_QTY)0) {                           /* See Notes #1.                                        */
        FSDev_ReleaseUnlock(p_dev);
       *p_err = FS_ERR_DEV_VOL_OPEN;
        return;
    }

    if (partition_size > p_dev->Size) {                         /* Chk partition size.                                  */
        FSDev_ReleaseUnlock(p_dev);
       *p_err = FS_ERR_PARTITION_INVALID_SIZE;
        return;
    }



                                                                /* ------------- INIT PARTITION STRUCTURE ------------- */
    if (partition_size == 0u) {
        partition_size =  p_dev->Size;
    }

    FSPartition_Init(p_dev,
                     partition_size,
                     p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}
#endif
#endif


/*
*********************************************************************************************************
*                                            FSDev_Query()
*
* Description : Obtain information about a device.
*
* Argument(s) : name_dev    Device name.
*
*               p_info      Pointer to structure that will receive device information.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                   Complete device information obtained.
*                               FS_ERR_NAME_NULL              Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR               Argument 'p_info' passed a NULL pointer.
*
*                                                             ---- RETURNED BY FSDev_AcquireLockChk() ---
*                               FS_ERR_DEV_NOT_OPEN           Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*
*                                                             ----- RETURNED BY FSDev_QueryLocked() -----
*                               FS_ERR_DEV_INVALID_SEC_NBR    Sector start or count invalid.
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in 'FSDev_QueryLocked()'.
*
*               (2) In case the media is not present it's not possible to call FSDev_QueryLocked()
*                   which may assume a media is present. In that case assuming the device itself is
*                   available the 'State' and 'Fixed' fields of p_info are filled from the device data.
*                   Both 'SecSize' and 'Size' will be set to 0. An appropriate error is returned as
*                   to why the device was inaccessible.
*
*                   (a) The device info will not be valid if a fatal error such as FS_ERR_DEV_NOT_OPEN
*                       or FS_ERR_OS_LOCK is returned.
*********************************************************************************************************
*/

void  FSDev_Query (CPU_CHAR     *name_dev,
                   FS_DEV_INFO  *p_info,
                   FS_ERR       *p_err)
{
    FS_DEV       *p_dev;
    CPU_BOOLEAN   lock_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == DEF_NULL) {                                 /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_info == DEF_NULL) {                                   /* Validate info ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif


    p_dev = FSDev_AcquireLockChk(name_dev,  p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev != DEF_NULL) {                                    /* If media is present go ahead with the query.         */
        FSDev_QueryLocked(p_dev, p_info, p_err);
        p_info->State = p_dev->State;
    } else {                                                    /* Otherwise use the info stored in p_dev. See note #2. */
        p_dev = FSDev_Acquire(name_dev);                        /* Acquire dev ref.                                     */
        if (p_dev == DEF_NULL) {                                /* Rtn err if dev not found.                            */
           *p_err = FS_ERR_DEV_NOT_OPEN;
            return;
        }

        lock_success = FSDev_Lock(p_dev);
        if (lock_success != DEF_YES) {
            FSDev_Release(p_dev);
           *p_err = FS_ERR_OS_LOCK;
            return;
        }

        p_info->State   = p_dev->State;
        p_info->Fixed   = p_dev->Fixed;
        p_info->Size    = 0u;
        p_info->SecSize = 0u;
    }

    FSDev_ReleaseUnlock(p_dev);
}


/*
*********************************************************************************************************
*                                               FSDev_Rd()
*
* Description : Read data from device sector(s).
*
* Argument(s) : name_dev    Device name.
*
*               p_dest      Pointer to destination buffer.
*
*               start       Start sector of read.
*               -----       Argument checked in 'FSDev_RdLocked()'.
*
*               cnt         Number of sectors to read.
*               ---         Argument checked in 'FSDev_RdLocked()'.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) read.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR                Argument 'p_dest' passed a NULL pointer.
*
*                                                              --- RETURNED BY FSDev_AcquireLockChk() ---
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*
*                                                              ------ RETURNED BY FSDev_RdLocked() ------
*                               FS_ERR_DEV_INVALID_SEC_NBR     Sector start or count invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in 'FSDev_RdLocked()'.
*********************************************************************************************************
*/

void  FSDev_Rd (CPU_CHAR    *name_dev,
                void        *p_dest,
                FS_SEC_NBR   start,
                FS_SEC_QTY   cnt,
                FS_ERR      *p_err)
{
    FS_DEV  *p_dev;



#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_dest == (void *)0) {                                  /* Validate dest ptr.                                   */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return;
    }



                                                                /* ---------------------- RD DEV ---------------------- */
    FSDev_RdLocked(p_dev, p_dest, start, cnt, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}


/*
*********************************************************************************************************
*                                             FSDev_Refresh()
*
* Description : Refresh a device.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Device refreshed.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_VOL_OPEN            Volume open on device.
*
*                                                              ---------- FSDev_RefreshLocked() ---------
*                               FS_ERR_DEV_INVALID_SEC_SIZE    Invalid device sector size.
*                               FS_ERR_DEV_INVALID_SIZE        Invalid device size.
*                               FS_ERR_DEV_UNKNOWN             Unknown device error.
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : DEF_YES, if device has     changed.
*               DEF_NO,  if device has NOT changed.
*
* Note(s)     : (1) If device has changed, all volumes open on the device must be refreshed & all files
*                   closed and reopened.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in 'FSDev_RefreshLocked()'.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_Refresh (CPU_CHAR  *name_dev,
                            FS_ERR    *p_err)
{
    FS_DEV       *p_dev;
    CPU_BOOLEAN   chngd;
    CPU_BOOLEAN   lock_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_dev == (CPU_CHAR *)0) {
       *p_err = FS_ERR_NAME_NULL;
        return (DEF_NO);
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_Acquire(name_dev);
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return (DEF_NO);
    }

    lock_success = FSDev_Lock(p_dev);
    if (lock_success != DEF_YES) {
        FSDev_Release(p_dev);
       *p_err = FS_ERR_OS_LOCK;
        return (DEF_NO);
    }



                                                                /* ------------------- VALIDATE DEV ------------------- */
    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:
        case FS_DEV_STATE_PRESENT:
        case FS_DEV_STATE_LOW_FMT_VALID:
             break;


        case FS_DEV_STATE_CLOSED:
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
             FSDev_ReleaseUnlock(p_dev);
            *p_err = FS_ERR_DEV_NOT_OPEN;
             return (DEF_NO);
    }



                                                                /* -------------------- REFRESH DEV ------------------- */
    chngd = FSDev_RefreshLocked(p_dev, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);

    return (chngd);
}


/*
*********************************************************************************************************
*                                              FSDev_Sync()
*
* Description : Synchronize a device. Performs any pending operation that would otherwise prevent the
*               device from being remounted in the current state.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TBD
*
* Return(s)   : none.
*
* Note(s)     : (1) Doesn't flush volume caches or file buffers. Those must be flushed prior to calling
*                   this function to ensure a volume is fully synchronized.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_Sync (CPU_CHAR  *name_dev,
                  FS_ERR    *p_err)
{
    FS_DEV       *p_dev;
    CPU_BOOLEAN   lock_success;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_dev == (CPU_CHAR *)0) {
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_Acquire(name_dev);
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }

    lock_success = FSDev_Lock(p_dev);
    if (lock_success != DEF_YES) {
       *p_err = FS_ERR_OS_LOCK;
        return;
    }



                                                                /* ------------------- VALIDATE DEV ------------------- */
    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:
        case FS_DEV_STATE_PRESENT:
        case FS_DEV_STATE_LOW_FMT_VALID:
             break;


        case FS_DEV_STATE_CLOSED:
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
             FSDev_ReleaseUnlock(p_dev);
            *p_err = FS_ERR_DEV_NOT_OPEN;
             return;
    }



                                                                /* --------------------- SYNC DEV --------------------- */
    FSDev_SyncLocked(p_dev, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}
#endif


/*
*********************************************************************************************************
*                                               FSDev_Wr()
*
* Description : Write data to device sector(s).
*
* Argument(s) : name_dev    Device name.
*
*               p_src       Pointer to source buffer.
*
*               start       Start sector of write.
*               -----       Argument checked in FSDev_WrLocked().
*
*               cnt         Number of sectors to write.
*               ---         Argument checked in FSDev_WrLocked().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR                Argument 'p_src' passed a NULL pointer.
*
*                                                              --- RETURNED BY FSDev_AcquireLockChk() ---
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*
*                                                              ------ RETURNED BY FSDev_WrLocked() ------
*                               FS_ERR_DEV_INVALID_SEC_NBR     Sector start or count invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) Device state change will result from device I/O, not present or timeout error.
*
*                   (a) Errors resulting from device access are handled in 'FSDev_WrLocked()'.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_Wr (CPU_CHAR    *name_dev,
                void        *p_src,
                FS_SEC_NBR   start,
                FS_SEC_QTY   cnt,
                FS_ERR      *p_err)
{
    FS_DEV  *p_dev;



#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
    if (p_src == (void *)0) {                                   /* Validate src ptr.                                    */
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif



                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
        return;
    }



                                                                /* ---------------------- WR DEV ---------------------- */
    FSDev_WrLocked(p_dev, p_src, start, cnt, p_err);

                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);
}
#endif


/*
*********************************************************************************************************
*                                           FSDev_Invalidate()
*
* Description : Invalidate files and volumes opened on the device.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_NULL_PTR                Argument 'p_src' passed a NULL pointer.
*
*                                                              --- RETURNED BY FSDev_AcquireLockChk() ---
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*
* Return(s)   : none.
*
* Note(s)     : (1) Operations performed after invalidation on previously opened files and volumes will
*                   fail with error FS_ERR_DEV_CHNGD.
*
*               (2) Invalidation will also automatically happen if a removable media is changed.
*
*********************************************************************************************************
*/

void FSDev_Invalidate (CPU_CHAR  *name_dev,
                       FS_ERR    *p_err)
{
    FS_DEV  *p_dev;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (p_err == (FS_ERR *)0) {                                 /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(;);
    }
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_AcquireLockChk(name_dev, p_err);
    (void)p_err;                                               /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == DEF_NULL) {                                    /* Rtn err if dev not found.                            */
        if(*p_err == FS_ERR_NONE) {
           *p_err = FS_ERR_DEV;
        }
        return;
    }
    *p_err = FS_ERR_NONE;                                       /* Err clr'd if invalidation can be performed.          */


	                                                            /* ----------- INVALIDATE FILES AND VOLUMES ----------- */
    p_dev->RefreshCnt++;

		                                                        /* ----------------- RELEASE DEV LOCK ----------------- */
    FSDev_ReleaseUnlock(p_dev);

}


/*
*********************************************************************************************************
*                                         FSDev_AccessLock()
*
* Description : Acquire global device access lock and block high level uC/FS operations on that device.
*
* Argument(s) : name_dev    Device name.
*
*               timeout     Time to wait for a lock in milliseconds (See note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                                                              --- RETURNED BY FS_OS_DevAccessLock() ---
*                               FS_ERR_OS_LOCK                 File system device access NOT acquired.
*                               FS_ERR_OS_LOCK_TIMEOUT         File system device access timed out.
*
* Return(s)   : none.
*
* Note(s)     : (1) Used when the application wants to perform low level device operations without
*                   interference from other tasks performing high level(FSFile_*) operations.
*
*               (2) May be ignored for OS without timeout support.
*
*********************************************************************************************************
*/

void  FSDev_AccessLock (CPU_CHAR   *name_dev,
                        CPU_INT32U  timeout,
                        FS_ERR     *p_err)
{
    FS_DEV  *p_dev;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_dev == DEF_NULL) {
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

   *p_err = FS_ERR_NONE;

    p_dev = FSDev_Acquire(name_dev);
    if (p_dev == DEF_NULL) {
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }

                                                                /* -------------- ACQUIRE DEV ACCESS LOCK ------------- */
    FS_OS_DevAccessLock(p_dev->ID, timeout, p_err);
    if (*p_err != FS_ERR_NONE) {
        FSDev_Release(p_dev);
        return;
    }

    FSDev_Release(p_dev);

    return;
}


/*
*********************************************************************************************************
*                                        FSDev_AccessUnlock()
*
* Description : Release device access lock.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               FS_ERR_NONE                    Sector(s) written.
*                               FS_ERR_NAME_NULL               Argument 'name_dev' passed a NULL pointer.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_AccessUnlock (CPU_CHAR  *name_dev,
                          FS_ERR      *p_err)
{
    FS_DEV  *p_dev;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE PTR ------------------- */
    if (p_err == DEF_NULL) {                                    /* Validate error ptr.                                  */
        CPU_SW_EXCEPTION(DEF_NO);
    }
    if (name_dev == DEF_NULL) {
       *p_err = FS_ERR_NAME_NULL;
        return;
    }
#endif

   *p_err = FS_ERR_NONE;

    p_dev = FSDev_Acquire(name_dev);
    if (p_dev == DEF_NULL) {
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return;
    }

                                                                /* ------------- RELEASE DEV ACCESS LOCK -------------- */
    FS_OS_DevAccessUnlock(p_dev->ID);

    FSDev_Release(p_dev);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     DEVICE MANAGEMENT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          FSDev_GetDevCnt()
*
* Description : Get the number of open devices.
*
* Argument(s) : none.
*
* Return(s)   : The number of open devices.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSDev_GetDevCnt (void)
{
    FS_QTY  dev_cnt;
    FS_ERR  err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return (0u);
    }



                                                                /* -------------------- GET DEV CNT ------------------- */
    dev_cnt = FSDev_Cnt;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (dev_cnt);
}


/*
*********************************************************************************************************
*                                        FSDev_GetDevCntMax()
*
* Description : Get the maximum possible number of open devices.
*
* Argument(s) : none.
*
* Return(s)   : The maximum number of open devices.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_QTY  FSDev_GetDevCntMax (void)
{
    FS_QTY  dev_cnt_max;


                                                                /* ------------------ GET DEV CNT MAX ----------------- */
    dev_cnt_max = FSDev_DevCntMax;

    return (dev_cnt_max);
}


/*
*********************************************************************************************************
*                                         FSDev_GetDevName()
*
* Description : Get name of the nth open device.
*
* Argument(s) : dev_nbr     Number of device to get.
*
*               name_dev    String buffer that will receive the device name (see Note #2).
*
* Return(s)   : none.
*
* Note(s)     : (1) 'name_dev' MUST be a character array of 'FS_CFG_MAX_DEV_NAME_LEN + 1' characters.
*
*               (2) If the device does not exist 'name_dev' will receive an empty string.
*********************************************************************************************************
*/

void  FSDev_GetDevName (FS_QTY     dev_nbr,
                        CPU_CHAR  *name_dev)
{
    FS_ERR    err;
    FS_QTY    dev_ix;
    FS_QTY    dev_cnt_cur;


#if (FS_CFG_ERR_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------- VALIDATE ARGS ------------------ */
    if (name_dev == (CPU_CHAR *)0) {                            /* Validate name ptr.                                   */
        return;
    }
#endif

    name_dev[0] = (CPU_CHAR)ASCII_CHAR_NULL;                    /* See Note #2.                                         */


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return;
    }


                                                                /* ------------------- GET DEV NAME ------------------- */
    dev_cnt_cur = 0u;
    for (dev_ix = 0u; dev_ix < FSDev_DevCntMax; dev_ix++) {
        if (FSDev_Tbl[dev_ix] != DEF_NULL) {
            if (dev_cnt_cur == dev_nbr) {
                Str_Copy_N(name_dev,
                           FSDev_Tbl[dev_ix]->Name,
                           FS_CFG_MAX_DEV_NAME_LEN);
                break;
            } else {
                dev_cnt_cur++;
            }
        }
    }


                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         FSDev_ModuleInit()
*
* Description : Initialize File System Device Management module.
*
* Argument(s) : dev_cnt         Number of devices to allocate.
*
*               dev_drv_cnt     Number of device drivers to allocate.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*               ----------      Argument validated by caller.
*
*                                   FS_ERR_NONE         Module initialized.
*                                   FS_ERR_MEM_ALLOC    Memory could not be allocated.
*
* Return(s)   : none.
*
* Note(s)     : (1) In the DEVICE-ONLY BUILD, FSDev_ModuleInit() MUST be called ...
*
*                   (a) ONLY ONCE from a product's application; ...
*                   (b) (1) AFTER  product's OS has been initialized
*                       (2) BEFORE product's application calls any file system suite function(s).
*
*               (2) In the FULL BUILD, FSDev_ModuleInit() should NOT be called by application functions.
*********************************************************************************************************
*/

void  FSDev_ModuleInit (FS_QTY   dev_cnt,
                        FS_QTY   dev_drv_cnt,
                        FS_ERR  *p_err)
{
    CPU_SIZE_T  octets_reqd;
    LIB_ERR     pool_err;


#if (FS_CFG_TRIAL_EN == DEF_ENABLED)
    if (dev_cnt > FS_TRIAL_MAX_DEV_CNT) {                       /* Trial limitation: max 1 dev.                         */
       *p_err = FS_ERR_INVALID_CFG;
        return;
    }
#endif

    FSDev_DevCntMax = 0u;
    FSDev_Cnt       = 0u;

                                                                /* ---------------- PERFORM FS/OS INIT ---------------- */
    FS_OS_Init(p_err);                                          /* See 'FS_Init()  Note #4'.                            */
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* -------------- PERFORM FS/OS DEV INIT -------------- */
    FS_OS_DevInit(dev_cnt, p_err);
    if (*p_err != FS_ERR_NONE) {
        return;
    }



                                                                /* ------------------ INIT DEV POOL ------------------- */
    Mem_PoolCreate(&FSDev_Pool,
                    DEF_NULL,
                    0,
                    dev_cnt,
                    sizeof(FS_DEV),
                    sizeof(CPU_ALIGN),
                   &octets_reqd,
                   &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSDev_ModuleInit(): Could not alloc mem for devs: %d octets required.\r\n", octets_reqd));
        return;
    }

    FSDev_Tbl = (FS_DEV **)Mem_HeapAlloc(dev_cnt * sizeof(FS_DEV *),
                                         sizeof(CPU_ALIGN),
                                        &octets_reqd,
                                        &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err  = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSDev_ModuleInit(): Could not alloc mem for devs: %d octets required.\r\n", octets_reqd));
        return;
    }

    Mem_Clr(FSDev_Tbl, dev_cnt * sizeof(FS_DEV *));

    FSDev_DevCntMax = dev_cnt;

                                                                /* ----------------- INIT DEV DRV TBL ----------------- */
    FS_DevDrvTbl = (FS_DEV_API  **)Mem_HeapAlloc(dev_drv_cnt * sizeof(FS_DEV_API *),
                                                 sizeof(CPU_ALIGN),
                                                &octets_reqd,
                                                &pool_err);
    if (pool_err != LIB_MEM_ERR_NONE) {
       *p_err = FS_ERR_MEM_ALLOC;
        FS_TRACE_INFO(("FSDev_ModuleInit(): Could not alloc mem for dev drv tbl: %d octets required.\r\n", octets_reqd));
        return;
    }

    Mem_Clr(FS_DevDrvTbl, dev_drv_cnt * sizeof(FS_DEV_API *));

    FS_DevDrvCntMax = dev_drv_cnt;
}


/*
*********************************************************************************************************
*                                       FSDev_AcquireLockChk()
*
* Description : Acquire device reference & lock.
*
* Argument(s) : name_dev    Device name.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                   Device present & lock acquired.
*                               FS_ERR_DEV_NOT_OPEN           Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*
* Return(s)   : Pointer to a device, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_DEV  *FSDev_AcquireLockChk (CPU_CHAR  *name_dev,
                               FS_ERR    *p_err)
{
    FS_DEV       *p_dev;
    CPU_BOOLEAN   lock_success;


                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    p_dev = FSDev_Acquire(name_dev);                            /* Acquire dev ref.                                     */
    if (p_dev == (FS_DEV *)0) {                                 /* Rtn err if dev not found.                            */
       *p_err = FS_ERR_DEV_NOT_OPEN;
        return ((FS_DEV *)0);
    }

    lock_success = FSDev_Lock(p_dev);
    if (lock_success != DEF_YES) {
        FSDev_Release(p_dev);
       *p_err = FS_ERR_OS_LOCK;
        return ((FS_DEV *)0);
    }



                                                                /* ------------------- VALIDATE DEV ------------------- */
    if (p_dev->State == FS_DEV_STATE_OPEN) {                    /* Dev not present ...                                  */
       (void)FSDev_RefreshLocked(p_dev, p_err);                 /* ... try to reopen dev.                               */
        if (*p_err != FS_ERR_NONE) {
            FSDev_ReleaseUnlock(p_dev);
            return DEF_NULL;
        }
    }

    switch (p_dev->State) {
        case FS_DEV_STATE_OPEN:                                 /* Dev not present.                                     */
            *p_err = FS_ERR_DEV_NOT_PRESENT;
             break;


        case FS_DEV_STATE_PRESENT:                              /* Dev not low fmt'd: app MUST fmt dev.                 */
            *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
             break;


        case FS_DEV_STATE_LOW_FMT_VALID:                        /* Dev present.                                         */
             break;


        case FS_DEV_STATE_CLOSED:                               /* Dev closed, closing or opening: app MUST wait.       */
        case FS_DEV_STATE_CLOSING:
        case FS_DEV_STATE_OPENING:
        default:
            *p_err = FS_ERR_DEV_NOT_OPEN;
             break;
    }

    if (p_dev->State != FS_DEV_STATE_LOW_FMT_VALID) {           /* If still not present & low fmt'd ...                 */
        FSDev_ReleaseUnlock(p_dev);
        return ((FS_DEV *)0);                                   /* ... rtn.                                             */
    }


                                                                /* ------------------------ RTN ----------------------- */
   *p_err = FS_ERR_NONE;
    return (p_dev);
}


/*
*********************************************************************************************************
*                                           FSDev_Acquire()
*
* Description : Acquire device reference.
*
* Argument(s) : name_dev    Device name.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to a device, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

FS_DEV  *FSDev_Acquire (CPU_CHAR  *name_dev)
{
    FS_DEV  *p_dev;
    FS_ERR   err;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return ((FS_DEV *)0);
    }



                                                                /* --------------------- FIND DEV --------------------- */
    p_dev = FSDev_ObjFind(name_dev);                            /* Find dev.                                            */

    if (p_dev == (FS_DEV *)0) {                                 /* Rtn NULL if dev not found.                           */
        FS_OS_Unlock();
        return ((FS_DEV *)0);
    }

    p_dev->RefCnt++;



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();

    return (p_dev);
}


/*
*********************************************************************************************************
*                                        FSDev_ReleaseUnlock()
*
* Description : Release device reference & lock.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_ReleaseUnlock (FS_DEV  *p_dev)
{
    FSDev_Unlock(p_dev);
    FSDev_Release(p_dev);
}


/*
*********************************************************************************************************
*                                           FSDev_Release()
*
* Description : Release device reference.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_Release (FS_DEV  *p_dev)
{
    FS_ERR  err;
    FS_CTR  ref_cnt;


                                                                /* ------------------ ACQUIRE FS LOCK ----------------- */
    FS_OS_Lock(&err);
    if (err != FS_ERR_NONE) {
        return;
    }



                                                                /* -------------------- DEC REF CNT ------------------- */
    ref_cnt = p_dev->RefCnt;
    if (ref_cnt == 1u) {
        FSDev_ObjFree(p_dev);
    } else if (ref_cnt > 0u) {
        p_dev->RefCnt = ref_cnt - 1u;
    } else {
        FS_TRACE_DBG(("FSDev_Release(): Release cnt dec'd to zero.\r\n"));
    }



                                                                /* ------------------ RELEASE FS LOCK ----------------- */
    FS_OS_Unlock();
}


/*
*********************************************************************************************************
*                                            FSDev_Lock()
*
* Description : Acquire device lock.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
* Return(s)   : DEF_YES, if device lock     acquired.
*               DEF_NO,  if device lock NOT acquired.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_Lock (FS_DEV  *p_dev)
{
    FS_ERR  err;


                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    FS_OS_DevLock(p_dev->ID, &err);
    if (err != FS_ERR_NONE) {
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                           FSDev_Unlock()
*
* Description : Release device lock.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSDev_Unlock (FS_DEV  *p_dev)
{
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    FS_OS_DevUnlock(p_dev->ID);
}


/*
*********************************************************************************************************
*                                         FSDev_QueryLocked()
*
* Description : Obtain info about a device.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_info      Pointer to structure that will receive device information.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device information obtained.
*
*                                                              ------ RETURNED BY DEV DRV's Query() -----
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*********************************************************************************************************
*/

void  FSDev_QueryLocked (FS_DEV       *p_dev,
                         FS_DEV_INFO  *p_info,
                         FS_ERR       *p_err)
{
                                                                /* ------------------- GET DEV INFO ------------------- */
    p_dev->DevDrvPtr->Query(p_dev,                              /* Get dev info.                                        */
                            p_info,
                            p_err);



                                                                /* -------------------- HANDLE ERR -------------------- */
    FSDev_HandleErr(p_dev, *p_err);                             /* See Note #2.                                         */
    p_info->State = p_dev->State;
}


/*
*********************************************************************************************************
*                                          FSDev_RdLocked()
*
* Description : Read data from device sector(s).
*
* Argument(s) : p_dev       Pointer to device.
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
*                               FS_ERR_NONE                    Device sector(s) read.
*                               FS_ERR_DEV_INVALID_SEC_NBR     Sector start or count invalid.
*
*                                                              ------- RETURNED BY DEV DRV's Rd() -------
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*********************************************************************************************************
*/

void  FSDev_RdLocked (FS_DEV      *p_dev,
                      void        *p_dest,
                      FS_SEC_NBR   start,
                      FS_SEC_QTY   cnt,
                      FS_ERR      *p_err)
{
    FS_SEC_QTY  size;



                                                                /* ------------------ VALIDATE ARGS ------------------- */
    size = p_dev->Size;

    if (cnt == 0u) {
       *p_err = FS_ERR_NONE;
        return;
    }

    if (start > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }

    if (start + cnt > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }



                                                                /* ---------------------- RD DEV ---------------------- */
    p_dev->DevDrvPtr->Rd(p_dev,
                         p_dest,
                         start,
                         cnt,
                         p_err);



                                                                /* ----------------- UPDATE DEV STATS ----------------- */
    FS_CTR_STAT_ADD(p_dev->StatRdSecCtr, (FS_CTR)cnt);



                                                                /* -------------------- HANDLE ERR -------------------- */
    FSDev_HandleErr(p_dev, *p_err);                             /* See Note #2.                                         */
}


/*
*********************************************************************************************************
*                                          FSDev_RefreshLocked()
*
* Description : Refresh a device.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device refreshed & present.
*                               FS_ERR_DEV_INVALID_SEC_SIZE    Invalid device sector size.
*                               FS_ERR_DEV_INVALID_SIZE        Invalid device size.
*                               FS_ERR_DEV_UNKNOWN             Unknown device error.
*
*                                                              ----- RETURNED BY DEV DRV's IO_Ctrl() ----
*                               FS_ERR_DEV_INVALID_UNIT_NBR    Device unit number is invalid.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_NOT_OPEN            Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*                               FS_ERR_DEV_TIMEOUT             Device timeout.
*
* Return(s)   : DEF_YES, if device has     changed.
*               DEF_NO,  if device has NOT changed.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) If device has changed, all volumes open on the device must be refreshed & all files
*                   closed and reopened.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSDev_RefreshLocked (FS_DEV  *p_dev,
                                  FS_ERR  *p_err)
{
    FS_DEV_INFO   dev_info;
    CPU_BOOLEAN   chngd;
    CPU_BOOLEAN   chngd_dev;
    FS_SEC_SIZE   sec_size_prev;
    FS_SEC_QTY    size_prev;
    FS_STATE      state_prev;
    FS_ERR        query_err;


                                                                /* -------------------- REFRESH DEV ------------------- */
    sec_size_prev = p_dev->SecSize;
    size_prev     = p_dev->Size;
    state_prev    = p_dev->State;

    p_dev->DevDrvPtr->IO_Ctrl(         p_dev,
                                       FS_DEV_IO_CTRL_REFRESH,
                              (void *)&chngd_dev,
                                       p_err);

    FSDev_HandleErr(p_dev, *p_err);

    if (*p_err == FS_ERR_DEV_INVALID_IO_CTRL) {                 /* If dev drv does not support I/O ctrl ...             */
        switch (p_dev->State) {
            case FS_DEV_STATE_LOW_FMT_VALID:
                *p_err = FS_ERR_NONE;
                 break;


            case FS_DEV_STATE_PRESENT:
                *p_err = FS_ERR_DEV_INVALID_LOW_FMT;
                 break;


            case FS_DEV_STATE_OPEN:
            default:
                *p_err = FS_ERR_DEV_NOT_PRESENT;
                 break;
        }

        return (DEF_NO);                                        /* ... dev is unchanged.                                */
    }



                                                                /* ---------------- DETERMINE DEV STATE --------------- */
    switch (*p_err) {
        case FS_ERR_NONE:
                                                                /* Get dev info.                                        */
             p_dev->DevDrvPtr->Query(p_dev, &dev_info, &query_err);
             if(query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 return (DEF_NO);
             }
             p_dev->Fixed = dev_info.Fixed;
                                                                /* Validate size & sec size.                            */
             if (FS_DEV_IS_VALID_SIZE(dev_info.Size) == DEF_YES) {
                 if (FS_DEV_IS_VALID_SEC_SIZE(dev_info.SecSize) == DEF_YES) {
                     p_dev->State   = FS_DEV_STATE_LOW_FMT_VALID;
                     p_dev->Size    = dev_info.Size;
                     p_dev->SecSize = dev_info.SecSize;
                 } else {
                    *p_err = FS_ERR_DEV_INVALID_SEC_SIZE;
                 }
             } else {
                *p_err = FS_ERR_DEV_INVALID_SIZE;
             }
             break;


        case FS_ERR_DEV_INVALID_LOW_FMT:
             p_dev->DevDrvPtr->Query(p_dev, &dev_info, &query_err);
             if(query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 return (DEF_NO);
             }
             p_dev->Fixed = dev_info.Fixed;
             p_dev->State = FS_DEV_STATE_PRESENT;
             break;


        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_TIMEOUT:
        case FS_ERR_DEV_INVALID_UNIT_NBR:
        case FS_ERR_DEV_INVALID_CFG:
             p_dev->DevDrvPtr->Query(p_dev, &dev_info, &query_err);
             if(query_err != FS_ERR_NONE) {
                *p_err = query_err;
                 return (DEF_NO);
             }
             p_dev->Fixed = dev_info.Fixed;
             p_dev->State = FS_DEV_STATE_OPEN;
             break;


        case FS_ERR_DEV_ALREADY_OPEN:
        default:
            *p_err = FS_ERR_DEV_UNKNOWN;
             p_dev->State = FS_DEV_STATE_OPEN;
             break;
    }



                                                                /* -------------------- HANDLE CHNG ------------------- */
    chngd = (state_prev != p_dev->State) ? DEF_YES : DEF_NO;    /* Chk state change.                                    */

    if (chngd == DEF_NO) {
        switch (p_dev->State) {
            case FS_DEV_STATE_PRESENT:                          /* If dev present ...                                   */
                 chngd = chngd_dev;                             /* ... dev chngd if drv asserts dev chngd.              */
                 break;


            case FS_DEV_STATE_LOW_FMT_VALID:                    /* If dev present & low fmt'd            ...            */
                 if (chngd_dev == DEF_NO) {                     /* ... even if drv asserts dev not chngd ...            */
                                                                /* ... chk dev params.                                  */
                     if ((sec_size_prev != p_dev->SecSize) ||
                         (size_prev     != p_dev->Size)) {
                        chngd = DEF_YES;
                     }

                 } else {                                       /* If drv asserts dev chngd ...                         */
                    chngd = DEF_YES;                            /* ... dev chngd.                                       */
                 }
                 break;


            case FS_DEV_STATE_CLOSED:
            case FS_DEV_STATE_CLOSING:
            case FS_DEV_STATE_OPENING:
            case FS_DEV_STATE_OPEN:
            default:
                 break;
        }
    }

    if (chngd == DEF_YES) {
        p_dev->RefreshCnt++;
    }

    return (chngd);
}


/*
*********************************************************************************************************
*                                        FSDev_ReleaseLocked()
*
* Description : Release device sector(s).
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               start       Start sector of released.
*
*               cnt         Number of sectors to released.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                               FS_ERR_NONE                    Device sector(s) released.
*                               FS_ERR_DEV_INVALID_SEC_NBR     Sector start or count invalid.
*
*                                                              ----- RETURNED BY DEV DRV's IO_Ctrl() ----
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_ReleaseLocked (FS_DEV      *p_dev,
                           FS_SEC_NBR   start,
                           FS_SEC_QTY   cnt,
                           FS_ERR      *p_err)
{
    FS_SEC_QTY  size;


                                                                /* ------------------ VALIDATE ARGS ------------------- */
    size = p_dev->Size;

    if (cnt == 0u) {
       *p_err = FS_ERR_NONE;
        return;
    }

    if (start > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }

    if (start + cnt > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }



                                                                /* ------------------- RELEASE SECs ------------------- */
   *p_err = FS_ERR_NONE;
    while ((*p_err == FS_ERR_NONE) &&
           ( cnt    > 0u         )   ) {
        p_dev->DevDrvPtr->IO_Ctrl(         p_dev,
                                           FS_DEV_IO_CTRL_SEC_RELEASE,
                                  (void *)&start,
                                           p_err);

        cnt--;
        start++;
    }


                                                                /* -------------------- HANDLE ERR -------------------- */
    if (*p_err == FS_ERR_DEV_INVALID_IO_CTRL) {
        *p_err =  FS_ERR_NONE;
    }

    FSDev_HandleErr(p_dev, *p_err);                             /* See Note #2.                                         */
}
#endif


/*
*********************************************************************************************************
*                                           FSDev_SyncLocked()
*
* Description : Synchronize a device. Perform any pending operation that would otherwise prevent the
*               device from being remounted in the current state.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*               ----------  Argument validated by caller.
*
*                              TDB
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_SyncLocked (FS_DEV  *p_dev,
                        FS_ERR  *p_err)
{
    p_dev->DevDrvPtr->IO_Ctrl(p_dev,
                              FS_DEV_IO_CTRL_SYNC,
                              DEF_NULL,
                              p_err);
}
#endif


/*
*********************************************************************************************************
*                                          FSDev_WrLocked()
*
* Description : Write data to device sector(s).
*
* Argument(s) : p_dev       Pointer to device.
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
*                               FS_ERR_NONE                    Device sector(s) read.
*                               FS_ERR_DEV_INVALID_SEC_NBR     Sector start or count invalid.
*
*                                                              ------- RETURNED BY DEV DRV's Wr() -------
*                               FS_ERR_DEV_INVALID_LOW_FMT     Device needs to be low-level formatted.
*                               FS_ERR_DEV_IO                  Device I/O error.
*                               FS_ERR_DEV_TIMEOUT             Device timeout error.
*                               FS_ERR_DEV_NOT_PRESENT         Device is not present.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*
*               (2) Device state change will result from device I/O, not present or timeout error.
*********************************************************************************************************
*/

#if (FS_CFG_RD_ONLY_EN == DEF_DISABLED)
void  FSDev_WrLocked (FS_DEV      *p_dev,
                      void        *p_src,
                      FS_SEC_NBR   start,
                      FS_SEC_QTY   cnt,
                      FS_ERR      *p_err)
{
    FS_SEC_QTY    size;
#if (FS_CFG_DBG_WR_VERIFY_EN == DEF_ENABLED)
    FS_BUF       *p_buf;
    FS_SEC_NBR    sec;
    void         *p_dest;
    CPU_INT08U   *p_src_08;
    CPU_BOOLEAN   cmp;
    FS_ERR        verify_err;
#endif


                                                                /* ------------------ VALIDATE ARGS ------------------- */
    size = p_dev->Size;

    if (cnt == 0u) {
       *p_err = FS_ERR_NONE;
        return;
    }

    if (start > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }

    if (start + cnt > size) {
       *p_err = FS_ERR_DEV_INVALID_SEC_NBR;
        return;
    }



                                                                /* ---------------------- WR DEV ---------------------- */
    p_dev->DevDrvPtr->Wr(p_dev,
                         p_src,
                         start,
                         cnt,
                         p_err);



                                                                /* ----------------- UPDATE DEV STATS ----------------- */
    FS_CTR_STAT_ADD(p_dev->StatWrSecCtr, (FS_CTR)cnt);



                                                                /* -------------------- HANDLE ERR -------------------- */
    FSDev_HandleErr(p_dev, *p_err);                             /* See Note #2.                                         */



                                                                /* --------------------- VERIFY WR -------------------- */
#if (FS_CFG_DBG_WR_VERIFY_EN == DEF_ENABLED)
    if (*p_err == FS_ERR_NONE) {
        p_buf = FSBuf_Get((FS_VOL *)0);                         /* Alloc buf.                                           */
        if (p_buf == (FS_BUF *)0) {                             /* If no buf ...                                        */
            FS_TRACE_DBG(("FSDev_WrLocked(): Could not verify wr, buf could not be alloc'd.\r\n"));
            return;                                             /* ... just rtn.                                        */
        }

        sec      =  start;
        p_dest   =  p_buf->DataPtr;
        p_src_08 = (CPU_INT08U *)p_src;
        while (cnt > 0u) {
            p_dev->DevDrvPtr->Rd(p_dev,                         /* Rd data wr.                                          */
                                 p_dest,
                                 sec,
                                 1u,
                                &verify_err);

            if (verify_err != FS_ERR_NONE) {
                FS_TRACE_DBG(("FSDev_WrLocked(): Wr verify failed; could not rd dev.\r\n"));
                FSBuf_Free(p_buf);
                return;
            }

            cmp = Mem_Cmp((void       *)p_dest,                 /* Cmp data wr to data rd.                              */
                          (void       *)p_src_08,
                          (CPU_SIZE_T  )p_dev->SecSize);

            if (cmp != DEF_YES) {
                FS_TRACE_DBG(("FSDev_WrLocked(): Wr verify failed at sec %d.\r\n", sec));
                FSBuf_Free(p_buf);
                return;
            }

            cnt--;
            sec++;
            p_src_08 += p_dev->SecSize;
        }

        FSBuf_Free(p_buf);
    }
#endif
}
#endif


/*
*********************************************************************************************************
*                                            FSDev_VolAdd()
*
* Description : Add volume to open volume list.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

void  FSDev_VolAdd (FS_DEV  *p_dev,
                    FS_VOL  *p_vol)
{
   (void)p_vol;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */

    p_dev->VolCnt++;
}


/*
*********************************************************************************************************
*                                          FSDev_VolRemove()
*
* Description : Remove volume from open volume list.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
*               p_vol       Pointer to volume.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

void  FSDev_VolRemove (FS_DEV  *p_dev,
                       FS_VOL  *p_vol)
{
   (void)p_vol;                                                 /*lint --e{550} Suppress "Symbol not accessed".         */

    p_dev->VolCnt--;
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
*                                           FS_DevDrvFind()
*
* Description : Find device driver by name.
*
* Argument(s) : name_dev    Pointer to a device name.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to device driver.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_DEV_API  *FS_DevDrvFind (const  CPU_CHAR  *name_dev)
{
           FS_QTY        ix;
           CPU_INT16S    cmp;
           FS_DEV_API   *p_dev_drv;
    const  CPU_CHAR     *p_name_drv;


    p_dev_drv = DEF_NULL;


    for (ix = 0u; ix < FS_DevDrvCntMax; ix++) {
        if (FS_DevDrvTbl[ix] != DEF_NULL) {
            p_name_drv = FS_DevDrvTbl[ix]->NameGet();
            cmp        = Str_Cmp_N(name_dev, p_name_drv, FS_CFG_MAX_DEV_DRV_NAME_LEN);
            if (cmp == 0) {
                p_dev_drv = FS_DevDrvTbl[ix];
                break;
            }
        }
    }

    if (p_dev_drv == DEF_NULL) {
        return (DEF_NULL);
    }

    return (p_dev_drv);
}


/*
*********************************************************************************************************
*                                          FSDev_HandleErr()
*
* Description : Handle error from device access.
*
* Argument(s) : p_dev       Pointer to device.
*               -----       Argument validated by caller.
*
*               err         Error returned by device driver access :
*
*                               FS_ERR_DEV_IO                 Device I/O error.
*                               FS_ERR_DEV_INVALID_LOW_FMT    Device needs to be low-level formatted.
*                               FS_ERR_DEV_NOT_OPEN           Device is not open.
*                               FS_ERR_DEV_NOT_PRESENT        Device is not present.
*                               FS_ERR_DEV_TIMEOUT            Device timeout error.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST have acquired a reference to the device & hold the device lock.
*********************************************************************************************************
*/

static  void  FSDev_HandleErr (FS_DEV  *p_dev,
                               FS_ERR   err)
{
    FS_STATE   state_prev;
    FS_STATE   state_next;


    state_prev = p_dev->State;
    state_next = state_prev;

    switch (err) {
        case FS_ERR_DEV_INVALID_IO_CTRL:
        case FS_ERR_NONE:
             break;


        case FS_ERR_DEV_INVALID_LOW_FMT:
             state_next = FS_DEV_STATE_PRESENT;
             break;


        case FS_ERR_DEV_IO:
        case FS_ERR_DEV_NOT_OPEN:
        case FS_ERR_DEV_NOT_PRESENT:
        case FS_ERR_DEV_TIMEOUT:
             state_next = FS_DEV_STATE_OPEN;
             break;


        default:
             break;
    }

    if (state_next != state_prev) {                             /* If dev state chnged ...                              */
        p_dev->State = state_next;
        p_dev->RefreshCnt++;                                    /* ... inc refresh cnt to invalidate vols & files.      */
    }
}


/*
*********************************************************************************************************
*                                            FSDev_ObjClr()
*
* Description : Clear a device object.
*
* Argument(s) : p_dev       Pointer to device.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSDev_ObjClr (FS_DEV  *p_dev)
{
    p_dev->ID           =  0u;
    p_dev->State        =  FS_DEV_STATE_CLOSED;
    p_dev->RefCnt       =  0u;
    p_dev->RefreshCnt   =  0u;

    Mem_Set((void       *)&p_dev->Name[0],
            (CPU_INT08U  ) ASCII_CHAR_NULL,
            (CPU_SIZE_T  )(FS_CFG_MAX_DEV_NAME_LEN + 1u));

    p_dev->UnitNbr      =  0u;
    p_dev->Size         =  0u;
    p_dev->SecSize      =  0u;
    p_dev->Fixed        =  DEF_NO;

    p_dev->VolCnt       =  0u;
    p_dev->DevDrvPtr    = (FS_DEV_API *)0;
    p_dev->DataPtr      =  DEF_NULL;

#if (FS_CFG_CTR_STAT_EN == DEF_ENABLED)
    p_dev->StatRdSecCtr =  0u;
    p_dev->StatWrSecCtr =  0u;
#endif
}


/*
*********************************************************************************************************
*                                           FSDev_ObjFind()
*
* Description : Find a device object by name.
*
* Argument(s) : name_dev    Pointer to device name.
*               ----------  Argument validated by caller.
*
* Return(s)   : Pointer to a device, if found.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_DEV  *FSDev_ObjFind (CPU_CHAR  *name_dev)
{
    CPU_INT16S   cmp;
    FS_DEV      *p_dev;
    FS_QTY       dev_ix;


    p_dev = DEF_NULL;

    for (dev_ix = 0u; dev_ix < FSDev_DevCntMax; dev_ix++) {
        if (FSDev_Tbl[dev_ix]!= DEF_NULL) {
            cmp = Str_Cmp_N(name_dev, FSDev_Tbl[dev_ix]->Name, FS_CFG_MAX_DEV_NAME_LEN);
            if (cmp == 0) {
                p_dev = FSDev_Tbl[dev_ix];
                break;
            }
        }
    }

    return (p_dev);
}


/*
*********************************************************************************************************
*                                           FSDev_ObjFree()
*
* Description : Free a device object.
*
* Argument(s) : p_dev       Pointer to a device.
*               ----------  Argument validated by caller.
*
* Return(s)   : none.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  void  FSDev_ObjFree (FS_DEV  *p_dev)
{
    LIB_ERR  pool_err;


    FSDev_Tbl[p_dev->ID] = DEF_NULL;

#if (FS_CFG_DBG_MEM_CLR_EN == DEF_ENABLED)
    FSDev_ObjClr(p_dev);
#endif

    Mem_PoolBlkFree(        &FSDev_Pool,
                    (void *) p_dev,
                            &pool_err);

    if (pool_err != LIB_MEM_ERR_NONE) {
        FS_TRACE_INFO(("FSDev_ObjFree(): Could not free dev to pool.\r\n"));
        CPU_SW_EXCEPTION(;);
    }

    FSDev_Cnt--;
}


/*
*********************************************************************************************************
*                                            FSDev_ObjGet()
*
* Description : Allocate & initialize a device object.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to a device, if NO errors.
*               Pointer to NULL,     otherwise.
*
* Note(s)     : (1) The function caller MUST hold the file system lock.
*********************************************************************************************************
*/

static  FS_DEV  *FSDev_ObjGet (void)
{
    FS_DEV   *p_dev    = DEF_NULL;
    LIB_ERR   pool_err;
    FS_QTY    dev_ix;


    for (dev_ix = 0u; dev_ix < FSDev_DevCntMax; dev_ix++) {
        if (FSDev_Tbl[dev_ix] == DEF_NULL) {
            break;
        }
    }

    if (dev_ix >= FSDev_DevCntMax) {
        goto exit_fail;
    }

    p_dev = (FS_DEV *)Mem_PoolBlkGet(&FSDev_Pool,
                                      sizeof(FS_DEV),
                                     &pool_err);
    (void)pool_err;                                            /* Err ignored. Ret val chk'd instead.                  */
    if (p_dev == (FS_DEV *)0) {
        goto exit_fail;
    }

    FSDev_ObjClr(p_dev);

    FSDev_Tbl[dev_ix] = p_dev;

    FSDev_Cnt++;

    p_dev->ID = dev_ix;

    return (p_dev);

exit_fail:
    FS_TRACE_INFO(("FSDev_ObjGet(): Could not alloc dev from pool. Opened device limit reached.\r\n"));
    return (p_dev);
}


/*
*********************************************************************************************************
*                                          FSDev_NameParse()
*
* Description : Extract name & unit number of device.
*
* Argument(s) : name_full       Full file name.
*               ----------      Argument validated by caller.
*
*               name_dev        String that will receive the device base name.
*                                   OR
*                               String that will receive empty string if device name is not specified.
*               ----------      Argument validated by caller.
*
*               p_unit_nbr      Pointer to variable that will receive the unit number.
*               ----------      Argument validated by caller.
*
* Return(s)   : Pointer to start of string after device name.
*
* Note(s)     : (1) See 'FSDev_Open() Notes #1a'.
*
*               (2) 'name_dev' MUST point to a character array of FS_CFG_MAX_DEV_NAME_LEN
*                   characters.
*
*               (3) The last character in a valid device name is the second colon.  Consequently, the
*                   return pointer from this function, if no error is returned, should be checked to be
*                   certain that it is a pointer to a NULL character.
*********************************************************************************************************
*/

static  CPU_CHAR  *FSDev_NameParse (CPU_CHAR  *name_full,
                                    CPU_CHAR  *name_dev,
                                    FS_QTY    *p_unit_nbr)
{
    CPU_CHAR     *p_colon;
    CPU_SIZE_T    dev_name_len;
    CPU_SIZE_T    dev_drv_name_len;
    CPU_BOOLEAN   dig;
    CPU_CHAR     *p_name_rem;
    CPU_BOOLEAN   ok;
    FS_QTY        unit_nbr;


    Mem_Clr((void *)name_dev, FS_CFG_MAX_DEV_NAME_LEN + 1u);

   *p_unit_nbr = 0u;                                            /* Assign dflt rtn val.                                 */



                                                                /* ----------------- CHK DEV NAME LEN ----------------- */
    dev_name_len = Str_Len_N(name_full, FS_CFG_MAX_DEV_NAME_LEN + 1u);
    if (dev_name_len > FS_CFG_MAX_DEV_NAME_LEN) {               /* Rtn err if dev name too long.                        */
        return ((CPU_CHAR *)0);
    }



                                                                /* ------------------ FIND 1st COLON ------------------ */
    p_colon = Str_Char_N(name_full, FS_CFG_MAX_DEV_NAME_LEN, FS_CHAR_DEV_SEP);

    if (p_colon == (CPU_CHAR *)0) {                             /* Rtn err if 1st colon not found                       */
        return ((CPU_CHAR *)0);
    }

    if (p_colon == name_full) {                                 /* Rtn err if 1st char is colon.                        */
        return ((CPU_CHAR *)0);
    }

    dev_drv_name_len = (CPU_SIZE_T)(p_colon - name_full);       /*lint !e946 !e947 Both pts are in str 'name_full'.     */

    if (dev_drv_name_len > FS_CFG_MAX_DEV_DRV_NAME_LEN) {       /* Rtn err if dev drv name too long.                    */
        return ((CPU_CHAR *)0);
    }



                                                                /* ------------------- COPY DEV NAME ------------------ */
    Str_Copy_N(name_dev,
               name_full,
               dev_drv_name_len);



                                                                /* ------------------ FIND 2nd COLON ------------------ */
    name_full = p_colon + 1u;
    p_colon   = Str_Char_N(name_full, FS_CFG_MAX_DEV_NAME_LEN, FS_CHAR_DEV_SEP);

    if (p_colon == (CPU_CHAR *)0) {                             /* Rtn err if 2nd colon not found.                      */
        name_dev[0] = (CPU_CHAR)ASCII_CHAR_NULL;
        return ((CPU_CHAR *)0);
    }

    if (p_colon == name_full) {                                 /* Rtn err if two consecutive colons found.             */
        name_dev[0] = (CPU_CHAR)ASCII_CHAR_NULL;
        return ((CPU_CHAR *)0);
    }



                                                                /* ------------------ PARSE UNIT NBR ------------------ */
    unit_nbr = 0u;
    ok       = DEF_YES;

    while ((name_full <  p_colon ) &&                           /*lint !e946 !e947 Both pts are in str 'name_full'.     */
           (ok        == DEF_YES)) {                            /* Parse unit nbr.                                      */
        dig = ASCII_IS_DIG(*name_full);
        if (dig == DEF_YES) {                                   /* Digit found.                                         */
            if (unit_nbr > 24u) {                               /* Unit nbr ovf.                                        */
                ok = DEF_NO;
            } else {
                unit_nbr *=  DEF_NBR_BASE_DEC;
                unit_nbr += (FS_QTY)*name_full - (FS_QTY)ASCII_CHAR_DIGIT_ZERO;
            }
        } else {                                                /* Non-digit found.                                     */
            ok = DEF_NO;
        }
        name_full++;
    }


    if (ok == DEF_NO) {                                         /* Invalid unit nbr.                                    */
       *p_unit_nbr = 0u;
        return ((CPU_CHAR *)0);
    } else {
        p_name_rem = p_colon + 1u;
       *p_unit_nbr = unit_nbr;
        return (p_name_rem);
    }
}

