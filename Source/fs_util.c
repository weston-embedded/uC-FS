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
*                                  FILE SYSTEM SUITE UTILITY LIBRARY
*
* Filename : fs_util.c
* Version  : V4.08.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  FS_UTIL_MODULE


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <lib_def.h>
#include  "fs.h"
#include  "fs_util.h"


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
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            FSUtil_Log2()
*
* Description : Calculate ceiling of base-2 logarithm of integer.
*
* Argument(s) : val         Integer value.
*
* Return(s)   : Logarithm of value, if val > 0;
*               0,                  otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  FSUtil_Log2 (CPU_INT32U  val)
{
    CPU_SIZE_T  log_val;
    CPU_SIZE_T  val_cmp;


    val_cmp = 1u;
    log_val = 0u;
    while (val_cmp < val) {
        val_cmp *= 2u;
        log_val += 1u;
    }

    return (log_val);
}


/*
*********************************************************************************************************
*                                            FSUtil_ValPack32()
*
* Description : Packs a specified number of least significant bits of a 32-bit value to a specified octet
*               and bit position within an octet array, in little-endian order.
*
* Argument(s) : p_dest          Pointer to destination octet array.
*
*               p_offset_octet  Pointer to octet offset into 'p_dest'. This function adjusts the pointee
*                               to the new octet offset within octet array.
*
*               p_offset_bit    Pointer to bit offset into initial 'p_dest[*p_offset_octet]'. This function
*                               adjusts the pointee to the new bit offset within octet.
*
*               val             Value to pack into 'p_dest' array.
*
*               nbr_bits        Number of least-significants bits of 'val' to pack into 'p_dest'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSUtil_ValPack32 (CPU_INT08U  *p_dest,
                        CPU_SIZE_T  *p_offset_octet,
                        CPU_DATA    *p_offset_bit,
                        CPU_INT32U   val,
                        CPU_DATA     nbr_bits)
{
    CPU_INT32U  val_32_rem;
    CPU_DATA    nbr_bits_rem;
    CPU_DATA    nbr_bits_partial;
    CPU_INT08U  val_08;
    CPU_INT08U  val_08_mask;
    CPU_INT08U  dest_08_mask;
    CPU_INT08U  dest_08_mask_lsb;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------ VALIDATE ARGS ------------------- */
    if (p_dest == DEF_NULL) {                                   /* Validate dest  array  ptr.                           */
        CPU_SW_EXCEPTION(;);
    }

    if (p_offset_octet == DEF_NULL) {                           /* Validate octet offset ptr.                           */
        CPU_SW_EXCEPTION(;);
    }

    if (p_offset_bit == DEF_NULL) {                             /* Validate bit   offset ptr.                           */
        CPU_SW_EXCEPTION(;);
    }

    if (nbr_bits > (sizeof(val) * DEF_OCTET_NBR_BITS)) {        /* Validate bits nbr.                                   */
        CPU_SW_EXCEPTION(;);
    }

    if (*p_offset_bit >= DEF_OCTET_NBR_BITS) {                  /* Validate bit  offset.                                */
        CPU_SW_EXCEPTION(;);
    }
