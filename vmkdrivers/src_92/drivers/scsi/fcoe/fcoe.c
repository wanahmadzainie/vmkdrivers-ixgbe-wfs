/* ****************************************************************
 * Portions Copyright 2010-2011 VMware, Inc.
 * ****************************************************************/
/*
 * Copyright(c) 2007 - 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#if !defined(__VMKLNX__)
#include <linux/cpu.h>
#endif /* !defined(__VMKLNX__) */
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#if !defined(__VMKLNX__)
#include <net/rtnetlink.h>
#endif /* !defined(__VMKLNX__) */

#include <scsi/fc/fc_encaps.h>
#include <scsi/fc/fc_fip.h>

#include <scsi/libfc.h>
#include <scsi/fc_frame.h>
#include <scsi/libfcoe.h>
#include <scsi/fcoe_compat.h>

#include "fcoe.h"
#if defined(__VMKLNX__)
#include <vmklinux_92/vmklinux_scsi.h>
#include <vmklinux_92/vmklinux_cna.h>
#endif /* defined(__VMKLNX__) */

MODULE_AUTHOR("Open-FCoE.org");
MODULE_DESCRIPTION("FCoE");
MODULE_LICENSE("GPLv2");
#if defined(__VMKLNX__)
#define FCOE_INFO_BUF 8192
#define fcoe_version_str "1.0.29.9.2-7vmw"
MODULE_VERSION(fcoe_version_str)
#endif /* defined(__VMKLNX__) */

/* Performance tuning parameters for fcoe */
static unsigned int fcoe_ddp_min = 2112;
module_param_named(ddp_min, fcoe_ddp_min, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ddp_min, "Minimum I/O size in bytes for "	\
		 "Direct Data Placement (DDP).");

static u16 fcoe_max_xid = FCOE_MAX_XID;
module_param_named(max_xid, fcoe_max_xid, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_xid, "Maximum exchange ID supported by FCoE stack.");

DEFINE_MUTEX(fcoe_config_mutex);

/* fcoe_percpu_clean completion.  Waiter protected by fcoe_create_mutex */
static DECLARE_COMPLETION(fcoe_flush_completion);

#if defined(__VMKLNX__)
/*
 * Sentinel skb used with fcoe_percpu_clean instead of skb->destructor,
 * which is unsupported in Vmklinux at the moment.
 */
static struct sk_buff *flush_skb;
#endif /* defined(__VMKLNX__) */

/* fcoe host list */
/* must only by accessed under the RTNL mutex */
LIST_HEAD(fcoe_hostlist);
DEFINE_PER_CPU(struct fcoe_percpu_s, fcoe_percpu);

#if defined(__VMKLNX__)
/* fcoe log */
#define FCOE_MODULE_NAME        "fcoe"
vmk_LogComponent fcoeLog;

/*
 * Turn on the FCOE_LKST_DBG to log link state events.
 * We should remove this after FC.
 */
unsigned int fcoe_debug_logging = 0x04;
#else
unsigned int fcoe_debug_logging;
#endif /* defined(__VMKLNX__) */
module_param_named(fcoe_debug_logging, fcoe_debug_logging, int, S_IRUGO|S_IWUSR);
/* Function Prototypes */
static int fcoe_reset(struct Scsi_Host *);
static int fcoe_xmit(struct fc_lport *, struct fc_frame *);
static int fcoe_rcv(struct sk_buff *, struct net_device *,
		    struct packet_type *, struct net_device *);
static int fcoe_percpu_receive_thread(void *);
static void fcoe_clean_pending_queue(struct fc_lport *);
static void fcoe_percpu_clean(struct fc_lport *);
static int fcoe_link_ok(struct fc_lport *);

static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *);
static int fcoe_hostlist_add(const struct fc_lport *);

static void fcoe_check_wait_queue(struct fc_lport *, struct sk_buff *);
static int fcoe_device_notification(struct notifier_block *, ulong, void *);
static void fcoe_dev_setup(void);
static void fcoe_dev_cleanup(void);
static struct fcoe_interface
*fcoe_hostlist_lookup_port(const struct net_device *);

#if !defined(__VMKLNX__)
static int fcoe_fip_recv(struct sk_buff *, struct net_device *,
			 struct packet_type *, struct net_device *);
#endif /* !defined(__VMKLNX__) */

static inline int fcoe_start_io(struct sk_buff *);
static void fcoe_fip_send(struct fcoe_ctlr *, struct sk_buff *);
static void fcoe_update_src_mac(struct fc_lport *, u8 *);
static u8 *fcoe_get_src_mac(struct fc_lport *);
static void fcoe_destroy_work(struct work_struct *);

static int fcoe_ddp_setup(struct fc_lport *, u16, struct scatterlist *,
			  unsigned int);
static int fcoe_ddp_done(struct fc_lport *, u16);

#if !defined(__VMKLNX__)
static int fcoe_cpu_callback(struct notifier_block *, unsigned long, void *);
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
static int fcoe_create(const char *, struct kernel_param *);
static int fcoe_destroy(const char *, struct kernel_param *);
#else /* defined(__VMKLNX__) */
static int fcoe_create(struct net_device *netdevice);
static int fcoe_destroy(struct net_device *netdevice);
static int fcoe_wakeup(struct net_device *);
#endif /* !defined(__VMKLNX__) */

static struct fc_seq *fcoe_elsct_send(struct fc_lport *,
				      u32 did, struct fc_frame *,
				      unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *, u32 timeout);
static void fcoe_recv_frame(struct sk_buff *skb);

static void fcoe_get_lesb(struct fc_lport *, struct fc_els_lesb *);

#if !defined(__VMKLNX__)
module_param_call(create, fcoe_create, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(create, "string");
MODULE_PARM_DESC(create, " Creates fcoe instance on a ethernet interface");
module_param_call(destroy, fcoe_destroy, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(destroy, "string");
MODULE_PARM_DESC(destroy, " Destroys fcoe instance on a ethernet interface");
module_param_call(enable, fcoe_enable, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(enable, "string");
MODULE_PARM_DESC(enable, " Enables fcoe on a ethernet interface.");
module_param_call(disable, fcoe_disable, NULL, NULL, S_IWUSR);
__MODULE_PARM_TYPE(disable, "string");
MODULE_PARM_DESC(disable, " Disables fcoe on a ethernet interface.");
#endif /* !defined(__VMKLNX__) */


/* notification function for packets from net device */
static struct notifier_block fcoe_notifier = {
	.notifier_call = fcoe_device_notification,
};

#if !defined(__VMKLNX__)
/* notification function for CPU hotplug events */
static struct notifier_block fcoe_cpu_notifier = {
	.notifier_call = fcoe_cpu_callback,
};
#endif /* !defined(__VMKLNX__) */

static struct scsi_transport_template *fcoe_transport_template;
static struct scsi_transport_template *fcoe_vport_transport_template;

#if !defined(__VMKLNX__)
static int fcoe_vport_destroy(struct fc_vport *);
static int fcoe_vport_create(struct fc_vport *, bool disabled);
static int fcoe_vport_disable(struct fc_vport *, bool disable);
static void fcoe_set_vport_symbolic_name(struct fc_vport *);
#endif /* !defined(__VMKLNX__) */


#if defined(__VMKLNX__)
/*
 * Module parameters for software FCOE's queue depth.
 */

#define DEFAULT_LUN_QDEPTH     32
#define DEFAULT_CMD_PER_LUN    3
#define DEFAULT_CAN_QUEUE      FCOE_MAX_OUTSTANDING_COMMANDS
#define DEFAULT_SG_TABLESIZE   SG_ALL

static short           lun_qdepth_param    = DEFAULT_LUN_QDEPTH;
static short           cmd_per_lun_param   = DEFAULT_CMD_PER_LUN;
static int             can_queue_param     = DEFAULT_CAN_QUEUE;
static unsigned short  sg_tablesize_param  = DEFAULT_SG_TABLESIZE;

module_param(lun_qdepth_param,    short,   S_IRWXUGO);
module_param(cmd_per_lun_param,   short,   S_IRWXUGO);
module_param(can_queue_param,     int,     S_IRWXUGO);
module_param(sg_tablesize_param,  ushort,  S_IRWXUGO);

MODULE_PARM_DESC(lun_qdepth_param,
   "'Maximum number of commands to queue on tagged device\n"
   "    (default == DEFAULT_LUN_QDEPTH == 32)");
MODULE_PARM_DESC(cmd_per_lun_param,
   "'Maximum number of commands to queue on untagged device\n"
   "    (default == DEFAULT_CMD_PER_LUN == 3)");
MODULE_PARM_DESC(can_queue_param,
   "Maximum number of commands the host adapter will accept\n"
   "    (default == DEFAULT_CAN_QUEUE == 1024)");
MODULE_PARM_DESC(sg_tablesize_param,
   "Maximum number of scatter gather elements supported by host adapter\n"
   "    (default == DEFAULT_SG_TABLESIZE == 255)");

static   int  open_fcoe_recv(struct sk_buff *skb);
static   int  fip_recv(struct sk_buff *skb);
int fcoe_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
		off_t offset, int length, int func);
static struct vmklnx_fcoe_template open_fcoe_template = {
    .name = "fcoe_sw_esx",
    .fcoe_create = fcoe_create,
    .fcoe_destroy = fcoe_destroy,
    .fcoe_wakeup = fcoe_wakeup,
    .fcoe_recv = open_fcoe_recv,
    .fip_recv = fip_recv,
};

#endif /* defined(__VMKLNX__) */


static struct libfc_function_template fcoe_libfc_fcn_templ = {
	.frame_send = fcoe_xmit,
	.ddp_setup = fcoe_ddp_setup,
	.ddp_done = fcoe_ddp_done,
	.elsct_send = fcoe_elsct_send,
	.get_lesb = fcoe_get_lesb,
#if defined(__VMKLNX__)
	.get_cna_netdev = fcoe_netdev,
#endif /* defined(__VMKLNX__) */
};

struct fc_function_template fcoe_transport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

#if !defined(__VMKLNX__)
	/*
	 * PR632000 NPIV not supported by SW FCoE in M/N
	 */
	.vport_create = fcoe_vport_create,
	.vport_delete = fcoe_vport_destroy,
	.vport_disable = fcoe_vport_disable,
	.set_vport_symbolic_name = fcoe_set_vport_symbolic_name,
#endif /* !defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
	.bsg_request = fc_lport_bsg_request,
#endif /* !defined(__VMKLNX__) */
};

struct fc_function_template fcoe_vport_transport_function = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

#if !defined(__VMKLNX__)
	.bsg_request = fc_lport_bsg_request,
#endif /* !defined(__VMKLNX__) */
};

static struct scsi_host_template fcoe_shost_template = {
	.module = THIS_MODULE,
	.name = "FCoE Driver",
	.proc_name = FCOE_NAME,
	.queuecommand = fc_queuecommand,
	.eh_abort_handler = fc_eh_abort,
	.eh_device_reset_handler = fc_eh_device_reset,
#if defined(__VMKLNX__)
        .eh_bus_reset_handler = vmklnx_fc_eh_bus_reset,
	.slave_configure = fc_slave_configure,
	.proc_info = fcoe_proc_info,
#endif /* defined(__VMKLNX__) */
	.eh_host_reset_handler = fc_eh_host_reset,
	.slave_alloc = fc_slave_alloc,
	.change_queue_depth = fc_change_queue_depth,
	.change_queue_type = fc_change_queue_type,
	.this_id = -1,
	.cmd_per_lun = 3,
	.can_queue = FCOE_MAX_OUTSTANDING_COMMANDS,
	.use_clustering = ENABLE_CLUSTERING,
#if defined (__VMKLNX__)
	.sg_tablesize = DEFAULT_SG_TABLESIZE,
#else /* !defined(__VMKLNX__) */
	.sg_tablesize = SG_ALL,
#endif /* defined(__VMKLNX__) */
	.max_sectors = 0xffff,
};

/**
 * fcoe_interface_setup() - Setup a FCoE interface
 * @fcoe:   The new FCoE interface
 * @netdev: The net device that the fcoe interface is on
 *
 * Returns : 0 for success
 * Locking: must be called with the RTNL mutex held
 */
static int fcoe_interface_setup(struct fcoe_interface *fcoe,
				struct net_device *netdev)
{
	struct fcoe_ctlr *fip = &fcoe->ctlr;
#if !defined(__VMKLNX__)
	struct netdev_hw_addr *ha;
	struct net_device *real_dev;
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	fcoe->netdev = netdev;

	/* Let LLD initialize for FCoE */
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_enable) {
		if (ops->ndo_fcoe_enable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to enable FCoE"
					" specific feature for LLD.\n");
	}

	/* Do not support for bonding device */
	if ((netdev->priv_flags & IFF_MASTER_ALB) ||
	    (netdev->priv_flags & IFF_SLAVE_INACTIVE) ||
	    (netdev->priv_flags & IFF_MASTER_8023AD)) {
		FCOE_NETDEV_DBG(netdev, "Bonded interfaces not supported\n");
		return -EOPNOTSUPP;
	}

	/* look for SAN MAC address, if multiple SAN MACs exist, only
	 * use the first one for SPMA */
	real_dev = (netdev->priv_flags & IFF_802_1Q_VLAN) ?
		vlan_dev_real_dev(netdev) : netdev;
	rcu_read_lock();
	for_each_dev_addr(real_dev, ha) {
		if ((ha->type == NETDEV_HW_ADDR_T_SAN) &&
		    (is_valid_ether_addr(ha->addr))) {
			memcpy(fip->ctl_src_addr, ha->addr, ETH_ALEN);
			fip->spma = 1;
			break;
		}
	}
	rcu_read_unlock();

	/* setup Source Mac Address */
	if (!fip->spma)
		memcpy(fip->ctl_src_addr, netdev->dev_addr, netdev->addr_len);

	/*
	 * Add FCoE MAC address as second unicast MAC address
	 * or enter promiscuous mode if not capable of listening
	 * for multiple unicast MACs.
	 */
	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_unicast_add(netdev, flogi_maddr);
	if (fip->spma)
		dev_unicast_add(netdev, fip->ctl_src_addr);
	dev_mc_add(netdev, FIP_ALL_ENODE_MACS, ETH_ALEN, 0);

	/*
	 * setup the receive function from ethernet driver
	 * on the ethertype for the given device
	 */
	fcoe->fcoe_packet_type.func = fcoe_rcv;
	fcoe->fcoe_packet_type.type = __constant_htons(ETH_P_FCOE);
	fcoe->fcoe_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fcoe_packet_type);

	fcoe->fip_packet_type.func = fcoe_fip_recv;
	fcoe->fip_packet_type.type = htons(ETH_P_FIP);
	fcoe->fip_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fip_packet_type);
