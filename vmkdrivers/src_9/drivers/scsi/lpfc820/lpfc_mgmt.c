/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2011 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
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

#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif

#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_version.h"
#include "fc_fs.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_compat.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_scsi.h"
#include "hbaapi.h"
#include "lpfc_dfc.h"
#include "lpfc_ioctl.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_events.h"
#include "lpfc_mgmt.h"

/**
 * lpfc_mgmt_dma_page_alloc - allocate a mgmt mbox page sized dma buffers
 * @phba: Pointer to HBA context object
 *
 * This function allocates MAILBOX_MGMT_MAX (4KB) page size dma buffer and
 * retruns the pointer to the buffer.
 **/
static struct lpfc_dmabuf *
lpfc_mgmt_dma_page_alloc(struct lpfc_hba *phba)
{
	struct lpfc_dmabuf *dmabuf;
	struct pci_dev *pcidev = phba->pcidev;

	/* allocate dma buffer struct */
	dmabuf = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return NULL;

	INIT_LIST_HEAD(&dmabuf->list);

	/* now, allocate dma buffer */
	dmabuf->virt = dma_alloc_coherent(&pcidev->dev, MAILBOX_MGMT_MAX,
					  &(dmabuf->phys), GFP_KERNEL);

	if (!dmabuf->virt) {
		kfree(dmabuf);
		return NULL;
	}
	memset((uint8_t *)dmabuf->virt, 0, MAILBOX_MGMT_MAX);

	return dmabuf;
}

/**
 * lpfc_mgmt_dma_page_free - free a mgmt mbox page sized dma buffer
 * @phba: Pointer to HBA context object.
 * @dmabuf: Pointer to the mgmt mbox page sized dma buffer descriptor.
 *
 * This routine just simply frees a dma buffer and its associated buffer
 * descriptor referred by @dmabuf.
 **/
static void
lpfc_mgmt_dma_page_free(struct lpfc_hba *phba, struct lpfc_dmabuf *dmabuf)
{
	struct pci_dev *pcidev = phba->pcidev;

	if (!dmabuf)
		return;

	if (dmabuf->virt)
		dma_free_coherent(&pcidev->dev, MAILBOX_MGMT_MAX,
				  dmabuf->virt, dmabuf->phys);
	kfree(dmabuf);
	return;
}

/**
 * lpfc_mgmt_dma_page_list_free - free a list of mgmt mbox page sized dma bufs
 * @phba: Pointer to HBA context object.
 * @dmabuf_list: Pointer to a list of mgmt mbox page sized dma buffer descs.
 *
 * This routine just simply frees all dma buffers and their associated buffer
 * descriptors referred by @dmabuf_list.
 **/
static void
lpfc_mgmt_dma_page_list_free(struct lpfc_hba *phba,
			    struct list_head *dmabuf_list)
{
	struct lpfc_dmabuf *dmabuf, *next_dmabuf;

	if (list_empty(dmabuf_list))
		return;

	list_for_each_entry_safe(dmabuf, next_dmabuf, dmabuf_list, list) {
		list_del_init(&dmabuf->list);
		lpfc_mgmt_dma_page_free(phba, dmabuf);
	}
	return;
}

/**
 * lpfc_mgmt_sli_cfg_dma_desc_setup - set mgmt mbox external dma buffers
 * @phba: Pointer to HBA context object.
 * @memb_typ: Type of mailbox with multiple external buffers.
 * @index: Index to the external buffer descriptor.
 * @mbx_dmabuff: Pointer to the dma buffer contains mailbox command.
 * @ext_dmabuff: Pointer to the dma buffer contains external buffered data.
 *
 * This routine sets up external dam buffer reference for multi-buffer mailbox
 * command.
 **/
