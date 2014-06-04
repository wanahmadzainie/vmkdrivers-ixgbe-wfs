/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2003-2010 Emulex.  All rights reserved.           *
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

/*
 * $Id: lpfc_ioctl.h 3037 2007-05-22 14:02:22Z bsebastian $
 */

#ifndef _H_LPFC_IOCTL
#define _H_LPFC_IOCTL

#define DIAG_VPORT_NAME "Emulex-Diagnostic\0"

/* Used for libdfc/lpfc driver rev lock*/
#define DFC_MAJOR_REV   4	
#define DFC_MINOR_REV	0

/* Used to control libdfc/lpfc driver compatibility */
#define DFC_MIX_N_MATCH 1

#define LPFC_WWPN_TYPE		0
#define LPFC_PORTID_TYPE	1
#define LPFC_WWNN_TYPE		2



/* Define values for SetDiagEnv flag */
enum lpfc_ioctl_diag_commands {
	DDI_BRD_SHOW   = 0x10,
	DDI_BRD_ONDI   = 0x11,
	DDI_BRD_OFFDI  = 0x12,
	DDI_BRD_WARMDI = 0x13,
	DDI_BRD_DIAGDI = 0x14,
};



struct iocb_timeout_args {
	struct lpfc_hba   *phba;
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_iocbq *rspiocbq;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_dmabuf *db0;
	struct lpfc_dmabuf *db1;
	struct lpfc_dmabuf *db2;
	struct lpfc_dmabuf *db3;
	struct lpfc_dmabufext *dbext0;
	struct lpfc_dmabufext *dbext1;
	struct lpfc_dmabufext *dbext2;
	struct lpfc_dmabufext *dbext3;
};
typedef struct iocb_timeout_args IOCB_TIMEOUT_T;

/* Max depth of unsolicted event processing queue */
#define LPFC_IOCTL_MAX_EVENTS 128
#define LPFC_IOCTL_PAGESIZE   8192


/* Definition for LPFC_HBA_GET_EVENT events */
struct event_type {
	uint32_t mask;
	uint32_t cat;
	uint32_t sub;
};


/* Definitions for Temperature event processing */
struct temp_event {
	unsigned int event_type;	/* FC_REG_TEMPERATURE_EVENT */
	unsigned int event_code;	/* Criticality */
	unsigned int data;		/* Temperature */
}; 
typedef struct temp_event tempEvent_t;

#define LPFC_CRIT_TEMP          0x1
#define LPFC_THRESHOLD_TEMP     0x2
#define LPFC_NORMAL_TEMP        0x3

/* IOCTL LPFC_CMD Definitions */

/* Debug API - Range 0 - 99, x00 - x63 */
#define LPFC_LIP			0x01	/* Issue a LIP */ 
#define LPFC_SD_TEST			0x02	/* Inject SD Event */
#define LPFC_SET_SD_TEST		0x03	/* Set SD Testing */
#define LPFC_IOCTL_NODE_STAT		0x04	/* Get Node Stats */
#define LPFC_RESET			0x05	/* Reset the adapter */
#define LPFC_INDUCE_DUMP		0x06	/* Induce a fw dump */
#define LPFC_DEVP			0x0a	/* Get Device information */

/* Primary IOCTL CMD Definitions. Range 100 - 299, 0x64 - 0x12b */
#define LPFC_WRITE_PCI				0x64
#define LPFC_READ_PCI				0x65
#define LPFC_READ_MEM				0x66
#define LPFC_MBOX				0x67
#define LPFC_GET_DFC_REV			0x68
#define LPFC_WRITE_CTLREG			0x69
#define LPFC_READ_CTLREG			0x6a
#define LPFC_INITBRDS				0x6b
#define LPFC_SETDIAG				0x6c
#define LPFC_GETCFG				0x6d
#define LPFC_SETCFG				0x6e
#define LPFC_WRITE_MEM				0x6f
#define LPFC_GET_VPD				0x70
#define LPFC_GET_LPFCDFC_INFO			0x71
#define LPFC_GET_DUMPREGION			0x72
#define LPFC_LOOPBACK_TEST      		0x73
#define LPFC_LOOPBACK_MODE      		0x74
#define LPFC_TEMP_SENSOR_SUPPORT		0x75
#define LPFC_VPORT_GET_ATTRIB			0x76
#define LPFC_VPORT_GET_LIST			0x77
#define LPFC_VPORT_GET_RESRC			0x78
#define LPFC_VPORT_GET_NODE_INFO		0x79
#define LPFC_NPIV_READY				0x7a
#define LPFC_LINKINFO				0x7b
#define LPFC_IOINFO  				0x7c
#define LPFC_NODEINFO  				0x7d
#define LPFC_MENLO				0x7e
#define LPFC_CT					0x7f
#define LPFC_SEND_ELS				0x80
#define LPFC_HBA_SEND_SCSI			0x81
#define LPFC_HBA_SEND_FCP			0x82
#define LPFC_HBA_SET_EVENT			0x83
#define LPFC_HBA_GET_EVENT			0x84
#define LPFC_HBA_SEND_MGMT_CMD			0x85
#define LPFC_HBA_SEND_MGMT_RSP			0x86
#define LPFC_LIST_BIND                  	0x87
#define LPFC_VPORT_GETCFG               	0x88
#define LPFC_VPORT_SETCFG               	0x89
#define LPFC_HBA_UNSET_EVENT			0x8a
#define LPFC_VPORT_CREATE			0x8b
#define LPFC_VPORT_DELETE			0x8c
#define LPFC_MBOX_SLI_CFG_EXT			0x8d
#define LPFC_MBOX_SLI_CFG_EXT_SUP		0x8f

