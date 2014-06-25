/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright (c) 1999 - 2014 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_H_
#define _IXGBE_H_

#include <net/ip.h>

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#ifdef __VMKLNX__
#define NODE_ADDRESS_SIZE ETH_ALEN
#endif /* __VMKLNX__ */

#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif
#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
#include <linux/if_vlan.h>
#endif
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_DCA
#include <linux/dca.h>
#endif
#include "ixgbe_dcb.h"

#include "kcompat.h"
#ifdef __VMKLNX__
#include "kcompat_esx.h"
#endif /* __VMKLNX__ */

#ifdef CONFIG_NET_RX_BUSY_POLL
#include <net/busy_poll.h>
#define BP_EXTENDED_STATS
#endif

#ifdef HAVE_SCTP
#include <linux/sctp.h>
#endif

#ifdef HAVE_INCLUDE_LINUX_MDIO_H
#include <linux/mdio.h>
#endif

#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#define IXGBE_FCOE
#include "ixgbe_fcoe.h"
#endif /* CONFIG_FCOE or CONFIG_FCOE_MODULE */

#include "ixgbe_api.h"

#include "ixgbe_common.h"

#ifdef IXGBE_WFS
#include "ixgbe_wfs.h"
#define DPRINTK(nlevel, klevel, fmt, args...) \
	printk(KERN_INFO "%s: %s: " fmt, adapter->wfs_parent->name, __func__, ## args)
#else
#define PFX "ixgbe: "
#define DPRINTK(nlevel, klevel, fmt, args...) \
	((void)((NETIF_MSG_##nlevel & adapter->msg_enable) && \
	printk(KERN_##klevel PFX "%s: %s: " fmt, adapter->netdev->name, \
		__func__ , ## args)))
#endif /* IXGBE_WFS */
#ifdef __VMKLNX__

#define NETIF_MSG_VIRT			0x8000
#endif /* __VMKLNX__ */


/* TX/RX descriptor defines */
#ifdef __VMKLNX__
#define IXGBE_DEFAULT_TXD                  1024
#define IXGBE_DEFAULT_TX_WORK              1024
#else
#define IXGBE_DEFAULT_TXD		512
#define IXGBE_DEFAULT_TX_WORK		256
#endif /* __VMKLNX__ */
#ifdef IXGBE_WFS
#define IXGBE_MAX_TXD			8192
#define IXGBE_MIN_TXD			64

#define IXGBE_DEFAULT_RXD		1024
#define IXGBE_DEFAULT_RX_WORK		512
#define IXGBE_MAX_RXD			8192
#define IXGBE_MIN_RXD			64
#else
#define IXGBE_MAX_TXD			4096
#define IXGBE_MIN_TXD			64

#define IXGBE_DEFAULT_RXD		512
#define IXGBE_DEFAULT_RX_WORK		256
#define IXGBE_MAX_RXD			4096
#define IXGBE_MIN_RXD			64
#endif /* IXGBE_WFS */

#ifdef __VMKLNX__
#define IXGBE_JUMBO_FRAME_DEFAULT_RXD	512
#define IXGBE_ESX_RSS_QUEUES		4
#define IXGBE_ESX_HW_QUEUES_PER_POOL	4

/* Length in number of words. 1 word = 4 bytes */
#define IXGBE_ESX_RSS_SEED_LEN 10

/* This flag replaces VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS which is
 * a VMWare flag to indicate RSS feature.
 */
#define IXGBE_ESX_FEAT_RSS		0x4
#define IXGBE_ESX_FILTERS_PER_Q		32

#endif /* __VMKLNX__ */

/* flow control */
#define IXGBE_MIN_FCRTL			0x40
#define IXGBE_MAX_FCRTL			0x7FF80
#define IXGBE_MIN_FCRTH			0x600
#define IXGBE_MAX_FCRTH			0x7FFF0
#define IXGBE_DEFAULT_FCPAUSE		0xFFFF
#define IXGBE_MIN_FCPAUSE		0
#define IXGBE_MAX_FCPAUSE		0xFFFF

/* Supported Rx Buffer Sizes */
#define IXGBE_RXBUFFER_256       256  /* Used for skb receive header */
#ifdef IXGBE_WFS
#define IXGBE_RXBUFFER_512  512
#define IXGBE_RXBUFFER_1K   1024
#endif /* IXGBE_WFS */
#define IXGBE_RXBUFFER_2K   2048
#define IXGBE_RXBUFFER_3K	3072
#define IXGBE_RXBUFFER_4K	4096
#define IXGBE_RXBUFFER_1536	1536
#define IXGBE_RXBUFFER_7K	7168
#define IXGBE_RXBUFFER_8K	8192
#define IXGBE_RXBUFFER_15K	15360
#define IXGBE_MAX_RXBUFFER	16384  /* largest size for single descriptor */

/*
 * NOTE: netdev_alloc_skb reserves up to 64 bytes, NET_IP_ALIGN means we
 * reserve 64 more, and skb_shared_info adds an additional 320 bytes more,
 * this adds up to 448 bytes of extra data.
 *
 * Since netdev_alloc_skb now allocates a page fragment we can use a value
 * of 256 and the resultant skb will have a truesize of 960 or less.
 */
#ifdef IXGBE_WFS
#define IXGBE_RX_HDR_SIZE   IXGBE_RXBUFFER_1K
#else
#define IXGBE_RX_HDR_SIZE	IXGBE_RXBUFFER_256
#endif /* IXGBE_WFS */

#define MAXIMUM_ETHERNET_VLAN_SIZE	(VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)

/* How many Rx Buffers do we bundle into one write to the hardware ? */
#define IXGBE_RX_BUFFER_WRITE	16	/* Must be power of 2 */

enum ixgbe_tx_flags {
	/* cmd_type flags */
	IXGBE_TX_FLAGS_HW_VLAN	= 0x01,
	IXGBE_TX_FLAGS_TSO	= 0x02,
	IXGBE_TX_FLAGS_TSTAMP	= 0x04,

	/* olinfo flags */
	IXGBE_TX_FLAGS_CC	= 0x08,
	IXGBE_TX_FLAGS_IPV4	= 0x10,
	IXGBE_TX_FLAGS_CSUM	= 0x20,

	/* software defined flags */
	IXGBE_TX_FLAGS_SW_VLAN	= 0x40,
	IXGBE_TX_FLAGS_FCOE	= 0x80,
};

/* VLAN info */
#define IXGBE_TX_FLAGS_VLAN_MASK	0xffff0000
#define IXGBE_TX_FLAGS_VLAN_PRIO_MASK	0xe0000000
#define IXGBE_TX_FLAGS_VLAN_PRIO_SHIFT	29
#define IXGBE_TX_FLAGS_VLAN_SHIFT	16

#define IXGBE_MAX_RX_DESC_POLL		10

#define IXGBE_MAX_VF_MC_ENTRIES		30
#define IXGBE_MAX_VF_FUNCTIONS		64
#define IXGBE_MAX_VFTA_ENTRIES		128
#define MAX_EMULATION_MAC_ADDRS		16
#define IXGBE_MAX_PF_MACVLANS		15
#define IXGBE_82599_VF_DEVICE_ID	0x10ED
#define IXGBE_X540_VF_DEVICE_ID		0x1515

#define VMDQ_P(p)	((p) + adapter->ring_feature[RING_F_VMDQ].offset)

#define UPDATE_VF_COUNTER_32bit(reg, last_counter, counter)	\
	{							\
		u32 current_counter = IXGBE_READ_REG(hw, reg);	\
		if (current_counter < last_counter)		\
			counter += 0x100000000LL;		\
		last_counter = current_counter;			\
		counter &= 0xFFFFFFFF00000000LL;		\
		counter |= current_counter;			\
	}

#define UPDATE_VF_COUNTER_36bit(reg_lsb, reg_msb, last_counter, counter) \
	{								 \
		u64 current_counter_lsb = IXGBE_READ_REG(hw, reg_lsb);	 \
		u64 current_counter_msb = IXGBE_READ_REG(hw, reg_msb);	 \
		u64 current_counter = (current_counter_msb << 32) |	 \
			current_counter_lsb;				 \
		if (current_counter < last_counter)			 \
			counter += 0x1000000000LL;			 \
		last_counter = current_counter;				 \
		counter &= 0xFFFFFFF000000000LL;			 \
		counter |= current_counter;				 \
	}

struct vf_stats {
	u64 gprc;
	u64 gorc;
	u64 gptc;
	u64 gotc;
	u64 mprc;
};
struct vf_data_storage {
	unsigned char vf_mac_addresses[ETH_ALEN];
	u16 vf_mc_hashes[IXGBE_MAX_VF_MC_ENTRIES];
	u16 num_vf_mc_hashes;
	u16 default_vf_vlan_id;
	u16 vlans_enabled;
	bool clear_to_send;
#ifdef __VMKLNX__
	bool allocated;
	bool init;
	bool pf_set_vlan; /* When true, guest VLAN config not allowed. */
	bool pf_set_mac;
	u32 buffer_mode;
	u32 coml_method;
	u32 mtu;
	u32 irq_rate;
	u16 num_queue_pairs;
	int rar;
#endif /* __VMKLNX__ */
	struct vf_stats vfstats;
	struct vf_stats last_vfstats;
	struct vf_stats saved_rst_vfstats;
#ifndef __VMKLNX__
	bool pf_set_mac;
#endif /* __VMKLNX__ */
	u16 pf_vlan; /* When set, guest VLAN config not allowed. */
	u16 pf_qos;
	u16 tx_rate;
	u16 vlan_count;
	u8 spoofchk_enabled;
	unsigned int vf_api;
};

struct vf_macvlans {
	struct list_head l;
	int vf;
	bool free;
	bool is_macvlan;
	u8 vf_macvlan[ETH_ALEN];
};

#define IXGBE_MAX_TXD_PWR	14
#define IXGBE_MAX_DATA_PER_TXD	(1 << IXGBE_MAX_TXD_PWR)

/* Tx Descriptors needed, worst case */
#define TXD_USE_COUNT(S)	DIV_ROUND_UP((S), IXGBE_MAX_DATA_PER_TXD)
#ifndef MAX_SKB_FRAGS
#define DESC_NEEDED	4
#elif (MAX_SKB_FRAGS < 16)
#define DESC_NEEDED	((MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE)) + 4)
#else
#define DESC_NEEDED	(MAX_SKB_FRAGS + 4)
#endif

/* wrapper around a pointer to a socket buffer,
 * so a DMA handle can be stored along with the buffer */
struct ixgbe_tx_buffer {
	union ixgbe_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	struct sk_buff *skb;
	unsigned int bytecount;
	unsigned short gso_segs;
	__be16 protocol;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};

struct ixgbe_rx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
};

struct ixgbe_queue_stats {
	u64 packets;
	u64 bytes;
#ifdef BP_EXTENDED_STATS
	u64 yields;
	u64 misses;
	u64 cleaned;
#endif  /* BP_EXTENDED_STATS */
};

struct ixgbe_tx_queue_stats {
	u64 restart_queue;
	u64 tx_busy;
	u64 tx_done_old;
};

struct ixgbe_rx_queue_stats {
	u64 rsc_count;
	u64 rsc_flush;
	u64 non_eop_descs;
	u64 alloc_rx_page_failed;
	u64 alloc_rx_buff_failed;
	u64 csum_err;
	u64 rx_hdr_split;
};

enum ixgbe_ring_state_t {
	__IXGBE_TX_FDIR_INIT_DONE,
	__IXGBE_TX_XPS_INIT_DONE,
	__IXGBE_TX_DETECT_HANG,
	__IXGBE_HANG_CHECK_ARMED,
	__IXGBE_RX_RSC_ENABLED,
	__IXGBE_RX_CSUM_UDP_ZERO_ERR,
#ifdef IXGBE_FCOE
	__IXGBE_RX_FCOE,
#endif
#ifdef __VMKLNX__
	__IXGBE_RING_ALLOCATED,
	__IXGBE_RING_CLEAN_BUSY,
	__IXGBE_RING_RSS,
#endif /* __VMKLNX__ */
	__IXGBE_RING_NETDEV_CNA,
};

#define check_for_tx_hang(ring) \
	test_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define set_check_for_tx_hang(ring) \
	set_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define clear_check_for_tx_hang(ring) \
	clear_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#ifndef IXGBE_NO_HW_RSC
#define ring_is_rsc_enabled(ring) \
	test_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#else
#define ring_is_rsc_enabled(ring)	false
#endif
#define set_ring_rsc_enabled(ring) \
	set_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#define clear_ring_rsc_enabled(ring) \
	clear_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#ifdef __VMKLNX__
#define ring_is_allocated(ring) \
	test_bit(__IXGBE_RING_ALLOCATED, &(ring)->state)
#define set_ring_allocated(ring) \
	set_bit(__IXGBE_RING_ALLOCATED, &(ring)->state)
#define clear_ring_allocated(ring) \
	clear_bit(__IXGBE_RING_ALLOCATED, &(ring)->state)
#define ring_is_rss_enabled(ring) \
	test_bit(__IXGBE_RING_RSS, &(ring)->state)
#define set_ring_rss_enabled(ring) \
	set_bit(__IXGBE_RING_RSS, &(ring)->state)
#define clear_ring_rss_enabled(ring) \
	clear_bit(__IXGBE_RING_RSS, &(ring)->state)
#endif /* __VMKLNX__ */
/*
 * queues are splitted up into 2 distinct pools. This internal
 * resources split is due to the multiple clients of queues.
 */
#define ring_type_is_cna(ring) \
	test_bit(__IXGBE_RING_NETDEV_CNA, &(ring)->state)
#define set_ring_type_cna(ring) \
	set_bit(__IXGBE_RING_NETDEV_CNA, &(ring)->state)
#define clear_ring_type_cna(ring) \
	clear_bit(__IXGBE_RING_NETDEV_CNA, &(ring)->state)

#define netdev_ring(ring) (ring->netdev)
#define ring_queue_index(ring) (ring->queue_index)


struct ixgbe_ring {
	struct ixgbe_ring *next;	/* pointer to next ring in q_vector */
	struct ixgbe_q_vector *q_vector; /* backpointer to host q_vector */
	struct net_device *netdev;	/* netdev ring belongs to */
	struct device *dev;		/* device for DMA mapping */
	void *desc;			/* descriptor ring memory */
	union {
		struct ixgbe_tx_buffer *tx_buffer_info;
		struct ixgbe_rx_buffer *rx_buffer_info;
	};
	unsigned long state;
#ifndef NO_LER_WRITE_CHECKS
	u8 __iomem **adapter_present;	/* Points to field in ixgbe_hw */
#endif /* NO_LER_WRITE_CHECKS */
	u8 __iomem *tail;
	dma_addr_t dma;			/* phys. address of descriptor ring */
	unsigned int size;		/* length in bytes */

	u16 count;			/* amount of descriptors */

	u8 queue_index; /* needed for multiqueue queue management */
	u8 reg_idx;			/* holds the special value that gets
					 * the hardware register offset
					 * associated with this ring, which is
					 * different for DCB and RSS modes
					 */
	u16 next_to_use;
	u16 next_to_clean;

	union {
		u16 rx_buf_len;
		struct {
			u8 atr_sample_rate;
			u8 atr_count;
		};
	};

	u8 dcb_tc;
#ifdef __VMKLNX__
	u8 active;
#endif /* __VMKLNX__ */
	struct ixgbe_queue_stats stats;
#ifdef HAVE_NDO_GET_STATS64
	struct u64_stats_sync syncp;
#endif
	union {
		struct ixgbe_tx_queue_stats tx_stats;
		struct ixgbe_rx_queue_stats rx_stats;
	};
} ____cacheline_internodealigned_in_smp;

static inline void ixgbe_write_tail(struct ixgbe_ring *ring, u32 value)
{
#ifndef NO_LER_WRITE_CHECKS
	if (unlikely(!*ring->adapter_present))
		return;
#endif /* NO_LER_WRITE_CHECKS */
	writel(value, ring->tail);
}

enum ixgbe_ring_f_enum {
	RING_F_NONE = 0,
	RING_F_VMDQ,  /* SR-IOV uses the same ring feature */
	RING_F_RSS,
	RING_F_FDIR,
#ifdef IXGBE_FCOE
	RING_F_FCOE,
#endif /* IXGBE_FCOE */
	RING_F_ARRAY_SIZE  /* must be last in enum set */
};

#define IXGBE_MAX_DCB_INDICES		8
#define IXGBE_MAX_RSS_INDICES		16
#define IXGBE_MAX_VMDQ_INDICES		64
#define IXGBE_MAX_FDIR_INDICES		63
#ifdef IXGBE_FCOE
#define IXGBE_MAX_FCOE_INDICES	8
#define MAX_RX_QUEUES	(IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#define MAX_TX_QUEUES	(IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#else
#define MAX_RX_QUEUES	(IXGBE_MAX_FDIR_INDICES + 1)
#define MAX_TX_QUEUES	(IXGBE_MAX_FDIR_INDICES + 1)
#endif /* IXGBE_FCOE */
struct ixgbe_ring_feature {
	u16 limit;	/* upper limit on feature indices */
	u16 indices;	/* current value of indices */
	u16 mask;	/* Mask used for feature to ring mapping */
	u16 offset;	/* offset to start of feature */
};

#define IXGBE_82599_VMDQ_8Q_MASK 0x78
#define IXGBE_82599_VMDQ_4Q_MASK 0x7C
#define IXGBE_82599_VMDQ_2Q_MASK 0x7E

struct ixgbe_ring_container {
	struct ixgbe_ring *ring;	/* pointer to linked list of rings */
	unsigned int total_bytes;	/* total bytes processed this int */
	unsigned int total_packets;	/* total packets processed this int */
	u16 work_limit;			/* total work allowed per interrupt */
	u8 count;			/* total number of rings in vector */
	u8 itr;				/* current ITR setting for ring */
};

/* iterator for handling rings in ring container */
#define ixgbe_for_each_ring(pos, head) \
	for (pos = (head).ring; pos != NULL; pos = pos->next)

#define MAX_RX_PACKET_BUFFERS	((adapter->flags & IXGBE_FLAG_DCB_ENABLED) \
				 ? 8 : 1)
#define MAX_TX_PACKET_BUFFERS	MAX_RX_PACKET_BUFFERS

/* MAX_MSIX_Q_VECTORS of these are allocated,
 * but we only use one per queue-specific vector.
 */
struct ixgbe_q_vector {
	struct ixgbe_adapter *adapter;
	int cpu;	/* CPU for DCA */
	u16 v_idx;	/* index of q_vector within array, also used for
			 * finding the bit in EICR and friends that
			 * represents the vector for this ring */
	u16 itr;	/* Interrupt throttle rate written to EITR */
	struct ixgbe_ring_container rx, tx;

	struct napi_struct napi;
#ifndef HAVE_NETDEV_NAPI_LIST
	struct net_device poll_dev;
#endif
#ifdef HAVE_IRQ_AFFINITY_HINT
	cpumask_t affinity_mask;
#endif
	int numa_node;
	struct rcu_head rcu;	/* to avoid race with update stats on free */
	char name[IFNAMSIZ + 9];

#ifdef CONFIG_NET_RX_BUSY_POLL
	unsigned int state;
#define IXGBE_QV_STATE_IDLE        0
#define IXGBE_QV_STATE_NAPI	   1    /* NAPI owns this QV */
#define IXGBE_QV_STATE_POLL	   2    /* poll owns this QV */
#define IXGBE_QV_STATE_DISABLED	   4    /* QV is disabled */
#define IXGBE_QV_OWNED (IXGBE_QV_STATE_NAPI | IXGBE_QV_STATE_POLL)
#define IXGBE_QV_LOCKED (IXGBE_QV_OWNED | IXGBE_QV_STATE_DISABLED)
#define IXGBE_QV_STATE_NAPI_YIELD  8    /* NAPI yielded this QV */
#define IXGBE_QV_STATE_POLL_YIELD  16   /* poll yielded this QV */
#define IXGBE_QV_YIELD (IXGBE_QV_STATE_NAPI_YIELD | IXGBE_QV_STATE_POLL_YIELD)
#define IXGBE_QV_USER_PEND (IXGBE_QV_STATE_POLL | IXGBE_QV_STATE_POLL_YIELD)
	spinlock_t lock;
#endif  /* CONFIG_NET_RX_BUSY_POLL */

	/* for dynamic allocation of rings associated with this q_vector */
	struct ixgbe_ring ring[0] ____cacheline_internodealigned_in_smp;
};
#ifdef CONFIG_NET_RX_BUSY_POLL
static inline void ixgbe_qv_init_lock(struct ixgbe_q_vector *q_vector)
{

	spin_lock_init(&q_vector->lock);
	q_vector->state = IXGBE_QV_STATE_IDLE;
}

/* called from the device poll routine to get ownership of a q_vector */
static inline bool ixgbe_qv_lock_napi(struct ixgbe_q_vector *q_vector)
{
	int rc = true;
	spin_lock_bh(&q_vector->lock);
	if (q_vector->state & IXGBE_QV_LOCKED) {
		WARN_ON(q_vector->state & IXGBE_QV_STATE_NAPI);
		q_vector->state |= IXGBE_QV_STATE_NAPI_YIELD;
		rc = false;
#ifdef BP_EXTENDED_STATS
		q_vector->tx.ring->stats.yields++;
#endif
	} else {
		/* we don't care if someone yielded */
		q_vector->state = IXGBE_QV_STATE_NAPI;
	}
	spin_unlock_bh(&q_vector->lock);
	return rc;
}

/* returns true is someone tried to get the qv while napi had it */
static inline bool ixgbe_qv_unlock_napi(struct ixgbe_q_vector *q_vector)
{
	int rc = false;
	spin_lock_bh(&q_vector->lock);
	WARN_ON(q_vector->state & (IXGBE_QV_STATE_POLL |
			       IXGBE_QV_STATE_NAPI_YIELD));

	if (q_vector->state & IXGBE_QV_STATE_POLL_YIELD)
		rc = true;
	/* reset state to idle, unless QV is disabled */
	q_vector->state &= IXGBE_QV_STATE_DISABLED;
	spin_unlock_bh(&q_vector->lock);
	return rc;
}

/* called from ixgbe_low_latency_poll() */
static inline bool ixgbe_qv_lock_poll(struct ixgbe_q_vector *q_vector)
{
	int rc = true;
	spin_lock_bh(&q_vector->lock);
	if ((q_vector->state & IXGBE_QV_LOCKED)) {
		q_vector->state |= IXGBE_QV_STATE_POLL_YIELD;
		rc = false;
#ifdef BP_EXTENDED_STATS
		q_vector->rx.ring->stats.yields++;
#endif
	} else {
		/* preserve yield marks */
		q_vector->state |= IXGBE_QV_STATE_POLL;
	}
	spin_unlock_bh(&q_vector->lock);
	return rc;
}

/* returns true if someone tried to get the qv while it was locked */
static inline bool ixgbe_qv_unlock_poll(struct ixgbe_q_vector *q_vector)
{
	int rc = false;
	spin_lock_bh(&q_vector->lock);
	WARN_ON(q_vector->state & (IXGBE_QV_STATE_NAPI));

	if (q_vector->state & IXGBE_QV_STATE_POLL_YIELD)
		rc = true;
	/* reset state to idle, unless QV is disabled */
	q_vector->state &= IXGBE_QV_STATE_DISABLED;
	spin_unlock_bh(&q_vector->lock);
	return rc;
}

/* true if a socket is polling, even if it did not get the lock */
static inline bool ixgbe_qv_busy_polling(struct ixgbe_q_vector *q_vector)
{
	WARN_ON(!(q_vector->state & IXGBE_QV_LOCKED));
	return q_vector->state & IXGBE_QV_USER_PEND;
}

/* false if QV is currently owned */
static inline bool ixgbe_qv_disable(struct ixgbe_q_vector *q_vector)
{
	int rc = true;
	spin_lock_bh(&q_vector->lock);
	if (q_vector->state & IXGBE_QV_OWNED)
		rc = false;
	q_vector->state |= IXGBE_QV_STATE_DISABLED;
	spin_unlock_bh(&q_vector->lock);
	return rc;
}
#endif /* CONFIG_NET_RX_BUSY_POLL */
#ifdef IXGBE_HWMON

#define IXGBE_HWMON_TYPE_LOC		0
#define IXGBE_HWMON_TYPE_TEMP		1
#define IXGBE_HWMON_TYPE_CAUTION	2
#define IXGBE_HWMON_TYPE_MAX		3

struct hwmon_attr {
	struct device_attribute dev_attr;
	struct ixgbe_hw *hw;
	struct ixgbe_thermal_diode_data *sensor;
	char name[12];
};

struct hwmon_buff {
	struct device *device;
	struct hwmon_attr *hwmon_list;
	unsigned int n_hwmon;
};
#endif /* IXGBE_HWMON */

/*
 * microsecond values for various ITR rates shifted by 2 to fit itr register
 * with the first 3 bits reserved 0
 */
#define IXGBE_MIN_RSC_ITR	24
#define IXGBE_100K_ITR		40
#define IXGBE_20K_ITR		200
#define IXGBE_16K_ITR		248
#define IXGBE_10K_ITR		400
#define IXGBE_8K_ITR		500

/* ixgbe_test_staterr - tests bits in Rx descriptor status and error fields */
static inline __le32 ixgbe_test_staterr(union ixgbe_adv_rx_desc *rx_desc,
					const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}

/* ixgbe_desc_unused - calculate if we have unused descriptors */
static inline u16 ixgbe_desc_unused(struct ixgbe_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	u16 ntu = ring->next_to_use;

	return ((ntc > ntu) ? 0 : ring->count) + ntc - ntu - 1;
}

#define IXGBE_RX_DESC(R, i)	\
	(&(((union ixgbe_adv_rx_desc *)((R)->desc))[i]))
#define IXGBE_TX_DESC(R, i)	\
	(&(((union ixgbe_adv_tx_desc *)((R)->desc))[i]))
#define IXGBE_TX_CTXTDESC(R, i)	\
	(&(((struct ixgbe_adv_tx_context_desc *)((R)->desc))[i]))

#define IXGBE_MAX_JUMBO_FRAME_SIZE	9728
#ifdef IXGBE_FCOE
/* use 3K as the baby jumbo frame size for FCoE */
#define IXGBE_FCOE_JUMBO_FRAME_SIZE	3072
#endif /* IXGBE_FCOE */

#define TCP_TIMER_VECTOR	0
#define OTHER_VECTOR	1
#define NON_Q_VECTORS	(OTHER_VECTOR + TCP_TIMER_VECTOR)

#define IXGBE_MAX_MSIX_Q_VECTORS_82599	64
#define IXGBE_MAX_MSIX_Q_VECTORS_82598	16

struct ixgbe_mac_addr {
	u8 addr[ETH_ALEN];
	u16 queue;
	u16 state; /* bitmask */
};


#define IXGBE_MAC_STATE_DEFAULT		0x1
#define IXGBE_MAC_STATE_MODIFIED	0x2
#define IXGBE_MAC_STATE_IN_USE		0x4

struct ixgbe_therm_proc_data {
	struct ixgbe_hw *hw;
	struct ixgbe_thermal_diode_data *sensor_data;
};

/*
 * Only for array allocations in our adapter struct.  On 82598, there will be
 * unused entries in the array, but that's not a big deal.  Also, in 82599,
 * we can actually assign 64 queue vectors based on our extended-extended
 * interrupt registers.  This is different than 82598, which is limited to 16.
 */
#define MAX_MSIX_Q_VECTORS	IXGBE_MAX_MSIX_Q_VECTORS_82599
#define MAX_MSIX_COUNT		IXGBE_MAX_MSIX_VECTORS_82599

#define MIN_MSIX_Q_FCOE_VECTORS	2
#define MIN_MSIX_Q_VECTORS	(1 + MIN_MSIX_Q_FCOE_VECTORS)
#define MIN_MSIX_COUNT		(MIN_MSIX_Q_VECTORS + NON_Q_VECTORS)

/* default to trying for four seconds */
#define IXGBE_TRY_LINK_TIMEOUT	(4 * HZ)

/* board specific private data structure */
struct ixgbe_adapter {
#ifdef IXGBE_WFS
	char name[IFNAMSIZ];
	spinlock_t xmit_lock;
	bool is_wfs_primary;
	struct ixgbe_wfs_adapter *wfs_parent;
	struct ixgbe_adapter *wfs_next;
	struct ixgbe_adapter *wfs_other;
	u8 wfs_port;	/* devfn & 0x7 */
#endif /* IXGBE_WFS */
#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
#ifdef HAVE_VLAN_RX_REGISTER
	struct vlan_group *vlgrp; /* must be first, see ixgbe_receive_skb */
#else
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif
#endif /* NETIF_F_HW_VLAN_TX || NETIF_F_HW_VLAN_CTAG_TX */
	/* OS defined structs */
	struct net_device *netdev;
	struct net_device *cnadev;
	struct pci_dev *pdev;

	unsigned long state;

	/* Some features need tri-state capability,
	 * thus the additional *_CAPABLE flags.
	 */
	u32 flags;
#define IXGBE_FLAG_MSI_CAPABLE			(u32)(1 << 0)
#define IXGBE_FLAG_MSI_ENABLED			(u32)(1 << 1)
#define IXGBE_FLAG_MSIX_CAPABLE			(u32)(1 << 2)
#define IXGBE_FLAG_MSIX_ENABLED			(u32)(1 << 3)
#ifndef IXGBE_NO_LLI
#define IXGBE_FLAG_LLI_PUSH			(u32)(1 << 4)
#endif
#define IXGBE_FLAG_IN_NETPOLL                   (u32)(1 << 5)
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_FLAG_DCA_ENABLED			(u32)(1 << 6)
#define IXGBE_FLAG_DCA_CAPABLE			(u32)(1 << 7)
#define IXGBE_FLAG_DCA_ENABLED_DATA		(u32)(1 << 8)
#else
#define IXGBE_FLAG_DCA_ENABLED			(u32)0
#define IXGBE_FLAG_DCA_CAPABLE			(u32)0
#define IXGBE_FLAG_DCA_ENABLED_DATA             (u32)0
#endif
#define IXGBE_FLAG_MQ_CAPABLE			(u32)(1 << 9)
#define IXGBE_FLAG_DCB_ENABLED			(u32)(1 << 10)
#define IXGBE_FLAG_VMDQ_ENABLED			(u32)(1 << 11)
#define IXGBE_FLAG_FAN_FAIL_CAPABLE		(u32)(1 << 12)
#define IXGBE_FLAG_NEED_LINK_UPDATE		(u32)(1 << 13)
#define IXGBE_FLAG_NEED_LINK_CONFIG		(u32)(1 << 14)
#define IXGBE_FLAG_FDIR_HASH_CAPABLE		(u32)(1 << 15)
#define IXGBE_FLAG_FDIR_PERFECT_CAPABLE		(u32)(1 << 16)
#ifdef IXGBE_FCOE
#define IXGBE_FLAG_FCOE_CAPABLE			(u32)(1 << 17)
#define IXGBE_FLAG_FCOE_ENABLED			(u32)(1 << 18)
#endif /* IXGBE_FCOE */
#define IXGBE_FLAG_SRIOV_CAPABLE		(u32)(1 << 19)
#define IXGBE_FLAG_SRIOV_ENABLED		(u32)(1 << 20)
#define IXGBE_FLAG_SRIOV_REPLICATION_ENABLE	(u32)(1 << 21)
#define IXGBE_FLAG_SRIOV_L2SWITCH_ENABLE	(u32)(1 << 22)
#define IXGBE_FLAG_SRIOV_L2LOOPBACK_ENABLE	(u32)(1 << 23)
#define IXGBE_FLAG_RX_HWTSTAMP_ENABLED          (u32)(1 << 24)

/* preset defaults */
#define IXGBE_FLAGS_82598_INIT		(IXGBE_FLAG_MSI_CAPABLE |	\
					 IXGBE_FLAG_MSIX_CAPABLE |	\
					 IXGBE_FLAG_MQ_CAPABLE)

#define IXGBE_FLAGS_82599_INIT		(IXGBE_FLAGS_82598_INIT |	\
					 IXGBE_FLAG_SRIOV_CAPABLE)

#define IXGBE_FLAGS_X540_INIT		IXGBE_FLAGS_82599_INIT


	u32 flags2;
#ifndef IXGBE_NO_HW_RSC
#define IXGBE_FLAG2_RSC_CAPABLE			(u32)(1 << 0)
#define IXGBE_FLAG2_RSC_ENABLED			(u32)(1 << 1)
#else
#define IXGBE_FLAG2_RSC_CAPABLE			0
#define IXGBE_FLAG2_RSC_ENABLED			0
#endif
#define IXGBE_FLAG2_CNA_ENABLED			(u32)(1 << 2)
#define IXGBE_FLAG2_TEMP_SENSOR_CAPABLE		(u32)(1 << 3)
#define IXGBE_FLAG2_TEMP_SENSOR_EVENT		(u32)(1 << 4)
#define IXGBE_FLAG2_SEARCH_FOR_SFP		(u32)(1 << 5)
#define IXGBE_FLAG2_SFP_NEEDS_RESET		(u32)(1 << 6)
#define IXGBE_FLAG2_RESET_REQUESTED		(u32)(1 << 7)
#define IXGBE_FLAG2_FDIR_REQUIRES_REINIT	(u32)(1 << 8)
#define IXGBE_FLAG2_RSS_FIELD_IPV4_UDP		(u32)(1 << 9)
#define IXGBE_FLAG2_RSS_FIELD_IPV6_UDP		(u32)(1 << 10)
#define IXGBE_FLAG2_PTP_PPS_ENABLED		(u32)(1 << 11)
	bool cloud_mode;

	/* Tx fast path data */
	int num_tx_queues;
	u16 tx_itr_setting;
	u16 tx_work_limit;

	/* Rx fast path data */
	int num_rx_queues;
	u16 rx_itr_setting;
	u16 rx_work_limit;

	/* TX */
	struct ixgbe_ring *tx_ring[MAX_TX_QUEUES] ____cacheline_aligned_in_smp;

	u64 restart_queue;
	u64 lsc_int;
	u32 tx_timeout_count;

	/* RX */
	struct ixgbe_ring *rx_ring[MAX_RX_QUEUES];
	int num_rx_pools;		/* == num_rx_queues in 82598 */
	int num_rx_queues_per_pool;	/* 1 if 82598, can be many if 82599 */
	u64 hw_csum_rx_error;
	u64 hw_rx_no_dma_resources;
	u64 rsc_total_count;
	u64 rsc_total_flush;
	u64 non_eop_descs;
	u64 rx_hdr_split;
	u32 alloc_rx_page_failed;
	u32 alloc_rx_buff_failed;

	struct ixgbe_q_vector *q_vector[MAX_MSIX_Q_VECTORS];

	struct ixgbe_dcb_config dcb_cfg;
	struct ixgbe_dcb_config temp_dcb_cfg;
	u8 dcb_set_bitmap;
	u8 dcbx_cap;
#ifndef HAVE_MQPRIO
	u8 dcb_tc;
#endif
	enum ixgbe_fc_mode last_lfc_mode;

	int num_q_vectors;	/* current number of q_vectors for device */
	int max_q_vectors;	/* upper limit of q_vectors for device */
	struct ixgbe_ring_feature ring_feature[RING_F_ARRAY_SIZE];
	struct msix_entry *msix_entries;

#ifndef HAVE_NETDEV_STATS_IN_NETDEV
	struct net_device_stats net_stats;
#endif

#ifdef ETHTOOL_TEST
	u32 test_icr;
	struct ixgbe_ring test_tx_ring;
	struct ixgbe_ring test_rx_ring;
#endif

	/* structs defined in ixgbe_hw.h */
	struct ixgbe_hw hw;
	u16 msg_enable;
	struct ixgbe_hw_stats stats;
#ifndef IXGBE_NO_LLI
	u32 lli_port;
	u32 lli_size;
	u32 lli_etype;
	u32 lli_vlan_pri;
#endif /* IXGBE_NO_LLI */

	u32 *config_space;
	u64 tx_busy;
#ifdef __VMKLNX__
	u32 n_rx_queues_allocated;
	u32 n_tx_queues_allocated;
#endif
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;

	u32 link_speed;
	bool link_up;
	unsigned long link_check_timeout;

	struct timer_list service_timer;
	struct work_struct service_task;

	struct hlist_head fdir_filter_list;
	unsigned long fdir_overflow; /* number of times ATR was backed off */
	union ixgbe_atr_input fdir_mask;
	int fdir_filter_count;
	u32 fdir_pballoc;
	u32 atr_sample_rate;
	spinlock_t fdir_perfect_lock;

#ifdef IXGBE_FCOE
	struct ixgbe_fcoe fcoe;
#endif /* IXGBE_FCOE */
	u8 __iomem *io_addr;	/* Mainly for iounmap use */
	u32 wol;

	u16 bd_number;

	char eeprom_id[32];
	u16 eeprom_cap;
	bool netdev_registered;
	u32 interrupt_event;
#ifdef HAVE_ETHTOOL_SET_PHYS_ID
	u32 led_reg;
#endif


	DECLARE_BITMAP(active_vfs, IXGBE_MAX_VF_FUNCTIONS);
	unsigned int num_vfs;
	struct vf_data_storage *vfinfo;
	int vf_rate_link_speed;
	struct vf_macvlans vf_mvs;
	struct vf_macvlans *mv_list;
	u32 timer_event_accumulator;
	u32 vferr_refcount;
	struct ixgbe_mac_addr *mac_table;
	struct proc_dir_entry *eth_dir;
	struct proc_dir_entry *info_dir;
	u64 old_lsc;
	struct proc_dir_entry *therm_dir[IXGBE_MAX_SENSORS];
	struct ixgbe_therm_proc_data therm_data[IXGBE_MAX_SENSORS];

#ifdef __VMKLNX__
	u16 SmbTblLen;
	u32 SmbTblAddr;
#endif /* __VMKLNX__ */
#ifdef HAVE_IXGBE_DEBUG_FS
	struct dentry *ixgbe_dbg_adapter;
#endif /*HAVE_IXGBE_DEBUG_FS*/
	u8 default_up;
#ifdef HAVE_TX_MQ
#ifndef HAVE_NETDEV_SELECT_QUEUE
	unsigned int indices;
#endif
#endif
};

static inline u8 ixgbe_max_rss_indices(struct ixgbe_adapter *adapter)
{
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82598EB:
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		return IXGBE_MAX_RSS_INDICES;
		break;
	default:
		return 0;
		break;
	}
}

