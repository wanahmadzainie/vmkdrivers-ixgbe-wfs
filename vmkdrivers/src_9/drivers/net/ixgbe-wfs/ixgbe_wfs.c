
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#ifdef NETIF_F_TSO
#include <net/checksum.h>
#ifdef NETIF_F_TSO6
#include <net/ip6_checksum.h>
#endif
#endif
#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif

#ifndef __VMKLNX__
#include <linux/if_bridge.h>
#endif /* __VMKLNX__ */

#include <linux/ktime.h>
#include "ixgbe_wfs.h"

/*
 * WFS variable declaration
 */
/* macro for individual iwa */
#define myID            (iwa->wfs_id)
#define myMacAddr       (iwa->mac_addr)
#define myIP            (iwa->ip)
#define wfspeer         (iwa->wfspeer)
#define wfsbert			(iwa->wfsbert)
#define	myBertSeqNo		(iwa->bert_seqno)

static struct ixgbe_wfs_adapter ixgbe_wfs_adapter[IXGBE_MAX_WFS_NIC] =
            { [0 ... IXGBE_MAX_WFS_NIC-1] = { .state = unused, .wfs_id = 0 } };


#ifdef __VMKLNX__
static inline u64 ixgbe_wfs_time_to_usecs(struct timeval *tv)
{
        return ((tv->tv_sec * USEC_PER_SEC) + tv->tv_usec);
}
#endif /* __VMKLNX__ */

/*
 * adapter bonding
 */

void disp_frag(unsigned char * addr, int len)
{
    int i;

    for(i=0; i<len; i++)
    {
        if(!(i%16))
            printk("%04X: ", i);
        printk("%02x ", addr[i]);
        if(!((i+1)%4))
            printk(", ");
        if(!((i+1)%16))
            printk("\n");
    }
    printk("\n");
}

/*
 * allocate/search WFS entry
 */
struct ixgbe_wfs_adapter *ixgbe_wfs_get_adapter(u32 dev_id)
{
    int i;

    /* find matched device */
    for (i=0; i<IXGBE_MAX_WFS_NIC; i++) {
        if (ixgbe_wfs_adapter[i].state == unused)
            continue;
        if (ixgbe_wfs_adapter[i].dev_id == dev_id)
            return &ixgbe_wfs_adapter[i];
    }

    /* return unused entry for new device */
    for (i=0; i<IXGBE_MAX_WFS_NIC; i++) {
        if (ixgbe_wfs_adapter[i].state == unused) {
            memset(&ixgbe_wfs_adapter[i], 0, sizeof(struct ixgbe_wfs_adapter));
            ixgbe_wfs_adapter[i].index = i;
            ixgbe_wfs_adapter[i].wfs_id = i+1;
            strcpy(ixgbe_wfs_adapter[i].name, "ring");
            ixgbe_wfs_adapter[i].state = allocated;
            ixgbe_wfs_adapter[i].dev_id = dev_id;
            return &ixgbe_wfs_adapter[i];
        }
    }

    return NULL;
}

/*
 * packet encapsulation
 */
static struct sk_buff *wfspkt_alloc(struct ixgbe_wfs_adapter *iwa, int data_len)
{
    struct sk_buff *skb;

    /* allocate a skb to store the frags */
    skb = netdev_alloc_skb_ip_align(iwa->ndev, WFSPKT_MAX_SIZE + ETH_FCS_LEN + data_len);
    if (unlikely(!skb)) {
        return NULL;
    }
	skb->protocol = htons(WFSPKT_ETHERTYPE);

    return skb;
}

static void wfspkt_init(struct ixgbe_wfs_adapter *iwa, struct wfspkt *pkt, int type, unsigned short dstVid, unsigned char *dstMac)
{
    u8 broadcast_mac[6]  = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    memset(pkt, 0, sizeof(struct wfspkt));
    /* wfspkt type */
    pkt->ethhdr.h_proto = htons(WFSPKT_ETHERTYPE);
    if (dstMac[0] & 0x01)
        type |= WFSPKT_TYPE_BROADCAST_MASK;
    pkt->type = type;

	/* wfspkt wfs-id */
#ifdef WFS_FIB
    if (type & WFSPKT_TYPE_BROADCAST_MASK)
        pkt->dest = WFSID_ALL;
    else
        pkt->dest = ixgbe_wfs_fib_lookup(iwa, dstVid, dstMac);
#else
    pkt->dest = WFSID_ALL;
#endif

    pkt->src = myID;

	/* wfspkt MAC */
	memcpy((void *)pkt->ethhdr.h_dest,
			(pkt->dest == WFSID_ALL) ? (void *)broadcast_mac : wfspeer[pkt->dest-1].mac, 6);
	memcpy((void *)pkt->ethhdr.h_source, (void *)myMacAddr, 6);

    pkt->len = WFSPKT_HDR_SIZE;
    pkt->opts[0] = WFSOPT_END;
}

static struct wfsopt * wfspkt_getopt(struct wfspkt *pkt, int opt_type)
{
    int len = pkt->len - WFSPKT_HDR_SIZE;
    struct wfsopt *opt = (struct wfsopt *)pkt->opts;