#else /* defined(__VMKLNX__) */
	memcpy(fip->ctl_src_addr, netdev->dev_addr, netdev->addr_len);
	fcoe->netdev = netdev;
	netdev->fcoe_ptr = (void *) fcoe;
	fip->spma = 1;
#endif /* !defined(__VMKLNX__) */

	return 0;
}

/**
 * fcoe_interface_create() - Create a FCoE interface on a net device
 * @netdev: The net device to create the FCoE interface on
 *
 * Returns: pointer to a struct fcoe_interface or NULL on error
 */
static struct fcoe_interface *fcoe_interface_create(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	int err;

	fcoe = kzalloc(sizeof(*fcoe), GFP_KERNEL);
	if (!fcoe) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate fcoe structure\n");
		return NULL;
	}

	dev_hold(netdev);
	kref_init(&fcoe->kref);

#if defined(__VMKLNX__)
	fcoe->link_state = FCOE_ST_LINK_DOWN;
#endif /* defined(__VMKLNX__) */

	/*
	 * Initialize FIP.
	 */
	fcoe_ctlr_init(&fcoe->ctlr);
	fcoe->ctlr.send = fcoe_fip_send;
	fcoe->ctlr.update_mac = fcoe_update_src_mac;
	fcoe->ctlr.get_src_addr = fcoe_get_src_mac;

	err = fcoe_interface_setup(fcoe, netdev);
	if (err) {
		fcoe_ctlr_destroy(&fcoe->ctlr);
		kfree(fcoe);
		dev_put(netdev);
		return NULL;
	}

	return fcoe;
}

/**
 * fcoe_interface_cleanup() - Clean up a FCoE interface
 * @fcoe: The FCoE interface to be cleaned up
 *
 * Caller must be holding the RTNL mutex
 */
void fcoe_interface_cleanup(struct fcoe_interface *fcoe)
{
	struct net_device *netdev = fcoe->netdev;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
#if !defined(__VMKLNX__)
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	/*
	 * Don't listen for Ethernet packets anymore.
	 * synchronize_net() ensures that the packet handlers are not running
	 * on another CPU. dev_remove_pack() would do that, this calls the
	 * unsyncronized version __dev_remove_pack() to avoid multiple delays.
	 */
	__dev_remove_pack(&fcoe->fcoe_packet_type);
	__dev_remove_pack(&fcoe->fip_packet_type);
	synchronize_net();

	/* Delete secondary MAC addresses */
	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_unicast_delete(netdev, flogi_maddr);
	if (fip->spma)
		dev_unicast_delete(netdev, fip->ctl_src_addr);
	dev_mc_delete(netdev, FIP_ALL_ENODE_MACS, ETH_ALEN, 0);

	/* Tell the LLD we are done w/ FCoE */
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_disable) {
		if (ops->ndo_fcoe_disable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to disable FCoE"
					" specific feature for LLD.\n");
	}
#else /* defined(__VMKLNX__) */
	if (vmklnx_cna_remove_macaddr(netdev, fip->ctl_src_addr)) {
		printk(KERN_WARNING "fcoe: Unable to remove FCOE Controller MAC filter\n");
	} else {
		printk(KERN_INFO "fcoe: Removed FCOE Controller MAC filter\n");
	}
#endif /* !defined(__VMKLNX__) */
}

/**
 * fcoe_interface_release() - fcoe_port kref release function
 * @kref: Embedded reference count in an fcoe_interface struct
 */
static void fcoe_interface_release(struct kref *kref)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;

	fcoe = container_of(kref, struct fcoe_interface, kref);
	netdev = fcoe->netdev;
	/* tear-down the FCoE controller */
	fcoe_ctlr_destroy(&fcoe->ctlr);
	kfree(fcoe);
#if defined(__VMKLNX__)
	netdev->fcoe_ptr = NULL;
#endif /* defined(__VMKLNX__) */
	dev_put(netdev);
}

/**
 * fcoe_interface_get() - Get a reference to a FCoE interface
 * @fcoe: The FCoE interface to be held
 */
static inline void fcoe_interface_get(struct fcoe_interface *fcoe)
{
	kref_get(&fcoe->kref);
}

/**
 * fcoe_interface_put() - Put a reference to a FCoE interface
 * @fcoe: The FCoE interface to be released
 */
static inline void fcoe_interface_put(struct fcoe_interface *fcoe)
{
	kref_put(&fcoe->kref, fcoe_interface_release);
}

#if !defined(__VMKLNX__)
/**
 * fcoe_fip_recv() - Handler for received FIP frames
 * @skb:      The receive skb
 * @netdev:   The associated net device
 * @ptype:    The packet_type structure which was used to register this handler
 * @orig_dev: The original net_device the the skb was received on.
 *	      (in case dev is a bond)
 *
 * Returns: 0 for success
 */
static int fcoe_fip_recv(struct sk_buff *skb, struct net_device *netdev,
			 struct packet_type *ptype,
			 struct net_device *orig_dev)
{
	struct fcoe_interface *fcoe;

	fcoe = container_of(ptype, struct fcoe_interface, fip_packet_type);
	fcoe_ctlr_recv(&fcoe->ctlr, skb);
	return 0;
}
#endif /* !defined(__VMKLNX__) */
/**
 * fcoe_fip_send() - Send an Ethernet-encapsulated FIP frame
 * @fip: The FCoE controller
 * @skb: The FIP packet to be sent
 */
static void fcoe_fip_send(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	skb->dev = fcoe_from_ctlr(fip)->netdev;

	if (fcoe_start_io(skb)) {
		struct fc_lport *lport = fip->lp;
		fcoe_check_wait_queue(lport, skb);
	}
}

/**
 * fcoe_update_src_mac() - Update the Ethernet MAC filters
 * @lport: The local port to update the source MAC on
 * @addr:  Unicast MAC address to add
 *
 * Remove any previously-set unicast MAC filter.
 * Add secondary FCoE MAC address filter for our OUI.
 */
static void fcoe_update_src_mac(struct fc_lport *lport, u8 *addr)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;

	rtnl_lock();
	if (!is_zero_ether_addr(port->data_src_addr))
		dev_unicast_delete(fcoe->netdev, port->data_src_addr);
	if (!is_zero_ether_addr(addr))
		dev_unicast_add(fcoe->netdev, addr);
	memcpy(port->data_src_addr, addr, ETH_ALEN);
	rtnl_unlock();
}

/**
 * fcoe_get_src_mac() - return the Ethernet source address for an lport
 * @lport: libfc lport
 */
static u8 *fcoe_get_src_mac(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);

	return port->data_src_addr;
}

/**
 * fcoe_lport_config() - Set up a local port
 * @lport: The local port to be setup
 *
 * Returns: 0 for success
 */
static int fcoe_lport_config(struct fc_lport *lport)
{
	lport->link_up = 0;
	lport->qfull = 0;
	lport->max_retry_count = 3;
	lport->max_rport_retry_count = 3;
	lport->e_d_tov = 2 * 1000;	/* FC-FS default */
	lport->r_a_tov = 2 * 2 * 1000;
	lport->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS |
				 FCP_SPPF_RETRY | FCP_SPPF_CONF_COMPL);
	lport->does_npiv = 1;

	fc_lport_init_stats(lport);

	/* lport fc_lport related configuration */
	fc_lport_config(lport);

	/* offload related configuration */
	lport->crc_offload = 0;
	lport->seq_offload = 0;
	lport->lro_enabled = 0;
	lport->lro_xid = 0;
	lport->lso_max = 0;

	return 0;
}

/**
 * fcoe_queue_timer() - The fcoe queue timer
 * @lport: The local port
 *
 * Calls fcoe_check_wait_queue on timeout
 */
static void fcoe_queue_timer(ulong lport)
{
	fcoe_check_wait_queue((struct fc_lport *)lport, NULL);
}


/**
 * fcoe_get_wwn() - Get the world wide name from netdev->perm_addr
 * @netdev: the associated net device
 * @wwn: the output WWN
 * @type: the type of WWN (WWPN or WWNN)
 *
 * Returns: 0 for success
 */
static int fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type)
{
	int rc = -EINVAL;

	/* Check to ensure that we do not populate invalid WWNs */
	if ((strlen(netdev->perm_addr) == ETH_ADDR_LENGTH) ||
	    (strlen(netdev->perm_addr) > (WWN_LENGTH_BYTES * 2)) ||
	    (netdev->perm_addr[0] == 0x0))
	        return rc;

	switch (type) {
	case NETDEV_FCOE_WWPN:
	       memcpy(wwn, netdev->perm_addr, WWN_LENGTH_BYTES);
	       rc = 0;
	       break;
	case NETDEV_FCOE_WWNN:
	       memcpy(wwn, (netdev->perm_addr + WWN_LENGTH_BYTES), WWN_LENGTH_BYTES);
	       rc = 0;
	       break;
	default:
	        break;
	}
	return rc;
}


/**
 * fcoe_netdev_config() - Set up net devive for SW FCoE
 * @lport:  The local port that is associated with the net device
 * @netdev: The associated net device
 *
 * Must be called after fcoe_lport_config() as it will use local port mutex
 *
 * Returns: 0 for success
 */
static int fcoe_netdev_config(struct fc_lport *lport, struct net_device *netdev)
{
	u32 mfs;
	u64 wwnn, wwpn;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	/* Setup lport private data to point to fcoe softc */
	port = lport_priv(lport);
	fcoe = port->fcoe;

	/*
	 * Determine max frame size based on underlying device and optional
	 * user-configured limit.  If the MFS is too low, fcoe_link_ok()
	 * will return 0, so do this first.
	 */
	mfs = netdev->mtu;
	if (netdev->features & NETIF_F_FCOE_MTU)
	{
		mfs = FCOE_MTU;
		FCOE_NETDEV_DBG(netdev, "Supports FCOE_MTU of %d bytes\n", mfs);
	}
	mfs -= (sizeof(struct fcoe_hdr) + sizeof(struct fcoe_crc_eof));
	if (fc_set_mfs(lport, mfs))
		return -EINVAL;

	/* offload features support */

	FCOE_NETDEV_DBG(netdev, "netdev->features = 0x%lx\n", netdev->features );

	if (netdev->features & NETIF_F_SG)
		lport->sg_supp = 1;

	if (netdev->features & NETIF_F_FCOE_CRC) {
		lport->crc_offload = 1;
		FCOE_NETDEV_DBG(netdev, "Supports FCCRC offload\n");
	}
	if (netdev->features & NETIF_F_FSO) {
		lport->seq_offload = 1;
		lport->lso_max = netdev->gso_max_size;
		FCOE_NETDEV_DBG(netdev, "Supports LSO for max len 0x%x\n",
				lport->lso_max);
	}
	if (netdev->fcoe_ddp_xid) {
		lport->lro_enabled = 1;
		lport->lro_xid = netdev->fcoe_ddp_xid;
		FCOE_NETDEV_DBG(netdev, "Supports LRO for max xid 0x%x\n",
				lport->lro_xid);
	}
	skb_queue_head_init(&port->fcoe_pending_queue);
	port->fcoe_pending_queue_active = 0;
	setup_timer(&port->timer, fcoe_queue_timer, (unsigned long)lport);

	if (!lport->vport) {
		if (fcoe_get_wwn(netdev, &wwnn, NETDEV_FCOE_WWNN))
			wwnn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr, 1, 0);
		fc_set_wwnn(lport, wwnn);

		if (fcoe_get_wwn(netdev, &wwpn, NETDEV_FCOE_WWPN))
			wwpn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr, 2, 0);
		fc_set_wwpn(lport, wwpn);
	}

	return 0;
}

/**
 * fcoe_shost_config() - Set up the SCSI host associated with a local port
 * @lport: The local port
 * @shost: The SCSI host to associate with the local port
 * @dev:   The device associated with the SCSI host
 *
 * Must be called after fcoe_lport_config() and fcoe_netdev_config()
 *
 * Returns: 0 for success
 */
static int fcoe_shost_config(struct fc_lport *lport, struct Scsi_Host *shost,
			     struct device *dev)
{
	int rc = 0;
#if defined (__VMKLNX__)
	struct fcoe_port *port = lport_priv(lport);
	unsigned short vlan_tag;
#endif /* defined (__VMKLNX__) */

	/* lport scsi host config */
	lport->host->max_lun = FCOE_MAX_LUN;
	lport->host->max_id = FCOE_MAX_FCP_TARGET;
	lport->host->max_channel = 0;
#if defined (__VMKLNX__)
	lport->host->max_cmd_len = 16;
#endif /* defined (__VMKLNX__) */

	if (lport->vport)
		lport->host->transportt = fcoe_vport_transport_template;
	else
		lport->host->transportt = fcoe_transport_template;

	/* add the new host to the SCSI-ml */
	rc = scsi_add_host(lport->host, dev);
	if (rc) {
		FCOE_NETDEV_DBG(fcoe_netdev(lport), "fcoe_shost_config: "
				"error on scsi_add_host\n");
		return rc;
	}

	if (!lport->vport)
		fc_host_max_npiv_vports(lport->host) = USHORT_MAX;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name);