/* Start OC5.0 definitions. */
#define LPFC_HBA_ISSUE_MBX_RTRYV2          	0x90
#define LPFC_HBA_GET_PERSISTENT_LINK_DOWN  	0x91
#define LPFC_HBA_SET_PERSISTENT_LINK_DOWN  	0x92
#define LPFC_HBA_GET_FCF_LIST			0x93
#define LPFC_HBA_GET_PARAMS        		0x94
#define LPFC_HBA_SET_PARAMS                	0x95
#define LPFC_HBA_GET_FCF_CONN_LIST         	0x96
#define LPFC_HBA_SET_FCF_CONN_LIST         	0x97
#define LPFC_HBA_ISSUE_DMP_MBX             	0x98
#define LPFC_HBA_ISSUE_UPD_MBX			0x99
#define LPFC_GET_LOGICAL_LINK_SPEED		0x9a
#define LPFC_LINK_DIAG_TEST			0x9b    /* SLI4 link diag tst */
#define LPFC_DIAG_MODE_END			0x9c    /* SLI4 diag mode end */
#define LPFC_PRIMARY_IOCTL_RANGE_END		0x12b	/* End range. */

/*  HBAAPI IOCTL CMD Definitions Range 300 - 499, 0x12c - x1f3 */
#define LPFC_HBA_ADAPTERATTRIBUTES	0x12c	/* Get attributes of HBA */
#define LPFC_HBA_PORTATTRIBUTES		0x12d	/* Get attributes of HBA Port */
#define LPFC_HBA_PORTSTATISTICS		0x12e	/* Get statistics of HBA Port */
#define LPFC_HBA_DISCPORTATTRIBUTES	0x12f	/* Get attibutes of the discovered adapter Ports */
#define LPFC_HBA_WWPNPORTATTRIBUTES	0x130	/* Get attributes of the Port specified by WWPN */
#define LPFC_HBA_INDEXPORTATTRIBUTES	0x131	/* Get attributes of the Port specified by index */
#define LPFC_HBA_FCPTARGETMAPPING	0x132	/* Get info for all FCP tgt's */
#define LPFC_HBA_FCPBINDING		0x133	/* Binding info for FCP tgts */
#define LPFC_HBA_SETMGMTINFO		0x134	/* Sets driver values with default HBA_MGMTINFO vals */
#define LPFC_HBA_GETMGMTINFO		0x135	/* Get driver values for HBA_MGMTINFO vals */
#define LPFC_HBA_RNID			0x136	/* Send an RNID request */
#define LPFC_HBA_REFRESHINFO		0x137	/* Do a refresh of the stats */
#define LPFC_HBA_GETEVENT		0x138	/* Get HBAAPI event(s) */
/*  LPFC_LAST_IOCTL_USED 	        0x138       Last LPFC Ioctl used  */

#define INTERNAL_LOOP_BACK              0x1
#define EXTERNAL_LOOP_BACK              0x2
#define LOOPBACK_MAX_BUFSIZE            0x2000	/* 8192 (dec) bytes */
/* the DfcRevInfo structure */
struct DfcRevInfo {
	uint32_t a_Major;
	uint32_t a_Minor;
	uint32_t a_MixNMatch;
};

#define DFC_DRVID_STR_SZ 16
#define DFC_FW_STR_SZ 32

struct dfc_info {
	uint32_t a_pci;
	uint32_t a_busid;
	uint32_t a_devid;
	uint32_t a_ddi;
	uint32_t a_onmask;
	uint32_t a_offmask;
	uint8_t  a_drvrid[DFC_DRVID_STR_SZ];
	uint8_t  a_fwname[DFC_FW_STR_SZ];
	uint8_t  a_wwpn[8];
	uint8_t  a_pciFunc;
};

/* Define the idTypes for the nport_id. */
#define NPORT_ID_TYPE_WWPN 0
#define NPORT_ID_TYPE_DID  1
#define NPORT_ID_TYPE_WWNN 2