    while (len > 0) {
        if (opt->type == opt_type)
            return opt;
        len -= opt->len;
        opt = (struct wfsopt *)((u8 *)opt + opt->len);
    }
    return NULL;
}

#ifdef WFS_DATASEQ
static struct wfsopt *wfspkt_set_sequence(struct wfspkt *pkt, u32 seqno)
{
    struct wfsopt *opt = (struct wfsopt *) ((char *)pkt + pkt->len);

    memset(opt, 0, sizeof(struct wfsopt_sequence));
    opt->type = WFSOPT_SEQUENCE;
    opt->len = WFSOPT_HDR_SIZE + sizeof(struct wfsopt_sequence);

    opt->val.sequence.no = htonl(seqno);
    pkt->len += opt->len;
    BUG_ON(pkt->len > WFSPKT_MAX_SIZE);
    ((u8 *)pkt)[pkt->len] = WFSOPT_END;
    return opt;
}
#endif

static inline void wfspkt_set_announce_primary(struct wfsopt *opt, u8 port)
{
    opt->val.announce.port_pri = 1;
    opt->val.announce.port_sec = 0;
    opt->val.announce.port = port;
}

static inline void wfspkt_set_announce_secondary(struct wfsopt *opt, u8 port)
{
    opt->val.announce.port_pri = 0;
    opt->val.announce.port_sec = 1;
    opt->val.announce.port = port;
}

static inline void wfspkt_clear_announce_channel(struct wfsopt *opt)
{
    opt->val.announce.port_pri =
    opt->val.announce.port_sec = 0;
}

static struct wfsopt *wfspkt_set_announce(struct ixgbe_wfs_adapter *iwa, struct wfspkt *pkt)
{
    struct wfsopt *opt = (struct wfsopt *) ((char *)pkt + pkt->len);

    memset(opt, 0, sizeof(struct wfsopt_announce));
    opt->type = WFSOPT_ANNOUNCE;
    opt->len = WFSOPT_HDR_SIZE + sizeof(struct wfsopt_announce);

    memcpy(opt->val.announce.mac, myMacAddr, 6);
    opt->val.announce.ip = htonl(myIP);
    pkt->len += opt->len;
    BUG_ON(pkt->len > WFSPKT_MAX_SIZE);
    ((u8 *)pkt)[pkt->len] = WFSOPT_END;
    return opt;
}

static inline void wfspkt_set_raps_sf(struct wfsopt *opt)
{
    opt->val.raps.request = WFSOPT_RAPS_REQ_SF;
}

static inline void wfspkt_set_raps_nr(struct wfsopt *opt)
{
    opt->val.raps.request = WFSOPT_RAPS_REQ_NR;
}

static inline void wfspkt_set_raps_dnf(struct wfsopt *opt)
{
    opt->val.raps.status.dnf = 1;
}

static inline void wfspkt_set_raps_flush(struct wfsopt *opt)
{
    opt->val.raps.status.flush = 1;
}

static struct wfsopt *wfspkt_set_raps(struct wfspkt *pkt)
{
    struct wfsopt *opt = (struct wfsopt *) ((char *)pkt + pkt->len);

    memset(opt, 0, sizeof(struct wfsopt_raps));
    opt->type = WFSOPT_RAPS;
    opt->len = WFSOPT_HDR_SIZE + sizeof(struct wfsopt_raps);

    pkt->len += opt->len;
    BUG_ON(pkt->len > WFSPKT_MAX_SIZE);
    ((u8 *)pkt)[pkt->len] = WFSOPT_END;
    return opt;
}

#ifdef WFS_BERT
static struct wfsopt *wfspkt_set_bert(struct wfspkt *pkt)
{
    struct wfsopt *opt = (struct wfsopt *) ((char *)pkt + pkt->len);

    memset(opt, 0, sizeof(struct wfsopt_bert));
    opt->type = WFSOPT_BERT;
    opt->len = WFSOPT_HDR_SIZE + sizeof(struct wfsopt_bert);

    pkt->len += opt->len;
    BUG_ON(pkt->len > WFSPKT_MAX_SIZE);
    ((u8 *)pkt)[pkt->len] = WFSOPT_END;
    return opt;
}
#endif

static struct sk_buff *getWfsAncePkt(struct ixgbe_wfs_adapter *iwa, struct wfsopt **opt)
{
    struct sk_buff *n = wfspkt_alloc(iwa, 0);
    u8 broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

    if (n == NULL) {
        log_err("no more wfspkt buffer\n");
    } else {
        wfspkt_init(iwa, (struct wfspkt *)n->data, WFSPKT_TYPE_CTRL_ANNOUNCE, 0, broadcast_mac);
        *opt = wfspkt_set_announce(iwa, (struct wfspkt *)n->data);
        skb_put(n, ((struct wfspkt *)n->data)->len);
    }
    return n;
}