#if defined (__VMKLNX__)
	/*
	 * If scsi_add_host succeeds, we've registered a SCSI adapter with
	 * the Vmkernel storage stack.  Populate what information is already
	 * known for FCOE adapter attributes.  This enables basic information
	 * to be viewed between now and receiving the FCF and VNPort MAC
	 * addresses.
	 */
	vlan_tag = vmklnx_cna_get_vlan_tag(fcoe_netdev(lport)) & VLAN_VID_MASK;
	vmklnx_init_fcoe_attribs(lport->host,
	                         fcoe_netdev(lport)->name,
	                         vlan_tag,
	                         port->fcoe->ctlr.ctl_src_addr,
	                         NULL,
	                         NULL);
#endif /* defined (__VMKLNX__) */

	return 0;
}

/**
 * fcoe_oem_match() - The match routine for the offloaded exchange manager
 * @fp: The I/O frame
 *
 * This routine will be associated with an exchange manager (EM). When
 * the libfc exchange handling code is looking for an EM to use it will
 * call this routine and pass it the frame that it wishes to send. This
 * routine will return True if the associated EM is to be used and False
 * if the echange code should continue looking for an EM.
 *
 * The offload EM that this routine is associated with will handle any
 * packets that are for SCSI read requests.
 *
 * Returns: True for read types I/O, otherwise returns false.
 */
bool fcoe_oem_match(struct fc_frame *fp)
{
	return fc_fcp_is_read(fr_fsp(fp)) &&
		(fr_fsp(fp)->data_len > fcoe_ddp_min);
}

/**
 * fcoe_em_config() - Allocate and configure an exchange manager
 * @lport: The local port that the new EM will be associated with
 *
 * Returns: 0 on success
 */
static inline int fcoe_em_config(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct fcoe_interface *oldfcoe = NULL;
	struct net_device *old_real_dev, *cur_real_dev;
	u16 min_xid = FCOE_MIN_XID;
	u16 max_xid = fcoe_max_xid;

	/*
	 * Check if need to allocate an em instance for
	 * offload exchange ids to be shared across all VN_PORTs/lport.
	 */
	if (!lport->lro_enabled || !lport->lro_xid ||
	    (lport->lro_xid >= max_xid)) {
		lport->lro_xid = 0;
		goto skip_oem;
	}

	/*
	 * Reuse existing offload em instance in case
	 * it is already allocated on real eth device
	 */
	if (fcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
		cur_real_dev = vlan_dev_real_dev(fcoe->netdev);
	else
		cur_real_dev = fcoe->netdev;

	list_for_each_entry(oldfcoe, &fcoe_hostlist, list) {
		if (oldfcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
			old_real_dev = vlan_dev_real_dev(oldfcoe->netdev);
		else
			old_real_dev = oldfcoe->netdev;

		if (cur_real_dev == old_real_dev) {
			fcoe->oem = oldfcoe->oem;
			break;
		}
	}

	if (fcoe->oem) {
		if (!fc_exch_mgr_add(lport, fcoe->oem, fcoe_oem_match)) {
			printk(KERN_ERR "fcoe_em_config: failed to add "
			       "offload em:%p on interface:%s\n",
			       fcoe->oem, fcoe->netdev->name);
			return -ENOMEM;
		}
	} else {
		fcoe->oem = fc_exch_mgr_alloc(lport, FC_CLASS_3,
					      FCOE_MIN_XID, lport->lro_xid,
					      fcoe_oem_match);
		if (!fcoe->oem) {
			printk(KERN_ERR "fcoe_em_config: failed to allocate "
			       "em for offload exches on interface:%s\n",
			       fcoe->netdev->name);
			return -ENOMEM;
		}
	}

	/*
	 * Exclude offload EM xid range from next EM xid range.
	 */
	min_xid += lport->lro_xid + 1;

skip_oem:
	if (!fc_exch_mgr_alloc(lport, FC_CLASS_3, min_xid, max_xid, NULL)) {
		printk(KERN_ERR "fcoe_em_config: failed to "
		       "allocate em on interface %s\n", fcoe->netdev->name);
		return -ENOMEM;
	}

	return 0;
}

/**
 * fcoe_if_destroy() - Tear down a SW FCoE instance
 * @lport: The local port to be destroyed
 */
static void fcoe_if_destroy(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct net_device *netdev = fcoe->netdev;

	FCOE_NETDEV_DBG(netdev, "Destroying interface\n");

	/* Logout of the fabric */
	fc_fabric_logoff(lport);

	/* Cleanup the fc_lport */
	fc_lport_destroy(lport);

	/* Stop the transmit retry timer */
	del_timer_sync(&port->timer);

	/* Free existing transmit skbs */
	fcoe_clean_pending_queue(lport);

	rtnl_lock();
#if !defined(__VMKLNX__)
	if (!is_zero_ether_addr(port->data_src_addr))
		dev_unicast_delete(netdev, port->data_src_addr);
#else /* defined(__VMKLNX__) */
	if (!is_zero_ether_addr(port->data_src_addr)) {
		if (vmklnx_cna_remove_macaddr(netdev, port->data_src_addr)) {
			printk(KERN_WARNING "fcoe: Unable to remove the MAC filter\n");
		} else {
			printk(KERN_INFO "fcoe: Removed the MAC filter\n");
		}
	}
#endif /* !defined(__VMKLNX__) */
	rtnl_unlock();

	/* receives may not be stopped until after this */
	fcoe_interface_put(fcoe);

	/* Free queued packets for the per-CPU receive threads */
	fcoe_percpu_clean(lport);

	/* Detach from the scsi-ml */
	fc_remove_host(lport->host);
#if defined(__VMKLNX__)
        vmklnx_scsi_remove_cna(lport->host, netdev);
#endif /* defined(__VMKLNX__) */

	scsi_remove_host(lport->host);

	/* Destroy lport scsi_priv */
	fc_fcp_destroy(lport);

	/* There are no more rports or I/O, free the EM */
	fc_exch_mgr_free(lport);

	/* Free memory used by statistical counters */
	fc_lport_free_stats(lport);

	/* Release the Scsi_Host */
	scsi_host_put(lport->host);
	module_put(THIS_MODULE);
}

/**
 * fcoe_ddp_setup() - Call a LLD's ddp_setup through the net device
 * @lport: The local port to setup DDP for
 * @xid:   The exchange ID for this DDP transfer
 * @sgl:   The scatterlist describing this transfer
 * @sgc:   The number of sg items
 *
 * Returns: 0 if the DDP context was not configured
 */
static int fcoe_ddp_setup(struct fc_lport *lport, u16 xid,
			  struct scatterlist *sgl, unsigned int sgc)
{
	struct net_device *netdev = fcoe_netdev(lport);

#if !defined(__VMKLNX__)
	if (netdev->netdev_ops->ndo_fcoe_ddp_setup)
		return netdev->netdev_ops->ndo_fcoe_ddp_setup(netdev,
							      xid, sgl,
							      sgc);
#else /* defined(__VMKLNX__) */
	if (netdev->ndo_fcoe_ddp_setup) {
		return netdev->ndo_fcoe_ddp_setup(netdev, xid, sgl, sgc);
	} else {
		printk(KERN_ERR ">> fcoe_ddp_setup(): netdev->ndo_fcoe_ddp_setup = NULL\n");
	}
#endif /* !defined(__VMKLNX__) */
	return 0;
}

/**
 * fcoe_ddp_done() - Call a LLD's ddp_done through the net device
 * @lport: The local port to complete DDP on
 * @xid:   The exchange ID for this DDP transfer
 *
 * Returns: the length of data that have been completed by DDP
 */
static int fcoe_ddp_done(struct fc_lport *lport, u16 xid)
{
	struct net_device *netdev = fcoe_netdev(lport);

#if !defined(__VMKLNX__)
	if (netdev->netdev_ops->ndo_fcoe_ddp_done)
	{
		return netdev->netdev_ops->ndo_fcoe_ddp_done(netdev, xid);
	}
#else /* defined(__VMKLNX__) */
	if( netdev->ndo_fcoe_ddp_done ) {
		return netdev->ndo_fcoe_ddp_done(netdev, xid);
	}
#endif /* !defined(__VMKLNX__) */

	return 0;
}

/**
 * fcoe_if_create() - Create a FCoE instance on an interface
 * @fcoe:   The FCoE interface to create a local port on
 * @parent: The device pointer to be the parent in sysfs for the SCSI host
 * @npiv:   Indicates if the port is a vport or not
 *
 * Creates a fc_lport instance and a Scsi_Host instance and configure them.
 *
 * Returns: The allocated fc_lport or an error pointer
 */
static struct fc_lport *fcoe_if_create(struct fcoe_interface *fcoe,
				       struct device *parent, int npiv)
{
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *lport = NULL;
	struct fcoe_port *port;
	struct Scsi_Host *shost;
	int rc;
	/*
	 * parent is only a vport if npiv is 1,
	 * but we'll only use vport in that case so go ahead and set it
	 */
	struct fc_vport *vport = dev_to_vport(parent);

	FCOE_NETDEV_DBG(netdev, "Create Interface\n");

	if (!npiv) {
		lport = libfc_host_alloc(&fcoe_shost_template,
					 sizeof(struct fcoe_port));
	} else	{
		lport = libfc_vport_create(vport,
					   sizeof(struct fcoe_port));
	}
	if (!lport) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate host structure\n");
		rc = -ENOMEM;
		goto out;
	}
	shost = lport->host;
	port = lport_priv(lport);
	port->lport = lport;
	port->fcoe = fcoe;
	INIT_WORK(&port->destroy_work, fcoe_destroy_work);

	/* configure a fc_lport including the exchange manager */
	rc = fcoe_lport_config(lport);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure lport for the "
				"interface\n");
		goto out_host_put;
	}

	if (npiv) {
		FCOE_NETDEV_DBG(netdev, "Setting vport names, "
				"%16.16llx %16.16llx\n",
				vport->node_name, vport->port_name);
		fc_set_wwnn(lport, vport->node_name);
		fc_set_wwpn(lport, vport->port_name);
	}

	/* configure lport network properties */
	rc = fcoe_netdev_config(lport, netdev);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure netdev for the "
				"interface\n");
		goto out_lp_destroy;
	}
#if defined(__VMKLNX__)
        /*
         * Set the transport types as FCoE
         */
        shost->xportFlags = VMKLNX_SCSI_TRANSPORT_TYPE_FCOE;
#endif /* defined(__VMKLNX__) */

	/* configure lport scsi host properties */
#if !defined(__VMKLNX__)
	rc = fcoe_shost_config(lport, shost, parent);
#else /* defined(__VMKLNX__) */
	device_initialize(&port->dummy_dev);
	rc = fcoe_shost_config(lport, shost, &port->dummy_dev);
#endif /* !defined(__VMKLNX__) */
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure shost for the "
				"interface\n");
		goto out_lp_destroy;
	}
#if defined(__VMKLNX__)
        vmklnx_scsi_attach_cna(lport->host, netdev);
#endif /* defined(__VMKLNX__) */

	/* Initialize the library */
	rc = fcoe_libfc_config(lport, &fcoe_libfc_fcn_templ);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure libfc for the "
				"interface\n");
		goto out_lp_destroy;
	}

	if (!npiv) {
		/*
		 * fcoe_em_alloc() and fcoe_hostlist_add() both
		 * need to be atomic with respect to other changes to the
		 * hostlist since fcoe_em_alloc() looks for an existing EM
		 * instance on host list updated by fcoe_hostlist_add().
		 *
		 * This is currently handled through the fcoe_config_mutex
		 * begin held.
		 */

		/* lport exch manager allocation */
		rc = fcoe_em_config(lport);
		if (rc) {
			FCOE_NETDEV_DBG(netdev, "Could not configure the EM "
					"for the interface\n");
			goto out_lp_destroy;
		}
	}

	fcoe_interface_get(fcoe);
	return lport;

out_lp_destroy:
	fc_exch_mgr_free(lport);
out_host_put:
	scsi_host_put(lport->host);
out:
	return ERR_PTR(rc);
}

#if defined(__VMKLNX__)
/*
 * fcoe_init_queue_params - use module-load parameters for queue settings
 *
 * Use module parameters for software FCOE's queue depth.
 */
static void fcoe_init_queue_params(void)
{
	libfc_lun_qdepth = lun_qdepth_param;
	fcoe_shost_template.cmd_per_lun = cmd_per_lun_param;
	fcoe_shost_template.can_queue = can_queue_param;
	fcoe_shost_template.sg_tablesize = sg_tablesize_param;

	printk(KERN_DEBUG
	       "fcoe_init_queue_params: lun_qdepth(%d) cmd_per_lun(%d) "
	       "can_queue(%d) sg(%d)\n",
	       libfc_lun_qdepth,
	       fcoe_shost_template.cmd_per_lun,
	       fcoe_shost_template.can_queue,
	       fcoe_shost_template.sg_tablesize);
}
#endif /* defined (__VMKLNX__) */

/**
 * fcoe_if_init() - Initialization routine for fcoe.ko
 *
 * Attaches the SW FCoE transport to the FC transport
 *
 * Returns: 0 on success
 */