struct ixgbe_fdir_filter {
	struct  hlist_node fdir_node;
	union ixgbe_atr_input filter;
	u16 sw_idx;
	u16 action;
};

enum ixgbe_state_t {
	__IXGBE_TESTING,
	__IXGBE_RESETTING,
	__IXGBE_DOWN,
	__IXGBE_DISABLED,
	__IXGBE_REMOVE,
	__IXGBE_SERVICE_SCHED,
	__IXGBE_SERVICE_INITED,
	__IXGBE_IN_SFP_INIT,
};

struct ixgbe_cb {
	union {				/* Union defining head/tail partner */
		struct sk_buff *head;
		struct sk_buff *tail;
	};
	dma_addr_t dma;
#ifdef HAVE_VLAN_RX_REGISTER
	u16	vid;			/* VLAN tag */
#endif
	u16	append_cnt;		/* number of skb's appended */
};
#define IXGBE_CB(skb) ((struct ixgbe_cb *)(skb)->cb)

/* ESX ixgbe CIM IOCTL definition */
#define SIOCINTELCIM		0x89F8
#define INTELCIM_ENUMDIAGS	0x01 /* enumerate diagnostics */
#define INTELCIM_RUNDIAG	0x02 /* run diagnostics */
#define INTELCIM_FNDSMB		0x03 /* Find SMBIOS entry and size */
#define INTELCIM_GETSMBTBL	0x04 /* get SMBIOS tables */
#define INTELCIM_WRITEMEM	0x05 /* write data from user space to memory */
#define INTELCIM_READMEM	0x06 /* read data from memory to user space */
#define INTELCIM_GET_PCIE_ERROR_INFO   0x07
#define INTELCIM_GET_PCI_LINK_STATUS   0x08
#ifdef __VMKLNX__
#define INTELDIAG_ESX_READ_REG	0x09 /* diagnostics - read  registers */
#define INTELDIAG_ESX_WRITE_REG	0x0A /* diagnostics - write register */
#endif /* __VMKLNX__ */
#define SM_ADDR_HIGH	0x000FFFFF
#define SM_ADDR_LOW	0x000F0000
static const unsigned char sm_anchor[4] = "_SM_";

