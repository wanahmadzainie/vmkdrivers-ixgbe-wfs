/******************************************************************************
 * wfsioc.c	    WFS Module. /dev/wfsring device
 *
 *      This module is completely hardware-independent and provides
 *      a char device for user application to communicate with WFS adaptor
 *
 * Author:  Naiyang Hsiao <nhsiao@powerallnetworks.com>
 *
 * Copyright:	2013-2014 (c) Power-All Networks
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *
 *****************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "ixgbe.h"
#include "ixgbe_wfs.h"

#define myID            (iwa->wfs_id)
#define myDev           (iwa->ioc_dev)
#define myDevName       (iwa->name)
#define myDevClass      (iwa->ioc_dev_class)
#define myDevNo         (iwa->ioc_devno)
#define myDevOpenCount  (iwa->ioc_dev_open_count)
#define myDevLock       (iwa->ioc_lock)
#define wfspeer         (iwa->wfspeer)
#define bert_lock       (iwa->bert_lock);
#define wfsbert         (iwa->wfsbert)

/****** Function Prototypes *************************************************/


/*
 * IOC support function
 *
 * These routines are external reference by ioctl and require spinlock
 * of FWS network driver
 */

/* return at most max_count of peers
 */
static int get_wfspeer_list(struct ixgbe_wfs_adapter *iwa, wfsctl_peer_data *plist, int max_count)
{
    struct wfs_peer *peer;
    int id, j;

    for (id=1,j=0; id<=WFSID_MAX && j<max_count; id++) {
        peer = &wfspeer[id-1];
        if (peer->fsm_state == S_init)
            continue;

        plist[j].id = id;
        memcpy(plist[j].mac, peer->mac, 6);
        plist[j].ip = peer->ip;
        plist[j].port_pri = peer->channel_pri ? peer->port_pri : -1;
        plist[j].port_sec = peer->channel_sec ? peer->port_sec : -1;
        plist[j].flag.local = (id == myID ? 1 : 0);
        plist[j].flag.pri = (peer->fsm_state == S_idle) ? 1 : 0;
        plist[j].flag.sec = (peer->fsm_state == S_protect) ? 1 : 0;
#if 0
        if (plist[j].flag.local)
            plist[j].eport = -1;
        else
#endif
            plist[j].eport = (peer->fsm_state == S_idle && peer->channel_pri) ? peer->channel_pri->wfs_port :
                (peer->fsm_state == S_protect && peer->channel_sec) ? peer->channel_sec->wfs_port :
                -1;
        j++;
    }

    return j;
}

#ifdef WFS_FIB
static int get_wfsfib_entry(struct ixgbe_wfs_adapter *iwa, wfsctl_fib_data *fib, int max_entry)
{
    int i, num;
    struct wfs_peer *peer;

    num = ixgbe_wfs_fib_get_entries(iwa, fib, max_entry);
    for (i=0; i<num; i++) {
        peer = &wfspeer[fib[i].id-1];
#if 0
        if (fib[i].id == myID)
            fib[i].eport = -1;
        else
#endif
            fib[i].eport = (peer->fsm_state == S_idle && peer->channel_pri) ? peer->channel_pri->wfs_port :
               (peer->fsm_state == S_protect && peer->channel_sec) ? peer->channel_sec->wfs_port :
               -1;
    }

    return num;
}
#endif

#ifdef WFS_BERT
static void get_bert_stats(struct ixgbe_wfs_adapter *iwa, wfsctl_bert_stats *stats)
{
    struct wfs_bert_cfg *bertcfg;

    if (stats->wfsid == 0)
        stats->wfsid = myID;

    bertcfg = &wfsbert[stats->wfsid-1];

    stats->interval = jiffies_to_msecs(bertcfg->jfs_last - bertcfg->jfs) / 1000;
    stats->tx_pkts = bertcfg->stats.tx_pkts;
    stats->tx_bytes = bertcfg->stats.tx_bytes;
    stats->rx_pkts = bertcfg->stats.rx_pkts;
    stats->rx_bytes = bertcfg->stats.rx_bytes;
    stats->err_csum = bertcfg->stats.err_csum;
    stats->err_drop = bertcfg->stats.err_drop;
    stats->err_seq = bertcfg->stats.err_seq;
    stats->err_size = bertcfg->stats.err_size;
    stats->rtt_min = bertcfg->stats.rtt_min;
    stats->rtt_max = bertcfg->stats.rtt_max;
    stats->rtt_avg = bertcfg->stats.rtt_avg;
}
#endif

/*
 * IOC device prototype
 */
static int ixgbe_cdev_open(struct inode * in, struct file * filp)
{
    struct ixgbe_wfs_adapter *iwa = container_of(in->i_cdev, struct ixgbe_wfs_adapter, ioc_dev);

    log_debug("Enter\n");

    filp->private_data = iwa;

    spin_lock_bh(&myDevLock);
    myDevOpenCount++;
    log_debug("fp=0x%p %d users\n", (void *)filp, myDevOpenCount);
    spin_unlock_bh(&myDevLock);

    return 0;
}

