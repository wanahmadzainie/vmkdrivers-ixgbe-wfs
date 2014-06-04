/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2011 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "exioct.h"
#include "inioct.h"

/*
 * NVRAM support routines
 */

/**
 * qla2x00_lock_nvram_access() -
 * @ha: HA context
 */
void
qla2x00_lock_nvram_access(struct qla_hw_data *ha)
{
	uint16_t data;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	if (!IS_QLA2100(ha) && !IS_QLA2200(ha) && !IS_QLA2300(ha)) {
		data = RD_REG_WORD(&reg->nvram);
		while (data & NVR_BUSY) {
			udelay(100);
			data = RD_REG_WORD(&reg->nvram);
		}

		/* Lock resource */
		WRT_REG_WORD(&reg->u.isp2300.host_semaphore, 0x1);
		RD_REG_WORD(&reg->u.isp2300.host_semaphore);
		udelay(5);
		data = RD_REG_WORD(&reg->u.isp2300.host_semaphore);
		while ((data & BIT_0) == 0) {
			/* Lock failed */
			udelay(100);
			WRT_REG_WORD(&reg->u.isp2300.host_semaphore, 0x1);
			RD_REG_WORD(&reg->u.isp2300.host_semaphore);
			udelay(5);
			data = RD_REG_WORD(&reg->u.isp2300.host_semaphore);
		}
	}
}

/**
 * qla2x00_unlock_nvram_access() -
 * @ha: HA context
 */
void
qla2x00_unlock_nvram_access(struct qla_hw_data *ha)
{
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	if (!IS_QLA2100(ha) && !IS_QLA2200(ha) && !IS_QLA2300(ha)) {
		WRT_REG_WORD(&reg->u.isp2300.host_semaphore, 0);
		RD_REG_WORD(&reg->u.isp2300.host_semaphore);
	}
}

/**
 * qla2x00_nv_write() - Prepare for NVRAM read/write operation.
 * @ha: HA context
 * @data: Serial interface selector
 */
static void
qla2x00_nv_write(struct qla_hw_data *ha, uint16_t data)
{
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	WRT_REG_WORD(&reg->nvram, data | NVR_SELECT | NVR_WRT_ENABLE);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	NVRAM_DELAY();
	WRT_REG_WORD(&reg->nvram, data | NVR_SELECT| NVR_CLOCK |
	    NVR_WRT_ENABLE);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	NVRAM_DELAY();
	WRT_REG_WORD(&reg->nvram, data | NVR_SELECT | NVR_WRT_ENABLE);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	NVRAM_DELAY();
}

/**
 * qla2x00_nvram_request() - Sends read command to NVRAM and gets data from
 *	NVRAM.
 * @ha: HA context
 * @nv_cmd: NVRAM command
 *
 * Bit definitions for NVRAM command:
 *
 *	Bit 26     = start bit
 *	Bit 25, 24 = opcode
 *	Bit 23-16  = address
 *	Bit 15-0   = write data
 *
 * Returns the word read from nvram @addr.
 */
static uint16_t
qla2x00_nvram_request(struct qla_hw_data *ha, uint32_t nv_cmd)
{
	uint8_t		cnt;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	uint16_t	data = 0;
	uint16_t	reg_data;

	/* Send command to NVRAM. */
	nv_cmd <<= 5;
	for (cnt = 0; cnt < 11; cnt++) {
		if (nv_cmd & BIT_31)
			qla2x00_nv_write(ha, NVR_DATA_OUT);
		else
			qla2x00_nv_write(ha, 0);
		nv_cmd <<= 1;
	}

	/* Read data from NVRAM. */
	for (cnt = 0; cnt < 16; cnt++) {
		WRT_REG_WORD(&reg->nvram, NVR_SELECT | NVR_CLOCK);
		RD_REG_WORD(&reg->nvram);	/* PCI Posting. */
		NVRAM_DELAY();
		data <<= 1;
		reg_data = RD_REG_WORD(&reg->nvram);
		if (reg_data & NVR_DATA_IN)
			data |= BIT_0;
		WRT_REG_WORD(&reg->nvram, NVR_SELECT);
		RD_REG_WORD(&reg->nvram);	/* PCI Posting. */
		NVRAM_DELAY();
	}

	/* Deselect chip. */
	WRT_REG_WORD(&reg->nvram, NVR_DESELECT);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	NVRAM_DELAY();

	return (data);
}

/**
 * qla2x00_get_nvram_word() - Calculates word position in NVRAM and calls the
 *	request routine to get the word from NVRAM.
 * @ha: HA context
 * @addr: Address in NVRAM to read
 *
 * Returns the word read from nvram @addr.
 */
uint16_t
qla2x00_get_nvram_word(struct qla_hw_data *ha, uint32_t addr)
{
	uint16_t	data;
	uint32_t	nv_cmd;

	nv_cmd = addr << 16;
	nv_cmd |= NV_READ_OP;
	data = qla2x00_nvram_request(ha, nv_cmd);

	return (data);
}

/**
 * qla2x00_nv_deselect() - Clean NVRAM operations.
 * @ha: HA context
 */
static void
qla2x00_nv_deselect(struct qla_hw_data *ha)
{
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	WRT_REG_WORD(&reg->nvram, NVR_DESELECT);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	NVRAM_DELAY();
}

/**
 * qla2x00_write_nvram_word() - Write NVRAM data.
 * @ha: HA context
 * @addr: Address in NVRAM to write
 * @data: word to program
 */
void
qla2x00_write_nvram_word(struct qla_hw_data *ha, uint32_t addr, uint16_t data)
{
	int count;
	uint16_t word;
	uint32_t nv_cmd, wait_cnt;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	qla2x00_nv_write(ha, NVR_DATA_OUT);
	qla2x00_nv_write(ha, 0);
	qla2x00_nv_write(ha, 0);

	for (word = 0; word < 8; word++)
		qla2x00_nv_write(ha, NVR_DATA_OUT);

	qla2x00_nv_deselect(ha);

	/* Write data */
	nv_cmd = (addr << 16) | NV_WRITE_OP;
	nv_cmd |= data;
	nv_cmd <<= 5;
	for (count = 0; count < 27; count++) {
		if (nv_cmd & BIT_31)
			qla2x00_nv_write(ha, NVR_DATA_OUT);
		else
			qla2x00_nv_write(ha, 0);

		nv_cmd <<= 1;
	}

	qla2x00_nv_deselect(ha);

	/* Wait for NVRAM to become ready */
	WRT_REG_WORD(&reg->nvram, NVR_SELECT);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	wait_cnt = NVR_WAIT_CNT;
	do {
		if (!--wait_cnt) {
 			DEBUG9_10(qla_printk(KERN_WARNING, ha,
 			    "NVRAM didn't go ready...\n"));
			break;
		}
		NVRAM_DELAY();
		word = RD_REG_WORD(&reg->nvram);
	} while ((word & NVR_DATA_IN) == 0);

	qla2x00_nv_deselect(ha);

	/* Disable writes */
	qla2x00_nv_write(ha, NVR_DATA_OUT);
	for (count = 0; count < 10; count++)
		qla2x00_nv_write(ha, 0);

	qla2x00_nv_deselect(ha);
}

static int
qla2x00_write_nvram_word_tmo(struct qla_hw_data *ha, uint32_t addr, uint16_t data,
    uint32_t tmo)
{
	int ret, count;
	uint16_t word;
	uint32_t nv_cmd;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	ret = QLA_SUCCESS;

	qla2x00_nv_write(ha, NVR_DATA_OUT);
	qla2x00_nv_write(ha, 0);
	qla2x00_nv_write(ha, 0);

	for (word = 0; word < 8; word++)
		qla2x00_nv_write(ha, NVR_DATA_OUT);

	qla2x00_nv_deselect(ha);

	/* Write data */
	nv_cmd = (addr << 16) | NV_WRITE_OP;
	nv_cmd |= data;
	nv_cmd <<= 5;
	for (count = 0; count < 27; count++) {
		if (nv_cmd & BIT_31)
			qla2x00_nv_write(ha, NVR_DATA_OUT);
		else
			qla2x00_nv_write(ha, 0);

		nv_cmd <<= 1;
	}

	qla2x00_nv_deselect(ha);

	/* Wait for NVRAM to become ready */
	WRT_REG_WORD(&reg->nvram, NVR_SELECT);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	do {
		NVRAM_DELAY();
		word = RD_REG_WORD(&reg->nvram);
		if (!--tmo) {
			ret = QLA_FUNCTION_FAILED;
			break;
		}
	} while ((word & NVR_DATA_IN) == 0);

	qla2x00_nv_deselect(ha);

	/* Disable writes */
	qla2x00_nv_write(ha, NVR_DATA_OUT);
	for (count = 0; count < 10; count++)
		qla2x00_nv_write(ha, 0);

	qla2x00_nv_deselect(ha);

	return ret;
}

/**
 * qla2x00_clear_nvram_protection() -
 * @ha: HA context
 */
static int
qla2x00_clear_nvram_protection(struct qla_hw_data *ha)
{
	int ret, stat;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	uint32_t word, wait_cnt;
	uint16_t wprot, wprot_old;

	/* Clear NVRAM write protection. */
	ret = QLA_FUNCTION_FAILED;

	wprot_old = cpu_to_le16(qla2x00_get_nvram_word(ha, ha->nvram_base));
	stat = qla2x00_write_nvram_word_tmo(ha, ha->nvram_base,
	    __constant_cpu_to_le16(0x1234), 100000);
	wprot = cpu_to_le16(qla2x00_get_nvram_word(ha, ha->nvram_base));
	if (stat != QLA_SUCCESS || wprot != 0x1234) {
		/* Write enable. */
		qla2x00_nv_write(ha, NVR_DATA_OUT);
		qla2x00_nv_write(ha, 0);
		qla2x00_nv_write(ha, 0);
		for (word = 0; word < 8; word++)
			qla2x00_nv_write(ha, NVR_DATA_OUT);

		qla2x00_nv_deselect(ha);

		/* Enable protection register. */
		qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
		qla2x00_nv_write(ha, NVR_PR_ENABLE);
		qla2x00_nv_write(ha, NVR_PR_ENABLE);
		for (word = 0; word < 8; word++)
			qla2x00_nv_write(ha, NVR_DATA_OUT | NVR_PR_ENABLE);

		qla2x00_nv_deselect(ha);

		/* Clear protection register (ffff is cleared). */
		qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
		qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
		qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
		for (word = 0; word < 8; word++)
			qla2x00_nv_write(ha, NVR_DATA_OUT | NVR_PR_ENABLE);

		qla2x00_nv_deselect(ha);

		/* Wait for NVRAM to become ready. */
		WRT_REG_WORD(&reg->nvram, NVR_SELECT);
		RD_REG_WORD(&reg->nvram);	/* PCI Posting. */
		wait_cnt = NVR_WAIT_CNT;
		do {
			if (!--wait_cnt) {
	 			DEBUG9_10(qla_printk(KERN_WARNING, ha,
	 			    "NVRAM didn't go ready...\n"));
				break;
			}
			NVRAM_DELAY();
			word = RD_REG_WORD(&reg->nvram);
		} while ((word & NVR_DATA_IN) == 0);

		if (wait_cnt)
			ret = QLA_SUCCESS;
	} else
		qla2x00_write_nvram_word(ha, ha->nvram_base, wprot_old);

	return ret;
}

static void
qla2x00_set_nvram_protection(struct qla_hw_data *ha, int stat)
{
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	uint32_t word, wait_cnt;

	if (stat != QLA_SUCCESS)
		return;

	/* Set NVRAM write protection. */
	/* Write enable. */
	qla2x00_nv_write(ha, NVR_DATA_OUT);
	qla2x00_nv_write(ha, 0);
	qla2x00_nv_write(ha, 0);
	for (word = 0; word < 8; word++)
		qla2x00_nv_write(ha, NVR_DATA_OUT);

	qla2x00_nv_deselect(ha);

	/* Enable protection register. */
	qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
	qla2x00_nv_write(ha, NVR_PR_ENABLE);
	qla2x00_nv_write(ha, NVR_PR_ENABLE);
	for (word = 0; word < 8; word++)
		qla2x00_nv_write(ha, NVR_DATA_OUT | NVR_PR_ENABLE);

	qla2x00_nv_deselect(ha);

	/* Enable protection register. */
	qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
	qla2x00_nv_write(ha, NVR_PR_ENABLE);
	qla2x00_nv_write(ha, NVR_PR_ENABLE | NVR_DATA_OUT);
	for (word = 0; word < 8; word++)
		qla2x00_nv_write(ha, NVR_PR_ENABLE);

	qla2x00_nv_deselect(ha);

	/* Wait for NVRAM to become ready. */
	WRT_REG_WORD(&reg->nvram, NVR_SELECT);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	wait_cnt = NVR_WAIT_CNT;
	do {
		if (!--wait_cnt) {
 			DEBUG9_10(qla_printk(KERN_WARNING, ha,
 			    "NVRAM didn't go ready...\n"));
			break;
		}
		NVRAM_DELAY();
		word = RD_REG_WORD(&reg->nvram);
	} while ((word & NVR_DATA_IN) == 0);
}


/*****************************************************************************/
/* Flash Manipulation Routines                                               */
/*****************************************************************************/

#define OPTROM_BURST_SIZE	0x1000
#define OPTROM_BURST_DWORDS	(OPTROM_BURST_SIZE / 4)

static inline uint32_t
flash_conf_addr(struct qla_hw_data *ha, uint32_t faddr)
{
	return ha->flash_conf_off | faddr;
}

static inline uint32_t
flash_data_addr(struct qla_hw_data *ha, uint32_t faddr)
{
	return ha->flash_data_off | faddr;
}

static inline uint32_t
nvram_conf_addr(struct qla_hw_data *ha, uint32_t naddr)
{
	return ha->nvram_conf_off | naddr;
}

static inline uint32_t
nvram_data_addr(struct qla_hw_data *ha, uint32_t naddr)
{
	return ha->nvram_data_off | naddr;
}

static uint32_t
qla24xx_read_flash_dword(struct qla_hw_data *ha, uint32_t addr)
{
	int rval;
	uint32_t cnt, data;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	WRT_REG_DWORD(&reg->flash_addr, addr & ~FARX_DATA_FLAG);
	/* Wait for READ cycle to complete. */
	rval = QLA_SUCCESS;
	for (cnt = 3000;
	    (RD_REG_DWORD(&reg->flash_addr) & FARX_DATA_FLAG) == 0 &&
	    rval == QLA_SUCCESS; cnt--) {
		if (cnt)
			udelay(10);
		else
			rval = QLA_FUNCTION_TIMEOUT;
		cond_resched();
	}

	/* TODO: What happens if we time out? */
	data = 0xDEADDEAD;
	if (rval == QLA_SUCCESS)
		data = RD_REG_DWORD(&reg->flash_data);

	return data;
}

