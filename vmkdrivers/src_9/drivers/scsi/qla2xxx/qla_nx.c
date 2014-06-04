/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2011 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"
#include <linux/delay.h>
#include <linux/pci.h>

#define MASK(n)			((1ULL<<(n))-1)
#define MN_WIN(addr) (((addr & 0x1fc0000) >> 1) | ((addr >> 25) & 0x3ff))
#define OCM_WIN(addr) (((addr & 0x1ff0000) >> 1) | ((addr >> 25) & 0x3ff)) //64K?
#define MS_WIN(addr) (addr & 0x0ffc0000)
#define QLA82XX_PCI_MN_2M   (0)
#define QLA82XX_PCI_MS_2M   (0x80000)
#define QLA82XX_PCI_OCM0_2M (0xc0000)
#define VALID_OCM_ADDR(addr) (((addr) & 0x3f800) != 0x3f800)
#define GET_MEM_OFFS_2M(addr) (addr & MASK(18))

/* CRB window related */
#define CRB_BLK(off)	((off >> 20) & 0x3f)
#define CRB_SUBBLK(off)	((off >> 16) & 0xf)
#define CRB_WINDOW_2M	(0x130060)
#define QLA82XX_PCI_CAMQM_2M_END	(0x04800800UL)
#define CRB_HI(off)	((qla82xx_crb_hub_agt[CRB_BLK(off)] << 20) | ((off) & 0xf0000))
#define QLA82XX_PCI_CAMQM_2M_BASE	(0x000ff800UL)
#define CRB_INDIRECT_2M	(0x1e0000UL)
//#define ADDR_ERROR ((unsigned long ) 0xffffffff )

inline int qla82xx_need_reset(struct qla_hw_data *);

static inline void *qla82xx_pci_base_offsetfset(struct qla_hw_data *ha,
		unsigned long off)
{
	if ((off < ha->first_page_group_end) &&
			(off >= ha->first_page_group_start))
		return (void *)(ha->nx_pcibase + off);

	return NULL;
}

#define MAX_CRB_XFORM 60
static unsigned long crb_addr_xform[MAX_CRB_XFORM];
int qla82xx_crb_table_initialized=0;

#define qla82xx_crb_addr_transform(name) \
	(crb_addr_xform[QLA82XX_HW_PX_MAP_CRB_##name] = \
	 QLA82XX_HW_CRB_HUB_AGT_ADR_##name << 20)
static void qla82xx_crb_addr_transform_setup(void)
{
	qla82xx_crb_addr_transform(XDMA);
	qla82xx_crb_addr_transform(TIMR);
	qla82xx_crb_addr_transform(SRE);
	qla82xx_crb_addr_transform(SQN3);
	qla82xx_crb_addr_transform(SQN2);
	qla82xx_crb_addr_transform(SQN1);
	qla82xx_crb_addr_transform(SQN0);
	qla82xx_crb_addr_transform(SQS3);
	qla82xx_crb_addr_transform(SQS2);
	qla82xx_crb_addr_transform(SQS1);
	qla82xx_crb_addr_transform(SQS0);
	qla82xx_crb_addr_transform(RPMX7);
	qla82xx_crb_addr_transform(RPMX6);
	qla82xx_crb_addr_transform(RPMX5);
	qla82xx_crb_addr_transform(RPMX4);
	qla82xx_crb_addr_transform(RPMX3);
	qla82xx_crb_addr_transform(RPMX2);
	qla82xx_crb_addr_transform(RPMX1);
	qla82xx_crb_addr_transform(RPMX0);
	qla82xx_crb_addr_transform(ROMUSB);
	qla82xx_crb_addr_transform(SN);
	qla82xx_crb_addr_transform(QMN);
	qla82xx_crb_addr_transform(QMS);
	qla82xx_crb_addr_transform(PGNI);
	qla82xx_crb_addr_transform(PGND);
	qla82xx_crb_addr_transform(PGN3);
	qla82xx_crb_addr_transform(PGN2);
	qla82xx_crb_addr_transform(PGN1);
	qla82xx_crb_addr_transform(PGN0);
	qla82xx_crb_addr_transform(PGSI);
	qla82xx_crb_addr_transform(PGSD);
	qla82xx_crb_addr_transform(PGS3);
	qla82xx_crb_addr_transform(PGS2);
	qla82xx_crb_addr_transform(PGS1);
	qla82xx_crb_addr_transform(PGS0);
	qla82xx_crb_addr_transform(PS);
	qla82xx_crb_addr_transform(PH);
	qla82xx_crb_addr_transform(NIU);
	qla82xx_crb_addr_transform(I2Q);
	qla82xx_crb_addr_transform(EG);
	qla82xx_crb_addr_transform(MN);
	qla82xx_crb_addr_transform(MS);
	qla82xx_crb_addr_transform(CAS2);
	qla82xx_crb_addr_transform(CAS1);
	qla82xx_crb_addr_transform(CAS0);
	qla82xx_crb_addr_transform(CAM);
	qla82xx_crb_addr_transform(C2C1);
	qla82xx_crb_addr_transform(C2C0);
	qla82xx_crb_addr_transform(SMB);
	qla82xx_crb_addr_transform(OCM0);
	/*
	 * Used only in P3 just define it for P2 also.
	 */
	qla82xx_crb_addr_transform(I2C0);

	qla82xx_crb_table_initialized = 1;
}

crb_128M_2M_block_map_t crb_128M_2M_map[64] = {
	{{{0, 0,         0,         0}}},		/* 0: PCI */
	{{{1, 0x0100000, 0x0102000, 0x120000},	/* 1: PCIE */
		 {1, 0x0110000, 0x0120000, 0x130000},
		 {1, 0x0120000, 0x0122000, 0x124000},
		 {1, 0x0130000, 0x0132000, 0x126000},
		 {1, 0x0140000, 0x0142000, 0x128000},
		 {1, 0x0150000, 0x0152000, 0x12a000},
		 {1, 0x0160000, 0x0170000, 0x110000},
		 {1, 0x0170000, 0x0172000, 0x12e000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {1, 0x01e0000, 0x01e0800, 0x122000},
		 {0, 0x0000000, 0x0000000, 0x000000}}},
	{{{1, 0x0200000, 0x0210000, 0x180000}}},/* 2: MN */
	{{{0, 0,         0,         0}}},	    /* 3: */
	{{{1, 0x0400000, 0x0401000, 0x169000}}},/* 4: P2NR1 */
	{{{1, 0x0500000, 0x0510000, 0x140000}}},/* 5: SRE   */
	{{{1, 0x0600000, 0x0610000, 0x1c0000}}},/* 6: NIU   */
	{{{1, 0x0700000, 0x0704000, 0x1b8000}}},/* 7: QM    */
	{{{1, 0x0800000, 0x0802000, 0x170000},  /* 8: SQM0  */
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {1, 0x08f0000, 0x08f2000, 0x172000}}},
	{{{1, 0x0900000, 0x0902000, 0x174000},	/* 9: SQM1*/
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {1, 0x09f0000, 0x09f2000, 0x176000}}},
	{{{0, 0x0a00000, 0x0a02000, 0x178000},	/* 10: SQM2*/
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {1, 0x0af0000, 0x0af2000, 0x17a000}}},
	{{{0, 0x0b00000, 0x0b02000, 0x17c000},	/* 11: SQM3*/
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {1, 0x0bf0000, 0x0bf2000, 0x17e000}}},
	{{{1, 0x0c00000, 0x0c04000, 0x1d4000}}},/* 12: I2Q */
	{{{1, 0x0d00000, 0x0d04000, 0x1a4000}}},/* 13: TMR */
	{{{1, 0x0e00000, 0x0e04000, 0x1a0000}}},/* 14: ROMUSB */
	{{{1, 0x0f00000, 0x0f01000, 0x164000}}},/* 15: PEG4 */
	{{{0, 0x1000000, 0x1004000, 0x1a8000}}},/* 16: XDMA */
	{{{1, 0x1100000, 0x1101000, 0x160000}}},/* 17: PEG0 */
	{{{1, 0x1200000, 0x1201000, 0x161000}}},/* 18: PEG1 */
	{{{1, 0x1300000, 0x1301000, 0x162000}}},/* 19: PEG2 */
	{{{1, 0x1400000, 0x1401000, 0x163000}}},/* 20: PEG3 */
	{{{1, 0x1500000, 0x1501000, 0x165000}}},/* 21: P2ND */
	{{{1, 0x1600000, 0x1601000, 0x166000}}},/* 22: P2NI */
	{{{0, 0,         0,         0}}},	/* 23: */
	{{{0, 0,         0,         0}}},	/* 24: */
	{{{0, 0,         0,         0}}},	/* 25: */
	{{{0, 0,         0,         0}}},	/* 26: */
	{{{0, 0,         0,         0}}},	/* 27: */
	{{{0, 0,         0,         0}}},	/* 28: */
	{{{1, 0x1d00000, 0x1d10000, 0x190000}}},/* 29: MS */
	{{{1, 0x1e00000, 0x1e01000, 0x16a000}}},/* 30: P2NR2 */
	{{{1, 0x1f00000, 0x1f10000, 0x150000}}},/* 31: EPG */
	{{{0}}},				/* 32: PCI */
	{{{1, 0x2100000, 0x2102000, 0x120000},	/* 33: PCIE */
		 {1, 0x2110000, 0x2120000, 0x130000},
		 {1, 0x2120000, 0x2122000, 0x124000},
		 {1, 0x2130000, 0x2132000, 0x126000},
		 {1, 0x2140000, 0x2142000, 0x128000},
		 {1, 0x2150000, 0x2152000, 0x12a000},
		 {1, 0x2160000, 0x2170000, 0x110000},
		 {1, 0x2170000, 0x2172000, 0x12e000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000},
		 {0, 0x0000000, 0x0000000, 0x000000}}},
	{{{1, 0x2200000, 0x2204000, 0x1b0000}}},/* 34: CAM */
	{{{0}}},				/* 35: */
	{{{0}}},				/* 36: */
	{{{0}}},				/* 37: */
	{{{0}}},				/* 38: */
	{{{0}}},				/* 39: */
	{{{1, 0x2800000, 0x2804000, 0x1a4000}}},/* 40: TMR */
	{{{1, 0x2900000, 0x2901000, 0x16b000}}},/* 41: P2NR3 */
	{{{1, 0x2a00000, 0x2a00400, 0x1ac400}}},/* 42: RPMX1 */
	{{{1, 0x2b00000, 0x2b00400, 0x1ac800}}},/* 43: RPMX2 */
	{{{1, 0x2c00000, 0x2c00400, 0x1acc00}}},/* 44: RPMX3 */
	{{{1, 0x2d00000, 0x2d00400, 0x1ad000}}},/* 45: RPMX4 */
	{{{1, 0x2e00000, 0x2e00400, 0x1ad400}}},/* 46: RPMX5 */
	{{{1, 0x2f00000, 0x2f00400, 0x1ad800}}},/* 47: RPMX6 */
	{{{1, 0x3000000, 0x3000400, 0x1adc00}}},/* 48: RPMX7 */
	{{{0, 0x3100000, 0x3104000, 0x1a8000}}},/* 49: XDMA */
	{{{1, 0x3200000, 0x3204000, 0x1d4000}}},/* 50: I2Q */
	{{{1, 0x3300000, 0x3304000, 0x1a0000}}},/* 51: ROMUSB */
	{{{0}}},				/* 52: */
	{{{1, 0x3500000, 0x3500400, 0x1ac000}}},/* 53: RPMX0 */
	{{{1, 0x3600000, 0x3600400, 0x1ae000}}},/* 54: RPMX8 */
	{{{1, 0x3700000, 0x3700400, 0x1ae400}}},/* 55: RPMX9 */
	{{{1, 0x3800000, 0x3804000, 0x1d0000}}},/* 56: OCM0 */
	{{{1, 0x3900000, 0x3904000, 0x1b4000}}},/* 57: CRYPTO */
	{{{1, 0x3a00000, 0x3a04000, 0x1d8000}}},/* 58: SMB */
	{{{0}}},				/* 59: I2C0 */
	{{{0}}},				/* 60: I2C1 */
	{{{1, 0x3d00000, 0x3d04000, 0x1dc000}}},/* 61: LPC */
	{{{1, 0x3e00000, 0x3e01000, 0x167000}}},/* 62: P2NC */
	{{{1, 0x3f00000, 0x3f01000, 0x168000}}}	/* 63: P2NR0 */
};

/*
 * top 12 bits of crb internal address (hub, agent)
 */
unsigned qla82xx_crb_hub_agt[64] =
{
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PS,
	QLA82XX_HW_CRB_HUB_AGT_ADR_MN,
	QLA82XX_HW_CRB_HUB_AGT_ADR_MS,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SRE,
	QLA82XX_HW_CRB_HUB_AGT_ADR_NIU,
	QLA82XX_HW_CRB_HUB_AGT_ADR_QMN,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SQN0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SQN1,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SQN2,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SQN3,
	QLA82XX_HW_CRB_HUB_AGT_ADR_I2Q,
	QLA82XX_HW_CRB_HUB_AGT_ADR_TIMR,
	QLA82XX_HW_CRB_HUB_AGT_ADR_ROMUSB,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGN4,
	QLA82XX_HW_CRB_HUB_AGT_ADR_XDMA,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGN0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGN1,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGN2,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGN3,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGND,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGNI,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGS0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGS1,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGS2,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGS3,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGSI,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SN,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_EG,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PS,
	QLA82XX_HW_CRB_HUB_AGT_ADR_CAM,
	0,
	0,
	0,
	0,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_TIMR,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX1,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX2,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX3,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX4,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX5,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX6,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX7,
	QLA82XX_HW_CRB_HUB_AGT_ADR_XDMA,
	QLA82XX_HW_CRB_HUB_AGT_ADR_I2Q,
	QLA82XX_HW_CRB_HUB_AGT_ADR_ROMUSB,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX8,
	QLA82XX_HW_CRB_HUB_AGT_ADR_RPMX9,
	QLA82XX_HW_CRB_HUB_AGT_ADR_OCM0,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_SMB,
	QLA82XX_HW_CRB_HUB_AGT_ADR_I2C0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_I2C1,
	0,
	QLA82XX_HW_CRB_HUB_AGT_ADR_PGNC,
	0,
};

/*
 * Set the CRB window based on the offset.
 * Return 0 if successful; 1 otherwise
 */
void qla82xx_pci_change_crbwindow_128M(struct qla_hw_data *ha, int wndw)
{
	WARN_ON(1);
}

/*
 * In: 'off' is offset from CRB space in 128M pci map
 * Out: 'off' is 2M pci map addr
 * side effect: lock crb window
 */
static void qla82xx_pci_set_crbwindow_2M(struct qla_hw_data *ha, u64 *off)
{
	u32 win_read;

	ha->crb_win = CRB_HI(*off);
	writel(ha->crb_win,
		(void *)(CRB_WINDOW_2M + ha->nx_pcibase));

	/* Read back value to make sure write has gone through before trying
	* to use it. */
	win_read = RD_REG_DWORD((void *)(CRB_WINDOW_2M + ha->nx_pcibase));
	if (win_read != ha->crb_win) {
		DEBUG2(qla_printk(KERN_INFO, ha,
			"%s: Written crbwin (0x%x) != Read crbwin (0x%x), off=0x%llx\n",
			__func__, ha->crb_win, win_read, *off));
	}
	*off = (*off & MASK(16)) + CRB_INDIRECT_2M + ha->nx_pcibase;
}

static inline unsigned long
qla82xx_pci_set_crbwindow(struct qla_hw_data *ha, u64 off)
{
	/*
	 * See if we are currently pointing to the region we want to use next.
	 */
	if ((off >= QLA82XX_CRB_PCIX_HOST) && (off < QLA82XX_CRB_DDR_NET)) {
		/*
		 * No need to change window. PCIX and PCIE regs are in both
		 * windows.
		 */
		return (off);
	}

	if ((off >= QLA82XX_CRB_PCIX_HOST) && (off < QLA82XX_CRB_PCIX_HOST2)) {
		/* We are in first CRB window */
		if (ha->curr_window != 0) {
			qla82xx_pci_change_crbwindow_128M(ha, 0);
		}
		return (off);
	}

	if ((off > QLA82XX_CRB_PCIX_HOST2) && (off < QLA82XX_CRB_MAX)) {
		/* We are in second CRB window */
		off = off - QLA82XX_CRB_PCIX_HOST2 + QLA82XX_CRB_PCIX_HOST;

		if (ha->curr_window != 1) {
			qla82xx_pci_change_crbwindow_128M(ha, 1);
			return (off);
		}

		if ((off >= QLA82XX_PCI_DIRECT_CRB) && (off < QLA82XX_PCI_CAMQM_MAX)) {
			/*
			 * We are in the QM or direct access register region - do
			 * nothing
			 */
			return (off);
		}
	}
	/* strange address given */
	dump_stack();
	qla_printk(KERN_WARNING, ha,
			"%s: Warning: qla82xx_pci_set_crbwindow called with"
			" an unknown address(%llx)\n", QLA2XXX_DRIVER_NAME, off);

	return (off);
}

int qla82xx_wr_32(struct qla_hw_data *ha, u64 off, u32 data)
{
	unsigned long flags = 0;
	int rv;

	rv = qla82xx_pci_get_crb_addr_2M(ha, &off);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&ha->hw_lock, flags);
		qla82xx_crb_win_lock(ha);
		qla82xx_pci_set_crbwindow_2M(ha, &off);
	}

	writel(data, (void __iomem *)off);

	if (rv == 1) {
		qla82xx_crb_win_unlock(ha);
		write_unlock_irqrestore(&ha->hw_lock, flags);
	}

	return 0;
}

int qla82xx_rd_32(struct qla_hw_data *ha, u64 off)
{
	unsigned long flags = 0;
	int rv;
	u32 data;

	rv = qla82xx_pci_get_crb_addr_2M(ha, &off);

	BUG_ON(rv == -1);

	if (rv == 1) {
		write_lock_irqsave(&ha->hw_lock, flags);
		qla82xx_crb_win_lock(ha);
		qla82xx_pci_set_crbwindow_2M(ha, &off);
	}
	data = RD_REG_DWORD((void __iomem *)off);

	if (rv == 1) {
		qla82xx_crb_win_unlock(ha);
		write_unlock_irqrestore(&ha->hw_lock, flags);
	}

	return data;
}

/* Minidump related function */
int
qla82xx_md_rw_32(struct qla_hw_data *ha, uint32_t off, u32 data, uint8_t flag)
{
	uint32_t win_read, off_value, rval = 0;
	unsigned long flags = 0;

	off_value = off & 0xFFFF0000;
	writel(off_value, (void *)(CRB_WINDOW_2M + ha->nx_pcibase));

	/* Read back value to make sure write has gone through before trying
	 * to use it.
	 */
	win_read = readl((void *)(CRB_WINDOW_2M + ha->nx_pcibase));
	if (win_read != off_value) {
		DEBUG2(qla_printk(KERN_ERR, ha,
			"%s: Written (0x%x) != Read (0x%x), off=0x%x\n",
			__func__, off_value, win_read, off));
		qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM7_UNLOCK));
		write_unlock_irqrestore(&ha->hw_lock, flags);
		return 0;
	}

	off_value  = off & 0x0000FFFF;
	if (flag)
		writel(data, (void *)(off_value + CRB_INDIRECT_2M + ha->nx_pcibase));
	else
		rval = readl((void *)(off_value + CRB_INDIRECT_2M + ha->nx_pcibase));

	return rval;
}

