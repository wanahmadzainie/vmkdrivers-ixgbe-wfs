
/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Portset Incompatible Messages                                  */ /**
 * \addtogroup PortsetMessage
 * @{
 *
 * \defgroup PortsetMsgIncompat Portset Incompatible Messages
 *
 * Definition of incompatible portset messages
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PORTSET_MESSAGE_INCOMPAT_H_
#define _VMKAPI_NET_PORTSET_MESSAGE_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "vds/vmkapi_vds_lacp_incompat.h"

/** Enable LACP on a portset */
#define VMK_PORTSET_MSG_LACP_ENABLE        "portset.lacp.enable"

/** Disable LACP on a portset */
#define VMK_PORTSET_MSG_LACP_DISABLE       "portset.lacp.disable"

/** Change uplink state to LACP port up in a LAG */
#define VMK_PORTSET_MSG_LACP_PORT_UP       "portset.lacp.portup"

/** Change uplink state to LACP port down in a LAG */
#define VMK_PORTSET_MSG_LACP_PORT_DOWN     "portset.lacp.portdown"

/** LACP message */
struct vmk_LACPMessage;

/*
 ***********************************************************************
 * vmk_LACPMessageCompletionCB --                                 */ /**
 *
 * \brief Handler to call when ESX completes processing a LACP message
 *
 * LACP message can be posted to vmkernel via vmk_PortsetPostMessage,
 * which will return to the caller right away. LACP message will be
 * processed later. Once vmkernel finishes processing, it will call
 * this handler specified in LACP message. Then caller can proceed
 * message processing, like checking error code or cleaning up memory.
 *
 * \note No portset handle is held when handler is called.
 *
 * \param[in]   msg      LACP message being completed, allocated by
 *                       vmkernel. Handler should not free its memory.
 * \param[in]   status   LACP message processing return status.
 *
 * \retval      None
 ***********************************************************************
 */
typedef void (*vmk_LACPMessageCompletionCB)(struct vmk_LACPMessage *msg,
                                            VMK_ReturnStatus status);

/**
 * \brief Cause identifier of LACP port up/down message
 */
typedef enum vmk_LACPCauseID {
   /** None */
   VMK_LACP_CAUSE_NONE = 0,

   /** LACP message caused by uplink speed change */
   VMK_LACP_CAUSE_SPEED_CHANGE,

   /** LACP message caused by uplink duplex change */
   VMK_LACP_CAUSE_DUPLEX_CHANGE,

   /** LACP message caused by uplink physical link up */
   VMK_LACP_CAUSE_LINK_UP,

   /** LACP message caused by uplink physical link down */
   VMK_LACP_CAUSE_LINK_DOWN,

   /** LACP message caused by administrator blocking uplink */
   VMK_LACP_CAUSE_BLOCKED,

   /** LACP message caused by administrator unblocking uplink */
   VMK_LACP_CAUSE_UNBLOCKED,

   /** LACP message caused by no response from peer */
   VMK_LACP_CAUSE_PEER_NO_RESPONSE,

   /** LACP message caused by uplink connected to portset */
   VMK_LACP_CAUSE_PORT_CONNECTED,

   /** LACP message caused by uplink disconnected from portset */
   VMK_LACP_CAUSE_PORT_DISCONNECTED,
} vmk_LACPCauseID;

/**
 * \brief Data associated with LACP message
 */
typedef struct vmk_LACPMessage {
   /** size of this structure, must be sizeof(vmk_LACPMessage) */
   vmk_uint32 size;

   /** Name of portset message will be sent to */
   vmk_Name portsetName;

   /** LAG ID, for future use, must set to 0 */
   vmk_uint32 lagID;

   /**
    * The mode LACP will run in. It must be either VMK_LACP_MODE_ACTIVE
    * or VMK_LACP_MODE_PASSIVE in LACP enable message. It must be
    * VMK_LACP_MODE_DISABLE in LACP disable message. It is ignored in
    * other LACP messages
    */
   vmk_LACPMode mode;

   /**
    * Name of uplink where the message regarding to. It can't be
    * empty for message VMK_PORTSET_MSG_LACP_PORT_UP and
    * VMK_PORTSET_MSG_LACP_PORT_DOWN. It is ignored in message
    * VMK_PORTSET_MSG_LACP_ENABLE and VMK_PORTSET_MSG_LACP_DISABLE.
    */
   vmk_Name uplinkName;

   /**
    * Cause identifier of this message, it must be VMK_LACP_CAUSE_NONE
    * for message VMK_PORTSET_MSG_LACP_ENABLE and
    * VMK_PORTSET_MSG_LACP_DISABLE. It can be any vmk_LACPCauseID
    * except VMK_LACP_CAUSE_NONE for other messages
    */
   vmk_LACPCauseID causeID;

   /**
    * LACP message completion callback handler, if not NULL,  it will be
    * called once vmkernel finishes processing this message
    */
   vmk_LACPMessageCompletionCB completionCB;

   /** custom completion data associated with this message */
   void *completionCBData;
} vmk_LACPMessage;

#endif /* _VMKAPI_NET_PORTSET_MESSAGE_INCOMPAT_H_ */
/** @} */
/** @} */