static void
lpfc_mgmt_sli_cfg_dma_desc_setup(struct lpfc_hba *phba, enum nemb_type nemb_tp,
				uint32_t index, struct lpfc_dmabuf *mbx_dmabuf,
				struct lpfc_dmabuf *ext_dmabuf)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;

	/* pointer to the start of mailbox command */
	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)mbx_dmabuf->virt;

	if (nemb_tp == nemb_mse) {
		if (index == 0) {
			sli_cfg_mbx->un.sli_config_emb0_subsys.
				mse[index].pa_hi =
				putPaddrHigh(mbx_dmabuf->phys +
					     sizeof(MAILBOX_t));
			sli_cfg_mbx->un.sli_config_emb0_subsys.
				mse[index].pa_lo =
				putPaddrLow(mbx_dmabuf->phys +
					    sizeof(MAILBOX_t));
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"2943 SLI_CONFIG(mse)[%d], "
					"bufLen:%d, addrHi:x%x, addrLo:x%x\n",
					index,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].buf_len,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].pa_hi,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].pa_lo);
		} else {
			sli_cfg_mbx->un.sli_config_emb0_subsys.
				mse[index].pa_hi =
				putPaddrHigh(ext_dmabuf->phys);
			sli_cfg_mbx->un.sli_config_emb0_subsys.
				mse[index].pa_lo =
				putPaddrLow(ext_dmabuf->phys);
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"2944 SLI_CONFIG(mse)[%d], "
					"bufLen:%d, addrHi:x%x, addrLo:x%x\n",
					index,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].buf_len,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].pa_hi,
					sli_cfg_mbx->un.sli_config_emb0_subsys.
					mse[index].pa_lo);
		}
	} else {
		if (index == 0) {
			sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_hi =
				putPaddrHigh(mbx_dmabuf->phys +
					     sizeof(MAILBOX_t));
			sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_lo =
				putPaddrLow(mbx_dmabuf->phys +
					    sizeof(MAILBOX_t));
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"3007 SLI_CONFIG(hbd)[%d], "
					"bufLen:%d, addrHi:x%x, addrLo:x%x\n",
				index,
				mgmt_bf_get(lpfc_mbox_sli_config_ecmn_hbd_len,
				&sli_cfg_mbx->un.
				sli_config_emb1_subsys.hbd[index]),
				sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_hi,
				sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_lo);
		} else {
			sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_hi =
				putPaddrHigh(ext_dmabuf->phys);
			sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_lo =
				putPaddrLow(ext_dmabuf->phys);
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"3046 SLI_CONFIG(hbd)[%d], "
					"bufLen:%d, addrHi:x%x, addrLo:x%x\n",
				index,
				mgmt_bf_get(lpfc_mbox_sli_config_ecmn_hbd_len,
				&sli_cfg_mbx->un.
				sli_config_emb1_subsys.hbd[index]),
				sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_hi,
				sli_cfg_mbx->un.sli_config_emb1_subsys.
				hbd[index].pa_lo);
		}
	}
	return;
}

/**
 * lpfc_mgmt_mbext_to_dma_copy - mbox with external buffer to dma buffer copy
 * @phba: Pointer to HBA context object.
 * @nemb_tp: Enumerate of non-embedded mailbox command type.
 * @index: Index to external buffer descriptor.
 * @offset: Offset into the mailbox and external buffer memory block.
 * @mbxbuf: Pointer to mailbox and external buffer memory block.
 * @dmabuf: Pointer to a DMA buffer descriptor.
 *
 * This routine copies from mailbox and external buffer memory block to dma
 * bufffer provided.
 **/
static int
lpfc_mgmt_mbext_to_dma_copy(struct lpfc_hba *phba, enum nemb_type nemb_tp,
			    int index, int offset, uint8_t *mbxbuf,
			    struct lpfc_dmabuf *dmabuf)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;
	int buf_len;

	/* pointer to the start of mailbox command */
	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)mbxbuf;

	if (nemb_tp == nemb_mse)
		buf_len = sli_cfg_mbx->un.sli_config_emb0_subsys.
			  mse[index].buf_len;
	else
		buf_len = mgmt_bf_get(lpfc_mbox_sli_config_ecmn_hbd_len,
			  &sli_cfg_mbx->un.sli_config_emb1_subsys.hbd[index]);

	if (index == 0)
		buf_len += sizeof(MAILBOX_t);

	memcpy((uint8_t *)dmabuf->virt, (mbxbuf + offset), buf_len);
	offset += buf_len;

	return offset;
}