struct smbios_structure_table {
	u8 AnchorString[4];
	u8 EntryPointChecksum;
	u8 EntryPointLength;
	u8 SmMajorVersion;
	u8 SmMinorVersion;
	u16 MaxStructureSize;
	u8 EntryPointRevision;
	u8 FormattedArea[5];
	u8 IntermediateAnchorString[5];
	u8 IntermediateChecksum;
	u16 TableLength;
	u32 TableAddress;
	u16 NumberSmStructures;
	u8 SmBcdRevision;
} __attribute__((__packed__));

struct intelcim_mem_buf {
	u64 addr;
	u32 len; /* Length in bytes */
	u8 data[0];
} __attribute__((__packed__));

struct intelcim_pcie_error_info {
	u32 num_regs; /* Number of dwords */
	u32 data[0];
} __attribute__((__packed__));
#ifdef __VMKLNX__
struct inteldiag_esx_reg_access	{
	u32 reg_offset;
	u32 data;
} __attribute__((__packed__));
#endif /*__VMKLNX__*/
struct ixgbe_intelcim_ioctl_req {
	u32 cmd;
	union {
		struct ethtool_gstrings gstrings;
		struct ethtool_test test;
		struct smbios_structure_table tbl;
		u8 smbios[0];
		struct intelcim_mem_buf buf;
		struct intelcim_pcie_error_info info;
		u16 link_status;
#ifdef __VMKLNX__
		struct inteldiag_esx_reg_access reg_info;
#endif /*__VMKLNX__*/
	} cmd_req;
} __attribute__((packed));

