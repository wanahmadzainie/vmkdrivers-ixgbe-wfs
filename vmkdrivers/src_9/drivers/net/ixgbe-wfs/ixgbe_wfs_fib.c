/******************************************************************************
 * wfsfib.c	WFS Module. Forwarding table
 *
 *		This module is completely hardware-independent and provides
 *		MAC-to-WorkstationID forwarding table used in WFS adaptor
 *
 * Author:  Naiyang Hsiao <nhsiao@powerallnetworks.com>
 *
 * Copyright:	2013-2014 (c) Power-All Networks
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *****************************************************************************/


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rbtree.h>

#include "ixgbe.h"
#include "ixgbe_wfs.h"

#ifdef WFS_FIB

/****** Function Prototypes *************************************************/

/* Methods for preparing data for reading proc entries */

/*
 *	Global Data
 */


#define FIB_SIZE_MAX    4096
#define FIB_SIZE_MIN    256
#define FIB_TIMEOUT     (300*HZ)

#define PRINT_MAC_FMT           "%02x:%02x:%02x:%02x:%02x:%02x"
#define PRINT_MAC_VAL(mac)      mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]

#define fib_root    (iwa->fib_root)     /* rbtree root */
#define fibns       (iwa->fibns)        /* list of allocated nodes */
#define fibn_head   (iwa->fibn_head)    /* head of free nodes */
#define fibn_tail   (iwa->fibn_tail)    /* tail of free nodes */
#define fib_lock    (iwa->fib_lock)
#define fib_timer   (iwa->fib_timer)
#define current_time_tag    (iwa->current_time_tag)
#define fib_alloc_size  (iwa->fib_alloc_size)
#define fib_size        (iwa->fib_size)

/*
 *	Interface functions
 */
static void fibn_free(struct ixgbe_wfs_adapter *iwa, struct fib_node *n)
{
    BUG_ON(n == 0);
    n->next_free = NULL;
    if (fibn_tail) {
        fibn_tail->next_free = n;
        fibn_tail = n;
    } else {
        fibn_head = fibn_tail = n;
    }
}

static struct fib_node *fibn_alloc(struct ixgbe_wfs_adapter *iwa)
{
    struct fib_node *n = NULL;

    if (fibn_head)
    {   /* get free node */
        n = fibn_head;
        fibn_head = n->next_free;
        if (fibn_head == NULL)
            fibn_tail = NULL;
    }
    else if (fib_alloc_size < FIB_SIZE_MAX)
    {   /* no free node, allocate one and add to fib_nodes */
        n = kmalloc(sizeof(struct fib_node), GFP_ATOMIC);
        if (n == NULL) {
            return NULL;
        }
        n->next_alloc = fibns;
        fibns = n;
        fib_alloc_size++;
    }

    return n;
}

static struct fib_node *my_search(struct ixgbe_wfs_adapter *iwa, struct rb_root *root, u8 *mac)
{
    struct rb_node *rbn = root->rb_node;
    struct fib_node *n;
    int result;

    while (rbn) {
        n = container_of(rbn, struct fib_node, node);
        result = memcmp(mac, n->mac, 6);
        if (result < 0)
            rbn = rbn->rb_left;
        else if (result > 0)
            rbn = rbn->rb_right;
        else
            break;
    }

    return rbn ? n : NULL;
}

static struct fib_node *my_insert(struct ixgbe_wfs_adapter *iwa, struct rb_root *root, struct fib_node *fibn)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    struct fib_node *n;
    int result;

    while (*new) {
        n = container_of(*new, struct fib_node, node);
        result = memcmp(fibn->mac, n->mac, 6);
        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else {
            /* update */
            if (n->wfsid != fibn->wfsid) {
                n->time_tag = fibn->time_tag;
                n->wfsid = fibn->wfsid;
            }
            return n;
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&fibn->node, parent, new);
    rb_insert_color(&fibn->node, root);

    return fibn;
}