/**
 * qla2x00_read_flash_byte() - Reads a byte from flash
 * @ha: HA context
 * @addr: Address in flash to read
 *
 * A word is read from the chip, but, only the lower byte is valid.
 *
 * Returns the byte read from flash @addr.
 */
static uint8_t
qla2x00_read_flash_byte(struct qla_hw_data *ha, uint32_t addr)
{
	uint16_t data;
	uint16_t bank_select;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	bank_select = RD_REG_WORD(&reg->ctrl_status);

	if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
		/* Specify 64K address range: */
		/*  clear out Module Select and Flash Address bits [19:16]. */
		bank_select &= ~0xf8;
		bank_select |= addr >> 12 & 0xf0;
		bank_select |= CSR_FLASH_64K_BANK;
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */

		WRT_REG_WORD(&reg->flash_address, (uint16_t)addr);
		data = RD_REG_WORD(&reg->flash_data);

		return (uint8_t)data;
	}

	/* Setup bit 16 of flash address. */
	if ((addr & BIT_16) && ((bank_select & CSR_FLASH_64K_BANK) == 0)) {
		bank_select |= CSR_FLASH_64K_BANK;
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */
	} else if (((addr & BIT_16) == 0) &&
	    (bank_select & CSR_FLASH_64K_BANK)) {
		bank_select &= ~(CSR_FLASH_64K_BANK);
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */
	}

	/* Always perform IO mapped accesses to the FLASH registers. */
	if (ha->pio_address) {
		uint16_t data2;

		reg = (struct device_reg_2xxx __iomem *)ha->pio_address;
		WRT_REG_WORD_PIO(&reg->flash_address, (uint16_t)addr);
		do {
			data = RD_REG_WORD_PIO(&reg->flash_data);
			barrier();
			cpu_relax();
			data2 = RD_REG_WORD_PIO(&reg->flash_data);
		} while (data != data2);
	} else {
		WRT_REG_WORD(&reg->flash_address, (uint16_t)addr);
		data = qla2x00_debounce_register(&reg->flash_data);
	}

	return (uint8_t)data;
}

static void
qla2x00_read_flash_data(scsi_qla_host_t *vha, uint8_t *tmp_buf, uint32_t saddr,
        uint32_t length)
{
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	uint32_t midpoint, ilength;
	uint8_t data;

	midpoint = length / 2;

	WRT_REG_WORD(&reg->nvram, 0);
	RD_REG_WORD(&reg->nvram);
	for (ilength = 0; ilength < length; saddr++, ilength++, tmp_buf++) {
		if (ilength == midpoint) {
			WRT_REG_WORD(&reg->nvram, NVR_SELECT);
			RD_REG_WORD(&reg->nvram);
		}
		data = qla2x00_read_flash_byte(ha, saddr);
		if (saddr % 100)
			udelay(10);
		*tmp_buf = data;
		cond_resched();
	}
}

uint32_t *
qla24xx_read_flash_data(scsi_qla_host_t *vha, uint32_t *dwptr, uint32_t faddr,
    uint32_t dwords)
{
	uint32_t i;
	struct qla_hw_data *ha = vha->hw;

	/* Dword reads to flash. */
	for (i = 0; i < dwords; i++, faddr++)
 		dwptr[i] = cpu_to_le32(qla24xx_read_flash_dword(ha,
 		    flash_data_addr(ha, faddr)));

	return dwptr;
}

/**
 * qla2x00_get_oem_serial_number() - Stores the serial number in ha,
 *	the serial number for this OEM is found in VPD data in FLASH
 * @ha: HA context
 * @pcids: Start of the PCI image which contain VPD data for the OEM HBA
 *
 * Returns
 *	0 - If PCI images has valid VPD
 *	1 - If VPD data is not found
 */
static int
qla2x00_get_oem_serial_number(struct scsi_qla_host *vha, uint32_t pcids)
{
	INT_PCI_DATA_STRUCT pci_data;
	uint32_t oem_vpd_size = 0x200; /* 512 */
	uint32_t len = 0;
	uint32_t byte_idx = 0;
	uint8_t vpd_buf[oem_vpd_size];
	struct qla_hw_data *ha = vha->hw;

	/* Read the PCI data structure */
	qla2x00_read_flash_data(vha, (uint8_t *)&pci_data, pcids,
	    sizeof(pci_data));

	if (le16_to_cpu(pci_data.DeviceId) != INT_PDS_DID_VPD)
		return 1;

	memset(vpd_buf, 0, oem_vpd_size);
	memset(ha->oem_serial_num, '\0', MAX_OEM_SERIAL_LEN);

	/* PCI data is pointing to VPD page */
	qla2x00_read_flash_data(vha, vpd_buf, pcids +
	    le16_to_cpu(pci_data.Length), oem_vpd_size);

	/* Parse the buffer for "SN" */
	while (byte_idx < oem_vpd_size) {
		if (vpd_buf[byte_idx] == 'S' && vpd_buf[byte_idx + 1] == 'N') {
			len = vpd_buf[byte_idx + 2];
			len = len > (MAX_OEM_SERIAL_LEN - 1) ?
			    (MAX_OEM_SERIAL_LEN - 1) : len;
			memcpy(ha->oem_serial_num, &vpd_buf[byte_idx + 3],
			    len);
			break;
		}
		byte_idx++;
	}

	return 0;
}
static int
qla24xx_write_flash_dword(struct qla_hw_data *ha, uint32_t addr, uint32_t data)
{
	int rval;
	uint32_t cnt;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	WRT_REG_DWORD(&reg->flash_data, data);
	RD_REG_DWORD(&reg->flash_data);		/* PCI Posting. */
	WRT_REG_DWORD(&reg->flash_addr, addr | FARX_DATA_FLAG);
	/* Wait for Write cycle to complete. */
	rval = QLA_SUCCESS;
	for (cnt = 500000; (RD_REG_DWORD(&reg->flash_addr) & FARX_DATA_FLAG) &&
	    rval == QLA_SUCCESS; cnt--) {
		if (cnt)
			udelay(10);
		else
			rval = QLA_FUNCTION_TIMEOUT;
		cond_resched();
	}
	return rval;
}

static void
qla24xx_get_flash_manufacturer(struct qla_hw_data *ha, uint8_t *man_id,
    uint8_t *flash_id)
{
	uint32_t ids;

	ids = qla24xx_read_flash_dword(ha, flash_conf_addr(ha, 0x03ab));
	*man_id = LSB(ids);
	*flash_id = MSB(ids);

	/* Check if man_id and flash_id are valid. */
	if (ids != 0xDEADDEAD && (*man_id == 0 || *flash_id == 0)) {
		/* Read information using 0x9f opcode
		 * Device ID, Mfg ID would be read in the format:
		 *   <Ext Dev Info><Device ID Part2><Device ID Part 1><Mfg ID>
		 * Example: ATMEL 0x00 01 45 1F
		 * Extract MFG and Dev ID from last two bytes.
		 */
		ids = qla24xx_read_flash_dword(ha, flash_conf_addr(ha, 0x009f));
		*man_id = LSB(ids);
		*flash_id = MSB(ids);
	}
}

static int
qla2xxx_find_flt_start(scsi_qla_host_t *vha, uint32_t *start)
{
	const char *loc, *locations[] = { "DEF", "PCI" };
	uint32_t pcihdr, pcids;
	uint32_t *dcode;
	uint8_t *buf, *bcode, last_image;
	uint16_t cnt, chksum, *wptr;
	struct qla_flt_location *fltl;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = ha->req_q_map[0];
	/*
	 * FLT-location structure resides after the last PCI region.
	 */

	/* Begin with sane defaults. */
	loc = locations[0];
	*start = 0;
	if (IS_QLA24XX_TYPE(ha))
		*start = FA_FLASH_LAYOUT_ADDR_24;
	else if (IS_QLA25XX(ha))
		*start = FA_FLASH_LAYOUT_ADDR;
	else if (IS_QLA81XX(ha))
		*start = FA_FLASH_LAYOUT_ADDR_81;
	else if (IS_QLA82XX(ha)) {
		*start = FA_FLASH_LAYOUT_ADDR_82;
		goto end;
	}
	/* Begin with first PCI expansion ROM header. */
	buf = (uint8_t *)req->ring;
	dcode = (uint32_t *)req->ring;
	pcihdr = 0;
	last_image = 1;
	do {
		/* Verify PCI expansion ROM header. */
		qla24xx_read_flash_data(vha, dcode, pcihdr >> 2, 0x20);
		bcode = buf + (pcihdr % 4);
		if (bcode[0x0] != 0x55 || bcode[0x1] != 0xaa)
			goto end;

		/* Locate PCI data structure. */
		pcids = pcihdr + ((bcode[0x19] << 8) | bcode[0x18]);
		qla24xx_read_flash_data(vha, dcode, pcids >> 2, 0x20);
		bcode = buf + (pcihdr % 4);

		/* Validate signature of PCI data structure. */
		if (bcode[0x0] != 'P' || bcode[0x1] != 'C' ||
		    bcode[0x2] != 'I' || bcode[0x3] != 'R')
			goto end;

		last_image = bcode[0x15] & BIT_7;

		/* Locate next PCI expansion ROM. */
		pcihdr += ((bcode[0x11] << 8) | bcode[0x10]) * 512;
	} while (!last_image);

	/* Now verify FLT-location structure. */
	fltl = (struct qla_flt_location *)req->ring;
	qla24xx_read_flash_data(vha, dcode, pcihdr >> 2,
	    sizeof(struct qla_flt_location) >> 2);
	if (fltl->sig[0] != 'Q' || fltl->sig[1] != 'F' ||
	    fltl->sig[2] != 'L' || fltl->sig[3] != 'T')
		goto end;

	wptr = (uint16_t *)req->ring;
	cnt = sizeof(struct qla_flt_location) >> 1;
	for (chksum = 0; cnt; cnt--)
		chksum += le16_to_cpu(*wptr++);
	if (chksum) {
		qla_printk(KERN_ERR, ha,
		    "Inconsistent FLTL detected: checksum=0x%x.\n", chksum);
		qla2x00_dump_buffer(buf, sizeof(struct qla_flt_location));
		return QLA_FUNCTION_FAILED;
	}

	/* Good data.  Use specified location. */
	loc = locations[1];
	*start = (le16_to_cpu(fltl->start_hi) << 16 |
	    le16_to_cpu(fltl->start_lo)) >> 2;
end:
	DEBUG2(qla_printk(KERN_DEBUG, ha, "FLTL[%s] = 0x%x.\n", loc, *start));
	return QLA_SUCCESS;
}

static void
qla2xxx_get_flt_info(scsi_qla_host_t *vha, uint32_t flt_addr)
{
	const char *loc, *locations[] = { "DEF", "FLT" };
	const uint32_t def_fw[] =
		{ FA_RISC_CODE_ADDR, FA_RISC_CODE_ADDR, FA_RISC_CODE_ADDR_81 };
	const uint32_t def_boot[] =
		{ FA_BOOT_CODE_ADDR, FA_BOOT_CODE_ADDR, FA_BOOT_CODE_ADDR_81 };
	const uint32_t def_vpd_nvram[] =
		{ FA_VPD_NVRAM_ADDR, FA_VPD_NVRAM_ADDR, FA_VPD_NVRAM_ADDR_81 };
	const uint32_t def_vpd0[] = { 0, 0, FA_VPD0_ADDR_81 };
	const uint32_t def_vpd1[] = { 0, 0, FA_VPD1_ADDR_81 };
	const uint32_t def_nvram0[] = { 0, 0, FA_NVRAM0_ADDR_81 };
	const uint32_t def_nvram1[] = { 0, 0, FA_NVRAM1_ADDR_81 };
	const uint32_t def_fdt[] =
		{ FA_FLASH_DESCR_ADDR_24, FA_FLASH_DESCR_ADDR,
			FA_FLASH_DESCR_ADDR_81 };
	const uint32_t def_npiv_conf0[] =
		{ FA_NPIV_CONF0_ADDR_24, FA_NPIV_CONF0_ADDR,
			FA_NPIV_CONF0_ADDR_81 };
	const uint32_t def_npiv_conf1[] =
		{ FA_NPIV_CONF1_ADDR_24, FA_NPIV_CONF1_ADDR,
			FA_NPIV_CONF1_ADDR_81 };
	uint32_t def;
	uint16_t *wptr;
	uint16_t cnt, chksum;
	uint32_t start;
	struct qla_flt_header *flt;
	struct qla_flt_region *region;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = ha->req_q_map[0];

	/* Assign FCP prio region since older FLT's may not have FLT or
	 * FCP prio region in it's FLT.
	 */
	ha->flt_region_fcp_prio = !(PCI_FUNC(ha->pdev->devfn) & 1) ?
		((IS_QLA24XX_TYPE(ha)) ? FA_FCP_PRIO0_ADDR : FA_FCP_PRIO0_ADDR_25) :
		((IS_QLA24XX_TYPE(ha)) ? FA_FCP_PRIO1_ADDR : FA_FCP_PRIO1_ADDR_25);

	ha->flt_region_flt = flt_addr;
	wptr = (uint16_t *)req->ring;
	flt = (struct qla_flt_header *)req->ring;
	region = (struct qla_flt_region *)&flt[1];
	ha->isp_ops->read_optrom(vha, (uint8_t *)req->ring,
	    flt_addr << 2, OPTROM_BURST_SIZE);
	if (*wptr == __constant_cpu_to_le16(0xffff))
		goto no_flash_data;
	if (flt->version != __constant_cpu_to_le16(1)) {
		DEBUG2(qla_printk(KERN_INFO, ha, "Unsupported FLT detected: "
		    "version=0x%x length=0x%x checksum=0x%x.\n",
		    le16_to_cpu(flt->version), le16_to_cpu(flt->length),
		    le16_to_cpu(flt->checksum)));
		goto no_flash_data;
	}

	cnt = (sizeof(struct qla_flt_header) + le16_to_cpu(flt->length)) >> 1;
	for (chksum = 0; cnt; cnt--)
		chksum += le16_to_cpu(*wptr++);
	if (chksum) {
		DEBUG2(qla_printk(KERN_INFO, ha, "Inconsistent FLT detected: "
		    "version=0x%x length=0x%x checksum=0x%x.\n",
		    le16_to_cpu(flt->version), le16_to_cpu(flt->length),
		    chksum));
		goto no_flash_data;
	}

	loc = locations[1];
	cnt = le16_to_cpu(flt->length) / sizeof(struct qla_flt_region);
	for ( ; cnt; cnt--, region++) {
		/* Store addresses as DWORD offsets. */
		start = le32_to_cpu(region->start) >> 2;

		DEBUG3(qla_printk(KERN_DEBUG, ha, "FLT[%02x]: start=0x%x "
		    "end=0x%x size=0x%x.\n", le32_to_cpu(region->code), start,
		    le32_to_cpu(region->end) >> 2, le32_to_cpu(region->size)));

		switch (le32_to_cpu(region->code) & 0xff) {
		case FLT_REG_FW:
			ha->flt_region_fw = start;
			break;
		case FLT_REG_BOOT_CODE:
			ha->flt_region_boot = start;
			break;
		case FLT_REG_VPD_0:
			ha->flt_region_vpd_nvram = start;
 			if (ha->flags.port0)
 				ha->flt_region_vpd = start;
			break;
 		case FLT_REG_VPD_1:
 			if (!ha->flags.port0)
 				ha->flt_region_vpd = start;
			break;
 		case FLT_REG_NVRAM_0:
 			if (ha->flags.port0)
 				ha->flt_region_nvram = start;
			break;
 		case FLT_REG_NVRAM_1:
 			if (!ha->flags.port0)
 				ha->flt_region_nvram = start;
 			break;
 		case FLT_REG_FDT:
 			ha->flt_region_fdt = start;
			break;
		case FLT_REG_NPIV_CONF_0:
 			if (ha->flags.port0)
				ha->flt_region_npiv_conf = start;
			break;
		case FLT_REG_NPIV_CONF_1:
 			if (!ha->flags.port0)
				ha->flt_region_npiv_conf = start;
			break;
 		case FLT_REG_GOLD_FW:
 			ha->flt_region_gold_fw = start;
 			break;
		case FLT_REG_FCP_PRIO_0:
			if (ha->flags.port0)
				ha->flt_region_fcp_prio = start;
			break;
		case FLT_REG_FCP_PRIO_1:
			if (!ha->flags.port0)
				ha->flt_region_fcp_prio = start;
			break;
		case FLT_REG_BOOT_CODE_82:
			ha->flt_region_boot = start;
			break;
		case FLT_REG_FW_82:
			ha->flt_region_fw = start;
			break;
		case FLT_REG_GOLD_FW_82:
			ha->flt_region_gold_fw = start;
			break;
		case FLT_REG_BOOTLOAD_82:
			ha->flt_region_bootload = start;
			break;
		case FLT_REG_VPD_82:
			ha->flt_region_vpd = start;
		}
	}
	goto done;

no_flash_data:
	/* Use hardcoded defaults. */
	loc = locations[0];
 	def = 0;
 	if (IS_QLA24XX_TYPE(ha))
 		def = 0;
 	else if (IS_QLA25XX(ha))
 		def = 1;
 	else if (IS_QLA81XX(ha))
 		def = 2;
 	ha->flt_region_fw = def_fw[def];
 	ha->flt_region_boot = def_boot[def];
 	ha->flt_region_vpd_nvram = def_vpd_nvram[def];
	ha->flt_region_vpd = ha->flags.port0 ? def_vpd0[def] : def_vpd1[def];
	ha->flt_region_nvram = ha->flags.port0 ?
		def_nvram0[def] : def_nvram1[def];
 	ha->flt_region_fdt = def_fdt[def];
	ha->flt_region_npiv_conf = ha->flags.port0 ?
		def_npiv_conf0[def] : def_npiv_conf1[def];
done:
	DEBUG2(qla_printk(KERN_DEBUG, ha, "FLT[%s]: boot=0x%x fw=0x%x "
 	    "vpd_nvram=0x%x vpd=0x%x nvram=0x%x fdt=0x%x flt=0x%x "
		"npiv=0x%x fcp_prio=0x%x.\n", loc, ha->flt_region_boot,
		ha->flt_region_fw, ha->flt_region_vpd_nvram, ha->flt_region_vpd,
		ha->flt_region_nvram, ha->flt_region_fdt, ha->flt_region_flt,
		ha->flt_region_npiv_conf, ha->flt_region_fcp_prio));
}

