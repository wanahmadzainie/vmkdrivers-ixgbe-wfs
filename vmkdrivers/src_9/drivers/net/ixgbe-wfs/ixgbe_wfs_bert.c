/******************************************************************************
 * wfsbert.c	WFS Module. Bit Error Rate Test
 *
 *		This module is completely hardware-independent and provides
 *		packet error rate test routines
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/crc32.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include <asm/checksum.h>


#include "ixgbe.h"
#include "ixgbe_wfs.h"

#ifdef WFS_BERT

#define ALIGNMENT_RECV          32
#ifdef X86_64
#define BUFFER_ALIGNRECV(adr) ((ALIGNMENT_RECV - ((u64) adr)) % ALIGNMENT_RECV)
#else
#define BUFFER_ALIGNRECV(adr) ((ALIGNMENT_RECV - ((u32) adr)) % ALIGNMENT_RECV)
#endif

/****** Function Prototypes *************************************************/

/* Methods for preparing data for reading proc entries */

/*
 *	Global Data
 */
#define     BERT_TX_INT     1  // msec
#define     BERT_RX_INT     2  // msec


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#define INIT_BERT_WORK(_t, _f)  INIT_WORK((struct work_struct *)(_t), (void (*)(void *))(_f), (ulong)(_t));
#else
#define INIT_BERT_WORK(_t, _f)  INIT_WORK((struct work_struct *)(_t), (_f));
#endif

#define myID			(iwa->wfs_id)
#define myRequest		(iwa->bert_ctrl)
#define request_queue	(iwa->bert_rq)
#define bert_lock		(iwa->bert_lock)
#define wfsbert			(iwa->wfsbert)
#define myBertSeqNo		(iwa->bert_seqno)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28)
struct timeval ns_to_timeval(const s64 nsec)
{
        struct timespec ts = ns_to_timespec(nsec);
        struct timeval tv;

        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = (suseconds_t) ts.tv_nsec / 1000;

        return tv;
}
EXPORT_SYMBOL(ns_to_timeval);
#endif

/*
 *  Interface functions
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static void bert_request_main(ulong data)
{
    struct wfs_bert_ctrl *wk = (struct wfs_bert_ctrl *)data;
#else
static void bert_request_main(struct work_struct *work)
{
    struct wfs_bert_ctrl *wk = (struct wfs_bert_ctrl *)work;
#endif
    struct ixgbe_wfs_adapter *iwa = container_of(wk, struct ixgbe_wfs_adapter, bert_ctrl);
    struct wfs_bert_cfg *bertcfg = &wfsbert[myID-1];
    int i, j, loop_per_sec, pkt_per_loop, count;
    long jfs, jfs_sec;

    /* thread running */
    wk->running = 1;
    log_info("BERT workqueue running\n");
    msleep(jiffies_to_msecs(HZ));

    /* calculate burst */
    loop_per_sec = HZ / (BERT_TX_INT+BERT_RX_INT);
    pkt_per_loop = MAX(wk->burst/loop_per_sec, 1); // at least one

    /* sending */
    while (wk->on) {
        jfs = jiffies;
        jfs_sec = jiffies + HZ;
        count = 0;
        for (i=0; i<loop_per_sec && count<wk->burst; i++) {
            for (j=0; j<pkt_per_loop && count<wk->burst; j++,count++) {
                ixgbe_wfs_send_bert(iwa, wk->responder);
            }
            msleep(BERT_RX_INT);
        }
        if (jiffies < jfs_sec)
            msleep(jiffies_to_msecs(jfs_sec-jiffies));
        log_debug("%u BERT pkts sent in %u ms\n", count, jiffies_to_msecs(jiffies-jfs));
    }

    /* exit */
    wk->running = 0;

    /* clean up BERT configuration */
    bertcfg->on = 0;
    bertcfg->data_len = 0;
    bertcfg->data_csum = 0;
    if (bertcfg->skb)
        dev_kfree_skb_any(bertcfg->skb);
    bertcfg->skb = NULL;

    log_info("BERT workqueue exited\n");
    return;
}


