/******************************************************************************
 * wfsctl.h     WFS Module.     /dev/wfsctl ring control device
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
 *****************************************************************************/

#ifndef __WFSCTL_H__
#define __WFSCTL_H__

#include <linux/types.h>
#include <linux/ioctl.h>

/************************** Constant Definitions *****************************/


/** Structure used in IOCTL to set/get driver data */
#define MAX_PEER_LIST    8
#define FIB_GET_SIZE    32

/* Peer List */
typedef struct wfsctl_peer_data {
    struct {
        u_char local;   /* 1 if this local record */
        u_char pri;     /* 1 if primary port is active (IDLE) */
        u_char sec;     /* 1 if secondary port is active (PROTECT) */
    } flag;
    u_char id;          /* workstation id, 1-based */
    u_char mac[6];      /* workstation MAC address */
    u_int ip;           /* workstation IP address */
    char port_pri;      /* primary port number, -1 if unknown */
    char port_sec;      /* secondary port number, -1 if unknown */
    char eport;         /* egress port number, -1 if unknown */
} wfsctl_peer_data  ;

/* Forwarding Table */
typedef struct wfsctl_fib_data {
    u_int no;       /* entry no */
    u_char id;      /* workstation id, 1-based */
    u_int ip;       /* workstation IP address */
    u_char mac[6];  /* workstation MAC address */
    char eport;     /* local egress port, 0-based, -1 if unknown */
} wfsctl_fib_data;

/* Packet Error Rate Test */
#ifndef WFSID_MAX
#define WFSID_MAX   8
#endif

#ifndef WFSID_MIN
#define WFSID_MIN   1
#endif

#define BERT_DATA_SIZE_MAX      (9000)

typedef struct wfsctl_bert_cfg {
    u_char onoff;        /* 0:off, 1:on */
    u_char wfsid;        /* responder wfsid */
    u_char pkt_pattern_flag; /* 0: use pattern. 1:random */
    u_char pkt_pattern;  /* data byte pattern */
    u_int pkt_size;      /* test packet size in byte */
    u_int bandwidth;     /* test bandwidth in bytes/sec */
} wfsctl_bert_cfg;

typedef struct wfsctl_bert_stats {
    u_char wfsid;       /* set 0 for local stats */
    u_int interval;     /* stats collection interval in sec */
    u_long tx_bytes;
    u_long tx_pkts;
    u_long rx_bytes;
    u_long rx_pkts;
    u_long err_csum;
    u_long err_drop;
    u_long err_seq;
    u_long err_size;
    u_long rtt_min;      /* in usec */
    u_long rtt_max;      /* in usec */
    u_long rtt_avg;      /* in usec */
} wfsctl_bert_stats;


/*
 * IOC data
 */
typedef struct wfsctl_data {
    u_int len;    /* data length */
    union {
        wfsctl_peer_data *plist;
        wfsctl_fib_data *fib;
        wfsctl_bert_cfg *bertcfg;
        wfsctl_bert_stats *bertstats;
    } v;
} wfsctl_data;

/*
 * IOCTL commands
 */
/* Selecting magic number for our ioctls */
#define WFSCTL_DEVNAME     "ring"
#define WFSCTL_MAGIC       0x82
#define WFSCTL_MAX_CMD     5

#define WFSCTL_GET_PEER_LIST   _IOWR(WFSCTL_MAGIC, 1, wfsctl_data)
#define WFSCTL_GET_PHY_STATS   _IOWR(WFSCTL_MAGIC, 2, wfsctl_data)
#define WFSCTL_GET_FIB         _IOWR(WFSCTL_MAGIC, 3, wfsctl_data)
#define WFSCTL_SET_BERT_CFG    _IOWR(WFSCTL_MAGIC, 4, wfsctl_data)
#define WFSCTL_GET_BERT_STATS  _IOWR(WFSCTL_MAGIC, 5, wfsctl_data)

#endif /* !(__WFSCTL_H__) */