int ixgbe_intelcim_ioctl(struct net_device *netdev, struct ifreq *ifr);

void ixgbe_procfs_exit(struct ixgbe_adapter *adapter);
int ixgbe_procfs_init(struct ixgbe_adapter *adapter);
int ixgbe_procfs_topdir_init(void);
void ixgbe_procfs_topdir_exit(void);

extern struct dcbnl_rtnl_ops dcbnl_ops;
int ixgbe_copy_dcb_cfg(struct ixgbe_adapter *adapter, int tc_max);

u8 ixgbe_dcb_txq_to_tc(struct ixgbe_adapter *adapter, u8 index);

/* needed by ixgbe_main.c */
int ixgbe_validate_mac_addr(u8 *mc_addr);
void ixgbe_check_options(struct ixgbe_adapter *adapter);
void ixgbe_assign_netdev_ops(struct net_device *netdev);

/* needed by ixgbe_ethtool.c */
extern char ixgbe_driver_name[];
extern const char ixgbe_driver_version[];

void ixgbe_up(struct ixgbe_adapter *adapter);
void ixgbe_down(struct ixgbe_adapter *adapter);
void ixgbe_reinit_locked(struct ixgbe_adapter *adapter);
void ixgbe_reset(struct ixgbe_adapter *adapter);
void ixgbe_set_ethtool_ops(struct net_device *netdev);
int ixgbe_setup_rx_resources(struct ixgbe_ring *);
int ixgbe_setup_tx_resources(struct ixgbe_ring *);
void ixgbe_free_rx_resources(struct ixgbe_ring *);
void ixgbe_free_tx_resources(struct ixgbe_ring *);
void ixgbe_configure_rx_ring(struct ixgbe_adapter *,
				    struct ixgbe_ring *);