static struct sk_buff *getWfsRapsPkt(struct ixgbe_wfs_adapter *iwa, struct wfsopt **opt)
{
    struct sk_buff *n = wfspkt_alloc(iwa, 0);
    u8 broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

    if (n == NULL) {
        log_err("no more wfspkt buffer\n");
    } else {
        wfspkt_init(iwa, (struct wfspkt *)n->data, WFSPKT_TYPE_CTRL_RAPS, 0, broadcast_mac);
        *opt = wfspkt_set_raps((struct wfspkt *)n->data);
        skb_put(n, ((struct wfspkt *)n->data)->len);
    }
    return n;
}

#ifdef WFS_BERT
static struct sk_buff *getWfsBertPkt(struct ixgbe_wfs_adapter *iwa, u8 wfsid, int data_len, struct wfsopt **opt)
{
    struct sk_buff *n = wfspkt_alloc(iwa, data_len);

    if (n == NULL) {
        log_err("no more wfspkt buffer\n");
    } else {
        wfspkt_init(iwa, (struct wfspkt *)n->data, WFSPKT_TYPE_CTRL_BERT, 0, wfspeer[wfsid-1].mac);
        *opt = wfspkt_set_bert((struct wfspkt *)n->data);
        skb_put(n, ((struct wfspkt *)n->data)->len);
    }
    return n;
}
#endif

void ixgbe_wfs_send_announce(struct ixgbe_wfs_adapter *iwa)
{
    struct sk_buff *skb;
    struct wfspkt *wfspkt;
    struct wfsopt *wfsopt;
    struct ixgbe_adapter *adapter = iwa->primary;
    netdev_tx_t rc;

    log_debug("Enter\n");

    if(!netif_carrier_ok(iwa->ndev))
        return;

    /* send announce via all available channels */
    for (adapter=iwa->primary; adapter; adapter=adapter->wfs_next)
    {
        if (!adapter->link_up)
            continue;

        if ((skb = getWfsAncePkt(iwa, &wfsopt)) == NULL)
            continue;

        wfspkt = (struct wfspkt *)skb->data;

        if (adapter == iwa->primary)
            wfspkt_set_announce_primary(wfsopt, adapter->wfs_port);
        else
            wfspkt_set_announce_secondary(wfsopt, adapter->wfs_port);

#if 0
        disp_frag(skb->data, wfspkt->len);
#endif
        rc = ixgbe_xmit_wfs_frame(skb, adapter);
        if (rc != NETDEV_TX_OK) {
            if ((iwa->xmit_err%1000) == 0)
                log_warn("sent announce len %d to port %d failed %d\n",
                    wfspkt->len, adapter->wfs_port, rc);
            dev_kfree_skb_any(skb);
        } else {
            log_debug("port %d sent announce (%s) len %d \n",
                adapter->wfs_port,
                wfsopt->val.announce.port_pri ? "pri" : wfsopt->val.announce.port_sec ? "sec" : "-",
                wfspkt->len);
        }
    }

    return;
}

void ixgbe_wfs_send_raps(struct ixgbe_wfs_adapter *iwa, wfs_fsm_event event)
{
    struct sk_buff *skb;
    struct wfspkt *wfspkt;
    struct wfsopt *wfsopt;
    struct ixgbe_adapter *adapter = iwa->primary;
    netdev_tx_t rc;

    log_debug("Enter\n");

    if(!netif_carrier_ok(iwa->ndev))
        return;

    /* send announce via all available channels */
    for (adapter=iwa->primary; adapter; adapter=adapter->wfs_next)
    {
        if (!adapter->link_up)
            continue;

        if ((skb = getWfsRapsPkt(iwa, &wfsopt)) == NULL)
            continue;

        if (event == E_rapsNR)
            wfspkt_set_raps_nr(wfsopt);
        else if (event == E_rapsSF)
            wfspkt_set_raps_sf(wfsopt);

        wfspkt = (struct wfspkt *)skb->data;

#if 0
        disp_frag(skb->data, wfspkt->len);
#endif
        rc = ixgbe_xmit_wfs_frame(skb, adapter);
        if (rc != NETDEV_TX_OK) {
            if ((iwa->xmit_err%1000) == 0)
                log_warn("sent raps len %d to port %d failed %d\n",
                    wfspkt->len, adapter->wfs_port, rc);
            dev_kfree_skb_any(skb);
        } else {
            log_debug("port %d sent raps (%s) len %d \n",
                adapter->wfs_port, (event==E_rapsSF) ? "SF" : "NR", wfspkt->len);
        }
    }

    return;
}

#ifdef WFS_BERT
#define CBLK    128
/*
 *  calculate checksum in block
 */
static inline u16 cblk_csum(const int block_size, char *data, u16 csum)
{
    return csum ^ ip_compute_csum(data, block_size);
}