#endif  /* FS_CFG_ERR_ARG_CHK_EXT_EN */

    nbr_bits_rem = nbr_bits;
    val_32_rem   = val;


                                                                /* ------------ PACK LEADING PARTIAL OCTET ------------ */
    if (*p_offset_bit > 0) {
                                                                /* Calc nbr bits to pack in initial array octet.        */
        nbr_bits_partial = DEF_OCTET_NBR_BITS - *p_offset_bit;
        if (nbr_bits_partial > nbr_bits_rem) {
            nbr_bits_partial = nbr_bits_rem;
        }


        val_08_mask =  DEF_BIT_FIELD_08(nbr_bits_partial, 0u);  /* Calc  mask to apply on 'val_32_rem'.                 */
        val_08      =  val_32_rem & val_08_mask;                /* Apply mask.                                          */
        val_08    <<= *p_offset_bit;                            /* Shift according to bit offset in leading octet.      */


                                                                /* Calc mask for kept non-val bits from leading octet.  */
        dest_08_mask_lsb = *p_offset_bit + nbr_bits_partial;
        dest_08_mask     =  DEF_BIT_FIELD_08(DEF_OCTET_NBR_BITS - dest_08_mask_lsb, dest_08_mask_lsb);
        dest_08_mask    |=  DEF_BIT_FIELD_08(*p_offset_bit, 0u);

        p_dest[*p_offset_octet] &= dest_08_mask;                /* Keep      non-val bits from leading array octet.     */
        p_dest[*p_offset_octet] |= val_08;                      /* Merge leading val bits into leading array octet.     */

                                                                /* Update bit/octet offsets.                            */
       *p_offset_bit += nbr_bits_partial;
        if (*p_offset_bit >= DEF_OCTET_NBR_BITS) {              /* If bit offset > octet nbr bits, ...                  */
            *p_offset_bit  = 0u;                                /* ... zero bit offset (offset <= DEF_OCTET_NBR_BITS)   */
           (*p_offset_octet)++;                                 /* ... and inc octet offset.                            */
        }

                                                                /* Update rem'ing val/nbr bits.                         */
        val_32_rem   >>= nbr_bits_partial;
        nbr_bits_rem  -= nbr_bits_partial;
    }


                                                                /* ---------------- PACK FULL OCTET(S) ---------------- */
    while (nbr_bits_rem >= DEF_OCTET_NBR_BITS) {
        val_08                  = (CPU_INT08U)val_32_rem & DEF_OCTET_MASK;
        p_dest[*p_offset_octet] =  val_08;                      /* Merge full-octet val bits into array octet.          */
      (*p_offset_octet)++;                                      /* Update octet offset.                                 */

                                                                /* Update rem'ing val/nbr bits.                         */
        val_32_rem   >>= DEF_OCTET_NBR_BITS;
        nbr_bits_rem  -= DEF_OCTET_NBR_BITS;
    }


                                                                /* ----------- PACK TRAILING PARTIAL OCTET ------------ */
    if (nbr_bits_rem > 0) {
        val_08_mask  =  DEF_BIT_FIELD_08(nbr_bits_rem, 0u);
        val_08       = (CPU_INT08U)val_32_rem & val_08_mask;    /* Mask trailing val bits for merge.                    */

        dest_08_mask =  DEF_BIT_FIELD_08(DEF_OCTET_NBR_BITS - nbr_bits_rem,
                                         nbr_bits_rem);

        p_dest[*p_offset_octet] &= dest_08_mask;                /* Keep non-val bits of         trailing array octet.   */
        p_dest[*p_offset_octet] |= val_08;                      /* Merge trailing val bits into trailing array octet.   */

       *p_offset_bit += nbr_bits_rem;                           /* Update/rtn final bit offset.                         */
    }
}


/*
*********************************************************************************************************
*                                          FSUtil_ValUnpack32()
*
* Description : Unpacks a specified number of least-significant bits from a specified octet and bit
*               position within an octet array, in little-endian order to a 32-bit value.
*
* Argument(s) : p_src           Pointer to source octet array.
*
*               p_offset_octet  Pointer to octet offset into 'p_src'. This function adjusts the pointee
*                               to the new position within octet array.
*
*               p_offset_bit    Pointer to bit offset into initial 'p_src[*p_offset_octet]'. This function
*                               ajusts the pointee to the new position within octet array.
*
*               nbr_bits        Number of least-significants bits to unpack from 'p_src' into value.
*
* Return(s)   : Unpacked 32-bit value, if no errors;
*               DEF_INT_32U_MAX_VAL,   otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  FSUtil_ValUnpack32 (CPU_INT08U  *p_src,
                                CPU_SIZE_T  *p_offset_octet,
                                CPU_DATA    *p_offset_bit,
                                CPU_DATA     nbr_bits)
{
    CPU_INT32U  val_32;
    CPU_DATA    nbr_bits_partial;
    CPU_DATA    nbr_bits_rem;
    CPU_INT08U  val_08;
    CPU_INT08U  val_08_mask;


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* ------------------ VALIDATE ARGS ------------------- */
    if (p_src == DEF_NULL) {                                    /* Validate src ptr.                                    */
        CPU_SW_EXCEPTION(DEF_INT_32U_MAX_VAL);
    }

    if (p_offset_octet == DEF_NULL) {                           /* Validate octet offset ptr.                           */
        CPU_SW_EXCEPTION(DEF_INT_32U_MAX_VAL);
    }

    if (p_offset_bit == DEF_NULL) {                             /* Validate bit   offset ptr.                           */
        CPU_SW_EXCEPTION(DEF_INT_32U_MAX_VAL);
    }

    if (nbr_bits > (sizeof(val_32) * DEF_OCTET_NBR_BITS)) {     /* Validate bits nbr.                                   */
        CPU_SW_EXCEPTION(DEF_INT_32U_MAX_VAL);
    }

    if (*p_offset_bit >= DEF_OCTET_NBR_BITS) {                  /* Validate bit  offset.                                */
        CPU_SW_EXCEPTION(DEF_INT_32U_MAX_VAL);
    }
