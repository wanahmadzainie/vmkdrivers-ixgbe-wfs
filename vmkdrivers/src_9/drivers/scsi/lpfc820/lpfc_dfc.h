/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2006-2011 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#ifndef _H_LPFC_DFC
#define _H_LPFC_DFC

#define LPFC_INQSN_SZ	64   /* Max size of Inquiry serial number */
#define MBX_EXT_WSIZE	MAILBOX_EXT_WSIZE


struct lpfcdfc_host;

/* Initialize/Un-initialize char device */
int lpfc_cdev_init(void);
void lpfc_cdev_exit(void);
void lpfcdfc_host_del(struct lpfcdfc_host *);
struct lpfcdfc_host *lpfcdfc_host_add(struct pci_dev *, struct Scsi_Host *,
				      struct lpfc_hba *);

/* Define values for SetDiagEnv flag */
enum lpfc_dfc_diag_responses {
	DDI_SHOW       = 0x00,
	DDI_ONDI       = 0x01,
	DDI_OFFDI      = 0x02,
	DDI_WARMDI     = 0x03,
	DDI_DIAGDI     = 0x04,
};


/** Bitmasks for interfaces supported with HBA in the on-line mode */
enum ondi_masks {
	ONDI_MBOX      = 0x1,     /* allows non-destructive mailbox commands */
	ONDI_IOINFO    = 0x2,     /* supports retrieval of I/O info          */
	ONDI_LNKINFO   = 0x4,     /* supports retrieval of link info         */
	ONDI_NODEINFO  = 0x8,     /* supports retrieval of node info         */
	ACEINFO = 0x10,           /* supports retrieval of trace info        */
	ONDI_SETTRACE  = 0x20,    /* supports configuration of trace info    */
	ONDI_SLI1      = 0x40,    /* hardware supports SLI-1 interface       */
	ONDI_SLI2      = 0x80,    /* hardware supports SLI-2 interface       */
	ONDI_BIG_ENDIAN = 0x100,  /* DDI interface is BIG Endian             */
	ONDI_LTL_ENDIAN = 0x200,  /* DDI interface is LITTLE Endian          */
	ONDI_RMEM      = 0x400,   /* allows reading of adapter shared memory */
	ONDI_RFLASH    = 0x800,   /* allows reading of adapter flash         */
	ONDI_RPCI      = 0x1000,  /* allows reading of adapter pci registers */
	ONDI_RCTLREG   = 0x2000,  /* allows reading of adapter cntrol reg    */
	ONDI_CFGPARAM  = 0x4000,  /* supports get/set configuration params   */
	ONDI_CT        = 0x8000,  /* supports passthru CT interface          */
	ONDI_HBAAPI    = 0x10000, /* supports HBA API interface              */
	ONDI_SBUS      = 0x20000, /* supports SBUS adapter interface         */
	ONDI_FAILOVER  = 0x40000, /* supports adapter failover               */
	ONDI_MPULSE    = 0x80000  /* This is a MultiPulse adapter            */
};

/** Bitmasks for interfaces supported with HBA in the off-line mode */
enum offdi_masks {
	OFFDI_MBOX     = 0x1,       /* allows all mailbox commands           */
	OFFDI_RMEM     = 0x2,       /* allows reading of adapter shared mem  */
	OFFDI_WMEM     = 0x4,       /* allows writing of adapter shared mem  */
	OFFDI_RFLASH   = 0x8,       /* allows reading of adapter flash       */
	OFFDI_WFLASH   = 0x10,      /* allows writing of adapter flash       */
	OFFDI_RPCI     = 0x20,      /* allows reading of adapter pci reg     */
	OFFDI_WPCI     = 0x40,      /* allows writing of adapter pci reg     */
	OFFDI_RCTLREG  = 0x80,      /* allows reading of adapter cntrol regs */
	OFFDI_WCTLREG  = 0x100,     /* allows writing of adapter cntrol regs */
	OFFDI_OFFLINE  = 0x80000000 /* if set, adapter is in offline state   */
};

/* Define the CfgParam structure shared between libdfc and the driver. */
#define MAX_CFG_PARAM 64