static int ixgbe_cdev_release(struct inode * in, struct file * filp)
{
    struct ixgbe_wfs_adapter *iwa = container_of(in->i_cdev, struct ixgbe_wfs_adapter, ioc_dev);

    log_debug("Enter\n");

    if (!myDevOpenCount) {
        log_err("Device not in use, reset count\n");
        spin_lock_bh(&myDevLock);
        myDevOpenCount = 0;
        spin_unlock_bh(&myDevLock);
        return -EFAULT;
    }

    spin_lock_bh(&myDevLock);
    myDevOpenCount--;
    log_debug("fp=0x%p, %d users\n", (void *)filp, myDevOpenCount);
    spin_unlock_bh(&myDevLock);

    return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int ixgbe_cdev_ioctl(struct inode * in, struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct ixgbe_wfs_adapter *iwa = container_of(in->i_cdev, struct ixgbe_wfs_adapter, ioc_dev);
#else
static long ixgbe_cdev_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    struct ixgbe_wfs_adapter *iwa = (struct ixgbe_wfs_adapter *)filp->private_data;
#endif
    int retval = 0, num;
    unsigned long val;
    wfsctl_data iocd;
    wfsctl_peer_data plist[MAX_PEER_LIST];
#ifdef WFS_FIB
    wfsctl_fib_data fib[FIB_GET_SIZE];
#endif
#ifdef WFS_BERT
    wfsctl_bert_cfg bertcfg;
    wfsctl_bert_stats bertstats;
#endif

    log_debug("Enter, cmd %x\n", cmd);

    /* Check cmd type and value */
    if(_IOC_TYPE(cmd) != WFSCTL_MAGIC) return -ENOTTY;
    if(_IOC_NR(cmd) > WFSCTL_MAX_CMD) return -ENOTTY;

    /* Check read/write and corresponding argument */
    if(_IOC_DIR(cmd) & _IOC_READ)
        if(!access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    if(_IOC_DIR(cmd) & _IOC_WRITE)
        if(!access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd)))
            return -EFAULT;

    /* Looks ok, let us continue */
    spin_lock_bh(&myDevLock);

    /* get control data from user */
    if ((val = copy_from_user(&iocd, (wfsctl_data *)arg, sizeof(wfsctl_data)))) {
        log_err("copy_from_user failed, val=%ld\n", val);
        retval = -EFAULT;
        goto dev_ioctl_done;
    }

    if (cmd == WFSCTL_GET_PEER_LIST) {
        log_debug("IOC process WFSCTL_GET_PEER_LIST\n");

        num = get_wfspeer_list(iwa, plist, MAX_PEER_LIST);
        iocd.len = num * sizeof(wfsctl_peer_data);
        if (num) {
            if ((val = copy_to_user(iocd.v.plist, plist, num * sizeof(wfsctl_peer_data)))) {
                log_err("copy_to_user failed, val=%ld\n", val);
                retval = -EFAULT;
                goto dev_ioctl_done;
            }
        }
        if ((val = copy_to_user((wfsctl_data *)arg, &iocd.len, sizeof(iocd.len)))) {
            log_err("copy_to_user failed, val=%ld\n", val);
            retval = -EFAULT;
        }
    }

    else if (cmd == WFSCTL_GET_PHY_STATS) {
        log_debug("IOC process WFSCTL_GET_PHY_STATS\n");
        // TODO
    }

#ifdef WFS_FIB
    else if (cmd == WFSCTL_GET_FIB) {
        log_debug("IOC process WFSCTL_GET_FIB\n");

        // get start entry no.
        if ((val = copy_from_user(&fib[0], iocd.v.fib, sizeof(wfsctl_fib_data)))) {
            log_err("copy_from_user failed, val=%ld\n", val);
            retval = -EFAULT;
            goto dev_ioctl_done;
        }

        num = get_wfsfib_entry(iwa, fib, FIB_GET_SIZE);
        iocd.len = num * sizeof(wfsctl_fib_data);
        if (num) {
            if ((val = copy_to_user(iocd.v.fib, fib, num * sizeof(wfsctl_fib_data)))) {
                log_err("copy_to_user failed, val=%ld\n", val);
                retval = -EFAULT;
                goto dev_ioctl_done;
            }
        }
        if ((val = copy_to_user((wfsctl_data *)arg, &iocd.len, sizeof(iocd.len)))) {
            log_err("copy_to_user failed, val=%ld\n", val);
            retval = -EFAULT;
        }
    }
#endif

#ifdef WFS_BERT
    else if (cmd == WFSCTL_SET_BERT_CFG) {
        log_debug("IOC process WFSCTL_SET_BERT_CFG\n");

        if ((val = copy_from_user(&bertcfg, iocd.v.bertcfg, sizeof(wfsctl_bert_cfg)))) {
            log_err("copy_from_user failed, val=%ld\n", val);
            retval = -EFAULT;
            goto dev_ioctl_done;
        }

        if (bertcfg.onoff)
            ixgbe_wfs_bert_start_request(iwa, &bertcfg);
        else
            ixgbe_wfs_bert_stop_request(iwa);
    }

    else if (cmd == WFSCTL_GET_BERT_STATS) {
        log_debug("IOC process WFSCTL_GET_BERT_STATS\n");

        // get wfsid
        if ((val = copy_from_user(&bertstats, iocd.v.bertstats, sizeof(wfsctl_bert_stats)))) {
            log_err("copy_from_user failed, val=%ld\n", val);
            retval = -EFAULT;
            goto dev_ioctl_done;
        }

        get_bert_stats(iwa, &bertstats);

        iocd.len = sizeof(bertstats);
        if ((val = copy_to_user(iocd.v.bertstats, &bertstats, sizeof(wfsctl_bert_stats)))) {
            log_err("copy_to_user failed, val=%ld\n", val);
            retval = -EFAULT;
            goto dev_ioctl_done;
        }
        if ((val = copy_to_user((wfsctl_data *)arg, &iocd.len, sizeof(iocd.len)))) {
            log_err("copy_to_user failed, val=%ld\n", val);
            retval = -EFAULT;
        }
    }
#endif
    else {
        log_err("unknown command 0x%x\n", cmd);
        retval = -EFAULT;
    }

dev_ioctl_done:

    spin_unlock_bh(&myDevLock);

    return retval;
}