#endif  /* FS_CFG_ERR_ARG_CHK_EXT_EN */


    nbr_bits_rem = nbr_bits;
    val_32       = 0u;
                                                                /* ----------- UNPACK LEADING PARTIAL OCTET ----------- */
    if (*p_offset_bit > 0) {
                                                                /* Calc nbr of bits to unpack from first initial octet. */
        nbr_bits_partial = DEF_OCTET_NBR_BITS - *p_offset_bit;
        if (nbr_bits_partial > nbr_bits) {
            nbr_bits_partial = nbr_bits;
        }

        val_08_mask  =  DEF_BIT_FIELD_08(nbr_bits_partial, *p_offset_bit);
        val_08       =  p_src[*p_offset_octet];
        val_08      &=  val_08_mask;                            /* Keep val leading bits.                               */
        val_08     >>= *p_offset_bit;                           /* Shift bit offset to least sig of val.                */

        val_32      |= (CPU_INT32U)val_08;                      /* Merge leading val bits from leading array octet.     */

                                                                /* Update bit/octet offsets.                            */
       *p_offset_bit += nbr_bits_partial;
        if (*p_offset_bit >= DEF_OCTET_NBR_BITS) {              /* If bit offset > octet nbr bits, ...                  */
            *p_offset_bit  = 0u;                                /* ... zero bit offset (offset <= DEF_OCTET_NBR_BITS)   */
           (*p_offset_octet)++;                                 /* ... and inc octet offset.                            */
        }


        nbr_bits_rem -= nbr_bits_partial;                       /* Update rem'ing nbr bits.                             */
    }


                                                                /* -------------- UNPACK FULL OCTET(S) ---------------- */
    while (nbr_bits_rem >= DEF_OCTET_NBR_BITS) {
        val_08 = p_src[*p_offset_octet];
                                                                /* Merge full-octet val bits into array octet.          */
        if (nbr_bits >= nbr_bits_rem) {
            val_32 |= (val_08 << ((CPU_INT08U)nbr_bits - (CPU_INT08U)nbr_bits_rem));
        } else {
            CPU_SW_EXCEPTION(DEF_INT_08U_MAX_VAL);
        }

      (*p_offset_octet)++;                                      /* Update octet offset.                                 */

        nbr_bits_rem -= DEF_OCTET_NBR_BITS;                     /* Update rem'ing nbr bits.                             */
    }


                                                                /* ----------- UNPACK FINAL TRAILING OCTET ------------ */
    if (nbr_bits_rem  >  0) {
        val_08_mask   =  DEF_BIT_FIELD_08(nbr_bits_rem, 0u);
        val_08        =  p_src[*p_offset_octet];
        val_08       &=  val_08_mask;                           /* Keep val trailing bits.                              */

                                                                /* Merge trailing val bits from trailing array octet.   */
        if (nbr_bits >= nbr_bits_rem) {
            val_32 |= (val_08 << ((CPU_INT08U)nbr_bits - (CPU_INT08U)nbr_bits_rem));
        } else {
            CPU_SW_EXCEPTION(DEF_INT_08U_MAX_VAL);
        }


       *p_offset_bit += nbr_bits_rem;                           /* Update bit offset.                                   */
    }

    return (val_32);
}