static int __init fcoe_if_init(void)
{
#if defined(__VMKLNX__)
	fcoe_init_queue_params();
#endif /* defined(__VMKLNX__) */

	/* attach to scsi transport */
	fcoe_transport_template = fc_attach_transport(&fcoe_transport_function);
	fcoe_vport_transport_template =
		fc_attach_transport(&fcoe_vport_transport_function);

	if (!fcoe_transport_template) {
		printk(KERN_ERR "fcoe: Failed to attach to the FC transport\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * fcoe_if_exit() - Tear down fcoe.ko
 *
 * Detaches the SW FCoE transport from the FC transport
 *
 * Returns: 0 on success
 */
int __exit fcoe_if_exit(void)
{
	fc_release_transport(fcoe_transport_template);
	fc_release_transport(fcoe_vport_transport_template);
	fcoe_transport_template = NULL;
	fcoe_vport_transport_template = NULL;
	return 0;
}

/**
 * fcoe_percpu_thread_create() - Create a receive thread for an online CPU
 * @cpu: The CPU index of the CPU to create a receive thread for
 */
static void fcoe_percpu_thread_create(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(fcoe_percpu, cpu);

	thread = kthread_create(fcoe_percpu_receive_thread,
				(void *)p, "fcoethread/%d", cpu);

	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		wake_up_process(thread);

		spin_lock_bh(&p->fcoe_rx_list.lock);
		p->thread = thread;
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
}

/**
 * fcoe_percpu_thread_destroy() - Remove the receive thread of a CPU
 * @cpu: The CPU index of the CPU whose receive thread is to be destroyed
 *
 * Destroys a per-CPU Rx thread. Any pending skbs are moved to the
 * current CPU's Rx thread. If the thread being destroyed is bound to
 * the CPU processing this context the skbs will be freed.
 */
static void fcoe_percpu_thread_destroy(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;
	struct page *crc_eof;
	struct sk_buff *skb;
#ifdef CONFIG_SMP
	struct fcoe_percpu_s *p0;
	unsigned targ_cpu = get_cpu();
#endif /* CONFIG_SMP */

	FCOE_DBG("Destroying receive thread for CPU %d\n", cpu);

	/* Prevent any new skbs from being queued for this CPU. */
	p = &per_cpu(fcoe_percpu, cpu);
	spin_lock_bh(&p->fcoe_rx_list.lock);
	thread = p->thread;
	p->thread = NULL;
	crc_eof = p->crc_eof_page;
	p->crc_eof_page = NULL;
	p->crc_eof_offset = 0;
	spin_unlock_bh(&p->fcoe_rx_list.lock);

#ifdef CONFIG_SMP
	/*
	 * Don't bother moving the skb's if this context is running
	 * on the same CPU that is having its thread destroyed. This
	 * can easily happen when the module is removed.
	 */
	if (cpu != targ_cpu) {
		p0 = &per_cpu(fcoe_percpu, targ_cpu);
		spin_lock_bh(&p0->fcoe_rx_list.lock);
		if (p0->thread) {
			FCOE_DBG("Moving frames from CPU %d to CPU %d\n",
				 cpu, targ_cpu);

			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				__skb_queue_tail(&p0->fcoe_rx_list, skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		} else {
			/*
			 * The targeted CPU is not initialized and cannot accept
			 * new	skbs. Unlock the targeted CPU and drop the skbs
			 * on the CPU that is going offline.
			 */
			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				kfree_skb(skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		}
	} else {
		/*
		 * This scenario occurs when the module is being removed
		 * and all threads are being destroyed. skbs will continue
		 * to be shifted from the CPU thread that is being removed
		 * to the CPU thread associated with the CPU that is processing
		 * the module removal. Once there is only one CPU Rx thread it
		 * will reach this case and we will drop all skbs and later
		 * stop the thread.
		 */
		spin_lock_bh(&p->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
			kfree_skb(skb);
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
	put_cpu();
#else
	/*
	 * This a non-SMP scenario where the singular Rx thread is
	 * being removed. Free all skbs and stop the thread.
	 */
	spin_lock_bh(&p->fcoe_rx_list.lock);
	while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
		kfree_skb(skb);
	spin_unlock_bh(&p->fcoe_rx_list.lock);
#endif

	if (thread)
		kthread_stop(thread);

	if (crc_eof)
		put_page(crc_eof);
}

#if !defined(__VMKLNX__)
/**
 * fcoe_cpu_callback() - Handler for CPU hotplug events
 * @nfb:    The callback data block
 * @action: The event triggering the callback
 * @hcpu:   The index of the CPU that the event is for
 *
 * This creates or destroys per-CPU data for fcoe
 *
 * Returns NOTIFY_OK always.
 */
static int fcoe_cpu_callback(struct notifier_block *nfb,
			     unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		FCOE_DBG("CPU %x online: Create Rx thread\n", cpu);
		fcoe_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		FCOE_DBG("CPU %x offline: Remove Rx thread\n", cpu);
		fcoe_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}
#endif /* !defined(__VMKLNX__) */
/**
 * fcoe_rcv() - Receive packets from a net device
 * @skb:    The received packet
 * @netdev: The net device that the packet was received on
 * @ptype:  The packet type context
 * @olddev: The last device net device
 *
 * This routine is called by NET_RX_SOFTIRQ. It receives a packet, builds a
 * FC frame and passes the frame to libfc.
 *
 * Returns: 0 for success
 */
int fcoe_rcv(struct sk_buff *skb, struct net_device *netdev,
	     struct packet_type *ptype, struct net_device *olddev)
{
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_interface *fcoe;
	struct fc_frame_header *fh;
	struct fcoe_percpu_s *fps;
	struct fcoe_port *port;
	struct ethhdr *eh;
	unsigned int cpu;

	fcoe = container_of(ptype, struct fcoe_interface, fcoe_packet_type);
	lport = fcoe->ctlr.lp;
	if (unlikely(!lport)) {
		FCOE_NETDEV_DBG(netdev, "Cannot find hba structure\n");
		goto err2;
	}
	if (!lport->link_up)
		goto err2;

	FCOE_NETDEV_DBG(netdev, "skb_info: len:%d data_len:%d head:%p "
			"data:%p tail:%p end:%p sum:%d dev:%s\n",
			skb->len, skb->data_len, skb->head, skb->data,
			skb_tail_pointer(skb), skb_end_pointer(skb),
			skb->csum, skb->dev ? skb->dev->name : "<NULL>");

	/*
	 * Added on 10/06/2011 from open-fcoe.org (fcoe-next.git)
	 * Version: Linux 2.6.36
	 */

	/* check for mac addresses */
	eh = eth_hdr(skb);
	port = lport_priv(lport);

	if (compare_ether_addr(eh->h_dest, port->data_src_addr) &&
		 compare_ether_addr(eh->h_dest, fcoe->ctlr.ctl_src_addr) &&
		 compare_ether_addr(eh->h_dest, (u8[6])FC_FCOE_FLOGI_MAC)) {
		FCOE_NETDEV_DBG(netdev, "wrong destination mac address:%pM\n",
							 eh->h_dest);
		goto err;
	}

	if (is_fip_mode(&fcoe->ctlr) &&
		 compare_ether_addr(eh->h_source, fcoe->ctlr.dest_addr)) {
		 FCOE_NETDEV_DBG(netdev, "wrong source mac address:%pM\n",
							  eh->h_source);
		 goto err;
	}

	/*
	 * Check for minimum frame length, and make sure required FCoE
	 * and FC headers are pulled into the linear data area.
	 */
	if (unlikely((skb->len < FCOE_MIN_FRAME) ||
		     !pskb_may_pull(skb, FCOE_HEADER_LEN)))
	{
		FCOE_NETDEV_DBG(netdev, "skb->len = %d\n", skb->len);
		goto err;
	}


	skb_set_transport_header(skb, sizeof(struct fcoe_hdr));
	fh = (struct fc_frame_header *) skb_transport_header(skb);

	fr = fcoe_dev_from_skb(skb);
	fr->fr_dev = lport;
	fr->ptype = ptype;

	/*
	 * In case the incoming frame's exchange is originated from
	 * the initiator, then received frame's exchange id is ANDed
	 * with fc_cpu_mask bits to get the same cpu on which exchange
	 * was originated, otherwise just use the current cpu.
	 */
	if (ntoh24(fh->fh_f_ctl) & FC_FC_EX_CTX) {
		cpu = ntohs(fh->fh_ox_id) & fc_cpu_mask;
#if defined(__VMKLNX__)
		/*
		 * Make sure the CPU ID derived from the OXID is valid.
		 * Otherwise, just put this on the current CPU.
		 */
		if (cpu >= nr_cpu_ids) {
			FCOE_DBG("Detected invalid CPU(%d), move to CPU(%d)\n",
				cpu, smp_processor_id());
			cpu = smp_processor_id();
		}
#endif /* defined(__VMKLNX__) */
	} else
		cpu = smp_processor_id();

	fps = &per_cpu(fcoe_percpu, cpu);
	spin_lock_bh(&fps->fcoe_rx_list.lock);
	if (unlikely(!fps->thread)) {
#if !defined(__VMKLNX__)
		/*
		 * The targeted CPU is not ready, let's target
		 * the first CPU now. For non-SMP systems this
		 * will check the same CPU twice.
		 */
		FCOE_NETDEV_DBG(netdev, "CPU is online, but no receive thread "
				"ready for incoming skb- using first online "
				"CPU.\n");

		spin_unlock_bh(&fps->fcoe_rx_list.lock);
		cpu = cpumask_first(cpu_online_mask);
		fps = &per_cpu(fcoe_percpu, cpu);
		spin_lock_bh(&fps->fcoe_rx_list.lock);
		if (!fps->thread) {
			spin_unlock_bh(&fps->fcoe_rx_list.lock);
			goto err;
		}
#else  /* !defined(__VMKLNX__) */
		panic("fcoe_rcv running without per-cpu initialization\n");
#endif /* !defined(__VMKLNX__) */
	}

	/*
	 * We now have a valid CPU that we're targeting for
	 * this skb. We also have this receive thread locked,
	 * so we're free to queue skbs into it's queue.
	 */

	/* If this is a SCSI-FCP frame, and this is already executing on the
	 * correct CPU, and the queue for this CPU is empty, then go ahead
	 * and process the frame directly in the softirq context.
	 * This lets us process completions without context switching from the
	 * NET_RX softirq, to our receive processing thread, and then back to
	 * BLOCK softirq context.
	 */
	if (fh->fh_type == FC_TYPE_FCP &&
	    cpu == smp_processor_id() &&
	    skb_queue_empty(&fps->fcoe_rx_list)) {
		spin_unlock_bh(&fps->fcoe_rx_list.lock);
		fcoe_recv_frame(skb);
	} else {
		__skb_queue_tail(&fps->fcoe_rx_list, skb);
		if (fps->fcoe_rx_list.qlen == 1)
			wake_up_process(fps->thread);
		spin_unlock_bh(&fps->fcoe_rx_list.lock);
	}

	return 0;
err:
#if defined(__VMKLNX__)
	FCOE_PER_CPU_PTR(lport->dev_stats,
	                 get_cpu(),
	                 sizeof(struct fcoe_dev_stats))->ErrorFrames++;
#else
	per_cpu_ptr(lport->dev_stats, get_cpu())->ErrorFrames++;
#endif
	put_cpu();
err2:
	kfree_skb(skb);
	return -1;
}

/**
 * fcoe_start_io() - Start FCoE I/O
 * @skb: The packet to be transmitted
 *
 * This routine is called from the net device to start transmitting
 * FCoE packets.
 *
 * Returns: 0 for success
 */
static inline int fcoe_start_io(struct sk_buff *skb)
{
	struct sk_buff *nskb;
	int rc;

	nskb = skb_clone(skb, GFP_ATOMIC);
	rc = dev_queue_xmit(nskb);
#if !defined(__VMKLNX__)
	if (rc != 0)
		return rc;
#else /* defined(__VMKLNX__) */
        if (rc != 0) {
            /*
             * Decrement the reference here. vmklnx_cna_queue_xmit
             * does not decrement reference on error.
             */
            kfree_skb(skb); 
            FCOE_DBG("fcoe: fcoe_tx failed for %s\n",
                   skb->dev->name);
            return rc;
        }
#endif /* defined(__VMKLNX__) */
	kfree_skb(skb);
	return 0;
}

/**
 * fcoe_get_paged_crc_eof() - Allocate a page to be used for the trailer CRC
 * @skb:  The packet to be transmitted
 * @tlen: The total length of the trailer
 *
 * This routine allocates a page for frame trailers. The page is re-used if
 * there is enough room left on it for the current trailer. If there isn't
 * enough buffer left a new page is allocated for the trailer. Reference to
 * the page from this function as well as the skbs using the page fragments
 * ensure that the page is freed at the appropriate time.
 *
 * Returns: 0 for success
 */
static int fcoe_get_paged_crc_eof(struct sk_buff *skb, int tlen)
{
	struct fcoe_percpu_s *fps;
	struct page *page;

	fps = &get_cpu_var(fcoe_percpu);
	page = fps->crc_eof_page;
	if (!page) {
		page = alloc_page(GFP_ATOMIC);
		if (!page) {
			put_cpu_var(fcoe_percpu);
			return -ENOMEM;
		}
		fps->crc_eof_page = page;
		fps->crc_eof_offset = 0;
	}

	/* get_page is currently not defined. Was previously a no-op */
	get_page(page);
	skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags, page,
			   fps->crc_eof_offset, tlen);
	skb->len += tlen;
	skb->data_len += tlen;
	skb->truesize += tlen;
	fps->crc_eof_offset += sizeof(struct fcoe_crc_eof);

	if (fps->crc_eof_offset >= PAGE_SIZE) {
		fps->crc_eof_page = NULL;
		fps->crc_eof_offset = 0;
		put_page(page);
	}
	put_cpu_var(fcoe_percpu);
	return 0;
}

/**
 * fcoe_fc_crc() - Calculates the CRC for a given frame
 * @fp: The frame to be checksumed
 *
 * This uses crc32() routine to calculate the CRC for a frame
 *
 * Return: The 32 bit CRC value
 */
u32 fcoe_fc_crc(struct fc_frame *fp)
{
	struct sk_buff *skb = fp_skb(fp);
	struct skb_frag_struct *frag;
	unsigned char *data;
	unsigned long off, len, clen;
	u32 crc;
	unsigned i;

	crc = crc32(~0, skb->data, skb_headlen(skb));

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		off = frag->page_offset;
		len = frag->size;
		while (len > 0) {
			clen = min(len, PAGE_SIZE - (off & ~PAGE_MASK));
#if !defined(__VMKLNX__)
			data = kmap_atomic(frag->page + (off >> PAGE_SHIFT),
					   KM_SKB_DATA_SOFTIRQ);
#else /* defined(__VMKLNX__) */
			data = kmap_atomic(nth_page(frag->page, (off >> PAGE_SHIFT)),
					   KM_SKB_DATA_SOFTIRQ);
#endif /* !defined(__VMKLNX__) */
			crc = crc32(crc, data + (off & ~PAGE_MASK), clen);
			kunmap_atomic(data, KM_SKB_DATA_SOFTIRQ);
			off += clen;
			len -= clen;
		}
	}
	return crc;
}

/**
 * fcoe_xmit() - Transmit a FCoE frame
 * @lport: The local port that the frame is to be transmitted for
 * @fp:	   The frame to be transmitted
 *
 * Return: 0 for success
 */
int fcoe_xmit(struct fc_lport *lport, struct fc_frame *fp)
{
	int wlen;
	u32 crc;
	struct ethhdr *eh;
	struct fcoe_crc_eof *cp;
	struct sk_buff *skb;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	unsigned int hlen;		/* header length implies the version */
	unsigned int tlen;		/* trailer length */
	unsigned int elen;		/* eth header, may include vlan */
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	u8 sof, eof;
	struct fcoe_hdr *hp;

	WARN_ON((fr_len(fp) % sizeof(u32)) != 0);

	fh = fc_frame_header_get(fp);
	skb = fp_skb(fp);
	wlen = skb->len / FCOE_WORD_TO_BYTE;

#if 0 && defined(__VMKLNX__) /* PR 580593 */
#ifdef VMX86_DEBUG
	/*
	 * Drop and free fragmented skbs sporadically to catch FRAGSOWNER bugs.
	 */
#define FCOE_XMIT_STRESS_DROP_RATE 102400
	static long fcoeXmitStressDrop = 0;

	if ((skb_shinfo(skb)->nr_frags) &&
	    (++fcoeXmitStressDrop == FCOE_XMIT_STRESS_DROP_RATE)) {
		printk(KERN_WARNING "fcoe_xmit: Stress dropping skb "
		                    "(nr_frags %d, vmklnx_frags_owner %d)\n",
		                    skb_shinfo(skb)->nr_frags,
		                    vmklnx_is_skb_frags_owner(skb));
		fcoeXmitStressDrop = 0;
		kfree_skb(skb);
		return 0;
	}
#endif /* VMX86_DEBUG */
#endif /* defined(__VMKLNX__) */
	if (!lport->link_up) {
		kfree_skb(skb);
		return 0;
	}

	if (unlikely(fh->fh_type == FC_TYPE_ELS) &&
	    fcoe_ctlr_els_send(&fcoe->ctlr, lport, skb))
		return 0;

	sof = fr_sof(fp);
	eof = fr_eof(fp);

	elen = sizeof(struct ethhdr);
	hlen = sizeof(struct fcoe_hdr);
	tlen = sizeof(struct fcoe_crc_eof);
	wlen = (skb->len - tlen + sizeof(crc)) / FCOE_WORD_TO_BYTE;

	/* crc offload */
	if (likely(lport->crc_offload)) {
		skb->ip_summed = CHECKSUM_PARTIAL;
#if !defined(__VMKLNX__)
		skb->csum_start = skb_headroom(skb);
		skb->csum_offset = skb->len;
#endif /* !defined(__VMKLNX__) */
		crc = 0;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
		crc = fcoe_fc_crc(fp);
	}

	/* copy port crc and eof to the skb buff */
	if (skb_is_nonlinear(skb)) {
		skb_frag_t *frag;
		if (fcoe_get_paged_crc_eof(skb, tlen)) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		frag = &skb_shinfo(skb)->frags[skb_shinfo(skb)->nr_frags - 1];
		cp = kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ)
			+ frag->page_offset;
	} else {
		cp = (struct fcoe_crc_eof *)skb_put(skb, tlen);
	}

#if  !defined(__VMKLNX__)
	memset(cp, 0, sizeof(*cp));
#else
	*(uint16_t *)(cp->fcoe_resvd) = 0;
	cp->fcoe_resvd[2] = 0;
#endif
	cp->fcoe_eof = eof;
	cp->fcoe_crc32 = cpu_to_le32(~crc);

	if (skb_is_nonlinear(skb)) {
		kunmap_atomic(cp, KM_SKB_DATA_SOFTIRQ);
		cp = NULL;
	}

	/* adjust skb network/transport offsets to match mac/fcoe/port */
	skb_push(skb, elen + hlen);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
#if !defined(__VMKLNX__)
	skb->mac_len = elen;
#endif /* !defined(__VMKLNX__) */
	skb->protocol = htons(ETH_P_FCOE);
	skb->dev = fcoe->netdev;

	/* fill up mac and fcoe headers */
	eh = eth_hdr(skb);
	eh->h_proto = htons(ETH_P_FCOE);
	if (fcoe->ctlr.map_dest)
		fc_fcoe_set_mac(eh->h_dest, fh->fh_d_id);
	else
		/* insert GW address */
		memcpy(eh->h_dest, fcoe->ctlr.dest_addr, ETH_ALEN);

	if (unlikely(fcoe->ctlr.flogi_oxid != FC_XID_UNKNOWN))
		memcpy(eh->h_source, fcoe->ctlr.ctl_src_addr, ETH_ALEN);
	else
		memcpy(eh->h_source, port->data_src_addr, ETH_ALEN);

	hp = (struct fcoe_hdr *)(eh + 1);
#if  !defined(__VMKLNX__)
	memset(hp, 0, sizeof(*hp));
#else
	*(uint64_t *)hp->fcoe_resvd = 0;
	*(uint32_t *)(hp->fcoe_resvd + 8) = 0;
	hp->fcoe_ver = 0;
#endif
	if (FC_FCOE_VER)
		FC_FCOE_ENCAPS_VER(hp, FC_FCOE_VER);
	hp->fcoe_sof = sof;

	/* fcoe lso, mss is in max_payload which is non-zero for FCP data */
	if (lport->seq_offload && fr_max_payload(fp)) {
		skb_shinfo(skb)->gso_type = SKB_GSO_FCOE;
		skb_shinfo(skb)->gso_size = fr_max_payload(fp);
	} else {
		skb_shinfo(skb)->gso_type = 0;
		skb_shinfo(skb)->gso_size = 0;
	}
	/* update tx stats: regardless if LLD fails */
#if defined(__VMKLNX__)
	stats = FCOE_PER_CPU_PTR(lport->dev_stats,
	                         get_cpu(),
	                         sizeof(struct fcoe_dev_stats));
#else
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
#endif
	stats->TxFrames++;
	stats->TxWords += wlen;
	put_cpu();

	/* send down to lld */
	fr_dev(fp) = lport;
	if (port->fcoe_pending_queue.qlen)
		fcoe_check_wait_queue(lport, skb);
	else if (fcoe_start_io(skb))
		fcoe_check_wait_queue(lport, skb);

	return 0;
}

/**
 * fcoe_percpu_flush_done() - Indicate per-CPU queue flush completion
 * @skb: The completed skb (argument required by destructor)
 */
#if !defined(__VMKLNX__)
static void fcoe_percpu_flush_done(struct sk_buff *skb)
#else /* defined(__VMKLNX__) */
static void fcoe_percpu_flush_done(void)
#endif /* !defined(__VMKLNX__) */
{
	complete(&fcoe_flush_completion);
}

/**
 * fcoe_recv_frame() - process a single received frame
 * @skb: frame to process
 */
static void fcoe_recv_frame(struct sk_buff *skb)
{
	u32 fr_len;
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	struct fcoe_crc_eof crc_eof;
	struct fc_frame *fp;
	u8 *mac = NULL;
	struct fcoe_port *port;
	struct fcoe_hdr *hp;

	fr = fcoe_dev_from_skb(skb);
	lport = fr->fr_dev;
	if (unlikely(!lport)) {
#if !defined(__VMKLNX__)
		if (skb->destructor != fcoe_percpu_flush_done)
			FCOE_NETDEV_DBG(skb->dev, "NULL lport in skb\n");
#else /* defined(__VMKLNX__) */
		if (skb != flush_skb) {
			FCOE_NETDEV_DBG(skb->dev, "NULL lport in skb\n");
		}
		if (skb == flush_skb) {
			fcoe_percpu_flush_done();
		}
#endif /* !defined(__VMKLNX__) */

		kfree_skb(skb);
		return;
	}

#if defined(__VMKLNX__)
	FCOE_NETDEV_DBG(skb->dev, "fcoe_recv_frame: skb_info: len:%d data_len:%d "
#else /* !defined(__VMKLNX__) */
	FCOE_NETDEV_DBG(skb->dev, "skb_info: len:%d data_len:%d "
#endif /* defined(__VMKLNX__) */
			"head:%p data:%p tail:%p end:%p sum:%d dev:%s\n",
			skb->len, skb->data_len,
			skb->head, skb->data, skb_tail_pointer(skb),
			skb_end_pointer(skb), skb->csum,
			skb->dev ? skb->dev->name : "<NULL>");

	/*
	 * Save source MAC address before discarding header.
	 */
	port = lport_priv(lport);
	if (skb_is_nonlinear(skb))
		skb_linearize(skb);	/* not ideal */
	mac = eth_hdr(skb)->h_source;

	/*
	 * Frame length checks and setting up the header pointers
	 * was done in fcoe_rcv already.
	 */
#if !defined(__VMKLNX__)
	hp = (struct fcoe_hdr *) skb_network_header(skb);
	fh = (struct fc_frame_header *) skb_transport_header(skb);
#else /* !defined(__VMKLNX__) */
	hp = (struct fcoe_hdr *)skb->data;
#endif /* !defined(__VMKLNX__) */


#if defined(__VMKLNX__)
	stats = FCOE_PER_CPU_PTR(lport->dev_stats,
	                         get_cpu(),
	                         sizeof(struct fcoe_dev_stats));
#else
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
#endif
	if (unlikely(FC_FCOE_DECAPS_VER(hp) != FC_FCOE_VER)) {
		if (stats->ErrorFrames < 5)
			printk(KERN_WARNING "fcoe: FCoE version "
			       "mismatch: The frame has "
			       "version %x, but the "
			       "initiator supports version "
			       "%x\n", FC_FCOE_DECAPS_VER(hp),
			       FC_FCOE_VER);
		goto drop;
	}

	skb_pull(skb, sizeof(struct fcoe_hdr));
	fr_len = skb->len - sizeof(struct fcoe_crc_eof);

	stats->RxFrames++;
	stats->RxWords += fr_len / FCOE_WORD_TO_BYTE;

	fp = (struct fc_frame *)skb;
	fc_frame_init(fp);
	fr_dev(fp) = lport;
	fr_sof(fp) = hp->fcoe_sof;

	/* Copy out the CRC and EOF trailer for access */
	if (skb_copy_bits(skb, fr_len, &crc_eof, sizeof(crc_eof)))
		goto drop;
	fr_eof(fp) = crc_eof.fcoe_eof;
	fr_crc(fp) = crc_eof.fcoe_crc32;
	if (pskb_trim(skb, fr_len))
		goto drop;

	/*
	 * We only check CRC if no offload is available and if it is
	 * it's solicited data, in which case, the FCP layer would
	 * check it during the copy.
	 */
	if (lport->crc_offload &&
	    skb->ip_summed == CHECKSUM_UNNECESSARY)
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
	else
		fr_flags(fp) |= FCPHF_CRC_UNCHECKED;

	fh = fc_frame_header_get(fp);
	if (fh->fh_r_ctl != FC_RCTL_DD_SOL_DATA ||
	    fh->fh_type != FC_TYPE_FCP) {

	    if (fr_flags(fp) & FCPHF_CRC_UNCHECKED) {
		if (le32_to_cpu(fr_crc(fp)) !=
		    ~crc32(~0, skb->data, fr_len)) {
			if (stats->InvalidCRCCount < 5)
				printk(KERN_WARNING "fcoe: dropping "
				       "frame with CRC error\n");
			stats->InvalidCRCCount++;
			goto drop;
		}
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
	    }

	    struct fcoe_interface *fcoe = ((struct fcoe_port *)lport_priv(lport))->fcoe;
	    if ( (fcoe->ctlr.state == FIP_ST_ENABLED) &&
		fc_frame_payload_op(fp) == ELS_LOGO && ntoh24(fh->fh_s_id) == FC_FID_FLOGI) {
		FCOE_DBG("fcoe: dropping FCoE lport LOGO in fip mode\n");
		goto drop;
	    }
	}

	put_cpu();
	fc_exch_recv(lport, fp);
	return;

drop:
	stats->ErrorFrames++;
	put_cpu();
	kfree_skb(skb);
}

/**
 * fcoe_percpu_receive_thread() - The per-CPU packet receive thread
 * @arg: The per-CPU context
 *
 * Return: 0 for success
 */
int fcoe_percpu_receive_thread(void *arg)
{
	struct fcoe_percpu_s *p = arg;
	struct sk_buff *skb;

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {

		spin_lock_bh(&p->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&p->fcoe_rx_list)) == NULL) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_bh(&p->fcoe_rx_list.lock);
			schedule();
			set_current_state(TASK_RUNNING);
			if (kthread_should_stop())
				return 0;
			spin_lock_bh(&p->fcoe_rx_list.lock);
		}
		spin_unlock_bh(&p->fcoe_rx_list.lock);
		fcoe_recv_frame(skb);
	}
	return 0;
}

/**
 * fcoe_check_wait_queue() - Attempt to clear the transmit backlog
 * @lport: The local port whose backlog is to be cleared
 *
 * This empties the wait_queue, dequeues the head of the wait_queue queue
 * and calls fcoe_start_io() for each packet. If all skb have been
 * transmitted it returns the qlen. If an error occurs it restores
 * wait_queue (to try again later) and returns -1.
 *
 * The wait_queue is used when the skb transmit fails. The failed skb
 * will go in the wait_queue which will be emptied by the timer function or
 * by the next skb transmit.
 */
static void fcoe_check_wait_queue(struct fc_lport *lport, struct sk_buff *skb)
{
	struct fcoe_port *port = lport_priv(lport);
	int rc;

	spin_lock_bh(&port->fcoe_pending_queue.lock);

	if (skb)
		__skb_queue_tail(&port->fcoe_pending_queue, skb);

	if (port->fcoe_pending_queue_active)
		goto out;
	port->fcoe_pending_queue_active = 1;

	while (port->fcoe_pending_queue.qlen) {
		/* keep qlen > 0 until fcoe_start_io succeeds */
		port->fcoe_pending_queue.qlen++;
		skb = __skb_dequeue(&port->fcoe_pending_queue);

		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		rc = fcoe_start_io(skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);

		if (rc) {
			__skb_queue_head(&port->fcoe_pending_queue, skb);
			/* undo temporary increment above */
			port->fcoe_pending_queue.qlen--;
			break;
		}
		/* undo temporary increment above */
		port->fcoe_pending_queue.qlen--;
	}

	if (port->fcoe_pending_queue.qlen < FCOE_LOW_QUEUE_DEPTH)
		lport->qfull = 0;
	if (port->fcoe_pending_queue.qlen && !timer_pending(&port->timer))
		mod_timer(&port->timer, jiffies + 2);
	port->fcoe_pending_queue_active = 0;
out:
	if (port->fcoe_pending_queue.qlen > FCOE_MAX_QUEUE_DEPTH)
		lport->qfull = 1;
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
	return;
}

/**
 * fcoe_dev_setup() - Setup the link change notification interface
 */
static void fcoe_dev_setup(void)
{
	register_netdevice_notifier(&fcoe_notifier);
}

/**
 * fcoe_dev_cleanup() - Cleanup the link change notification interface
 */
static void fcoe_dev_cleanup(void)
{
	unregister_netdevice_notifier(&fcoe_notifier);
}

#if defined(__VMKLNX__)
/**
 * fcoe_link_state_string() - Helper function to convert the link state
 *	into a string.
 */
static char *
fcoe_link_state_string(fcoe_link_state state)
{
	char *string;

	switch(state) {
	case FCOE_ST_LINK_DOWN:
		string = "LINK_DOWN";
		break;
	case FCOE_ST_LINK_DORMANT:
		string = "LINK_DORMANT";
		break;
	case FCOE_ST_LINK_UP:
		string = "LINK_UP";
		break;
	default:
		string = "INVALID_STATE";
	}

	return string;
}
#endif /* defined(__VMKLNX__) */

/**
 * fcoe_device_notification() - Handler for net device events
 * @notifier: The context of the notification
 * @event:    The type of event
 * @ptr:      The net device that the event was on
 *
 * This function is called by the Ethernet driver in case of link change event.
 *
 * Returns: 0 for success
 */
static int fcoe_device_notification(struct notifier_block *notifier,
				    ulong event, void *ptr)
{
	struct fc_lport *lport = NULL;
	struct net_device *netdev = ptr;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;
	struct fcoe_dev_stats *stats;
	u32 link_possible = 1;
	u32 mfs;
	int rc = NOTIFY_OK;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {

#if defined(__VMKLNX__)
		/*
		 * Link up/down notifications come in on behalf
		 * of actual net_device, not the CNA net_device.
		 */
		if (!strcmp(fcoe->netdev->name, netdev->name))
#else /* !defined(__VMKLNX__) */
                if (fcoe->netdev == netdev)
#endif /* defined(__VMKLNX__) */
		{
			lport = fcoe->ctlr.lp;
			break;
		}
	}
	if (!lport) {
		rc = NOTIFY_DONE;
		goto out;
	}

#if defined(__VMKLNX__)
	FCOE_LKST_DBG("link state change for %s event %ld\n",
		      fcoe->netdev->name, event);
#endif /* defined(__VMKLNX__) */

	switch (event) {
	case NETDEV_DOWN:
	case NETDEV_GOING_DOWN:
		link_possible = 0;
		break;
	case NETDEV_UP:
	case NETDEV_CHANGE:
		break;
	case NETDEV_CHANGEMTU:
		if (netdev->features & NETIF_F_FCOE_MTU)
			break;
		mfs = netdev->mtu - (sizeof(struct fcoe_hdr) +
				     sizeof(struct fcoe_crc_eof));
		if (mfs >= FC_MIN_MAX_FRAME)
			fc_set_mfs(lport, mfs);
		break;
	case NETDEV_REGISTER:
		break;
	case NETDEV_UNREGISTER:
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
#if !defined(__VMKLNX__)
		fcoe_interface_cleanup(fcoe);
#endif /* !defined(__VMKLNX__) */
		schedule_work(&port->destroy_work);
		goto out;
		break;
	default:
		FCOE_NETDEV_DBG(netdev, "Unknown event %ld "
				"from netdev netlink\n", event);
	}
	if (link_possible && !fcoe_link_ok(lport)) {
#if defined(__VMKLNX__)
		/*
		 * Do not handle the link up here.
		 * Put the link into DORMANT state for now
		 * and wait for the DCBX negotiation to finish.
		 */
		if (fcoe->link_state != FCOE_ST_LINK_UP) {
			FCOE_LKST_DBG("%s link state change: %s->%s\n",
				 fcoe->netdev->name,
				 fcoe_link_state_string(fcoe->link_state),
				 fcoe_link_state_string(FCOE_ST_LINK_DORMANT));
			fcoe->link_state = FCOE_ST_LINK_DORMANT;
		}
#else
		fcoe_ctlr_link_up(&fcoe->ctlr);
#endif /* defined(__VMKLNX__) */
	} else if (fcoe_ctlr_link_down(&fcoe->ctlr)) {
#if defined(__VMKLNX__)
		FCOE_LKST_DBG("%s link state change: %s->%s\n",
			      fcoe->netdev->name,
			      fcoe_link_state_string(fcoe->link_state),
			      fcoe_link_state_string(FCOE_ST_LINK_DOWN));
		fcoe->link_state = FCOE_ST_LINK_DOWN;
		stats = FCOE_PER_CPU_PTR(lport->dev_stats,
		                         get_cpu(),
		                         sizeof(struct fcoe_dev_stats));
#else
		stats = per_cpu_ptr(lport->dev_stats, get_cpu());
#endif /* defined(__VMKLNX__) */
		stats->LinkFailureCount++;
		put_cpu();
		fcoe_clean_pending_queue(lport);
	}
out:
	return rc;
}

#if defined(__VMKLNX__)
/**
 * fcoe_wakeup() - Wakeup fcoe device from the dormant state.
 * @netdev: net_device handle
 *
 * This function transitions the fcoe device link state from DORMANT
 * to LINK_UP and proceed with LINK_UP processing.
 *
 * Returns: 0 for success
 *          -EAGAIN for wakeup is called when L2 link is down
            -ENODEV for no fcoe interface found
 */
static int fcoe_wakeup(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	int rtn = 0;

	dev_hold(netdev);

	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	if (!fcoe) {
		FCOE_DBG("Cannot find the FCOE interface %s\n",
		         netdev->name);
		rtnl_unlock();
		dev_put(netdev);
		return (-ENODEV);
	}

	/*
	 * DCBX negotiation is done.
	 * Make the transition from DORMANT to LINK_UP
	 * Handle link up process and FLOGI.
	 */
	if (fcoe->link_state != FCOE_ST_LINK_UP) {
	    struct fc_lport *lport = fcoe->ctlr.lp;
	    if (!fcoe_link_ok(lport)) {
		fcoe_ctlr_link_up(&fcoe->ctlr);
		FCOE_LKST_DBG("%s link state change: %s->%s\n",
			      netdev->name,
			      fcoe_link_state_string(fcoe->link_state),
			      fcoe_link_state_string(FCOE_ST_LINK_UP));
		fcoe->link_state = FCOE_ST_LINK_UP;
	    } else {
		FCOE_LKST_DBG("Cannot wakeup %s since the link is down\n",
			      netdev->name);
		rtn = -EAGAIN;
	    }
	} else {
		FCOE_LKST_DBG("Waking up is called while %s is in %s\n",
			      netdev->name,
			      fcoe_link_state_string(fcoe->link_state));
	}

	rtnl_unlock();
	dev_put(netdev);

	return (rtn);
}
#endif /* defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
/**
 * fcoe_if_to_netdev() - Parse a name buffer to get a net device
 * @buffer: The name of the net device
 *
 * Returns: NULL or a ptr to net_device
 */
static struct net_device *fcoe_if_to_netdev(const char *buffer)
{
	char *cp;
	char ifname[IFNAMSIZ + 2];

	if (buffer) {
		strlcpy(ifname, buffer, IFNAMSIZ);
		cp = ifname + strlen(ifname);
		while (--cp >= ifname && *cp == '\n')
			*cp = '\0';
		return dev_get_by_name(&init_net, ifname);
	}
	return NULL;
}


/**
 * fcoe_disable() - Disables a FCoE interface
 * @buffer: The name of the Ethernet interface to be disabled
 * @kp:	    The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_disable(const char *buffer, struct kernel_param *kp)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (fcoe)
		fc_fabric_logoff(fcoe->ctlr.lp);
	else
		rc = -ENODEV;

	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_enable() - Enables a FCoE interface
 * @buffer: The name of the Ethernet interface to be enabled
 * @kp:     The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
static int fcoe_enable(const char *buffer, struct kernel_param *kp)
{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

	netdev = fcoe_if_to_netdev(buffer);
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}

	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (fcoe)
		rc = fc_fabric_login(fcoe->ctlr.lp);
	else
		rc = -ENODEV;

	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}
#endif /* !defined(__VMKLNX__) */


/**
 * fcoe_destroy() - Destroy a FCoE interface
 * @buffer: The name of the Ethernet interface to be destroyed
 * @kp:	    The associated kernel parameter
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
#if !defined(__VMKLNX__)
static int fcoe_destroy(const char *buffer, struct kernel_param *kp)
#else /* defined(__VMKLNX__) */
static int fcoe_destroy(struct net_device *netdevice)
#endif /* !defined(__VMKLNX__) */

{
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nodev;
	}
#endif

#if !defined(__VMKLNX__)
	netdev = fcoe_if_to_netdev(buffer);
#else /* defined(__VMKLNX__) */
	netdev = netdevice;
#endif /* !defined(__VMKLNX__) */
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}
#if defined(__VMKLNX__)
	dev_hold(netdev);
#endif /* defined(__VMKLNX__) */

	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	if (!fcoe) {
		rtnl_unlock();
		rc = -ENODEV;
		goto out_putdev;
	}
	list_del(&fcoe->list);
	fcoe_interface_cleanup(fcoe);
	rtnl_unlock();
	fcoe_if_destroy(fcoe->ctlr.lp);

out_putdev:
	dev_put(netdev);
out_nodev:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_destroy_work() - Destroy a FCoE port in a deferred work context
 * @work: Handle to the FCoE port to be destroyed
 */
static void fcoe_destroy_work(struct work_struct *work)
{
	struct fcoe_port *port;

	port = container_of(work, struct fcoe_port, destroy_work);
	mutex_lock(&fcoe_config_mutex);
	fcoe_if_destroy(port->lport);
	mutex_unlock(&fcoe_config_mutex);
}

/**
 * fcoe_create() - Create a fcoe interface
 * @buffer: The name of the Ethernet interface to create on
 * @kp:	    The associated kernel param
 *
 * Called from sysfs.
 *
 * Returns: 0 for success
 */
#if !defined(__VMKLNX__)
static int fcoe_create(const char *buffer, struct kernel_param *kp)
#else /* defined(__VMKLNX__) */
static int fcoe_create(struct net_device *netdevice)
#endif /* !defined(__VMKLNX__) */
{
	int rc;
	struct fcoe_interface *fcoe;
	struct fc_lport *lport;
	struct net_device *netdev;

	mutex_lock(&fcoe_config_mutex);
#ifdef CONFIG_FCOE_MODULE
	/*
	 * Make sure the module has been initialized, and is not about to be
	 * removed.  Module paramter sysfs files are writable before the
	 * module_init function is called and after module_exit.
	 */
	if (THIS_MODULE->state != MODULE_STATE_LIVE) {
		rc = -ENODEV;
		goto out_nomod;
	}
#endif

	if (!try_module_get(THIS_MODULE)) {
		rc = -EINVAL;
		goto out_nomod;
	}

	rtnl_lock();
#if !defined(__VMKLNX__)
	netdev = fcoe_if_to_netdev(buffer);
#else /* defined(__VMKLNX__) */
	netdev = netdevice;
#endif /* !defined(__VMKLNX__) */
	if (!netdev) {
		rc = -ENODEV;
		goto out_nodev;
	}
#if defined(__VMKLNX__)
	dev_hold(netdev);
#endif /* defined(__VMKLNX__) */

	/* look for existing lport */
	if (fcoe_hostlist_lookup(netdev)) {
		rc = -EEXIST;
		goto out_putdev;
	}

	fcoe = fcoe_interface_create(netdev);
	if (!fcoe) {
		rc = -ENOMEM;
		goto out_putdev;
	}
#if !defined(__VMKLNX__)
	lport = fcoe_if_create(fcoe, &netdev->dev, 0);
#else /* defined(__VMKLNX__) */
	/*
	 * Use NULL for the parent struct device; handled in fcoe_if_create.
	 */
	lport = fcoe_if_create(fcoe, NULL, 0);
#endif /* !defined(__VMKLNX__) */

	if (IS_ERR(lport)) {
		printk(KERN_ERR "fcoe: Failed to create interface (%s)\n",
		       netdev->name);
		rc = -EIO;
		fcoe_interface_cleanup(fcoe);
		goto out_free;
	}

	/* Make this the "master" N_Port */
	fcoe->ctlr.lp = lport;

	/* add to lports list */
	fcoe_hostlist_add(lport);

	/* start FIP Discovery and FLOGI */
	lport->boot_time = jiffies;
	fc_fabric_login(lport);
	if (!fcoe_link_ok(lport)) {
#if defined(__VMKLNX__)
		/*
		 * If the L2 link is up, put the foce interface
		 * into dormant state and wait for the wakeup from dcbd.
		 * For SW FCOE, we need to wait for the DCB
		 * negotiation complete before proceeding
		 * FIP discovery.
		 */
		fcoe->link_state = FCOE_ST_LINK_DORMANT;
        } else {
		fcoe->link_state = FCOE_ST_LINK_DOWN;
#else
		fcoe_ctlr_link_up(&fcoe->ctlr);
#endif /* defined(__VMKLNX__) */
        }

	/*
	 * Release from init in fcoe_interface_create(), on success lport
	 * should be holding a reference taken in fcoe_if_create().
	 */
	fcoe_interface_put(fcoe);
	dev_put(netdev);
	rtnl_unlock();
	mutex_unlock(&fcoe_config_mutex);

	return 0;
out_free:
	fcoe_interface_put(fcoe);
out_putdev:
	dev_put(netdev);
out_nodev:
	rtnl_unlock();
	module_put(THIS_MODULE);
out_nomod:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

/**
 * fcoe_link_ok() - Check if the link is OK for a local port
 * @lport: The local port to check link on
 *
 * Any permanently-disqualifying conditions have been previously checked.
 * This also updates the speed setting, which may change with link for 100/1000.
 *
 * This function should probably be checking for PAUSE support at some point
 * in the future. Currently Per-priority-pause is not determinable using
 * ethtool, so we shouldn't be restrictive until that problem is resolved.
 *
 * Returns: 0 if link is OK for use by FCoE.
 *
 */
int fcoe_link_ok(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct net_device *netdev = port->fcoe->netdev;

#if !defined(__VMKLNX__)
	struct ethtool_cmd ecmd = { ETHTOOL_GSET };

	if ((netdev->flags & IFF_UP) && netif_carrier_ok(netdev) &&
	    (!dev_ethtool_get_settings(netdev, &ecmd))) {
		lport->link_supported_speeds &=
			~(FC_PORTSPEED_1GBIT | FC_PORTSPEED_10GBIT);
		if (ecmd.supported & (SUPPORTED_1000baseT_Half |
				      SUPPORTED_1000baseT_Full))
			lport->link_supported_speeds |= FC_PORTSPEED_1GBIT;
		if (ecmd.supported & SUPPORTED_10000baseT_Full)
			lport->link_supported_speeds |=
				FC_PORTSPEED_10GBIT;
		if (ecmd.speed == SPEED_1000)
			lport->link_speed = FC_PORTSPEED_1GBIT;
		if (ecmd.speed == SPEED_10000)
			lport->link_speed = FC_PORTSPEED_10GBIT;

		return 0;
	}
#else /* defined(__VMKLNX__) */
	if ((netdev->flags & IFF_UP) && netif_carrier_ok(netdev)) {
		int err;
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };

		VMKAPI_MODULE_CALL(netdev->module_id, err,
			netdev->ethtool_ops->get_settings,
			netdev, &ecmd);

		if (!err) {
			lport->link_supported_speeds &=
			    ~(FC_PORTSPEED_1GBIT | FC_PORTSPEED_10GBIT);
			if (ecmd.supported & (SUPPORTED_1000baseT_Half |
				      SUPPORTED_1000baseT_Full))
				lport->link_supported_speeds |=
						    FC_PORTSPEED_1GBIT;
			if (ecmd.supported & SUPPORTED_10000baseT_Full)
				lport->link_supported_speeds |=
						    FC_PORTSPEED_10GBIT;
			if (ecmd.speed == SPEED_1000)
				lport->link_speed = FC_PORTSPEED_1GBIT;
			if (ecmd.speed == SPEED_10000)
				lport->link_speed = FC_PORTSPEED_10GBIT;
		}

		FCOE_LKST_DBG("%s: IFF_UP %d, netif_carrier %d\n",
		              __FUNCTION__,
		              netdev->flags & IFF_UP,
		              netif_carrier_ok(netdev));
		return 0;
	}
#endif /* !defined(__VMKLNX__) */

	return -1;
}

/**
 * fcoe_percpu_clean() - Clear all pending skbs for an local port
 * @lport: The local port whose skbs are to be cleared
 *
 * Must be called with fcoe_create_mutex held to single-thread completion.
 *
 * This flushes the pending skbs by adding a new skb to each queue and
 * waiting until they are all freed.  This assures us that not only are
 * there no packets that will be handled by the lport, but also that any
 * threads already handling packet have returned.
 */
void fcoe_percpu_clean(struct fc_lport *lport)
{
	struct fcoe_percpu_s *pp;
	struct fcoe_rcv_info *fr;
	struct sk_buff_head *list;
	struct sk_buff *skb, *next;
	struct sk_buff *head;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		pp = &per_cpu(fcoe_percpu, cpu);
		spin_lock_bh(&pp->fcoe_rx_list.lock);
		list = &pp->fcoe_rx_list;
		head = list->next;
		for (skb = head; skb != (struct sk_buff *)list;
		     skb = next) {
			next = skb->next;
			fr = fcoe_dev_from_skb(skb);
			if (fr->fr_dev == lport) {
				__skb_unlink(skb, list);
				kfree_skb(skb);
			}
		}

		if (!pp->thread || !cpu_online(cpu)) {
			spin_unlock_bh(&pp->fcoe_rx_list.lock);
			continue;
		}

		skb = dev_alloc_skb(0);
		if (!skb) {
			spin_unlock_bh(&pp->fcoe_rx_list.lock);
			continue;
		}
#if defined(__VMKLNX__)
		/*
		 * The OpenFCOE stack uses the zero-length skb as a sentinel
		 * for plunging out RX queues.  The code relies upon the fact
		 * that Linux zeroes out the beginning portion of its skbs
		 * (skb->cb in particular).  Vmklinux only selectively zeroes
		 * portions of alloc'd skbs.
		 */
		memset(skb->cb, 0, sizeof(skb->cb));
		flush_skb = skb;
#else /* !defined(__VMKLNX__) */
		skb->destructor = fcoe_percpu_flush_done;
#endif /* defined(__VMKLNX__) */

		__skb_queue_tail(&pp->fcoe_rx_list, skb);
		if (pp->fcoe_rx_list.qlen == 1)
			wake_up_process(pp->thread);
		spin_unlock_bh(&pp->fcoe_rx_list.lock);

		wait_for_completion(&fcoe_flush_completion);

#if defined(__VMKLNX__)
		flush_skb = NULL;
#endif /* defined(__VMKLNX__) */
	}
}