#define CRB_WIN_LOCK_TIMEOUT 100000000

int qla82xx_crb_win_lock(struct qla_hw_data *ha)
{
	int i;
	int done = 0, timeout = 0;

	while (!done) {
		/* acquire semaphore3 from PCI HW block */
		done = qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM7_LOCK));
		if (done == 1)
			break;
		if (timeout >= CRB_WIN_LOCK_TIMEOUT) {
			return -1;
		}
		timeout++;

		/* Yield CPU */
		if(!in_interrupt())
			schedule();
		else {
			for(i = 0; i < 20; i++)
				cpu_relax(); /* This a nop instr on i386 */
		}
	}
	qla82xx_wr_32(ha, QLA82XX_CRB_WIN_LOCK_ID, ha->portnum);

	return 0;
}

void qla82xx_crb_win_unlock(struct qla_hw_data *ha)
{
	qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM7_UNLOCK));
}

#define IDC_LOCK_TIMEOUT 100000000

int qla82xx_idc_lock(struct qla_hw_data *ha)
{
	int i;
	int done = 0, timeout = 0;

	while (!done) {
		/* acquire semaphore5 from PCI HW block */
		done = qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM5_LOCK));
		if (done == 1)
			break;
		if (timeout >= IDC_LOCK_TIMEOUT) {
			return -1;
		}
		timeout++;

		/* Yield CPU */
		if(!in_interrupt())
			schedule();
		else {
			for(i = 0; i < 20; i++)
				cpu_relax(); /*This a nop instr on i386*/
		}
	}
	//qla82xx_wr_32(ha, QLA82XX_CRB_WIN_LOCK_ID, ha->portnum); MH ??
	return 0;
}

void qla82xx_idc_unlock(struct qla_hw_data *ha)
{
	qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM5_UNLOCK));
}

int qla82xx_pci_get_crb_addr_2M(struct qla_hw_data *ha, u64 *off)
{
	crb_128M_2M_sub_block_map_t *m;

	if (*off >= QLA82XX_CRB_MAX)
		return -1;

	if (*off >= QLA82XX_PCI_CAMQM && (*off < QLA82XX_PCI_CAMQM_2M_END)) {
		*off = (*off - QLA82XX_PCI_CAMQM) + QLA82XX_PCI_CAMQM_2M_BASE + ha->nx_pcibase;
		return 0;
	}

	if (*off < QLA82XX_PCI_CRBSPACE)
		return -1;

	*off -= QLA82XX_PCI_CRBSPACE;
	/*
	 * Try direct map
	 */

	m = &crb_128M_2M_map[CRB_BLK(*off)].sub_block[CRB_SUBBLK(*off)];

	if (m->valid && (m->start_128M <= *off) && (m->end_128M > *off)) {
		*off = *off + m->start_2M - m->start_128M + ha->nx_pcibase;
		return 0;
	}

	/*
	 * Not in direct map, use crb window
	 */
	return 1;
}

/*  PCI Windowing for DDR regions.  */
#define QLA82XX_ADDR_IN_RANGE(addr, low, high)   \
	(((addr) <= (high)) && ((addr) >= (low)))

/*
 * check memory access boundary.
 * used by test agent. support ddr access only for now
 */
static unsigned long qla82xx_pci_mem_bound_check(struct qla_hw_data *ha,
		unsigned long long addr, int size)
{
	if (!QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_DDR_NET, QLA82XX_ADDR_DDR_NET_MAX) ||
			!QLA82XX_ADDR_IN_RANGE(addr + size -1, QLA82XX_ADDR_DDR_NET,
				QLA82XX_ADDR_DDR_NET_MAX) ||
			((size != 1) && (size != 2) && (size != 4) && (size != 8))) {
		return 0;
	}
	return 1;
}

int qla82xx_pci_set_window_warning_count = 0;

unsigned long qla82xx_pci_set_window(struct qla_hw_data *ha,
		unsigned long long addr)
{
	int window;
	u32 win_read;

	if ( QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_DDR_NET,
				QLA82XX_ADDR_DDR_NET_MAX)) {
		/* DDR network side */
		window = MN_WIN(addr);
		ha->ddr_mn_window = window;
		qla82xx_wr_32(ha, ha->mn_win_crb | QLA82XX_PCI_CRBSPACE, window);
		win_read = qla82xx_rd_32(ha, ha->mn_win_crb | QLA82XX_PCI_CRBSPACE);
		if ((win_read << 17) != window) {
			qla_printk(KERN_WARNING, ha,
				"%s: Written MNwin (0x%x) != Read MNwin (0x%x)\n",
				__func__, window, win_read);
		}
		addr = GET_MEM_OFFS_2M(addr) + QLA82XX_PCI_DDR_NET;
	} else if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_OCM0,
				QLA82XX_ADDR_OCM0_MAX)) {
		unsigned int temp1;
		if ((addr & 0x00ff800) == 0xff800) { // if bits 19:18&17:11 are on
			printk("%s: QM access not handled.\n", __func__);
			addr = -1UL;
		}

		window = OCM_WIN(addr);
		ha->ddr_mn_window = window;
		qla82xx_wr_32(ha, ha->mn_win_crb | QLA82XX_PCI_CRBSPACE, window);
		win_read = qla82xx_rd_32(ha, ha->mn_win_crb | QLA82XX_PCI_CRBSPACE);
		temp1 = ((window & 0x1FF) << 7) | ((window & 0x0FFFE0000) >> 17);
		if ( win_read != temp1 ) {
			printk("%s: Written OCMwin (0x%x) != Read OCMwin (0x%x)\n",
				__func__, temp1, win_read);
		}
		addr = GET_MEM_OFFS_2M(addr) + QLA82XX_PCI_OCM0_2M;

	} else if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_QDR_NET,
				QLA82XX_P3_ADDR_QDR_NET_MAX)) {
		/* QDR network side */
		window = MS_WIN(addr);
		ha->qdr_sn_window = window;
		qla82xx_wr_32(ha, ha->ms_win_crb | QLA82XX_PCI_CRBSPACE, window);
		win_read = qla82xx_rd_32(ha, ha->ms_win_crb | QLA82XX_PCI_CRBSPACE);
		if (win_read != window) {
			printk("%s: Written MSwin (0x%x) != Read MSwin (0x%x)\n",
				__func__, window, win_read);
		}
		addr = GET_MEM_OFFS_2M(addr) + QLA82XX_PCI_QDR_NET;

	} else {
		/*
		 * peg gdb frequently accesses memory that doesn't exist,
		 * this limits the chit chat so debugging isn't slowed down.
		 */
		if((qla82xx_pci_set_window_warning_count++ < 8)
				|| (qla82xx_pci_set_window_warning_count%64 == 0)) {
			printk("%s: Warning:%s Unknown address range!\n", __func__,
				QLA2XXX_DRIVER_NAME);
		}
		addr = -1UL;
	}
	/* printk("New address: 0x%08lx\n",addr); */

	return addr;
}

/* check if address is in the same windows as the previous access */
static int qla82xx_pci_is_same_window(struct qla_hw_data *ha,
		unsigned long long addr)
{
	int window;
	unsigned long long qdr_max;

	qdr_max = QLA82XX_P3_ADDR_QDR_NET_MAX;

	if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_DDR_NET, QLA82XX_ADDR_DDR_NET_MAX)) {
		/* DDR network side */
		BUG();	/* MN access can not come here */
	} else if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_OCM0, QLA82XX_ADDR_OCM0_MAX)) {
		return 1;
	} else if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_OCM1, QLA82XX_ADDR_OCM1_MAX)) {
		return 1;
	} else if (QLA82XX_ADDR_IN_RANGE(addr, QLA82XX_ADDR_QDR_NET, qdr_max)) {
		/* QDR network side */
		window = ((addr - QLA82XX_ADDR_QDR_NET) >> 22) & 0x3f;
		if (ha->qdr_sn_window == window) {
			return 1;
		}
	}

	return 0;
}

static int qla82xx_pci_mem_read_direct(struct qla_hw_data *ha,
		u64 off, void *data, int size)
{
	unsigned long   flags;
	void           *addr;
	int             ret = 0;
	u64             start;
	uint8_t         *mem_ptr = NULL;
	unsigned long   mem_base;
	unsigned long   mem_page;

	write_lock_irqsave(&ha->hw_lock, flags);

	/*
	 * If attempting to access unknown address or straddle hw windows,
	 * do not access.
	 */
	if (((start = qla82xx_pci_set_window(ha, off)) == -1UL) ||
			(qla82xx_pci_is_same_window(ha, off + size -1) == 0)) {
		write_unlock_irqrestore(&ha->hw_lock, flags);
		printk(KERN_ERR"%s out of bound pci memory access. "
				"offset is 0x%llx\n", QLA2XXX_DRIVER_NAME, off);
		return -1;
	}

	addr = qla82xx_pci_base_offsetfset(ha, start);
	if(!addr) {
		write_unlock_irqrestore(&ha->hw_lock, flags);
		mem_base = pci_resource_start(ha->pdev, 0);
		mem_page = start & PAGE_MASK;
		/* Map two pages whenever user tries to access addresses in two
		   consecutive pages.
		   */
		if(mem_page != ((start + size - 1) & PAGE_MASK))
			mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE * 2);
		else
			mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
		if(mem_ptr == 0UL) {
			*(u8  *)data = 0;
			return -1;
		}
		addr = mem_ptr;
		addr += start & (PAGE_SIZE - 1);
		write_lock_irqsave(&ha->hw_lock, flags);
	}

	switch (size) {
		case 1:
			*(u8  *)data = readb(addr);
			break;
		case 2:
			*(u16 *)data = readw(addr);
			break;
		case 4:
			*(u32 *)data = readl(addr);
			break;
		case 8:
			*(u64 *)data = readq(addr);
			break;
		default:
			ret = -1;
			break;
	}
	write_unlock_irqrestore(&ha->hw_lock, flags);

	if(mem_ptr)
		iounmap(mem_ptr);
	return ret;
}