u16 ixgbe_wfs_get_bert_csum(struct ixgbe_wfs_adapter *iwa, char *data, int data_len)
{
    u16 csum;
    unsigned char *ba, cblk[CBLK];
    int len;

    csum = 0;
    ba = (unsigned char *)data;
    len = data_len;
    /* fragment in csum block */
    while (len >= CBLK) {
        csum = cblk_csum(CBLK, ba, csum);
        ba += CBLK; len -= CBLK;
    }
    /* final csum block */
    if (len) {
        memset(cblk, 0, CBLK);
        memcpy(cblk, ba, len);
        csum = cblk_csum(CBLK, cblk, csum);
    }

    log_debug("calculate BERT data len %d csum %04x\n", data_len, csum);

    return csum;
}

u16 ixgbe_wfs_get_bert_skb_csum(struct ixgbe_wfs_adapter *iwa, struct sk_buff *skb)
{
    struct wfspkt *wfspkt = (struct wfspkt *)skb->data;
    struct wfsopt *wfsopt = wfspkt_getopt((struct wfspkt *)wfspkt, WFSOPT_BERT);
    u16 csum;
    unsigned char *va, *ba, cblk[CBLK];
    int f, b, len, tt;

    if (!wfsopt)
        return (u16)-1;

    /* head fragment */
    csum = 0; b = CBLK; ba = cblk; tt = 0;
    len = skb_headlen(skb) - wfspkt->len;
    va = (unsigned char *)wfsopt->val.bert.data;
    while (len >= CBLK) {
        csum = cblk_csum(CBLK, va, csum); tt += CBLK;
        va += CBLK; len -= CBLK;
    }
    if (len) {
        memset(cblk, 0, CBLK);
        memcpy(cblk, va, len);
        ba += len; b -= len;
    }
    /* fragments */
    for (f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
        len = skb_shinfo(skb)->frags[f].size;
        va = skb_frag_address(&skb_shinfo(skb)->frags[f]);
        /* try filling up cblk with residue from previous fragment */
        if (b < CBLK) {
            if (len < b) {
                memcpy(ba, va, len);
                len = 0; ba += len; b -= len;
            } else {
                memcpy(ba, va, b);
                va += b; len -= b; b = CBLK; ba = cblk;
                csum = cblk_csum(CBLK, cblk, csum); tt += CBLK;
            }
        }
        while (len >= CBLK) {
            csum = cblk_csum(CBLK, va, csum); tt += CBLK;
            va += CBLK; len -= CBLK;
        }
        if (len) {
            memset(cblk, 0, CBLK);
            memcpy(ba, va, len);
            ba += len; b -= len;
        }
    }
    /* final csum block */
    if (b < CBLK)
        csum = cblk_csum(CBLK, cblk, csum); tt += CBLK;

    log_debug("calculate BERT skb len %d csum %04x\n", tt, csum);

    return csum;
}

struct sk_buff *ixgbe_wfs_get_bert_skb(struct ixgbe_wfs_adapter *iwa,
        u8 wfsid, const char *data, int data_len, u16 csum)
{
    struct sk_buff *skb;
    struct wfspkt *wfspkt;
    struct wfsopt *wfsopt;

    /* prepare BERT skb */
    if ((skb = getWfsBertPkt(iwa, wfsid, data_len, &wfsopt)) == NULL)
        return NULL;
    wfspkt = (struct wfspkt *)skb->data;
    wfsopt->val.bert.data_len = data_len;
    wfsopt->val.bert.data_csum = csum;
    memcpy(skb_put(skb, data_len), data, data_len);

    log_debug("get BERT skb len %d nr_frags %d, BERT data len %d csum %04x\n",
            skb->len, skb_shinfo(skb)->nr_frags, wfsopt->val.bert.data_len, wfsopt->val.bert.data_csum);
#if 0
    disp_frag(skb->data, MIN(60,wfspkt->len));
#endif
    return skb;
}

void ixgbe_wfs_send_bert(struct ixgbe_wfs_adapter *iwa, u8 wfsid)
{
    struct sk_buff *skb;
    struct wfspkt *wfspkt;
    struct wfsopt *wfsopt;
    struct ixgbe_adapter *adapter;
    struct wfs_bert_cfg *bertcfg = &wfsbert[myID-1];
    netdev_tx_t rc;
#ifdef __VMKLNX__
	struct timeval current_time;
#endif

    log_debug("Enter\n");

    if (!bertcfg->skb ||
        !(skb = skb_copy(bertcfg->skb, GFP_ATOMIC)))
        return;

    if (wfspeer[wfsid-1].fsm_state == S_idle)
        adapter = wfspeer[wfsid-1].channel_pri;
    else if (wfspeer[wfsid-1].fsm_state == S_protect)
        adapter = wfspeer[wfsid-1].channel_sec;
    else
        return;

    wfspkt = (struct wfspkt *)skb->data;
    wfsopt = wfspkt_getopt((struct wfspkt *)wfspkt, WFSOPT_BERT);
    wfsopt->val.bert.seqno = ++myBertSeqNo;
#ifdef __VMKLNX__
	do_gettimeofday(&current_time);
	wfsopt->val.bert.ts = ixgbe_wfs_time_to_usecs(&current_time);
#else
    wfsopt->val.bert.ts = ktime_to_us(ktime_get_real());
#endif /* __VMKLNX__ */

    /* first 5 packets to reset responder data/stats, no response required */
    if (myBertSeqNo <= 5) {
        wfsopt->val.bert.type = WFSOPT_BERT_RESET;
        bertcfg->seqno = wfsopt->val.bert.seqno + 1;
        bertcfg->jfs = jiffies;
    } else {
        wfsopt->val.bert.type = WFSOPT_BERT_REQUEST;
    }

    bertcfg->jfs_last = jiffies;

#if 0
    disp_frag(skb->data, MIN(60,skb_headlen(skb)));
#endif
    rc = ixgbe_xmit_wfs_frame(skb, adapter);
    if (rc != NETDEV_TX_OK) {
        if ((iwa->xmit_err%1000) == 0)
            log_warn("sent BERT len %d to port %d failed %d\n",
                wfspkt->len, adapter->wfs_port, rc);
        dev_kfree_skb_any(skb);
    } else {
        if (wfsopt->val.bert.type == WFSOPT_BERT_REQUEST) {
            bertcfg->stats.tx_pkts++;
            bertcfg->stats.tx_bytes += bertcfg->skb->len;
        }
        log_debug("port %d sent BERT (%s) seq %d len %d \n",
            adapter->wfs_port, (wfsopt->val.bert.type == WFSOPT_BERT_RESET) ? "config" : "request",
            wfsopt->val.bert.seqno, bertcfg->skb->len);
    }

    return;
}
#endif

