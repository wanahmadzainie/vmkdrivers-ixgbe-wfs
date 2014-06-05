/******************************************************************************
 * wfsfib.h    WFS Module. Forwarding table
 *
 *      This module is completely hardware-independent and provides
 *      MAC-to-WorkstationID forwarding table used in WFS adaptor
 *
 * Author:  Naiyang Hsiao <nhsiao@powerallnetworks.com>
 *
 * Copyright:   2013-2014 (c) Power-All Networks
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *****************************************************************************/

#ifndef _IXGBE_WFS_FIB_H_
#define _IXGBE_WFS_FIB_H_

struct fib_node {
    struct rb_node node;
    struct fib_node *next_alloc;
    struct fib_node *next_free;
    u32 ip;
    u8 mac[6]; /* key */
    u8 wfsid;
    u8 time_tag;
};

struct ixgbe_wfs_adapter;
struct wfsctl_fib_data;

extern int ixgbe_wfs_fib_init(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_fib_cleanup(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_fib_update(struct ixgbe_wfs_adapter *iwa, unsigned char *mac, unsigned int ip, unsigned char wfsid);
extern u8 ixgbe_wfs_fib_lookup(struct ixgbe_wfs_adapter *iwa, unsigned char *mac);
extern void ixgbe_wfs_fib_delete(struct ixgbe_wfs_adapter *iwa, unsigned char *mac);
extern void ixgbe_wfs_fib_delete_wfsid(struct ixgbe_wfs_adapter *iwa, unsigned char wfsid);
extern int ixgbe_wfs_fib_get_entries(struct ixgbe_wfs_adapter *iwa, struct wfsctl_fib_data *fib, int max_entry);

#endif /* !(_IXGBE_WFS_FIB_H_) */
