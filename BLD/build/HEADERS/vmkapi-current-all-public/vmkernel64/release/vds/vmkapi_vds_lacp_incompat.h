/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * LACP                                                           */ /**
 * \addtogroup VDS
 * @{
 *
 * \defgroup LACP LACP support in VDS
 *
 * Definition of LACP related VDS properties
 *
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_LACP_INCOMPAT_H_
#define _VMKAPI_VDS_LACP_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief VDS property to configure LACP
 *
 * To enable LACP on VDS, pass vmk_LACPConfig to this property and set
 * field enabled to VMK_TRUE. Set enabled to VMK_FALSE to disable LACP
 * on VDS.
 */
#define VMK_ESPROP_PORT_LACP              "com.vmware.etherswitch.port.lacp"


/**
 * \brief LACP modes
 */
typedef enum vmk_LACPMode {
   /** LACP mode is invalid */
   VMK_LACP_MODE_NONE = 0,

   /** LACP always sends frames along the configured uplinks */
   VMK_LACP_MODE_ACTIVE,

   /** LACP acts as "speak when spoken to" */
   VMK_LACP_MODE_PASSIVE,
} vmk_LACPMode;

/**
 * \brief LACP configuration
 * Parameters passed while enabling LACP on VDS
 */
typedef struct vmk_LACPConfig {
   /** LACP is enabled or not on vds */
   vmk_Bool      enabled;

   /** mode LACP is enabled in */
   vmk_LACPMode  mode;
} vmk_LACPConfig;

#endif /* _VMKAPI_VDS_LACP_INCOMPAT_H_ */
/** @} */
/** @} */
