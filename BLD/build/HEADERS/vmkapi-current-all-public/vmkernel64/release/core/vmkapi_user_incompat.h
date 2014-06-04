/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * User Space Interface                                                  */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup User Space
 * @{
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_USER_INCOMPAT_H_
#define _VMKAPI_CORE_USER_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_UserMapCallback --                                         */ /**
 *
 * \brief Callback function invoked when user mapping is released.
 *
 * \param[in] callbackParam    Opaque parameter for callback function.
 *
 ***********************************************************************
 */
typedef void (*vmk_UserMapCallback)(void *);

/*
 ***********************************************************************
 * vmk_UserMap --                                                 */ /**
 *
 * \brief Map the provided machine address ranges into a contiguous
 *        virtual address space of current user world.
 *
 * \note See Mapping section for description of mapRequest parameter.
 * \note The only supported mapping attributes are READONLY, READWRITE,
 *       WRITECOMBINE, and UNCACHED.  Any other attribute will cause
 *       mapping to fail with return status VMK_BAD_PARAM.
 *
 * \param[in]     moduleID     Module ID of the caller.
 * \param[in]     mapRequest   Pointer to a mapRequest structure.
 * \param[in,out] vaddr        Pointer to virtual address of mapping
 *                             (non-zero to specify a virtual address,
 *                              or zero for default address).
 * \param[in] callbackFunction Function to call when mapping is released.
 * \param[in] callbackParam    Opaque parameter for callbackFunction.
 *
 * \retval VMK_OK              Map is successful.
 * \retval VMK_BAD_PARAM       Input parameter is invalid.
 * \retval VMK_NO_MEMORY       Unable to allocate mapping request.
 * \retval VMK_NO_RESOURCES    Unable to allocate mapping request.
 * \retval VMK_INVALID_ADDRESS Requested address not in map range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserMap(
   vmk_ModuleID moduleID,
   vmk_MapRequest *mapRequest,
   vmk_VA *vaddr,
   vmk_UserMapCallback callbackFunction,
   void *callbackParam);

/*
 ***********************************************************************
 * vmk_UserUnmap --                                               */ /**
 *
 * \brief Unmap user world virtual address space mapped by vmk_UserMap().
 *
 * \param[in] vaddr            Virtual address to unmap.
 * \param[in] length           Length of address space in bytes.
 *
 * \retval VMK_OK              Unmap is successful.
 * \retval VMK_NOT_FOUND       Virtual address and length not mapped.
 * \retval VMK_INVALID_ADDRESS Requested address is not page aligned.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserUnmap(
   vmk_VA vaddr,
   vmk_ByteCount length);

/*
 ***********************************************************************
 * vmk_UserAddValidMPNRange --                                    */ /**
 *
 * \brief Indicate a range of consecutive MPNs can be referenced by
 *        user worlds.
 *
 * \param[in] mpn              First MPN in range.
 * \param[in] numPages         Number of machine pages in range.
 *
 * \retval VMK_OK              MPNs added to user worlds.
 * \retval VMK_BAD_PARAM       Input parameter is invalid.
 * \retval VMK_NO_MEMORY       Unable to allocate memory for request.
 * \retval VMK_INVALID_PAGE_NUMBER  MPN range intersects with existing
 *                             MPN range.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserAddValidMPNRange(
   vmk_MPN mpn,
   vmk_uint32 numPages);

/*
 ***********************************************************************
 * vmk_UserRemoveValidMPNRange --                                 */ /**
 *
 * \brief Remove a range of consecutive MPNs from user worlds.
 *
 * \param[in] mpn              First MPN in range.
 * \param[in] numPages         Number of machine pages in range.
 *
 * \retval VMK_OK              MPNs removed from user worlds.
 * \retval VMK_NOT_FOUND       MPN range not found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UserRemoveValidMPNRange(
   vmk_MPN mpn,
   vmk_uint32 numPages);

#endif
/** @} */
/** @} */