static void
qla2xxx_get_fdt_info(scsi_qla_host_t *vha)
{
#define FLASH_BLK_SIZE_4K	0x1000
#define FLASH_BLK_SIZE_32K	0x8000
#define FLASH_BLK_SIZE_64K	0x10000
	const char *loc, *locations[] = { "MID", "FDT" };
	uint16_t cnt, chksum;
	uint16_t *wptr;
	struct qla_fdt_layout *fdt;
	uint8_t	man_id, flash_id;
	uint16_t mid = 0, fid = 0;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = ha->req_q_map[0];

	if (!IS_QLA24XX_TYPE(ha) && !IS_QLA25XX(ha) && !IS_QLA8XXX_TYPE(ha))
		return;

	wptr = (uint16_t *)req->ring;
	fdt = (struct qla_fdt_layout *)req->ring;
	ha->isp_ops->read_optrom(vha, (uint8_t *)req->ring,
	    ha->flt_region_fdt << 2, OPTROM_BURST_SIZE);
	if (*wptr == __constant_cpu_to_le16(0xffff))
		goto no_flash_data;
	if (fdt->sig[0] != 'Q' || fdt->sig[1] != 'L' || fdt->sig[2] != 'I' ||
	    fdt->sig[3] != 'D')
		goto no_flash_data;

	for (cnt = 0, chksum = 0; cnt < sizeof(struct qla_fdt_layout) >> 1;
	    cnt++)
		chksum += le16_to_cpu(*wptr++);
	if (chksum) {
		DEBUG2(qla_printk(KERN_INFO, ha, "Inconsistent FDT detected: "
		    "checksum=0x%x id=%c version=0x%x.\n", chksum, fdt->sig[0],
		    le16_to_cpu(fdt->version)));
		DEBUG9(qla2x00_dump_buffer((uint8_t *)fdt, sizeof(*fdt)));
		goto no_flash_data;
	}

	loc = locations[1];
	mid = le16_to_cpu(fdt->man_id);
	fid = le16_to_cpu(fdt->id);
	ha->fdt_odd_index = mid == 0x1f;
	ha->fdt_wrt_disable = fdt->wrt_disable_bits;
	ha->fdt_erase_cmd = flash_conf_addr(ha, 0x0300 | fdt->erase_cmd);
	ha->fdt_block_size = le32_to_cpu(fdt->block_size);
	if (fdt->unprotect_sec_cmd) {
		ha->fdt_unprotect_sec_cmd = flash_conf_addr(ha, 0x0300 |
		    fdt->unprotect_sec_cmd);
		ha->fdt_protect_sec_cmd = fdt->protect_sec_cmd ?
		    flash_conf_addr(ha, 0x0300 | fdt->protect_sec_cmd):
		    flash_conf_addr(ha, 0x0336);
	}
	goto done;
no_flash_data:
	loc = locations[0];
	if (IS_QLA82XX(ha)) {
		ha->fdt_block_size = FLASH_BLK_SIZE_64K;
		goto done;
	}
	qla24xx_get_flash_manufacturer(ha, &man_id, &flash_id);
	mid = man_id;
	fid = flash_id;
	ha->fdt_wrt_disable = 0x9c;
	ha->fdt_erase_cmd = flash_conf_addr(ha, 0x03d8);
	switch (man_id) {
	case 0xbf: /* STT flash. */
		if (flash_id == 0x8e)
			ha->fdt_block_size = FLASH_BLK_SIZE_64K;
		else
			ha->fdt_block_size = FLASH_BLK_SIZE_32K;

		if (flash_id == 0x80)
 			ha->fdt_erase_cmd = flash_conf_addr(ha, 0x0352);
		break;
	case 0x13: /* ST M25P80. */
		ha->fdt_block_size = FLASH_BLK_SIZE_64K;
		break;
	case 0x1f: /* Atmel 26DF081A. */
		ha->fdt_block_size = FLASH_BLK_SIZE_4K;
 		ha->fdt_erase_cmd = flash_conf_addr(ha, 0x0320);
 		ha->fdt_unprotect_sec_cmd = flash_conf_addr(ha, 0x0339);
 		ha->fdt_protect_sec_cmd = flash_conf_addr(ha, 0x0336);
		break;
	default:
		/* Default to 64 kb sector size. */
		ha->fdt_block_size = FLASH_BLK_SIZE_64K;
		break;
	}
done:
	DEBUG2(qla_printk(KERN_DEBUG, ha, "FDT[%s]: (0x%x/0x%x) erase=0x%x "
	    "pro=%x upro=%x idx=%d wrtd=0x%x blk=0x%x.\n", loc, mid, fid,
	    ha->fdt_erase_cmd, ha->fdt_protect_sec_cmd,
	    ha->fdt_unprotect_sec_cmd, ha->fdt_odd_index, ha->fdt_wrt_disable,
	    ha->fdt_block_size));
}

static void qla2xxx_get_idc_param(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);
	struct req_que *req = base_vha->req;
#define QLA82XX_IDC_PARAM_ADDR	0x003e885c
	uint32_t *wptr;

	if (!IS_QLA82XX(ha))
		return;

	wptr = (uint32_t *)req->ring;
	ha->isp_ops->read_optrom(vha, (uint8_t *)req->ring,
			QLA82XX_IDC_PARAM_ADDR, 8);

	if (*wptr == __constant_cpu_to_le32(0xffffffff)) {
		ha->nx_dev_init_timeout = ROM_DEV_INIT_TIMEOUT;
		ha->nx_reset_timeout = ROM_DRV_RESET_ACK_TIMEOUT;
	} else {
		ha->nx_dev_init_timeout = le32_to_cpu(*wptr++);
		ha->nx_reset_timeout = le32_to_cpu(*wptr);
	}

	DEBUG2(qla_printk(KERN_DEBUG, ha,
			"ha->dev_init_timeout = %d\n", ha->nx_dev_init_timeout));
	DEBUG2(qla_printk(KERN_DEBUG, ha,
			"ha->reset_timeout = %d\n", ha->nx_reset_timeout));

	return;
}

int
qla2xxx_get_flash_info(scsi_qla_host_t *vha)
{
	int ret;
	uint32_t flt_addr;
	struct qla_hw_data *ha = vha->hw;

	if (!IS_QLA24XX_TYPE(ha) && !IS_QLA25XX(ha) && !IS_QLA8XXX_TYPE(ha))
		return QLA_SUCCESS;

	ret = qla2xxx_find_flt_start(vha, &flt_addr);
	if (ret != QLA_SUCCESS)
		return ret;

	qla2xxx_get_flt_info(vha, flt_addr);
	qla2xxx_get_fdt_info(vha);
	qla2xxx_get_idc_param(vha);

	return QLA_SUCCESS;
}

/*
 * qla2xxx_flash_npiv_conf 
 * 		Create pre-boot hosts based on the entries in 
 * 		the npiv region.
 *
 * Input:
 * 		@ha: Physical port context.
 * 		@context: Context from which the function is called.
 */
int
qla2xxx_flash_npiv_conf(scsi_qla_host_t *vha, uint8_t context)
{
#define NPIV_CONFIG_SIZE	(16*1024)
	uint16_t port_cnt = 0;
	int i;
	void *data;
	uint16_t *wptr;
	uint16_t cnt, chksum;
	struct qla_npiv_header hdr;
	struct qla_npiv_entry *entry;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *fvha;

	if (!IS_FWI2_CAPABLE(ha))
		return QLA_FUNCTION_FAILED;

	ha->isp_ops->read_optrom(vha, (uint8_t *)&hdr,
	    ha->flt_region_npiv_conf << 2, sizeof(struct qla_npiv_header));
	if (hdr.version == __constant_cpu_to_le16(0xffff))
		return QLA_FUNCTION_FAILED;
	if (hdr.version != __constant_cpu_to_le16(1)) {
		DEBUG2(qla_printk(KERN_INFO, ha, "Unsupported NPIV-Config "
		    "detected: version=0x%x entries=0x%x checksum=0x%x.\n",
		    le16_to_cpu(hdr.version), le16_to_cpu(hdr.entries),
		    le16_to_cpu(hdr.checksum)));
		return QLA_FUNCTION_FAILED;
	}

	data = kmalloc(NPIV_CONFIG_SIZE, GFP_KERNEL);
	if (!data) {
		DEBUG2(qla_printk(KERN_INFO, ha, "NPIV-Config: Unable to "
		    "allocate memory.\n"));
		return QLA_FUNCTION_FAILED;
	}

	ha->isp_ops->read_optrom(vha, (uint8_t *)data,
	    ha->flt_region_npiv_conf << 2, NPIV_CONFIG_SIZE);

	cnt = (sizeof(struct qla_npiv_header) + le16_to_cpu(hdr.entries) *
	    sizeof(struct qla_npiv_entry)) >> 1;
	for (wptr = data, chksum = 0; cnt; cnt--)
		chksum += le16_to_cpu(*wptr++);
	if (chksum) {
		DEBUG2(qla_printk(KERN_INFO, ha, "Inconsistent NPIV-Config "
		    "detected: version=0x%x entries=0x%x checksum=0x%x.\n",
		    le16_to_cpu(hdr.version), le16_to_cpu(hdr.entries),
		    chksum));
		kfree(data);
		return QLA_FUNCTION_FAILED;
	}

	entry = data + sizeof(struct qla_npiv_header);
	cnt = le16_to_cpu(hdr.entries);

	DEBUG15(printk(KERN_INFO "No. of npiv entries in flash = %d\n", cnt));

	for (i = 0; cnt; cnt--, entry++, i++) {
		uint16_t flags;
		struct fc_vport fcvport;
		struct fc_vport *fc_vport = &fcvport;
		memset(fc_vport, 0, sizeof(struct fc_vport));

		memcpy(&ha->npiv_info[i], entry, sizeof(struct qla_npiv_entry));

		flags = le16_to_cpu(entry->flags);
		if (flags == 0xffff)
			continue;
		if ((flags & BIT_0) == 0)
			continue;

		DEBUG2(qla_printk(KERN_DEBUG, ha, "NPIV[%02x]: "
			"wwpn =%02x%02x%02x%02x%02x%02x%02x%02x "
		    "wwnn =%02x%02x%02x%02x%02x%02x%02x%02x "
			"vf_id=0x%x q_qos=0x%x.\n", cnt, 
			entry->port_name[0], entry->port_name[1],
			entry->port_name[2], entry->port_name[3],
			entry->port_name[4], entry->port_name[5],
			entry->port_name[6], entry->port_name[7],
		    entry->node_name[0], entry->node_name[1], 
		    entry->node_name[2], entry->node_name[3], 
		    entry->node_name[4], entry->node_name[5], 
		    entry->node_name[6], entry->node_name[7], 
			le16_to_cpu(entry->vf_id),
 		    le16_to_cpu(entry->q_qos)));

		if(!vha->flags.init_done && i < QLA_PRECONFIG_VPORTS) {
			fc_vport->shost = vha->host;
			fc_vport->port_name = wwn_to_u64(entry->port_name);
			fc_vport->node_name = wwn_to_u64(entry->node_name);

			fvha = qla24xx_create_vhost(fc_vport, 1);
			if (fvha == NULL) {
				/* Something went wrong. Cleanup. */
				qla_printk(KERN_INFO, ha, "NPIV-Config: Failed to "
					"create vport [%02x]: wwpn =%02x%02x%02x%02x%02x%02x%02x%02x "
					"wwnn =%02x%02x%02x%02x%02x%02x%02x%02x.\n", cnt,
					entry->port_name[0], entry->port_name[1],
					entry->port_name[2], entry->port_name[3],
					entry->port_name[4], entry->port_name[5],
					entry->port_name[6], entry->port_name[7],
					entry->node_name[0], entry->node_name[1], 
					entry->node_name[2], entry->node_name[3], 
					entry->node_name[4], entry->node_name[5], 
					entry->node_name[6], entry->node_name[7]);
			}
		} else if(vha->flags.init_done){
			/* Fr future use. */
		}	
		port_cnt++;
	}

	DEBUG15(printk("scsi(%ld): Created %u pre-boot vports\n",
		vha->host_no, port_cnt));

	kfree(data);
	return QLA_SUCCESS;
}


