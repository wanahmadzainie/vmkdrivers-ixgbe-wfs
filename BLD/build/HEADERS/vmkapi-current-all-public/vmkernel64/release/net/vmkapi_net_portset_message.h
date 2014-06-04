
/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Portset Message                                                */ /**
 * \addtogroup Network
 *@{
 * \defgroup PortsetMessage Portset Message
 *@{
 *
 * \par PortsetMessage:
 *
 * In vmkernel, many different port clients could need to communicate
 * with the external world but also between themselves. These internal
 * communications are done through a portset which is roughly a set of
 * ports with policies connecting a set of ports together.
 *
 * Each client is connected to a port and all the inbound/outbound
 * network packets are going through it.
 * To emulate a physical switch behavior, every port owns a chain of
 * command processing, filtering the packet and post them to their next
 * destination.
 *
 * A client can register a callback handler to receive portset messages.
 * There's a global portset message handler list. Handlers registered to
 * it will receive messages from every portset. Each portset also has
 * its own message callback handler list. Handlers registered to it will
 * only receive messages related to this specific portset.
 *
 * Portset message generated by vmkernel are port related and will be
 * delivered to the callback handler asynchronously. In the handler,
 * client must acquire a portset handle before calling portset or port
 * related APIs. Calling APIs that may generate another portset message
 * is not recommended in callback handler, since it may cause infinite
 * portset message loop.
 *
 * Beside receiving portset messages, client can define its own message
 * and post it to vmkernel globally or to a specific portset. Client
 * must not hold any portset handle when posting a global portset
 * message. While it must hold at least an immutable portset handle when
 * posting a message specific to this portset.
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PORTSET_MESSAGE_H_
#define _VMKAPI_NET_PORTSET_MESSAGE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "vds/vmkapi_vds_prop.h"
#include "vds/vmkapi_vds_portset.h"

/** \brief Message data associated with a portset message */
typedef struct vmk_PortsetMessageData {
   /** \brief Name of custom message */
   vmk_Name      msgName;

   /** \brief Length of data associated with this message */
   vmk_ByteCount msgDataLen;

   /** \brief Message data */
   vmk_uint8     msgData[0];
} vmk_PortsetMessageData;

/** \brief Portset message callback handle */
typedef void *vmk_PortsetMessageCBHandle;

/**
 * \brief Message identifier for portset notifications.
 */
typedef vmk_uint64 vmk_PortsetMessageID;

/** Port has been connected */
#define VMK_PORTSET_MSG_ID_PORT_CONNECT            0x1

/** Port has been disconnected */
#define VMK_PORTSET_MSG_ID_PORT_DISCONNECT         0x2

/** Port has been blocked */
#define VMK_PORTSET_MSG_ID_PORT_BLOCK              0x4

/** Port has been unblocked */
#define VMK_PORTSET_MSG_ID_PORT_UNBLOCK            0x8

/** Port ethernet frame policy has been updated */
#define VMK_PORTSET_MSG_ID_PORT_L2ADDR             0x10

/** Port has been enabled */
#define VMK_PORTSET_MSG_ID_PORT_ENABLE             0x20

/** Port has been disabled */
#define VMK_PORTSET_MSG_ID_PORT_DISABLE            0x40

/** Portset MTU has been updated */
#define VMK_PORTSET_MSG_ID_PORTSET_MTU_UPDATED     0x80

/** Portset VLAN has been updated */
#define VMK_PORTSET_MSG_ID_PORTSET_VLAN_UPDATED    0x100

/** Custom message posted by vmk_PortsetPostMessage */
#define VMK_PORTSET_MSG_ID_CUSTOM                  0x80000000

/*
 ***********************************************************************
 * vmk_PortsetMessageCB --                                        */ /**
 *
 * \brief Message callback used for portset message notification
 *
 * \note Portset message notifications are asynchronous, meaning that
 *       the message handler should examine the port ID to determine
 *       current state at the time the callback is made.
 *
 * \note No portset handle is held when callback is made. To acquire a
 *       portset, message handler needs to call
 *       vmk_PortsetAcquireByPortID.
 *
 * \note msgData will be NULL for messages with ID different than
 *       VMK_PORTSET_MSG_ID_CUSTOM
 *
 * \param[in]  portID      ID of port the message implicates
 * \param[in]  msgID       ID of message
 * \param[in]  msgData     Data associated with message
 * \param[in]  cbData      Data passed to vmk_PortsetRegisterMessageCB
 *
 * \retval     None
 *
 ***********************************************************************
 */

