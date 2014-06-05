/******************************************************************************
 * wfsioc.h 	WFS Module. /dev/wfsring device
 *
 *      This module is completely hardware-independent and provides
 *      a char device for user application to communicate with WFS adaptor
 *
 * Author:  Naiyang Hsiao <nhsiao@powerallnetworks.com>
 *
 * Copyright:   2013-2014 (c) Power-All Networks
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 *****************************************************************************/

#ifndef __IXGBE_WFSIOC_H__
#define __IXGBE_WFSIOC_H__


struct ixgbe_wfs_adapter;

extern int ixgbe_wfs_ioc_init(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_ioc_cleanup(struct ixgbe_wfs_adapter *iwa);

#endif /* !(__IXGBE_WFSIOC_H__) */