/*
 * Supported Functions
 */
int ixgbe_wfs_bert_start_request(struct ixgbe_wfs_adapter *iwa, wfsctl_bert_cfg *cfg)
{
    struct wfs_bert_cfg *bertcfg = &wfsbert[myID-1];
    unsigned char *ka, *buff;
    int size;

    spin_lock(&bert_lock);

    if (myRequest.on) {
        spin_unlock(&bert_lock);
        return 1;
    }

    /* set my BERT configuration */
    size = ALIGN(cfg->pkt_size,8); /* align 64-bit */
    if ((buff = kmalloc(size, GFP_ATOMIC)) == NULL) {
        log_err("allocate BERT csum buffer failed.\n");
        spin_unlock(&bert_lock);
        return -1;
    }
    ka = buff + size - cfg->pkt_size;
    if (cfg->pkt_pattern_flag == 0)
        memset(ka, cfg->pkt_pattern, cfg->pkt_size);
    else
        get_random_bytes(ka, cfg->pkt_size);
    bertcfg->data_len = cfg->pkt_size;
    bertcfg->data_csum = ixgbe_wfs_get_bert_csum(iwa, ka, bertcfg->data_len);
    bertcfg->skb = ixgbe_wfs_get_bert_skb(iwa, cfg->wfsid, ka, bertcfg->data_len, bertcfg->data_csum);
    if (!bertcfg->skb) {
        log_err("error allocating BERT skb\n");
        spin_unlock(&bert_lock);
        return -1;
    }
    bertcfg->seqno = 0;
    bertcfg->jfs = bertcfg->jfs_last = 0;
    memset(&bertcfg->stats, 0, sizeof(struct wfs_bert_stats));
    bertcfg->on = 1;
    kfree(buff);

    /* BERT wq control */
    myRequest.responder = cfg->wfsid;
    myRequest.burst = MAX(cfg->bandwidth/cfg->pkt_size, 1); // at least one
    myBertSeqNo = 0;
    myRequest.on = 1;

    if (cfg->pkt_pattern_flag == 0) {
        log_info("BERT set data len %d pattern %02X burst %ld/s csum %04x\n",
            bertcfg->data_len, cfg->pkt_pattern, myRequest.burst, bertcfg->data_csum);
    } else {
        log_info("BERT set data len %d random pattern burst %ld/s csum %04x\n",
            bertcfg->data_len, myRequest.burst, bertcfg->data_csum);
    }

    spin_unlock(&bert_lock);

    /* start BERT wq */
    INIT_BERT_WORK(&myRequest, bert_request_main);
    queue_work(request_queue, (struct work_struct *)&myRequest);

    return 0;
}

void ixgbe_wfs_bert_stop_request(struct ixgbe_wfs_adapter *iwa)
{
    spin_lock(&bert_lock);

    if (myRequest.on)
        myRequest.on = 0;

    spin_unlock(&bert_lock);
}

int ixgbe_wfs_bert_init(struct ixgbe_wfs_adapter *iwa)
{
    spin_lock_init(&bert_lock);
    request_queue = create_workqueue("ixgbe_wfs_bert");
    if (!request_queue) {
        log_err("BERT request workqueue create failed");
        return -1;
    }

    /* request is initially off */
    memset(&myRequest, 0, sizeof(struct wfs_bert_ctrl));
    log_info("BERT workqueue created\n");
    return 0;
}

void ixgbe_wfs_bert_cleanup(struct ixgbe_wfs_adapter *iwa)
{
    ixgbe_wfs_bert_stop_request(iwa);
    flush_workqueue(request_queue);
    destroy_workqueue(request_queue);
    log_info("BERT workqueue destroyed\n");
}


#endif // WFS_BERT



