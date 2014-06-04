/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#ident "$Id: kcompat.h 56483 2010-08-17 12:21:35Z dwang2 $"

#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_


#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

#ifndef PCI_VENDOR_ID_CISCO
#define PCI_VENDOR_ID_CISCO	0x1137
#endif

/*
 * Kernel backward-compatibility definitions
 */

#ifndef ioread8
#define ioread8 readb
#endif

#ifndef ioread16
#define ioread16 readw
#endif

#ifndef ioread32
#define ioread32 readl
#endif

#ifndef iowrite8
#define iowrite8 writeb
#endif

#ifndef iowrite16
#define iowrite16 writew
#endif

#ifndef iowrite32
#define iowrite32 writel
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#endif

#ifndef NETIF_F_GSO
#define gso_size tso_size
#endif

#ifndef NETIF_F_TSO6
#define NETIF_F_TSO6 0
#endif

#ifndef NETIF_F_TSO_ECN
#define NETIF_F_TSO_ECN 0
#endif

#ifndef NETIF_F_GRO
#define NETIF_F_GRO 0
#endif

#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL CHECKSUM_HW
#define CHECKSUM_COMPLETE CHECKSUM_HW
#endif

#ifndef IRQ_HANDLED
#define IRQ_HANDLED 1
#define IRQ_NONE 0
#endif

#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#ifndef PCI_VDEVICE
#define PCI_VDEVICE(vendor, device) \
	PCI_VENDOR_ID_##vendor, (device), \
	PCI_ANY_ID, PCI_ANY_ID, 0, 0
#endif

#ifndef round_jiffies
#define round_jiffies(j) (j)
#endif

#ifndef netdev_tx_t
#define netdev_tx_t int
#endif

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif
#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) 0
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14))
static inline signed long schedule_timeout_uninterruptible(signed long timeout)
{
	set_current_state(TASK_UNINTERRUPTIBLE);
	return schedule_timeout(timeout);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15))
static inline void *kzalloc(size_t size, unsigned int flags)
{
	void *mem = kmalloc(size, flags);
	if (mem)
		memset(mem, 0, size);
	return mem;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18))
static inline int kcompat_skb_linearize(struct sk_buff *skb, int gfp)
{
	return skb_linearize(skb, gfp);
}
#undef skb_linearize
#define skb_linearize(skb) kcompat_skb_linearize(skb, GFP_ATOMIC)
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20))
#define csum_offset csum
#endif

#ifndef __packed
#define __packed __attribute__ ((packed))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22))
#define ip_hdr(skb) (skb->nh.iph)
#define ipv6_hdr(skb) (skb->nh.ipv6h)
#define tcp_hdr(skb) (skb->h.th)
#define tcp_hdrlen(skb) (skb->h.th->doff << 2)
#define skb_transport_offset(skb) (skb->h.raw - skb->data)
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)) && \
     (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))) || \
    defined(__VMKLNX__)

#define napi_schedule_prep(napi) netif_rx_schedule_prep((napi)->dev, napi)
#define napi_schedule(napi) netif_rx_schedule((napi)->dev, napi)
#define __napi_schedule(napi) __netif_rx_schedule((napi)->dev, napi)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)) && !defined(__VMKLNX__)
#define netif_napi_del(napi) do { } while (0)
#endif

#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24))

#if (RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(5, 3))
struct napi_struct {
	struct net_device *dev;
	int (*poll)(struct napi_struct *, int);
};
#endif
#define napi_complete(napi) netif_rx_complete((napi)->dev)
#define napi_schedule_prep(napi) netif_rx_schedule_prep((napi)->dev)
#define napi_schedule(napi) netif_rx_schedule((napi)->dev)
#define __napi_schedule(napi) __netif_rx_schedule((napi)->dev)
#define napi_enable(napi) netif_poll_enable((napi)->dev)
#define napi_disable(napi) netif_poll_disable((napi)->dev)
#define netif_napi_add(ndev, napi, _poll, _weight) \
	do { \
		struct napi_struct *__napi = napi; \
		ndev->poll = __enic_poll; \
		ndev->weight = _weight; \
		__napi->poll = _poll; \
		__napi->dev = ndev; \
		netif_poll_disable(ndev); \
	} while (0)
#define netif_napi_del(napi) do { } while (0)

#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25))
#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[]
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25))
#define pci_enable_device_mem pci_enable_device
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))
#undef pr_err
#define pr_err(fmt, ...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#undef pr_info
#define pr_info(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29))
#define vlan_gro_receive(napi, vlan_group, vlan_tci, skb) \
	vlan_hwaccel_receive_skb(skb, vlan_group, vlan_tci)
#define napi_gro_receive(napi, skb) \
	netif_receive_skb(skb)
#endif

#define for_each_sg(start, var, count, index) \
		for ((var) = (start), (index) = 0; (index) < (count); \
			(var) = sg_next(var), (index)++)

#define kmem_cache kmem_cache_s

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33))
static inline struct sk_buff *kcompat_netdev_alloc_skb_ip_align(
	struct net_device *dev, unsigned int length)
{
	struct sk_buff *skb = netdev_alloc_skb(dev, length + NET_IP_ALIGN);

	if (NET_IP_ALIGN && skb)
		skb_reserve(skb, NET_IP_ALIGN);
	return skb;
}
#undef netdev_alloc_skb_ip_align
#define netdev_alloc_skb_ip_align(netdev, len) \
	kcompat_netdev_alloc_skb_ip_align(netdev, len)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
#define netdev_for_each_mc_addr(mclist, dev) \
	for (mclist = dev->mc_list; mclist; mclist = mclist->next)
#define netdev_mc_count(dev) ((dev)->mc_count)

#if ((!RHEL_RELEASE_CODE) || \
       (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6, 0)))
static inline const char *netdev_name(const struct net_device *dev)
{
	if (dev->reg_state != NETREG_REGISTERED)
		return "(unregistered net_device)";
	return dev->name;
}
#endif

#define netdev_printk(level, netdev, format, args...) \
	dev_printk(level, &((netdev)->pdev->dev), \
		"%s: " format, \
		netdev_name(netdev), ##args)

#define netdev_err(dev, format, args...) \
	netdev_printk(KERN_ERR, dev, format, ##args)
#define netdev_warn(dev, format, args...) \
	netdev_printk(KERN_WARNING, dev, format, ##args)
#define netdev_info(dev, format, args...) \
	netdev_printk(KERN_INFO, dev, format, ##args)
#endif

#define PORT_PROFILE_MAX 40
#define PORT_UUID_MAX  16



#endif /* _KCOMPAT_H_ */