static int qla82xx_pci_mem_write_direct(struct qla_hw_data *ha, u64 off,
		void *data, int size)
{
	unsigned long   flags;
	void           *addr;
	int             ret = 0;
	u64             start;
	uint8_t         *mem_ptr = NULL;
	unsigned long   mem_base;
	unsigned long   mem_page;

	write_lock_irqsave(&ha->hw_lock, flags);

	/*
	 * If attempting to access unknown address or straddle hw windows,
	 * do not access.
	 */
	if (((start = qla82xx_pci_set_window(ha, off)) == -1UL) ||
			(qla82xx_pci_is_same_window(ha, off + size -1) == 0)) {
		write_unlock_irqrestore(&ha->hw_lock, flags);
		printk(KERN_ERR"%s out of bound pci memory access. "
				"offset is 0x%llx\n", QLA2XXX_DRIVER_NAME, off);
		return -1;
	}

	addr = qla82xx_pci_base_offsetfset(ha, start);
	if(!addr) {
		write_unlock_irqrestore(&ha->hw_lock, flags);
		mem_base = pci_resource_start(ha->pdev, 0);
		mem_page = start & PAGE_MASK;
		/* Map two pages whenever user tries to access addresses in two
		   consecutive pages.
		   */
		if(mem_page != ((start + size - 1) & PAGE_MASK))
			mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE*2);
		else
			mem_ptr = ioremap(mem_base + mem_page, PAGE_SIZE);
		if(mem_ptr == 0UL) {
			return -1;
		}
		addr = mem_ptr;
		addr += start & (PAGE_SIZE - 1);
		write_lock_irqsave(&ha->hw_lock, flags);
	}

	switch (size) {
		case 1:
			writeb( *(u8  *)data, addr);
			break;
		case 2:
			writew(*(u16 *)data, addr);
			break;
		case 4:
			writel(*(u32 *)data, addr);
			break;
		case 8:
			writeq(*(u64 *)data, addr);
			break;
		default:
			ret = -1;
			break;
	}
	write_unlock_irqrestore(&ha->hw_lock, flags);
	if(mem_ptr)
		iounmap(mem_ptr);

	return ret;
}

int qla82xx_wrmem(struct qla_hw_data *ha, u64 off, void *data, int size)
{
	int i, j, ret = 0, loop, sz[2], off0;
	u32 temp;
	u64 off8, mem_crb, tmpw, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */
	if (off >= QLA82XX_ADDR_QDR_NET && off <= QLA82XX_P3_ADDR_QDR_NET_MAX) {
		mem_crb = QLA82XX_CRB_QDR_NET;
	} else {
		mem_crb = QLA82XX_CRB_DDR_NET;
		if (qla82xx_pci_mem_bound_check(ha, off, size) == 0)
			return(qla82xx_pci_mem_write_direct(ha, off, data, size));
	}

	off8 = off & 0xfffffff8;
	off0 = off & 0x7;
	sz[0] = (size < (8 - off0)) ? size : (8 - off0);
	sz[1] = size - sz[0];
	loop = ((off0 + size - 1) >> 3) + 1;

	if ((size != 8) || (off0 != 0))  {
		for (i = 0; i < loop; i++) {
			if (qla82xx_rdmem(ha, off8 + (i << 3), &word[i], 8))
				return -1;
		}
	}

	switch (size) {
		case 1:
			tmpw = *((u8 *)data);
			break;
		case 2:
			tmpw = *((u16 *)data);
			break;
		case 4:
			tmpw = *((u32 *)data);
			break;
		case 8:
		default:
			tmpw = *((u64 *)data);
			break;
	}

	word[0] &= ~((~(~0ULL << (sz[0] * 8))) << (off0 * 8));
	word[0] |= tmpw << (off0 * 8);

	if (loop == 2) {
		word[1] &= ~(~0ULL << (sz[1] * 8));
		word[1] |= tmpw >> (sz[0] * 8);
	}

	for (i = 0; i < loop; i++) {
		temp = off8 + (i << 3);
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_ADDR_LO, temp);
		temp = 0;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_ADDR_HI, temp);
		temp = word[i] & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_LO, temp);
		temp = (word[i] >> 32) & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_HI, temp);
		temp = MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_CTRL, temp);
		temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_CTRL, temp);

		for (j = 0; j< MAX_CTL_CHECK; j++) {
			temp = qla82xx_rd_32(ha, mem_crb + MIU_TEST_AGT_CTRL);
			if ((temp & MIU_TA_CTL_BUSY) == 0) {
				break;
			}
		}

		if (j >= MAX_CTL_CHECK) {
			printk("%s: Fail to write through agent\n", QLA2XXX_DRIVER_NAME);
			ret = -1;
			break;
		}
	}

	return ret;
}

int qla82xx_rdmem(struct qla_hw_data *ha, u64 off, void *data, int size)
{
	int i, j=0, k, start, end, loop, sz[2], off0[2];
	u32 temp;
	u64 off8, val, mem_crb, word[2] = {0,0};
#define MAX_CTL_CHECK   1000

	/*
	 * If not MN, go check for MS or invalid.
	 */

	if (off >= QLA82XX_ADDR_QDR_NET && off <= QLA82XX_P3_ADDR_QDR_NET_MAX) {
		mem_crb = QLA82XX_CRB_QDR_NET;
	} else {
		mem_crb = QLA82XX_CRB_DDR_NET;
		if (qla82xx_pci_mem_bound_check(ha, off, size) == 0)
			return(qla82xx_pci_mem_read_direct(ha, off, data, size));
	}

	off8 = off & 0xfffffff8;
	off0[0] = off & 0x7;
	off0[1] = 0;
	sz[0] = (size < (8 - off0[0])) ? size : (8 - off0[0]);
	sz[1] = size - sz[0];
	loop = ((off0[0] + size - 1) >> 3) + 1;

	for (i = 0; i < loop; i++) {
		temp = off8 + (i << 3);
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_ADDR_LO,
				temp);
		temp = 0;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_ADDR_HI,
				temp);
		temp = MIU_TA_CTL_ENABLE;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_CTRL,
				temp);
		temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_CTRL,
				temp);

		for (j = 0; j< MAX_CTL_CHECK; j++) {
			temp = qla82xx_rd_32(ha, mem_crb + MIU_TEST_AGT_CTRL);
			if ((temp & MIU_TA_CTL_BUSY) == 0) {
				break;
			}
		}

		if (j >= MAX_CTL_CHECK) {
			printk("%s: Fail to read through agent\n",QLA2XXX_DRIVER_NAME);
			break;
		}

		start = off0[i] >> 2;
		end   = (off0[i] + sz[i] - 1) >> 2;
		for (k = start; k <= end; k++) {
			temp = qla82xx_rd_32(ha, mem_crb + MIU_TEST_AGT_RDDATA(k));
			word[i] |= ((u64)temp << (32 * k));
		}
	}


	if (j >= MAX_CTL_CHECK)
		return -1;

	if (sz[0] == 8) {
		val = word[0];
	} else {
		val = ((word[0] >> (off0[0] * 8)) & (~(~0ULL << (sz[0] * 8)))) |
			((word[1] & (~(~0ULL << (sz[1] * 8)))) << (sz[0] * 8));
	}

	switch (size) {
		case 1:
			*(u8  *)data = val;
			break;
		case 2:
			*(u16 *)data = val;
			break;
		case 4:
			*(u32 *)data = val;
			break;
		case 8:
			*(u64 *)data = val;
			break;
	}

	return 0;
}

#define MTU_FUDGE_FACTOR 100

unsigned long qla82xx_decode_crb_addr(unsigned long addr)
{
	int i;
	unsigned long base_addr, offset, pci_base;

	if (!qla82xx_crb_table_initialized)
		qla82xx_crb_addr_transform_setup();

	pci_base = ADDR_ERROR;
	base_addr = addr & 0xfff00000;
	offset = addr & 0x000fffff;

	for (i=0; i< MAX_CRB_XFORM; i++) {
		if (crb_addr_xform[i] == base_addr) {
			pci_base = i << 20;
			break;
		}
	}
	if (pci_base == ADDR_ERROR) {
		return pci_base;
	} else {
		return (pci_base + offset);
	}
}

static long rom_max_timeout= 100;
//static long qla82xx_rom_lock_timeout= 10000; // changed for debugging purpose
static long qla82xx_rom_lock_timeout= 100;

int qla82xx_rom_lock(struct qla_hw_data *ha)
{
	int i;
	int done = 0, timeout = 0;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	while (!done) {
		/* acquire semaphore2 from PCI HW block */

		done = qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM2_LOCK));
		if (done == 1)
			break;
		if (timeout >= qla82xx_rom_lock_timeout) {
			printk("%s: scsi(%ld): Failed to acquire rom lock.\n",
				__func__, base_vha->host_no);
			return -1;
		}
		timeout++;

		/* Yield CPU */
		if(!in_interrupt())
			schedule();
		else {
			for(i = 0; i < 20; i++)
				cpu_relax(); /*This a nop instr on i386*/
		}
	}
	qla82xx_wr_32(ha, QLA82XX_ROM_LOCK_ID, ROM_LOCK_DRIVER);

	return 0;
}

void qla82xx_rom_unlock(struct qla_hw_data *ha)
{
	qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM2_UNLOCK));
}

int qla82xx_wait_rom_busy(struct qla_hw_data *ha)
{
	long timeout = 0;
	long done = 0 ;

	while (done == 0) {
		done = qla82xx_rd_32(ha, QLA82XX_ROMUSB_GLB_STATUS);
		done &= 4;
		timeout++;
		if (timeout >= rom_max_timeout) {
			qla_printk(KERN_WARNING, ha,
				"Timeout reached:  waiting for rom busy\n");
			return -1;
		}
	}
	return 0;
}

int qla82xx_wait_rom_done(struct qla_hw_data *ha)
{
	long timeout=0;
	long done=0 ;

	while (done == 0) {
		done = qla82xx_rd_32(ha, QLA82XX_ROMUSB_GLB_STATUS);
		done &=2;
		timeout++;
		if (timeout >= rom_max_timeout) {
			qla_printk(KERN_WARNING, ha,
				"Timeout reached:  waiting for rom done");
			return -1;
		}
	}

	return 0;
}

int qla82xx_do_rom_fast_read(struct qla_hw_data *ha, int addr, int *valp)
{
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ADDRESS, addr);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_DUMMY_BYTE_CNT, 0);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 3);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, 0xb);
	qla82xx_wait_rom_busy(ha);
	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		return -1;
	}
	/* reset abyte_cnt and dummy_byte_cnt */
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_DUMMY_BYTE_CNT, 0);
	udelay(10);
	cond_resched();
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 0);

	*valp = qla82xx_rd_32(ha, QLA82XX_ROMUSB_ROM_RDATA);

	return 0;
}

int qla82xx_rom_fast_read(struct qla_hw_data *ha, int addr, int *valp)
{
	int ret, loops = 0;

	while ((qla82xx_rom_lock(ha) != 0) && (loops < 50000)) {
		udelay(100);
		schedule();
		loops++;
	}
	if (loops >= 50000) {
		qla_printk(KERN_WARNING, ha, "qla82xx_rom_lock failed\n");
		return QLA_LOCK_TIMEOUT;
	}
	ret = qla82xx_do_rom_fast_read(ha, addr, valp);
	qla82xx_rom_unlock(ha);

	return ret;
}

int qla82xx_read_status_reg(struct qla_hw_data *ha, uint32_t *val)
{
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, M25P_INSTR_RDSR);
	qla82xx_wait_rom_busy(ha);
	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		return -1;
	}

	*val = qla82xx_rd_32(ha, QLA82XX_ROMUSB_ROM_RDATA);

	return 0;
}

int qla82xx_flash_wait_write_finish(struct qla_hw_data *ha)
{
	long timeout = 0;
	uint32_t done = 1 ;
	uint32_t val;
	int ret = 0;

	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 0);
	while ((done != 0) && (ret == 0)) {
		ret = qla82xx_read_status_reg(ha, &val);
		done = val & 1;
		timeout++;
		udelay(10);
		cond_resched();
		if (timeout >= 50000) {
			qla_printk(KERN_WARNING, ha,
				"Timeout reached  waiting for write finish");
			return -1;
		}
	}

	return ret;
}

int qla82xx_flash_set_write_enable(struct qla_hw_data *ha)
{
	uint32_t val;
	qla82xx_wait_rom_busy(ha);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 0);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, M25P_INSTR_WREN);
	qla82xx_wait_rom_busy(ha);
	if (qla82xx_wait_rom_done(ha))
		return -1;
	if (qla82xx_read_status_reg(ha, &val) != 0)
		return -1;
	if ((val & 2) != 2)
		return -1;

	return 0;
}

int qla82xx_write_status_reg(struct qla_hw_data *ha, uint32_t val)
{
	if (qla82xx_flash_set_write_enable(ha))
		return -1;

	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_WDATA, val);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, 0x1);
	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		return -1;
	}

	return qla82xx_flash_wait_write_finish(ha);
}

int qla82xx_write_disable_flash(struct qla_hw_data *ha)
{
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, M25P_INSTR_WRDI);
	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		return -1;
	}

	return 0;
}

int ql82xx_rom_lock_d(struct qla_hw_data *ha)
{
	int loops = 0;
	while ((qla82xx_rom_lock(ha) != 0) && (loops < 50000)) {
		udelay(100);
		cond_resched();
		loops++;
	}
	if (loops >= 50000) {
		qla_printk(KERN_WARNING, ha, "ROM lock failed\n");
		return QLA_LOCK_TIMEOUT;
	}
	return 0;
}

