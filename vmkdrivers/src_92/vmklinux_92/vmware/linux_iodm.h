/* ****************************************************************
 * Copyright 2008-2011 VMware, Inc.
 * * ****************************************************************/
#ifndef _IODM_IOCTL_H_
#define _IODM_IOCTL_H_

#if !defined(__VMKLNX__)  /* user space esxcli code shares a copy of this file */
#include <stdint.h>
#endif

#include "vmkapi.h"

/*
 * The current transport between the user mode and the kernel mode is a char
 * node. This node is a 2-way street. While the user makes a request going
 * down, the kernel fills in the response coming back up
 */
#define IODM_CHAR_NAME  		"iodm"
#define IODM_DEV_NAME                 "/vmfs/devices/char/vmkdriver/" IODM_CHAR_NAME

/*
 * The largest of the payload size. The Device List takes much of this size
 */
#define MAX_IODM_IOCTL_CMD_SIZE	3072

#define MAX_IODM_NAMELEN	32
#define MAX_IODM_DEVICES	32

/*
 * IODM Command codes - Sent down by the application
 */
#define IODM_IOCTL_GET_IODMDEVS         0x0001
#define IODM_IOCTL_GET_EVENTS           0x0002
#define IODM_IOCTL_GET_HOST_STATS       0x0004
#define IODM_IOCTL_GET_HOST_ATTR        0x0008
#define IODM_IOCTL_ISSUE_RESET          0x0010
#define IODM_IOCTL_CLEAR_EVENTS         0x0020
#define IODM_IOCTL_GET_EVENTCNT         0x0040

/*
 * IODM Response codes - Sent up by the kernel
 */
#define IODM_IOCTL_RESPONSE_CODE     0x00010000

/*
 *  IODM Ioctl Status codes
 */
#define IODM_CMD_FAILED			0xFFFF
#define IODM_CMD_NOT_SUPPORTED		0xFFFE

#define IODM_TRANSPORT_TYPE_UNKNOWN    0x0000000000000000UL
#define IODM_TRANSPORT_TYPE_FC         0x0000000000000001UL
#define IODM_TRANSPORT_TYPE_FCOE       0x0000000000000002UL
#define IODM_TRANSPORT_TYPE_ISCSI      0x0000000000000004UL
#define IODM_TRANSPORT_TYPE_SAS        0x0000000000000008UL

/*
 * IODM Ioctl Delivery Mechanism - Goal is to keep this under 4K
 */
struct iodm_ioc {
  uint32_t 	cmd;
  uint32_t	status;
  uint32_t	len;
  char		vmhba_name[MAX_IODM_NAMELEN];
  char		data[MAX_IODM_IOCTL_CMD_SIZE];
};

struct iodm_name {
  char 		vmhba_name[MAX_IODM_NAMELEN];
  char 		driver_name[MAX_IODM_NAMELEN];
  uint32_t     	transport;
};

struct iodm_list {
  char		    count;
  struct iodm_name  dev_name[MAX_IODM_DEVICES];	/* 32 device names */
};

/*
 * Scsi Event struct
 */

typedef struct iodm_ScsiEvent {
   uint64_t          sec;     /* time stamp second */
   uint32_t          usec;       /* time stamp microsecond */
   uint32_t          id;         /* event type id */
   uint32_t          cmd;        /* scsi opcode */
   uint32_t          status;     /* scsi status */
   uint16_t          channel;
   uint16_t          target;
   uint16_t          lun;
   uint8_t           cc;         /* check_condition */
   char              sense[3];   /* sense key and ascq */
   uint64_t          flags;
   uint32_t          data[4];    /* private data */
}__attribute__((packed)) iodm_ScsiEvent_t;


#define FCPORTSPEED_1GBIT              1
#define FCPORTSPEED_2GBIT              2
#define FCPORTSPEED_4GBIT              4
#define FCPORTSPEED_10GBIT             8
#define FCPORTSPEED_8GBIT              0x10
#define FCPORTSPEED_16GBIT             0x20
#define FCPORTSPEED_NOT_NEGOTIATED     (1 << 15) /* Speed not established */

/*
 * FC Port Type
 */
enum fcport_type {
   FCPORTTYPE_UNKNOWN,
   FCPORTTYPE_OTHER,
   FCPORTTYPE_NPORT,              /* Attached to FPort */
   FCPORTTYPE_NLPORT,             /* (Public) Loop w/ FLPort */
   FCPORTTYPE_LPORT,              /* (Private) Loop w/o FLPort */
   FCPORTTYPE_PTP,                /* Point to Point w/ another NPort */
   FCPORTTYPE_NPIV,               /* VPORT based on NPIV */
   FCPORTTYPE_VNPORT,             /* Virtual N Port for FCoE */
};

/*
 * fc_port_state:
 */