/**
 * lpfc_mgmt_dma_to_mbext_copy - dma buffer to mbox with external buffer copy
 * @phba: Pointer to HBA context object.
 * @nemb_tp: Enumerate of non-embedded mailbox command type.
 * @index: Index to external buffer descriptor.
 * @offset: Offset into the mailbox and external buffer memory block.
 * @mbxbuf: Pointer to mailbox and external buffer memory block.
 * @dmabuf: Pointer to a DMA buffer descriptor.
 *
 * This routine copies from dma buffer provided to mailbox and external buffer
 * memory block.
 **/
static int
lpfc_mgmt_dma_to_mbext_copy(struct lpfc_hba *phba, enum nemb_type nemb_tp,
			    int index, int offset, uint8_t *mbxbuf,
			    struct lpfc_dmabuf *dmabuf)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;
	int buf_len;

	/* pointer to the start of mailbox command */
	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)mbxbuf;

	if (nemb_tp == nemb_mse)
		buf_len = sli_cfg_mbx->un.sli_config_emb0_subsys.
			  mse[index].buf_len;
	else
		buf_len = mgmt_bf_get(lpfc_mbox_sli_config_ecmn_hbd_len,
			  &sli_cfg_mbx->un.sli_config_emb1_subsys.hbd[index]);

	if (index == 0)
		buf_len += sizeof(MAILBOX_t);

	memcpy((mbxbuf + offset), (uint8_t *)dmabuf->virt, buf_len);
	offset += buf_len;

	return offset;
}

/**
 * lpfc_mgmt_sli_cfg_mse_read_cmd_ext - sli_config non-embedded mbox cmd read
 * @phba: Pointer to HBA context object.
 * @nemb_tp: Enumerate of non-embedded mailbox command type.
 * @sysfs_mbox: Pointer to sysfs mailbox object.
 *
 * This routine performs SLI_CONFIG (0x9B) read mailbox command operation with
 * non-embedded external bufffers.
 **/