void ixgbe_configure_tx_ring(struct ixgbe_adapter *,
				    struct ixgbe_ring *);
void ixgbe_update_stats(struct ixgbe_adapter *adapter);
int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter);
void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter);
bool ixgbe_is_ixgbe(struct pci_dev *pcidev);
netdev_tx_t ixgbe_xmit_frame_ring(struct sk_buff *,
					 struct ixgbe_adapter *,
					 struct ixgbe_ring *);
void ixgbe_unmap_and_free_tx_resource(struct ixgbe_ring *,
					     struct ixgbe_tx_buffer *);
void ixgbe_alloc_rx_buffers(struct ixgbe_ring *, u16);
void ixgbe_configure_rscctl(struct ixgbe_adapter *adapter,
				   struct ixgbe_ring *);
void ixgbe_clear_rscctl(struct ixgbe_adapter *adapter,
			       struct ixgbe_ring *);
void ixgbe_set_rx_mode(struct net_device *netdev);
int ixgbe_write_mc_addr_list(struct net_device *netdev);
int ixgbe_setup_tc(struct net_device *dev, u8 tc);
void ixgbe_tx_ctxtdesc(struct ixgbe_ring *, u32, u32, u32, u32);
void ixgbe_do_reset(struct net_device *netdev);
void ixgbe_write_eitr(struct ixgbe_q_vector *q_vector);
int ixgbe_poll(struct napi_struct *napi, int budget);
void ixgbe_disable_rx_queue(struct ixgbe_adapter *adapter,
				   struct ixgbe_ring *);
