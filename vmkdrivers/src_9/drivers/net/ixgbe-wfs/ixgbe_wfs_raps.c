#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
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
#include "ixgbe.h"
#include "ixgbe_wfs.h"

/* FSM event and control */
static wfs_fsm_trans_table fsm[S_max][E_max][2];
static bool wfs_fsm_trans_table_initialized = false;

/* macro for individual iwa */
#define myID            (iwa->wfs_id)
#define myMacAddr       (iwa->mac_addr)
#define myIP            (iwa->ip)
#define raps_timer      (iwa->raps_timer)
#define raps_ctrl       (iwa->raps_ctrl)
#define raps_lock       (iwa->raps_lock)
#define fsm_lock        (iwa->fsm_lock)
#define wfspeer         (iwa->wfspeer)
#define fsmEvBufs       (iwa->fsmEvBufs)
#define fsmEvBuf_lock   (iwa->fsmEvBuf_lock)
#define fsmEvBuf_head   (iwa->fsmEvBuf_head)
#define fsmEvBuf_tail   (iwa->fsmEvBuf_tail)
#define fsmEvBuf_count  (iwa->fsmEvBuf_count)

/*
 * FSM support functions
 */
static void wfs_fsm_close_event(struct ixgbe_wfs_adapter *iwa, wfs_fsm_event_buffer *n)
{
    BUG_ON(n == NULL);

    spin_lock_bh(&fsmEvBuf_lock);

    n->next = NULL;
    if (fsmEvBuf_tail) {
        fsmEvBuf_tail->next = n;
        fsmEvBuf_tail = n;
    } else {
        fsmEvBuf_head = fsmEvBuf_tail = n;
    }

    spin_unlock_bh(&fsmEvBuf_lock);
}

static wfs_fsm_event_buffer *wfs_fsm_alloc_event(struct ixgbe_wfs_adapter *iwa)
{
    wfs_fsm_event_buffer *n = NULL;

    spin_lock_bh(&fsmEvBuf_lock);

    if (fsmEvBuf_head)
    {   /* get free buffer */
        n = fsmEvBuf_head;
        fsmEvBuf_head = n->next;
        if (fsmEvBuf_head == NULL)
            fsmEvBuf_tail = NULL;
    }
    else if (fsmEvBuf_count < EVBUF_MAX_NUM)
    {   /* no free buffer, allocate one and add to fsmEvBufs */
        n = kmalloc(sizeof(wfs_fsm_event_buffer), GFP_ATOMIC);
        if (n == NULL) {
            log_err("error allocating event buffer descriptor\n");
        } else {
            n->next_eb = fsmEvBufs;
            fsmEvBufs = n;
            fsmEvBuf_count++;
        }
    }

    spin_unlock_bh(&fsmEvBuf_lock);

    return n;
}

void ixgbe_wfs_fsm_set_event(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event e, wfs_fsm_event_data *evdata)
{
    wfs_fsm_event_buffer *n;
    struct wfs_peer *peer;
    int id;


    if (iwa->state != opened) {
        log_debug("not in open state\n");
        return;
    }

    spin_lock_bh(&fsm_lock);

    for (id=WFSID_MIN; id<=WFSID_MAX; id++) {
        if (wfsid != WFSID_ALL && wfsid != id)
            continue;

        peer = &wfspeer[id-1];
        if (!fsm[peer->fsm_state][e][id==myID].valid_entry) {
            log_debug("FSM wfsid %d set event %d ignored, current state %d\n", id, e, peer->fsm_state);
            continue;
        }

        if ((n = wfs_fsm_alloc_event(iwa)) == NULL) {
            log_err("no more event buffer\n");
            break;
        }
        n->wfsid = id;
        n->event = e;
        if (evdata)
            memcpy(&n->data, evdata, sizeof(wfs_fsm_event_data));
        else
            memset(&n->data, 0, sizeof(wfs_fsm_event_data));

        n->next = NULL;
        if (raps_ctrl.fsm_event_tail) {
            raps_ctrl.fsm_event_tail->next = n;
            raps_ctrl.fsm_event_tail = n;
        } else {
            raps_ctrl.fsm_event_head = raps_ctrl.fsm_event_tail = n;
        }
        raps_ctrl.flag |= RAPS_CTRL_FSM_EVENT;
        log_debug("FSM wfsid %d event %d enqueue\n", id, e);
    }

    spin_unlock_bh(&fsm_lock);

}

