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

#include <linux/types.h>
#include <linux/module.h>

#include "ixgbe.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */

#define IXGBE_MAX_NIC 32

#define OPTION_UNSET    -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1

#define STRINGIFY(foo) #foo         /* magic for getting defines into strings */
#define XSTRINGIFY(bar) STRINGIFY(bar)

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */

#define IXGBE_PARAM_INIT { [0 ... IXGBE_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when ixgbe_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define IXGBE_PARAM(X, desc) \
	static const int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	MODULE_PARM(X, "1-" __MODULE_STRING(IXGBE_MAX_NIC) "i"); \
	MODULE_PARM_DESC(X, desc);
#else
#define IXGBE_PARAM(X, desc) \
	static int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	static unsigned int num_##X; \
	module_param_array_named(X, X, int, &num_##X, 0); \
	MODULE_PARM_DESC(X, desc);
#endif

/* IntMode (Interrupt Mode)
 *
 * Valid Range: 0-2
 *  - 0 - Legacy Interrupt
 *  - 1 - MSI Interrupt
 *  - 2 - MSI-X Interrupt(s)
 *
 * Default Value: 2
 */
IXGBE_PARAM(InterruptType, "Change Interrupt Mode (0=Legacy, 1=MSI, 2=MSI-X), default IntMode (deprecated)");
IXGBE_PARAM(IntMode, "Change Interrupt Mode (0=Legacy, 1=MSI, 2=MSI-X), default 2");
#define IXGBE_INT_LEGACY		      0
#define IXGBE_INT_MSI			      1
#define IXGBE_INT_MSIX			      2
#define IXGBE_DEFAULT_INT	 IXGBE_INT_MSIX

IXGBE_PARAM(Node, "set the starting node to allocate memory on, default -1");

/* MQ - Multiple Queue enable/disable
 *
 * Valid Range: 0, 1
 *  - 0 - disables MQ
 *  - 1 - enables MQ
 *
 * Default Value: 1
 */

IXGBE_PARAM(MQ, "Disable or enable Multiple Queues, default 1");

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
/* DCA - Direct Cache Access (DCA) Control
 *
 * This option allows the device to hint to DCA enabled processors
 * which CPU should have its cache warmed with the data being
 * transferred over PCIe.  This can increase performance by reducing
 * cache misses.  ixgbe hardware supports DCA for:
 * tx descriptor writeback
 * rx descriptor writeback
 * rx data
 * rx data header only (in packet split mode)
 *
 * enabling option 2 can cause cache thrash in some tests, particularly
 * if the CPU is completely utilized
 *
 * Valid Range: 0 - 2
 *  - 0 - disables DCA
 *  - 1 - enables DCA
 *  - 2 - enables DCA with rx data included
 *
 * Default Value: 2
 */

#define IXGBE_MAX_DCA 2

IXGBE_PARAM(DCA, "Disable or enable Direct Cache Access, 0=disabled, 1=descriptor only, 2=descriptor and data");

#endif
/* RSS - Receive-Side Scaling (RSS) Descriptor Queues
 *
 * Valid Range: 0-16
 *  - 0 - disables RSS
 *  - 1 - enables RSS and sets the Desc. Q's to min(16, num_online_cpus()).
 *  - 2-16 - enables RSS and sets the Desc. Q's to the specified value.
 *
 * Default Value: 1
 */

IXGBE_PARAM(RSS, "Number of Receive-Side Scaling Descriptor Queues, default 1=number of cpus");

#ifdef IXGBE_VMDQ
/* VMDQ - Virtual Machine Device Queues (VMDQ)
 *
 * Valid Range: 1-16
 *  - 1 Disables VMDQ by allocating only a single queue.
 *  - 2-16 - enables VMDQ and sets the Desc. Q's to the specified value.
 *
 * Default Value: 1
 */

#define IXGBE_DEFAULT_NUM_VMDQ 8

IXGBE_PARAM(VMDQ, "Number of Virtual Machine Device Queues: 0/1 = disable, 2-16 enable (default=" XSTRINGIFY(IXGBE_DEFAULT_NUM_VMDQ) ")");
#endif

/* max_vfs - SR I/O Virtualization
 *
 * Valid Range: 0-63
 *  - 0 Disables SR-IOV
 *  - 1 Enables SR-IOV to default number of VFs enabled
 *  - 2-63 - enables SR-IOV and sets the number of VFs enabled
 *
 * Default Value: 0
 */

#define MAX_SRIOV_VFS 63

IXGBE_PARAM(max_vfs, "Number of Virtual Functions: 0 = disable (default), 1-" XSTRINGIFY(MAX_SRIOV_VFS) " = enable this many VFs");

/* L2LBen - L2 Loopback enable
 *
 * Valid Range: 0-1
 *  - 0 Disables L2 loopback
 *  - 1 Enables L2 loopback
 *
 * Default Value: 1
 */
/*
 *Note:
 *=====
 * This is a temporary solution to enable SR-IOV features testing with
 * external switches. As soon as an integrated VEB management interface
 * becomes available this feature will be removed.
*/
IXGBE_PARAM(L2LBen, "L2 Loopback Enable: 0 = disable, 1 = enable (default)");

/* Interrupt Throttle Rate (interrupts/sec)
 *
 * Valid Range: 956-488281 (0=off, 1=dynamic)
 *
 * Default Value: 1
 */
#ifdef __VMKLNX__
#define DEFAULT_ITR                16000
IXGBE_PARAM(InterruptThrottleRate, "Maximum interrupts per second, per vector, (956-488281), default 16000");
#else
#define DEFAULT_ITR		1
IXGBE_PARAM(InterruptThrottleRate, "Maximum interrupts per second, per vector, (0,1,956-488281), default 1");
#endif
#define MAX_ITR       IXGBE_MAX_INT_RATE
#define MIN_ITR       IXGBE_MIN_INT_RATE

#ifndef IXGBE_NO_LLI
/* LLIPort (Low Latency Interrupt TCP Port)
 *
 * Valid Range: 0 - 65535
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIPort, "Low Latency Interrupt TCP Port (0-65535)");

#define DEFAULT_LLIPORT                0
#define MAX_LLIPORT               0xFFFF
#define MIN_LLIPORT                    0

/* LLIPush (Low Latency Interrupt on TCP Push flag)
 *
 * Valid Range: 0,1
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIPush, "Low Latency Interrupt on TCP Push flag (0,1)");

#define DEFAULT_LLIPUSH                0
#define MAX_LLIPUSH                    1
#define MIN_LLIPUSH                    0

/* LLISize (Low Latency Interrupt on Packet Size)
 *
 * Valid Range: 0 - 1500
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLISize, "Low Latency Interrupt on Packet Size (0-1500)");

#define DEFAULT_LLISIZE                0
#define MAX_LLISIZE                 1500
#define MIN_LLISIZE                    0

/* LLIEType (Low Latency Interrupt Ethernet Type)
 *
 * Valid Range: 0 - 0x8fff
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIEType, "Low Latency Interrupt Ethernet Protocol Type");

#define DEFAULT_LLIETYPE               0
#define MAX_LLIETYPE              0x8fff
#define MIN_LLIETYPE                   0

/* LLIVLANP (Low Latency Interrupt on VLAN priority threshold)
 *
 * Valid Range: 0 - 7
 *
 * Default Value: 0 (disabled)
 */
IXGBE_PARAM(LLIVLANP, "Low Latency Interrupt on VLAN priority threshold");

#define DEFAULT_LLIVLANP               0
#define MAX_LLIVLANP                   7
#define MIN_LLIVLANP                   0

#endif /* IXGBE_NO_LLI */
/* Rx buffer mode
 *
 * Valid Range: 0-3 0 = 1buf_mode_always,
 * 		    1 = ps_mode_always
 * 		    2 = optimal
 * 		    3 = bounce_buffer_mode -- NAPI only option
 *
 * Default Value: 2
 */
#ifdef __VMKLNX__
IXGBE_PARAM(RxBufferMode, "0=1 descriptor per packet,\n"
                          "\t\t\t1=use packet split, multiple descriptors per jumbo frame\n"
                          "\t\t\t2 (default)=use 1buf mode for 1500 mtu, packet split for jumbo\n");
#else
IXGBE_PARAM(RxBufferMode, "0=1 descriptor per packet,\n"
                          "\t\t\t1=use packet split, multiple descriptors per jumbo frame\n"
                          "\t\t\t2 (default)=use 1buf mode for 1500 mtu, packet split for jumbo\n"
                          "\t\t\t3=use half page to receive buffer, and other half to receive next");
#endif
#define IXGBE_RXBUFMODE_1BUF_ALWAYS			0
#define IXGBE_RXBUFMODE_PS_ALWAYS			1
#define IXGBE_RXBUFMODE_OPTIMAL				2
#define IXGBE_RXBUFMODE_BOUNCE_BUF			3
#define IXGBE_DEFAULT_RXBUFMODE	  IXGBE_RXBUFMODE_OPTIMAL

#ifdef HAVE_TX_MQ
/* Flow Director filtering mode
 *
 * Valid Range: 0-2  0 = off, 1 = Hashing (ATR), and 2 = perfect filters
 *
 * Default Value: 1 (ATR)
 */
IXGBE_PARAM(FdirMode, "Flow Director filtering modes:\n"
	              "\t\t\t0 = Filtering off\n"
	              "\t\t\t1 = Signature Hashing filters (SW ATR)\n"
	              "\t\t\t2 = Perfect Filters");

#define IXGBE_FDIR_FILTER_OFF				0
#define IXGBE_FDIR_FILTER_HASH				1
#define IXGBE_FDIR_FILTER_PERFECT			2
#ifdef __VMKLNX__
#define IXGBE_DEFAULT_FDIR_FILTER  IXGBE_FDIR_FILTER_OFF
#else
#define IXGBE_DEFAULT_FDIR_FILTER  IXGBE_FDIR_FILTER_HASH
#endif

/* Flow Director packet buffer allocation level
 *
 * Valid Range: 0-3  0 = No memory allocation,	1 = 8k hash/2k perfect,
 * 		     2 = 16k hash/4k perfect,	3 = 32k hash/8k perfect
 *
 * Default Value: 0
 */
IXGBE_PARAM(FdirPballoc, "Flow Director packet buffer allocation level:\n"
	                 "\t\t\t1 = 8k hash filters or 2k perfect filters\n"
	                 "\t\t\t2 = 16k hash filters or 4k perfect filters\n"
	                 "\t\t\t3 = 32k hash filters or 8k perfect filters");

#define IXGBE_DEFAULT_FDIR_PBALLOC IXGBE_FDIR_PBALLOC_64K

/* Software ATR packet sample rate
 *
 * Valid Range: 0-255  0 = off, 1-255 = rate of Tx packet inspection
 *
 * Default Value: 20
 */
IXGBE_PARAM(AtrSampleRate, "Software ATR Tx packet sample rate");

#define IXGBE_MAX_ATR_SAMPLE_RATE       255
#define IXGBE_MIN_ATR_SAMPLE_RATE	  1
#define IXGBE_ATR_SAMPLE_RATE_OFF	  0
#ifdef __VMKLNX__
#define IXGBE_DEFAULT_ATR_SAMPLE_RATE	 IXGBE_ATR_SAMPLE_RATE_OFF
#else
#define IXGBE_DEFAULT_ATR_SAMPLE_RATE	 20
#endif
#endif /* HAVE_TX_MQ */
/* enable/disable CNA
 *
 * Valid Range: 0-8
 * - 0   Disables CNA functionality
 * - 1-8 Enables  CNA functionality with number of Qs allocated to FCoE
 *
 * Default Value: 1 - CNA enabled with 1 Q
 */
IXGBE_PARAM(CNA, "Number of FCoE Queues: 0 = disable, 1-8 = enable this many Qs");

/* Enable/disable Large Receive Offload
 *
 * Valid Values: 0(off), 1(on)
 *
 * Default Value: 0
 */
IXGBE_PARAM(LRO, "Large Receive Offload (0,1), default 1 = on");

struct ixgbe_option {
	enum { enable_option, range_option, list_option } type;
	const char *name;
	const char *err;
	int def;
	union {
		struct { /* range_option info */
			int min;
			int max;
		} r;
		struct { /* list_option info */
			int nr;
			const struct ixgbe_opt_list {
				int i;
				char *str;
			} *p;
		} l;
	} arg;
};

static int __devinit ixgbe_validate_option(unsigned int *value,
                                           struct ixgbe_option *opt)
{
	if (*value == OPTION_UNSET) {
		*value = opt->def;
		return 0;
	}

	switch (opt->type) {
	case enable_option:
		switch (*value) {
		case OPTION_ENABLED:
			printk(KERN_INFO "ixgbe: %s Enabled\n", opt->name);
			return 0;
		case OPTION_DISABLED:
			printk(KERN_INFO "ixgbe: %s Disabled\n", opt->name);
			return 0;
		}
		break;
	case range_option:
		if (*value >= opt->arg.r.min && *value <= opt->arg.r.max) {
			printk(KERN_INFO "ixgbe: %s set to %d\n", opt->name, *value);
			return 0;
		}
		break;
	case list_option: {
		int i;
		const struct ixgbe_opt_list *ent;

		for (i = 0; i < opt->arg.l.nr; i++) {
			ent = &opt->arg.l.p[i];
			if (*value == ent->i) {
				if (ent->str[0] != '\0')
					printk(KERN_INFO "%s\n", ent->str);
				return 0;
			}
		}
	}
		break;
	default:
		BUG();
	}

	printk(KERN_INFO "ixgbe: Invalid %s specified (%d),  %s\n",
	       opt->name, *value, opt->err);
	*value = opt->def;
	return -1;
}

#define LIST_LEN(l) (sizeof(l) / sizeof(l[0]))

/**
 * ixgbe_check_options - Range Checking for Command Line Parameters
 * @adapter: board private structure
 *
 * This routine checks all command line parameters for valid user
 * input.  If an invalid value is given, or if no user specified
 * value exists, a default value is used.  The final value is stored
 * in a variable in the adapter structure.
 **/
void __devinit ixgbe_check_options(struct ixgbe_adapter *adapter)
{
	int bd = adapter->bd_number;
	u32 *aflags = &adapter->flags;
	struct ixgbe_ring_feature *feature = adapter->ring_feature;

	if (bd >= IXGBE_MAX_NIC) {
		printk(KERN_NOTICE
		       "Warning: no configuration for board #%d\n", bd);
		printk(KERN_NOTICE "Using defaults for all values\n");
#ifndef module_param_array
		bd = IXGBE_MAX_NIC;
#endif
	}

	{ /* Interrupt Mode */
		unsigned int int_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Interrupt Mode",
			.err =
			  "using default of "__MODULE_STRING(IXGBE_DEFAULT_INT),
			.def = IXGBE_DEFAULT_INT,
			.arg = { .r = { .min = IXGBE_INT_LEGACY,
					.max = IXGBE_INT_MSIX}}
		};

#ifdef module_param_array
		if (num_IntMode > bd || num_InterruptType > bd) {
#endif
			int_mode = IntMode[bd];
			if (int_mode == OPTION_UNSET)
				int_mode = InterruptType[bd];
			ixgbe_validate_option(&int_mode, &opt);
			switch (int_mode) {
			case IXGBE_INT_MSIX:
				if (!(*aflags & IXGBE_FLAG_MSIX_CAPABLE))
					printk(KERN_INFO
					       "Ignoring MSI-X setting; "
					       "support unavailable\n");
				break;
			case IXGBE_INT_MSI:
				if (!(*aflags & IXGBE_FLAG_MSI_CAPABLE)) {
					printk(KERN_INFO
					       "Ignoring MSI setting; "
					       "support unavailable\n");
				} else {
					*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
					*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
				}
				break;
			case IXGBE_INT_LEGACY:
			default:
				*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
				*aflags &= ~IXGBE_FLAG_MSI_CAPABLE;
				*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
				break;
			}
#ifdef module_param_array
		} else {
			/* default settings */
			if (opt.def == IXGBE_INT_MSIX &&
			    *aflags & IXGBE_FLAG_MSIX_CAPABLE) {
				*aflags |= IXGBE_FLAG_MSIX_CAPABLE;
				*aflags |= IXGBE_FLAG_MSI_CAPABLE;
			} else if (opt.def == IXGBE_INT_MSI &&
			    *aflags & IXGBE_FLAG_MSI_CAPABLE) {
				*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
				*aflags |= IXGBE_FLAG_MSI_CAPABLE;
				*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
			} else {
				*aflags &= ~IXGBE_FLAG_MSIX_CAPABLE;
				*aflags &= ~IXGBE_FLAG_MSI_CAPABLE;
				*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
			}
		}
#endif
	}
	{ /* Multiple Queue Support */
		static struct ixgbe_option opt = {
			.type = enable_option,
			.name = "Multiple Queue Support",
			.err  = "defaulting to Enabled",
			.def  = OPTION_ENABLED
		};

#ifdef module_param_array
		if (num_MQ > bd) {
#endif
			unsigned int mq = MQ[bd];
			ixgbe_validate_option(&mq, &opt);
			if (mq)
				*aflags |= IXGBE_FLAG_MQ_CAPABLE;
			else
				*aflags &= ~IXGBE_FLAG_MQ_CAPABLE;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_ENABLED)
				*aflags |= IXGBE_FLAG_MQ_CAPABLE;
			else
				*aflags &= ~IXGBE_FLAG_MQ_CAPABLE;
		}
#endif
		/* Check Interoperability */
		if ((*aflags & IXGBE_FLAG_MQ_CAPABLE) &&
		    !(*aflags & IXGBE_FLAG_MSIX_CAPABLE)) {
			DPRINTK(PROBE, INFO,
			        "Multiple queues are not supported while MSI-X "
			        "is disabled.  Disabling Multiple Queues.\n");
			*aflags &= ~IXGBE_FLAG_MQ_CAPABLE;
		}
	}
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
	{ /* Direct Cache Access (DCA) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Direct Cache Access (DCA)",
			.err  = "defaulting to Enabled",
			.def  = IXGBE_MAX_DCA,
			.arg  = { .r = { .min = OPTION_DISABLED,
					 .max = IXGBE_MAX_DCA}}
		};
		unsigned int dca = opt.def;

#ifdef module_param_array
		if (num_DCA > bd) {
#endif
			dca = DCA[bd];
			ixgbe_validate_option(&dca, &opt);
			if (!dca)
				*aflags &= ~IXGBE_FLAG_DCA_CAPABLE;

			/* Check Interoperability */
			if (!(*aflags & IXGBE_FLAG_DCA_CAPABLE)) {
				DPRINTK(PROBE, INFO, "DCA is disabled\n");
				*aflags &= ~IXGBE_FLAG_DCA_ENABLED;
			}

			if (dca == IXGBE_MAX_DCA) {
				DPRINTK(PROBE, INFO,
				        "DCA enabled for rx data\n");
				adapter->flags |= IXGBE_FLAG_DCA_ENABLED_DATA;
			}
#ifdef module_param_array
		} else {
			/* make sure to clear the capability flag if the
			 * option is disabled by default above */
			if (opt.def == OPTION_DISABLED)
				*aflags &= ~IXGBE_FLAG_DCA_CAPABLE;
		}