struct sk_buff *ixgbe_wfs_encap(struct ixgbe_adapter *adapter, struct sk_buff *skb)
{
    struct ixgbe_wfs_adapter *iwa = adapter->wfs_parent;
    u8 _pktbuf_[WFSPKT_MAX_SIZE];
    struct wfspkt *wfspkt = (struct wfspkt *)_pktbuf_;
    struct wfsopt *opt;
    union {
        unsigned char *network;
        /* l2 headers */
        struct ethhdr *eth;
        struct vlan_ethhdr *veth;
        /* l3 headers */
        struct iphdr *ipv4;
        struct ipv6hdr *ipv6;
    } hdr;
    u8 broadcast_mac[6]  = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#ifdef HAVE_VLAN_RX_REGISTER
	u16 vid = IXGBE_CB(skb)->vid;
#else
	u16 vid = skb->vlan_tci;
#endif

	log_debug("Tx skb len %d data_len %d vlan_tag %d headroom %d tailroom %d\n",
			skb->len, skb->data_len, vid & 0xfff, skb_headroom(skb), skb_tailroom(skb));

    hdr.network = skb->data;

    /*
     * encapsulate data packet
     */
    if (memcmp(hdr.eth->h_dest, broadcast_mac, 6) == 0) {
        wfspkt_init(iwa, wfspkt, WFSPKT_TYPE_DATA_BROADCAST, vid, hdr.eth->h_dest);
    } else {
        wfspkt_init(iwa, wfspkt, WFSPKT_TYPE_DATA_UNICAST, vid, hdr.eth->h_dest);
    }
#ifdef WFS_DATASEQ
    opt = wfspkt_set_sequence(wfspkt, iwa->data_seqno++);
#endif
    if (wfspkt->len > skb_headroom(skb)) {
        if (pskb_expand_head(skb, wfspkt->len, 0, GFP_ATOMIC)) {
            log_err("WFS Encap failed.\n");
            return NULL;
        }
        log_debug("Tx WFS expanded skb len %d data_len %d headroom %d tailroom %d\n",
            skb->len, skb->data_len, skb_headroom(skb), skb_tailroom(skb));
    }

    skb_push(skb, wfspkt->len);
    memcpy(skb->data, _pktbuf_, wfspkt->len);

    log_debug("Tx skb encap WFS header len %d skb len %d\n", wfspkt->len, skb->len);
#if 0
    disp_frag(skb->data, MIN(60,skb_headlen(skb)));
#endif

    return skb;
}

struct sk_buff *ixgbe_wfs_decap(struct ixgbe_adapter *adapter, struct sk_buff *skb)
{
    struct ixgbe_wfs_adapter *iwa = adapter->wfs_parent;
    struct wfspkt *wfspkt;
    union {
        unsigned char *network;
        /* l2 headers */
        struct ethhdr *eth;
        struct vlan_ethhdr *veth;
        /* l3 headers */
        struct iphdr *ipv4;
        struct ipv6hdr *ipv6;
    } hdr;
    __be16 protocol;

    log_debug("Rx skb len %d data_len %d headroom %d tailroom %d\n",
            skb->len, skb->data_len, skb_headroom(skb), skb_tailroom(skb));
#if 0
    disp_frag(skb->data, MIN(60,skb_headlen(skb)));
#endif

    hdr.network = skb->data;
    protocol = hdr.eth->h_proto;

    if (protocol != htons(WFSPKT_ETHERTYPE)) {
        log_warn("Non WFS-Encap packet, skipped.\n");
        return NULL;
    }

    wfspkt = (struct wfspkt *)skb->data;