static wfs_fsm_state wfs_fsm_action_discover(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_discover\n", wfsid);

    if (wfsid == myID) {
        /* start t_announce timer */
        raps_ctrl.flag |= RAPS_CTRL_ANNOUNCE;
        raps_ctrl.t_announce = RAPS_ANNOUNCE_INT;
    } else {
        /* start t_peer timer */
        raps_ctrl.flag |= RAPS_CTRL_PEER_TTL;
        raps_ctrl.t_peer = RAPS_PEER_TIMEOUT;

        /* update channels */
        if (evdata->announce.is_primary_annouce) {
            peer->channel_pri = evdata->announce.recv_channel;
            peer->port_pri = evdata->announce.peer_port;
            peer->ttl_pri = RAPS_PEER_TIMEOUT;
            /* in case peer switch ports */
            if (peer->channel_sec == peer->channel_pri) {
                peer->channel_sec = NULL;
                peer->port_sec = -1;
            }
        } else {
            peer->channel_sec = evdata->announce.recv_channel;
            peer->port_sec = evdata->announce.peer_port;
            peer->ttl_sec = RAPS_PEER_TIMEOUT;
            /* in case peer switch ports */
            if (peer->channel_pri == peer->channel_sec) {
                peer->channel_pri = NULL;
                peer->port_pri = -1;
            }
        }
    }

    /* link status based on discovered channel */
    priLinkup = (peer->channel_pri != NULL);
    secLinkup = (peer->channel_sec != NULL);

    if (peer->fsm_state == S_init) {
        if (priLinkup) {
            peer->fsm_state = S_idle;
            log_info( "** Workstation #%d(%s) initialized, enter IDLE state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        } else if (secLinkup) {
            peer->fsm_state = S_protect;
            log_info( "** Workstation #%d(%s) initialized, enter PROTECT state\n",
                wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    /* since this is discover message, at least on link is up */
    else if (peer->fsm_state == S_idle && !priLinkup) {
        peer->fsm_state = S_protect;
        log_info( "** Workstation #%d(%s) re-initialized, enter PROTECT state\n",
                wfsid, wfsid==myID ? "local" : "remote" );
    }

    else if (peer->fsm_state == S_protect) {
        if (priLinkup) {
            peer->fsm_state = S_idle;
            log_info( "** Workstation #%d(%s) re-initialized, enter IDLE state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    if (peer->fsm_state != S_init) {
#ifdef WFS_FIB
        ixgbe_wfs_fib_update(iwa, peer->mac, peer->ip, wfsid);
#endif
    }

    return peer->fsm_state;
}

static wfs_fsm_state wfs_fsm_action_expire(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_expire\n", wfsid);

    /* expire event is for remtoe peers only */
    BUG_ON(wfsid == myID);

    /* update channel */
    if (peer->ttl_pri <= 0) {
        peer->channel_pri = NULL;
        peer->port_pri = -1;
    }
    if (peer->ttl_sec <= 0) {
        peer->channel_sec = NULL;
        peer->port_sec = -1;
    }

    /* link status based on expired channel */
    priLinkup = (peer->channel_pri != NULL);
    secLinkup = (peer->channel_sec != NULL);

    /* both links may down */
    if (peer->fsm_state == S_idle && !priLinkup) {
        if (secLinkup) {
            peer->fsm_state = S_protect;
            log_debug("wfsid %d primary channel expired, switch to secondary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter PROTECT state\n",
                        wfsid, wfsid==myID ? "local" : "remote" );
        } else {
            peer->fsm_state = S_init;
            log_debug("wfsid %d primary channel expired, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    else if (peer->fsm_state == S_protect) {
        if (priLinkup) {
            peer->fsm_state = S_idle;
            log_debug("wfsid %d secondary channel expired, switch to primary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter IDLE state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        } else if (!secLinkup) {
            peer->fsm_state = S_init;
            log_debug("wfsid %d secondary channel expired, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    if (peer->fsm_state == S_init) {
#ifdef WFS_FIB
        ixgbe_wfs_fib_delete_wfsid(iwa, wfsid);
#endif
        /* t_peer shutdown in wfs_update_peers() */
    }

    return peer->fsm_state;
}

static wfs_fsm_state wfs_fsm_action_localSF(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_localSF\n", wfsid);

    /* link status based on assigned channel and PHY status */
    priLinkup = (peer->channel_pri && peer->channel_pri->link_up);
    secLinkup = (peer->channel_sec && peer->channel_sec->link_up);

    if (wfsid == myID) {
        raps_ctrl.flag |= RAPS_CTRL_SF;
        raps_ctrl.flag &= ~RAPS_CTRL_NR;
        raps_ctrl.t_rapsSF = RAPS_SF_INT;
    } else {
        if (!priLinkup) {
            peer->channel_pri = NULL;
            peer->port_pri = -1;
        }
        if (!secLinkup) {
            peer->channel_sec = NULL;
            peer->port_sec = -1;
        }
    }

    /* both links may down (eg. single cable switch around) */
    if (peer->fsm_state == S_idle && !priLinkup) {
        if (secLinkup) {
            peer->fsm_state = S_protect;
            log_debug("wfsid %d primary link failure, switch to secondary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter PROTECT state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        } else if (wfsid != myID) {
            peer->fsm_state = S_init;
            log_debug("wfsid %d primary link failure, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    else if (peer->fsm_state == S_protect && !secLinkup)  {
        if (priLinkup) {
            peer->fsm_state = S_idle;
            log_debug("wfsid %d secondary link failure, switch to primary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter IDLE state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        } else if (wfsid != myID) {
            peer->fsm_state = S_init;
            log_debug("wfsid %d secondary link failure, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                            wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

#ifdef WFS_FIB
    if (peer->fsm_state == S_init) {
        ixgbe_wfs_fib_delete_wfsid(iwa, wfsid);
        /* t_peer shutdown in wfs_update_peers() */
    }
#endif

    return peer->fsm_state;
}

static wfs_fsm_state wfs_fsm_action_clear_localSF(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_clear_localSF\n", wfsid);

    /* expire event is for local only */
    BUG_ON(wfsid != myID);

    raps_ctrl.flag &= ~RAPS_CTRL_SF;
    raps_ctrl.flag |= RAPS_CTRL_NR;
    raps_ctrl.t_rapsNR = RAPS_NR_INT;
    raps_ctrl.c_rapsNR = RAPS_NR_COUNT;

    /* link status based on assigned channel and PHY status */
    priLinkup = (peer->channel_pri && peer->channel_pri->link_up);
    secLinkup = (peer->channel_sec && peer->channel_sec->link_up);

    if (priLinkup && secLinkup) {
        if (peer->fsm_state == S_protect) {
            peer->fsm_state = S_idle;
            log_debug("link recovered, wfsid %d revert to primary channel %d\n",
                    wfsid, peer->channel_sec->wfs_port);
            log_info( "** Workstation #%d(%s) enter IDLE state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    return peer->fsm_state;
}

static wfs_fsm_state wfs_fsm_action_rapsSF(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_rapsSF\n", wfsid);

    /* expire event is for remote only */
    BUG_ON(wfsid == myID);

    if (evdata->raps.failure_on_primary) {
        peer->channel_pri = NULL;
        peer->port_pri = -1;
    } else {
        peer->channel_sec = NULL;
        peer->port_sec = -1;
    }

    /* link status based on assigned channel */
    priLinkup = (peer->channel_pri != NULL);
    secLinkup = (peer->channel_sec != NULL);

    /* both links may down (eg. single cable switch around) */
    if (peer->fsm_state == S_idle && !priLinkup) {
        if (secLinkup) {
            peer->fsm_state = S_protect;
            log_debug("rapsSF received, wfsid %d primary failure, switch to secondary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter PROTECT state\n",
                wfsid, wfsid==myID ? "local" : "remote" );
        } else {
            peer->fsm_state = S_init;
            log_debug("rapsSF received, wfsid %d primary failure, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

    else if (peer->fsm_state == S_protect && !secLinkup) {
        if (priLinkup) {
            peer->fsm_state = S_idle;
            log_debug("rapsSF received, wfsid %d secondary failure, switch to primary\n", wfsid);
            log_info( "** Workstation #%d(%s) enter IDLE state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        }else {
            peer->fsm_state = S_init;
            log_debug("rapsSF received, wfsid %d secondary failure, no active channel available, reset to S_init\n", wfsid);
            log_info( "** Workstation #%d(%s) enter INIT state\n",
                    wfsid, wfsid==myID ? "local" : "remote" );
        }
    }

#ifdef WFS_FIB
    if (peer->fsm_state == S_init) {
        ixgbe_wfs_fib_delete_wfsid(iwa, wfsid);
        /* t_peer shutdown in wfs_update_peers() */
    }
#endif

    return peer->fsm_state;
}

static wfs_fsm_state wfs_fsm_action_rapsNR(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata)
{
    struct wfs_peer *peer = &wfspeer[wfsid-1];
    int priLinkup, secLinkup;

    log_debug("FSM wfsid %d action E_rapsNR\n", wfsid);

    priLinkup = (peer->channel_pri && peer->channel_pri->link_up);
    secLinkup = (peer->channel_sec && peer->channel_sec->link_up);

    if (priLinkup && secLinkup) {
        peer->fsm_state = S_idle;
        log_debug("rapsNR received, wfsid %d set to primary channel %d\n",
                wfsid, peer->channel_sec->wfs_port);
        log_info( "** Workstation #%d(%s) enter IDLE state\n",
                wfsid, wfsid==myID ? "local" : "remote" );
    }

    return peer->fsm_state;
}

int ixgbe_wfs_fsm_init(struct ixgbe_wfs_adapter *iwa)
{
    const int M_local = 1, M_remote = 0;
    wfs_fsm_event_buffer *eb;
    int i;

    if (!wfs_fsm_trans_table_initialized) {
    memset(fsm, 0, sizeof(fsm));

    /* S_init */
    fsm[S_init][E_discover][M_local].valid_entry =
    fsm[S_init][E_discover][M_remote].valid_entry = 1;
    fsm[S_init][E_discover][M_local].action =
    fsm[S_init][E_discover][M_remote].action = wfs_fsm_action_discover;

    /* S_idle */
    fsm[S_idle][E_discover][M_remote].valid_entry = 1;
    fsm[S_idle][E_discover][M_remote].action = wfs_fsm_action_discover;

    fsm[S_idle][E_expire][M_remote].valid_entry = 1;
    fsm[S_idle][E_expire][M_remote].action = wfs_fsm_action_expire;

    fsm[S_idle][E_localSF][M_local].valid_entry =
    fsm[S_idle][E_localSF][M_remote].valid_entry = 1;
    fsm[S_idle][E_localSF][M_local].action =
    fsm[S_idle][E_localSF][M_remote].action = wfs_fsm_action_localSF;

    fsm[S_idle][E_clear_localSF][M_local].valid_entry = 1;
    fsm[S_idle][E_clear_localSF][M_local].action = wfs_fsm_action_clear_localSF;

    fsm[S_idle][E_rapsSF][M_remote].valid_entry = 1;
    fsm[S_idle][E_rapsSF][M_remote].action = wfs_fsm_action_rapsSF;

    /* S_protect */
    fsm[S_protect][E_discover][M_remote].valid_entry = 1;
    fsm[S_protect][E_discover][M_remote].action = wfs_fsm_action_discover;

    fsm[S_protect][E_expire][M_remote].valid_entry = 1;
    fsm[S_protect][E_expire][M_remote].action = wfs_fsm_action_expire;

    fsm[S_protect][E_localSF][M_local].valid_entry =
    fsm[S_protect][E_localSF][M_remote].valid_entry = 1;
    fsm[S_protect][E_localSF][M_local].action =
    fsm[S_protect][E_localSF][M_remote].action = wfs_fsm_action_localSF;

    fsm[S_protect][E_clear_localSF][M_local].valid_entry = 1;
    fsm[S_protect][E_clear_localSF][M_local].action = wfs_fsm_action_clear_localSF;

    fsm[S_protect][E_rapsSF][M_remote].valid_entry = 1;
    fsm[S_protect][E_rapsSF][M_remote].action = wfs_fsm_action_rapsSF;

    fsm[S_protect][E_rapsNR][M_remote].valid_entry = 1;
    fsm[S_protect][E_rapsNR][M_remote].action = wfs_fsm_action_rapsNR;

    wfs_fsm_trans_table_initialized = true;
    }

    /* allocate WFS_FSM event buffer */
    spin_lock_init(&fsmEvBuf_lock);
    for (i=0; i<EVBUF_MIN_NUM; i++) {
        eb = wfs_fsm_alloc_event(iwa);
        if (!eb) {
            log_err("error allocating event buffer\n");
            return -ENOMEM;
        }
    }
    log_info("%d event buffer allocated\n", EVBUF_MIN_NUM);
    for (eb=fsmEvBufs; eb; eb=eb->next_eb) {
        wfs_fsm_close_event(iwa, eb);
    }

    return 0;
}

void ixgbe_wfs_fsm_cleanup(struct ixgbe_wfs_adapter *iwa)
{
    wfs_fsm_event_buffer *eb, *nexteb;
    int i;

    spin_lock_bh(&fsmEvBuf_lock);
    for (i=0,eb=fsmEvBufs; eb; i++,eb=nexteb) {
        nexteb = eb->next_eb;
        kfree(eb);
    }
    spin_unlock_bh(&fsmEvBuf_lock);
    log_info("free %d/%d event buffer\n", i, fsmEvBuf_count);
}

static void wfs_fsm_run(struct ixgbe_wfs_adapter *iwa)
{
    wfs_fsm_event_buffer *eb, *nexteb;
    wfs_fsm_state old_state;
    struct wfs_peer *peer;

    spin_lock_bh(&fsm_lock);

    for (eb=raps_ctrl.fsm_event_head; eb; eb=nexteb) {
        BUG_ON(eb->wfsid<WFSID_MIN || eb->wfsid>WFSID_MAX);
        peer = &wfspeer[eb->wfsid-1];
        nexteb = eb->next;
        old_state = peer->fsm_state;

        if (!fsm[peer->fsm_state][eb->event][eb->wfsid==myID].valid_entry) {
            log_debug("FSM set wfsid %d event %d ignored, current state %d\n",
                    eb->wfsid, eb->event, peer->fsm_state);
            wfs_fsm_close_event(iwa, eb);
            continue;
        }

        fsm[peer->fsm_state][eb->event][eb->wfsid==iwa->wfs_id].action(iwa, eb->wfsid, &eb->data);
        log_debug("FSM wfsid %d event %d processed, state %d --> %d\n",
                eb->wfsid, eb->event, old_state, peer->fsm_state);

        /* validate peer data */
#if 0
        if ((peer->fsm_state == S_idle && peer->channel_pri == NULL) ||
                (peer->fsm_state == S_protect && peer->channel_sec == NULL))
        {
            /*
             * for FSM DEBUG:
             *    if you see this log, means your state machine has problem,
             *
             */
            int idle_error = (peer->fsm_state == S_idle && peer->channel_pri == NULL);
            int protect_error = (peer->fsm_state == S_protect && peer->channel_sec == NULL);

            printk("**********************************************************\n");
            printk("FSM wfsid %d event %d processed, state %d --> %d, idle error %d, protect error %d\n",
                            eb->wfsid, eb->event, old_state, peer->fsm_state,
                            idle_error, protect_error);
            printk("**********************************************************\n");
            if (eb->wfsid != myID)
                peer->fsm_state = S_init;
        }
#else
        BUG_ON(peer->fsm_state == S_idle && peer->channel_pri == NULL);
        BUG_ON(peer->fsm_state == S_protect && peer->channel_sec == NULL);
#endif
        wfs_fsm_close_event(iwa, eb);
    }

    raps_ctrl.fsm_event_head = raps_ctrl.fsm_event_tail = NULL;
    raps_ctrl.flag &= ~RAPS_CTRL_FSM_EVENT;

    spin_unlock_bh(&fsm_lock);
}


/*
 * RAPS supporting functions
 */


static void wfs_update_peers(struct ixgbe_wfs_adapter *iwa)
{
    struct wfs_peer *peer;
    int id, count, sent_expired;

    for (id=WFSID_MIN,count=0; id<=WFSID_MAX; id++) {
        peer = &wfspeer[id-1];
        if (peer->fsm_state == S_init || id == myID)
            continue;

        count++; sent_expired = 0;
        if (peer->ttl_pri > 0) {
            peer->ttl_pri -= RAPS_TICK;
            sent_expired += (peer->ttl_pri <= 0);
        }
        if (peer->ttl_sec > 0) {
            peer->ttl_sec -= RAPS_TICK;
            sent_expired += (peer->ttl_sec <= 0);
        }
        if (sent_expired)
            ixgbe_wfs_fsm_set_event(iwa, id, E_expire, 0);
    }

    if (count == 0) {
        /* if no peers active, stop t_peer */
        raps_ctrl.flag &= ~RAPS_CTRL_PEER_TTL;
    }
}

static void raps_main(unsigned long data)
{
    struct ixgbe_wfs_adapter *iwa = (struct ixgbe_wfs_adapter *)data;

    spin_lock_bh(&raps_lock);
    log_debug("Enter, raps_ctrl.flag = %04x\n", raps_ctrl.flag);

    /* process FSM event */
    if (raps_ctrl.flag & RAPS_CTRL_FSM_EVENT) {
        wfs_fsm_run(iwa);
    }

    /* process announce timer */
    if (raps_ctrl.flag & RAPS_CTRL_ANNOUNCE) {
        raps_ctrl.t_announce -= RAPS_TICK;
        if (raps_ctrl.t_announce <= 0) {
            ixgbe_wfs_send_announce(iwa);
            // set next announce
            raps_ctrl.t_announce = RAPS_ANNOUNCE_INT;
        }
    }

    /* process raps timer */
    if (raps_ctrl.flag & RAPS_CTRL_NR) {
        raps_ctrl.t_rapsNR -= RAPS_TICK;
        if (raps_ctrl.t_rapsNR <= 0) {
            /* sending multiple rapsNR */
            if (raps_ctrl.c_rapsNR > 0) {
                ixgbe_wfs_send_raps(iwa, E_rapsNR);
                raps_ctrl.c_rapsNR--;
                raps_ctrl.t_rapsNR = RAPS_NR_INT;
            } else
                raps_ctrl.flag &= ~RAPS_CTRL_NR;
        }
    }

    if (raps_ctrl.flag & RAPS_CTRL_SF) {
        raps_ctrl.t_rapsSF -= RAPS_TICK;
        if (raps_ctrl.t_rapsSF <= 0) {
            ixgbe_wfs_send_raps(iwa, E_rapsSF);
            raps_ctrl.t_rapsNR = RAPS_SF_INT;
        }
    }

    /* process peer ttl timer */
    if (raps_ctrl.flag & RAPS_CTRL_PEER_TTL) {
        wfs_update_peers(iwa);
    }

    /* misc */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
    wfspeer[myID-1].ip = myIP =
            ((struct in_device *)iwa->ndev->ip_ptr)->ifa_list == NULL ?
                    0 : ntohl(((struct in_device *)iwa->ndev->ip_ptr)->ifa_list[0].ifa_address);
#else
    wfspeer[myID-1].ip = myIP =
            iwa->ndev->ip_ptr->ifa_list == NULL ? 0 : ntohl(iwa->ndev->ip_ptr->ifa_list[0].ifa_address);
#endif

    /* Set up the timer so we'll get called again. */
    raps_timer.expires = jiffies + RAPS_TICK;
    add_timer(&raps_timer);

    spin_unlock_bh(&raps_lock);
}

/*
 * RAPS controls
 */
void ixgbe_wfs_raps_start(struct ixgbe_wfs_adapter *iwa)
{
    /* initialize RAPS control */
    memset(&raps_ctrl, 0, sizeof(struct raps_timer_ctrl));
    spin_lock_init(&raps_lock);

    /* initialize state machine */
    spin_lock_init(&fsm_lock);
    memset(wfspeer, 0, sizeof(wfspeer));

    memcpy(wfspeer[myID-1].mac, myMacAddr, 6);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
    wfspeer[myID-1].ip = myIP =
            ((struct in_device *)iwa->ndev->ip_ptr)->ifa_list == NULL ?
                    0 : ntohl(((struct in_device *)iwa->ndev->ip_ptr)->ifa_list[0].ifa_address);
#else
    wfspeer[myID-1].ip = myIP =
            iwa->ndev->ip_ptr->ifa_list == NULL ? 0 : ntohl(iwa->ndev->ip_ptr->ifa_list[0].ifa_address);
#endif

    wfspeer[myID-1].channel_pri = iwa->primary;
    wfspeer[myID-1].channel_sec = iwa->secondary;
    wfspeer[myID-1].port_pri = iwa->primary->wfs_port;
    wfspeer[myID-1].port_sec = iwa->secondary->wfs_port;
    wfspeer[myID-1].ttl_pri = wfspeer[myID-1].ttl_sec = RAPS_PEER_TIMEOUT;

    /* kick off local */
    ixgbe_wfs_fsm_set_event(iwa, myID, E_discover, 0);

    /* Fire RAPS engine */
    raps_timer.expires = jiffies + RAPS_TICK;
    raps_timer.data = (unsigned long)iwa;
    raps_timer.function = &raps_main;
    init_timer(&raps_timer);
    add_timer(&raps_timer);
}

void ixgbe_wfs_raps_stop(struct ixgbe_wfs_adapter *iwa)
{
    /* stop BERT if it's running */
    ixgbe_wfs_bert_stop_request(iwa);

    /* stop RAPS engine */
    spin_lock_bh(&raps_lock);
    del_timer_sync(&raps_timer);
    spin_unlock_bh(&raps_lock);
}