static void my_trim_tree(struct ixgbe_wfs_adapter *iwa)
{
    struct rb_root root = RB_ROOT;
    struct rb_node *rbn;
    struct fib_node *n;
    int i;

    if (fib_size < FIB_SIZE_MAX)
        goto trim_done;

    fib_size = 0;
    for (rbn=rb_last(&fib_root); rbn; rbn=rb_last(&fib_root))
    {
        n = rb_entry(rbn, struct fib_node, node);
        rb_erase(rbn, &fib_root);
        if (n->time_tag != current_time_tag) {
            fibn_free(iwa,n);
        } else {
            my_insert(iwa, &root, n);
            fib_size++;
        }
    }

    if (fib_size == FIB_SIZE_MAX) {
        /* fib still full, try to trim down a little bit */
        for (i=FIB_SIZE_MAX; i>FIB_SIZE_MIN; i--) {
            rbn = rb_last(&root);
            n = rb_entry(rbn, struct fib_node, node);
            rb_erase(rbn, &root);
            fibn_free(iwa,n);
            fib_size--;
        }
    }

    fib_root = root;

trim_done:
    current_time_tag++;
    fib_timer.expires = jiffies + FIB_TIMEOUT;
    add_timer(&fib_timer);
}

u8 ixgbe_wfs_fib_lookup(struct ixgbe_wfs_adapter *iwa, u8 *mac)
{
    struct fib_node *n;
    u8 wfsid = 0;

    spin_lock_bh(&fib_lock);

    n = my_search(iwa, &fib_root, mac);

    if (n) {
        wfsid = n->wfsid;
        log_debug("fib found mac " PRINT_MAC_FMT " wfsid %d, fib size %d\n",
                PRINT_MAC_VAL(n->mac), wfsid, fib_size);
    } else {
        wfsid = WFSID_ALL;
        log_debug("fib can't find mac " PRINT_MAC_FMT ", set wfsid %d, fib size %d\n",
                PRINT_MAC_VAL(mac), wfsid, fib_size);
    }

    spin_unlock_bh(&fib_lock);

    return wfsid;
}

void ixgbe_wfs_fib_delete(struct ixgbe_wfs_adapter *iwa, u8 *mac)
{
    struct fib_node *n;

    spin_lock_bh(&fib_lock);

    n = my_search(iwa, &fib_root, mac);
    if (n) {
        rb_erase(&n->node, &fib_root);
        fibn_free(iwa,n);
        fib_size--;
        log_debug("fib delete mac " PRINT_MAC_FMT " wfsid %d, fib size %d\n",
                PRINT_MAC_VAL(n->mac), n->wfsid, fib_size);
    }

    spin_unlock_bh(&fib_lock);
}

void ixgbe_wfs_fib_delete_wfsid(struct ixgbe_wfs_adapter *iwa, u8 wfsid)
{
    struct rb_root root = RB_ROOT;
    struct rb_node *rbn;
    struct fib_node *n;

    BUG_ON (wfsid < WFSID_MIN || wfsid > WFSID_MAX);

    spin_lock_bh(&fib_lock);

    fib_size = 0;
    for (rbn=rb_last(&fib_root); rbn; rbn=rb_last(&fib_root))
    {
        n = rb_entry(rbn, struct fib_node, node);
        rb_erase(rbn, &fib_root);
        if (n->wfsid == wfsid) {
            fibn_free(iwa,n);
        } else {
            my_insert(iwa, &root, n);
            fib_size++;
        }
    }
    fib_root = root;

    log_debug("fib delete wfsid %d, fib size %d\n", wfsid, fib_size);

    spin_unlock_bh(&fib_lock);
}

