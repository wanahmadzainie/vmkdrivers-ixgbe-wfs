#ifndef _IXGBE_WFS_H_
#define _IXGBE_WFS_H_

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include "ixgbe.h"
#include "ixgbe_wfs_enc.h"
#include "ixgbe_wfs_raps.h"
#include "ixgbe_wfs_fib.h"
#include "ixgbe_wfsctl.h"
#include "ixgbe_wfs_ioc.h"
#include "ixgbe_wfs_bert.h"

/*
 * feature enable macro
 */
#define WFS_DATASEQ
#define WFS_FIB
#define WFS_IOC
#define WFS_BERT

#define IXGBE_MAX_WFS_NIC   2   /* max WFS NIC allowed */

#define WFS_DEVNAME_FMT     "ring%d"

#define MIN(a,b)    ((a)<(b)) ? (a) : (b)
#define MAX(a,b)    ((a)>(b)) ? (a) : (b)
#define ALIGN_BUFFER(addr,algn)     (((u64)addr+algn)%(algn))
/* log marco */
#ifndef IXGBE_WFS_DEBUG
#define IXGBE_WFS_DEBUGLEVEL 3
#endif

#if IXGBE_WFS_DEBUGLEVEL >= 4
#define log_debug(format, arg...) \
    printk(KERN_INFO "%s: %s: " format, __func__, iwa->name, ## arg)
#else
#define log_debug(format, arg...)
#endif
#if IXGBE_WFS_DEBUGLEVEL >= 3
#define log_info(format, arg...) \
    printk(KERN_INFO "%s: " format, iwa->name, ## arg)
#else
#define log_info( format, arg...)
#endif

#define log_warn(format, arg...) \
    printk(KERN_WARNING "%s: " format, iwa->name, ## arg)
#define log_err(format, arg...) \
    printk(KERN_ERR "%s: " format, iwa->name, ## arg)

#define PRINT_MAC_FMT           "%02x:%02x:%02x:%02x:%02x:%02x"
#define PRINT_MAC_VAL(mac)      mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]

/*
 * adapter bonding
 */
#ifndef PCI_DEVID
#define PCI_DEVID(bus, devfn)  ((((u16)bus) << 8) | devfn)
#endif
#define WFS_DEVID(pdev)     PCI_DEVID(pdev->bus->number,(pdev->devfn & ~0x7))
#define WFS_PORTID(pdev)    (pdev->devfn & 0x7)

#define WFS_DEFAULT_MTU     9000
#define WFS_DEFAULT_TXQLEN  10000

enum ixgbe_wfs_adapter_state {
    unused,
    allocated,
    partial_initialized,
    initialized,
    opened,
    closed,
    suspended
};

struct ixgbe_wfs_adapter {
    u8 index;           /* index */
    enum ixgbe_wfs_adapter_state state;
    char name[IFNAMSIZ];/* name */
    u8 wfs_id;          /* from module option */
    u8 mac_addr[6];     /* from primary adapter perm_addr */
    u32 ip;             /* from net_device */
    u32 dev_id;         /* PCI_DEVID(bus,devfn) & 0x7 */
    struct ixgbe_adapter *primary;    /* first probed adapter */
    struct ixgbe_adapter *secondary;  /* second probed adapter */
    struct net_device *ndev;
    bool link_up;
    spinlock_t xmit_lock;
    u32 xmit_err;
#ifdef WFS_DATASEQ
    u32 data_seqno;
#endif
    /* FSM */
    struct timer_list raps_timer;
    struct raps_timer_ctrl raps_ctrl;
    spinlock_t raps_lock;
    spinlock_t fsm_lock;
    spinlock_t fsmEvBuf_lock;
    wfs_fsm_event_buffer *fsmEvBufs;        /* list of allocated buffers */
    wfs_fsm_event_buffer *fsmEvBuf_head;    /* head of free buffers */
    wfs_fsm_event_buffer *fsmEvBuf_tail;    /* tail of free buffers */
    u32 fsmEvBuf_count;
    struct wfs_peer wfspeer[WFSID_MAX];
#ifdef WFS_FIB
    /* FIB */
    struct rb_root fib_root;        /* rbtree root */
    struct fib_node *fibns;         /* list of allocated nodes */
    struct fib_node *fibn_head;     /* head of free nodes */
    struct fib_node *fibn_tail;     /* tail of free nodes */
    spinlock_t fib_lock;
    struct timer_list fib_timer;
    u8 current_time_tag;
    u32 fib_alloc_size;
    u32 fib_size;
#endif
#ifdef WFS_IOC
    /* IOC */
    spinlock_t ioc_lock;
    struct cdev ioc_dev;
    struct class *ioc_dev_class;
    int ioc_devno;
    int ioc_dev_open_count;
#endif
#ifdef WFS_BERT
    /* BERT */
    spinlock_t bert_lock;
    struct wfs_bert_ctrl bert_ctrl;
    struct workqueue_struct *bert_rq;
    u32 bert_seqno;
    struct wfs_bert_cfg wfsbert[WFSID_MAX];
#endif
};

extern int ixgbe_wfs_init(struct ixgbe_wfs_adapter *iwa);
extern int ixgbe_wfs_init2(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_cleanup(struct ixgbe_wfs_adapter *iwa);
extern struct ixgbe_wfs_adapter *ixgbe_wfs_get_adapter(u32 dev_id);
extern struct sk_buff *ixgbe_wfs_encap(struct ixgbe_adapter *adapter, struct sk_buff *skb);
extern struct sk_buff *ixgbe_wfs_decap(struct ixgbe_adapter *adapter, struct sk_buff *skb);
extern void ixgbe_wfs_send_announce(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_send_raps(struct ixgbe_wfs_adapter *iwa, wfs_fsm_event event);
extern void ixgbe_wfs_send_bert(struct ixgbe_wfs_adapter *iwa, u8 wfsid);
extern struct sk_buff *ixgbe_wfs_get_bert_skb(struct ixgbe_wfs_adapter *iwa, u8 wfsid,
        const char *data, int data_len, u16 csum);
extern u16 ixgbe_wfs_get_bert_csum(struct ixgbe_wfs_adapter *iwa, char *data, int data_len);
extern u16 ixgbe_wfs_get_bert_skb_csum(struct ixgbe_wfs_adapter *iwa, struct sk_buff *skb);
extern int ixgbe_wfs_recv_control(struct ixgbe_adapter *adapter, struct sk_buff *skb);
extern void disp_frag(unsigned char * addr, int len);

#endif /* _IXGBE_WFS_H_ */