struct CfgParam {
	char    a_string[32];
	uint32_t a_low;
	uint32_t a_hi;
	uint32_t a_default;
	uint32_t a_current;
	uint16_t a_flag;
	uint16_t a_changestate;
	char    a_help[80];
};

/* Define the CfgEntry structure shared between lpfc_attr and lpfc_ioctl. */
struct CfgEntry {
	struct CfgParam *entry;
	uint32_t (*getcfg)(struct lpfc_hba *phba);
	int (*setcfg)(struct lpfc_hba *phba, uint32_t val);
};

/* Define the CfgEntry structure shared between lpfc_attr and lpfc_ioctl. */
struct VPCfgEntry {
	struct CfgParam *entry;
	uint32_t (*getcfg)(struct lpfc_vport *vport);
	int (*setcfg)(struct lpfc_vport *vport, uint32_t val);
};

/* values for a_flag */
enum CfgParam_flag {
	CFG_EXPORT = 0x0001, /* Export this parameter to the end user */
	CFG_IGNORE = 0x0002, /* Ignore this parameter */
	CFG_THIS_HBA = 0x0004, /* Applicable to this HBA */
	CFG_COMMON = 0x0008, /* Common to all HBAs */
	CFG_SLI4   = 0x1000, /* SLI4 */
	CFG_SLI3   = 0x2000, /* SLI3 */
	CFG_FCOE   = 0x4000, /* FCoE */
	CFG_FC     = 0x8000  /* FC   */
};

enum CfgParam_changestate {
	CFG_REBOOT    = 0x0, /* Changes effective after system reboot */
	CFG_DYNAMIC   = 0x1, /* Changes effective immediately */
	CFG_RESTART   = 0x2, /* Changes effective after driver restart */
	CFG_LINKRESET = 0x3  /* Changes effective after link reset */
};


#define SLI_CT_ELX_LOOPBACK 0x10
enum ELX_LOOPBACK_CMD {
	ELX_LOOPBACK_XRI_SETUP = 100,
	ELX_LOOPBACK_DATA      = 101
};


enum lpfc_dfc_drv_info_sliMode {
	FBIT_DIAG         = 0x0001, /* Diagnostic support */
	FBIT_LUNMAP       = 0x0002, /* LUN Mapping support */
	FBIT_DHCHAP       = 0x0004, /* Authentication/security support */
	FBIT_IKE          = 0x0008, /* Authentication/security support */
	FBIT_NPIV         = 0x0010, /* NPIV support */
	FBIT_RESET_WWN    = 0x0020, /* Driver supports resets to new WWN */
	FBIT_VOLATILE_WWN = 0x0040, /* Driver supports volitile WWN */
	FBIT_E2E_AUTH     = 0x0080, /* End-to-end authentication */
	FBIT_SD           = 0x0100, /* SanDiag support */
	FBIT_FCOE_SUPPORT = 0x0200, /* Driver supports FCoE if set */
	FBIT_PERSIST_LINK_SUPPORT = 0x0400, /* Link Persistence support */
	FBIT_TARGET_MODE_SUPPORT  = 0x0800, /* Target Mode Supported */
	FBIT_SLI_CONFIG_EXTENSION = 0x1000, /* Ext SLI4_Config MB support */
	FEATURE_LIST      = (FBIT_DIAG | FBIT_NPIV |  FBIT_RESET_WWN |
			     FBIT_VOLATILE_WWN | FBIT_SD)
};