static int
lpfc_mgmt_sli_cfg_read_cmd_ext(struct lpfc_hba *phba, enum nemb_type nemb_tp,
			       struct lpfc_sysfs_mbox *sysfs_mbox)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;
	struct lpfc_dmabuf *dmabuf = NULL, *curr_dmabuf, *next_dmabuf;
	uint32_t ext_buf_cnt, ext_buf_index;
	uint32_t shdr_status, shdr_add_status;
	struct lpfc_dmabuf *ext_dmabuf = NULL;
	LPFC_MBOXQ_t *pmboxq = NULL;
	MAILBOX_t *pmb;
	uint32_t tmo;
	uint8_t *pmbx;
	int rc, i, offset;

	/* pointer to the start of mailbox command */
	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)sysfs_mbox->mbext;

	/* initialize additional external read buffer list */
	INIT_LIST_HEAD(&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);

	if (nemb_tp == nemb_mse) {
		ext_buf_cnt = mgmt_bf_get(lpfc_mbox_hdr_mse_cnt,
			&sli_cfg_mbx->un.sli_config_emb0_subsys.sli_config_hdr);
		if (ext_buf_cnt > LPFC_MBX_SLI_CONFIG_MAX_MSE) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"2945 SLI_CONFIG(mse) rd, "
					"ext_buf_cnt(%d) out of range(%d)\n",
					ext_buf_cnt,
					LPFC_MBX_SLI_CONFIG_MAX_MSE);
			rc = -ERANGE;
			goto issue_read_out;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"2941 Handled SLI_CONFIG(mse) rd, "
				"ext_buf_cnt:%d\n", ext_buf_cnt);
	} else {
		/* sanity check on interface type for support */
		if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) !=
		    LPFC_SLI_INTF_IF_TYPE_2) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"3029 SLI_CONFIG(hbd) rd to interface "
					"type:x%x\n",
					bf_get(lpfc_sli_intf_if_type,
					       &phba->sli4_hba.sli_intf));
			rc = -ENODEV;
			goto issue_read_out;
		}
		/* nemb_tp == nemb_hbd */
		ext_buf_cnt = sli_cfg_mbx->un.sli_config_emb1_subsys.hbd_count;
		if (ext_buf_cnt > LPFC_MBX_SLI_CONFIG_MAX_HBD) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"2946 SLI_CONFIG(hbd) rd, "
					"ext_buf_cnt(%d) out of range(%d)\n",
					ext_buf_cnt,
					LPFC_MBX_SLI_CONFIG_MAX_HBD);
			rc = -ERANGE;
			goto issue_read_out;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"2942 Handled SLI_CONFIG(hbd) rd, "
				"ext_buf_cnt:%d\n", ext_buf_cnt);
	}

	/* mailbox command with no external buffer handled by normal path */
	if (ext_buf_cnt == 0) {
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"3038 Not handled SLI_CONFIG(hbd) rd, "
				"with ext_buf_cnt(0)\n");
		return SLI_CONFIG_NOT_HANDLED;
	}

	/* dma buffer for mbox command and first external buffer */
	dmabuf = lpfc_mgmt_dma_page_alloc(phba);
	if (!dmabuf) {
		rc = -ENOMEM;
		goto issue_read_out;
	}

	offset = lpfc_mgmt_mbext_to_dma_copy(phba, nemb_tp, 0, 0,
					     sysfs_mbox->mbext, dmabuf);

	/* additional external read buffers */
	if (ext_buf_cnt > 1) {
		for (i = 1; i < ext_buf_cnt; i++) {
			ext_dmabuf = lpfc_mgmt_dma_page_alloc(phba);
			if (!ext_dmabuf) {
				rc = -ENOMEM;
				goto issue_read_out;
			}
			list_add_tail(&ext_dmabuf->list,
				&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);
		}
	}

	/* mailbox command structure for base driver */
	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq) {
		rc = -ENOMEM;
		goto issue_read_out;
	}
	memset(pmboxq, 0, sizeof(LPFC_MBOXQ_t));

	/* for the first external buffer */
	lpfc_mgmt_sli_cfg_dma_desc_setup(phba, nemb_tp, 0, dmabuf, dmabuf);

	/* for the rest of external buffer descriptors if any */
	if (ext_buf_cnt > 1) {
		ext_buf_index = 1;
		list_for_each_entry_safe(curr_dmabuf, next_dmabuf,
				&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list,
				list) {
			lpfc_mgmt_sli_cfg_dma_desc_setup(phba, nemb_tp,
							 ext_buf_index, dmabuf,
							 curr_dmabuf);
			ext_buf_index++;
		}
	}

	/* construct base driver mbox command */
	pmb = &pmboxq->u.mb;
	pmbx = (uint8_t *)dmabuf->virt;
	memcpy((uint8_t *)pmb, pmbx, sizeof(MAILBOX_t));
	pmb->mbxOwner = OWN_HOST;
	pmboxq->vport = phba->pport;

	/* multi-buffer handling context */
	sysfs_mbox->mbox_ext_buf_ctx.nembType = nemb_tp;
	sysfs_mbox->mbox_ext_buf_ctx.mboxType = mbox_rd;
	sysfs_mbox->mbox_ext_buf_ctx.numBuf = ext_buf_cnt;
	sysfs_mbox->mbox_ext_buf_ctx.mbx_dmabuf = dmabuf;

	/* issue the mailbox command in waiting mode */
	tmo = lpfc_mbox_tmo_val(phba, pmboxq);
	rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, tmo);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		pmboxq = NULL;
		lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
				"2950 Failed SLI_CONFIG(hbd) rd (x%x)\n", rc);
		rc = ((rc == MBX_TIMEOUT) ? -ETIME : -ENODEV);
		goto issue_read_out;
	}

	/* fetch the status and additional status, using the correct hdr */
	if (nemb_tp == nemb_mse) {
		shdr_status = mgmt_bf_get(lpfc_emb0_subcmnd_status,
			&sli_cfg_mbx->un.sli_config_emb0_subsys);
		shdr_add_status = mgmt_bf_get(lpfc_emb0_subcmnd_add_status,
			&sli_cfg_mbx->un.sli_config_emb0_subsys);
	} else {
		shdr_status = mgmt_bf_get(lpfc_emb1_subcmnd_status,
			&sli_cfg_mbx->un.sli_config_emb1_subsys);
		shdr_add_status = mgmt_bf_get(lpfc_emb1_subcmnd_add_status,
			&sli_cfg_mbx->un.sli_config_emb1_subsys);
	}

	if (pmboxq->u.mb.mbxStatus || shdr_status || shdr_add_status)
		lpfc_printf_log(phba, KERN_WARNING, LOG_LIBDFC,
				"3039 SLI_CONFIG(hbd) rd mailbox command "
				"error, mbox_sta:x%x, shdr_sta:x%x, "
				"sdr_add_sta:x%x\n", pmboxq->u.mb.mbxStatus,
				shdr_status, shdr_add_status);
	else
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"3040 SLI_CONFIG(hbd) rd mailbox command "
				"successfual\n");

	/*
	 * outgoing buffer readily referred from the dma buffer, just need
	 * to get header part from mailboxq structure.
	 */
	memcpy(pmbx, pmb, sizeof(MAILBOX_t));

	offset = lpfc_mgmt_dma_to_mbext_copy(phba, nemb_tp, 0, 0,
					     sysfs_mbox->mbext, dmabuf);
	/* get the payload from external buffers */
	if (ext_buf_cnt > 1) {
		ext_buf_index = 1;
		list_for_each_entry_safe(curr_dmabuf, next_dmabuf,
				&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list,
				list) {
			offset = lpfc_mgmt_dma_to_mbext_copy(phba, nemb_tp,
							ext_buf_index, offset,
							sysfs_mbox->mbext,
							curr_dmabuf);
			ext_buf_index++;
		}
	}
	rc = SLI_CONFIG_HANDLED;