/*
 * qla2xxx_npiv_cleanup
 * 		Cleanup pre-boot hosts.
 * 
 * Input:
 * 		@ha: Physical port  context.
 *
 * Note: It is assumed that when this function gets invoked,
 * the physical port's dpc and isr has stopped.
 */
void
qla2xxx_npiv_cleanup(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw; 
	scsi_qla_host_t *vp = NULL, *tvp = NULL;
	unsigned long   flags;

	spin_lock_irqsave(&ha->vport_lock, flags);
	/* Cleanup all hanging virtual ports. */
	list_for_each_entry_safe(vp, tvp, &ha->vp_list, list) {

		if (!vp->vp_idx)
			continue;

		/* Needs additional logic to handle this. As no DPC,
		 * the curresponding vp will never really get freed.
		 * Skip if vport delete is in progress. */
		if (test_bit(VP_DELETE_ACTIVE, &vp->dpc_flags))
			continue;

		spin_unlock_irqrestore(&ha->vport_lock, flags);

		qla2xxx_delete_boothost(vp, 1);

		spin_lock_irqsave(&ha->vport_lock, flags);
	}	
	spin_unlock_irqrestore(&ha->vport_lock, flags);
}

static int
qla24xx_unprotect_flash(scsi_qla_host_t *vha)
{
 	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

 	if (ha->flags.fac_supported)
 		return qla81xx_fac_do_write_enable(vha, 1);
 
	/* Enable flash write. */
	WRT_REG_DWORD(&reg->ctrl_status,
	    RD_REG_DWORD(&reg->ctrl_status) | CSRX_FLASH_ENABLE);
	RD_REG_DWORD(&reg->ctrl_status);	/* PCI Posting. */

	if (!ha->fdt_wrt_disable)
 		goto done;

 	/* Disable flash write-protection, first clear SR protection bit */
 	qla24xx_write_flash_dword(ha, flash_conf_addr(ha, 0x101), 0);
 	/* Then write zero again to clear remaining SR bits.*/
 	qla24xx_write_flash_dword(ha, flash_conf_addr(ha, 0x101), 0);
done:
 	return QLA_SUCCESS;
}

static int
qla24xx_protect_flash(scsi_qla_host_t *vha)
{
	uint32_t cnt;
 	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

 	if (ha->flags.fac_supported)
 		return qla81xx_fac_do_write_enable(vha, 0);
 
	if (!ha->fdt_wrt_disable)
		goto skip_wrt_protect;

	/* Enable flash write-protection and wait for completion. */
 	qla24xx_write_flash_dword(ha, flash_conf_addr(ha, 0x101),
	    ha->fdt_wrt_disable);
	for (cnt = 300; cnt &&
 	    qla24xx_read_flash_dword(ha, flash_conf_addr(ha, 0x005)) & BIT_0;
	    cnt--) {
		udelay(10);
	}

skip_wrt_protect:
	/* Disable flash write. */
	WRT_REG_DWORD(&reg->ctrl_status,
	    RD_REG_DWORD(&reg->ctrl_status) & ~CSRX_FLASH_ENABLE);
	RD_REG_DWORD(&reg->ctrl_status);	/* PCI Posting. */

	return QLA_SUCCESS;
}

static int
qla24xx_erase_sector(scsi_qla_host_t *vha, uint32_t fdata)
{
	struct qla_hw_data *ha = vha->hw;
	uint32_t start, finish;

	if (ha->flags.fac_supported) {
		start = fdata >> 2;
		finish = start + (ha->fdt_block_size >> 2) - 1;
		return qla81xx_fac_erase_sector(vha, flash_data_addr(ha,
		    start), flash_data_addr(ha, finish));
	}

	return qla24xx_write_flash_dword(ha, ha->fdt_erase_cmd,
	    (fdata & 0xff00) | ((fdata << 16) & 0xff0000) |
	    ((fdata >> 16) & 0xff));
}

uint8_t *
qla2x00_read_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
		uint32_t bytes)
{
	uint32_t i;
	uint16_t *wptr;
	struct qla_hw_data *ha = vha->hw;

	/* Word reads to NVRAM via registers. */
	wptr = (uint16_t *)buf;
	qla2x00_lock_nvram_access(ha);
	for (i = 0; i < bytes >> 1; i++, naddr++)
		wptr[i] = cpu_to_le16(qla2x00_get_nvram_word(ha, naddr));
	qla2x00_unlock_nvram_access(ha);

	return buf;
}

#define BLOCK_PROTECT_BITS 0x0F
int qla82xx_unprotect_flash(scsi_qla_host_t *vha)
{
	int ret;
	uint32_t val;
	struct qla_hw_data *ha = vha->hw;

	ret = ql82xx_rom_lock_d(ha);
	if (ret < 0) {
		printk("%s(%ld): ROM Lock failed\n", __func__, vha->host_no);
		return ret;
	}
	ret = qla82xx_read_status_reg(ha, &val);
	if (ret < 0)
		goto done_unprotect;

	val &= ~(BLOCK_PROTECT_BITS << 2);
	ret = qla82xx_write_status_reg(ha, val);
	if (ret < 0) {
		val |= (BLOCK_PROTECT_BITS << 2);
		qla82xx_write_status_reg(ha, val);
	}

	if (qla82xx_write_disable_flash(ha) != 0)
		qla_printk(KERN_WARNING, ha, "Write disable failed\n");

done_unprotect:
	qla82xx_rom_unlock(ha);
	return ret;
}

int qla82xx_protect_flash(scsi_qla_host_t *vha)
{
	int ret;
	uint32_t val;
	struct qla_hw_data *ha = vha->hw;

	ret = ql82xx_rom_lock_d(ha);
	if (ret < 0) {
		printk("%s(%ld): ROM Lock failed\n", __func__, vha->host_no);
		return ret;
	}
	ret = qla82xx_read_status_reg(ha, &val);
	if (ret < 0)
		goto done_protect;

	val |= (BLOCK_PROTECT_BITS << 2);
	/* LOCK all sectors */
	ret = qla82xx_write_status_reg(ha, val);
	if (ret < 0)
		qla_printk(KERN_WARNING, ha, "Write status register failed\n");

	if (qla82xx_write_disable_flash(ha) != 0)
		qla_printk(KERN_WARNING, ha, "Write disable failed\n");

done_protect:
	qla82xx_rom_unlock(ha);
	return ret;
}

int qla82xx_unprotect_sector(scsi_qla_host_t *vha, int addr)
{
	return 0;
}

int qla82xx_protect_sector(scsi_qla_host_t *vha, int addr)
{
	return 0;
}

int qla82xx_erase_sector(scsi_qla_host_t *vha, int addr)
{
	int ret = 0;
	struct qla_hw_data *ha = vha->hw;

	ret = ql82xx_rom_lock_d(ha);
	if (ret < 0) {
		printk("%s(%ld): ROM Lock failed\n", __func__, vha->host_no);
		return ret;
	}

	qla82xx_flash_set_write_enable(ha);

	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ADDRESS, addr);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_ABYTE_CNT, 3);
	qla82xx_wr_32(ha, QLA82XX_ROMUSB_ROM_INSTR_OPCODE, M25P_INSTR_SE);

	if (qla82xx_wait_rom_done(ha)) {
		qla_printk(KERN_WARNING, ha, "Error waiting for rom done\n");
		ret = -1;
		goto done;
	}

	ret = qla82xx_flash_wait_write_finish(ha);

done:
	qla82xx_rom_unlock(ha);
	return ret;
}

uint8_t *
qla24xx_read_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
    uint32_t bytes)
{
	uint32_t i;
	uint32_t *dwptr;
	struct qla_hw_data *ha = vha->hw;

	/* Dword reads to flash. */
	dwptr = (uint32_t *)buf;
	for (i = 0; i < bytes >> 2; i++, naddr++)
 		dwptr[i] = cpu_to_le32(qla24xx_read_flash_dword(ha,
 		    nvram_data_addr(ha, naddr)));

	return buf;
}

int
qla2x00_write_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
    uint32_t bytes)
{
	int ret, stat;
	uint32_t i;
	uint16_t *wptr;

	ret = QLA_SUCCESS;
	struct qla_hw_data *ha = vha->hw;

	qla2x00_lock_nvram_access(ha);

	/* Disable NVRAM write-protection. */
	stat = qla2x00_clear_nvram_protection(ha);

	wptr = (uint16_t *)buf;
	for (i = 0; i < bytes >> 1; i++, naddr++) {
		qla2x00_write_nvram_word(ha, naddr,
		    cpu_to_le16(*wptr));
		wptr++;
	}

	/* Enable NVRAM write-protection. */
	qla2x00_set_nvram_protection(ha, stat);

	qla2x00_unlock_nvram_access(ha);

	return ret;
}

int
qla24xx_write_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
    uint32_t bytes)
{
	int ret;
	uint32_t i;
	uint32_t *dwptr;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	ret = QLA_SUCCESS;

	/* Enable flash write. */
	WRT_REG_DWORD(&reg->ctrl_status,
	    RD_REG_DWORD(&reg->ctrl_status) | CSRX_FLASH_ENABLE);
	RD_REG_DWORD(&reg->ctrl_status);	/* PCI Posting. */

	/* Disable NVRAM write-protection. */
 	qla24xx_write_flash_dword(ha, nvram_conf_addr(ha, 0x101), 0);
 	qla24xx_write_flash_dword(ha, nvram_conf_addr(ha, 0x101), 0);

	/* Dword writes to flash. */
	dwptr = (uint32_t *)buf;
	for (i = 0; i < bytes >> 2; i++, naddr++, dwptr++) {
		ret = qla24xx_write_flash_dword(ha,
 		    nvram_data_addr(ha, naddr), cpu_to_le32(*dwptr));
		if (ret != QLA_SUCCESS) {
 			DEBUG9(qla_printk(KERN_WARNING, ha,
 			    "Unable to program nvram address=%x data=%x.\n",
 			    naddr, *dwptr));
			break;
		}
	}

	/* Enable NVRAM write-protection. */
 	qla24xx_write_flash_dword(ha, nvram_conf_addr(ha, 0x101), 0x8c);

	/* Disable flash write. */
	WRT_REG_DWORD(&reg->ctrl_status,
	    RD_REG_DWORD(&reg->ctrl_status) & ~CSRX_FLASH_ENABLE);
	RD_REG_DWORD(&reg->ctrl_status);	/* PCI Posting. */

	return ret;
}

uint8_t *
qla25xx_read_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
    uint32_t bytes)
{
	uint32_t i;
	uint32_t *dwptr;
	struct qla_hw_data *ha = vha->hw;
	/* Dword reads to flash. */
	dwptr = (uint32_t *)buf;
	for (i = 0; i < bytes >> 2; i++, naddr++)
		dwptr[i] = cpu_to_le32(qla24xx_read_flash_dword(ha,
 		    flash_data_addr(ha, ha->flt_region_vpd_nvram | naddr)));

	return buf;
}

int
qla25xx_write_nvram_data(scsi_qla_host_t *vha, uint8_t *buf, uint32_t naddr,
    uint32_t bytes)
{
#define RMW_BUFFER_SIZE	(64 * 1024)
	uint8_t *dbuf;
	struct qla_hw_data *ha = vha->hw;

	dbuf = vmalloc(RMW_BUFFER_SIZE);
	if (!dbuf)
		return QLA_MEMORY_ALLOC_FAILED;
	ha->isp_ops->read_optrom(vha, dbuf, ha->flt_region_vpd_nvram << 2,
	    RMW_BUFFER_SIZE);
	memcpy(dbuf + (naddr << 2), buf, bytes);
	ha->isp_ops->write_optrom(vha, dbuf, ha->flt_region_vpd_nvram << 2,
	    RMW_BUFFER_SIZE);
	vfree(dbuf);

	return QLA_SUCCESS;
}

static inline void
qla2x00_flip_colors(struct qla_hw_data *ha, uint16_t *pflags)
{
	if (IS_QLA2322(ha)) {
		/* Flip all colors. */
		if (ha->beacon_color_state == QLA_LED_ALL_ON) {
			/* Turn off. */
			ha->beacon_color_state = 0;
			*pflags = GPIO_LED_ALL_OFF;
		} else {
			/* Turn on. */
			ha->beacon_color_state = QLA_LED_ALL_ON;
			*pflags = GPIO_LED_RGA_ON;
		}
	} else {
		/* Flip green led only. */
		if (ha->beacon_color_state == QLA_LED_GRN_ON) {
			/* Turn off. */
			ha->beacon_color_state = 0;
			*pflags = GPIO_LED_GREEN_OFF_AMBER_OFF;
		} else {
			/* Turn on. */
			ha->beacon_color_state = QLA_LED_GRN_ON;
			*pflags = GPIO_LED_GREEN_ON_AMBER_OFF;
		}
	}
}