void ixgbe_wfs_fib_update(struct ixgbe_wfs_adapter *iwa, u8 *mac, u32 ip, u8 wfsid)
{
    struct fib_node *n, *fibn;

    BUG_ON (wfsid < WFSID_MIN || wfsid > WFSID_MAX);

    spin_lock_bh(&fib_lock);

    fibn = my_search(iwa, &fib_root, mac);
    if (fibn) {
        memcpy(fibn->mac, mac, 6);
        fibn->ip = ip;
        fibn->wfsid = wfsid;
        fibn->time_tag = current_time_tag;
        log_debug("fib update mac " PRINT_MAC_FMT " wfsid %d, fib size %d\n",
                PRINT_MAC_VAL(fibn->mac), fibn->wfsid, fib_size);
        goto fib_insert_done;
    }

    n = fibn_alloc(iwa);
    if (!n) {
        my_trim_tree(iwa);
        n = fibn_alloc(iwa);
    }

    memcpy(n->mac, mac, 6);
    n->ip = ip;
    n->wfsid = wfsid;
    n->time_tag = current_time_tag;

    fibn = my_insert(iwa, &fib_root, (void *)n);

    if (n != fibn)
        fibn_free(iwa,n);
    else {
        fib_size++;
        log_debug("fib insert mac " PRINT_MAC_FMT " wfsid %d, fib size %d\n",
                PRINT_MAC_VAL(fibn->mac), fibn->wfsid, fib_size);
    }

fib_insert_done:

    spin_unlock_bh(&fib_lock);

}

static void timer_action(unsigned long data)
{
    struct ixgbe_wfs_adapter *iwa = (struct ixgbe_wfs_adapter *)data;
    spin_lock_bh(&fib_lock);
    my_trim_tree(iwa);
    spin_unlock_bh(&fib_lock);
}

int ixgbe_wfs_fib_init(struct ixgbe_wfs_adapter *iwa)
{
    struct fib_node *n;
    int i;

    spin_lock_init(&fib_lock);
    fib_root = RB_ROOT;
    fibns = NULL;
    fibn_head = NULL;
    fibn_tail = NULL;
    current_time_tag = 0;
    fib_alloc_size = 0;
    fib_size = 0;

    for (i=0; i<FIB_SIZE_MIN; i++) {
        n = fibn_alloc(iwa);
        if (!n) {
            log_err("error allocating fib node\n");
            return -ENOMEM;
        }
    }
    log_info("%d fib entries allocated\n", FIB_SIZE_MIN);

    for (n=fibns; n; n=n->next_alloc) {
        fibn_free(iwa,n);
    }

    fib_timer.expires = jiffies + FIB_TIMEOUT;
    fib_timer.data = (unsigned long) iwa;
    fib_timer.function = &timer_action;

    init_timer(&fib_timer);
    add_timer(&fib_timer);

    return 0;
}


void ixgbe_wfs_fib_cleanup(struct ixgbe_wfs_adapter *iwa)
{
    struct fib_node *n, *nextn;
    int i;

    del_timer_sync(&fib_timer);

    spin_lock_bh(&fib_lock);
    for (i=0,n=fibns; n; i++, n=nextn) {
        nextn = n->next_alloc;
        kfree(n);
    }
    spin_unlock_bh(&fib_lock);
    log_info("free %d/%d fib entries\n", i, fib_alloc_size);
}

#ifdef WFS_IOC
int ixgbe_wfs_fib_get_entries(struct ixgbe_wfs_adapter *iwa, struct wfsctl_fib_data *fib, int max_entry)
{
    struct rb_node *rbn;
    struct fib_node *n;
    int i, num, start_entry;

    start_entry = fib[0].no - 1;
    spin_lock_bh(&fib_lock);

    for (i=num=0,rbn=rb_first(&fib_root);
            rbn && num<max_entry; i++,rbn=rb_next(rbn))
    {
        if (i < start_entry)
            continue;
        n = rb_entry(rbn, struct fib_node, node);
        fib[num].no = i+1;
        fib[num].id = n->wfsid;
        memcpy(fib[num].mac, n->mac, 6);
        fib[num].ip = n->ip;
        num++;
    }

    spin_unlock_bh(&fib_lock);

    return num;
}
#endif


#endif // WFS_FIB