enum fcport_state {
   FCPORTSTATE_UNKNOWN,
   FCPORTSTATE_NOTPRESENT,
   FCPORTSTATE_ONLINE,
   FCPORTSTATE_OFFLINE,           /* User has taken Port Offline */
   FCPORTSTATE_BLOCKED,
   FCPORTSTATE_BYPASSED,
   FCPORTSTATE_DIAGNOSTICS,
   FCPORTSTATE_LINKDOWN,
   FCPORTSTATE_ERROR,
   FCPORTSTATE_LOOPBACK,
   FCPORTSTATE_DELETED,
};

struct fchost_statistics {
   uint64_t tx_frames;
   uint64_t rx_frames;
   uint64_t lip_count;
   uint64_t error_frames;
   uint64_t dumped_frames;
   uint64_t link_failure_count;
   uint64_t loss_of_signal_count;
   uint64_t prim_seq_protocol_err_count;
   uint64_t invalid_tx_word_count;
   uint64_t invalid_crc_count;

   /* FCP Data */
   uint64_t fcp_input_requests;
   uint64_t fcp_output_requests;
   uint64_t fcp_control_requests;
};

typedef struct iodm_fc_attrs {
   uint32_t port_id;
   uint64_t node_name;
   uint64_t port_name;
   uint32_t speed;
   enum fcport_type port_type;
   enum fcport_state port_state;
} __attribute__((packed)) iodm_fc_attrs_t;

/**
 * struct fcoe_dev_stats - fcoe stats structure
 */
struct fcoedev_stats {
   uint64_t     TxFrames;
   uint64_t     RxFrames;
   uint64_t     ErrorFrames;
   uint64_t     DumpedFrames;
   uint64_t     LinkFailureCount;
   uint64_t     LossOfSignalCount;
   uint64_t     InvalidTxWordCount;
   uint64_t     InvalidCRCCount;
   uint64_t     VLinkFailureCount;
   uint64_t     MissDiscAdvCount;

   /* FCP Data */
   uint64_t     InputRequests;
   uint64_t     OutputRequests;
   uint64_t     ControlRequests;
};

typedef struct iodm_fcoe_attrs {
   uint32_t	port_id;
   uint64_t	node_name;
   uint64_t	port_name;
   uint32_t	speed;
   enum fcport_type	port_type;
   enum fcport_state	port_state;

   /* FCoE attributes */
   uint8_t	fcoeContlrMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   uint8_t	fcfMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   uint8_t	vnPortMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   uint16_t	vlanId;
   char		vmnicName[VMK_DEVICE_NAME_MAX_LENGTH];
} __attribute__((packed)) iodm_fcoe_attrs_t;

#ifndef SCSI_TRANSPORT_SAS_H
enum sas_linkrate {
   SAS_LINK_RATE_UNKNOWN,
   SAS_PHY_DISABLED,
   SAS_LINK_RATE_FAILED,
   SAS_SATA_SPINUP_HOLD,
   SAS_SATA_PORT_SELECTOR,
   SAS_LINK_RATE_1_5_GBPS,
   SAS_LINK_RATE_3_0_GBPS,
   SAS_LINK_RATE_6_0_GBPS,
   SAS_LINK_VIRTUAL,
};
#endif

typedef struct iodm_sas_attrs {
   uint64_t	sas_address;
   uint64_t	enclosureID;
   uint32_t	bayID;
   uint8_t	phy_identifier;
   enum sas_linkrate	negotiated_linkrate;
   enum sas_linkrate	minimum_linkrate;
   enum sas_linkrate	maximum_linkrate;
} iodm_sas_attrs_t;

typedef struct iodm_sas_stats {
   /* link error statistics */
   uint32_t	invalid_dword_count;
   uint32_t	running_disparity_error_count;
   uint32_t	loss_of_dword_sync_count;
   uint32_t	phy_reset_problem_count;
} iodm_sas_stats_t;

typedef struct iodm_iscsi_attrs {
   uint32_t	targetID;	/* Target ID for this session */
   uint32_t	channelID;	/* Channel ID for this session */
   uint32_t	recovery_tmo;   /* Timeout in seconds */
   uint64_t	caps;		/* Capabilities */
   uint32_t	conndata_size;	/* Connection data size */
   uint32_t	sessiondata_size;   /* Session data size */
   uint32_t	max_lun;	/* Max Lun */
   uint32_t	max_conn;	/* Max connections */
   uint32_t	max_cmd_len;	/* Maximum Command Length */
} iodm_iscsi_sttrs_t;

#if defined(__VMKLNX__)
#define IODM_EVENTBUF_FULL  0x0000000000000001UL

#define IODM_EVENT_LIMIT    400

typedef struct iodm_ScsiEventBuf {
   struct Scsi_Host         *shost;
   char                     vmhba_name[MAX_IODM_NAMELEN];
   atomic_t                 cur;       /* current index to event[] */
   uint64_t                 flags;
   struct iodm_ScsiEvent    event[IODM_EVENT_LIMIT];
   unsigned long            log_ts;    /* log timestamp in jiffies */
   unsigned long            warn_ts;   /* warning timestamp in jiffies */
} iodm_ScsiEventBuf_t;
#endif

#endif /* _IODM_IOCTL_H_ */
