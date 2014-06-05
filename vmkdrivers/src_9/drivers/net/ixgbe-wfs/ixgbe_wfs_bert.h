/******************************************************************************
 * wfsbert.h	WFS Module. Bit Error Rate Test
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

#ifndef __WFS_BERT_INC__
#define __WFS_BERT_INC__

struct sk_buff;

struct wfs_bert_ctrl {
    struct work_struct wk;  /* work */
    /* BERT control */
    volatile int on;        /* switch on/off */
    volatile int running;   /* running state */
    u8 responder;           /* wfsid of BERT responder */
    ulong burst;            /* packet per second */
};

struct wfs_bert_stats {
    u_long tx_bytes;    /* total tx bytes */
    u_long tx_pkts;     /* total tx pkts */
    u_long rx_bytes;    /* total rx bytes */
    u_long rx_pkts;     /* total rx pkts */
    u_long err_drop;    /* drop pkts count */
    u_long err_seq;     /* out-of-sequence pkts count */
    u_long err_csum;    /* checksum error count */
    u_long err_size;    /* error size pkts count */
    u_long rtt_max;     /* maximal round trip time in usec */
    u_long rtt_min;     /* minimal round trip time in usec */
    u_long rtt_avg;     /* average round trip time in usec */
};

struct wfs_bert_cfg {
    volatile int on;    /* switch on/off */
    u_long jfs;         /* local jiffies at reset */
    u_long jfs_last;    /* local jiffies at last send/receive */
    u32 seqno;          /* local:seqno of next response, remote:seqno of next request */
    struct sk_buff *skb;/* BERT skb (for local only) */
    u16 data_len;       /* BERT data length */
    u16 data_csum;      /* BERT data checksum */
    struct wfs_bert_stats stats;    /* test stats */
};

struct ixgbe_wfs_adapter;

extern int ixgbe_wfs_bert_init(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_bert_cleanup(struct ixgbe_wfs_adapter *iwa);
extern int ixgbe_wfs_bert_start_request(struct ixgbe_wfs_adapter *iwa, struct wfsctl_bert_cfg *cfg);
extern void ixgbe_wfs_bert_stop_request(struct ixgbe_wfs_adapter *iwa);
//extern void wfs_bert_get_stats(struct ixgbe_wfs_adapter *iwa, struct wfsctl_bert_stats *stats);

#endif /* !(__WFS_BERT_INC__) */
