#ifndef _IXGBE_WFS_ENC_H_
#define _IXGBE_WFS_ENC_H_

#include <linux/types.h>

#define WFSID_MAX   8
#define WFSID_MIN   1
#define WFSID_BAD   0xFE
#define WFSID_ALL   0xFF

/*
 * packet encapsulation
 */
#define WFSPKT_TYPE_NONE            0x00

#define WFSPKT_TYPE_BROADCAST_MASK  0x80
#define WFSPKT_TYPE_CTRL_MASK       0x40
#define WFSPKT_TYPE_DATA_MASK       0x20

#define WFSPKT_TYPE_CTRL_NONE       (WFSPKT_TYPE_CTRL_MASK)
#define WFSPKT_TYPE_CTRL_ANNOUNCE   (WFSPKT_TYPE_BROADCAST_MASK + WFSPKT_TYPE_CTRL_MASK + 0x01)
#define WFSPKT_TYPE_CTRL_RAPS       (WFSPKT_TYPE_BROADCAST_MASK + WFSPKT_TYPE_CTRL_MASK + 0x02)
#define WFSPKT_TYPE_CTRL_BERT       (WFSPKT_TYPE_CTRL_MASK + 0x03)

#define WFSPKT_TYPE_DATA_BROADCAST  (WFSPKT_TYPE_BROADCAST_MASK + WFSPKT_TYPE_DATA_MASK)
#define WFSPKT_TYPE_DATA_UNICAST    (WFSPKT_TYPE_DATA_MASK)

#define WFSPKT_ETHERTYPE        0x7083  /* Xilinx PCI Device ID */

#define WFSPKT_MIN_NUM          1999    /* same as DMA_BD_CNT */
#define WFSPKT_MAX_NUM          3000
#define WFSPKT_MAX_SIZE         64
#define WFSPKT_HDR_SIZE         sizeof(struct wfspkt)
#define WFSOPT_HDR_SIZE         sizeof(struct wfsopt_hdr)

#define WFSOPT_END              0
#define WFSOPT_SEQUENCE         1
#define WFSOPT_ANNOUNCE         2
#define WFSOPT_RAPS             3
#define WFSOPT_BERT             4

#define WFSOPT_RAPS_REQ_NONE    0
#define WFSOPT_RAPS_REQ_NR      1
#define WFSOPT_RAPS_REQ_SF      2

#define WFSOPT_BERT_NONE        0
#define WFSOPT_BERT_RESET       1
#define WFSOPT_BERT_REQUEST     2
#define WFSOPT_BERT_RESPONSE    3

/* option header */
struct __attribute__ ((__packed__)) wfsopt_hdr {
    u8 type;    /* type */
    u8 len;     /* length */
};

/* data sequence option */
struct __attribute__ ((__packed__)) wfsopt_sequence {
    u32 no;
};

/* announce packet option */
struct __attribute__ ((__packed__)) wfsopt_announce {
    u8 mac[6];
    u32 ip;
    u8 port_pri:1;   /* port select - primary port */
    u8 port_sec:1;   /* port select - secondary port */
    u8 port:6;       /* port number */
};

/* ring automatic protection switch option */
struct __attribute__ ((__packed__)) wfsopt_raps {
    u8 request;
    struct {
        u8 flush:1; /* Flush FIB */
        u8 dnf:1;   /* Do Not Forward */
        u8 zero:6;
    } status;
};

/* bit error rate test */
struct __attribute__ ((__packed__)) wfsopt_bert {
    u8 type;            /* BERT type */
    u32 seqno;          /* seqno */
    u64 ts;             /* timestamp (in usec) marked by sender when tx */
    u16 data_len;       /* data length */
    u16 data_csum;      /* checksum calculated by sender/responder */
    u8 data[0];         /* data */
};

/*
 * WFS packet option
 */
struct __attribute__ ((__packed__)) wfsopt {
    u8 type;    /* type */
    u8 len;     /* length */
    union {     /* value */
        struct wfsopt_announce announce;
        struct wfsopt_sequence sequence;
        struct wfsopt_raps raps;
        struct wfsopt_bert bert;
    } val;
};


/*
 * workflow packet need to be on 32-byte aligned address
 */
struct __attribute__ ((__packed__)) wfspkt {
    struct ethhdr ethhdr; /* this is for MAC interface only */
    u8 type;        /* packet type */
    u8 dest;        /* packet receiver workflow ID */
    u8 src;         /* packet sender workflow ID */
    u8 len;         /* header len (header + options) */
    u8 opts[0];     /* options */
};

#endif /* _IXGBE_WFS_ENC_H_ */