#endif
		if (dca == IXGBE_MAX_DCA)
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED_DATA;
	}
#endif /* CONFIG_DCA or CONFIG_DCA_MODULE */
	{ /* Receive-Side Scaling (RSS) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Receive-Side Scaling (RSS)",
			.err  = "using default.",
#ifdef __VMKLNX__
			.def  = OPTION_DISABLED,
#else
			.def  = OPTION_ENABLED,
#endif /* __VMKLNX__ */
			.arg  = { .r = { .min = OPTION_DISABLED,
					 .max = IXGBE_MAX_RSS_INDICES}}
		};
		unsigned int rss = RSS[bd];

#ifdef module_param_array
		if (num_RSS > bd) {
#endif
			if (rss != OPTION_ENABLED)
				ixgbe_validate_option(&rss, &opt);
			/*
			 * we cannot use an else since validate option may
			 * have changed the state of RSS
			 */
			if (rss == OPTION_ENABLED) {
				/*
				 * Base it off num_online_cpus() with
				 * a hardware limit cap.
				 */
				rss = min(IXGBE_MAX_RSS_INDICES,
				          (int)num_online_cpus());
			}
			feature[RING_F_RSS].indices = rss ? : 1;
			if (rss)
				*aflags |= IXGBE_FLAG_RSS_ENABLED;
			else
				*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_DISABLED) {
				*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
			} else {
				rss = min(IXGBE_MAX_RSS_INDICES,
				          (int)num_online_cpus());
				feature[RING_F_RSS].indices = rss;
				if (rss)
					*aflags |= IXGBE_FLAG_RSS_ENABLED;
				else
					*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
			}
		}