int qla82xx_write_flash_dword(scsi_qla_host_t *vha, uint32_t flashaddr,
		uint32_t data)
{
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;

	ret = ql82xx_rom_lock_d(ha);
	if (ret < 0) {
		printk("%s(%ld): ROM Lock failed\n", __func__, vha->host_no);
		return ret;
	}

	if (qla82xx_flash_set_write_enable(ha))
		goto done_write;

	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_WDATA, data);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ADDRESS, flashaddr);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 3);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, M25P_INSTR_PP);
	qla82xx_wait_rom_busy(ha);
	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		ret = -1;
		goto done_write;
	}

	ret = qla82xx_flash_wait_write_finish(ha);

done_write:
	qla82xx_rom_unlock(ha);
	return ret;
}

/*
 * Reset all block protect bits
 */
int qla82xx_pinit_from_rom(scsi_qla_host_t *base_vha, int verbose)
{
	int addr, val;
	int i, ret;
	int init_delay=0;
	struct crb_addr_pair *buf;
	unsigned long off;
	unsigned offset, n;
	struct qla_hw_data *ha = base_vha->hw;

	/* Grab the lock so that no one can read flash when we reset the chip. */
	ret = qla82xx_rom_lock(ha);
	if (ret) {
		qla_printk(KERN_INFO, ha,
			"scsi(%ld): (1)Rom lock failure in %s\n",
			base_vha->host_no, __func__);
		return ret;
	}

	/* disable all I2Q */
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x10, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x14, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x18, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x1c, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x20, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_I2Q + 0x24, 0x0);

	/* disable all niu interrupts */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0x40, 0xff);
	/* disable xge rx/tx */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0x70000, 0x00);
	/* disable xg1 rx/tx */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0x80000, 0x00);
	/* disable sideband mac */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0x90000, 0x00);
	/* disable ap0 mac */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0xa0000, 0x00);
	/* disable ap1 mac */
	qla82xx_wr_32(ha, QLA82XX_CRB_NIU + 0xb0000, 0x00);

	/* halt sre */
	val = qla82xx_rd_32(ha, QLA82XX_CRB_SRE + 0x1000);
	qla82xx_wr_32(ha, QLA82XX_CRB_SRE + 0x1000, val & (~(0x1)));

	/* halt epg */
	qla82xx_wr_32(ha, QLA82XX_CRB_EPG + 0x1300, 0x1);

	/* halt timers */
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x0, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x8, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x10, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x18, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x100, 0x0);
	qla82xx_wr_32(ha, QLA82XX_CRB_TIMER + 0x200, 0x0);

	/* halt pegs */
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_0 + 0x3c, 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_1 + 0x3c, 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_2 + 0x3c, 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_3 + 0x3c, 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_4 + 0x3c, 1);
	msleep(5);

	/* big hammer */
	if (test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags)) {
		/* don't reset CAM block on reset */
		qla82xx_wr_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET, 0xfeffffff);
	} else {
		qla82xx_wr_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET, 0xffffffff);
	}

	/* reset ms */
	val = qla82xx_rd_32(ha, QLA82XX_CRB_QDR_NET + 0xe4);
	val |= (1 << 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_QDR_NET + 0xe4, val);
	msleep(20);

	/* unreset ms */
	val = qla82xx_rd_32(ha, QLA82XX_CRB_QDR_NET + 0xe4);
	val &= ~(1 << 1);
	qla82xx_wr_32(ha, QLA82XX_CRB_QDR_NET + 0xe4, val);
	msleep(20);

	/* Just in case it was held when we reset the chip */
	qla82xx_rom_unlock(ha);

	/* Read the signature value from the flash.
	 * Offset 0: Contain signature (0xcafecafe)
	 * Offset 4: Offset and number of addr/value pairs
	 * that present in CRB initialize sequence
	 */
	ret = qla82xx_rom_fast_read(ha, 0, &n);
	if (ret) {
		if (ret == QLA_LOCK_TIMEOUT) {
			qla_printk(KERN_INFO, ha,
				"scsi(%ld):(3)Rom lock failure in %s\n",
				base_vha->host_no, __func__);
		} else {
			qla_printk(KERN_WARNING, ha,
				"[ERROR] Reading crb_init area: n: %08x\n", n);
		}
		return ret;
	}

	if (n != 0xcafecafeUL)
		return ret;

	ret = qla82xx_rom_fast_read(ha, 4, &n);
	if (ret) {
		if (ret == QLA_LOCK_TIMEOUT) {
			qla_printk(KERN_INFO, ha,
				"scsi(%ld):(4)Rom lock failure in %s\n",
				base_vha->host_no, __func__);
		} else {
			qla_printk(KERN_WARNING, ha,
				"[ERROR] Reading crb_init area: n: %08x\n", n);
		}
		return ret;
	}

	/* Offset in flash = lower 16 bits
	 * Number of enteries = upper 16 bits
	 */
	offset = n & 0xffffU;
	n = (n >> 16) & 0xffffU;

	/* number of addr/value pair should not exceed 1024 enteries */
	if (n  >= 1024) {
		qla_printk(KERN_WARNING, ha,
				"%s: %s:n=0x%x [ERROR] Card flash not initialized.\n",
				QLA2XXX_DRIVER_NAME,__FUNCTION__, n);
		return -1;
	}

	qla_printk(KERN_INFO, ha,
			"%s: %d CRB init values found in ROM.\n", QLA2XXX_DRIVER_NAME, n);

	buf = kmalloc(n*sizeof(struct crb_addr_pair), GFP_KERNEL);
	if (buf==NULL) {
		qla_printk(KERN_WARNING, ha,
				"%s: [ERROR] Unable to malloc memory.\n", QLA2XXX_DRIVER_NAME);
		return -1;
	}

	for (i = 0; i < n; i++) {
		ret = qla82xx_rom_fast_read(ha, 8*i + 4*offset, &val);
		if (ret) {
			if (ret == QLA_LOCK_TIMEOUT) {
				qla_printk(KERN_INFO, ha,
					"scsi(%ld): (3)Rom lock failure in %s\n",
					base_vha->host_no, __func__);
			} else {
				qla_printk(KERN_WARNING, ha,
					"[ERROR] Reading crb_init area: n: %08x\n", n);
			}

			kfree(buf);
			return ret;
		}
		ret = qla82xx_rom_fast_read(ha, 8*i + 4*offset + 4, &addr);
		if (ret) {
			if (ret == QLA_LOCK_TIMEOUT) {
				qla_printk(KERN_INFO, ha,
					"scsi(%ld): (3)Rom lock failure in %s\n",
					base_vha->host_no, __func__);
			} else {
				qla_printk(KERN_WARNING, ha,
					"[ERROR] Reading crb_init area: n: %08x\n", n);
			}
			kfree(buf);
			return ret;
		}

		buf[i].addr=addr;
		buf[i].data=val;
	}

	for (i = 0; i < n; i++) {
		/* Translate internal CRB initialization
		 * address to PCI bus address
		 */
		off = qla82xx_decode_crb_addr((unsigned long)buf[i].addr) +
			QLA82XX_PCI_CRBSPACE;
		/* skipping cold reboot MAGIC */
		if (off == QLA82XX_CAM_RAM(0x1fc))
			continue;

		/* do not reset PCI */
		if (off == (ROMUSB_GLB + 0xbc))
			continue;

		/* skip core clock, so that firmware can increase the clock */
		if (off == (ROMUSB_GLB + 0xc8))
			continue;

		/* skip the function enable register */
		if (off == QLA82XX_PCIE_REG(PCIE_SETUP_FUNCTION)) {
			continue;
		}
		if (off == QLA82XX_PCIE_REG(PCIE_SETUP_FUNCTION2)) {
			continue;
		}
		if ((off & 0x0ff00000) == QLA82XX_CRB_SMB) {
			continue;
		}

		if ((off & 0x0ff00000) == QLA82XX_CRB_DDR_NET) {
			continue;
		}

		if (off == ADDR_ERROR) {
			qla_printk(KERN_WARNING, ha,
				"%s: [ERROR] Unknown addr: 0x%08lx\n",
				QLA2XXX_DRIVER_NAME, buf[i].addr);
			continue;
		}

		/* After writing this register, HW needs time for CRB */
		/* to quiet down (else crb_window returns 0xffffffff) */
		if (off == QLA82XX_ROMUSB_GLB_SW_RESET) {
			init_delay=1;
		}

		qla82xx_wr_32(ha, off, buf[i].data);
		if (init_delay==1) {
			msleep(1000);
			init_delay=0;
		}

		msleep(1);
	}
	kfree(buf);

	// p2dn replyCount
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_D+0xec, 0x1e);
	// disable_peg_cache 0
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_D+0x4c,8);
	// disable_peg_cache 1
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_I+0x4c,8);

	// peg_clr_all
	// peg_clr 0
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_0+0x8,0);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_0+0xc,0);
	// peg_clr 1
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_1+0x8,0);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_1+0xc,0);
	// peg_clr 2
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_2+0x8,0);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_2+0xc,0);
	// peg_clr 3
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_3+0x8,0);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_3+0xc,0);

	return 0;
}

int qla82xx_pci_mem_read_2M(struct qla_hw_data *ha,
		u64 off, void *data, int size)
{
	int i, j = 0, k, start, end, loop, sz[2], off0[2];
	int shift_amount;
	uint32_t temp;
	uint64_t off8, val, mem_crb, word[2] = {0, 0};

	/*
	 * If not MN, go check for MS or invalid.
	 */

	if (off >= QLA82XX_ADDR_QDR_NET && off <= QLA82XX_P3_ADDR_QDR_NET_MAX)
		mem_crb = QLA82XX_CRB_QDR_NET;
	else {
		mem_crb = QLA82XX_CRB_DDR_NET;
		if (qla82xx_pci_mem_bound_check(ha, off, size) == 0)
			return qla82xx_pci_mem_read_direct(ha,
					off, data, size);
	}

	off8 = off & 0xfffffff0;
	off0[0] = off & 0xf;
	sz[0] = (size < (16 - off0[0])) ? size : (16 - off0[0]);
	shift_amount = 4;

	loop = ((off0[0] + size - 1) >> shift_amount) + 1;
	off0[1] = 0;
	sz[1] = size - sz[0];

	/*
	 * don't lock here - write_wx gets the lock if each time
	 * write_lock_irqsave(&adapter->adapter_lock, flags);
	 * qla82xx_pci_change_crbwindow_128M(adapter, 0);
	 */
	for (i = 0; i < loop; i++) {
		temp = off8 + (i << shift_amount);
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_ADDR_LO, temp);
		temp = 0;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_ADDR_HI, temp);
		temp = MIU_TA_CTL_ENABLE;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_CTRL, temp);
		temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE;
		qla82xx_wr_32(ha, mem_crb + MIU_TEST_AGT_CTRL, temp);

		for (j = 0; j < MAX_CTL_CHECK; j++) {
			temp = qla82xx_rd_32(ha, mem_crb + MIU_TEST_AGT_CTRL);
			if ((temp & MIU_TA_CTL_BUSY) == 0)
				break;
		}

		if (j >= MAX_CTL_CHECK) {
			if (printk_ratelimit())
				dev_err(&ha->pdev->dev,
						"failed to read through agent\n");
			break;
		}

		start = off0[i] >> 2;
		end   = (off0[i] + sz[i] - 1) >> 2;
		for (k = start; k <= end; k++) {
			temp = qla82xx_rd_32(ha,
					mem_crb + MIU_TEST_AGT_RDDATA(k));
			word[i] |= ((uint64_t)temp << (32 * (k & 1)));
		}
	}

	/*
	 * qla82xx_pci_change_crbwindow_128M(adapter, 1);
	 * write_unlock_irqrestore(&adapter->adapter_lock, flags);
	 */

	if (j >= MAX_CTL_CHECK)
		return -1;

	if ((off0[0] & 7) == 0) {
		val = word[0];
	} else {
		val = ((word[0] >> (off0[0] * 8)) & (~(~0ULL << (sz[0] * 8)))) |
			((word[1] & (~(~0ULL << (sz[1] * 8)))) << (sz[0] * 8));
	}

	switch (size) {
		case 1:
			*(uint8_t  *)data = val;
			break;
		case 2:
			*(uint16_t *)data = val;
			break;
		case 4:
			*(uint32_t *)data = val;
			break;
		case 8:
			*(uint64_t *)data = val;
			break;
	}

	return 0;
}

int qla82xx_pci_mem_write_2M(struct qla_hw_data *ha, u64 off,
		void *data, int size)
{
	int i, j, ret = 0, loop, sz[2], off0;
	int scale, shift_amount, startword;
	uint32_t temp;
	uint64_t off8, mem_crb, tmpw, word[2] = {0, 0};

	/*
	 * If not MN, go check for MS or invalid.
	 */
	if (off >= QLA82XX_ADDR_QDR_NET && off <= QLA82XX_P3_ADDR_QDR_NET_MAX)
		mem_crb = QLA82XX_CRB_QDR_NET;
	else {
		mem_crb = QLA82XX_CRB_DDR_NET;
		if (qla82xx_pci_mem_bound_check(ha, off, size) == 0)
			return qla82xx_pci_mem_write_direct(ha,
					off, data, size);
	}

	off0 = off & 0x7;
	sz[0] = (size < (8 - off0)) ? size : (8 - off0);
	sz[1] = size - sz[0];

	off8 = off & 0xfffffff0;
	loop = (((off & 0xf) + size - 1) >> 4) + 1;
	shift_amount = 4;
	scale = 2;
	startword = (off & 0xf)/8;

	for (i = 0; i < loop; i++) {
		if (qla82xx_pci_mem_read_2M(ha, off8 + (i << shift_amount),
					&word[i * scale], 8))
			return -1;
	}

	switch (size) {
		case 1:
			tmpw = *((uint8_t *)data);
			break;
		case 2:
			tmpw = *((uint16_t *)data);
			break;
		case 4:
			tmpw = *((uint32_t *)data);
			break;
		case 8:
		default:
			tmpw = *((uint64_t *)data);
			break;
	}

	if (sz[0] == 8) {
		word[startword] = tmpw;
	} else {
		word[startword] &= ~((~(~0ULL << (sz[0] * 8))) << (off0 * 8));
		word[startword] |= tmpw << (off0 * 8);
	}

	if (sz[1] != 0) {
		word[startword+1] &= ~(~0ULL << (sz[1] * 8));
		word[startword+1] |= tmpw >> (sz[0] * 8);
	}

	for (i = 0; i < loop; i++) {
		temp = off8 + (i << shift_amount);
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_ADDR_LO, temp);
		temp = 0;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_ADDR_HI, temp);
		temp = word[i * scale] & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_LO, temp);
		temp = (word[i * scale] >> 32) & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_HI, temp);
		temp = word[i*scale + 1] & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_UPPER_LO, temp);
		temp = (word[i*scale + 1] >> 32) & 0xffffffff;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_WRDATA_UPPER_HI, temp);

		temp = MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_CTRL, temp);
		temp = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE | MIU_TA_CTL_WRITE;
		qla82xx_wr_32(ha, mem_crb+MIU_TEST_AGT_CTRL, temp);

		for (j = 0; j < MAX_CTL_CHECK; j++) {
			temp = qla82xx_rd_32(ha, mem_crb + MIU_TEST_AGT_CTRL);
			if ((temp & MIU_TA_CTL_BUSY) == 0)
				break;
		}

		if (j >= MAX_CTL_CHECK) {
			if (printk_ratelimit())
				dev_err(&ha->pdev->dev,
						"failed to write through agent\n");
			ret = -1;
			break;
		}
	}

	return ret;
}