/**
 * fcoe_clean_pending_queue() - Dequeue a skb and free it
 * @lport: The local port to dequeue a skb on
 */
void fcoe_clean_pending_queue(struct fc_lport *lport)
{
	struct fcoe_port  *port = lport_priv(lport);
	struct sk_buff *skb;

	spin_lock_bh(&port->fcoe_pending_queue.lock);
	while ((skb = __skb_dequeue(&port->fcoe_pending_queue)) != NULL) {
		spin_unlock_bh(&port->fcoe_pending_queue.lock);
		kfree_skb(skb);
		spin_lock_bh(&port->fcoe_pending_queue.lock);
	}
	spin_unlock_bh(&port->fcoe_pending_queue.lock);
}

/**
 * fcoe_reset() - Reset a local port
 * @shost: The SCSI host associated with the local port to be reset
 *
 * Returns: Always 0 (return value required by FC transport template)
 */
int fcoe_reset(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);
	fc_lport_reset(lport);
	return 0;
}

/**
 * fcoe_hostlist_lookup_port() - Find the FCoE interface associated with a net device
 * @netdev: The net device used as a key
 *
 * Locking: Must be called with the RNL mutex held.
 *
 * Returns: NULL or the FCoE interface
 */
static struct fcoe_interface *
fcoe_hostlist_lookup_port(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev == netdev)
			return fcoe;
	}
	return NULL;
}