#endif
		/* Check Interoperability */
		if (*aflags & IXGBE_FLAG_RSS_ENABLED) {
			if (!(*aflags & IXGBE_FLAG_RSS_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "RSS is not supported on this "
				        "hardware.  Disabling RSS.\n");
				*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
				feature[RING_F_RSS].indices = 0;
			} else if (!(*aflags & IXGBE_FLAG_MQ_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "RSS is not supported while multiple "
				        "queues are disabled.  "
				        "Disabling RSS.\n");
				*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
				*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
				feature[RING_F_RSS].indices = 0;
			}
		}
#ifdef __VMKLNX__
		/* Add extra ESX specific interoperability check */
                if (*aflags & IXGBE_FLAG_RSS_ENABLED) {
			/* RSS is not supported on 82588 */
			if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
				*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
				feature[RING_F_RSS].indices = 0;
				DPRINTK(PROBE, ERR,
                                        "RSS is not supported on 82598. "
                                        "Disabling RSS.\n");
			}
			/* The only value allowed is IXGBE_ESX_RSS_QUEUES */ 
			if (feature[RING_F_RSS].indices !=
				IXGBE_ESX_RSS_QUEUES) {
				feature[RING_F_RSS].indices =
					IXGBE_ESX_RSS_QUEUES;
				DPRINTK(PROBE, ERR,
					"RSS queues can only be set to %d. "
					"Changing RSS queue number.\n",
					IXGBE_ESX_RSS_QUEUES);
			}
		}
		/* Explicitly enable UDP RSS flags since they are turn off
		 * by default.
		 */
		if (*aflags & IXGBE_FLAG_RSS_ENABLED) {
			adapter->flags2 |= IXGBE_FLAG2_RSS_FIELD_IPV4_UDP;
			adapter->flags2 |= IXGBE_FLAG2_RSS_FIELD_IPV6_UDP;
		}
