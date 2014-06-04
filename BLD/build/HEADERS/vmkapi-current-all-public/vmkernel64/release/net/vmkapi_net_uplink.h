/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink                                                         */ /**
 * \addtogroup Network
 *@{
 * \defgroup Uplink Uplink management
 *@{
 *
 * In VMkernel, uplinks are physical NICs, also known as `pNics'. They
 * provide external connectivity.
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_UPLINK_H_
#define _VMKAPI_NET_UPLINK_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */



/**
 * \brief Uplink handle
 *
 * vmk_Uplink is used as a handle to perform operations on uplink devices.
 *
 */
typedef struct UplinkDev * vmk_Uplink;

/** \brief Uplink event callback handle */
typedef void *vmk_UplinkEventCBHandle;

/** \brief Event identifier for uplink notifications. */
typedef vmk_uint64 vmk_UplinkEvent;

/** \brief Data associated with uplink event */
typedef struct vmk_UplinkEventData vmk_UplinkEventData;

/** Uplink link state is physical up */
#define VMK_UPLINK_EVENT_LINK_UP      0x01

/** Uplink link state is physical down */
#define VMK_UPLINK_EVENT_LINK_DOWN    0x02

/** Uplink has been connected to a portset */
#define VMK_UPLINK_EVENT_CONNECTED    0x04

/** Uplink has been disconnected from a portset */
#define VMK_UPLINK_EVENT_DISCONNECTED 0x08

/** Uplink has been enabled on a portset */
#define VMK_UPLINK_EVENT_ENABLED      0x10

/** Uplink has been disabled on a portset */
#define VMK_UPLINK_EVENT_DISABLED     0x20

/** Uplink has been blocked on a portset */
#define VMK_UPLINK_EVENT_BLOCKED      0x40

/** Uplink bas been unblocked on a portset */
#define VMK_UPLINK_EVENT_UNBLOCKED    0x80

/*
 ***********************************************************************
 * vmk_UplinkEventCB --                                           */ /**
 *
 * \brief Message callback used for uplink event notification
 *
 * \note Uplink event notifications are asynchronous, meaning that the
 *       event handler should examine the uplink name to determine
 *       current state at the time the callback is made.
 *
 * \note No portset lock is held when callback is made. To acquire a
 *       portset, callback needs to call vmk_UplinkGetByName,
 *       vmk_UplinkGetPortID and vmk_PortsetAcquireByPortID.
 *
 * \param[in]  uplinkName  name of uplink the event implicates
 * \param[in]  event       uplink event
 * \param[in]  eventData   data associated with event, reserved for
 *                         future use, always be NULL.
 * \param[in]  cbData      Data passed to vmk_UplinkRegisterEventCB
 *
 * \retval     None
 *
 ***********************************************************************
 */

typedef void (*vmk_UplinkEventCB)(vmk_Name *uplinkName,
                                  vmk_UplinkEvent event,
                                  vmk_UplinkEventData *eventData,
                                  void *cbData);


/*
 ***********************************************************************
 * vmk_UplinkIoctl --                                             */ /**
 *
 * \brief Do an ioctl call against the uplink.
 *
 * This function will call down to device driver to perform an ioctl.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the ioctl callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of ioctl calls with the provider of
 * the driver.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   cmd               Ioctl command
 * \param[in]   args              Ioctl arguments
 * \param[out]  result            Ioctl result
 *
 * \retval      VMK_OK            If the ioctl call succeeds
 * \retval      VMK_NOT_SUPPORTED If the uplink doesn't support ioctl
 * \retval      Other status      If the device ioctl call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkIoctl(vmk_Uplink  uplink,
                                 vmk_uint32  cmd,
                                 void       *args,
                                 vmk_uint32 *result);


/*
 ***********************************************************************
 * vmk_UplinkReset --                                             */ /**
 *
 * \brief Reset the uplink device underneath.
 *
 * This function will call down to device driver, close and re-open the
 * device. The link state will consequently go down and up.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the reset callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of the reset call with the provider of
 * the driver.
 *
 * \note This call is asynchronous, the function might return before
 * the driver call completed.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            if the reset call succeeds
 * \retval      VMK_NOT_SUPPORTED if the uplink doesn't support reset
 * \retval      Other status      if the device reset call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkReset(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkGetByName --                                         */ /**
 *
 * \brief Get uplink pointer by its name
 *
 * This function will look through uplink list and return the matched
 * uplink.
 *
 * \note The caller must not hold any lock
 *
 * \note This function may block
 *
 * \param[in]   uplinkName        Uplink name
 * \param[out]  uplink            Pointer to uplink
 *
 * \retval      VMK_OK            if uplink is found
 * \retval      VMK_NOT_FOUND     if uplink is not found
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkGetByName(vmk_Name *uplinkName,
                                     vmk_Uplink *uplink);



/*
 ***********************************************************************
 * vmk_UplinkGetPortID --                                         */ /**
 *
 * \brief Return ID of port uplink is connecting to
 *
 * This function will return the ID of port where uplink connects
 *
 * \note The caller must not hold any lock
 *
 * \note This function will not block
 *
 * \param[in]   uplink            Uplink name
 * \param[out]  portID            Port ID
 *
 * \retval      VMK_OK            If get port ID call succeeds
 * \retval      VMK_BAD_PARAM     If uplink or portID is invalid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkGetPortID(vmk_Uplink uplink,
                                     vmk_SwitchPortID *portID);



/*
 ***********************************************************************
 * vmk_UplinkRegisterEventCB --                                   */ /**
 *
 * \brief Register a handler to receive uplink event notification
 *
 * \note These are asynchronous event notifications, meaning that the
 *       event handler should examine the uplink to determine current
 *       state at the time the callback is made.
 *
 * \note If NULL uplink is specified, handler will be linked to global
 *       event list and it will receive events from any uplink.
 *
 * \param[in]   uplink            Pointer of uplink where handler will
 *                                be linked
 * \param[in]   eventMask         Combination of uplink event IDs
 *                                handler is interested in
 * \param[in]   cb                Handler to call to notify an uplink
 *                                event
 * \param[in]   cbData            Data to pass to the handler
 * \param[out]  handle            Handle to unregister this handler
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkRegisterEventCB(
   vmk_Uplink uplink,
   vmk_uint64 eventMask,
   vmk_UplinkEventCB cb,
   void *cbData,
   vmk_UplinkEventCBHandle *handle);


/*
 ***********************************************************************
 * vmk_UplinkUnregisterEventCB --                                 */ /**
 *
 * \brief  Unregister a handler to receive uplink event notifications.
 *
 * \param[in]   handle            Handle return by register process
 *
 * \retval      VMK_OK            Always
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkUnregisterEventCB(
   vmk_UplinkEventCBHandle handle);


#endif /* _VMKAPI_NET_UPLINK_H_ */
/** @} */
/** @} */