/**
 * fcoe_hostlist_lookup() - Find the local port associated with a
 *			    given net device
 * @netdev: The netdevice used as a key
 *
 * Locking: Must be called with the RTNL mutex held
 *
 * Returns: NULL or the local port
 */
static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	fcoe = fcoe_hostlist_lookup_port(netdev);
	return (fcoe) ? fcoe->ctlr.lp : NULL;
}

/**
 * fcoe_hostlist_add() - Add the FCoE interface identified by a local
 *			 port to the hostlist
 * @lport: The local port that identifies the FCoE interface to be added
 *
 * Locking: must be called with the RTNL mutex held
 *
 * Returns: 0 for success
 */
static int fcoe_hostlist_add(const struct fc_lport *lport)
{
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	fcoe = fcoe_hostlist_lookup_port(fcoe_netdev(lport));
	if (!fcoe) {
		port = lport_priv(lport);
		fcoe = port->fcoe;
		list_add_tail(&fcoe->list, &fcoe_hostlist);
	}
	return 0;
}

/**
 * fcoe_init() - Initialize fcoe.ko
 *
 * Returns: 0 on success, or a negative value on failure
 */
static int __init fcoe_init(void)
{
#if defined(__VMKLNX__)
	VMK_ReturnStatus vmkStat;
	vmk_LogProperties logProps;
#endif /* defined(__VMKLNX__) */
	struct fcoe_percpu_s *p;
	unsigned int cpu;
	int rc = 0;

#if defined(__VMKLNX__)
	if ((fcoe_percpu =
	     fcoe_alloc_percpu(sizeof(struct fcoe_percpu_s), 0)) == NULL) {
		return -1;
	}
#endif /* defined(__VMKLNX__) */

	mutex_lock(&fcoe_config_mutex);

	for_each_possible_cpu(cpu) {
		p = &per_cpu(fcoe_percpu, cpu);
		skb_queue_head_init(&p->fcoe_rx_list);
	}

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_create(cpu);

	/* Initialize per CPU interrupt thread */
#if !defined(__VMKLNX__)
	rc = register_hotcpu_notifier(&fcoe_cpu_notifier);
	if (rc)
		goto out_free;
#endif /* !defined(__VMKLNX__) */

	/* Setup link change notification */
	fcoe_dev_setup();

	rc = fcoe_if_init();
	if (rc)
		goto out_free;
#if defined(__VMKLNX__)
	if (fcoe_attach_transport(&open_fcoe_template)) {
		printk(KERN_ERR "FCoE Template failed to register.\n");
	} else {
		printk(KERN_ERR "FCoE Template registered!\n");
	}
#endif /* defined(__VMKLNX__) */

	mutex_unlock(&fcoe_config_mutex);
#if defined(__VMKLNX__)
	vmkStat = vmk_NameInitialize(&logProps.name, FCOE_MODULE_NAME);
	VMK_ASSERT(vmkStat == VMK_OK);
	logProps.module = vmklnx_this_module_id;
	logProps.heap = vmk_ModuleGetHeapID(vmklnx_this_module_id);
	logProps.defaultLevel = 0;
	logProps.throttle = NULL;
	vmkStat = vmk_LogRegister(&logProps, &fcoeLog);
	if (vmkStat != VMK_OK) {
		printk(KERN_ERR "FCoE vmk_LogRegister failed: %s.\n",
			vmk_StatusToString(vmkStat));
		goto out_free;
	}
	vmk_LogSetCurrentLogLevel(fcoeLog, fcoe_debug_logging);
#endif /* defined(__VMKLNX__) */

	return 0;

out_free:
	for_each_online_cpu(cpu) {
		fcoe_percpu_thread_destroy(cpu);
	}
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}
module_init(fcoe_init);