void
qla2x00_beacon_blink(struct scsi_qla_host *vha)
{
	uint16_t gpio_enable;
	uint16_t gpio_data;
	uint16_t led_color = 0;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	if (ha->pio_address)
		reg = (struct device_reg_2xxx __iomem *)ha->pio_address;

	spin_lock_irqsave(&ha->hardware_lock, flags);

	/* Save the Original GPIOE. */
	if (ha->pio_address) {
		gpio_enable = RD_REG_WORD_PIO(&reg->gpioe);
		gpio_data = RD_REG_WORD_PIO(&reg->gpiod);
	} else {
		gpio_enable = RD_REG_WORD(&reg->gpioe);
		gpio_data = RD_REG_WORD(&reg->gpiod);
	}

	/* Set the modified gpio_enable values */
	gpio_enable |= GPIO_LED_MASK;

	if (ha->pio_address) {
		WRT_REG_WORD_PIO(&reg->gpioe, gpio_enable);
	} else {
		WRT_REG_WORD(&reg->gpioe, gpio_enable);
		RD_REG_WORD(&reg->gpioe);
	}

	qla2x00_flip_colors(ha, &led_color);

	/* Clear out any previously set LED color. */
	gpio_data &= ~GPIO_LED_MASK;

	/* Set the new input LED color to GPIOD. */
	gpio_data |= led_color;

	/* Set the modified gpio_data values */
	if (ha->pio_address) {
		WRT_REG_WORD_PIO(&reg->gpiod, gpio_data);
	} else {
		WRT_REG_WORD(&reg->gpiod, gpio_data);
		RD_REG_WORD(&reg->gpiod);
	}

	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

int
qla2x00_beacon_on(struct scsi_qla_host *vha)
{
	uint16_t gpio_enable;
	uint16_t gpio_data;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	ha->fw_options[1] &= ~FO1_SET_EMPHASIS_SWING;
	ha->fw_options[1] |= FO1_DISABLE_GPIO6_7;

	if (qla2x00_set_fw_options(vha, ha->fw_options) != QLA_SUCCESS) {
		qla_printk(KERN_WARNING, ha,
		    "Unable to update fw options (beacon on).\n");
		return QLA_FUNCTION_FAILED;
	}

	if (ha->pio_address)
		reg = (struct device_reg_2xxx __iomem *)ha->pio_address;

	/* Turn off LEDs. */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	if (ha->pio_address) {
		gpio_enable = RD_REG_WORD_PIO(&reg->gpioe);
		gpio_data = RD_REG_WORD_PIO(&reg->gpiod);
	} else {
		gpio_enable = RD_REG_WORD(&reg->gpioe);
		gpio_data = RD_REG_WORD(&reg->gpiod);
	}
	gpio_enable |= GPIO_LED_MASK;

	/* Set the modified gpio_enable values. */
	if (ha->pio_address) {
		WRT_REG_WORD_PIO(&reg->gpioe, gpio_enable);
	} else {
		WRT_REG_WORD(&reg->gpioe, gpio_enable);
		RD_REG_WORD(&reg->gpioe);
	}

	/* Clear out previously set LED colour. */
	gpio_data &= ~GPIO_LED_MASK;
	if (ha->pio_address) {
		WRT_REG_WORD_PIO(&reg->gpiod, gpio_data);
	} else {
		WRT_REG_WORD(&reg->gpiod, gpio_data);
		RD_REG_WORD(&reg->gpiod);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	/*
	 * Let the per HBA timer kick off the blinking process based on
	 * the following flags. No need to do anything else now.
	 */
	ha->beacon_blink_led = 1;
	ha->beacon_color_state = 0;

	return QLA_SUCCESS;
}

int
qla2x00_beacon_off(struct scsi_qla_host *vha)
{
	int rval = QLA_SUCCESS;
	struct qla_hw_data *ha = vha->hw;

	ha->beacon_blink_led = 0;

	/* Set the on flag so when it gets flipped it will be off. */
	if (IS_QLA2322(ha))
		ha->beacon_color_state = QLA_LED_ALL_ON;
	else
		ha->beacon_color_state = QLA_LED_GRN_ON;

	ha->isp_ops->beacon_blink(vha);	/* This turns green LED off */

	ha->fw_options[1] &= ~FO1_SET_EMPHASIS_SWING;
	ha->fw_options[1] &= ~FO1_DISABLE_GPIO6_7;

	rval = qla2x00_set_fw_options(vha, ha->fw_options);
	if (rval != QLA_SUCCESS)
		qla_printk(KERN_WARNING, ha,
		    "Unable to update fw options (beacon off).\n");
	return rval;
}


static inline void
qla24xx_flip_colors(struct qla_hw_data *ha, uint16_t *pflags)
{
	/* Flip all colors. */
	if (ha->beacon_color_state == QLA_LED_ALL_ON) {
		/* Turn off. */
		ha->beacon_color_state = 0;
		*pflags = 0;
	} else {
		/* Turn on. */
		ha->beacon_color_state = QLA_LED_ALL_ON;
		*pflags = GPDX_LED_YELLOW_ON | GPDX_LED_AMBER_ON;
	}
}

void
qla24xx_beacon_blink(struct scsi_qla_host *vha)
{
	uint16_t led_color = 0;
	uint32_t gpio_data;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	/* Save the Original GPIOD. */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	gpio_data = RD_REG_DWORD(&reg->gpiod);

	/* Enable the gpio_data reg for update. */
	gpio_data |= GPDX_LED_UPDATE_MASK;

	WRT_REG_DWORD(&reg->gpiod, gpio_data);
	gpio_data = RD_REG_DWORD(&reg->gpiod);

	/* Set the color bits. */
	qla24xx_flip_colors(ha, &led_color);

	/* Clear out any previously set LED color. */
	gpio_data &= ~GPDX_LED_COLOR_MASK;

	/* Set the new input LED color to GPIOD. */
	gpio_data |= led_color;

	/* Set the modified gpio_data values. */
	WRT_REG_DWORD(&reg->gpiod, gpio_data);
	gpio_data = RD_REG_DWORD(&reg->gpiod);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

int
qla24xx_beacon_on(struct scsi_qla_host *vha)
{
	uint32_t gpio_data;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	if (ha->beacon_blink_led == 0) {
		/* Enable firmware for update */
		ha->fw_options[1] |= ADD_FO1_DISABLE_GPIO_LED_CTRL;

		if (qla2x00_set_fw_options(vha, ha->fw_options) != QLA_SUCCESS)
			return QLA_FUNCTION_FAILED;

		if (qla2x00_get_fw_options(vha, ha->fw_options) !=
		    QLA_SUCCESS) {
			qla_printk(KERN_WARNING, ha,
			    "Unable to update fw options (beacon on).\n");
			return QLA_FUNCTION_FAILED;
		}

		spin_lock_irqsave(&ha->hardware_lock, flags);
		gpio_data = RD_REG_DWORD(&reg->gpiod);

		/* Enable the gpio_data reg for update. */
		gpio_data |= GPDX_LED_UPDATE_MASK;
		WRT_REG_DWORD(&reg->gpiod, gpio_data);
		RD_REG_DWORD(&reg->gpiod);

		spin_unlock_irqrestore(&ha->hardware_lock, flags);
	}

	/* So all colors blink together. */
	ha->beacon_color_state = 0;

	/* Let the per HBA timer kick off the blinking process. */
	ha->beacon_blink_led = 1;

	return QLA_SUCCESS;
}

int
qla24xx_beacon_off(struct scsi_qla_host *vha)
{
	uint32_t gpio_data;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	ha->beacon_blink_led = 0;
	ha->beacon_color_state = QLA_LED_ALL_ON;

	ha->isp_ops->beacon_blink(vha);	/* Will flip to all off. */

	/* Give control back to firmware. */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	gpio_data = RD_REG_DWORD(&reg->gpiod);

	/* Disable the gpio_data reg for update. */
	gpio_data &= ~GPDX_LED_UPDATE_MASK;
	WRT_REG_DWORD(&reg->gpiod, gpio_data);
	RD_REG_DWORD(&reg->gpiod);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	ha->fw_options[1] &= ~ADD_FO1_DISABLE_GPIO_LED_CTRL;

	if (qla2x00_set_fw_options(vha, ha->fw_options) != QLA_SUCCESS) {
		qla_printk(KERN_WARNING, ha,
		    "Unable to update fw options (beacon off).\n");
		return QLA_FUNCTION_FAILED;
	}

	if (qla2x00_get_fw_options(vha, ha->fw_options) != QLA_SUCCESS) {
		qla_printk(KERN_WARNING, ha,
		    "Unable to get fw options (beacon off).\n");
		return QLA_FUNCTION_FAILED;
	}

	return QLA_SUCCESS;
}


/*
 * Flash support routines
 */

/**
 * qla2x00_flash_enable() - Setup flash for reading and writing.
 * @ha: HA context
 */
static void
qla2x00_flash_enable(struct qla_hw_data *ha)
{
	uint16_t data;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	data = RD_REG_WORD(&reg->ctrl_status);
	data |= CSR_FLASH_ENABLE;
	WRT_REG_WORD(&reg->ctrl_status, data);
	RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */
}

/**
 * qla2x00_flash_disable() - Disable flash and allow RISC to run.
 * @ha: HA context
 */
static void
qla2x00_flash_disable(struct qla_hw_data *ha)
{
	uint16_t data;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	data = RD_REG_WORD(&reg->ctrl_status);
	data &= ~(CSR_FLASH_ENABLE);
	WRT_REG_WORD(&reg->ctrl_status, data);
	RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */
}


/**
 * qla2x00_write_flash_byte() - Write a byte to flash
 * @ha: HA context
 * @addr: Address in flash to write
 * @data: Data to write
 */
static void
qla2x00_write_flash_byte(struct qla_hw_data *ha, uint32_t addr, uint8_t data)
{
	uint16_t bank_select;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	bank_select = RD_REG_WORD(&reg->ctrl_status);
	if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
		/* Specify 64K address range: */
		/*  clear out Module Select and Flash Address bits [19:16]. */
		bank_select &= ~0xf8;
		bank_select |= addr >> 12 & 0xf0;
		bank_select |= CSR_FLASH_64K_BANK;
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */

		WRT_REG_WORD(&reg->flash_address, (uint16_t)addr);
		RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */
		WRT_REG_WORD(&reg->flash_data, (uint16_t)data);
		RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */

		return;
	}

	/* Setup bit 16 of flash address. */
	if ((addr & BIT_16) && ((bank_select & CSR_FLASH_64K_BANK) == 0)) {
		bank_select |= CSR_FLASH_64K_BANK;
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */
	} else if (((addr & BIT_16) == 0) &&
	    (bank_select & CSR_FLASH_64K_BANK)) {
		bank_select &= ~(CSR_FLASH_64K_BANK);
		WRT_REG_WORD(&reg->ctrl_status, bank_select);
		RD_REG_WORD(&reg->ctrl_status);	/* PCI Posting. */
	}

	/* Always perform IO mapped accesses to the FLASH registers. */
	if (ha->pio_address) {
		reg = (struct device_reg_2xxx __iomem *)ha->pio_address;
		WRT_REG_WORD_PIO(&reg->flash_address, (uint16_t)addr);
		WRT_REG_WORD_PIO(&reg->flash_data, (uint16_t)data);
	} else {
		WRT_REG_WORD(&reg->flash_address, (uint16_t)addr);
		RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */
		WRT_REG_WORD(&reg->flash_data, (uint16_t)data);
		RD_REG_WORD(&reg->ctrl_status);		/* PCI Posting. */
	}
}

/**
 * qla2x00_poll_flash() - Polls flash for completion.
 * @ha: HA context
 * @addr: Address in flash to poll
 * @poll_data: Data to be polled
 * @man_id: Flash manufacturer ID
 * @flash_id: Flash ID
 *
 * This function polls the device until bit 7 of what is read matches data
 * bit 7 or until data bit 5 becomes a 1.  If that hapens, the flash ROM timed
 * out (a fatal error).  The flash book recommeds reading bit 7 again after
 * reading bit 5 as a 1.
 *
 * Returns 0 on success, else non-zero.
 */
static int
qla2x00_poll_flash(struct qla_hw_data *ha, uint32_t addr, uint8_t poll_data,
    uint8_t man_id, uint8_t flash_id)
{
	int status;
	uint8_t flash_data;
	uint32_t cnt;

	status = 1;

	/* Wait for 30 seconds for command to finish. */
	poll_data &= BIT_7;
	for (cnt = 3000000; cnt; cnt--) {
		flash_data = qla2x00_read_flash_byte(ha, addr);
		if ((flash_data & BIT_7) == poll_data) {
			status = 0;
			break;
		}

		if (man_id != 0x40 && man_id != 0xda) {
			if ((flash_data & BIT_5) && cnt > 2)
				cnt = 2;
		}
		udelay(10);
		barrier();
		cond_resched();
	}
	return status;
}

/**
 * qla2x00_program_flash_address() - Programs a flash address
 * @ha: HA context
 * @addr: Address in flash to program
 * @data: Data to be written in flash
 * @man_id: Flash manufacturer ID
 * @flash_id: Flash ID
 *
 * Returns 0 on success, else non-zero.
 */
static int
qla2x00_program_flash_address(struct qla_hw_data *ha, uint32_t addr, uint8_t data,
    uint8_t man_id, uint8_t flash_id)
{
	/* Write Program Command Sequence. */
	if (IS_OEM_001(ha)) {
		qla2x00_write_flash_byte(ha, 0xaaa, 0xaa);
		qla2x00_write_flash_byte(ha, 0x555, 0x55);
		qla2x00_write_flash_byte(ha, 0xaaa, 0xa0);
		qla2x00_write_flash_byte(ha, addr, data);
	} else {
		if (man_id == 0xda && flash_id == 0xc1) {
			qla2x00_write_flash_byte(ha, addr, data);
			if (addr & 0x7e)
				return 0;
		} else {
			qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
			qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
			qla2x00_write_flash_byte(ha, 0x5555, 0xa0);
			qla2x00_write_flash_byte(ha, addr, data);
		}
	}

	udelay(150);

	/* Wait for write to complete. */
	return qla2x00_poll_flash(ha, addr, data, man_id, flash_id);
}

/**
 * qla2x00_erase_flash() - Erase the flash.
 * @ha: HA context
 * @man_id: Flash manufacturer ID
 * @flash_id: Flash ID
 *
 * Returns 0 on success, else non-zero.
 */
static int
qla2x00_erase_flash(struct qla_hw_data *ha, uint8_t man_id, uint8_t flash_id)
{
	/* Individual Sector Erase Command Sequence */
	if (IS_OEM_001(ha)) {
		qla2x00_write_flash_byte(ha, 0xaaa, 0xaa);
		qla2x00_write_flash_byte(ha, 0x555, 0x55);
		qla2x00_write_flash_byte(ha, 0xaaa, 0x80);
		qla2x00_write_flash_byte(ha, 0xaaa, 0xaa);
		qla2x00_write_flash_byte(ha, 0x555, 0x55);
		qla2x00_write_flash_byte(ha, 0xaaa, 0x10);
	} else {
		qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
		qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
		qla2x00_write_flash_byte(ha, 0x5555, 0x80);
		qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
		qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
		qla2x00_write_flash_byte(ha, 0x5555, 0x10);
	}

	udelay(150);

	/* Wait for erase to complete. */
	return qla2x00_poll_flash(ha, 0x00, 0x80, man_id, flash_id);
}

/**
 * qla2x00_erase_flash_sector() - Erase a flash sector.
 * @ha: HA context
 * @addr: Flash sector to erase
 * @sec_mask: Sector address mask
 * @man_id: Flash manufacturer ID
 * @flash_id: Flash ID
 *
 * Returns 0 on success, else non-zero.
 */
static int
qla2x00_erase_flash_sector(struct qla_hw_data *ha, uint32_t addr,
    uint32_t sec_mask, uint8_t man_id, uint8_t flash_id)
{
	/* Individual Sector Erase Command Sequence */
	qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
	qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
	qla2x00_write_flash_byte(ha, 0x5555, 0x80);
	qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
	qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
	if (man_id == 0x1f && flash_id == 0x13)
		qla2x00_write_flash_byte(ha, addr & sec_mask, 0x10);
	else
		qla2x00_write_flash_byte(ha, addr & sec_mask, 0x30);

	udelay(150);

	/* Wait for erase to complete. */
	return qla2x00_poll_flash(ha, addr, 0x80, man_id, flash_id);
}

/**
 * qla2x00_get_flash_manufacturer() - Read manufacturer ID from flash chip.
 * @man_id: Flash manufacturer ID
 * @flash_id: Flash ID
 */
static void
qla2x00_get_flash_manufacturer(struct qla_hw_data *ha, uint8_t *man_id,
    uint8_t *flash_id)
{
	qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
	qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
	qla2x00_write_flash_byte(ha, 0x5555, 0x90);
	*man_id = qla2x00_read_flash_byte(ha, 0x0000);
	*flash_id = qla2x00_read_flash_byte(ha, 0x0001);
	qla2x00_write_flash_byte(ha, 0x5555, 0xaa);
	qla2x00_write_flash_byte(ha, 0x2aaa, 0x55);
	qla2x00_write_flash_byte(ha, 0x5555, 0xf0);
}


static inline void
qla2x00_suspend_hba(struct scsi_qla_host *vha)
{
	int cnt;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	/* Suspend HBA. */
	scsi_block_requests(vha->host);
	ha->isp_ops->disable_intrs(ha);
	set_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);

	/* Pause RISC. */
	spin_lock_irqsave(&ha->hardware_lock, flags);
	WRT_REG_WORD(&reg->hccr, HCCR_PAUSE_RISC);
	RD_REG_WORD(&reg->hccr);
	if (IS_QLA2100(ha) || IS_QLA2200(ha) || IS_QLA2300(ha)) {
		for (cnt = 0; cnt < 30000; cnt++) {
			if ((RD_REG_WORD(&reg->hccr) & HCCR_RISC_PAUSE) != 0)
				break;
			udelay(100);
		}
	} else {
		udelay(10);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static inline void
qla2x00_resume_hba(struct scsi_qla_host *vha)
{
	struct qla_hw_data *ha = vha->hw;
	/* Resume HBA. */
	clear_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);
	set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
	qla2xxx_wake_dpc(vha);
	qla2x00_wait_for_hba_online(vha);
	scsi_unblock_requests(vha->host);
}

uint8_t *
qla2x00_read_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	uint32_t addr, midpoint;
	uint8_t *data;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	/* Suspend HBA. */
	qla2x00_suspend_hba(vha);

	/* Go with read. */
	midpoint = ha->optrom_size / 2;

	qla2x00_flash_enable(ha);
	WRT_REG_WORD(&reg->nvram, 0);
	RD_REG_WORD(&reg->nvram);		/* PCI Posting. */
	for (addr = offset, data = buf; addr < length; addr++, data++) {
		if (addr == midpoint) {
			WRT_REG_WORD(&reg->nvram, NVR_SELECT);
			RD_REG_WORD(&reg->nvram);	/* PCI Posting. */
		}

		*data = qla2x00_read_flash_byte(ha, addr);
	}
	qla2x00_flash_disable(ha);

	/* Resume HBA. */
	qla2x00_resume_hba(vha);

	return buf;
}

static int
qla23xx_write_flash_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	int	ret = 0;
	uint8_t man_id, flash_id, sec_number, data;
	uint16_t wd;
	uint32_t addr, liter, sec_mask, rest_addr;
	struct qla_hw_data *ha = vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	sec_number = 0;

	/* Reset ISP chip. */
	WRT_REG_WORD(&reg->ctrl_status, CSR_ISP_SOFT_RESET);
	pci_read_config_word(ha->pdev, PCI_COMMAND, &wd);

	/* Go with write. */
	qla2x00_flash_enable(ha);
	do {	/* Loop once to provide quick error exit */
		/* Structure of flash memory based on manufacturer */
		if (IS_OEM_001(ha)) {
			/* OEM variant with special flash part. */
			man_id = flash_id = 0;
			rest_addr = 0xffff;
			sec_mask   = 0x10000;
			goto update_flash;
		}
		qla2x00_get_flash_manufacturer(ha, &man_id, &flash_id);
		switch (man_id) {
		case 0x20: /* ST flash. */
			if (flash_id == 0xd2 || flash_id == 0xe3) {
				/*
				 * ST m29w008at part - 64kb sector size with
				 * 32kb,8kb,8kb,16kb sectors at memory address
				 * 0xf0000.
				 */
				rest_addr = 0xffff;
				sec_mask = 0x10000;
				break;   
			}
			/*
			 * ST m29w010b part - 16kb sector size
			 * Default to 16kb sectors
			 */
			rest_addr = 0x3fff;
			sec_mask = 0x1c000;
			break;
		case 0x40: /* Mostel flash. */
			/* Mostel v29c51001 part - 512 byte sector size. */
			rest_addr = 0x1ff;
			sec_mask = 0x1fe00;
			break;
		case 0xbf: /* SST flash. */
			/* SST39sf10 part - 4kb sector size. */
			rest_addr = 0xfff;
			sec_mask = 0x1f000;
			break;
		case 0xda: /* Winbond flash. */
			/* Winbond W29EE011 part - 256 byte sector size. */
			rest_addr = 0x7f;
			sec_mask = 0x1ff80;
			break;
		case 0xc2: /* Macronix flash. */
			/* 64k sector size. */
			if (flash_id == 0x38 || flash_id == 0x4f) {
				rest_addr = 0xffff;
				sec_mask = 0x10000;
				break;
			}
			/* Fall through... */

		case 0x1f: /* Atmel flash. */
			/* 512k sector size. */
			if (flash_id == 0x13) {
				rest_addr = 0x7fffffff;
				sec_mask =   0x80000000;
				break;
			}
			/* Fall through... */

		case 0x01: /* AMD flash. */
			if (flash_id == 0x38 || flash_id == 0x40 ||
			    flash_id == 0x4f) {
				/* Am29LV081 part - 64kb sector size. */
				/* Am29LV002BT part - 64kb sector size. */
				rest_addr = 0xffff;
				sec_mask = 0x10000;
				break;
			} else if (flash_id == 0x3e) {
				/*
				 * Am29LV008b part - 64kb sector size with
				 * 32kb,8kb,8kb,16kb sector at memory address
				 * h0xf0000.
				 */
				rest_addr = 0xffff;
				sec_mask = 0x10000;
				break;
			} else if (flash_id == 0x20 || flash_id == 0x6e) {
				/*
				 * Am29LV010 part or AM29f010 - 16kb sector
				 * size.
				 */
				rest_addr = 0x3fff;
				sec_mask = 0x1c000;
				break;
			} else if (flash_id == 0x6d) {
				/* Am29LV001 part - 8kb sector size. */
				rest_addr = 0x1fff;
				sec_mask = 0x1e000;
				break;
			}
		default:
			/* Default to 16 kb sector size. */
			rest_addr = 0x3fff;
			sec_mask = 0x1c000;
			break;
		}

update_flash:
		if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
			if (qla2x00_erase_flash(ha, man_id, flash_id)) {
				ret = QLA_FUNCTION_FAILED;
				break;
			}
		}

		for (addr = offset, liter = 0; liter < length; liter++,
		    addr++) {
			data = buf[liter];
			/* Are we at the beginning of a sector? */
			if ((addr & rest_addr) == 0) {
				if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
					if (addr >= 0x10000UL) {
						if (((addr >> 12) & 0xf0) &&
						    ((man_id == 0x01 && flash_id == 0x3e) ||
						     (man_id == 0x20 && flash_id == 0xd2))) {
							sec_number++;
							if (sec_number == 1) {
								rest_addr = 0x7fff;
								sec_mask = 0x18000;
							} else if (sec_number == 2 ||
							    sec_number == 3) {
								rest_addr = 0x1fff;
								sec_mask = 0x1e000;
							} else if (sec_number == 4) {
								rest_addr = 0x3fff;
								sec_mask = 0x1c000;
							}
						}
					}
				} else if (addr == ha->optrom_size / 2) {
					WRT_REG_WORD(&reg->nvram, NVR_SELECT);
					RD_REG_WORD(&reg->nvram);
				}

				if (flash_id == 0xda && man_id == 0xc1) {
					qla2x00_write_flash_byte(ha, 0x5555,
					    0xaa);
					qla2x00_write_flash_byte(ha, 0x2aaa,
					    0x55);
					qla2x00_write_flash_byte(ha, 0x5555,
					    0xa0);
				} else if (!IS_QLA2322(ha) && !IS_QLA6322(ha)) {
					/* Then erase it */
					if (qla2x00_erase_flash_sector(ha,
					    addr, sec_mask, man_id,
					    flash_id)) {
						ret = QLA_FUNCTION_FAILED;
						break;
					}
					if (man_id == 0x01 && flash_id == 0x6d)
						sec_number++;
				}
			}

			if (man_id == 0x01 && flash_id == 0x6d) {
				if (sec_number == 1 &&
				    addr == (rest_addr - 1)) {
					rest_addr = 0x0fff;
					sec_mask   = 0x1f000;
				} else if (sec_number == 3 && (addr & 0x7ffe)) {
					rest_addr = 0x3fff;
					sec_mask   = 0x1c000;
				}
			}

			if (qla2x00_program_flash_address(ha, addr, data,
			    man_id, flash_id)) {
				ret = QLA_FUNCTION_FAILED;
				break;
			}
		}
	} while (0);
	qla2x00_flash_disable(ha);

	return ret;	
}