void ixgbe_vlan_stripping_enable(struct ixgbe_adapter *adapter);
void ixgbe_vlan_stripping_disable(struct ixgbe_adapter *adapter);
#ifdef ETHTOOL_OPS_COMPAT
int ethtool_ioctl(struct ifreq *ifr);
#endif

#ifdef IXGBE_FCOE
void ixgbe_configure_fcoe(struct ixgbe_adapter *adapter);
int ixgbe_fso(struct ixgbe_ring *tx_ring,
		     struct ixgbe_tx_buffer *first,
		     u8 *hdr_len);
int ixgbe_fcoe_ddp(struct ixgbe_adapter *adapter,
			  union ixgbe_adv_rx_desc *rx_desc,
			  struct sk_buff *skb);
int ixgbe_fcoe_ddp_get(struct net_device *netdev, u16 xid,
			      struct scatterlist *sgl, unsigned int sgc);
#ifdef HAVE_NETDEV_OPS_FCOE_DDP_TARGET
int ixgbe_fcoe_ddp_target(struct net_device *netdev, u16 xid,
				 struct scatterlist *sgl, unsigned int sgc);
#endif /* HAVE_NETDEV_OPS_FCOE_DDP_TARGET */
int ixgbe_fcoe_ddp_put(struct net_device *netdev, u16 xid);
int ixgbe_setup_fcoe_ddp_resources(struct ixgbe_adapter *adapter);
void ixgbe_free_fcoe_ddp_resources(struct ixgbe_adapter *adapter);
#ifdef HAVE_NETDEV_OPS_FCOE_ENABLE
int ixgbe_fcoe_enable(struct net_device *netdev);
int ixgbe_fcoe_disable(struct net_device *netdev);
#else
int ixgbe_fcoe_ddp_enable(struct ixgbe_adapter *adapter);
void ixgbe_fcoe_ddp_disable(struct ixgbe_adapter *adapter);
#endif /* HAVE_NETDEV_OPS_FCOE_ENABLE */
#ifdef CONFIG_DCB
#ifdef HAVE_DCBNL_OPS_GETAPP
u8 ixgbe_fcoe_getapp(struct net_device *netdev);
#endif /* HAVE_DCBNL_OPS_GETAPP */
u8 ixgbe_fcoe_setapp(struct ixgbe_adapter *adapter, u8 up);
#endif /* CONFIG_DCB */
u8 ixgbe_fcoe_get_tc(struct ixgbe_adapter *adapter);
#ifdef HAVE_NETDEV_OPS_FCOE_GETWWN
int ixgbe_fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type);
#endif
#endif /* IXGBE_FCOE */