/**
 * fcoe_exit() - Clean up fcoe.ko
 *
 * Returns: 0 on success or a  negative value on failure
 */
static void __exit fcoe_exit(void)
{
	struct fcoe_interface *fcoe, *tmp;
	struct fcoe_port *port;
	unsigned int cpu;

	mutex_lock(&fcoe_config_mutex);

#if defined (__VMKLNX__)
        if (fcoe_release_transport(&open_fcoe_template)) {
           printk(KERN_ERR "FCoE Template failed to unregister.\n");
        } else {
           printk(KERN_ERR "FCoE Template unregistered\n.");
        }
#endif /* defined (__VMKLNX__) */

	fcoe_dev_cleanup();

	/* releases the associated fcoe hosts */
	rtnl_lock();
	list_for_each_entry_safe(fcoe, tmp, &fcoe_hostlist, list) {
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
#if !defined(__VMKLNX__)
		fcoe_interface_cleanup(fcoe);
#endif /* !defined(__VMKLNX__) */
		schedule_work(&port->destroy_work);
	}
	rtnl_unlock();

#if !defined(__VMKLNX__)
	unregister_hotcpu_notifier(&fcoe_cpu_notifier);
#endif /* !defined(__VMKLNX__) */

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_destroy(cpu);

	mutex_unlock(&fcoe_config_mutex);

	/* flush any asyncronous interface destroys,
	 * this should happen after the netdev notifier is unregistered */
	flush_scheduled_work();
	/* That will flush out all the N_Ports on the hostlist, but now we
	 * may have NPIV VN_Ports scheduled for destruction */
	flush_scheduled_work();

	/* detach from scsi transport
	 * must happen after all destroys are done, therefor after the flush */
	fcoe_if_exit();

#if defined(__VMKLNX__)
	vmk_LogUnregister(fcoeLog);
	kfree(fcoe_percpu);
#endif /* defined(__VMKLNX__) */

}
module_exit(fcoe_exit);

/**
 * fcoe_flogi_resp() - FCoE specific FLOGI and FDISC response handler
 * @seq: active sequence in the FLOGI or FDISC exchange
 * @fp: response frame, or error encoded in a pointer (timeout)
 * @arg: pointer the the fcoe_ctlr structure
 *
 * This handles MAC address managment for FCoE, then passes control on to
 * the libfc FLOGI response handler.
 */
static void fcoe_flogi_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	u8 *mac;
#if defined(__VMKLNX__)
	unsigned short vlan_tag = 0;
#endif /* defined(__VMKLNX__) */

	if (IS_ERR(fp))
		goto done;

	mac = fr_cb(fp)->granted_mac;
	if (is_zero_ether_addr(mac)) {
		/* pre-FIP */
		if (fcoe_ctlr_recv_flogi(fip, lport, fp)) {
			fc_frame_free(fp);
			return;
		}
	}
	fcoe_update_src_mac(lport, mac);
#if defined(__VMKLNX__)
	if (!is_zero_ether_addr(fr_cb(fp)->granted_mac)) {
		if (vmklnx_cna_set_macaddr(fp->skb.dev,
		                           fr_cb(fp)->granted_mac)) {
			printk(KERN_WARNING "fcoe: Unable to set MAC filter\n");
		} else {
			printk(KERN_INFO "fcoe: Set the MAC filter\n");
		}
	} else {
		printk(KERN_INFO "fcoe: Not setting zero granted MAC filter\n");
	}

	vlan_tag = vmklnx_cna_get_vlan_tag(fcoe_netdev(lport)) & VLAN_VID_MASK;

	vmklnx_init_fcoe_attribs(lport->host,
	                         fcoe_netdev(lport)->name,
	                         vlan_tag,
	                         fip->ctl_src_addr,
	                         fr_cb(fp)->granted_mac,
	                         fip->dest_addr);
