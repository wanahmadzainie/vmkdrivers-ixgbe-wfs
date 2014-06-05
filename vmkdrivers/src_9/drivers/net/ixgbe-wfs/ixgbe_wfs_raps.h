#ifndef _IXGBE_WFS_RAPS_H_
#define _IXGBE_WFS_RAPS_H_

#include <linux/types.h>

 /*
 * WFS state machine (FSM)
 *
 * FSM Timer
 *   t_announce: timer to transmit announce
 *   t_peer:     timer to update peers TTL
 *   t_rapsSF:   timer to transmit SF alert
 *   t_rapsNR:   timer to transmit NR alert
 *
 * FSM transition table
 *
 *   Workstation state transit based on input event and current state.
 *   Same event may be ignored/triggered different actions for local and remote workstations.
 *
 *
 *           | input           | output
 *  ---------+-----------------+----------------------------------+-----------------------------
 *  state    | event           | actions (Local & Remote)         | next state
 *  ---------+-----------------+----------------------------------+-----------------------------
 *  S_init   | E_discover      | Local:                           | Local/Remote:
 *           |                 |   1. start t_announce            |   if priLink is up
 *           |                 |   2. state check                 |     S_idle
 *           |                 | Remote:                          |   else if secLink is up
 *           |                 |   1. start t_peer                |     S_protect
 *           |                 |   2. update channel              |
 *           |                 |   3. state check                 |
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_expire        | Local/Remote:                    | Local/Remote:
 *           | E_localSF       |   ignore                         |   no change
 *           | E_clear_localSF |                                  |
 *           | E_rapsSF        |                                  |
 *           | E_rapsNR        |                                  |
 *  ---------+-----------------+----------------------------------+-----------------------------
 *  S_idle   | E_discover      | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote:                          | Remote:
 *           |                 |   1. update channel              |   if only secLink is up
 *           |                 |   2. state check                 |     S_protect
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_expire        | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote:                          | Remote:
 *           |                 |   1. update channel              |   if priLink down
 *           |                 |   2. if no active channel,       |     if secLinkup
 *           |                 |      stop t_peer & flush fib     |       S_protect
 *           |                 |                                  |     else
 *           |                 |                                  |       S_init
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_localSF       | Local:                           | Local:
 *           |                 |   1. stop rapsNR                 |   if only secLink is up
 *           |                 |   2. start rapsSF                |     S_protect
 *           |                 |   3. state check                 | Remote:
 *           |                 | Remote:                          |   if only secLink is up
 *           |                 |   1. update channel              |     S_protect
 *           |                 |   2. state check                 |   else
 *           |                 |   3. if no active channel        |      S_init
 *           |                 |        stop t_peer & flush fib   |
 *           +---------------- +----------------------------------+-----------------------------
 *           | E_clear_localSF | Local:                           | Local/Remote:
 *           |                 |   1. stop rapsSF                 |    no change
 *           |                 |   2. start rapsNR                |
 *           |                 |   3. state check                 |
 *           |                 | Remote:                          |
 *           |                 |   ignore                         |
 *           +---------------- +----------------------------------+-----------------------------
 *           | E_rapsSF        | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote                           | Remote:
 *           |                 |   1. update channel              |   if priLink is down
 *           |                 |   2. state check                 |     if secLink is up
 *           |                 |   3. if no active channel        |       S_protect
 *           |                 |        stop t_peer & flush fib   |     else
 *           |                 |                                  |       S_init
 *           +---------------- +----------------------------------+-----------------------------
 *           | E_rapsNR        | Local/Remote:                    | Local/Remote:
 *           |                 |   ignore                         |   no change
 *  ---------+-----------------+----------------------------------+-----------------------------
 * S_protect | E_discover      | Local/Remote:                    | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote:                          | Remote:
 *           |                 |   1. update channel              |   if priLink is up
 *           |                 |   2. state check                 |     S_idle
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_expire        | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote:                          | Remote:
 *           |                 |   1. update channel              |   if priLink is up
 *           |                 |   2. if no active channel,       |      S_idle
 *           |                 |      stop t_peer & flush fib     |   else secLink is down
 *           |                 |                                  |      S_init
 *           |-----------------+----------------------------------+-----------------------------
 *           | E_localSF       | Local:                           | Local:
 *           |                 |   1. stop rapsNR                 |   if only priLink is up
 *           |                 |   2. start rapsSF                |     S_idle
 *           |                 |   3. state check                 | Remote:
 *           |                 | Remote:                          |   if only priLink is up
 *           |                 |   1. update channel              |     S_idle
 *           |                 |   2. state check                 |   else
 *           |                 |   3. if no active channel,       |     S_init
 *           |                 |      stop t_peer & flush fib     |
 *           +---------------- +----------------------------------+-----------------------------
 *           | E_clear_localSF | Local:                           | Local:
 *           |                 |   1. stop rapsSF                 |   if both link up
 *           |                 |   2. start rapsNR                |      S_idle
 *           |                 |   3. state check                 | Remote:
 *           |                 | Remote:                          |   no change
 *           |                 |   ignore                         |
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_rapsSF        | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote                           | Remote:
 *           |                 |   1. update channel              |   if secLink is down
 *           |                 |   2. state check                 |     if priLink is up
 *           |                 |   3. if no active channel        |       S_idle
 *           |                 |        stop t_peer & flush fib   |     else
 *           |                 |                                  |       S_init
 *           +-----------------+----------------------------------+-----------------------------
 *           | E_rapsNR        | Local:                           | Local:
 *           |                 |   ignore                         |   no change
 *           |                 | Remote:                          | Remote:
 *           |                 |   state_check                    |   if both link are up
 *           |                 |                                  |     S_idle
 *  ---------+-----------------+----------------------------------+-----------------------------
 *
 *
 */