#ifdef HAVE_IXGBE_DEBUG_FS
void ixgbe_dbg_adapter_init(struct ixgbe_adapter *adapter);
void ixgbe_dbg_adapter_exit(struct ixgbe_adapter *adapter);
void ixgbe_dbg_init(void);
void ixgbe_dbg_exit(void);
#endif /* HAVE_IXGBE_DEBUG_FS */

#ifdef CONFIG_DCB
#endif /* CONFIG_DCB */

int ixgbe_wol_supported(struct ixgbe_adapter *adapter, u16 device_id,
			       u16 subdevice_id);
void ixgbe_clean_rx_ring(struct ixgbe_ring *rx_ring);
int ixgbe_get_settings(struct net_device *netdev,
			      struct ethtool_cmd *ecmd);
int ixgbe_write_uc_addr_list(struct net_device *netdev, int vfn);
void ixgbe_full_sync_mac_table(struct ixgbe_adapter *adapter);
int ixgbe_add_mac_filter(struct ixgbe_adapter *adapter,
				u8 *addr, u16 queue);
#ifdef __VMKLNX__
void ixgbe_flush_sw_mac_table(struct ixgbe_adapter *adapter);
void ixgbe_del_mac_filter_by_index(struct ixgbe_adapter *adapter,
						int index);
#endif /* __VMKLNX__ */
int ixgbe_del_mac_filter(struct ixgbe_adapter *adapter,
				u8 *addr, u16 queue);
int ixgbe_available_rars(struct ixgbe_adapter *adapter);
#ifndef HAVE_VLAN_RX_REGISTER
void ixgbe_vlan_mode(struct net_device *, u32);
#endif

void ixgbe_sriov_reinit(struct ixgbe_adapter *adapter);

void ixgbe_set_rx_drop_en(struct ixgbe_adapter *adapter);

#ifdef __VMKLNX__
int ixgbe_calculate_rx_ring_size(struct ixgbe_adapter *adapter);
#endif /* __VMKLNX__ */

#ifdef IXGBE_WFS
extern netdev_tx_t ixgbe_xmit_wfs_frame(struct sk_buff *skb,
					struct ixgbe_adapter *adapter);
#endif /* IXGBE_WFS */
#endif /* _IXGBE_H_ */