#endif /* defined(__VMKLNX__) */
done:
	fc_lport_flogi_resp(seq, fp, lport);
}

/**
 * fcoe_logo_resp() - FCoE specific LOGO response handler
 * @seq: active sequence in the LOGO exchange
 * @fp: response frame, or error encoded in a pointer (timeout)
 * @arg: pointer the the fcoe_ctlr structure
 *
 * This handles MAC address managment for FCoE, then passes control on to
 * the libfc LOGO response handler.
 */
static void fcoe_logo_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_lport *lport = arg;
	static u8 zero_mac[ETH_ALEN] = { 0 };

	if (!IS_ERR(fp))
		fcoe_update_src_mac(lport, zero_mac);
	fc_lport_logo_resp(seq, fp, lport);
}

/**
 * fcoe_elsct_send - FCoE specific ELS handler
 *
 * This does special case handling of FIP encapsualted ELS exchanges for FCoE,
 * using FCoE specific response handlers and passing the FIP controller as
 * the argument (the lport is still available from the exchange).
 *
 * Most of the work here is just handed off to the libfc routine.
 */
static struct fc_seq *fcoe_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct fc_frame_header *fh = fc_frame_header_get(fp);

	switch (op) {
	case ELS_FLOGI:
	case ELS_FDISC:
		return fc_elsct_send(lport, did, fp, op, fcoe_flogi_resp,
				     fip, timeout);
	case ELS_LOGO:
		/* only hook onto fabric logouts, not port logouts */
		if (ntoh24(fh->fh_d_id) != FC_FID_FLOGI)
			break;
		return fc_elsct_send(lport, did, fp, op, fcoe_logo_resp,
				     lport, timeout);
	}
	return fc_elsct_send(lport, did, fp, op, resp, arg, timeout);
}

#if !defined(__VMKLNX__)
/**
 * fcoe_vport_create() - create an fc_host/scsi_host for a vport
 * @vport: fc_vport object to create a new fc_host for
 * @disabled: start the new fc_host in a disabled state by default?
 *
 * Returns: 0 for success
 */
static int fcoe_vport_create(struct fc_vport *vport, bool disabled)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fcoe_port *port = lport_priv(n_port);
	struct fcoe_interface *fcoe = port->fcoe;
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *vn_port;

	mutex_lock(&fcoe_config_mutex);
	vn_port = fcoe_if_create(fcoe, &vport->dev, 1);
	mutex_unlock(&fcoe_config_mutex);

	if (IS_ERR(vn_port)) {
		printk(KERN_ERR "fcoe: fcoe_vport_create(%s) failed\n",
		       netdev->name);
		return -EIO;
	}

	if (disabled) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
	} else {
		vn_port->boot_time = jiffies;
		fc_fabric_login(vn_port);
		fc_vport_setlink(vn_port);
	}
	return 0;
}

/**
 * fcoe_vport_destroy() - destroy the fc_host/scsi_host for a vport
 * @vport: fc_vport object that is being destroyed
 *
 * Returns: 0 for success
 */
static int fcoe_vport_destroy(struct fc_vport *vport)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fc_lport *vn_port = vport->dd_data;
	struct fcoe_port *port = lport_priv(vn_port);

	mutex_lock(&n_port->lp_mutex);
	list_del(&vn_port->list);
	mutex_unlock(&n_port->lp_mutex);
	schedule_work(&port->destroy_work);
	return 0;
}

/**
 * fcoe_vport_disable() - change vport state
 * @vport: vport to bring online/offline
 * @disable: should the vport be disabled?
 */
static int fcoe_vport_disable(struct fc_vport *vport, bool disable)
{
	struct fc_lport *lport = vport->dd_data;

	if (disable) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
		fc_fabric_logoff(lport);
	} else {
		lport->boot_time = jiffies;
		fc_fabric_login(lport);
		fc_vport_setlink(lport);
	}

	return 0;
}

/**
 * fcoe_vport_set_symbolic_name() - append vport string to symbolic name
 * @vport: fc_vport with a new symbolic name string
 *
 * After generating a new symbolic name string, a new RSPN_ID request is
 * sent to the name server.  There is no response handler, so if it fails
 * for some reason it will not be retried.
 */
static void fcoe_set_vport_symbolic_name(struct fc_vport *vport)
{
	struct fc_lport *lport = vport->dd_data;
	struct fc_frame *fp;
	size_t len;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s : %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name, vport->symbolic_name);

	if (lport->state != LPORT_ST_READY)
		return;

	len = strnlen(fc_host_symbolic_name(lport->host), 255);
	fp = fc_frame_alloc(lport,
			    sizeof(struct fc_ct_hdr) +
			    sizeof(struct fc_ns_rspn) + len);
	if (!fp)
		return;
	lport->tt.elsct_send(lport, FC_FID_DIR_SERV, fp, FC_NS_RSPN_ID,
			     NULL, NULL, 3 * lport->r_a_tov);
}
#endif /* !defined(__VMKLNX__) */

/**
 * fcoe_get_lesb() - Fill the FCoE Link Error Status Block
 * @lport: the local port
 * @fc_lesb: the link error status block
 */
static void fcoe_get_lesb(struct fc_lport *lport,
			 struct fc_els_lesb *fc_lesb)
{
	unsigned int cpu;
	u32 lfc, vlfc, mdac;
	struct fcoe_dev_stats *devst;
	struct fcoe_fc_els_lesb *lesb;
	struct net_device *netdev = fcoe_netdev(lport);

	lfc = 0;
	vlfc = 0;
	mdac = 0;
	lesb = (struct fcoe_fc_els_lesb *)fc_lesb;
	memset(lesb, 0, sizeof(*lesb));
	for_each_possible_cpu(cpu) {
#if !defined(__VMKLNX__)
		devst = per_cpu_ptr(lport->dev_stats, cpu);
#else /* defined(__VMKLNX__) */
		devst = FCOE_PER_CPU_PTR(lport->dev_stats, cpu,
                                         sizeof(struct fcoe_dev_stats));
#endif /* !defined(__VMKLNX__) */
		lfc += devst->LinkFailureCount;
		vlfc += devst->VLinkFailureCount;
		mdac += devst->MissDiscAdvCount;
	}
	lesb->lesb_link_fail = htonl(lfc);
	lesb->lesb_vlink_fail = htonl(vlfc);
	lesb->lesb_miss_fka = htonl(mdac);
#if !defined(__VMKLNX__)
	lesb->lesb_fcs_error = htonl(dev_get_stats(netdev)->rx_crc_errors);
#else /* !defined(__VMKLNX__) */
	lesb->lesb_fcs_error = htonl(netdev->stats.rx_crc_errors);
#endif /* !defined(__VMKLNX__) */
}


#if defined(__VMKLNX__)
static int open_fcoe_recv(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct fcoe_interface *fc = (struct fcoe_interface *) netdev->fcoe_ptr;
	struct packet_type *ptype = NULL;

	/*
	 * Only dispatch if fcoe_ptr non-NULL.
	 */
	if (fc) {
		ptype = (struct packet_type *) &(fc->fcoe_packet_type);
	        return fcoe_rcv(skb, netdev, ptype, netdev);
	} else {
		return 0;
	}
}


static   int  fip_recv(struct sk_buff *skb)
{
	struct net_device *netdev = skb->dev;
	struct fcoe_interface *fcoe =
		(struct fcoe_interface *) netdev->fcoe_ptr;

	/*
	 * Only dispatch if fcoe_ptr non-NULL.
	 */
	if (fcoe) {
		fcoe_ctlr_recv(&fcoe->ctlr, skb);
	}

	return 0;
}

static void fcoe_get_stats(struct fc_lport *lport,
	struct fcoe_dev_stats *out_stats)
{
	unsigned int cpu;

	memset(out_stats, 0, sizeof(*out_stats));

	for_each_possible_cpu(cpu) {
		struct fcoe_dev_stats *stats;

		stats = FCOE_PER_CPU_PTR(lport->dev_stats, cpu,
										 sizeof(struct fcoe_dev_stats));

		out_stats->TxFrames += stats->TxFrames;
		out_stats->TxWords += stats->TxWords;

		out_stats->RxFrames += stats->RxFrames;
		out_stats->RxWords += stats->RxWords;

		out_stats->ErrorFrames += stats->ErrorFrames;
		out_stats->DumpedFrames += stats->DumpedFrames;

		out_stats->InvalidTxWordCount += stats->InvalidTxWordCount;
		out_stats->InvalidCRCCount += stats->InvalidCRCCount;

		out_stats->InputRequests += stats->InputRequests;
		out_stats->OutputRequests += stats->OutputRequests;
		out_stats->ControlRequests += stats->ControlRequests;

		out_stats->InputMegabytes += stats->InputMegabytes;
		out_stats->OutputMegabytes += stats->OutputMegabytes;

		out_stats->LinkFailureCount += stats->LinkFailureCount;
		out_stats->LossOfSignalCount += stats->LossOfSignalCount;
		out_stats->VLinkFailureCount += stats->VLinkFailureCount;
		out_stats->MissDiscAdvCount += stats->MissDiscAdvCount;
	}
}


int fcoe_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
    off_t offset, int length, int func)
{
	struct fc_lport *lport = shost_priv(shost);
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->fcoe;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct fcoe_fcf *fcf, *next;
	struct fc_disc *disc;
	struct fc_rport_priv *rdata;
	struct fcoe_dev_stats fdev_stats;

	char *info_buf;
	int len = 0, byte_cnt = 0, index;
	if (func) {
		return 0;
	}

	info_buf = kzalloc(FCOE_INFO_BUF, GFP_KERNEL);
	if (info_buf == NULL)
		return 0;

	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"Driver Name           %s FCoE Driver\n", FCOE_VENDOR);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"Driver Version        %s\n", fcoe_version_str);

	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"Uplink Name           %s\n", fcoe->netdev->name);

	/* link state */
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"Link State            %s\n", fcoe_link_state_string(fcoe->link_state));

	/* Physical Node and Port WWN */
	mutex_lock(&lport->lp_mutex);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			 "Physical Port WWNN    %16.16llx\n", lport->wwnn);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			 "Physical Port WWPN    %16.16llx\n", lport->wwpn);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			 "PortID                0x%x\n", fc_host_port_id(lport->host));
	mutex_unlock(&lport->lp_mutex);

	/* FCF list */
	index = 1;
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len, "FCFs List :\n");
	list_for_each_entry_safe(fcf, next, &fip->fcfs, list) {
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"  FCF%d\n", index);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Switch Name         %16.16llx\n", fcf->switch_name);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Fabric Name         %16.16llx\n", fcf->fabric_name);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   FC Map              %x\n", fcf->fc_map);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   VFID                %d\n", fcf->vfid);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Priority            %x\n", fcf->pri);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Flags               %x\n", fcf->flags);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Keep Alive Period   %x\n", fcf->fka_period);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   MAC Address         %02x:%02x:%02x:%02x:%02x:%02x\n",
			fcf->fcf_mac[0], fcf->fcf_mac[1], fcf->fcf_mac[2],
			fcf->fcf_mac[3], fcf->fcf_mac[4], fcf->fcf_mac[5]);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   Type                %s\n",
			fcf == fip->sel_fcf ? "Selected" : "Backup");
		index++;
	}

	/* Remote Port List */
	index = 1;
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len, "Remote Ports List :\n");
	disc = &lport->disc;
	mutex_lock(&disc->disc_mutex);
	list_for_each_entry(rdata, &disc->rports, peers) {
		struct fc_rport_identifiers *ids = &rdata->ids;

		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"  Port%d\n", index);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   WWNN      %16.16llx\n", ids->node_name);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   WWPN      %16.16llx\n", ids->port_name);
		len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
			"   PortID    0x%x\n", ids->port_id);
		index++;
	}
	mutex_unlock(&disc->disc_mutex);


	fcoe_get_stats(lport, &fdev_stats);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"FCoE Statistics :\n");
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   TX Frames                    %llu\n", fdev_stats.TxFrames);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   TX Words                     %llu\n", fdev_stats.TxWords);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   RX Frames                    %llu\n", fdev_stats.RxFrames);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   RX Words                     %llu\n", fdev_stats.RxWords);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Error Frames                 %llu\n", fdev_stats.ErrorFrames);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Dumped Frames                %llu\n", fdev_stats.DumpedFrames);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Link Failure Count           %llu\n", fdev_stats.LinkFailureCount);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Loss Signal Count            %llu\n", fdev_stats.LossOfSignalCount);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Invalid TX Word Count        %llu\n",
		fdev_stats.InvalidTxWordCount);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Invalid CRC Count            %llu\n", fdev_stats.InvalidCRCCount);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Input Reqs                   %llu\n", fdev_stats.InputRequests);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Output Reqs                  %llu\n", fdev_stats.OutputRequests);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Ctrl Reqs                    %llu\n", fdev_stats.ControlRequests);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Input Megabytes              %llu\n", fdev_stats.InputMegabytes);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Output Megabytes             %llu\n", fdev_stats.OutputMegabytes);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Vlink Fail Count             %llu\n", fdev_stats.VLinkFailureCount);
	len += snprintf(info_buf+len, FCOE_INFO_BUF-len,
		"   Misc Disc Adv Count          %llu\n", fdev_stats.MissDiscAdvCount);


	byte_cnt = 0;
	if (len >= offset)
		byte_cnt = len - offset;

	byte_cnt = byte_cnt < length ? byte_cnt : length;
	len = 0;
	if (byte_cnt > 0) {
		memcpy(buffer, info_buf + offset, byte_cnt);
		len = byte_cnt;
	}
	kfree(info_buf);
	return len;
}
#endif /* defined(__VMKLNX__) */