issue_read_out:
	if (pmboxq)
		mempool_free(pmboxq, phba->mbox_mem_pool);
	lpfc_mgmt_dma_page_free(phba, dmabuf);
	lpfc_mgmt_dma_page_list_free(phba,
			&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);
	if (rc < 0)
		sysfs_mbox->mbox_ext_buf_ctx.state = LPFC_MGMT_MBOX_ABTS;
	else
		sysfs_mbox->mbox_ext_buf_ctx.state = LPFC_MGMT_MBOX_DONE;
	return rc;
}

/**
 * lpfc_mgmt_sli_cfg_mse_write_cmd_ext - sli_config non-embedded mbox cmd write
 * @phba: Pointer to HBA context object.
 * @nemb_tp: Enumerate of non-embedded mailbox command type.
 * @sysfs_mbox: Pointer to sysfs mailbox object.
 *
 * This routine performs SLI_CONFIG (0x9B) write mailbox command operation with
 * non-embedded external bufffers.
 **/
static int
lpfc_mgmt_sli_cfg_write_cmd_ext(struct lpfc_hba *phba, enum nemb_type nemb_tp,
			       struct lpfc_sysfs_mbox *sysfs_mbox)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;
	struct lpfc_dmabuf *dmabuf = NULL;
	uint32_t shdr_status, shdr_add_status;
	uint32_t ext_buf_cnt;
	struct lpfc_dmabuf *ext_dmabuf = NULL;
	LPFC_MBOXQ_t *pmboxq = NULL;
	MAILBOX_t *pmb;
	uint32_t tmo;
	uint8_t *pmbx;
	int rc, i, offset;

	/* pointer to the start of mailbox command */
	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)sysfs_mbox->mbext;

	/* additional external write buffers */
	INIT_LIST_HEAD(&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);

	if (nemb_tp == nemb_mse) {
		ext_buf_cnt = mgmt_bf_get(lpfc_mbox_hdr_mse_cnt,
			&sli_cfg_mbx->un.sli_config_emb0_subsys.sli_config_hdr);
		if (ext_buf_cnt > LPFC_MBX_SLI_CONFIG_MAX_MSE) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"2953 SLI_CONFIG(mse) wr, "
					"ext_buf_cnt(%d) out of range(%d)\n",
					ext_buf_cnt,
					LPFC_MBX_SLI_CONFIG_MAX_MSE);
			rc = -ERANGE;
			goto issue_write_out;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"2949 Handled SLI_CONFIG(mse) wr, "
				"ext_buf_cnt:%d\n", ext_buf_cnt);
	} else {
		/* sanity check on interface type for support */
		if (bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf) !=
		    LPFC_SLI_INTF_IF_TYPE_2) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"2954 SLI_CONFIG(hbd) wr to interface "
					"type:x%x\n",
					bf_get(lpfc_sli_intf_if_type,
					       &phba->sli4_hba.sli_intf));
			rc = -ENODEV;
			goto issue_write_out;
		}
		/* nemb_tp == nemb_hbd */
		ext_buf_cnt = sli_cfg_mbx->un.sli_config_emb1_subsys.hbd_count;
		if (ext_buf_cnt > LPFC_MBX_SLI_CONFIG_MAX_HBD) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
					"3045 SLI_CONFIG(hbd) wr, "
					"ext_buf_cnt(%d) out of range(%d)\n",
					ext_buf_cnt,
					LPFC_MBX_SLI_CONFIG_MAX_HBD);
			rc = -ERANGE;
			goto issue_write_out;
		}
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"3041 Handled SLI_CONFIG(hbd) wr, "
				"ext_buf_cnt:%d\n", ext_buf_cnt);
	}

	/* write mailbox command with no external buffer not permitted */
	if (ext_buf_cnt == 0) {
		rc = -EPERM;
		goto issue_write_out;
	}

	/* set up dma buffer for mbox command and first external buffer */
	dmabuf = lpfc_mgmt_dma_page_alloc(phba);
	if (!dmabuf) {
		rc = -ENOMEM;
		goto issue_write_out;
	}

	offset = lpfc_mgmt_mbext_to_dma_copy(phba, nemb_tp, 0, 0,
					     sysfs_mbox->mbext, dmabuf);

	lpfc_mgmt_sli_cfg_dma_desc_setup(phba, nemb_tp, 0, dmabuf, dmabuf);

	/* set up additional external write buffers */
	if (ext_buf_cnt > 1) {
		for (i = 1; i < ext_buf_cnt; i++) {
			ext_dmabuf = lpfc_mgmt_dma_page_alloc(phba);
			if (!ext_dmabuf) {
				rc = -ENOMEM;
				goto issue_write_out;
			}
			offset = lpfc_mgmt_mbext_to_dma_copy(phba, nemb_tp,
							     i, offset,
							     sysfs_mbox->mbext,
							     ext_dmabuf);
			lpfc_mgmt_sli_cfg_dma_desc_setup(phba, nemb_tp, i,
							 dmabuf, ext_dmabuf);
			list_add_tail(&ext_dmabuf->list,
				&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);
		}
	}

	/* mailbox command structure for base driver */
	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq) {
		rc = -ENOMEM;
		goto issue_write_out;
	}
	memset(pmboxq, 0, sizeof(LPFC_MBOXQ_t));

	/* construct base driver mbox command */
	pmb = &pmboxq->u.mb;
	pmbx = (uint8_t *)dmabuf->virt;
	memcpy((uint8_t *)pmb, pmbx, sizeof(MAILBOX_t));
	pmb->mbxOwner = OWN_HOST;
	pmboxq->vport = phba->pport;

	/* multi-buffer handling context */
	sysfs_mbox->mbox_ext_buf_ctx.nembType = nemb_tp;
	sysfs_mbox->mbox_ext_buf_ctx.mboxType = mbox_wr;
	sysfs_mbox->mbox_ext_buf_ctx.numBuf = ext_buf_cnt;
	sysfs_mbox->mbox_ext_buf_ctx.mbx_dmabuf = dmabuf;

	/* issue the mailbox command in waiting mode */
	tmo = lpfc_mbox_tmo_val(phba, pmboxq);
	rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, tmo);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		pmboxq = NULL;
		lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
				"3042 Failed SLI_CONFIG(hbd) wr (x%x)\n", rc);
		rc = ((rc == MBX_TIMEOUT) ? -ETIME : -ENODEV);
		goto issue_write_out;
	}

	if (nemb_tp == nemb_mse) {
		shdr_status = mgmt_bf_get(lpfc_emb0_subcmnd_status,
			&sli_cfg_mbx->un.sli_config_emb0_subsys);
		shdr_add_status = mgmt_bf_get(lpfc_emb0_subcmnd_add_status,
			&sli_cfg_mbx->un.sli_config_emb0_subsys);
	} else {
		shdr_status = mgmt_bf_get(lpfc_emb1_subcmnd_status,
			&sli_cfg_mbx->un.sli_config_emb1_subsys);
		shdr_add_status = mgmt_bf_get(lpfc_emb1_subcmnd_add_status,
			&sli_cfg_mbx->un.sli_config_emb1_subsys);
	}
	if (pmboxq->u.mb.mbxStatus || shdr_status || shdr_add_status)
		lpfc_printf_log(phba, KERN_WARNING, LOG_LIBDFC,
				"3043 SLI_CONFIG(hbd) wr mailbox command "
				"error, mbox_sta:x%x, shdr_sta:x%x, "
				"sdr_add_sta:x%x\n", pmboxq->u.mb.mbxStatus,
				shdr_status, shdr_add_status);
	else
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"3044 SLI_CONFIG(hbd) wr mailbox command "
				"successfual\n");

	/* for write, jut get the mailbox command response */
	memcpy(sysfs_mbox->mbext, pmb, sizeof(MAILBOX_t));
	rc = SLI_CONFIG_HANDLED;