static int qla82xx_rcvpeg_ready(struct qla_hw_data *ha)
{
	uint32_t state = 0;
	int loops = 0, err = 0;

	/* Window 1 call */
	read_lock(&ha->hw_lock);
	state = qla82xx_rd_32(ha, CRB_RCVPEG_STATE);
	read_unlock(&ha->hw_lock);

	while ((state != PHAN_PEG_RCV_INITIALIZED) && (loops < 30000)) {
		udelay(100);
		schedule();
		/* Window 1 call */

		read_lock(&ha->hw_lock);
		state = qla82xx_rd_32(ha, CRB_RCVPEG_STATE);
		read_unlock(&ha->hw_lock);

		loops++;
	}

	if (loops >= 30000) {
		DEBUG2(qla_printk(KERN_INFO, ha,
			"Receive Peg initialization not complete: 0x%x.\n", state));
		err = -EIO;
	}

	return err;
}

int qla82xx_load_from_flash(struct qla_hw_data *ha)
{
	int  i, ret;
	long size = 0;
	long flashaddr = BOOTLD_START, memaddr = BOOTLD_START;
	u64 data;
	u32 high, low;
	size = (IMAGE_START - BOOTLD_START)/8;

	for (i = 0; i < size; i++) {
		ret = qla82xx_rom_fast_read(ha, flashaddr, (int *)&low);
		if (ret) {
			if (ret == QLA_LOCK_TIMEOUT) {
				qla_printk(KERN_INFO, ha,
					"(1)Rom lock failure in %s\n", __func__);
			} else {
				qla_printk(KERN_WARNING, ha,
					"[ERROR] (1)Reading from flash\n");
			}
			return ret;
		}

		ret = qla82xx_rom_fast_read(ha, flashaddr + 4, (int *)&high);
		if (ret) {
			if (ret == QLA_LOCK_TIMEOUT) {
				qla_printk(KERN_INFO, ha,
					"(2)Rom lock failure in %s\n", __func__);
			} else {
				qla_printk(KERN_WARNING, ha,
					"[ERROR] (2)Reading from flash\n");
			}
			return ret;
		}

		data = ((u64)high << 32 ) | low ;
		qla82xx_pci_mem_write_2M(ha, memaddr, &data, 8);
		flashaddr += 8;
		memaddr   += 8;

		if(i%0x1000==0){
			msleep(1);
		}
	}

	udelay(100);

	read_lock(&ha->hw_lock);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_0 + 0x18, 0x1020);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET, 0x80001e);
	read_unlock(&ha->hw_lock);

	return 0;
}

static struct uri_table_desc *qla82xx_get_table_desc(const u8 *unirom,
		int section)
{
	uint32_t i;
	struct uri_table_desc *directory = (struct uri_table_desc *) &unirom[0];
	__le32 entries = cpu_to_le32(directory->num_entries);

	for (i = 0; i < entries; i++) {

		__le32 offset = cpu_to_le32(directory->findex) +
			(i * cpu_to_le32(directory->entry_size));
		__le32 tab_type = cpu_to_le32(*((u32 *)&unirom[offset] + 8));

		if (tab_type == section)
			return (struct uri_table_desc *) &unirom[offset];
	}

	return NULL;
}

static struct uri_data_desc *qla82xx_get_data_desc(struct qla_hw_data *ha,
		u32 section, u32 idx_offset)
{
	const u8 *unirom = ha->hablob->fw->data;
	int idx = cpu_to_le32(*((int *)&unirom[ha->file_prd_off] +
				idx_offset));
	struct uri_table_desc *tab_desc;
	__le32 offset;

	tab_desc = qla82xx_get_table_desc(unirom, section);

	if (tab_desc == NULL)
		return NULL;

	offset = cpu_to_le32(tab_desc->findex) +
		(cpu_to_le32(tab_desc->entry_size) * idx);

	return (struct uri_data_desc *)&unirom[offset];
}

static u8 *qla82xx_get_bootld_offset(struct qla_hw_data *ha)
{
	u32 offset = BOOTLD_START;

	if (ha->fw_type == QLA82XX_UNIFIED_ROMIMAGE)
		offset = cpu_to_le32((qla82xx_get_data_desc(ha,
						QLA82XX_URI_DIR_SECT_BOOTLD,
						QLA82XX_URI_BOOTLD_IDX_OFF))->findex);

	return (u8 *)&ha->hablob->fw->data[offset];
}

static __le32 qla82xx_get_fw_size(struct qla_hw_data *ha)
{
	if (ha->fw_type == QLA82XX_UNIFIED_ROMIMAGE)
		return cpu_to_le32((qla82xx_get_data_desc(ha,
						QLA82XX_URI_DIR_SECT_FW,
						QLA82XX_URI_FIRMWARE_IDX_OFF))->size);
	else
		return cpu_to_le32(*(u32 *)&ha->hablob->fw->data[FW_SIZE_OFFSET]);
}

static u8 *qla82xx_get_fw_offs(struct qla_hw_data *ha)
{
	u32 offset = MS_IMAGE_START;

	if (ha->fw_type == QLA82XX_UNIFIED_ROMIMAGE)
		offset = cpu_to_le32((qla82xx_get_data_desc(ha,
						QLA82XX_URI_DIR_SECT_FW,
						QLA82XX_URI_FIRMWARE_IDX_OFF))->findex);

	return (u8 *)&ha->hablob->fw->data[offset];
}

int qla82xx_load_from_blob(struct qla_hw_data *ha)
{
	u64 *ptr64;
	u32 i, flashaddr, size;
	__le64 data;

	size = (IMAGE_START - BOOTLD_START) / 8;

	ptr64 = (u64 *)qla82xx_get_bootld_offset(ha);
	flashaddr = BOOTLD_START;

	for (i = 0; i < size; i++) {
		data = cpu_to_le64(ptr64[i]);

		if (qla82xx_pci_mem_write_2M(ha, flashaddr, &data, 8))
			return -EIO;

		flashaddr += 8;
	}

	size = (__force u32)qla82xx_get_fw_size(ha) / 8;

	ptr64 = (u64 *)qla82xx_get_fw_offs(ha);
	flashaddr = IMAGE_START;

	for (i = 0; i < size; i++) {
		data = cpu_to_le64(ptr64[i]);

		if (qla82xx_pci_mem_write_2M(ha, flashaddr, &data, 8))
			return -EIO;

		flashaddr += 8;
	}
	udelay(100);

	/* Write magic number */
	qla82xx_wr_32(ha,QLA82XX_CAM_RAM(0x1fc), QLA82XX_BDINFO_MAGIC);

	read_lock(&ha->hw_lock);
	qla82xx_wr_32(ha, QLA82XX_CRB_PEG_NET_0 + 0x18, 0x1020);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET, 0x80001e);
	read_unlock(&ha->hw_lock);

	return 0;
}

static int qla82xx_set_product_offset(struct qla_hw_data *ha)
{
	struct uri_table_desc *ptab_desc;
	const uint8_t *unirom = ha->hablob->fw->data;
	uint32_t i;
	__le32 entries;

	ptab_desc = qla82xx_get_table_desc(unirom,
			QLA82XX_URI_DIR_SECT_PRODUCT_TBL);
	if (ptab_desc == NULL)
		return -1;

	entries = cpu_to_le32(ptab_desc->num_entries);

	for (i = 0; i < entries; i++) {

		__le32 flags, file_chiprev, offset;
		uint8_t chiprev = ha->chip_revision;
		uint32_t flagbit = 2;

		offset = cpu_to_le32(ptab_desc->findex) +
			(i * cpu_to_le32(ptab_desc->entry_size));
		flags = cpu_to_le32(*((int *)&unirom[offset] + QLA82XX_URI_FLAGS_OFF));
		file_chiprev = cpu_to_le32(*((int *)&unirom[offset] +
					QLA82XX_URI_CHIP_REV_OFF));

		if ((chiprev == file_chiprev) && ((1ULL << flagbit) & flags)) {
			ha->file_prd_off = offset;
			return 0;
		}
	}

	return -1;
}

static __le32 qla82xx_get_fw_version(struct qla_hw_data *ha)
{
	const struct firmware *fw = ha->hablob->fw;

	if (ha->fw_type == QLA82XX_UNIFIED_ROMIMAGE) {
		struct uri_data_desc *fw_data_desc;
		__le32 major, minor, sub;
		const u8 *ver_str;
		int i, ret = 0;

		fw_data_desc = qla82xx_get_data_desc(ha,
				QLA82XX_URI_DIR_SECT_FW, QLA82XX_URI_FIRMWARE_IDX_OFF);
		ver_str = fw->data + cpu_to_le32(fw_data_desc->findex) +
			cpu_to_le32(fw_data_desc->size) - 17;

		for (i = 0; i < 12; i++) {
			if (!strncmp(&ver_str[i], "REV=", 4)) {
				ret = sscanf(&ver_str[i+4], "%u.%u.%u ",
						&major, &minor, &sub);
				break;
			}
		}
		DEBUG2(qla_printk(KERN_INFO, ha, "%s: URI: F/W version major=0x%x "
			"minor=0x%x sub=0x%x\n", __func__, major, minor, sub));

		if (ret != 3)
			return 0;

		return (major + (minor << 8) + (sub << 16));

	} else
		return cpu_to_le32(*(u32 *)&fw->data[FW_VERSION_OFFSET]);
}

int qla82xx_validate_firmware_blob(struct qla_hw_data *ha, uint8_t fw_type)
{
	__le32 val;
	uint32_t min_size;
	//uint32_t ver, bios;
	const struct firmware *fw = ha->hablob->fw;

	ha->fw_type = fw_type;

	if (fw_type == QLA82XX_UNIFIED_ROMIMAGE) {
		if (qla82xx_set_product_offset(ha))
			return -EINVAL;

		min_size = QLA82XX_URI_FW_MIN_SIZE;
	} else {
		val = cpu_to_le32(*(u32 *)&fw->data[QLA82XX_FW_MAGIC_OFFSET]);
		if ((__force u32)val != QLA82XX_BDINFO_MAGIC)
			return -EINVAL;

		min_size = QLA82XX_FW_MIN_SIZE;
	}

	if (fw->size < min_size)
		return -EINVAL;

	val = qla82xx_get_fw_version(ha);
	DEBUG2(qla_printk(KERN_INFO, ha,"Firmware version (blob) = 0x%x\n", val));

	/* TODO Do we need f/w version check (flash vs file f/w) ? */
#if 0
	val = qla82xx_get_bios_version(ha);
	qla82xx_rom_fast_read(ha, QLA82XX_BIOS_VERSION_OFFSET, (int *)&bios);
	if ((__force u32)val != bios) {
		DEBUG2(printk("firmware (ver %d) bios (ver %d) is incompatible\n",
				val, bios));
		return -EINVAL;
	}

	/* check if flashed firmware is newer */
	if (qla82xx_rom_fast_read(ha, QLA82XX_FW_VERSION_OFFSET, (int *)&val))
		return -EIO;
	if (val > ver) {
		DEBUG2(qla_printk(KERN_INFO, ha,"firmware (ver %d) is older than flash"
				" (ver %d)\n", val, ver));
		return -EINVAL;
	}

	qla82xx_wr_32(ha, QLA82XX_CAM_RAM(0x1fc), QLA82XX_BDINFO_MAGIC);
#endif

	return 0;
}

int qla82xx_load_fw(scsi_qla_host_t *base_vha, int fw_load)
{
	u32 rst;
	struct fw_blob *blob;
	struct qla_hw_data *ha = base_vha->hw;
	int ret = QLA_SUCCESS;

	qla82xx_wr_32(ha, CRB_CMDPEG_STATE, 0);
	ret = qla82xx_pinit_from_rom(base_vha, 0);
	if (ret != QLA_SUCCESS) {
		printk("%s: Error during CRB Initialization\n", __func__);
		return ret;
	}

	udelay(500);

	/* at this point, QM is in reset. This could be a problem if there are
	 * incoming d* transition queue messages. QM/PCIE could wedge.
	 * To get around this, QM is brought out of reset.
	 */

	rst = qla82xx_rd_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET);
	/* unreset qm */
	rst &= ~(1 << 28);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_GLB_SW_RESET, rst);

	if (fw_load) {
		printk("%s: Attempt to load fw from flash..\n", __func__);
		ret = qla82xx_load_from_flash(ha);
		if (ret != QLA_SUCCESS) {
			printk("%s: Error trying to load fw from flash!\n", __func__);
			return ret;
		}
	} else {
		/* Load firmware blob. */
		blob = ha->hablob = qla2x00_request_firmware(base_vha);
		if (!blob) {
			qla_printk(KERN_ERR, ha, "Firmware image not present.\n");
			return QLA_FUNCTION_FAILED;
		}

		/* Validating firmware blob */
		if (qla82xx_validate_firmware_blob(ha, QLA82XX_FLASH_ROMIMAGE)) {
			/* Fallback to URI format */
			if (qla82xx_validate_firmware_blob(ha, QLA82XX_UNIFIED_ROMIMAGE)) {
				qla_printk(KERN_ERR, ha, "No valid firmware image found!!!");
				return QLA_FUNCTION_FAILED;
			}
		}

		printk("%s Attempt to load fw from file..\n", __func__);
		if (qla82xx_load_from_blob(ha)) {
			printk("%s: Error trying to load fw from file!\n", __func__);
			return QLA_FUNCTION_FAILED;
		}
	}

	return QLA_SUCCESS;
}