#endif /* __VMKLNX__ */
	}
	{
		adapter->flags2 &= ~IXGBE_FLAG2_CNA_ENABLED;
		if (adapter->hw.mac.type != ixgbe_mac_82598EB) {
			struct ixgbe_option opt = {
				.type = range_option,
				.name = "Enabled/Disable CNA",
				.err  = "defaulting to CNA Enabled",
				.def = 1,
				.arg  = { .r = { .min = OPTION_DISABLED,
						.max = IXGBE_FCRETA_SIZE} }
			};

			unsigned int cna_qs = opt.def;
#ifdef module_param_array
			if (num_CNA > bd)
				cna_qs = CNA[bd];
#endif
			ixgbe_validate_option(&cna_qs, &opt);
			/* enable CNA only when MSIX is enabled */
			if (!(*aflags & IXGBE_FLAG_MSIX_CAPABLE))
				cna_qs = 0;
			/* Disable CNA when RSS is enabled */
			if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
				cna_qs = 0;
				DPRINTK(PROBE, ERR, "CNA turned off when RSS "
				" is enabled thru mod_param\n");
			}
			if (cna_qs) {
				adapter->flags2 |= IXGBE_FLAG2_CNA_ENABLED;
				adapter->flags |= IXGBE_FLAG_FCOE_ENABLED;
				*aflags |= IXGBE_FLAG_DCB_ENABLED;
			}

			feature[RING_F_FCOE].indices = cna_qs;
			DPRINTK(PROBE, INFO, "CNA %sabled, %d queues\n",
				(adapter->flags2 & IXGBE_FLAG2_CNA_ENABLED) ?
				"en" : "dis", cna_qs);
		}

		/* Check Interoperability */
		if (adapter->flags2 & IXGBE_FLAG2_CNA_ENABLED) {
			/* disable PS, it doesn't get along with CNA/FCoE */
			adapter->flags &= ~IXGBE_FLAG_RX_PS_CAPABLE;
		}
	}