/* Define the IO information structure */
struct lpfc_io_info {
	uint32_t a_mbxCmd;	/* mailbox commands issued */
	uint32_t a_mboxCmpl;	/* mailbox commands completed */
	uint32_t a_mboxErr;	/* mailbox commands completed, error status */
	uint32_t a_iocbCmd;	/* iocb command ring issued */
	uint32_t a_iocbRsp;	/* iocb rsp ring received */
	uint32_t a_adapterIntr;	/* adapter interrupt events */
	uint32_t a_fcpCmd;	/* FCP commands issued */
	uint32_t a_fcpCmpl;	/* FCP command completions received */
	uint32_t a_fcpErr;	/* FCP command completions errors */
	uint32_t a_seqXmit;	/* IP xmit sequences sent */
	uint32_t a_seqRcv;	/* IP sequences received */
	uint32_t a_bcastXmit;	/* cnt of successful xmit bcast cmds issued */
	uint32_t a_bcastRcv;	/* cnt of receive bcast cmds received */
	uint32_t a_elsXmit;	/* cnt of successful ELS req cmds issued */
	uint32_t a_elsRcv;	/* cnt of ELS request commands received */
	uint32_t a_RSCNRcv;	/* cnt of RSCN commands received */
	uint32_t a_seqXmitErr;	/* cnt of unsuccessful xmit bcast cmds issued */
	uint32_t a_elsXmitErr;	/* cnt of unsuccessful ELS req cmds issued  */
	uint32_t a_elsBufPost;	/* cnt of ELS buffers posted to adapter */
	uint32_t a_ipBufPost;	/* cnt of IP buffers posted to adapter */
	uint32_t a_cnt1;	/* generic counter */
	uint32_t a_cnt2;	/* generic counter */
	uint32_t a_cnt3;	/* generic counter */
	uint32_t a_cnt4;	/* generic counter */
};

/* Define the nodeinfo structure */
struct lpfc_node_info {
	uint16_t a_flag;
	uint16_t a_state;
	uint32_t a_did;
	uint8_t a_wwpn[8];
	uint8_t a_wwnn[8];
	uint32_t a_targetid;
};

/* Values for a_flag field in struct NODEINFO */
enum node_info_flags {
	NODE_RPI_XRI    = 0x1,    /* creating xri for entry             */
	NODE_REQ_SND    = 0x2,    /* sent ELS request for this entry    */
	NODE_ADDR_AUTH  = 0x4,    /* Authenticating addr for this entry */
	NODE_RM_ENTRY   = 0x8,    /* Remove this entry                  */
	NODE_FARP_SND   = 0x10,   /* sent FARP request for this entry   */
	NODE_FABRIC     = 0x20,   /* this entry represents the Fabric   */
	NODE_FCP_TARGET = 0x40,   /* this entry is an FCP target        */
	NODE_IP_NODE    = 0x80,   /* this entry is an IP node           */
	NODE_DISC_START = 0x100,  /* start discovery on this entry      */
	NODE_SEED_WWPN  = 0x200,  /* Entry scsi id is seeded for WWPN   */
	NODE_SEED_WWNN  = 0x400,  /* Entry scsi id is seeded for WWNN   */
	NODE_SEED_DID   = 0x800,  /* Entry scsi id is seeded for DID    */
	NODE_SEED_MASK  = 0xe00,  /* mask for seeded flags              */
	NODE_AUTOMAP    = 0x1000, /* This entry was automap'ed          */
	NODE_NS_REMOVED = 0x2000  /* This entry removed from NameServer */
};


/* Values for  a_state in struct lpfc_node_info */
enum node_info_states {
	NODE_UNUSED =  0,
	NODE_LIMBO  = 0x1, /* entry needs to hang around for wwpn / sid  */
	NODE_LOGOUT = 0x2, /* NL_PORT is not logged in - entry is cached */
	NODE_PLOGI  = 0x3, /* PLOGI was sent to NL_PORT                  */
	NODE_LOGIN  = 0x4, /* NL_PORT is logged in / login REG_LOGINed   */
	NODE_PRLI   = 0x5, /* PRLI was sent to NL_PORT                   */
	NODE_ALLOC  = 0x6, /* NL_PORT is  ready to initiate adapter I/O  */
	NODE_SEED   = 0x7  /* seed scsi id bind in table                 */
};

struct lpfc_link_info {
	uint32_t a_linkEventTag;
	uint32_t a_linkUp;
	uint32_t a_linkDown;
	uint32_t a_linkMulti;
	uint32_t a_DID;
	uint8_t a_topology;
	uint8_t a_linkState;
	uint8_t a_alpa;
	uint8_t a_alpaCnt;
	uint8_t a_alpaMap[128];
	uint8_t a_wwpName[8];
	uint8_t a_wwnName[8];
};

enum lpfc_link_info_topology {
	LNK_LOOP        = 0x1,
	LNK_PUBLIC_LOOP = 0x2,
	LNK_FABRIC      = 0x3,
	LNK_PT2PT       = 0x4
};

