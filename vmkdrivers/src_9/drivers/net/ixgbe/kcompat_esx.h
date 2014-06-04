/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2011 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

/*
 *
 * VMware ESX compatibility layer
 *
 */

#include "vmkapi.h"

/* disable features that VMware ESX does not support */

#ifndef CONFIG_PM
#undef device_init_wakeup
#define device_init_wakeup(dev, val) \
	((dev)->power.can_wakeup = !!val)
#undef device_set_wakeup_enable
#define device_set_wakeup_enable(dev, val) do { } while (0)
#endif /* CONFIG_PM */

#define cpu_to_be16(x) __constant_htons(x)

#define IXGBE_FCOE_DEFTC 		0x3
/*
 * CNA device private region points to the net device private
 * region. Thereby, the actual implementation of netdev_priv()
 * is not applicable as it assumes there is a private region
 * allocated right after a net_device, which is not the case for
 * the CNA device.
 * See ixgbe_cna_enable() comments.
 */
#undef netdev_priv
#define netdev_priv(netdev) (netdev->priv)

#define vmalloc_node(a,b)  vmalloc(a)

#define skb_record_rx_queue(a, b)  \
	vmknetddi_queueops_set_skb_queueid((a),  \
				VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID((b)));


#define skb_trim _kc_skb_trim
static inline void _kc_skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len > len) {
		if (unlikely(skb->data_len)) {
			WARN_ON(1);
			return;
		}
		skb->len = len;
		skb->tail = skb->data + len;
	}
}

/* multiqueue netdev magic */
#define egress_subqueue_count real_num_tx_queues

/*
 * A couple of quick hacks for working with esx40
 */
#define HAVE_NETDEV_NAPI_LIST
#define vmk_set_module_version(x,y) 1
#define VMKNETDDI_REGISTER_QUEUEOPS(ndev, ops)  \
	do {                                    \
		ndev->netqueue_ops = ops;       \
	} while (0)

#ifdef NETIF_F_SW_LRO
#define NETIF_F_LRO        NETIF_F_SW_LRO
#endif

#define dca3_get_tag(d, c)       dca_get_tag(c)
#define dca_unregister_notify(x)
#define dca_register_notify(x)

#define USE_REBOOT_NOTIFIER
#define device_set_wakeup_enable(d, w) device_init_wakeup(d, w);


#define nr_cpu_ids     smp_num_cpus 
#define ESX_ALLOC_PERCPU( type )      \
	kmalloc(sizeof(type) * nr_cpu_ids, GFP_KERNEL)
#define ESX_FREE_PERCPU( ptr )        kfree(ptr)
#define ESX_PER_CPU_PTR( ptr, cpu, type )     (((cpu) < nr_cpu_ids)? \
	((typeof(ptr))((char*)(ptr) + (cpu) * sizeof(type))):NULL)
#define __percpu