typedef void (*vmk_PortsetMessageCB)(vmk_SwitchPortID portID,
                                     vmk_PortsetMessageID msgID,
                                     vmk_PortsetMessageData *msgData,
                                     void *cbData);

/*
 ***********************************************************************
 * vmk_PortsetRegisterMessageCB --                                */ /**
 *
 * \brief  Register a handler to receive portset message notifications.
 *
 * \note These are asynchronous message notifications, meaning that the
 *       message handler should examine the port to determine current
 *       state at the time the callback is made.
 *
 * \note If NULL portset is specified, handler will be linked to global
 *       message handler list and it will receive messages from every
 *       portset.
 *
 * \note If NULL portset is specified, caller must not hold any portset
 *       handle. Otherwise, caller must hold at least an immutable
 *       portset handle.
 *
 * \note If portID is set to VMK_VSWITCH_INVALID_PORT_ID, handler will
 *       receive messages related to every port on portset, or messages
 *       related to all ports on ESX if portset is set to NULL.
 *       Otherwise, handler will only receive message related to the
 *       specific port.
 *
 * \param[in]  portset     Pointer to portset where handler will be
 *                         linked
 * \param[in]  portID      ID of port whose message the handler is
 *                         interested in
 * \param[in]  msgMask     Combination of portset message IDs handler
 *                         is interested in
 * \param[in]  cb          Handler to call to notify a portset message
 * \param[in]  cbData      Data to pass to the handler
 * \param[out] handle      Handle to unregister the handler
 *
 * \retval     VMK_OK         Registration succeeded
 * \retval     VMK_BAD_PARAM  Invalid parameter
 * \retval     VMK_FAILURE    Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetRegisterMessageCB(
   vmk_Portset *portset,
   vmk_SwitchPortID portID,
   vmk_uint64 msgMask,
   vmk_PortsetMessageCB cb,
   void *cbData,
   vmk_PortsetMessageCBHandle *handle);

/*
 ***********************************************************************
 * vmk_PortsetUnregisterMessageCB --                              */ /**
 *
 *  \brief  Unregister a handler to receive portset message
 *          notifications.
 *
 *  \note   Caller must not hold any portset handle.
 *
 *  \param[in] handle      Handle returned from registration
 *
 *  \retval    VMK_OK        Unregistering succeeded
 *  \retval    VMK_BAD_PARAM Invalid parameter
 *  \retval    VMK_FAILURE   Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetUnregisterMessageCB(
   vmk_PortsetMessageCBHandle handle);

/*
 ***********************************************************************
 * vmk_PortsetPostMessage --                                      */ /**
 *
 * \brief Send an asynchronous message to portset
 *
 * Send a message to global or portset specific message subscribers.
 * Subscriber message handler will be called after this function
 * returns.
 *
 * \note This function will not block
 *
 * \note If portset is not NULL, the caller must hold at least an
 *       immutable portset handle. Otherwise, caller must not hold
 *       any portset handle.
 *
 * \note To post a global message to all ports on ESX, set portset
 *       pointer to NULL and portID to VMK_VSWITCH_INVALID_PORT_ID. To
 *       post a message to all ports connected to a specific portset,
 *       pass at least an immutable handle of that portset, and set
 *       portID to VMK_VSWITCH_INVALID_PORT_ID.
 *
 * \note data should be allocated/freed by caller.
 *
 * \param[in]   portset        Pointer of portset the message will be
 *                             sent to
 * \param[in]   portID         ID of port this message related to
 * \param[in]   msgID          Portset message ID
 * \param[in]   msgData        Data associated with this message
 *
 * \retval      VMK_OK         Message sent successfully
 * \retval      Other status   Send message failed
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPostMessage(vmk_Portset *portset,
                                        vmk_SwitchPortID portID,
                                        vmk_PortsetMessageID msgID,
                                        vmk_PortsetMessageData *msgData);

#endif /* _VMKAPI_NET_PORTSET_MESSAGE_H_ */
/** @} */
/** @} */