#ifdef IXGBE_VMDQ
	{ /* Virtual Machine Device Queues (VMDQ) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Virtual Machine Device Queues (VMDQ)",
			.err  = "defaulting to Disabled",
#ifndef __VMKLNX__
			.def  = OPTION_DISABLED,
#else
			.def  = OPTION_ENABLED,
#endif
			.arg  = { .r = { .min = OPTION_DISABLED,
#ifndef __VMKLNX__
					 .max = IXGBE_MAX_VMDQ_INDICES
#else
					 .max = 16
#endif
				}}
		};

#ifdef module_param_array
		if (num_VMDQ > bd) {
#endif
			unsigned int vmdq = VMDQ[bd];
#ifdef __VMKLNX__
			if (vmdq > opt.arg.r.max) {
				DPRINTK(PROBE, INFO,
					"ixgbe: Invalid %s specified: (%d); "
					"valid range: 0 - 16. Defaulting to %d \n",
					opt.name, vmdq, IXGBE_DEFAULT_NUM_VMDQ);
				vmdq = IXGBE_DEFAULT_NUM_VMDQ;
			}

#endif
			ixgbe_validate_option(&vmdq, &opt);
			feature[RING_F_VMDQ].indices = vmdq;
			adapter->flags2 |= IXGBE_FLAG2_VMDQ_DEFAULT_OVERRIDE;
			/* zero or one both mean disabled from our driver's
			 * perspective */
			if (vmdq > 1)
				*aflags |= IXGBE_FLAG_VMDQ_ENABLED;
			else
				*aflags &= ~IXGBE_FLAG_VMDQ_ENABLED;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_DISABLED) {
				*aflags &= ~IXGBE_FLAG_VMDQ_ENABLED;
			} else {
				feature[RING_F_VMDQ].indices = IXGBE_DEFAULT_NUM_VMDQ;
				*aflags |= IXGBE_FLAG_VMDQ_ENABLED;
			}
		}
#endif
		/* Check Interoperability */
		if (*aflags & IXGBE_FLAG_VMDQ_ENABLED) {
			if (!(*aflags & IXGBE_FLAG_VMDQ_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "VMDQ is not supported on this "
				        "hardware.  Disabling VMDQ.\n");
				*aflags &= ~IXGBE_FLAG_VMDQ_ENABLED;
				feature[RING_F_VMDQ].indices = 0;
			} else if (!(*aflags & IXGBE_FLAG_MQ_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "VMDQ is not supported while multiple "
				        "queues are disabled.  "
				        "Disabling VMDQ.\n");
				*aflags &= ~IXGBE_FLAG_VMDQ_ENABLED;
				feature[RING_F_VMDQ].indices = 0;
			}

			if  (adapter->hw.mac.type == ixgbe_mac_82598EB)
				feature[RING_F_VMDQ].indices =
				          min(feature[RING_F_VMDQ].indices, 16);
#ifndef __VMKLNX__
#endif
		} 
	}
#endif /* IXGBE_VMDQ */
	{ /* Single Root I/O Virtualization (SR-IOV) */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "I/O Virtualization (IOV)",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED,
			.arg  = { .r = { .min = OPTION_DISABLED,
#ifndef __VMKLNX__
					 .max = IXGBE_MAX_VF_FUNCTIONS}}
#else
					 .max = MAX_SRIOV_VFS}}
#endif /* __VMKLNX__ */
		};

#ifdef module_param_array
		if (num_max_vfs > bd) {
#endif
#ifndef __VMKLNX__
			unsigned int vfs = max_vfs[bd];
#else
			int vfs = max_vfs[bd];

			if (vfs < 0)
				vfs = 0;
			else 
				vfs = min(vfs, MAX_SRIOV_VFS);
#endif
			ixgbe_validate_option(&vfs, &opt);

			adapter->num_vfs = vfs;
			if (vfs)
				*aflags |= IXGBE_FLAG_SRIOV_ENABLED;
			else
				*aflags &= ~IXGBE_FLAG_SRIOV_ENABLED;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_DISABLED) {
				adapter->num_vfs = 0;
				*aflags &= ~IXGBE_FLAG_SRIOV_ENABLED;
			} else {
				adapter->num_vfs = opt.def;
				*aflags |= IXGBE_FLAG_SRIOV_ENABLED;
			}
		}