enum lpfc_link_info_linkState {
	LNK_DOWN        = 0x1,
	LNK_UP          = 0x2,
	LNK_FLOGI       = 0x3,
	LNK_DISCOVERY   = 0x4,
	LNK_REDISCOVERY = 0x5,
	LNK_READY       = 0x6,
	LNK_DOWN_PERSIST = 0x7
};

enum lpfc_host_event_code  {
	LPFCH_EVT_LIP            = 0x1,
	LPFCH_EVT_LINKUP         = 0x2,
	LPFCH_EVT_LINKDOWN       = 0x3,
	LPFCH_EVT_LIPRESET       = 0x4,
	LPFCH_EVT_RSCN           = 0x5,
	LPFCH_EVT_ADAPTER_CHANGE = 0x103,
	LPFCH_EVT_PORT_UNKNOWN   = 0x200,
	LPFCH_EVT_PORT_OFFLINE   = 0x201,
	LPFCH_EVT_PORT_ONLINE    = 0x202,
	LPFCH_EVT_PORT_FABRIC    = 0x204,
	LPFCH_EVT_LINK_UNKNOWN   = 0x500,
	LPFCH_EVT_VENDOR_UNIQUE  = 0xffff,
};

#define ELX_LOOPBACK_HEADER_SZ \
	(size_t)(&((struct lpfc_sli_ct_request *)NULL)->un)

struct lpfc_host_event {
	uint32_t seq_num;
	enum lpfc_host_event_code event_code;
	uint32_t data;
};

/*&&&PAE.  Begin 7.4 IOCTL inclusions.  Structure for OUTFCPIO command */
typedef struct dfcptr {
	uint32_t addrhi;
	uint32_t addrlo;
} dfcptr_t;

typedef struct dfcu64 {
	uint32_t hi;
	uint32_t lo;
} dfcu64_t;