int
qla2x00_write_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	int rval;
	rval = QLA_SUCCESS;
	/* Suspend HBA. */
	qla2x00_suspend_hba(vha);

	/* Go with write. */
	rval = qla23xx_write_flash_data(vha, buf, offset, length);

	/* Resume HBA. */
	qla2x00_resume_hba(vha);

	return rval;
}

static int
qla24xx_write_flash_data(scsi_qla_host_t *vha, uint32_t *dwptr, uint32_t faddr,
    uint32_t dwords)
{
	int ret;
	uint32_t liter;
	uint32_t sec_mask, rest_addr;
	uint32_t fdata;
	dma_addr_t optrom_dma;
	void *optrom = NULL;
	struct qla_hw_data *ha = vha->hw;

	/* Prepare burst-capable write on supported ISPs. */
	if ((IS_QLA25XX(ha) || IS_QLA81XX(ha)) && !(faddr & 0xfff) &&
	    dwords > OPTROM_BURST_DWORDS) {
		optrom = dma_alloc_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE,
		    &optrom_dma, GFP_KERNEL);
		if (!optrom) {
			qla_printk(KERN_DEBUG, ha,
			    "Unable to allocate memory for optrom burst write "
			    "(%x KB).\n", OPTROM_BURST_SIZE / 1024);
		}
	}

	rest_addr = (ha->fdt_block_size >> 2) - 1;
 	sec_mask = ~rest_addr;

 	ret = qla24xx_unprotect_flash(vha);
 	if (ret != QLA_SUCCESS) {
 		qla_printk(KERN_WARNING, ha,
 		    "Unable to unprotect flash for update.\n");
 		goto done;
 	}

	for (liter = 0; liter < dwords; liter++, faddr++, dwptr++) {
 		fdata = (faddr & sec_mask) << 2;

		/* Are we at the beginning of a sector? */
 		if ((faddr & rest_addr) == 0) {
			/* Do sector unprotect. */
			if (ha->fdt_unprotect_sec_cmd)
				qla24xx_write_flash_dword(ha,
				    ha->fdt_unprotect_sec_cmd,
				    (fdata & 0xff00) | ((fdata << 16) &
				    0xff0000) | ((fdata >> 16) & 0xff));

 			ret = qla24xx_erase_sector(vha, fdata);
			if (ret != QLA_SUCCESS) {
 				DEBUG9(qla_printk(KERN_WARNING, ha,
 				    "Unable to erase sector: address=%x.\n",
 				    faddr));
				break;
			}
		}

		/* Go with burst-write. */
		if (optrom && (liter + OPTROM_BURST_DWORDS) <= dwords) {
			/* Copy data to DMA'ble buffer. */
			memcpy(optrom, dwptr, OPTROM_BURST_SIZE);

			ret = qla2x00_load_ram(vha, optrom_dma,
			    flash_data_addr(ha, faddr),
			    OPTROM_BURST_DWORDS);
			if (ret != QLA_SUCCESS) {
				qla_printk(KERN_WARNING, ha,
				    "Unable to burst-write optrom segment "
				    "(%x/%x/%llx).\n", ret,
				    flash_data_addr(ha, faddr),
				    (unsigned long long)optrom_dma);
				qla_printk(KERN_WARNING, ha,
				    "Reverting to slow-write.\n");

				dma_free_coherent(&ha->pdev->dev,
				    OPTROM_BURST_SIZE, optrom, optrom_dma);
				optrom = NULL;
			} else {
				liter += OPTROM_BURST_DWORDS - 1;
				faddr += OPTROM_BURST_DWORDS - 1;
				dwptr += OPTROM_BURST_DWORDS - 1;
				continue;
			}
		}

		ret = qla24xx_write_flash_dword(ha,
		    flash_data_addr(ha, faddr), cpu_to_le32(*dwptr));
		if (ret != QLA_SUCCESS) {
			DEBUG9(printk("%s(%ld) Unable to program flash "
			    "address=%x data=%x.\n", __func__,
			    vha->host_no, faddr, *dwptr));
			break;
		}

		/* Do sector protect. */
		if (ha->fdt_unprotect_sec_cmd &&
		    ((faddr & rest_addr) == rest_addr))
			qla24xx_write_flash_dword(ha,
			    ha->fdt_protect_sec_cmd,
			    (fdata & 0xff00) | ((fdata << 16) &
			    0xff0000) | ((fdata >> 16) & 0xff));
	}

	ret = qla24xx_protect_flash(vha);
	if (ret != QLA_SUCCESS)
		qla_printk(KERN_WARNING, ha,
		    "Unable to protect flash after update.\n");
done:
	if (optrom)
		dma_free_coherent(&ha->pdev->dev,
		    OPTROM_BURST_SIZE, optrom, optrom_dma);

	return ret;
}

uint8_t *
qla24xx_read_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	struct qla_hw_data *ha = vha->hw;
	/* Suspend HBA. */
	scsi_block_requests(vha->host);
	set_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);

	/* Go with read. */
	qla24xx_read_flash_data(vha, (uint32_t *)buf, offset >> 2,
	    length >> 2);

	/* Resume HBA. */
	clear_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);
	scsi_unblock_requests(vha->host);

	return buf;
}