#endif

		/* Check Interoperability */
		if (*aflags & IXGBE_FLAG_SRIOV_ENABLED) {
#ifdef __VMKLNX__
			/* Uplink is not usable by the hypervisor, so
			   disable VMDQ, RSS and DCB... */
			adapter->flags2 &= ~IXGBE_FLAG2_VMDQ_DEFAULT_OVERRIDE;
			*aflags &= ~IXGBE_FLAG_VMDQ_CAPABLE;
			*aflags &= ~IXGBE_FLAG_VMDQ_ENABLED;
			feature[RING_F_VMDQ].indices = 0;
			*aflags &= ~IXGBE_FLAG_DCB_CAPABLE;
			*aflags &= ~IXGBE_FLAG_DCB_ENABLED;
			adapter->flags2 &= ~IXGBE_FLAG2_CNA_ENABLED;
			adapter->flags &= ~IXGBE_FLAG_FCOE_ENABLED;
			feature[RING_F_FCOE].indices = 0;
			*aflags &= ~IXGBE_FLAG_RSS_CAPABLE;
			*aflags &= ~IXGBE_FLAG_RSS_ENABLED;
			feature[RING_F_RSS].indices = 0;
			/* RSC disabled below */
#endif /* __VMKLNX__ */
			if (!(*aflags & IXGBE_FLAG_SRIOV_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "IOV is not supported on this "
				        "hardware.  Disabling IOV.\n");
				*aflags &= ~IXGBE_FLAG_SRIOV_ENABLED;
				adapter->num_vfs = 0;
			} else if (!(*aflags & IXGBE_FLAG_MQ_CAPABLE)) {
				DPRINTK(PROBE, INFO,
				        "IOV is not supported while multiple "
				        "queues are disabled.  "
				        "Disabling IOV.\n");
				*aflags &= ~IXGBE_FLAG_SRIOV_ENABLED;
				adapter->num_vfs = 0;
			} else {
#ifdef __VMKLNX__
				int max_vfs = MAX_SRIOV_VFS;

				/*
				 * if num_vfs wants the max vfs possible, scale
				 * it down to save room for VMDQ and CNA queues,
				 * or at least one default queue.  If CNA is
				 * enabled, max queues is 32 rather than 64
				 */
				if (feature[RING_F_FCOE].indices)
					max_vfs = 32 -
						feature[RING_F_FCOE].indices;
				max_vfs -= feature[RING_F_VMDQ].indices;
				adapter->num_vfs = min(adapter->num_vfs,
						       max_vfs);

#endif
				*aflags &= ~IXGBE_FLAG_RSS_CAPABLE;
				adapter->flags2 &= ~IXGBE_FLAG2_RSC_CAPABLE;
			}
		}
	}
	{ /* L2 Loopback Enable in SR-IOV mode */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "L2 Loopback Enable",
			.err  = "defaulting to Enable",
			.def  = OPTION_ENABLED,
			.arg  = { .r = { .min = OPTION_DISABLED,
					 .max = OPTION_ENABLED}}
		};

#ifdef module_param_array
		if (num_L2LBen > bd) {
#endif
			unsigned int l2LBen = L2LBen[bd];
			ixgbe_validate_option(&l2LBen, &opt);
			if (l2LBen)
				adapter->flags |= IXGBE_FLAG_SRIOV_L2LOOPBACK_ENABLE;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_ENABLED)
				adapter->flags |= IXGBE_FLAG_SRIOV_L2LOOPBACK_ENABLE;
		}
#endif
	}
	{ /* Interrupt Throttling Rate */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Interrupt Throttling Rate (ints/sec)",
			.err  = "using default of "__MODULE_STRING(DEFAULT_ITR),
			.def  = DEFAULT_ITR,
			.arg  = { .r = { .min = MIN_ITR,
					 .max = MAX_ITR }}
		};

#ifdef module_param_array
		if (num_InterruptThrottleRate > bd) {
#endif
			u32 itr = InterruptThrottleRate[bd];
			switch (itr) {
			case 0:
				DPRINTK(PROBE, INFO, "%s turned off\n",
				        opt.name);
				adapter->rx_itr_setting = 0;
				break;
			case 1:
				DPRINTK(PROBE, INFO, "dynamic interrupt "
                                        "throttling enabled\n");
				adapter->rx_itr_setting = 1;
				break;
			default:
				ixgbe_validate_option(&itr, &opt);
				/* the first bit is used as control */
				adapter->rx_itr_setting = (1000000/itr) << 2;
				break;
			}
			adapter->tx_itr_setting = adapter->rx_itr_setting;
#ifdef module_param_array
		} else {
#ifdef __VMKLNX__
			adapter->rx_itr_setting = IXGBE_16K_ITR;
			adapter->tx_itr_setting = IXGBE_8K_ITR;
#else
			adapter->rx_itr_setting = opt.def;
			adapter->tx_itr_setting = opt.def;
#endif
		}
#endif
	}
#ifndef IXGBE_NO_LLI
	{ /* Low Latency Interrupt TCP Port*/
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt TCP Port",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIPORT),
			.def  = DEFAULT_LLIPORT,
			.arg  = { .r = { .min = MIN_LLIPORT,
					 .max = MAX_LLIPORT }}
		};