typedef struct dfcringmask {
	uint8_t rctl;
	uint8_t type;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcringmask_t;

typedef struct dfcringinit {
	dfcringmask_t prt[LPFC_MAX_RING_MASK];
	uint32_t num_mask;
	uint32_t iotag_ctr;
	uint16_t numCiocb;
	uint16_t numRiocb;
	uint8_t  pad[4]; /* pad structure to 8 byte boundary */
} dfcringinit_t;

typedef struct dfcsliinit {
	dfcringinit_t ringinit[LPFC_MAX_RING];
	uint32_t num_rings;
	uint32_t sli_flag;
} dfcsliinit_t;

typedef struct dfcsliring {
	uint16_t txq_cnt;
	uint16_t txq_max;
	uint16_t txcmplq_cnt;
	uint16_t txcmplq_max;
	uint16_t postbufq_cnt;
	uint16_t postbufq_max;
	uint32_t missbufcnt;
	dfcptr_t cmdringaddr;
	dfcptr_t rspringaddr;
	uint8_t  rspidx;
	uint8_t  cmdidx;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcsliring_t;


typedef struct dfcslistat {
	dfcu64_t iocbEvent[LPFC_MAX_RING];
	dfcu64_t iocbCmd[LPFC_MAX_RING];
	dfcu64_t iocbRsp[LPFC_MAX_RING];
	dfcu64_t iocbCmdFull[LPFC_MAX_RING];
	dfcu64_t iocbCmdEmpty[LPFC_MAX_RING];
	dfcu64_t iocbRspFull[LPFC_MAX_RING];
	dfcu64_t mboxStatErr;
	dfcu64_t mboxCmd;
	dfcu64_t sliIntr;
	uint32_t errAttnEvent;
	uint32_t linkEvent;
	uint32_t mboxEvent;
	uint32_t mboxBusy;
} dfcslistat_t;

struct out_fcp_devp {
	uint16_t target;
	uint16_t lun;
	uint16_t tx_count;
	uint16_t txcmpl_count;
	uint16_t delay_count;
	uint16_t sched_count;
	uint16_t lun_qdepth;
	uint16_t current_qdepth;
	uint32_t qcmdcnt;
	uint32_t iodonecnt;
	uint32_t errorcnt;
	uint8_t  pad[4]; /* pad structure to 8 byte boundary */
};

/* Structure for VPD command */

struct vpd {
	uint32_t version;
#define VPD_VERSION1     1
	uint8_t  ModelDescription[256];    /* VPD field V1 */
	uint8_t  Model[80];                /* VPD field V2 */
	uint8_t  ProgramType[256];         /* VPD field V3 */
	uint8_t  PortNum[20];              /* VPD field V4 */
};


typedef struct dfcsli {
	dfcsliinit_t sliinit;
	dfcsliring_t ring[LPFC_MAX_RING];
	dfcslistat_t slistat;
	dfcptr_t MBhostaddr;
	uint16_t mboxq_cnt;
	uint16_t mboxq_max;
	uint32_t fcp_ring;
} dfcsli_t;

typedef struct dfchba {
	dfcsli_t sli;
	uint32_t hba_state;
	uint32_t cmnds_in_flight;
	uint8_t fc_busflag;
	uint8_t  pad[3]; /* pad structure to 8 byte boundary */
} dfchba_t;


typedef struct dfcnodelist {
	uint32_t nlp_failMask;
	uint16_t nlp_type;
	uint16_t nlp_rpi;
	uint16_t nlp_state;
	uint16_t nlp_xri;
	uint32_t nlp_flag;
	uint32_t nlp_DID;
	uint32_t nlp_oldDID;
	uint8_t  nlp_portname[8];
	uint8_t  nlp_nodename[8];
	uint16_t nlp_sid;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcnodelist_t;

typedef struct dfcscsilun {
	dfcu64_t lun_id;
	uint32_t lunFlag;
	uint32_t failMask;
	uint8_t  InquirySN[LPFC_INQSN_SZ];
	uint8_t  Vendor[8];
	uint8_t  Product[16];
	uint8_t  Rev[4];
	uint8_t  sizeSN;
	uint8_t  pad[3]; /* pad structure to 8 byte boundary */
} dfcscsilun_t;


typedef struct dfcscsitarget {
	dfcptr_t context;
	uint16_t max_lun;
	uint16_t scsi_id;
	uint16_t targetFlags;
	uint16_t addrMode;
	uint16_t rptLunState;
	uint8_t  pad[6]; /* pad structure to 8 byte boundary */
} dfcscsitarget_t;

typedef struct dfcstat {
	uint32_t elsRetryExceeded;
	uint32_t elsXmitRetry;
	uint32_t elsRcvDrop;
	uint32_t elsRcvFrame;
	uint32_t elsRcvRSCN;
	uint32_t elsRcvRNID;
	uint32_t elsRcvFARP;
	uint32_t elsRcvFARPR;
	uint32_t elsRcvFLOGI;
	uint32_t elsRcvPLOGI;
	uint32_t elsRcvADISC;
	uint32_t elsRcvPDISC;
	uint32_t elsRcvFAN;
	uint32_t elsRcvLOGO;
	uint32_t elsRcvPRLO;
	uint32_t elsRcvPRLI;
	uint32_t elsRcvRRQ;
	uint32_t elsRcvLIRR;
	uint32_t elsRcvRPS;
	uint32_t elsRcvRPL;
	uint32_t frameRcvBcast;
	uint32_t frameRcvMulti;
	uint32_t strayXmitCmpl;
	uint32_t frameXmitDelay;
	uint32_t xriCmdCmpl;
	uint32_t xriStatErr;
	uint32_t LinkUp;
	uint32_t LinkDown;
	uint32_t LinkMultiEvent;
	uint32_t NoRcvBuf;
	uint32_t fcpCmd;
	uint32_t fcpCmpl;
	uint32_t fcpRspErr;
	uint32_t fcpRemoteStop;
	uint32_t fcpPortRjt;
	uint32_t fcpPortBusy;
	uint32_t fcpError;
} dfcstats_t;


typedef struct dfc_vpqos {  /* temporary holding place */
       uint32_t  buf;
} DFC_VPQoS ;

#define VP_ATTRIB_VERSION       3 /* Data structure version */
#define VP_ATTRIB_NO_DELETE     1 /* VMware does not support VP Delete */
typedef struct dfc_VpAttrib {
	uint8_t ver;            /* [OUT]; set to VP_ATTRIB_VERSION*/
	uint8_t reserved1[3];
	HBA_WWN wwpn;           /* [OUT] virtual WWPN */
	HBA_WWN wwnn;           /* [OUT] virtual WWNN */
	char name[256];         /* [OUT] NS registered symbolic WWPN */
	uint32_t options;       /* Not Supported */
	uint32_t portFcId;     	/* [OUT] FDISC assigned DID to vport. */
	uint8_t state;          /* [OUT] */
	uint8_t restrictLogin;  /* Not Supported */
	uint8_t flags;		/* [OUT] for DFC_VPGetAttrib. */
	uint8_t reserved2;	/* Not used. */
	uint64_t buf;           /* [OUT] platform dependent specific info */
	HBA_WWN fabricName;     /* [OUT] Fabric WWN */
	uint32_t checklist;     /* Not Supported */
	uint8_t accessKey[32];  /* Not Supported*/
} DFC_VPAttrib;

enum dfc_VpAttrib_state {
	ATTRIB_STATE_UNKNOWN  = 0,
	ATTRIB_STATE_LINKDOWN = 1,
	ATTRIB_STATE_INIT     = 2,
	ATTRIB_STATE_NO_NPIV  = 3,
	ATTRIB_STATE_NO_RESRC = 4,
	ATTRIB_STATE_LOGO     = 5,
	ATTRIB_STATE_REJECT   = 6,
	ATTRIB_STATE_FAILED   = 7,
	ATTRIB_STATE_ACTIVE   = 8,
	ATTRIB_STATE_FAUTH    = 9
};

typedef struct dfc_NodeInfoEntry {
    uint32_t type ;
    HBA_SCSIID scsiId;
    HBA_FCPID fcpId ;
    uint32_t nodeState ;
    uint32_t reserved ;
} DFC_NodeInfoEntry;

/* To maintain backward compatibility with libdfc,
 * do not modify the NodeStateEntry structure. Bump
 * the NODE_STAT_VERSION and make a new structure,
 * DFC_NodeStatEntry_Vx where x is the next number.
 * Let libdfc figure out which version the driver supports
 * and act appropriately.
 */
#define NODE_STAT_VERSION 2
struct DFC_NodeStatEntry_V1 {
	HBA_WWN wwpn;
	HBA_WWN wwnn;
	HBA_UINT32 fc_did;
	HBA_UINT32 TargetNumber;
	HBA_UINT32 TargetQDepth;
	HBA_UINT32 TargetMaxCnt;
	HBA_UINT32 TargetActiveCnt;
	HBA_UINT32 TargetBusyCnt;
	HBA_UINT32 TargetFcpErrCnt;
	HBA_UINT32 TargetAbtsCnt;
	HBA_UINT32 TargetTimeOutCnt;
	HBA_UINT32 TargetNoRsrcCnt;
	HBA_UINT32 TargetInvldRpiCnt;
	HBA_UINT32 TargetLclRjtCnt;
	HBA_UINT32 TargetResetCnt;
	HBA_UINT32 TargetLunResetCnt;
} ;

struct DFC_NodeStatEntry_V2 {
	HBA_WWN wwpn;
	HBA_WWN wwnn;
	HBA_UINT32 fc_did;
	HBA_UINT32 TargetNumber;
	HBA_UINT32 TargetQDepth;
	HBA_UINT32 TargetMaxCnt;
	HBA_UINT32 TargetActiveCnt;
	HBA_UINT32 TargetBusyCnt;
	HBA_UINT32 TargetFcpErrCnt;
	HBA_UINT32 TargetAbtsCnt;
	HBA_UINT32 TargetTimeOutCnt;
	HBA_UINT32 TargetNoRsrcCnt;
	HBA_UINT32 TargetInvldRpiCnt;
	HBA_UINT32 TargetLclRjtCnt;
	HBA_UINT32 TargetResetCnt;
	HBA_UINT32 TargetLunResetCnt;
	HBA_UINT32 TargetFrameDropCnt;
	HBA_UINT32 TargetOverrunCnt;
	HBA_UINT32 TargetUnderrunCnt;
	HBA_UINT64 TargetIOCnt;
	char vmhba[32];
} ;

enum dfc_NodeInfoEntry_type {
	NODE_INFO_TYPE_DID                 = 0x00,
	NODE_INFO_TYPE_WWNN                = 0x01,
	NODE_INFO_TYPE_WWPN                = 0x02,
	NODE_INFO_TYPE_AUTOMAP             = 0x04,
	NODE_INFO_TYPE_UNMASK_LUNS         = 0x08,
	NODE_INFO_TYPE_DISABLE_LUN_AUTOMAP = 0x10,
	NODE_INFO_TYPE_ALPA                = 0x20
};

enum dfc_NodeInfoEntry_nodeState {
	NODE_INFO_STATE_EXIST      = 0x01,
	NODE_INFO_STATE_READY      = 0x02,
	NODE_INFO_STATE_LINKDOWN   = 0x04,
	NODE_INFO_STATE_UNMAPPED   = 0x08,
	NODE_INFO_STATE_PERSISTENT = 0x10
};

typedef struct dfc_GetNodeInfo {
    uint32_t numberOfEntries ;  /* number of nodes */
    DFC_NodeInfoEntry nodeInfo[1];  /* start of the DFC_NodeInfo array */
} DFC_GetNodeInfo;

struct DFC_GetNodeStat {
    uint32_t version;           /* Version of DFC_NodeStatEntry */
    uint32_t numberOfEntries ;  /* number of nodes */
    struct DFC_NodeStatEntry_V2 nodeStat[1];  /* start of DFC_NodeInfo array */
};


typedef struct dfc_VpEntry {
	HBA_WWN wwpn; /* vport wwpn */
	HBA_WWN wwnn; /* vport wwnn */
	uint32_t PortFcId; /* DID from successful FDISC. */
} DFC_VPEntry;

typedef struct DFC_VPENTRYLIST {
	uint32_t numberOfEntries;
	DFC_VPEntry vpentry[1];
} DFC_VPEntryList;

typedef struct DFC_VPRESOURCE {
	uint32_t vlinks_max;
	uint32_t vlinks_inuse;
	uint32_t rpi_max;
	uint32_t rpi_inuse;
} DFC_VPResource;


#define CHECKLIST_BIT_NPIV	0x0001	/* Set if driver NPIV enabled */
#define	CHECKLIST_BIT_SLI3	0x0002	/* Set if SLI-3 enabled */
#define CHECKLIST_BIT_HBA	0x0004	/* Set if HBA support NPIV */
#define CHECKLIST_BIT_RSRC	0x0008	/* Set if resources available*/
#define CHECKLIST_BIT_LINK	0x0010	/* Set if link up */
#define CHECKLIST_BIT_FBRC	0x0020	/* Set if P2P fabric connection */
#define CHECKLIST_BIT_FSUP	0x0040	/* Set if Fabric support NPIV */
#define CHECKLIST_BIT_NORSRC	0x0080	/* Set if FDISC fails w/o LS_RJT */

/* SD DIAG EVENT DEBUG */
enum sd_inject_set {
	SD_INJECT_PLOGI =       1,
	SD_INJECT_PRLO  =       2,
	SD_INJECT_ADISC =       3,
	SD_INJECT_LSRJCT =      4,
	SD_INJECT_LOGO =	5,
	SD_INJECT_FBUSY =	6,
	SD_INJECT_PBUSY =	7,
	SD_INJECT_FCPERR =	8,
	SD_INJECT_QFULL =	9,
	SD_INJECT_DEVBSY =	10,
	SD_INJECT_CHKCOND =	11,
	SD_INJECT_LUNRST =	12,
	SD_INJECT_TGTRST =	13,
	SD_INJECT_BUSRST =	14,
	SD_INJECT_QDEPTH =	15,
	SD_INJECT_PERR =	16,
	SD_INJECT_ARRIVE =	17
};

/*
 * Arguments for LPFC_MBOX_SLI_CFG_EXT
 */
enum mailbox_type {
	MAILBOX_WRITE = 1,
	MAILBOX_READ,
};

enum mailbox_format {
	MAILBOX_NONEMBEDDED = 1,
	MAILBOX_EMBEDDED_HBD,
};

#endif				/* _H_LPFC_DFC */