/*
*********************************************************************************************************
*                                          FSUtil_MapBitIsSet()
*
* Description : Determines if specified bit of bitmap is set.
*
* Argument(s) : p_bitmap        Pointer to bitmap.
*               --------        Argument validated by caller.
*
*               offset_bit      Offset of bit in bitmap to test.
*
* Return(s)   : DEF_YES, if bit is set;
*               DEF_NO , otherwise.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  FSUtil_MapBitIsSet (CPU_INT08U  *p_bitmap,
                                 CPU_SIZE_T   offset_bit)
{
    CPU_SIZE_T   offset_octet;
    CPU_DATA     offset_bit_in_octet;
    CPU_INT08U   bit_mask;
    CPU_BOOLEAN  bit_set;


    offset_octet        = offset_bit >> DEF_OCTET_TO_BIT_SHIFT;
    offset_bit_in_octet = offset_bit &  DEF_OCTET_TO_BIT_MASK;

    bit_mask = DEF_BIT(offset_bit_in_octet);
    bit_set  = DEF_BIT_IS_SET(p_bitmap[offset_octet], bit_mask);

    return (bit_set);
}


/*
*********************************************************************************************************
*                                           FSUtil_MapBitSet()
*
* Description : Set specified bit in bitmap.
*
* Argument(s) : p_bitmap        Pointer to bitmap.
*               --------        Argument validated by caller.
*
*               offset_bit      Offset of bit in bitmap to test.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSUtil_MapBitSet (CPU_INT08U  *p_bitmap,
                        CPU_SIZE_T   offset_bit)
{
    CPU_SIZE_T  offset_octet;
    CPU_DATA    offset_bit_in_octet;
    CPU_INT08U  bit_mask;


    offset_octet        = offset_bit >> DEF_OCTET_TO_BIT_SHIFT;
    offset_bit_in_octet = offset_bit &  DEF_OCTET_TO_BIT_MASK;

    bit_mask = DEF_BIT(offset_bit_in_octet);
    DEF_BIT_SET_08(p_bitmap[offset_octet], bit_mask);
}


/*
*********************************************************************************************************
*                                           FSUtil_MapBitClr()
*
* Description : Clear specified bit in bitmap.
*
* Argument(s) : p_bitmap        Pointer to bitmap.
*               --------        Argument validated by caller.
*
*               offset_bit      Offset of bit in bitmap to test.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  FSUtil_MapBitClr (CPU_INT08U  *p_bitmap,
                        CPU_SIZE_T   offset_bit)
{
    CPU_SIZE_T  offset_octet;
    CPU_DATA    offset_bit_in_octet;
    CPU_INT08U  bit_mask;


    offset_octet        = offset_bit >> DEF_OCTET_TO_BIT_SHIFT;
    offset_bit_in_octet = offset_bit &  DEF_OCTET_TO_BIT_MASK;

    bit_mask = DEF_BIT(offset_bit_in_octet);
    DEF_BIT_CLR_08(p_bitmap[offset_octet], bit_mask);
}


/*
*********************************************************************************************************
*                                        FSUtil_ModuleDataGet()
*
* Description : Allocate data for a module.
*
* Argument(s) : data_size       Size of module data to allocate, in bytes.
*
*               pp_data_head    Pointer to a void pointer provided by the caller for free data management.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                           FS_ERR_NONE                 Allocation sucessful.
*                           FS_ERR_MEM_ALLOC            Allocation failed.
*                           FS_ERR_NULL_PTR             Head ptr (pp_data_head) is DEF_NULL.
*
* Return(s)   : Pointer to allocated module data, if successful.
*
*               DEF_NULL, otherwise.
*
* Note(s)     : (1) The head ptr provided (void ptr pointed to by pp_data_head) MUST BE CLEARED to DEF_NULL
*                   before the first module data is allocated with this function.
*
*               (2) For proper free data management, the same head ptr must be used over all DataGet/DataFree
*                   operations of a given module.
*
*               (3) Allocated data is cleared to '0'.
*
*               (4) Implementation note :
*                   To provide free data management, a void pointer is added to every module data allocation.
*                   This void pointer, along with the padding needed to align it to CPU_ALIGN, is invisible
*                   to the caller.
*
*                                      +-----+
*                                      |     |
*                                      |     |
*                                      |DATA | +----> Module data.
*                                      |     |
*                                      |     |
*                                      +-----+ +----> Padding (as needed).
*                                      +-----+
*                                      |NEXTP| +----> Void pointer ("next" pointer).
*                                      +-----+
*
*                   As long as the module data is used, the void pointer remains unused.
*                   When the module data becomes useless and is freed with FSUtil_ModuleDataFree(), the
*                   void pointer is used as a "next" pointer to build a list of free module data.
*
*                      p_data_head+--->+-----+  +-->+-----+  +-->+-----+
*                                      |     |  |   |     |  |   |     |
*                                      |     |  |   |     |  |   |     |
*                                      |DATA |  |   |DATA |  |   |DATA |
*                                      |     |  |   |     |  |   |     |
*                                      |     |  |   |     |  |   |     |
*                                      +-----+  |   +-----+  |   +-----+
*                                      +-----+  |   +-----+  |   +-----+
*                                      |NEXTP+--+   |NEXTP+--+   |NEXTP+---> DEF_NULL
*                                      +-----+      +-----+      +-----+
*
*********************************************************************************************************
*/