struct ixgbe_wfs_adapter;

typedef enum {
    S_init,         /* initial state */
    S_idle,         /* primary channel is active */
    S_protect,      /* secondary channel is active */
    S_max,
    S_Invalid,
} wfs_fsm_state;

typedef enum {
    E_discover,     /* local, highest priority */
    E_expire,       /* local */
    E_localSF,      /* local */
    E_clear_localSF,/* local */
    E_rapsSF,       /* remote */
    E_rapsNR,       /* remote */
    E_max,
    E_Invalid
} wfs_fsm_event;

/* event buffer */
typedef struct {
    u8 is_primary_annouce;
    u8 peer_port;
    struct ixgbe_adapter *recv_channel;
} wfs_fsm_announce_event_data;

typedef struct {
    u8 failure_on_primary;
    u8 flush;
} wfs_fsm_raps_event_data;

typedef union {
    wfs_fsm_announce_event_data announce;
    wfs_fsm_raps_event_data raps;
} wfs_fsm_event_data;

#define EVBUF_MIN_NUM          64
#define EVBUF_MAX_NUM          256

typedef struct wfs_fsm_event_buffer {
    struct wfs_fsm_event_buffer *next;       /* used by use/free buffer list */
    struct wfs_fsm_event_buffer *next_eb;    /* used by allocated buffer list */
    u8 wfsid;
    wfs_fsm_event event;
    wfs_fsm_event_data data;
} wfs_fsm_event_buffer;


/* FSM transition table */
typedef struct {
    int valid_entry;
    wfs_fsm_state (*action)(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event_data *evdata);
} wfs_fsm_trans_table;


/*
 * WFS Workstation data
 */
struct wfs_peer {
    u8 mac[6];      /* peer's MAC address */
    u32 ip;         /* peer's IP address */
    short port_pri; /* peer's primary port number */
    short port_sec; /* peer's secondary port number */
    short ttl_pri;  /* time to live(expire) for peer's primary port */
    short ttl_sec;  /* time to live(expire) for peer's secondary port */
    struct ixgbe_adapter *channel_pri;    /* local channel toward peer's primary port */
    struct ixgbe_adapter *channel_sec;    /* local channel toward peer's secondary port */
    wfs_fsm_state fsm_state;  /* peer's current state */
};

/*
 * WFS Ring Automatic Protection Switch (RAPS)
 */
#define RAPS_TICK               (1*HZ)  /* 1 sec */
#define RAPS_ANNOUNCE_INT       (5*HZ)  /* announce interval */
#define RAPS_PEER_TIMEOUT       (30*HZ) /* peers timeout */
#define RAPS_SF_INT             (1*HZ)  /* rapsSF tx interval */
#define RAPS_NR_INT             (1*HZ)  /* rapsNR tx interval */
#define RAPS_NR_COUNT           (2*RAPS_ANNOUNCE_INT/RAPS_TICK) /* rapsNR tx count */

#define RAPS_CTRL_PEER_TTL       0x00000001
#define RAPS_CTRL_ANNOUNCE       0x00000002
#define RAPS_CTRL_NR             0x00000004
#define RAPS_CTRL_SF             0x00000008
#define RAPS_CTRL_FSM_EVENT      0x00000010

struct raps_timer_ctrl {
    u32 flag;       /* timer control flag */
    int t_announce; /* announce timer */
    int t_peer;     /* peer ttl timer */
    int t_rapsSF;   /* rapsSF timer */
    int t_rapsNR;   /* rapsNR timer */
    int c_rapsNR;   /* rapsNR packet count */
    wfs_fsm_event_buffer *fsm_event_head; /* events */
    wfs_fsm_event_buffer *fsm_event_tail;
};

extern int ixgbe_wfs_fsm_init(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_fsm_cleanup(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_fsm_set_event(struct ixgbe_wfs_adapter *iwa, u8 wfsid, wfs_fsm_event e, wfs_fsm_event_data *evdata);
extern void ixgbe_wfs_raps_start(struct ixgbe_wfs_adapter *iwa);
extern void ixgbe_wfs_raps_stop(struct ixgbe_wfs_adapter *iwa);

#endif /* _IXGBE_WFS_RAPS_H_ */