    if (!pskb_may_pull(skb, wfspkt->len)) {
        log_err("May not pull WFS header, not enough length, skipped.\n");
        return NULL;
    }

    log_debug("Rx decap WFS header len %d, skb len %d\n", wfspkt->len, skb->len);

    return skb;
}


static void wfs_process_announce(struct ixgbe_wfs_adapter *iwa, struct ixgbe_adapter *adapter,
        struct wfspkt *pkt, struct wfsopt *opt)
{
    struct wfs_peer *peer = &wfspeer[pkt->src-1];
    wfs_fsm_event_data event_data;

    log_debug("port %d recv wfsid %d announce (%s), mac " PRINT_MAC_FMT "\n",
            adapter->wfs_port, pkt->src,
            opt->val.announce.port_pri ? "pri" : opt->val.announce.port_sec ? "sec" : "-",
            PRINT_MAC_VAL(opt->val.announce.mac));

    memcpy(peer->mac, opt->val.announce.mac, 6);
    peer->ip = ntohl(opt->val.announce.ip);

    if (opt->val.announce.port_pri) {
        /* trigger FSM */
        event_data.announce.peer_port = opt->val.announce.port;
        event_data.announce.is_primary_annouce = 1;
        event_data.announce.recv_channel = adapter;
        ixgbe_wfs_fsm_set_event(iwa, pkt->src, E_discover, &event_data);
    }

    else if (opt->val.announce.port_sec) {
        /* trigger FSM */
        event_data.announce.is_primary_annouce = 0;
        event_data.announce.peer_port = opt->val.announce.port;
        event_data.announce.recv_channel = adapter;
        ixgbe_wfs_fsm_set_event(iwa, pkt->src, E_discover, &event_data);
    }

    log_debug("peer wfsid %d channel (pri:%d/sec:%d) current state %s\n", pkt->src,
            peer->channel_pri ? peer->channel_pri->wfs_port : -1,
            peer->channel_sec ? peer->channel_sec->wfs_port : -1,
            peer->fsm_state == S_idle ? "IDLE" : peer->fsm_state == S_protect ? "PROTECT" : "-" );

    /* no forward */
    pkt->type = WFSPKT_TYPE_CTRL_NONE;

}

static void wfs_process_raps(struct ixgbe_wfs_adapter *iwa, struct ixgbe_adapter *adapter,
        struct wfspkt *pkt, struct wfsopt *opt)
{
    struct wfs_peer *peer = &wfspeer[pkt->src-1];
    wfs_fsm_event_data event_data;
    u8 broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

    log_debug("port %d recv wfsid %d raps request %d status 0x%0x \n",
            adapter->wfs_port, pkt->src, opt->val.raps.request, *(u8 *)&opt->val.raps.status);

    if (opt->val.raps.request == WFSOPT_RAPS_REQ_NR) {
        ixgbe_wfs_fsm_set_event(iwa, pkt->src, E_rapsNR, 0);
    }

    else if (opt->val.raps.request == WFSOPT_RAPS_REQ_SF) {
        /* rapsSF from primary, SF is on secondary, otherwise SF is on primary */
        if (adapter == peer->channel_pri)
            event_data.raps.failure_on_primary = 0;
        else if (adapter == peer->channel_sec)
            event_data.raps.failure_on_primary = 1;
        else {
            /* peer in S_init, no event, no forward */
            pkt->type = WFSPKT_TYPE_CTRL_NONE;
            return;
        }
        event_data.raps.flush = opt->val.raps.status.flush;
        ixgbe_wfs_fsm_set_event(iwa, pkt->src, E_rapsSF, &event_data);
    }

    if (opt->val.raps.status.dnf) {
        /* do-not-forward */
        pkt->type = WFSPKT_TYPE_CTRL_NONE;
    } else {
        /* now trigger RAPS message for my own */
        memcpy(pkt->ethhdr.h_dest, broadcast_mac, 6);
        memcpy(pkt->ethhdr.h_source, myMacAddr, 6);
        pkt->dest = WFSID_ALL;
        pkt->src = myID;
        opt->val.raps.status.dnf = 1; /* won't trigger peer RAPS */
    }
}

#ifdef WFS_BERT
static void wfs_process_bert(struct ixgbe_wfs_adapter *iwa, struct ixgbe_adapter *adapter,
        struct wfspkt *pkt, struct wfsopt *opt, struct sk_buff *skb)
{
    struct wfs_peer *peer = &wfspeer[pkt->src-1];
    struct wfs_bert_cfg *bertcfg;
    u_long elapse;
    u16 csum = 0;
    int wrong_size = 0;
#ifdef __VMKLNX__
	struct timeval current_time;
#endif

    log_debug("port %d recv pkt len %d wfsid %d BERT (%s) seq %d len %d \n",
              adapter->wfs_port, pkt->len, pkt->src,
              opt->val.bert.type == WFSOPT_BERT_RESET ? "config" :
              opt->val.bert.type == WFSOPT_BERT_REQUEST ? "request" : "response",
              opt->val.bert.seqno, opt->val.bert.data_len);
#if 0
    disp_frag((char *)pkt, MIN(60,skb_headlen(skb)));
#endif