int qla82xx_phantom_init(struct qla_hw_data *ha, int pegtune_val)
{
	u32 val = 0;
	int retries = 60;

	if (!pegtune_val) {
		do {
			val = qla82xx_rd_32(ha, CRB_CMDPEG_STATE);
			if ((val == PHAN_INITIALIZE_COMPLETE) ||
					(val == PHAN_INITIALIZE_ACK))
				return 0;
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(500);

		} while (--retries);

		if (!retries) {
			pegtune_val = qla82xx_rd_32(ha, QLA82XX_ROMUSB_GLB_PEGTUNE_DONE);
			printk(KERN_WARNING "%s: init failed, "
					"pegtune_val = %x\n", __FUNCTION__, pegtune_val);
			return -1;
		}
	}

	return 0;
}

static int qla82xx_start_firmware(scsi_qla_host_t *base_vha, int fw_load)
{
	int           pcie_cap;
	uint16_t      lnk;
	struct qla_hw_data *ha = base_vha->hw;
	int ret = QLA_SUCCESS;

	/* scrub dma mask expansion register */
	qla82xx_wr_32(ha, CRB_DMA_SHIFT, 0x55555555);

	/* Overwrite stale initialization register values */
	qla82xx_wr_32(ha, CRB_CMDPEG_STATE, 0);
	qla82xx_wr_32(ha, CRB_RCVPEG_STATE, 0);
	qla82xx_wr_32(ha, QLA82XX_PEG_HALT_STATUS1, 0);
	qla82xx_wr_32(ha, QLA82XX_PEG_HALT_STATUS2, 0);

	ret = qla82xx_load_fw(base_vha, fw_load);
	if (ret != QLA_SUCCESS) {
		printk("%s: Error trying to start fw!\n", __func__);
		return ret;
	}

	/* Handshake with the card before we register the devices. */
	ret = qla82xx_phantom_init(ha, 0);
	if (ret != QLA_SUCCESS) {
		printk("%s: Error during card handshake!\n", __func__);
		return ret;
	}

	/* Negotiated Link width */
	pcie_cap = pci_find_capability(ha->pdev, PCI_CAP_ID_EXP);
	pci_read_config_word(ha->pdev, pcie_cap + PCI_EXP_LNKSTA, &lnk);
	ha->link_width = (lnk >> 4) & 0x3f;

	/* Synchronize with Receive peg */
	return (qla82xx_rcvpeg_ready(ha));
}

static int qla82xx_try_start_fw(scsi_qla_host_t *base_vha)
{
	int	rval;

	/*
	 * FW Load priority:
	 * 1) Operational firmware residing in flash.
	 * 2) Firmware via driver blob (.c file).
	 */
	rval = qla82xx_start_firmware(base_vha, ql2xloadfwbin);
	if (rval != QLA_SUCCESS) {
		rval = qla82xx_start_firmware(base_vha, !ql2xloadfwbin);
		if (rval != QLA_SUCCESS)
			qla_printk(KERN_ERR, base_vha->hw,
				"FW: Please update operational firmware...\n");
	}

	return rval;
}

void qla82xx_rom_lock_recovery(struct qla_hw_data *ha)
{
	if(qla82xx_rom_lock(ha)) {
		/* Someone else is holding the lock. */
		dev_info(&ha->pdev->dev,"Resetting rom_lock\n");
	}

	/*
	 * Either we got the lock, or someone else died while holding it.
	 * In either case, unlock.
	 */
	qla82xx_rom_unlock(ha);
}

static inline void qla82xx_set_drv_active(struct qla_hw_data *ha)
{
	uint32_t drv_active;

	qla82xx_idc_lock(ha);
	drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
	drv_active |= (1 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_ACTIVE, drv_active);
	qla82xx_idc_unlock(ha);
}

void qla82xx_clear_drv_active(struct qla_hw_data *ha)
{
	uint32_t drv_active;

	drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
	drv_active &= ~(1 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_ACTIVE, drv_active);
}

/* Minidump related functions */
/*
 * Read CRB operation.
 */
static void
qla82xx_minidump_process_rdcrb(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t r_addr, r_stride, loop_cnt, i, r_value;
	qla82xx_minidump_entry_crb_t *crb_hdr;
	uint32_t *data_ptr = *d_ptr;

	crb_hdr = (qla82xx_minidump_entry_crb_t *)entry_hdr;
	r_addr = crb_hdr->addr;
	r_stride = crb_hdr->crb_strd.addr_stride;
	loop_cnt = crb_hdr->op_count;

	for (i = 0; i < loop_cnt; i++) {
		r_value = qla82xx_md_rw_32(ha, r_addr, 0, 0);
		*data_ptr++ = cpu_to_le32(r_addr);
		*data_ptr++ = cpu_to_le32(r_value);
		r_addr += r_stride;
	}
	*d_ptr = data_ptr;
}

static int
qla82xx_minidump_process_l2tag(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t addr, r_addr, c_addr, t_r_addr;
	uint32_t i, k, loop_count, t_value, r_cnt, r_value;
	unsigned long p_wait, w_time, p_mask;
	volatile uint32_t c_value_w, c_value_r;
	qla82xx_minidump_entry_cache_t *cache_hdr;
	uint32_t *data_ptr = *d_ptr;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	cache_hdr = (qla82xx_minidump_entry_cache_t *)entry_hdr;

	loop_count = cache_hdr->op_count;
	r_addr = cache_hdr->read_addr;
	c_addr = cache_hdr->control_addr;
	c_value_w = cache_hdr->cache_ctrl.write_value;

	t_r_addr = cache_hdr->tag_reg_addr;
	t_value = cache_hdr->addr_ctrl.init_tag_value;
	r_cnt = cache_hdr->read_ctrl.read_addr_cnt;
	p_wait = cache_hdr->cache_ctrl.poll_wait;
	p_mask = cache_hdr->cache_ctrl.poll_mask;

	for (i = 0; i < loop_count; i++) {
		qla82xx_md_rw_32(ha, t_r_addr, t_value, 1);
		if (c_value_w)
			qla82xx_md_rw_32(ha, c_addr, c_value_w, 1);

		if (p_mask) {
			w_time = jiffies + p_wait;

			do {
				c_value_r = qla82xx_md_rw_32(ha, c_addr, 0, 0);
				if ((c_value_r & p_mask) == 0)
					break;
				else if (time_after_eq(jiffies, w_time)) {
					/* capturing dump failed */
					DEBUG2(qla_printk(KERN_ERR, ha,
						"%s(%ld): c_value_r=0x%x, poll_mask=0x%lx, "
						"w_time=0x%lx\n", __func__, base_vha->host_no,
						c_value_r, p_mask, w_time));
					return QLA_FUNCTION_FAILED;
				}
			} while (1);
		}

		addr = r_addr;
		for (k = 0; k < r_cnt; k++) {
			r_value = qla82xx_md_rw_32(ha, addr, 0, 0);
			*data_ptr++ = cpu_to_le32(r_value);
			addr += cache_hdr->read_ctrl.read_addr_stride;
		}

		t_value += cache_hdr->addr_ctrl.tag_value_stride;
	}
	*d_ptr = data_ptr;

	return QLA_SUCCESS;
}

static void
qla82xx_minidump_process_l1cache(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t addr, r_addr, c_addr, t_r_addr;
	uint32_t i, k, loop_count, t_value, r_cnt, r_value;
	volatile uint32_t c_value_w;
	qla82xx_minidump_entry_cache_t *cache_hdr;
	uint32_t *data_ptr = *d_ptr;

	cache_hdr = (qla82xx_minidump_entry_cache_t *)entry_hdr;

	loop_count = cache_hdr->op_count;
	r_addr = cache_hdr->read_addr;
	c_addr = cache_hdr->control_addr;
	c_value_w = cache_hdr->cache_ctrl.write_value;

	t_r_addr = cache_hdr->tag_reg_addr;
	t_value = cache_hdr->addr_ctrl.init_tag_value;
	r_cnt = cache_hdr->read_ctrl.read_addr_cnt;

	for (i = 0; i < loop_count; i++) {
		qla82xx_md_rw_32(ha, t_r_addr, t_value, 1);
		qla82xx_md_rw_32(ha, c_addr, c_value_w, 1);

		addr = r_addr;
		for (k = 0; k < r_cnt; k++) {
			r_value = qla82xx_md_rw_32(ha, addr, 0, 0);
			*data_ptr++ = cpu_to_le32(r_value);
			addr += cache_hdr->read_ctrl.read_addr_stride;
		}
		t_value += cache_hdr->addr_ctrl.tag_value_stride;
	}
	*d_ptr = data_ptr;
}

/*
 * Handling control entries.
 */
static int
qla82xx_minidump_process_control(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	qla82xx_minidump_entry_crb_t *crb_entry;
	uint32_t read_value, opcode, poll_time, addr, index, crb_addr;
	uint32_t rval = 0, i = 0;
	unsigned long wtime;
	qla82xx_minidump_template_hdr_t *tmplt_hdr;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	tmplt_hdr = (qla82xx_minidump_template_hdr_t*)ha->md_template_hdr;
	crb_entry = (qla82xx_minidump_entry_crb_t*)entry_hdr;
	crb_addr = crb_entry->addr;

	for (i = 0; i < crb_entry->op_count; i++) {
		opcode = crb_entry->crb_ctrl.opcode;
		if (opcode & QLA82XX_DBG_OPCODE_WR) {
			qla82xx_md_rw_32(ha, crb_addr, crb_entry->value_1, 1);
			opcode &= ~QLA82XX_DBG_OPCODE_WR;
		}

		if (opcode & QLA82XX_DBG_OPCODE_RW) {
			read_value = qla82xx_md_rw_32(ha, crb_addr, 0, 0);
			qla82xx_md_rw_32(ha, crb_addr, read_value, 1);
			opcode &= ~QLA82XX_DBG_OPCODE_RW;
		}

		if (opcode & QLA82XX_DBG_OPCODE_AND) {
			read_value = qla82xx_md_rw_32(ha, crb_addr, 0, 0);
			read_value &= crb_entry->value_2;
			opcode &= ~QLA82XX_DBG_OPCODE_AND;
			if (opcode & QLA82XX_DBG_OPCODE_OR) {
				read_value |= crb_entry->value_3;
				opcode &= ~QLA82XX_DBG_OPCODE_OR;
			}
			qla82xx_md_rw_32(ha, crb_addr, read_value, 1);
		}

		if (opcode & QLA82XX_DBG_OPCODE_OR) {
			read_value = qla82xx_md_rw_32(ha, crb_addr, 0, 0);
			read_value |= crb_entry->value_3;
			qla82xx_md_rw_32(ha, crb_addr, read_value, 1);
			opcode &= ~QLA82XX_DBG_OPCODE_OR;
		}

		if (opcode & QLA82XX_DBG_OPCODE_POLL) {
			poll_time = crb_entry->crb_strd.poll_timeout;
			wtime = jiffies + poll_time;
			read_value = qla82xx_md_rw_32(ha, crb_addr, 0, 0);

			do {
				if ((read_value & crb_entry->value_2) == crb_entry->value_1)
					break;
				else if (time_after_eq(jiffies, wtime)) {
					/* capturing dump failed */
					DEBUG2(qla_printk(KERN_ERR, ha,
						"%s(%ld): read_value=0x%x, value_2=0x%x, "
						"value_1=0x%x\n", __func__, base_vha->host_no,
						read_value, crb_entry->value_2, crb_entry->value_1));
					rval = -1;
					break;
				} else
					read_value = qla82xx_md_rw_32(ha, crb_addr, 0, 0);
			} while (1);
			opcode &= ~QLA82XX_DBG_OPCODE_POLL;
		}

		if (opcode & QLA82XX_DBG_OPCODE_RDSTATE) {
			if (crb_entry->crb_strd.state_index_a) {
				index = crb_entry->crb_strd.state_index_a;
				addr = tmplt_hdr->saved_state_array[index];
			}
			else
				addr = crb_addr;

			read_value = qla82xx_md_rw_32(ha, addr, 0, 0);
			index = crb_entry->crb_ctrl.state_index_v;
			tmplt_hdr->saved_state_array[index] = read_value;
			opcode &= ~QLA82XX_DBG_OPCODE_RDSTATE;
		}

		if (opcode & QLA82XX_DBG_OPCODE_WRSTATE) {
			if (crb_entry->crb_strd.state_index_a) {
				index = crb_entry->crb_strd.state_index_a;
				addr = tmplt_hdr->saved_state_array[index];
			}
			else
				addr = crb_addr;

			if (crb_entry->crb_ctrl.state_index_v) {
				index = crb_entry->crb_ctrl.state_index_v;
				read_value = tmplt_hdr->saved_state_array[index];
			} else
				read_value = crb_entry->value_1;

			qla82xx_md_rw_32(ha, addr, read_value, 1);
			opcode &= ~QLA82XX_DBG_OPCODE_WRSTATE;
		}

		if (opcode & QLA82XX_DBG_OPCODE_MDSTATE) {
			index = crb_entry->crb_ctrl.state_index_v;
			read_value = tmplt_hdr->saved_state_array[index];
			read_value <<= crb_entry->crb_ctrl.shl;
			read_value >>= crb_entry->crb_ctrl.shr;
			if (crb_entry->value_2)
				read_value &= crb_entry->value_2;
			read_value |= crb_entry->value_3;
			read_value += crb_entry->value_1;
			tmplt_hdr->saved_state_array[index] = read_value;
			opcode &= ~QLA82XX_DBG_OPCODE_MDSTATE;
		}

		crb_addr += crb_entry->crb_strd.addr_stride;
	}

	return (rval);
}

/*
 * Reading OCM memory
 */
static void
qla82xx_minidump_process_rdocm(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t r_addr, r_stride, loop_cnt, i, r_value;
	qla82xx_minidump_entry_rdocm_t *ocm_hdr;
	uint32_t *data_ptr = *d_ptr;

	ocm_hdr = (qla82xx_minidump_entry_rdocm_t *)entry_hdr;
	r_addr = ocm_hdr->read_addr;
	r_stride = ocm_hdr->read_addr_stride;
	loop_cnt = ocm_hdr->op_count;

	for ( i = 0; i < loop_cnt; i++) {
		r_value = readl((void *)(r_addr + ha->nx_pcibase));
		*data_ptr++ = cpu_to_le32(r_value);
		r_addr += r_stride;
	}
	*d_ptr = data_ptr;
}