int
qla24xx_write_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	int rval = 1;
	struct qla_hw_data *ha = vha->hw;

	/* Suspend HBA. */
	scsi_block_requests(vha->host);
	set_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);

	rval = qla24xx_write_flash_data(vha, (uint32_t *)buf,
			offset >> 2, length >> 2);

	clear_bit(MBX_UPDATE_FLASH_ACTIVE, &ha->mbx_cmd_flags);
	scsi_unblock_requests(vha->host);

	return rval;
}

uint8_t *
qla25xx_read_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
    uint32_t offset, uint32_t length)
{
	int rval;
	dma_addr_t optrom_dma;
	void *optrom;
	uint8_t *pbuf;
	uint32_t faddr, left, burst;
	struct qla_hw_data *ha = vha->hw;

	if (offset & 0xfff)
		goto slow_read;
	if (length < OPTROM_BURST_SIZE)
		goto slow_read;

	optrom = dma_alloc_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE,
	    &optrom_dma, GFP_KERNEL);
	if (!optrom) {
		qla_printk(KERN_DEBUG, ha,
		    "Unable to allocate memory for optrom burst read "
		    "(%x KB).\n", OPTROM_BURST_SIZE / 1024);

		goto slow_read;
	}

	pbuf = buf;
	faddr = offset >> 2;
	left = length >> 2;
	burst = OPTROM_BURST_DWORDS;
	while (left != 0) {
		if (burst > left)
			burst = left;

		rval = qla2x00_dump_ram(vha, optrom_dma,
		    flash_data_addr(ha, faddr), burst);
		if (rval) {
			qla_printk(KERN_WARNING, ha,
			    "Unable to burst-read optrom segment "
			    "(%x/%x/%llx).\n", rval,
			    flash_data_addr(ha, faddr),
			    (unsigned long long)optrom_dma);
			qla_printk(KERN_WARNING, ha,
			    "Reverting to slow-read.\n");

			dma_free_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE,
			    optrom, optrom_dma);
			goto slow_read;
		}

		memcpy(pbuf, optrom, burst * 4);

		left -= burst;
		faddr += burst;
		pbuf += burst * 4;
	}

	dma_free_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE, optrom,
	    optrom_dma);

	return buf;

slow_read:
    return qla24xx_read_optrom_data(vha, buf, offset, length);
}

static int
qla25xx_read_serdes(scsi_qla_host_t *vha, uint16_t elt, uint16_t adr,
		uint hibit, uint lobit, uint shift, uint8_t *val)
{
	int rval;
	struct qla_hw_data *ha = vha->hw;
	uint16_t address;
	uint16_t data;

	adr &= 0x3f;
	elt &= 0xf;
	address = (adr << 9) | (elt << 0);

	rval = qla2xxx_read_serdes_word(vha, address, &data);
	if (rval != QLA_SUCCESS)
		qla_printk(KERN_INFO, ha, "%s: Failed to read serdes %#x\n",
				__func__, adr);
	else if (val) {
		data = (data >> lobit) & ((1 << (hibit - lobit + 1)) - 1);

		if (shift == 0)
			*val = data;
		else
			*val |= data << shift;
	}

	return rval;
}

int qla25xx_read_serdes_params(scsi_qla_host_t *vha, serdes_params_t *sp)
{
	int rval = 0;

	memset(sp, 0, sizeof(*sp));

	rval = qla25xx_read_serdes(vha, 9, 0x0, 5, 1, 0, &sp->amp_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x0, 0, 0, 0, &sp->amp_fdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x1, 5, 1, 0, &sp->amp_hdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x1, 0, 0, 0, &sp->amp_hdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x2, 5, 1, 0, &sp->amp_qdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x2, 0, 0, 0, &sp->amp_qdr.en);

	rval |= qla25xx_read_serdes(vha, 9, 0x9, 7, 3, 0, &sp->imain_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x9, 2, 2, 0, &sp->imain_fdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0xa, 7, 3, 0, &sp->imain_hdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0xa, 2, 2, 0, &sp->imain_hdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0xb, 7, 3, 0, &sp->imain_qdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0xb, 2, 2, 0, &sp->imain_qdr.en);

	rval |= qla25xx_read_serdes(vha, 9, 0x6, 4, 1, 0, &sp->ipst_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x6, 0, 0, 0, &sp->ipst_fdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x7, 4, 1, 0, &sp->ipst_hdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x7, 0, 0, 0, &sp->ipst_hdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x8, 4, 1, 0, &sp->ipst_qdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x8, 0, 0, 0, &sp->ipst_qdr.en);

	rval |= qla25xx_read_serdes(vha, 9, 0x6, 7, 6, 0, &sp->ipre_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x9, 1, 0, 2, &sp->ipre_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x6, 5, 5, 0, &sp->ipre_fdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x7, 7, 6, 0, &sp->ipre_hdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0xa, 1, 0, 2, &sp->ipre_hdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x7, 5, 5, 0, &sp->ipre_hdr.en);
	rval |= qla25xx_read_serdes(vha, 9, 0x8, 7, 6, 0, &sp->ipre_qdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0xb, 1, 0, 2, &sp->ipre_qdr.dat);
	rval |= qla25xx_read_serdes(vha, 9, 0x8, 5, 5, 0, &sp->ipre_qdr.en);

	rval |= qla25xx_read_serdes(vha, 7, 0x1e, 3, 0, 0, &sp->z1cnt_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 7, 0x1f, 3, 0, 0, &sp->z1cnt_hdr.dat);

	rval |= qla25xx_read_serdes(vha, 7, 0x1e, 4, 4, 0, &sp->g1cnt_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 7, 0x1f, 4, 4, 0, &sp->g1cnt_hdr.dat);

	rval |= qla25xx_read_serdes(vha, 7, 0x1b, 4, 0, 0, &sp->zcnt_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 7, 0x1c, 4, 0, 0, &sp->zcnt_hdr.dat);

	rval |= qla25xx_read_serdes(vha, 7, 0x08, 5, 0, 0, &sp->dfelth_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 7, 0x21, 5, 0, 0, &sp->dfelth_hdr.dat);

	rval |= qla25xx_read_serdes(vha, 7, 0x09, 5, 0, 0, &sp->tlth_fdr.dat);
	rval |= qla25xx_read_serdes(vha, 7, 0x23, 5, 0, 0, &sp->tlth_hdr.dat);

	return rval;
}

uint32_t *qla82xx_read_flash_data(struct qla_hw_data *ha, uint32_t *dwptr,
		uint32_t faddr, uint32_t length)
{
	uint32_t i;
	uint32_t val;

	/* Dword reads to flash. */
	for (i = 0; i < length/4; i++, faddr += 4) {
		if (qla82xx_rom_fast_read(ha, faddr, &val)) {
			qla_printk(KERN_WARNING, ha, "Do ROM fast read failed\n");
			goto done_read;
		}
		dwptr[i] = __constant_cpu_to_le32(val);
	}

done_read:
	return dwptr;
}

/*
 * Address and length are byte address
 */
uint8_t *qla82xx_read_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
		uint32_t offset, uint32_t length)
{
	struct qla_hw_data *ha = vha->hw;

	scsi_block_requests(vha->host);

	qla82xx_read_flash_data(ha, (uint32_t *)buf, offset, length);

	scsi_unblock_requests(vha->host);

	return buf;
}

static int qla82xx_write_flash_data(scsi_qla_host_t *vha, uint32_t *dwptr,
		uint32_t faddr, uint32_t dwords)
{
	int ret;
	uint32_t liter;
	uint32_t sec_mask, rest_addr;
	dma_addr_t optrom_dma;
	void *optrom = NULL;
	int page_mode = 0;
	struct qla_hw_data *ha = vha->hw;

	ret = -1;

	/* Prepare burst-capable write on supported ISPs. */
	if (page_mode && !(faddr & 0xfff) && dwords > OPTROM_BURST_DWORDS) {
		optrom = dma_alloc_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE,
				&optrom_dma, GFP_KERNEL);
		if (!optrom) {
			qla_printk(KERN_DEBUG, ha,
				"Unable to allocate memory for optrom burst write "
				"(%x KB).\n", OPTROM_BURST_SIZE / 1024);
		}
	}

	rest_addr = ha->fdt_block_size - 1;
	sec_mask = ~rest_addr;

	ret = qla82xx_unprotect_flash(vha);
	if (ret) {
		qla_printk(KERN_WARNING, ha,
			"Unable to unprotect flash for update.\n");
		goto write_done;
	}

	for (liter = 0; liter < dwords; liter++, faddr += 4, dwptr++) {
		/* Are we at the beginning of a sector? */
		if ((faddr & rest_addr) == 0) {
			/* Do sector unprotect. */
			if (ha->fdt_unprotect_sec_cmd)
				qla82xx_unprotect_sector(vha, faddr);
			ret = qla82xx_erase_sector(vha, faddr);
			if (ret) {
				DEBUG9(qla_printk(KERN_ERR, ha,
						"Unable to erase sector: address=%x.\n", faddr));
				break;
			}
		}

		/* Go with burst-write. */
		if (optrom && (liter + OPTROM_BURST_DWORDS) <= dwords) {
			/* Copy data to DMA'ble buffer. */
			memcpy(optrom, dwptr, OPTROM_BURST_SIZE);

			ret = qla2x00_load_ram(vha, optrom_dma,
					flash_data_addr(ha, faddr),
					OPTROM_BURST_DWORDS);
			if (ret != QLA_SUCCESS) {
				qla_printk(KERN_WARNING, ha,
					"Unable to burst-write optrom segment "
					"(%x/%x/%llx).\n", ret,
					flash_data_addr(ha, faddr),
					(unsigned long long)optrom_dma);
				qla_printk(KERN_WARNING, ha, "Reverting to slow-write.\n");

				dma_free_coherent(&ha->pdev->dev,
						OPTROM_BURST_SIZE, optrom, optrom_dma);
				optrom = NULL;
			} else {
				liter += OPTROM_BURST_DWORDS - 1;
				faddr += OPTROM_BURST_DWORDS - 1;
				dwptr += OPTROM_BURST_DWORDS - 1;
				continue;
			}
		}

		ret = qla82xx_write_flash_dword(vha, faddr, cpu_to_le32(*dwptr));
		if (ret) {
			DEBUG9(qla_printk(KERN_DEBUG, ha, "%s(%ld) Unable to program "
					"flash address=%x data=%x.\n", __func__,
					vha->host_no, faddr, *dwptr));
			break;
		}

		/* Do sector protect. */
		if (ha->fdt_unprotect_sec_cmd && ((faddr & rest_addr) == rest_addr))
			qla82xx_protect_sector(vha, faddr);
	}

	ret = qla82xx_protect_flash(vha);
	if (ret)
		qla_printk(KERN_WARNING, ha,
			"Unable to protect flash after update.\n");

write_done:
	if (optrom)
		dma_free_coherent(&ha->pdev->dev, OPTROM_BURST_SIZE,
				optrom, optrom_dma);

	return ret;
}


int qla82xx_write_optrom_data(struct scsi_qla_host *vha, uint8_t *buf,
		uint32_t offset, uint32_t length)
{
	int rval;

	/* Suspend HBA. */
	scsi_block_requests(vha->host);

	/* Go with write. */
	rval = qla82xx_write_flash_data(vha, (uint32_t *)buf, offset,
			length >> 2);

	scsi_unblock_requests(vha->host);

	/* Convert return ISP82xx to generic */
	if (rval)
		rval = QLA_FUNCTION_FAILED;
	else
		rval = QLA_SUCCESS;

	return rval;
}

/**
 * qla2x00_get_fcode_version() - Determine an FCODE image's version.
 * @ha: HA context
 * @pcids: Pointer to the FCODE PCI data structure
 *
 * The process of retrieving the FCODE version information is at best
 * described as interesting.
 *
 * Within the first 100h bytes of the image an ASCII string is present
 * which contains several pieces of information including the FCODE
 * version.  Unfortunately it seems the only reliable way to retrieve
 * the version is by scanning for another sentinel within the string,
 * the FCODE build date:
 *
 *	... 2.00.02 10/17/02 ...
 *
 * Returns QLA_SUCCESS on successful retrieval of version.
 */
static void
qla2x00_get_fcode_version(struct qla_hw_data *ha, uint32_t pcids)
{
	int ret = QLA_FUNCTION_FAILED;
	uint32_t istart, iend, iter, vend;
	uint8_t do_next, rbyte, *vbyte;

	memset(ha->fcode_revision, 0, sizeof(ha->fcode_revision));

	/* Skip the PCI data structure. */
	istart = pcids +
	    ((qla2x00_read_flash_byte(ha, pcids + 0x0B) << 8) |
		qla2x00_read_flash_byte(ha, pcids + 0x0A));
	iend = istart + 0x100;
	do {
		/* Scan for the sentinel date string...eeewww. */
		do_next = 0;
		iter = istart;
		while ((iter < iend) && !do_next) {
			iter++;
			if (qla2x00_read_flash_byte(ha, iter) == '/') {
				if (qla2x00_read_flash_byte(ha, iter + 2) ==
				    '/')
					do_next++;
				else if (qla2x00_read_flash_byte(ha,
				    iter + 3) == '/')
					do_next++;
			}
		}
		if (!do_next)
			break;

		/* Backtrack to previous ' ' (space). */
		do_next = 0;
		while ((iter > istart) && !do_next) {
			iter--;
			if (qla2x00_read_flash_byte(ha, iter) == ' ')
				do_next++;
		}
		if (!do_next)
			break;

		/*
		 * Mark end of version tag, and find previous ' ' (space) or
		 * string length (recent FCODE images -- major hack ahead!!!).
		 */
		vend = iter - 1;
		do_next = 0;
		while ((iter > istart) && !do_next) {
			iter--;
			rbyte = qla2x00_read_flash_byte(ha, iter);
			if (rbyte == ' ' || rbyte == 0xd || rbyte == 0x10)
				do_next++;
		}
		if (!do_next)
			break;

		/* Mark beginning of version tag, and copy data. */
		iter++;
		if ((vend - iter) &&
		    ((vend - iter) < sizeof(ha->fcode_revision))) {
			vbyte = ha->fcode_revision;
			while (iter <= vend) {
				*vbyte++ = qla2x00_read_flash_byte(ha, iter);
				iter++;
			}
			ret = QLA_SUCCESS;
		}
	} while (0);

	if (ret != QLA_SUCCESS)
		memset(ha->fcode_revision, 0, sizeof(ha->fcode_revision));
}

/**
 * qla2x00_get_flash_version() - Read version information from flash.
 * @ha: HA context
 *
 * Returns QLA_SUCCESS on successful retrieval of flash version.
 */