    /* calculate size/checksum */
    if ((skb->len - pkt->len) != opt->val.bert.data_len)
        wrong_size = 1;
    else
        csum = ixgbe_wfs_get_bert_skb_csum(iwa, skb);

    /*
     * sender process a response
     */
    if (opt->val.bert.type == WFSOPT_BERT_RESPONSE) {
        /* no foward */
        pkt->type = WFSPKT_TYPE_CTRL_NONE;

        bertcfg = &wfsbert[myID-1];

        if (!bertcfg->on)
            return;

        bertcfg->jfs_last = jiffies;
#ifdef __VMKLNX__
		do_gettimeofday(&current_time);
		elapse = ixgbe_wfs_time_to_usecs(&current_time) - opt->val.bert.ts;
#else
        elapse = ktime_to_us(ktime_get_real()) - opt->val.bert.ts;
#endif /* __VMKLNX__ */

        if (elapse >= 0) {
            if (bertcfg->stats.rtt_avg == 0) {
                bertcfg->stats.rtt_max =
                bertcfg->stats.rtt_min =
                bertcfg->stats.rtt_avg = elapse;
            } else {
                if (elapse > bertcfg->stats.rtt_max)
                    bertcfg->stats.rtt_max = elapse;
                else if (elapse < bertcfg->stats.rtt_min)
                    bertcfg->stats.rtt_min = elapse;

                bertcfg->stats.rtt_avg =
                    (bertcfg->stats.rx_pkts * bertcfg->stats.rtt_avg + elapse) / (bertcfg->stats.rx_pkts + 1);
            }
        }

        bertcfg->stats.rx_pkts++;
        bertcfg->stats.rx_bytes += (pkt->len + bertcfg->data_len);

        if (opt->val.bert.seqno < bertcfg->seqno) {
            log_debug("BERT response seqno %u expect %u\n", opt->val.bert.seqno, bertcfg->seqno);
            //bertcfg->stats.err_drop--;
            bertcfg->stats.err_seq++;
            // seqno no change
        } else if (opt->val.bert.seqno > bertcfg->seqno) {
            bertcfg->stats.err_drop +=
                    (opt->val.bert.seqno - bertcfg->seqno);
            // set next expect seqno
            bertcfg->seqno = opt->val.bert.seqno + 1;
        } else {
            // set next expect seqno
            bertcfg->seqno = opt->val.bert.seqno + 1;
        }

        if (wrong_size) {
            bertcfg->stats.err_size++;
            log_warn("wrong BERT data size len %d expect %d\n",
                    skb->len - pkt->len, opt->val.bert.data_len);
        }

        if (csum != bertcfg->data_csum || opt->val.bert.data_csum != bertcfg->data_csum) {
            log_warn("wrong csum %04x, expect %04x, calculate %04x",
                    opt->val.bert.data_csum, bertcfg->data_csum, csum);
            bertcfg->stats.err_csum++;
        }

        log_debug("BERT response wfsid %d seqno %d data len %d csum %04x\n",
                pkt->src, opt->val.bert.seqno, opt->val.bert.data_len, opt->val.bert.data_csum);
        log_debug("BERT stats %lu pkts %lu bytes, rtt(max %lu min %lu avg %lu us), "
                "error(csum %lu drop %lu seq %lu)\n",
                bertcfg->stats.rx_pkts, bertcfg->stats.rx_bytes,
                bertcfg->stats.rtt_max, bertcfg->stats.rtt_min, bertcfg->stats.rtt_avg,
                bertcfg->stats.err_csum, bertcfg->stats.err_drop,  bertcfg->stats.err_seq);

        return;
    }

    /*
     * responder process reset or request
     */
    bertcfg = &wfsbert[pkt->src-1];

    /* a reset */
    if (opt->val.bert.type == WFSOPT_BERT_RESET) {

        /* no forward */
        pkt->type = WFSPKT_TYPE_CTRL_NONE;

        /* accept correct reset packet only */
        if (wrong_size || csum != opt->val.bert.data_csum) {
            log_info("wrong size or checksum, BERT (config) ignored.\n");
            return;
        }

        bertcfg->data_csum = opt->val.bert.data_csum;
        bertcfg->data_len = opt->val.bert.data_len;
        bertcfg->jfs = bertcfg->jfs_last = jiffies;
        bertcfg->seqno = opt->val.bert.seqno + 1;
        memset(&bertcfg->stats, 0, sizeof(struct wfs_bert_stats));
        log_debug("BERT config wfsid %d seqno %d data len %d csum %04x\n",
                pkt->src, opt->val.bert.seqno, opt->val.bert.data_len, opt->val.bert.data_csum);
    }