struct nport_id {
   uint32_t    idType;         /* 0 - wwpn, 1 - d_id, 2 - wwnn */
   uint32_t    d_id;
   uint8_t     wwpn[8];
};


/* Define the idTypes for the nport_id. */
#define NPORT_ID_TYPE_WWPN 0
#define NPORT_ID_TYPE_DID  1
#define NPORT_ID_TYPE_WWNN 2

#define LPFC_EVENT_LIP_OCCURRED		1
#define LPFC_EVENT_LINK_UP		2
#define LPFC_EVENT_LINK_DOWN		3
#define LPFC_EVENT_LIP_RESET_OCCURRED	4
#define LPFC_EVENT_RSCN			5
#define LPFC_EVENT_PROPRIETARY		0xFFFF

struct lpfc_hba_event_info {
	uint32_t event_code;
	uint32_t port_id;
	union {
		uint32_t rscn_event_info;
		uint32_t pty_event_info;
	} event;
};

/* Define the character device name. */
#define LPFC_CHAR_DEV_NAME "lpfcdfc"

struct sli4_link_diag {
	uint32_t command;
	uint32_t timeout;
	uint32_t test_id;
	uint32_t loops;
	uint32_t test_version;
	uint32_t error_action;
};

struct diag_status {
	uint32_t mbox_status;
	uint32_t shdr_status;
	uint32_t shdr_add_status;
};

/* Used for ioctl command */
#define LPFC_DFC_CMD_IOCTL_MAGIC 0xFC
#define LPFC_DFC_CMD_IOCTL _IOWR(LPFC_DFC_CMD_IOCTL_MAGIC, 0x1,\
		struct lpfcCmdInput)

/*
 * Diagnostic (DFC) Command & Input structures: (LPFC)
 */
struct lpfcCmdInput {
	short    lpfc_brd;
	short    lpfc_ring;
	short    lpfc_iocb;
	short    lpfc_flag;
	void    *lpfc_arg1;
	void    *lpfc_arg2;
	void    *lpfc_arg3;
	char    *lpfc_dataout;
	uint32_t lpfc_cmd;
	uint32_t lpfc_outsz;
	uint32_t lpfc_arg4;
	uint32_t lpfc_arg5;
	uint32_t lpfc_cntl;
	uint8_t  pad[4];
	struct sli4_link_diag diaglnk;
	struct diag_status diagsta;
};

typedef struct lpfcCmdInput LPFCCMDINPUT_t;

#if defined(CONFIG_COMPAT)
/* 32 bit version */
struct lpfcCmdInput32 {
	short    lpfc_brd;
	short    lpfc_ring;
	short    lpfc_iocb;
	short    lpfc_flag;
	u32	lpfc_arg1;
	u32	lpfc_arg2;
	u32     lpfc_arg3;
	u32     lpfc_dataout;
	uint32_t lpfc_cmd;
	uint32_t lpfc_outsz;
	uint32_t lpfc_arg4;
	uint32_t lpfc_arg5;
	uint32_t lpfc_cntl;
	uint8_t  pad[4];
	struct sli4_link_diag diaglnk;
	struct diag_status diagsta;
};

#define LPFC_DFC_CMD_IOCTL32 _IOWR(LPFC_DFC_CMD_IOCTL_MAGIC, 0x1, \
		struct lpfcCmdInput32)
#endif /*CONFIG_COMPAT */


/*
 * Command input control definition.  Inform the driver the calling
 * application is running in i386, 32bit mode.
 */
#define LPFC_CNTL_X86_APP  0x01



/*
 * Define the driver information structure. This structure is shared
 * with libdfc and has 32/64 bit alignment requirements.
 */
struct lpfc_dfc_drv_info {
	uint8_t  version[64];  /* Driver Version string */
	uint8_t  name[32];     /* Driver Name */
	uint32_t sliMode;      /* current operation SLI mode used */
	uint32_t align_1;
	uint64_t featureList;
	uint32_t hbaType;
	uint32_t align_2;
};



/* Structure used for transfering mailbox extension data */
struct ioctl_mailbox_ext_data {
	uint32_t in_ext_byte_len;
	uint32_t out_ext_byte_len;
	uint8_t  mbox_offset_word;
	uint8_t  mbox_extension_data[MBX_EXT_WSIZE * sizeof(uint32_t)];
};


typedef union IOARG{
	struct {
		HBA_WWN  vport_wwpn; /* Input Arg */
		HBA_WWN  targ_wwpn;  /* Input Arg */  
	}Iarg;

	struct {
		uint32_t rspcnt; /* Output Arg */
		uint32_t snscnt; /* Output Arg */
	}Oarg;
} IOargUn;

#endif				/* _H_LPFC_IOCTL */