issue_write_out:
	if (pmboxq)
		mempool_free(pmboxq, phba->mbox_mem_pool);
	lpfc_mgmt_dma_page_free(phba, dmabuf);
	lpfc_mgmt_dma_page_list_free(phba,
			&sysfs_mbox->mbox_ext_buf_ctx.ext_dmabuf_list);
	if (rc < 0)
		sysfs_mbox->mbox_ext_buf_ctx.state = LPFC_MGMT_MBOX_ABTS;
	else
		sysfs_mbox->mbox_ext_buf_ctx.state = LPFC_MGMT_MBOX_DONE;

	return rc;
}

/**
 * lpfc_mgmt_issue_sli_cfg_ext_mbox - issue sli-cfg mbxcmd with external buffer
 * @phba: Pointer to HBA context object.
 * @sysfs_mbox: Pointer to sysfs mailbox object.
 *
 * This routine issues SLI_CONFIG (0x9B) mailbox command with multiple external
 * buffers to the port and get the response back.
 **/
int
lpfc_mgmt_issue_sli_cfg_ext_mbox(struct lpfc_hba *phba,
				 struct lpfc_sysfs_mbox *sysfs_mbox)
{
	struct lpfc_sli_config_mbox *sli_cfg_mbx;
	uint32_t subsys, opcode;
	int rc = SLI_CONFIG_NOT_HANDLED;

