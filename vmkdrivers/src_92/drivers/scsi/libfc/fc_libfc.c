/*
 * Copyright(c) 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#include <linux/kernel.h>
#include <linux/types.h>
#if defined(__VMKLNX__)
#include <linux/stringify.h>
#endif /* defined(__VMKLNX__) */
#include <linux/scatterlist.h>
#include <linux/crc32.h>

#include <scsi/libfc.h>

#include "fc_libfc.h"

MODULE_AUTHOR("Open-FCoE.org");
MODULE_DESCRIPTION("libfc");
MODULE_LICENSE("GPLv2");
#if defined(__VMKLNX__)
MODULE_VERSION("1.0.40.9.2-6vmw")
#endif /* defined(__VMKLNX__) */

unsigned int fc_debug_logging;
module_param_named(debug_logging, fc_debug_logging, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug_logging, "a bit mask of logging levels");
#if defined(__VMKLNX__)
/* libfc log level */
#define LIBFC_MODULE_NAME       "libfc"
vmk_LogComponent libfcLog;
#endif /* defined(__VMKLNX__) */

/**
 * libfc_init() - Initialize libfc.ko
 */
static int __init libfc_init(void)
{
#if defined(__VMKLNX__)
	VMK_ReturnStatus vmkStat;
	vmk_LogProperties logProps;
#endif /* defined(__VMKLNX__) */
	int rc = 0;

	rc = fc_setup_fcp();
	if (rc)
		return rc;

	rc = fc_setup_exch_mgr();
	if (rc)
		goto destroy_pkt_cache;

	rc = fc_setup_rport();
	if (rc)
		goto destroy_em;
#if defined(__VMKLNX__)
	vmkStat = vmk_NameInitialize(&logProps.name, LIBFC_MODULE_NAME);
	VMK_ASSERT(vmkStat == VMK_OK);
	logProps.module = vmklnx_this_module_id;
	logProps.heap = vmk_ModuleGetHeapID(vmklnx_this_module_id);
	logProps.defaultLevel = 0;
	logProps.throttle = NULL;
	vmkStat = vmk_LogRegister(&logProps, &libfcLog);
	if (vmkStat != VMK_OK) {
		printk(KERN_ERR "Libfc vmk_LogRegister failed: %s.\n",
			vmk_StatusToString(vmkStat));
		goto destroy_em;
	}
	vmk_LogSetCurrentLogLevel(libfcLog, fc_debug_logging);
#endif /* defined(__VMKLNX__) */

	return rc;
destroy_em:
	fc_destroy_exch_mgr();
destroy_pkt_cache:
	fc_destroy_fcp();
	return rc;
}
module_init(libfc_init);

/**
 * libfc_exit() - Tear down libfc.ko
 */
static void __exit libfc_exit(void)
{
	fc_destroy_fcp();
	fc_destroy_exch_mgr();
	fc_destroy_rport();
#if defined(__VMKLNX__)
	vmk_LogUnregister(libfcLog);
#endif /* defined(__VMKLNX__) */
}
module_exit(libfc_exit);

/**
 * fc_copy_buffer_to_sglist() - This routine copies the data of a buffer
 *				into a scatter-gather list (SG list).
 *
 * @buf: pointer to the data buffer.
 * @len: the byte-length of the data buffer.
 * @sg: pointer to the pointer of the SG list.
 * @nents: pointer to the remaining number of entries in the SG list.
 * @offset: pointer to the current offset in the SG list.
 * @km_type: dedicated page table slot type for kmap_atomic.
 * @crc: pointer to the 32-bit crc value.
 *	 If crc is NULL, CRC is not calculated.
 */
u32 fc_copy_buffer_to_sglist(void *buf, size_t len,
			     struct scatterlist *sg,
			     u32 *nents, size_t *offset,
			     enum km_type km_type, u32 *crc)
{
	size_t remaining = len;
	u32 copy_len = 0;

	while (remaining > 0 && sg) {
		size_t off, sg_bytes;
		void *page_addr;
#if defined(__VMKLNX__)
		if (*offset >= sg_dma_len(sg)) {
			/*
			 * Check for end and drop resources
			 * from the last iteration.
			 */
			if (!(*nents))
				break;
			--(*nents);
			*offset -= sg_dma_len(sg);
			sg = sg_next(sg);
			continue;
		}
		sg_bytes = min(remaining, sg_dma_len(sg) - *offset);

		/*
		 * The scatterlist item may be bigger than PAGE_SIZE,
		 * but we are limited to mapping PAGE_SIZE at a time.
		 */
		off = *offset + vmklnx_sg_offset(sg);
		sg_bytes = min(sg_bytes,
			       (size_t)(PAGE_SIZE - (off & ~PAGE_MASK)));
		page_addr = kmap_atomic(nth_page(sg_page(sg), (off >> PAGE_SHIFT)),
		                        km_type);
#else /* !defined(__VMKLNX__) */
		if (*offset >= sg->length) {
			/*
			 * Check for end and drop resources
			 * from the last iteration.
			 */
			if (!(*nents))
				break;
			--(*nents);
			*offset -= sg->length;
			sg = sg_next(sg);
			continue;
		}
		sg_bytes = min(remaining, sg->length - *offset);

		/*
		 * The scatterlist item may be bigger than PAGE_SIZE,
		 * but we are limited to mapping PAGE_SIZE at a time.
		 */
		off = *offset + sg->offset;
		sg_bytes = min(sg_bytes,
			       (size_t)(PAGE_SIZE - (off & ~PAGE_MASK)));
		page_addr = kmap_atomic(sg_page(sg) + (off >> PAGE_SHIFT),
					km_type);
#endif /* !defined(__VMKLNX__) */
		if (crc)
			*crc = crc32(*crc, buf, sg_bytes);
		memcpy((char *)page_addr + (off & ~PAGE_MASK), buf, sg_bytes);
		kunmap_atomic(page_addr, km_type);
		buf += sg_bytes;
		*offset += sg_bytes;
		remaining -= sg_bytes;
		copy_len += sg_bytes;
	}
#if defined(__VMKLNX__)
	sg_reset(sg);
#endif /* defined(__VMKLNX__) */
	return copy_len;
}