#ifdef module_param_array
		if (num_LLIPort > bd) {
#endif
			adapter->lli_port = LLIPort[bd];
			if (adapter->lli_port) {
				ixgbe_validate_option(&adapter->lli_port, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_port = opt.def;
		}
#endif
	}
	{ /* Low Latency Interrupt on Packet Size */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on Packet Size",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLISIZE),
			.def  = DEFAULT_LLISIZE,
			.arg  = { .r = { .min = MIN_LLISIZE,
					 .max = MAX_LLISIZE }}
		};

#ifdef module_param_array
		if (num_LLISize > bd) {
#endif
			adapter->lli_size = LLISize[bd];
			if (adapter->lli_size) {
				ixgbe_validate_option(&adapter->lli_size, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_size = opt.def;
		}
#endif
	}
	{ /*Low Latency Interrupt on TCP Push flag*/
		static struct ixgbe_option opt = {
			.type = enable_option,
			.name = "Low Latency Interrupt on TCP Push flag",
			.err  = "defaulting to Disabled",
			.def  = OPTION_DISABLED
		};

#ifdef module_param_array
		if (num_LLIPush > bd) {
#endif
			unsigned int lli_push = LLIPush[bd];
			ixgbe_validate_option(&lli_push, &opt);
			if (lli_push)
				*aflags |= IXGBE_FLAG_LLI_PUSH;
			else
				*aflags &= ~IXGBE_FLAG_LLI_PUSH;
#ifdef module_param_array
		} else {
			if (opt.def == OPTION_ENABLED)
				*aflags |= IXGBE_FLAG_LLI_PUSH;
			else
				*aflags &= ~IXGBE_FLAG_LLI_PUSH;
		}
#endif
	}
	{ /* Low Latency Interrupt EtherType*/
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on Ethernet Protocol Type",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIETYPE),
			.def  = DEFAULT_LLIETYPE,
			.arg  = { .r = { .min = MIN_LLIETYPE,
					 .max = MAX_LLIETYPE }}
		};

#ifdef module_param_array
		if (num_LLIEType > bd) {
#endif
			adapter->lli_etype = LLIEType[bd];
			if (adapter->lli_etype) {
				ixgbe_validate_option(&adapter->lli_etype, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_etype = opt.def;
		}
#endif
	}
	{ /* LLI VLAN Priority */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Low Latency Interrupt on VLAN priority threashold",
			.err  = "using default of "
					__MODULE_STRING(DEFAULT_LLIVLANP),
			.def  = DEFAULT_LLIVLANP,
			.arg  = { .r = { .min = MIN_LLIVLANP,
					 .max = MAX_LLIVLANP }}
		};

#ifdef module_param_array
		if (num_LLIVLANP > bd) {
#endif
			adapter->lli_vlan_pri = LLIVLANP[bd];
			if (adapter->lli_vlan_pri) {
				ixgbe_validate_option(&adapter->lli_vlan_pri, &opt);
			} else {
				DPRINTK(PROBE, INFO, "%s turned off\n",
					opt.name);
			}
#ifdef module_param_array
		} else {
			adapter->lli_vlan_pri = opt.def;
		}
#endif
	}
#endif /* IXGBE_NO_LLI */
	{ /* Rx buffer mode */
		unsigned int rx_buf_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Rx buffer mode",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_RXBUFMODE),
			.def = IXGBE_DEFAULT_RXBUFMODE,
			.arg = {.r = {.min = IXGBE_RXBUFMODE_1BUF_ALWAYS,
#ifndef __VMKLNX__
				      .max = IXGBE_RXBUFMODE_BOUNCE_BUF}}
#else
				      .max = IXGBE_RXBUFMODE_OPTIMAL}}
#endif
		};

#ifdef module_param_array
		if (num_RxBufferMode > bd) {
#endif
			switch (adapter->hw.mac.type) {
#ifdef __VMKLNX__
			/*
			 * for 82599 only 1BUF supported value - erratum 45
			 * for 82598 only 1BUF supported value - for ESX
			 */
			case ixgbe_mac_82599EB:
			case ixgbe_mac_82598EB:
				rx_buf_mode = IXGBE_RXBUFMODE_1BUF_ALWAYS;
				break;
#endif
			default:
				rx_buf_mode = RxBufferMode[bd];
				break;
			}

			ixgbe_validate_option(&rx_buf_mode, &opt);
			switch (rx_buf_mode) {
			case IXGBE_RXBUFMODE_OPTIMAL:
				*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
				*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
#ifndef __VMKLNX__
			case IXGBE_RXBUFMODE_BOUNCE_BUF:
				*aflags |= IXGBE_FLAG_RX_BB_CAPABLE;
#endif
			case IXGBE_RXBUFMODE_PS_ALWAYS:
				/*
				 *  82599 only supports the bounce buffer
				 *  version of packet split due to hardware
				 *  erratum 45
				 */
				if (adapter->hw.mac.type == ixgbe_mac_82599EB)
					*aflags |= IXGBE_FLAG_RX_BB_CAPABLE;
				*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
			case IXGBE_RXBUFMODE_1BUF_ALWAYS:
				*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
				break;
			default:
				break;
			}
#ifdef module_param_array
		} else {
#ifdef __VMKLNX__
			/* for 82598 & 82599 only support 1BUF value on ESX */
			if ((adapter->hw.mac.type != ixgbe_mac_82598EB) &&
			    (adapter->hw.mac.type != ixgbe_mac_82599EB))
#else
			if (adapter->hw.mac.type == ixgbe_mac_82599EB)
				*aflags |= IXGBE_FLAG_RX_BB_CAPABLE;
#endif
				*aflags |= IXGBE_FLAG_RX_PS_CAPABLE;
			*aflags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
		}
#endif

#ifdef __VMKLNX__
		*aflags &= ~IXGBE_FLAG_RX_PS_CAPABLE;
		DPRINTK(PROBE, INFO, "Packet split is not supported.\n");
#endif
	}
#ifdef HAVE_TX_MQ
	{ /* Flow Director filtering mode */
		unsigned int fdir_filter_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Flow Director filtering mode",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_FDIR_FILTER),
			.def = IXGBE_DEFAULT_FDIR_FILTER,
			.arg = {.r = {.min = IXGBE_FDIR_FILTER_OFF,
				      .max = IXGBE_FDIR_FILTER_PERFECT}}
		};

		*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
		*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
		feature[RING_F_FDIR].indices = IXGBE_MAX_FDIR_INDICES;

		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			goto no_flow_director;

		if (num_FdirMode > bd) {
			fdir_filter_mode = FdirMode[bd];
			ixgbe_validate_option(&fdir_filter_mode, &opt);

			switch (fdir_filter_mode) {
			case IXGBE_FDIR_FILTER_PERFECT:
#ifdef ETHTOOL_GRXRINGS
				*aflags |= IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
				DPRINTK(PROBE, INFO,
				        "Flow Director perfect filtering enabled\n");
#else /* ETHTOOL_GRXRINGS */
				DPRINTK(PROBE, INFO, "No ethtool support for "
				        "Flow Director perfect filtering.\n");
#endif /* ETHTOOL_GRXRINGS */
				break;
			case IXGBE_FDIR_FILTER_HASH:
				*aflags |= IXGBE_FLAG_FDIR_HASH_CAPABLE;
				DPRINTK(PROBE, INFO,
				        "Flow Director hash filtering enabled\n");
				break;
			case IXGBE_FDIR_FILTER_OFF:
			default:
				DPRINTK(PROBE, INFO, "Flow Director disabled\n");
				break;
			}
		} else {
			*aflags |= IXGBE_FLAG_FDIR_HASH_CAPABLE;
		}
		/* Check interoperability */
		if ((*aflags & (IXGBE_FLAG_FDIR_HASH_CAPABLE |
		                IXGBE_FLAG_FDIR_PERFECT_CAPABLE)) &&
		    !(*aflags & IXGBE_FLAG_MQ_CAPABLE)) {
			DPRINTK(PROBE, INFO,
				"Flow Director is not supported while "
				"multiple queues are disabled. "
				"Disabling Flow Director\n");
			*aflags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
			*aflags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
		}

		/* limit the number of queues for FDIR using RSS param */
		if (feature[RING_F_RSS].indices && num_RSS > bd && RSS[bd])
			feature[RING_F_FDIR].indices = feature[RING_F_RSS].indices;