/*
 * Read MUX data
 */
static void
qla82xx_minidump_process_rdmux(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t r_addr, s_stride, s_addr, s_value, loop_cnt, i, r_value;
	qla82xx_minidump_entry_mux_t *mux_hdr;
	uint32_t *data_ptr = *d_ptr;

	mux_hdr = (qla82xx_minidump_entry_mux_t *)entry_hdr;
	r_addr = mux_hdr->read_addr;
	s_addr = mux_hdr->select_addr;
	s_stride = mux_hdr->select_value_stride;
	s_value = mux_hdr->select_value;
	loop_cnt = mux_hdr->op_count;

	for ( i = 0; i < loop_cnt; i++) {
		qla82xx_md_rw_32(ha, s_addr, s_value, 1);
		r_value = qla82xx_md_rw_32(ha, r_addr, 0, 0);
		*data_ptr++ = cpu_to_le32(s_value);
		*data_ptr++ = cpu_to_le32(r_value);
		s_value += s_stride;
	}
	*d_ptr = data_ptr;
}

/*
 * Handling Queue State Reads.
 */
static void
qla82xx_minidump_process_queue(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t s_addr, r_addr;
	uint32_t r_stride, r_value, r_cnt, qid = 0;
	uint32_t i, k, loop_cnt;
	qla82xx_minidump_entry_queue_t *q_hdr;
	uint32_t *data_ptr = *d_ptr;

	q_hdr = (qla82xx_minidump_entry_queue_t *)entry_hdr;
	s_addr = q_hdr->select_addr;
	r_cnt = q_hdr->rd_strd.read_addr_cnt;
	r_stride = q_hdr->rd_strd.read_addr_stride;
	loop_cnt = q_hdr->op_count;

	for ( i = 0; i < loop_cnt; i++) {
		qla82xx_md_rw_32(ha, s_addr, qid, 1);
		r_addr = q_hdr->read_addr;
		for ( k = 0; k < r_cnt; k++) {
			r_value = qla82xx_md_rw_32(ha, r_addr, 0, 0);
			*data_ptr++ = cpu_to_le32(r_value);
			r_addr += r_stride;
		}
		qid += q_hdr->q_strd.queue_id_stride;
	}
	*d_ptr = data_ptr;
}

uint32_t qla82xx_validate_template_chksum(struct qla_hw_data *ha)
{
	uint64_t chksum = 0;
	uint32_t *d_ptr = (uint32_t*)ha->md_template_hdr;
	int count = ha->md_template_size/sizeof(uint32_t);

	while (count-- > 0)
		chksum += *d_ptr++;
	while (chksum >> 32)
		chksum = (chksum & 0xFFFFFFFF) + (chksum >> 32);

	DEBUG2_11(qla_printk(KERN_INFO, ha,
		"Checksum of template header: 0x%llx\n", (unsigned long long)chksum));

	return ~chksum;
}

#define MD_DIRECT_ROM_WINDOW            0x42110030
#define MD_DIRECT_ROM_READ_BASE         0x42150000
static void
qla82xx_minidump_process_rdrom(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t r_addr, r_value;
	uint32_t i, loop_cnt;
	qla82xx_minidump_entry_rdrom_t *rom_hdr;
	uint32_t *data_ptr = *d_ptr;

	rom_hdr = (qla82xx_minidump_entry_rdrom_t *)entry_hdr;
	r_addr = rom_hdr->read_addr;
	loop_cnt = rom_hdr->read_data_size/sizeof(uint32_t);

	for (i = 0; i < loop_cnt; i++) {
		qla82xx_md_rw_32(ha, MD_DIRECT_ROM_WINDOW, (r_addr & 0xFFFF0000), 1);
		r_value = qla82xx_md_rw_32(ha,
			MD_DIRECT_ROM_READ_BASE + (r_addr & 0x0000FFFF), 0, 0);
		*data_ptr++ = cpu_to_le32(r_value);
		r_addr += sizeof(uint32_t);
	}
	*d_ptr = data_ptr;
}

#define MD_MIU_TEST_AGT_CTRL		0x41000090
#define MD_MIU_TEST_AGT_ADDR_LO		0x41000094
#define MD_MIU_TEST_AGT_ADDR_HI		0x41000098

static int
qla82xx_minidump_process_rdmem(struct qla_hw_data *ha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, uint32_t **d_ptr)
{
	uint32_t r_addr, r_value;
	uint32_t i, j, loop_cnt;
	qla82xx_minidump_entry_rdmem_t *m_hdr;
	uint32_t r_data;
	unsigned long flags;
	int lcount = 0;
	uint32_t *data_ptr = *d_ptr;

	m_hdr = (qla82xx_minidump_entry_rdmem_t *)entry_hdr;
	r_addr = m_hdr->read_addr;
	loop_cnt = m_hdr->read_data_size/16;

	if (r_addr & 0xf) {
		DEBUG2(qla_printk(KERN_WARNING, ha,
			"[%s]: Read addr 0x%x not 16 bytes alligned\n",
			__func__, r_addr));
		return QLA_FUNCTION_FAILED;
	}

	if (m_hdr->read_data_size % 16) {
		DEBUG2(qla_printk(KERN_WARNING, ha,
			"[%s]: Read data[0x%x] not multiple of 16 bytes\n",
			__func__, m_hdr->read_data_size));
		return QLA_FUNCTION_FAILED;
	}

	DEBUG11(qla_printk(KERN_INFO, ha,
		"[%s]: rdmem_addr: 0x%x, read_data_size: 0x%x, loop_cnt: 0x%x\n",
		__func__, r_addr, m_hdr->read_data_size, loop_cnt));

	write_lock_irqsave(&ha->hw_lock, flags);
	for (i = 0; i < loop_cnt; i++) {
		qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_ADDR_LO, r_addr, 1);
		r_value = 0;
		qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_ADDR_HI, r_value, 1);
		r_value = MIU_TA_CTL_ENABLE;
		qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_CTRL, r_value, 1);
		r_value = MIU_TA_CTL_START | MIU_TA_CTL_ENABLE;
		qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_CTRL, r_value, 1);

		for (j = 0; j < MAX_CTL_CHECK; j++) {
			r_value = qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_CTRL, 0, 0);
			if ((r_value & MIU_TA_CTL_BUSY) == 0)
				break;
		}

		if (j >= MAX_CTL_CHECK) {
			if (printk_ratelimit())
				dev_err(&ha->pdev->dev, "failed to read through agent\n");
			write_unlock_irqrestore(&ha->hw_lock, flags);
			return QLA_FUNCTION_FAILED;
		}

		for (j = 0; j < 4; j++) {
			r_data = qla82xx_md_rw_32(ha, MD_MIU_TEST_AGT_RDDATA[j], 0, 0);
			*data_ptr++ = cpu_to_le32(r_data);
			if (r_data && (lcount < 32)) {
				lcount++;
			}
		}

		r_addr += 16;
	}
	write_unlock_irqrestore(&ha->hw_lock, flags);
	*d_ptr = data_ptr;

	return QLA_SUCCESS;
}

static void
qla82xx_mark_entry_skipped(scsi_qla_host_t *vha,
	qla82xx_minidump_entry_hdr_t *entry_hdr, int index)
{
	struct qla_hw_data *ha = vha->hw;

	entry_hdr->d_ctrl.driver_flags |= QLA82XX_DBG_SKIPPED_FLAG;
	DEBUG2(qla_printk(KERN_WARNING, ha,
		"scsi(%ld): Skipping entry[%d]: ETYPE[0x%x]-ELEVEL[0x%x]\n",
		vha->host_no, index, entry_hdr->entry_type,
		entry_hdr->d_ctrl.entry_capture_mask));
}

/*
 * Processing the acquired minidump template.
 */
int
qla82xx_collect_md_data(scsi_qla_host_t *vha, int hardware_locked)
{
	int no_entry_hdr = 0;
	qla82xx_minidump_entry_hdr_t *entry_hdr;
	qla82xx_minidump_template_hdr_t *tmplt_hdr;
	uint32_t *data_ptr;
	uint32_t data_collected = 0;
	int i, rval = 0;
	struct qla_hw_data *ha = vha->hw;

	if (!ql2xenablemd) {
		DEBUG2(qla_printk(KERN_INFO, ha,
			"scsi(%ld): F/W Minidump capture disabled.\n", vha->host_no));
		return QLA_FUNCTION_FAILED;
	}

	if (ha->fw_dumped) {
		DEBUG2(qla_printk(KERN_INFO, ha,
			"scsi(%ld): F/W Minidump previously taken.\n", vha->host_no));
		return QLA_FUNCTION_FAILED;
	}

	if (!ha->fw_minidump || !ha->md_template_hdr) {
		DEBUG2(qla_printk(KERN_ERR, ha,
			"scsi(%ld): Minidump buffer not allocated.", vha->host_no));
		return QLA_FUNCTION_FAILED;
	}

	tmplt_hdr = (qla82xx_minidump_template_hdr_t *)ha->md_template_hdr;
	data_ptr = (uint32_t *)((uint8_t *)ha->fw_minidump);

	if (qla82xx_validate_template_chksum(ha)) {
		DEBUG2(qla_printk(KERN_ERR, ha,
			"[%s]: Template checksum validation error\n", __func__));
		return QLA_FUNCTION_FAILED;
	}

	no_entry_hdr = tmplt_hdr->num_of_entries;
	DEBUG2(qla_printk(KERN_INFO, ha,
		"Capture Mask obtained: 0x%x\n", tmplt_hdr->capture_debug_level));
	DEBUG2(qla_printk(KERN_INFO, ha,
		"Total_data_size 0x%x obtained\n", ha->fw_dump_size));
	DEBUG2(qla_printk(KERN_INFO, ha, "offset=0x%x start=%p final=%p \n",
		tmplt_hdr->first_entry_offset, ha->md_template_hdr,
		(((uint8_t*)ha->md_template_hdr) + tmplt_hdr->first_entry_offset)));

	/* Check whether template obtained is valid */
	if (tmplt_hdr->entry_type != QLA82XX_TLHDR) {
		DEBUG2(qla_printk(KERN_ERR, ha,
			"[%s]: Improper Template header entry type: "
			"0x%x obtained\n", __func__, tmplt_hdr->entry_type));
		return QLA_FUNCTION_FAILED;
	}

	tmplt_hdr->driver_timestamp = jiffies_to_msecs(jiffies);
	tmplt_hdr->driver_capture_mask = ha->capture_mask;
	tmplt_hdr->driver_info[0] = vha->host_no;
	tmplt_hdr->driver_info[1] = (QLA_DRIVER_MAJOR_VER << 24) |
		(QLA_DRIVER_MINOR_VER << 16) | (QLA_DRIVER_PATCH_VER << 8) |
		QLA_DRIVER_BETA_VER;

	entry_hdr = (qla82xx_minidump_entry_hdr_t*) \
			(((uint8_t*)ha->md_template_hdr) + tmplt_hdr->first_entry_offset);

	/* Walk through the entry headers - validate amd perform required action */
	for (i = 0; i < no_entry_hdr; i++) {

		if (data_collected > ha->fw_dump_size &&
				(entry_hdr->entry_type != QLA82XX_RDEND &&
				entry_hdr->entry_type != QLA82XX_CNTRL &&
				entry_hdr->entry_type != QLA82XX_RDNOP)) {
			DEBUG2(qla_printk(KERN_WARNING, ha,
				"Data collected: [0x%x], Total Dump size:[0x%x] "
				"nof_entry_hdr=%d cur_entry_hdr=%d\n",
				data_collected, ha->fw_dump_size, no_entry_hdr, i));
			return QLA_FUNCTION_FAILED;
		}

		if (!(entry_hdr->d_ctrl.entry_capture_mask & ha->capture_mask)) {
			qla82xx_mark_entry_skipped(vha, entry_hdr, i);
			goto skip_entry;
		}

		/* Decode the entry type and take required action to capture debug data */
		switch (entry_hdr->entry_type) {
			case QLA82XX_RDEND:
				qla82xx_mark_entry_skipped(vha, entry_hdr, i);
				break;

			case QLA82XX_CNTRL:
				rval = qla82xx_minidump_process_control
					(ha, entry_hdr, &data_ptr);
				if (rval) {
					qla82xx_mark_entry_skipped(vha, entry_hdr, i);
					return QLA_FUNCTION_FAILED;
				}
				break;

			case QLA82XX_RDCRB:
				qla82xx_minidump_process_rdcrb(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_RDMEM:
				rval = qla82xx_minidump_process_rdmem(ha, entry_hdr, &data_ptr);
				if (rval) {
					qla82xx_mark_entry_skipped(vha, entry_hdr, i);
					return QLA_FUNCTION_FAILED;
				}
				break;

			case QLA82XX_BOARD:
			case QLA82XX_RDROM:
				qla82xx_minidump_process_rdrom(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_L2DTG:
			case QLA82XX_L2ITG:
			case QLA82XX_L2DAT:
			case QLA82XX_L2INS:
				rval = qla82xx_minidump_process_l2tag(ha, entry_hdr, &data_ptr);
				if (rval) {
					qla82xx_mark_entry_skipped(vha, entry_hdr, i);
					return QLA_FUNCTION_FAILED;
				}
				break;

			case QLA82XX_L1DAT:
			case QLA82XX_L1INS:
				qla82xx_minidump_process_l1cache(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_RDOCM:
				qla82xx_minidump_process_rdocm(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_RDMUX:
				qla82xx_minidump_process_rdmux(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_QUEUE:
				qla82xx_minidump_process_queue(ha, entry_hdr, &data_ptr);
				break;

			case QLA82XX_RDNOP:
			default:
				qla82xx_mark_entry_skipped(vha, entry_hdr, i);
				break;
		}
		data_collected = (uint8_t *)data_ptr - (uint8_t *)ha->fw_minidump;
skip_entry:
		/*  next entry in the template */
		entry_hdr = (qla82xx_minidump_entry_hdr_t*)
				(((uint8_t*)entry_hdr) + entry_hdr->entry_size);
	}

	if (data_collected != ha->fw_dump_size) {
		DEBUG2(qla_printk(KERN_ERR, ha,
			"Dump data mismatch: Data collected: [0x%x], "
			"total_data_size:[0x%x]\n", data_collected, ha->fw_dump_size));
		return QLA_FUNCTION_FAILED;
	}

	ha->fw_dumped = 1;

	DEBUG3(qla2x00_dump_buffer(ha->fw_minidump, (14*16)));
	DEBUG2(qla_printk(KERN_INFO, ha,
		"Leaving %s Last entry: 0x%x\n", __func__, i));

	return 0;
}

void
qla82xx_collect_fw_dump(scsi_qla_host_t *vha, int hardware_locked)
{
	qla82xx_collect_md_data(vha, hardware_locked);
}

/*
 * qla82xx_device_bootstrap
 *    Initialize device, set DEV_READY, start fw
 *
 * Note:
 *	IDC lock must be held upon entry
 *
 * Return:
 *    Success : 0
 *    Failed  : 1
 */
static int qla82xx_device_bootstrap(scsi_qla_host_t *base_vha)
{
	int rval = QLA_SUCCESS, i, timeout;
	uint32_t old_count, count;
	int need_reset = 0, peg_stuck = 1;
	struct qla_hw_data *ha = base_vha->hw;

	need_reset = qla82xx_need_reset(ha);

	old_count = qla82xx_rd_32(ha, QLA82XX_PEG_ALIVE_COUNTER);

	for (i = 0; i < 10; i++) {
		timeout = msleep_interruptible(200);
		if (timeout) {
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_FAILED);
			return QLA_FUNCTION_FAILED;
		}

		count = qla82xx_rd_32(ha, QLA82XX_PEG_ALIVE_COUNTER);
		if (count != old_count)
			peg_stuck = 0;
	}

	if (peg_stuck) {
		/* Either we are the first or recovery in progress. */
		qla82xx_rom_lock_recovery(ha);
		goto dev_initialize;
	}

	if (!need_reset) {
		/* Firmware already running. */
		goto dev_ready;
	}

dev_initialize:
	/* set to DEV_INITIALIZING */
	qla_printk(KERN_INFO, ha, "HW State: INITIALIZING\n");
	qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_INITIALIZING);

	/* Dump the firmware if we are recovering from a reset */
	if (need_reset && ql2xallocfwdump && ha->flags.isp82xx_fw_hung) {
		qla82xx_idc_unlock(ha);
		if (qla82xx_collect_md_data(base_vha, 0) == QLA_SUCCESS)
			DEBUG2(qla_printk(KERN_INFO, ha,
				"scsi(%ld): F/W dumped (minidump format).\n",
				base_vha->host_no));
		qla82xx_idc_lock(ha);
	}

	/* Driver that sets device state to initializating sets IDC version */
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_IDC_VERSION, QLA82XX_IDC_VERSION);

	qla82xx_idc_unlock(ha);
	rval = qla82xx_try_start_fw(base_vha);
	qla82xx_idc_lock(ha);

	if (rval != QLA_SUCCESS) {
		if ( rval == QLA_LOCK_TIMEOUT) {
			if (ha->dev_init_retry_cnt == 5) {
				qla_printk(KERN_INFO, ha, "HW State: FAILED\n");
				qla82xx_clear_drv_active(ha);
				qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_FAILED);
				return rval;
			}
			qla_printk(KERN_INFO, ha,
				"scsi(%ld): Lock failure - Retry dev initialization (0x%x)\n",
				base_vha->host_no, ha->dev_init_retry_cnt);
			ha->dev_init_retry_cnt++;
			/* Unlock te rom and start again */
			qla82xx_rd_32(ha, QLA82XX_PCIE_REG(PCIE_SEM2_UNLOCK));
			goto dev_initialize;
		} else {
			qla_printk(KERN_INFO, ha, "HW State: FAILED\n");
			qla82xx_clear_drv_active(ha);
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_FAILED);
			return rval;
		}
	}

dev_ready:
	qla_printk(KERN_INFO, ha, "HW State: READY\n");
	qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_READY);

	ha->flags.isp82xx_reset_owner = 0;
	DEBUG2(qla_printk(KERN_INFO, ha, "%s(%ld): reset_owner reset by 0x%x\n",
		__func__, base_vha->host_no, ha->portnum));

	return QLA_SUCCESS;
}