int
qla2x00_get_flash_version(scsi_qla_host_t *vha, void *mbuf)
{
	int ret = QLA_SUCCESS;
	uint8_t code_type, last_image;
	uint32_t pcihdr, pcids;
	uint8_t *dbyte;
	uint16_t *dcode;
	struct qla_hw_data *ha = vha->hw;

	if (!ha->pio_address || !mbuf)
		return QLA_FUNCTION_FAILED;

	memset(ha->bios_revision, 0, sizeof(ha->bios_revision));
	memset(ha->efi_revision, 0, sizeof(ha->efi_revision));
	memset(ha->fcode_revision, 0, sizeof(ha->fcode_revision));
	memset(ha->fw_revision, 0, sizeof(ha->fw_revision));

	qla2x00_flash_enable(ha);

	/* Begin with first PCI expansion ROM header. */
	pcihdr = 0;
	last_image = 1;
	do {
		/* Verify PCI expansion ROM header. */
		if (qla2x00_read_flash_byte(ha, pcihdr) != 0x55 ||
		    qla2x00_read_flash_byte(ha, pcihdr + 0x01) != 0xaa) {
			/* No signature */
			DEBUG2(printk("scsi(%ld): No matching ROM "
			    "signature.\n", vha->host_no));
			ret = QLA_FUNCTION_FAILED;
			break;
		}

		/* Locate PCI data structure. */
		pcids = pcihdr +
		    ((qla2x00_read_flash_byte(ha, pcihdr + 0x19) << 8) |
			qla2x00_read_flash_byte(ha, pcihdr + 0x18));

		/* Validate signature of PCI data structure. */
		if (qla2x00_read_flash_byte(ha, pcids) != 'P' ||
		    qla2x00_read_flash_byte(ha, pcids + 0x1) != 'C' ||
		    qla2x00_read_flash_byte(ha, pcids + 0x2) != 'I' ||
		    qla2x00_read_flash_byte(ha, pcids + 0x3) != 'R') {
			/* Incorrect header. */
			DEBUG2(printk("%s(): PCI data struct not found "
			    "pcir_adr=%x.\n", __func__, pcids));
			ret = QLA_FUNCTION_FAILED;
			break;
		}

		/* Read version */
		code_type = qla2x00_read_flash_byte(ha, pcids + 0x14);
		switch (code_type) {
		case ROM_CODE_TYPE_BIOS:
			/* Intel x86, PC-AT compatible. */
			set_bit(ROM_CODE_TYPE_BIOS, &ha->code_types);
			ha->bios_revision[0] =
			    qla2x00_read_flash_byte(ha, pcids + 0x12);
			ha->bios_revision[1] =
			    qla2x00_read_flash_byte(ha, pcids + 0x13);
			DEBUG3(printk("%s(): read BIOS %d.%d.\n", __func__,
			    ha->bios_revision[1], ha->bios_revision[0]));
			break;
		case ROM_CODE_TYPE_FCODE:
			/* Open Firmware standard for PCI (FCode). */
			/* Eeeewww... */
			set_bit(ROM_CODE_TYPE_FCODE, &ha->code_types);
			qla2x00_get_fcode_version(ha, pcids);
			break;
		case ROM_CODE_TYPE_EFI:
			/* Extensible Firmware Interface (EFI). */
			set_bit(ROM_CODE_TYPE_EFI, &ha->code_types);
			ha->efi_revision[0] =
			    qla2x00_read_flash_byte(ha, pcids + 0x12);
			ha->efi_revision[1] =
			    qla2x00_read_flash_byte(ha, pcids + 0x13);
			DEBUG3(printk("%s(): read EFI %d.%d.\n", __func__,
			    ha->efi_revision[1], ha->efi_revision[0]));
			break;
		default:
			/*
			 * Check if this is OEM HBA, if yes get the serial
			 * number from VPD data.
			 */
			if (IS_OEM_002(ha) &&
			    !qla2x00_get_oem_serial_number(vha, pcids))
				break;
 
			DEBUG2(printk("%s(): Unrecognized code type %x at "
			    "pcids %x.\n", __func__, code_type, pcids));
			break;
		}

		last_image = qla2x00_read_flash_byte(ha, pcids + 0x15) & BIT_7;

		/* Locate next PCI expansion ROM. */
		pcihdr += ((qla2x00_read_flash_byte(ha, pcids + 0x11) << 8) |
		    qla2x00_read_flash_byte(ha, pcids + 0x10)) * 512;
	} while (!last_image);

	if (IS_QLA2322(ha)) {
		/* Read firmware image information. */
		memset(ha->fw_revision, 0, sizeof(ha->fw_revision));
		dbyte = mbuf;
		memset(dbyte, 0, 8);
		dcode = (uint16_t *)dbyte;

		qla2x00_read_flash_data(vha, dbyte, ha->flt_region_fw * 4 + 10,
		    8);
		DEBUG3(printk("%s(%ld): dumping fw ver from flash:\n",
		    __func__, vha->host_no));
		DEBUG3(qla2x00_dump_buffer((uint8_t *)dbyte, 8));

		if ((dcode[0] == 0xffff && dcode[1] == 0xffff &&
		    dcode[2] == 0xffff && dcode[3] == 0xffff) ||
		    (dcode[0] == 0 && dcode[1] == 0 && dcode[2] == 0 &&
		    dcode[3] == 0)) {
			DEBUG2(qla_printk(KERN_INFO, ha, "Unrecognized fw "
			    "revision at %x.\n", ha->flt_region_fw * 4));
		} else {
			/* values are in big endian */
			ha->fw_revision[0] = dbyte[0] << 16 | dbyte[1];
			ha->fw_revision[1] = dbyte[2] << 16 | dbyte[3];
			ha->fw_revision[2] = dbyte[4] << 16 | dbyte[5];
		}
	}

	qla2x00_flash_disable(ha);

	return ret;
}

int
qla24xx_get_flash_version(scsi_qla_host_t *vha, void *mbuf)
{
	int ret = QLA_SUCCESS;
	uint32_t pcihdr, pcids;
	uint32_t *dcode;
	uint8_t *bcode;
	uint8_t code_type, last_image;
	int i;
	struct qla_hw_data *ha = vha->hw;

	if (!mbuf)
		return QLA_FUNCTION_FAILED;

	memset(ha->bios_revision, 0, sizeof(ha->bios_revision));
	memset(ha->efi_revision, 0, sizeof(ha->efi_revision));
	memset(ha->fcode_revision, 0, sizeof(ha->fcode_revision));
	memset(ha->fw_revision, 0, sizeof(ha->fw_revision));

	dcode = mbuf;

	/* Begin with first PCI expansion ROM header. */
	pcihdr = ha->flt_region_boot << 2;
	last_image = 1;
	do {
		/* Verify PCI expansion ROM header. */
		qla24xx_read_flash_data(vha, dcode, pcihdr >> 2, 0x20);
		bcode = mbuf + (pcihdr % 4);
		if (bcode[0x0] != 0x55 || bcode[0x1] != 0xaa) {
			/* No signature */
			DEBUG2(printk("scsi(%ld): No matching ROM "
			    "signature.\n", vha->host_no));
			ret = QLA_FUNCTION_FAILED;
			break;
		}

		/* Locate PCI data structure. */
		pcids = pcihdr + ((bcode[0x19] << 8) | bcode[0x18]);

		qla24xx_read_flash_data(vha, dcode, pcids >> 2, 0x20);
		bcode = mbuf + (pcihdr % 4);

		/* Validate signature of PCI data structure. */
		if (bcode[0x0] != 'P' || bcode[0x1] != 'C' ||
		    bcode[0x2] != 'I' || bcode[0x3] != 'R') {
			/* Incorrect header. */
			DEBUG2(printk("%s(): PCI data struct not found "
			    "pcir_adr=%x.\n", __func__, pcids));
			ret = QLA_FUNCTION_FAILED;
			break;
		}

		/* Read version */
		code_type = bcode[0x14];
		switch (code_type) {
		case ROM_CODE_TYPE_BIOS:
			/* Intel x86, PC-AT compatible. */
			set_bit(ROM_CODE_TYPE_BIOS, &ha->code_types);
			ha->bios_revision[0] = bcode[0x12];
			ha->bios_revision[1] = bcode[0x13];
			DEBUG3(printk("%s(): read BIOS %d.%d.\n", __func__,
			    ha->bios_revision[1], ha->bios_revision[0]));
			break;
		case ROM_CODE_TYPE_FCODE:
			/* Open Firmware standard for PCI (FCode). */
			set_bit(ROM_CODE_TYPE_FCODE, &ha->code_types);
			ha->fcode_revision[0] = bcode[0x12];
			ha->fcode_revision[1] = bcode[0x13];
			DEBUG3(printk("%s(): read FCODE %d.%d.\n", __func__,
			    ha->fcode_revision[1], ha->fcode_revision[0]));
			break;
		case ROM_CODE_TYPE_EFI:
			/* Extensible Firmware Interface (EFI). */
			set_bit(ROM_CODE_TYPE_EFI, &ha->code_types);
			ha->efi_revision[0] = bcode[0x12];
			ha->efi_revision[1] = bcode[0x13];
			DEBUG3(printk("%s(): read EFI %d.%d.\n", __func__,
			    ha->efi_revision[1], ha->efi_revision[0]));
			break;
		default:
			DEBUG2(printk("%s(): Unrecognized code type %x at "
			    "pcids %x.\n", __func__, code_type, pcids));
			break;
		}

		last_image = bcode[0x15] & BIT_7;

		/* Locate next PCI expansion ROM. */
		pcihdr += ((bcode[0x11] << 8) | bcode[0x10]) * 512;
	} while (!last_image);

	/* Read firmware image information. */
	memset(ha->fw_revision, 0, sizeof(ha->fw_revision));
	dcode = mbuf;

	qla24xx_read_flash_data(vha, dcode, ha->flt_region_fw + 4, 4);
	for (i = 0; i < 4; i++)
		dcode[i] = be32_to_cpu(dcode[i]);

	if ((dcode[0] == 0xffffffff && dcode[1] == 0xffffffff &&
	    dcode[2] == 0xffffffff && dcode[3] == 0xffffffff) ||
	    (dcode[0] == 0 && dcode[1] == 0 && dcode[2] == 0 &&
	    dcode[3] == 0)) {
		DEBUG2(printk("%s(): Unrecognized fw version at %x.\n",
		    __func__, ha->flt_region_fw));
	} else {
		ha->fw_revision[0] = dcode[0];
		ha->fw_revision[1] = dcode[1];
		ha->fw_revision[2] = dcode[2];
		ha->fw_revision[3] = dcode[3];
	}

	return ret;
}

int qla82xx_get_flash_version(scsi_qla_host_t *vha, void *mbuf)
{
	struct qla_hw_data *ha = vha->hw;

	/* TODO: Flash FW version needs to be read from flash region.
	 * For now taking from running FW version */
	ha->fw_revision[0] = ha->fw_major_version;
	ha->fw_revision[1] = ha->fw_minor_version;
	ha->fw_revision[2] = ha->fw_subminor_version;

	return QLA_SUCCESS;
}

static int
qla2xxx_is_vpd_valid(uint8_t *pos, uint8_t *end)
{
	if (pos >= end || *pos != 0x82)
		return 0;

	pos += 3 + pos[1];
	if (pos >= end || *pos != 0x90)
		return 0;

	pos += 3 + pos[1];
	if (pos >= end || *pos != 0x78)
		return 0;

	return 1;
}

int
qla2xxx_get_vpd_field(scsi_qla_host_t *vha, char *key, char *str, size_t size)
{
	struct qla_hw_data *ha = vha->hw;
	uint8_t *pos = ha->vpd;
	uint8_t *end = pos + ha->vpd_size;
	int len = 0;

	if (!IS_FWI2_CAPABLE(ha) || !qla2xxx_is_vpd_valid(pos, end))
		return 0;

	while (pos < end && *pos != 0x78) {
		len = (*pos == 0x82) ? pos[1] : pos[2];

		if (!strncmp(pos, key, strlen(key)))
			break;

		if (*pos != 0x90 && *pos != 0x91)
			pos += len;

		pos += 3;
	}

	if (pos < end - len && *pos != 0x78)
		return snprintf(str, size, "%.*s", len, pos + 3);

	return 0;
}

int qla2xxx_read_fcp_prio_cfg(scsi_qla_host_t *vha)
{
	int len, max_len;
	uint32_t fcp_prio_addr;
	struct qla_hw_data *ha = vha->hw;

	if (!ha->fcp_prio_cfg) {
		ha->fcp_prio_cfg = vmalloc(FCP_PRIO_CFG_SIZE);
		if (!ha->fcp_prio_cfg) {
			qla_printk(KERN_WARNING, ha,
			    "Unable to allocate memory for fcp priority data "
			    "(%x).\n", FCP_PRIO_CFG_SIZE);
			return QLA_FUNCTION_FAILED;
		}
	}
	memset(ha->fcp_prio_cfg, 0, FCP_PRIO_CFG_SIZE);

	fcp_prio_addr = ha->flt_region_fcp_prio;

	/* first read the fcp priority data header from flash */
	ha->isp_ops->read_optrom(vha, (uint8_t *)ha->fcp_prio_cfg,
	    fcp_prio_addr << 2, FCP_PRIO_CFG_HDR_SIZE);

	if (!qla2xxx_fcp_prio_cfg_valid(ha->fcp_prio_cfg, 0))
		goto fail;

	/* read remaining FCP CMD config data from flash */
	fcp_prio_addr += (FCP_PRIO_CFG_HDR_SIZE >> 2);
	len = ha->fcp_prio_cfg->num_entries * FCP_PRIO_CFG_ENTRY_SIZE;
	max_len = FCP_PRIO_CFG_SIZE - FCP_PRIO_CFG_HDR_SIZE;

	ha->isp_ops->read_optrom(vha, (uint8_t *)&ha->fcp_prio_cfg->entry[0],
	    fcp_prio_addr << 2, (len < max_len ? len : max_len));

	/* revalidate the entire FCP priority config data, including entries */
	if (!qla2xxx_fcp_prio_cfg_valid(ha->fcp_prio_cfg, 1))
		goto fail;

	vha->flags.fcp_prio_enabled = 1;
	return QLA_SUCCESS;

fail:
	vfree(ha->fcp_prio_cfg);
	ha->fcp_prio_cfg = NULL;

	return QLA_FUNCTION_FAILED;
}

#define ENABLE_BEACONING	0xE
#define DISABLE_BEACONING	0xD
int qla82xx_beacon_on(struct scsi_qla_host *vha)
{
	int rval = QLA_SUCCESS;

	rval = qla82xx_config_led(vha, ENABLE_BEACONING);
	if (rval)
		DEBUG2_3(printk("scsi(%ld): Beaconing could not be enabled.\n", vha->host_no));

	return rval;
}

int qla82xx_beacon_off(struct scsi_qla_host *vha)
{
	int rval = QLA_SUCCESS;

	rval = qla82xx_config_led(vha, DISABLE_BEACONING);
	if (rval)
		DEBUG2_3(printk("scsi(%ld): Beaconing could not be disabled.\n", vha->host_no));

	return rval;
}

void qla82xx_beacon_blink(struct scsi_qla_host *vha)
{
}