void  *FSUtil_ModuleDataGet (CPU_SIZE_T    data_size,
                             void        **pp_data_head,
                             FS_ERR       *p_err)
{
    void         *p_data;
    void        **pp_next_data;
    CPU_SIZE_T    data_padding;
    CPU_SIZE_T    alloc_size;
    LIB_ERR       alloc_err;
    CPU_SR_ALLOC();


#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* Validate arg.                                        */
    if(pp_data_head == DEF_NULL) {
       *p_err = FS_ERR_NULL_PTR;
        return (DEF_NULL);
    }
#endif

    if(data_size % sizeof(CPU_ALIGN) != 0) {                    /* Get padding for next ptr alignment.                  */
        data_padding = sizeof(CPU_ALIGN) - data_size % sizeof(CPU_ALIGN);
    } else {
        data_padding = 0u;
    }
    alloc_size   = data_size + data_padding + sizeof(void *);   /* Get size of allocation for data and next ptr.        */

    CPU_CRITICAL_ENTER();
    if (*pp_data_head == DEF_NULL) {                            /* If free data list is empty ...                       */
        p_data = Mem_SegAlloc(DEF_NULL,                         /* ... alloc new data.                                  */
                              DEF_NULL,
                              alloc_size,
                             &alloc_err);
        if (alloc_err != LIB_MEM_ERR_NONE) {
            CPU_CRITICAL_EXIT();
           *p_err = FS_ERR_MEM_ALLOC;
            FS_TRACE_DBG(("FSUtil_ModuleDataGet(): Could not allocate module data."));
            return (DEF_NULL);
        }
    } else {                                                    /* If free data list is non-empty ...                   */
        p_data       = *pp_data_head;                           /* ... use previously freed data.                       */
                                                                /* Get ptr to previous "next data" ptr.                 */
        pp_next_data = (void **)(((CPU_CHAR  *) p_data) + data_size + data_padding);
       *pp_data_head = *pp_next_data;                           /* Set previous "next data" as current head.            */
    }
    CPU_CRITICAL_EXIT();

    Mem_Clr(p_data,                                             /* Clr data.                                            */
            alloc_size);

   *p_err = FS_ERR_NONE;
    return (p_data);
}


/*
*********************************************************************************************************
*                                        FSUtil_ModuleDataFree()
*
* Description : Add module data to list of free module data.
*
* Argument(s) : p_data          Pointer to module data to free.
*
*               pp_data_head    Pointer to the void pointer provided by the caller for free data management.
*                               (See FSUtil_ModuleDataGet().)
*
*               data_size       Size of module data to free, in bytes.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : none.
*
* Note(s)     : (1) The next ptr is invisible for the caller. It is appended at the end of the module data
*                   but is only used internally for free data management, by this function and
*                   FSUtil_ModuleDataGet().
*                   See FSUtil_ModuleDataGet(), Note #4, for more implementation details.
*********************************************************************************************************
*/

void  FSUtil_ModuleDataFree (void         *p_data,
                             CPU_SIZE_T    data_size,
                             void        **pp_data_head,
                             FS_ERR       *p_err)
{
    CPU_SIZE_T    data_padding;
    void        **pp_next_data;
    CPU_SR_ALLOC();


    (void)p_err;

#if (FS_CFG_ERR_ARG_CHK_DBG_EN == DEF_ENABLED)                  /* Validate arg.                                        */
    if ((pp_data_head == DEF_NULL) ||
        (p_data       == DEF_NULL)) {
       *p_err = FS_ERR_NULL_PTR;
        return;
    }
#endif

    if (data_size % sizeof(CPU_ALIGN) != 0) {                   /* Get padding for next ptr alignment.                  */
        data_padding = sizeof(CPU_ALIGN) - data_size % sizeof(CPU_ALIGN);
    } else {
        data_padding = 0u;
    }

    CPU_CRITICAL_ENTER();                                       /* Add to list of free data.                            */
                                                                /* Get ptr to "next data" ptr (See Note #1).            */
    pp_next_data = (void **)(((CPU_CHAR  *) p_data) + data_size + data_padding);
   *pp_next_data = *pp_data_head;                               /* Set "next data" ptr to current head.                 */
   *pp_data_head =  p_data;                                     /* Set data as new head.                                */
    CPU_CRITICAL_EXIT();
}