static ssize_t ixgbe_cdev_read(struct file *filp, __user char *buf, size_t count, loff_t *ppos)
{
#if IXGBE_WFS_DEBUGLEVEL >= 4
    struct ixgbe_wfs_adapter *iwa = (struct ixgbe_wfs_adapter *)filp->private_data;
#endif
    int retval = 0;

    log_debug("Enter\n");

    return retval;
}

static ssize_t ixgbe_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
#if IXGBE_WFS_DEBUGLEVEL >= 4
    struct ixgbe_wfs_adapter *iwa = (struct ixgbe_wfs_adapter *)filp->private_data;
#endif
    int retval = 0;

    log_debug("Enter\n");

    return retval;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = ixgbe_cdev_read,
    .write = ixgbe_cdev_write,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
    .ioctl = ixgbe_cdev_ioctl,
#else
    .unlocked_ioctl = ixgbe_cdev_ioctl,
#endif
    .open = ixgbe_cdev_open,
    .release = ixgbe_cdev_release,
};

/*
 *	Interface functions
 */

/*
 *	Clean up /dev/ringNNN
 */

void ixgbe_wfs_ioc_cleanup(struct ixgbe_wfs_adapter *iwa)
{
    log_debug("Enter\n");

    device_destroy(myDevClass, myDevNo);
    cdev_del(&myDev);
    class_destroy(myDevClass);
    unregister_chrdev_region(MKDEV(MAJOR(myDevNo),0), 1);

    log_info("Workflow control device unregistered\n");
}

/*
 *	Create /dev/ringNNN
 */
int ixgbe_wfs_ioc_init(struct ixgbe_wfs_adapter *iwa)
{
    int err;
    struct device *device;

    log_debug("Enter\n");

    myDevNo = 0;
    myDevOpenCount = 0;
    spin_lock_init(&myDevLock);

    /* allocate a major/minor number. */
    err = alloc_chrdev_region(&myDevNo, 0, 1, myDevName);
    if(err < 0) {
        log_err("Error allocating char device region, err = %d\n", err);
        return -1;
    }

    /* Create device class (before allocation of the array of devices) */
    myDevClass = class_create(THIS_MODULE, myDevName);
    if (IS_ERR(myDevClass)) {
        err = PTR_ERR(myDevClass);
        log_err("Error creating device class, err = %d\n", err);
        return -1;
    }

    /* register and create character device */
    cdev_init(&myDev, &fops);
    myDev.owner = THIS_MODULE;

    err = cdev_add(&myDev, myDevNo, 1);
    if(err < 0) {
        log_err("Error registering device driver, err = %d\n", err);
        unregister_chrdev_region(myDevNo, 1);
        return err;
    }
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
    device = device_create(myDevClass, NULL, myDevNo, myDevName);
#else
    device = device_create(myDevClass, NULL, myDevNo, NULL, myDevName);
#endif
    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        log_err("Error create device node, err = %d\n", err);
        cdev_del(&myDev);
        return err;
    }

    log_info("Workflow control device registered, major %d minor %d\n",
            MAJOR(myDevNo), MINOR(myDevNo));

    return 0;
}