	sli_cfg_mbx = (struct lpfc_sli_config_mbox *)sysfs_mbox->mbext;

	if (!mgmt_bf_get(lpfc_mbox_hdr_emb,
	    &sli_cfg_mbx->un.sli_config_emb0_subsys.sli_config_hdr)) {
		subsys = mgmt_bf_get(lpfc_emb0_subcmnd_subsys,
				    &sli_cfg_mbx->un.sli_config_emb0_subsys);
		opcode = mgmt_bf_get(lpfc_emb0_subcmnd_opcode,
				    &sli_cfg_mbx->un.sli_config_emb0_subsys);
		if (subsys == SLI_CONFIG_SUBSYS_FCOE) {
			switch (opcode) {
			case FCOE_OPCODE_READ_FCF:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2957 Handling SLI_CONFIG "
						"subsys_fcoe, opcode:x%x\n",
						opcode);
				rc = lpfc_mgmt_sli_cfg_read_cmd_ext(phba,
							nemb_mse, sysfs_mbox);
				break;
			case FCOE_OPCODE_ADD_FCF:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2958 Handling SLI_CONFIG "
						"subsys_fcoe, opcode:x%x\n",
						opcode);
				rc = lpfc_mgmt_sli_cfg_write_cmd_ext(phba,
							nemb_mse, sysfs_mbox);
				break;
			default:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2959 Reject SLI_CONFIG "
						"subsys_fcoe, opcode:x%x\n",
						opcode);
				sysfs_mbox->mbox_ext_buf_ctx.state =
							LPFC_MGMT_MBOX_ABTS;
				rc = -EPERM;
				break;
			}
		} else if (subsys == SLI_CONFIG_SUBSYS_COMN) {
			switch (opcode) {
			case COMN_OPCODE_GET_CNTL_ADDL_ATTRIBUTES:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"3108 Handling SLI_CONFIG "
						"subsys_comn, opcode:x%x\n",
						opcode);
				rc = lpfc_mgmt_sli_cfg_read_cmd_ext(phba,
							nemb_mse, sysfs_mbox);
				break;
			default:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"3109 Reject SLI_CONFIG "
						"subsys_comn, opcode:x%x\n",
						opcode);
				sysfs_mbox->mbox_ext_buf_ctx.state =
							LPFC_MGMT_MBOX_ABTS;
				rc = -EPERM;
				break;
			}
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"2977 Reject SLI_CONFIG "
					"subsys:x%d, opcode:x%x\n",
					subsys, opcode);
			sysfs_mbox->mbox_ext_buf_ctx.state =
							LPFC_MGMT_MBOX_ABTS;
			rc = -EPERM;
		}
	} else {
		subsys = mgmt_bf_get(lpfc_emb1_subcmnd_subsys,
				    &sli_cfg_mbx->un.sli_config_emb1_subsys);
		opcode = mgmt_bf_get(lpfc_emb1_subcmnd_opcode,
				    &sli_cfg_mbx->un.sli_config_emb1_subsys);
		if (subsys == SLI_CONFIG_SUBSYS_COMN) {
			switch (opcode) {
			case COMN_OPCODE_READ_OBJECT:
			case COMN_OPCODE_READ_OBJECT_LIST:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2960 Handling SLI_CONFIG "
						"subsys_comn, opcode:x%x\n",
						opcode);
				rc = lpfc_mgmt_sli_cfg_read_cmd_ext(phba,
							nemb_hbd, sysfs_mbox);
				break;
			case COMN_OPCODE_WRITE_OBJECT:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2961 Handling SLI_CONFIG "
						"subsys_comn, opcode:x%x\n",
						opcode);
				rc = lpfc_mgmt_sli_cfg_write_cmd_ext(phba,
							nemb_hbd, sysfs_mbox);
				break;
			default:
				lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
						"2962 Not handled SLI_CONFIG "
						"subsys_comn, opcode:x%x\n",
						opcode);
				break;
			}
		} else
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
					"2978 Not handled SLI_CONFIG "
					"subsys:x%d, opcode:x%x\n",
					subsys, opcode);
	}
	return rc;
}