inline int qla82xx_need_reset(struct qla_hw_data *ha)
{
	uint32_t drv_state;
	int rval;

	drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	rval = drv_state & (1 << (ha->portnum * 4));

	return rval;
}

inline void qla82xx_set_rst_ready(struct qla_hw_data *ha)
{
	uint32_t drv_state;

	drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	drv_state |= (1 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_STATE, drv_state);
}

void qla82xx_clear_rst_ready(struct qla_hw_data *ha)
{
	uint32_t drv_state;

	drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	drv_state &= ~(1 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_STATE, drv_state);
}

static inline void qla82xx_set_qsnt_ready(struct qla_hw_data *ha)
{
	uint32_t qsnt_state;

	qsnt_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	qsnt_state |= (2 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_STATE, qsnt_state);
}

void qla82xx_clear_qsnt_ready(struct qla_hw_data *ha)
{
	uint32_t qsnt_state;

	qsnt_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	qsnt_state &= ~(2 << (ha->portnum * 4));
	qla82xx_wr_32(ha, QLA82XX_CRB_DRV_STATE, qsnt_state);
}

void qla82xx_dev_failed_handler(scsi_qla_host_t *base_vha)
{
	struct qla_hw_data *ha = base_vha->hw;
	scsi_qla_host_t *vha, *tmp_vha;

	base_vha->flags.online = 0;
	base_vha->flags.init_done = 0;

	/* Disable the board */
	qla_printk(KERN_INFO, ha, "Disabling the board\n");
	/* Set DEV_FAILED flag to disable timer */
	base_vha->device_flags |= DFLG_DEV_FAILED;

	qla2x00_mark_all_devices_lost(base_vha, 0);

	/* Cleanup vport targets. */
	list_for_each_entry_safe(vha, tmp_vha, &ha->vp_list, list) {
		vha->device_flags |= DFLG_DEV_FAILED;
		qla2x00_mark_all_devices_lost(vha, 0);
	}

	/* Cleanup all outstanding cmds (for pha and vha) */
	qla2x00_abort_all_cmds(base_vha, DID_NO_CONNECT << 16);
}

/*
 * qla82xx_need_reset_handler
 *    Code to start reset sequence
 *
 * Note:
 *	IDC lock must be held upon entry
 *
 * Return:
 *    Success : 0
 *    Failed  : 1
 */
static void qla82xx_need_reset_handler(scsi_qla_host_t *base_vha)
{
	struct qla_hw_data *ha = base_vha->hw;
	uint32_t dev_state, drv_state, drv_active, active_mask = 0xFFFFFFFF;
	unsigned long reset_timeout;

	if (base_vha->flags.online) {
		qla82xx_idc_unlock(ha);
		qla2x00_abort_isp_cleanup(base_vha);
		qla82xx_idc_lock(ha);
	}

	if (!ha->flags.isp82xx_reset_owner) {
		qla_printk(KERN_INFO, ha, "%s(%ld): reset acknowledged by 0x%x\n",
			__func__, base_vha->host_no, ha->portnum);
		qla82xx_set_rst_ready(ha);
	} else {
		active_mask &= ~(1 << (ha->portnum * 4));
		DEBUG2(qla_printk(KERN_INFO, ha, "active_mask: 0x%x\n", active_mask));
	}

	/* wait for 10 seconds for reset ack from all functions */
	reset_timeout = jiffies + (ha->nx_reset_timeout * HZ);

	drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);

	while (drv_state != (drv_active & active_mask)) {

		if (time_after_eq(jiffies, reset_timeout)) {
			qla_printk(KERN_INFO, ha, "%s: RESET TIMEOUT! "
				"drv_state=9x%x drv_active=0x%x\n",
				QLA2XXX_DRIVER_NAME, drv_state, drv_active);
			break;
		}

		qla82xx_idc_unlock(ha);
		msleep(1000);
		qla82xx_idc_lock(ha);

		drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
		drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
	}

	dev_state = qla82xx_rd_32(ha, QLA82XX_CRB_DEV_STATE);

	/* Force to DEV_COLD unless someone else is starting a reset */
	if (dev_state != QLA82XX_DEV_INITIALIZING) {
		qla_printk(KERN_INFO, ha, "HW State: COLD/RE-INIT\n");
		qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_COLD);
		qla82xx_set_rst_ready(ha);
	}
}

static void qla82xx_need_qsnt_handler(scsi_qla_host_t *base_vha)
{
	struct qla_hw_data *ha = base_vha->hw;
	uint32_t dev_state, drv_state, drv_active;
	unsigned long reset_timeout;

	if (base_vha->flags.online) {
		/* Block any further queuing of i/o  */
		qla82xx_quiescent_state_cleanup(base_vha);
	}

	qla82xx_set_qsnt_ready(ha);

	/* wait for 30 seconds for quiescent ack from all functions */
	reset_timeout = jiffies + (30 * HZ);

	drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
	drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
	drv_active = drv_active << 0x01;

	while (drv_state != drv_active) {

		if (time_after_eq(jiffies, reset_timeout)) {
			qla_printk(KERN_INFO, ha,
				"%s: QUIESCENT TIMEOUT! drv_state=0x%x drv_active=0x%x\n",
				QLA2XXX_DRIVER_NAME, drv_state, drv_active);
			qla_printk(KERN_INFO, ha, "HW State: READY\n");
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_READY);

			qla82xx_idc_unlock(ha);
			qla2xxx_perform_loop_resync(base_vha);
			qla82xx_idc_lock(ha);

			qla82xx_clear_qsnt_ready(ha);
			return;
		}

		qla82xx_idc_unlock(ha);
		msleep(1000);
		qla82xx_idc_lock(ha);

		drv_state = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_STATE);
		drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
		drv_active = drv_active << 0x01;
	}

	dev_state = qla82xx_rd_32(ha, QLA82XX_CRB_DEV_STATE);
	/* Set DEV_QUIESCENT if everyone acks */
	if (dev_state == QLA82XX_DEV_NEED_QUIESCENT) {
		qla_printk(KERN_INFO, ha, "HW State: QUIESCENT\n");
		qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE, QLA82XX_DEV_QUIESCENT);
	}
}

/*
 * qla82xx_wait_for_state_change
 *    Wait for device state to change from given current state
 *
 * Note:
 *	IDC lock must not be held upon entry
 *
 * Return:
 *    Changed device state.
 */
uint32_t qla82xx_wait_for_state_change(struct qla_hw_data *ha, uint32_t state)
{
	uint32_t dev_state;

	do {
		msleep(1000);
		qla82xx_idc_lock(ha);
		dev_state = qla82xx_rd_32(ha, QLA82XX_CRB_DEV_STATE);
		qla82xx_idc_unlock(ha);
	} while (dev_state == state);

	return dev_state;
}

/*
 * qla82xx_device_state_handler
 *    Main state handler
 *
 * Note:
 *	IDC lock must not be held upon entry
 *
 * Return:
 *    Success : 0
 *    Failed  : 1
 */
int qla82xx_device_state_handler(scsi_qla_host_t *base_vha)
{
	struct qla_hw_data *ha = base_vha->hw;
	uint32_t dev_state;
	int rval = QLA_SUCCESS;
	unsigned long dev_init_timeout;

	if (!base_vha->flags.init_done)
		qla82xx_set_drv_active(ha);

	/* wait for 30 seconds for device to go ready */
	dev_init_timeout = jiffies + (ha->nx_dev_init_timeout * HZ);

	qla82xx_idc_lock(ha);
	while (1) {

		if (time_after_eq(jiffies, dev_init_timeout)) {
			printk("%s: Initialization TIMEOUT!\n", QLA2XXX_DRIVER_NAME);
			/* Unrecoverable error. Disable the device. */
			set_bit(ISP_UNRECOVERABLE, &base_vha->dpc_flags);
			qla2xxx_wake_dpc(base_vha);
			rval = QLA_FUNCTION_FAILED;
			break;
		}

		dev_state = qla82xx_rd_32(ha, QLA82XX_CRB_DEV_STATE);
		switch (dev_state) {
			case QLA82XX_DEV_READY:
				ha->flags.isp82xx_reset_owner = 0;
				DEBUG2(qla_printk(KERN_INFO, ha,
					"%s(%ld): reset_owner reset by 0x%x\n",
					__func__, base_vha->host_no, ha->portnum));
				goto exit;
			case QLA82XX_DEV_COLD:
				rval = qla82xx_device_bootstrap(base_vha);
				goto exit;
			case QLA82XX_DEV_INITIALIZING:
				qla82xx_idc_unlock(ha);
				msleep(1000);
				qla82xx_idc_lock(ha);
				break;
			case QLA82XX_DEV_NEED_RESET:
				if(!ql2xdontresethba)
					qla82xx_need_reset_handler(base_vha);
				else {
					qla82xx_idc_unlock(ha);
					msleep(1000);
					qla82xx_idc_lock(ha);
				}
				/* reset timeout value after need reset handler */
				dev_init_timeout = jiffies + (ha->nx_dev_init_timeout * HZ);
				break;
			case QLA82XX_DEV_NEED_QUIESCENT:
				qla82xx_need_qsnt_handler(base_vha);
				/* reset timeout value after need quiescent handler */
				dev_init_timeout = jiffies + (ha->nx_dev_init_timeout * HZ);
				break;
			case QLA82XX_DEV_QUIESCENT:
				if (ha->flags.quiesce_owner)
					goto exit;

				qla82xx_idc_unlock(ha);
				msleep(1000);
				qla82xx_idc_lock(ha);
				dev_init_timeout = jiffies + (ha->nx_dev_init_timeout * HZ);
				break;
			case QLA82XX_DEV_FAILED:
				qla82xx_idc_unlock(ha);
				qla82xx_dev_failed_handler(base_vha);
				rval = QLA_FUNCTION_FAILED;
				qla82xx_idc_lock(ha);
				goto exit;
			default:
				printk(KERN_INFO "Unknown Device State: %x\n",dev_state);
				qla82xx_idc_unlock(ha);
				qla82xx_dev_failed_handler(base_vha);
				rval = QLA_FUNCTION_FAILED;
				qla82xx_idc_lock(ha);
				goto exit;
		}
	}

exit:
	qla82xx_idc_unlock(ha);
	return rval;
}