no_flow_director:
		/* empty code line with semi-colon */ ;
	}
	{ /* Flow Director packet buffer allocation */
		unsigned int fdir_pballoc_mode;
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Flow Director packet buffer allocation",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_FDIR_PBALLOC),
			.def = IXGBE_DEFAULT_FDIR_PBALLOC,
			.arg = {.r = {.min = IXGBE_FDIR_PBALLOC_64K,
				      .max = IXGBE_FDIR_PBALLOC_256K}}
		};
		char pstring[10];

		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			goto no_fdir_pballoc;
		if (num_FdirPballoc > bd) {
			fdir_pballoc_mode = FdirPballoc[bd];
			ixgbe_validate_option(&fdir_pballoc_mode, &opt);
			switch (fdir_pballoc_mode) {
			case IXGBE_FDIR_PBALLOC_64K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_64K;
				sprintf(pstring, "64kB");
				break;
			case IXGBE_FDIR_PBALLOC_128K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_128K;
				sprintf(pstring, "128kB");
				break;
			case IXGBE_FDIR_PBALLOC_256K:
				adapter->fdir_pballoc = IXGBE_FDIR_PBALLOC_256K;
				sprintf(pstring, "256kB");
				break;
			default:
				break;
			}
			DPRINTK(PROBE, INFO,
			        "Flow Director will be allocated %s of packet buffer\n",
			        pstring);
		} else {
			adapter->fdir_pballoc = opt.def;
		}
no_fdir_pballoc:
		/* empty code line with semi-colon */ ;
	}
	{ /* Flow Director ATR Tx sample packet rate */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Software ATR Tx packet sample rate",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_ATR_SAMPLE_RATE),
			.def = IXGBE_DEFAULT_ATR_SAMPLE_RATE,
			.arg = {.r = {.min = IXGBE_ATR_SAMPLE_RATE_OFF,
				      .max = IXGBE_MAX_ATR_SAMPLE_RATE}}
		};
		static const char atr_string[] =
		                            "ATR Tx Packet sample rate set to";

		adapter->atr_sample_rate = IXGBE_ATR_SAMPLE_RATE_OFF;
		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			goto no_fdir_sample;

		if (num_AtrSampleRate > bd) {
			adapter->atr_sample_rate = AtrSampleRate[bd];

			if (adapter->atr_sample_rate) {
				ixgbe_validate_option(&adapter->atr_sample_rate,
				                      &opt);
				DPRINTK(PROBE, INFO, "%s %d\n", atr_string,
				        adapter->atr_sample_rate);
			}
		} else {
			adapter->atr_sample_rate = opt.def;
		}
no_fdir_sample:
		/* empty code line with semi-colon */ ;
	}
#endif /* HAVE_TX_MQ */
	{ /* LRO - Enable Large Receive Offload */
		struct ixgbe_option opt = {
			.type = enable_option,
			.name = "LRO - Large Receive Offload",
			.err  = "defaulting to Enabled",
#ifndef __VMKLNX__
			.def  = OPTION_ENABLED
#else
			.def  = OPTION_DISABLED
#endif
		};
		struct net_device *netdev = adapter->netdev;

		if (!(adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE))
			opt.def = OPTION_DISABLED;

#ifdef module_param_array
		if (num_LRO > bd) {
#endif
			unsigned int lro = LRO[bd];
			ixgbe_validate_option(&lro, &opt);
			if (lro)
				netdev->features |= NETIF_F_LRO;
			else
				netdev->features &= ~NETIF_F_LRO;
#ifdef module_param_array
		} else if (opt.def == OPTION_ENABLED) {
			netdev->features |= NETIF_F_LRO;
		} else {
			netdev->features &= ~NETIF_F_LRO;
		}
#endif
		if ((netdev->features & NETIF_F_LRO) &&
		    !(adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE)) {
			DPRINTK(PROBE, INFO,
			        "RSC is not supported on this "
			        "hardware.  Disabling RSC.\n");
			netdev->features &= ~NETIF_F_LRO;
		}
	}
#ifndef __VMKLNX__
	{ /* Node assignment */
		static struct ixgbe_option opt = {
			.type = range_option,
			.name = "Node to start on",
#ifdef HAVE_EARLY_VMALLOC_NODE
			.err  = "defaulting to 0",
			.def  = 0,
#else
			.err  = "defaulting to -1",
			.def  = -1,
#endif
			.arg  = { .r = { .min = 0,
					 .max = (MAX_NUMNODES - 1)}}
		};
		int node_param = opt.def;

		/* if the default was zero then we need to set the
		 * default value to an online node, which is not
		 * necessarily zero, and the constant initializer
		 * above can't take first_online_node */
		if (node_param == 0)
			/* must set opt.def for validate */
			opt.def = node_param = first_online_node;
#ifdef module_param_array
		if (num_Node > bd) {
#endif
			node_param = Node[bd];
			ixgbe_validate_option((uint *)&node_param, &opt);

			if (node_param != OPTION_UNSET) {
				DPRINTK(PROBE, INFO, "node set to %d\n", node_param);
			}
#ifdef module_param_array
		}
#endif
		/* check sanity of the value */
		if (node_param != -1 && !node_online(node_param)) {
			DPRINTK(PROBE, INFO,
			        "ignoring node set to invalid value %d\n",
			        node_param);
			node_param = opt.def;
		}

		adapter->node = node_param;
	}
#endif /* __VMKLNX__ */
}