    /* a request */
    else if (opt->val.bert.type == WFSOPT_BERT_REQUEST) {

        bertcfg->jfs_last = jiffies;

        if (opt->val.bert.seqno < bertcfg->seqno) {
            //bertcfg->stats.err_drop--;
            bertcfg->stats.err_seq++;
            // seqno no change
        } else if (opt->val.bert.seqno > bertcfg->seqno) {
            bertcfg->stats.err_drop +=
                    (opt->val.bert.seqno - bertcfg->seqno);
            // set next expect seqno
            bertcfg->seqno = opt->val.bert.seqno + 1;
        } else {
            // set next expect seqno
            bertcfg->seqno = opt->val.bert.seqno + 1;
        }

        if (wrong_size) {
            bertcfg->stats.err_size++;
            log_warn("wrong BERT data len %d expect %d\n",
                    skb->len - pkt->len, opt->val.bert.data_len);
        }

        if (csum != bertcfg->data_csum || opt->val.bert.data_csum != bertcfg->data_csum) {
            log_warn("wrong csum %04x, expect %04x, calculate %04x\n",
                    opt->val.bert.data_csum, bertcfg->data_csum, csum);
            bertcfg->stats.err_csum++;
        }

        bertcfg->stats.rx_pkts++;
        bertcfg->stats.rx_bytes += (pkt->len + bertcfg->data_len);

        log_debug("BERT request wfsid %d seqno %d data len %d csum %04x\n",
                pkt->src, opt->val.bert.seqno, opt->val.bert.data_len, opt->val.bert.data_csum);
        log_debug("BERT stats %lu pkts %lu bytes, error(csum %lu drop %lu seq %lu)\n",
                bertcfg->stats.rx_pkts, bertcfg->stats.rx_bytes,
                bertcfg->stats.err_csum, bertcfg->stats.err_drop,  bertcfg->stats.err_seq);

        /* now response back */
        pkt->type = WFSPKT_TYPE_CTRL_BERT;
        opt->val.bert.type = WFSOPT_BERT_RESPONSE;
        opt->val.bert.data_csum = csum;
        memcpy(pkt->ethhdr.h_dest, peer->mac, 6);
        memcpy(pkt->ethhdr.h_source, myMacAddr, 6);
        pkt->dest = pkt->src;
        pkt->src = myID;

        log_debug("port %d sent BERT (response) seq %d len %d \n",
                adapter->wfs_port, opt->val.bert.seqno, skb->len);

        bertcfg->stats.tx_pkts++;
        bertcfg->stats.tx_bytes += (pkt->len + bertcfg->data_len);
    }

    return;
}
#endif

int ixgbe_wfs_recv_control(struct ixgbe_adapter *adapter, struct sk_buff *skb)
{
    struct wfspkt *pkt = (struct wfspkt *)skb->data;
    struct wfsopt *opt = NULL;

    if ((opt = wfspkt_getopt(pkt, WFSOPT_ANNOUNCE)) != NULL) {
        wfs_process_announce(adapter->wfs_parent, adapter, pkt , opt);
    }

    else if ((opt = wfspkt_getopt(pkt, WFSOPT_RAPS)) != NULL) {
        wfs_process_raps(adapter->wfs_parent, adapter, pkt, opt);
    }

#ifdef WFS_BERT
    else if ((opt = wfspkt_getopt(pkt, WFSOPT_BERT)) != NULL) {
        wfs_process_bert(adapter->wfs_parent, adapter, pkt, opt, skb);
    }
#endif

    return 0;
}

/*
 * initial wfs before probe
 */
int ixgbe_wfs_init(struct ixgbe_wfs_adapter *iwa)
{
    log_debug("Enter\n");

    /* initialize adapter */
    spin_lock_init(&iwa->xmit_lock);

#ifdef WFS_DATASEQ
    get_random_bytes(&iwa->data_seqno, sizeof(u32));
#endif

    /* initialize FSM */
    if (ixgbe_wfs_fsm_init(iwa)) {
        log_err("error initialing workflow state machine\n");
        return -ENOMEM;
    }

#ifdef WFS_FIB
    /* initialize FIB */
    if (ixgbe_wfs_fib_init(iwa)) {
        log_err("error creating workflow fowarding table\n");
        return -ENOMEM;
    }
#endif

    return 0;
}

/*
 * initial wfs after probe
 */
int ixgbe_wfs_init2(struct ixgbe_wfs_adapter *iwa)
{
    log_debug("Enter\n");

#ifdef WFS_IOC
    /* initialize IOC */
    if (ixgbe_wfs_ioc_init(iwa)) {
        log_err("error creating workflow control device\n");
        return -ENOMEM;
    }
#endif

#ifdef WFS_BERT
    if (ixgbe_wfs_bert_init(iwa)) {
        log_err("error initialize workflow test control\n");
        return -ENOMEM;
    }
#endif

    return 0;
}

void ixgbe_wfs_cleanup(struct ixgbe_wfs_adapter *iwa)
{

#ifdef WFS_BERT
    ixgbe_wfs_bert_cleanup(iwa);
#endif

#ifdef WFS_IOC
    ixgbe_wfs_ioc_cleanup(iwa);
#endif

#ifdef WFS_FIB
    ixgbe_wfs_fib_cleanup(iwa);
#endif

    /* initialize FSM */
    ixgbe_wfs_fsm_cleanup(iwa);
}


