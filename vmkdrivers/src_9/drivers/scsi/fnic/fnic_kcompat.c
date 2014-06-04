#include "fnic.h"
#include "fnic_kcompat.h"

#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/kernel.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

/**
 * scsi_dma_map - perform DMA mapping against command's sg lists
 * @cmd:        scsi command
 *
 * Returns the number of sg lists actually used, zero if the sg lists
 * is NULL, or -ENOMEM if the mapping failed.
 */
int scsi_dma_map(struct scsi_cmnd *cmd)
{
	int nseg = 0;

	if (scsi_sg_count(cmd)) {
		struct device *dev = cmd->device->host->shost_gendev.parent;

		nseg = dma_map_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
				  cmd->sc_data_direction);
		if (unlikely(!nseg))
			return -ENOMEM;
	}
	return nseg;
}
#if !defined(__VMKLNX__)
EXPORT_SYMBOL(scsi_dma_map);
#endif /* !defined(__VMKLNX__) */

/**
 * scsi_dma_unmap - unmap command's sg lists mapped by scsi_dma_map
 * @cmd:        scsi command
 */
void scsi_dma_unmap(struct scsi_cmnd *cmd)
{
	if (scsi_sg_count(cmd)) {
		struct device *dev = cmd->device->host->shost_gendev.parent;

		dma_unmap_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
				cmd->sc_data_direction);
        }
}
#if !defined(__VMKLNX__)
EXPORT_SYMBOL(scsi_dma_unmap);
#endif /* !defined(__VMKLNX__) */

/*
 * memmove() implementation taken from vmklinux26/linux/lib/string.c
 */

void *memmove(void *dest, const void *src, size_t count)
{
        char *tmp;
        const char *s;

        if (dest <= src) {
                tmp = dest;
                s = src;
                while (count--)
                        *tmp++ = *s++;
        } else {
                tmp = dest;
                tmp += count;
                s = src;
                s += count;
                while (count--)
                        *--tmp = *--s;
        }
        return dest;
}
#if !defined(__VMKLNX__)
EXPORT_SYMBOL(memmove);
#endif /* !defined(__VMKLNX__) */

void fnic_block_error_handler(struct scsi_cmnd *sc)
{
        struct Scsi_Host *shost = sc->device->host;
        struct fc_rport *rport = starget_to_rport(scsi_target(sc->device));
        unsigned long flags;

        spin_lock_irqsave(shost->host_lock, flags);
        while (rport->port_state == FC_PORTSTATE_BLOCKED) {
                spin_unlock_irqrestore(shost->host_lock, flags);
                msleep(1000);
                spin_lock_irqsave(shost->host_lock, flags);
        }
        spin_unlock_irqrestore(shost->host_lock, flags);
}
