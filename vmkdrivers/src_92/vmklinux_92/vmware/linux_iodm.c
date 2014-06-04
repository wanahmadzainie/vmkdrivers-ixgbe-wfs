/* ****************************************************************
 * Portions Copyright 2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/types.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/libfc.h>
#include <vmklinux_9/vmklinux_iodm.h>

#include "linux_scsi_transport.h"
#include "linux_stubs.h"
#include "linux_scsi.h"
#include "linux_iodm.h"

#include "vmklinux_log.h"

/*
 * externs
 */
extern struct list_head linuxSCSIAdapterList;
extern vmk_SpinlockIRQ linuxSCSIAdapterLock;

static vmk_ModuleID iodmModID = VMK_INVALID_MODULE_ID;
static vmk_CharDev iodmCharHandle = VMK_INVALID_CHARDEV;

static VMK_ReturnStatus iodmCharUnregCleanup(vmk_AddrCookie devicePrivate);
static VMK_ReturnStatus iodmCharOpsOpen(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus iodmCharOpsClose(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus iodmCharOpsIoctl(vmk_CharDevFdAttr *attr,
                                        unsigned int cmd,
                                        vmk_uintptr_t userData,
                                        vmk_IoctlCallerSize callerSize,
                                        vmk_int32 *result);

#define isSupportedXportType(flag)		\
   (flag & (VMKLNX_SCSI_TRANSPORT_TYPE_SAS |	\
      VMKLNX_SCSI_TRANSPORT_TYPE_FC  |	\
      VMKLNX_SCSI_TRANSPORT_TYPE_FCOE)	\
   )

#define shost_to_eventbuf(sh)     \
   (sh->adapter ?							    \
      (((struct vmklnx_ScsiAdapterInt *)sh->adapter)->iodmEventBuf) :  \
      NULL								    \
   )

#define event_cnt(buf)     \
   (buf->flags & IODM_EVENTBUF_FULL ? IODM_EVENT_LIMIT : atomic_read(&buf->cur))

/* Only log events or warnings less often than every 'gap' seconds */
#define log_ok(ts, gap) (jiffies - ts > gap * HZ)

static vmk_CharDevOps iodmCharOps = {
   iodmCharOpsOpen,
   iodmCharOpsClose,
   iodmCharOpsIoctl,
   NULL,
   NULL
};

const char * iodm_event_str[] = {
   "IOERROR",     /* 0 */
   "RSCN",        /* 1 */
   "LINKUP",      /* 2 */
   "LINKDOWN",    /* 3 */
   "FRAMEDROP",   /* 4 */
   "LUNRESET",    /* 5 */
   "FCOE_CVL",    /* 6 */
};


/*
 *----------------------------------------------------------------------
 *
 * LinuxIODM_Init
 *      Main entry point from vmklinux init
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      none.
 *----------------------------------------------------------------------
 */
void
LinuxIODM_Init(void)
{
   iodmModID = vmklinuxModID;

   if (vmk_CharDevRegister(iodmModID, IODM_CHAR_NAME,
                           &iodmCharOps, iodmCharUnregCleanup, 0,
                           &iodmCharHandle) != VMK_OK) {
      VMKLNX_WARN("failed to register iodm char dev");
   }

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxIODM_Cleanup
 *	Clean up entry point
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      none.
 *----------------------------------------------------------------------
 */
void
LinuxIODM_Cleanup(void)
{
   vmk_CharDevUnregister(iodmCharHandle);
}

/*
 *----------------------------------------------------------------------
 *
 * iodmCharUnregCleanup
 *
 *	Clean up any device-private data registered to the iodm char
 *      device.  (Currently, the device does not use device-private
 *      data).
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmCharUnregCleanup(vmk_AddrCookie devicePrivate)
{
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * iodmCharOpsOpen
 *	Open the char dev for management
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Takes reference on vmklinux
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmCharOpsOpen(vmk_CharDevFdAttr *attr)
{
   vmk_WorldAssertIsSafeToBlock();

   return vmk_ModuleIncUseCount(iodmModID) == VMK_OK ? 0 : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * iodmCharOpsClose
 *	Closes the char dev for management
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Removes the reference taken on vmklinux
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmCharOpsClose(vmk_CharDevFdAttr *attr)
{
   VMK_ReturnStatus vmkRet = vmk_ModuleDecUseCount(iodmModID);
   VMK_ASSERT(vmkRet == VMK_OK);
   return vmkRet;
}

/*
 *-----------------------------------------------------------------------------
 *  getShostByVmhbaName
 *	Get Scsi_Host by vmhba name
 *
 *  Side Effects:
 *	This funciton increases the Scsi_Host ref count, caller needs to call
 *	scsi_host_put when done with Scsi_Host.
 *-----------------------------------------------------------------------------
 */
static inline struct Scsi_Host *
getShostByVmhbaName(char *vmhbaName)
{
  struct vmklnx_ScsiAdapter *hba;
  unsigned long vmkFlag;
  struct Scsi_Host *shost = NULL;

  vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
  list_for_each_entry(hba, &linuxSCSIAdapterList, entry) {
      if (!strncmp(vmhbaName, vmklnx_get_vmhba_name(hba->shost), MAX_IODM_NAMELEN)) {
         shost = scsi_host_get(hba->shost);
         break;
      }
   }
   vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

   return shost;
}

/*
 *-----------------------------------------------------------------------------
 *  iodmXportType
 *	Translate vmklinux transport type to IODM transport type
 *-----------------------------------------------------------------------------
 */
static inline unsigned long
iodmXportType(unsigned long flag)
{
   switch (flag) {
      case VMKLNX_SCSI_TRANSPORT_TYPE_SAS:
         return IODM_TRANSPORT_TYPE_SAS;
      case VMKLNX_SCSI_TRANSPORT_TYPE_FC:
         return IODM_TRANSPORT_TYPE_FC;
      case VMKLNX_SCSI_TRANSPORT_TYPE_FCOE:
         return IODM_TRANSPORT_TYPE_FCOE;
      case VMKLNX_SCSI_TRANSPORT_TYPE_ISCSI:
         return IODM_TRANSPORT_TYPE_ISCSI;
      default:
         return IODM_TRANSPORT_TYPE_UNKNOWN;
   }
}

/*
 *-----------------------------------------------------------------------------
 *  iodmGetDevices
 *	Get all scsi adapters with transport type FC/FCoe/SAS/iSCSI, which is
 *	currently supported by IODM
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmGetDevices(struct iodm_ioc *ioc)
{
  struct vmklnx_ScsiAdapter *hba;
  struct iodm_list *list = (struct iodm_list *)ioc->data;
  int count = 0;
  unsigned long vmkFlag;

  vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
  list_for_each_entry(hba, &linuxSCSIAdapterList, entry) {
      if (!isSupportedXportType(hba->shost->xportFlags))
         continue;
      list->dev_name[count].transport = iodmXportType(hba->shost->xportFlags);
      strncpy(list->dev_name[count].vmhba_name,
              vmklnx_get_vmhba_name(hba->shost), MAX_IODM_NAMELEN);
      strncpy(list->dev_name[count++].driver_name,
              hba->vmkAdapter->driverName.string, MAX_IODM_NAMELEN);
  }
  vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

  list->count = count;

  return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 * iodmFcTransport
 *	This function handles all IODM commands for FC transport
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmFcTransport(struct iodm_ioc *ioc, struct Scsi_Host *sh)
{
   struct fc_internal *i;
   struct fc_function_template *ft;

   if ((struct vmklnx_ScsiModule *)sh->transportt->module == NULL) {
       VMKLNX_WARN("FC transport not registered for %s", ioc->vmhba_name);
       return VMK_INVALID_HANDLE;
   }
   /* Both FC and FCoE register FC transport function template */
   i = to_fc_internal(sh->transportt);
   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      return VMK_NOT_FOUND;
   }
   ioc->status = IODM_TRANSPORT_TYPE_FC;
   switch (ioc->cmd) {
      case IODM_IOCTL_GET_HOST_STATS: {
         struct fc_host_statistics *inStats;
         struct fchost_statistics *outStats;

         outStats = (struct fchost_statistics*)ioc->data;

         if (ft->get_fc_host_stats == NULL) {
            return VMK_UNDEFINED_VMKCALL;
         }
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh),
                            inStats, ft->get_fc_host_stats, sh);
         if (!inStats) {
            return VMK_FAILURE;
         }
         outStats->tx_frames = inStats->tx_frames;
         outStats->rx_frames = inStats->rx_frames;
         outStats->lip_count = inStats->lip_count;
         outStats->error_frames = inStats->error_frames;
         outStats->dumped_frames = inStats->dumped_frames;
         outStats->link_failure_count = inStats->link_failure_count;
         outStats->loss_of_signal_count = inStats->loss_of_signal_count;
         outStats->prim_seq_protocol_err_count = inStats->prim_seq_protocol_err_count;
         outStats->invalid_tx_word_count = inStats->invalid_tx_word_count;
         outStats->invalid_crc_count = inStats->invalid_crc_count;
         outStats->fcp_input_requests = inStats->fcp_input_requests;
         outStats->fcp_output_requests = inStats->fcp_output_requests;
         outStats->fcp_control_requests = inStats->fcp_control_requests;
         ioc->len = sizeof(struct fchost_statistics);
         break;
      }
      case IODM_IOCTL_GET_HOST_ATTR: {
         iodm_fc_attrs_t *attr = (iodm_fc_attrs_t *) ioc->data;
         struct fc_host_attrs *fc_host = shost_to_fc_host(sh);

         if (ft->get_host_port_id) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->get_host_port_id, sh);
         }
         if (ft->get_host_speed) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->get_host_speed, sh);
         }
         attr->port_id = fc_host->port_id;
         attr->node_name = fc_host->node_name;
         attr->port_name = fc_host->port_name;
         attr->speed = fc_host->speed;
         attr->port_type = fc_host->port_type;
         attr->port_state = fc_host->port_state;
         ioc->len = sizeof(iodm_fc_attrs_t);
         VMKLNX_DEBUG(1, "FC ATTR: %s pid %X wwnn 0x%llX wwpn 0x%llX speed %X"
                      " type %d state %d", ioc->vmhba_name, attr->port_id,
                      attr->node_name, attr->port_name, attr->speed,
                      attr->port_type, attr->port_state);
         break;
      }
      case IODM_IOCTL_ISSUE_RESET:
         if (ft->issue_fc_host_lip) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->issue_fc_host_lip, sh);
	    break;
         } else {
            return VMK_NOT_SUPPORTED;
         }
      default:
         ioc->status = IODM_CMD_NOT_SUPPORTED;
   }

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 * iodmFcoeTransport
 *	This function handles all IODM commands for FCoE transport
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmFcoeTransport(struct iodm_ioc *ioc, struct Scsi_Host *sh)
{
   struct fc_internal *i;
   struct fc_function_template *ft;

   if ((struct vmklnx_ScsiModule *)sh->transportt->module == NULL) {
      VMKLNX_WARN("FCoE transport not registered for %s", ioc->vmhba_name);
      return VMK_INVALID_HANDLE;
   }

   /* Both FC and FCoE register FC transport function template */
   i = to_fc_internal(sh->transportt);
   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      return VMK_NOT_FOUND;
   }
   ioc->status = IODM_TRANSPORT_TYPE_FCOE;
   switch (ioc->cmd) {
      case IODM_IOCTL_GET_HOST_STATS: {
         struct fc_lport *lport = shost_priv(sh);
         struct fcoedev_stats *outStats;
         unsigned int cpu;

         if (lport == NULL) {
            VMKLNX_DEBUG(0, "fcoe null lport %s", ioc->vmhba_name);
            return VMK_NOT_FOUND;
         }
         outStats = (struct fcoedev_stats *)(ioc->data);
         memset(outStats, 0, sizeof(*outStats));

         // gather per cpu fcoe dev stats
         for_each_possible_cpu(cpu) {
            struct fcoe_dev_stats *stats;
            stats = FCOE_PER_CPU_PTR(lport->dev_stats, cpu,
                                     sizeof(struct fcoe_dev_stats));

            outStats->TxFrames += stats->TxFrames;
            outStats->RxFrames += stats->RxFrames;
            outStats->ErrorFrames += stats->ErrorFrames;
            outStats->DumpedFrames += stats->DumpedFrames;
            outStats->LinkFailureCount += stats->LinkFailureCount;
            outStats->LossOfSignalCount += stats->LossOfSignalCount;
            outStats->InvalidTxWordCount += stats->InvalidTxWordCount;
            outStats->InvalidCRCCount += stats->InvalidCRCCount;
            outStats->VLinkFailureCount += stats->VLinkFailureCount;
            outStats->MissDiscAdvCount += stats->MissDiscAdvCount;

            outStats->InputRequests += stats->InputRequests;
            outStats->OutputRequests += stats->OutputRequests;
            outStats->ControlRequests += stats->ControlRequests;
         }

         ioc->len = sizeof(struct fcoedev_stats);
            break;
      }
      case IODM_IOCTL_GET_HOST_ATTR: {
         iodm_fcoe_attrs_t *outAttr = (iodm_fcoe_attrs_t *) ioc->data;
         struct fc_lport *lport = shost_priv(sh);
         struct fc_host_attrs *inAttr = shost_to_fc_host(sh);
         struct vmk_FcoeAdapterAttrs *fcoe_attr;
         if (lport == NULL) {
            VMKLNX_DEBUG(0, "fcoe null lport %s", ioc->vmhba_name);
            return VMK_NOT_FOUND;
         }

         outAttr->port_id = fc_host_port_id(lport->host);
         outAttr->node_name = lport->wwnn;
         outAttr->port_name = lport->wwpn;
         outAttr->speed = lport->link_speed;
         if (ft->get_host_port_state) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->get_host_port_state, sh);
            outAttr->port_state = inAttr->port_state;
         }
         if (ft->get_host_port_type) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->get_host_port_type, sh);
            outAttr->port_type = inAttr->port_type;
         }

         VMKLNX_DEBUG(1, "FCoE ATTR: %s pid %X  wwnn 0x%llX wwpn 0x%llX speed %X"
                      " type %d state %d", ioc->vmhba_name, outAttr->port_id,
                      outAttr->node_name, outAttr->port_name, outAttr->speed,
                      outAttr->port_type, outAttr->port_state);
         fcoe_attr = (struct vmk_FcoeAdapterAttrs *)inAttr->cna_ops;
            if (fcoe_attr) {
               VMKLNX_DEBUG(0, "setting FCoE attributes");
               strncpy(outAttr->vmnicName, fcoe_attr->vmnicName, VMK_DEVICE_NAME_MAX_LENGTH);
               outAttr->vlanId = fcoe_attr->vlanId;
               memcpy(outAttr->fcoeContlrMacAddr, fcoe_attr->fcoeContlrMacAddr,
                  VMK_MAX_ETHERNET_MAC_LENGTH);
               memcpy(outAttr->vnPortMacAddr, fcoe_attr->vnPortMacAddr,
                  VMK_MAX_ETHERNET_MAC_LENGTH);
               memcpy(outAttr->fcfMacAddr, fcoe_attr->fcfMacAddr, VMK_MAX_ETHERNET_MAC_LENGTH);
         }
         ioc->len = sizeof(iodm_fcoe_attrs_t);
         break;
      }
      case IODM_IOCTL_ISSUE_RESET:
         if (ft->issue_fc_host_lip) {
            VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), ft->issue_fc_host_lip, sh);
            break;
         } else {
            return VMK_NOT_SUPPORTED;
         }
      default:
         ioc->status = IODM_CMD_NOT_SUPPORTED;
   }

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 * collect_sas_phy_stats
 *	Helper function to go through all sas_phys on a scsi_host, and collects
 *	all the error stats
 *-----------------------------------------------------------------------------
 */
static int
collect_sas_phy_stats(struct device *dev, void *data)
{
   iodm_sas_stats_t *stats = (iodm_sas_stats_t *)data;
   struct Scsi_Host *sh = dev_to_shost(dev);
   struct sas_function_template *ft;
   struct sas_internal *i = to_sas_internal(sh->transportt);
   int error = 0;

   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      VMKLNX_DEBUG(0, "SAS transport function not registered");
      return (-ENOSYS);
   }
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);

      if (ft->get_linkerrors) {
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), error, ft->get_linkerrors, phy);
         if (error)
            return error;
      }
      VMKLNX_DEBUG(1, "Phy #: %d, SAS address: 0x%llX PHY ID: %d "
                   "linkrate: %d min %d max %d",
                   phy->number,
                   phy->identify.sas_address,
                   phy->identify.phy_identifier,
                   phy->negotiated_linkrate,
                   phy->minimum_linkrate,
                   phy->maximum_linkrate);
      stats->invalid_dword_count += phy->invalid_dword_count;
      stats->running_disparity_error_count += phy->running_disparity_error_count;
      stats->loss_of_dword_sync_count += phy->loss_of_dword_sync_count;
      stats->phy_reset_problem_count += phy->phy_reset_problem_count;
   }
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 * sas_phy_reset
 *	Helper function to reset the link of sys_phy
 *-----------------------------------------------------------------------------
 */
static int
sas_phy_reset(struct device *dev, void *data)
{
   struct Scsi_Host *sh = dev_to_shost(dev);
   struct sas_function_template *ft;
   struct sas_internal *i = to_sas_internal(sh->transportt);
   int error = 0;

   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      VMKLNX_DEBUG(0, "SAS transport function not registered");
      return (-ENOSYS);
   }
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);

      if (ft->phy_reset) {
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), error, ft->phy_reset, phy, 0);
         if (error)
            return error;
      }
   }
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * find_initiator_sas_phy
 *	Helper function to find SAS_PHY from scsi_host. The SAS PHY
 *	attributes we are gathering are same across the vmhba.
 *-----------------------------------------------------------------------------
 */
static int
find_initiator_sas_phy(struct device *dev, void *data)
{
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);
      struct sas_phy **phyPtr = (struct sas_phy **)data;
      *phyPtr = phy;
   }
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 * iodmSasTransport
 *	This function handles all IODM commands for SAS transport
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmSasTransport(struct iodm_ioc *ioc, struct Scsi_Host *sh)
{
   int ret = 0;

   if ((struct vmklnx_ScsiModule *)sh->transportt->module == NULL) {
      return VMK_INVALID_HANDLE;
   }
   ioc->status = IODM_TRANSPORT_TYPE_SAS;
   switch (ioc->cmd) {
      case IODM_IOCTL_GET_HOST_STATS: {
         iodm_sas_stats_t *stats = (iodm_sas_stats_t *) ioc->data;

         /* sas_phy_alloc set up sas_phy parent dev as shost_gendev */
         ret = device_for_each_child(&sh->shost_gendev, (void *)stats,
                                     collect_sas_phy_stats);
         if (ret)
            return VMK_EXEC_FAILURE;
         ioc->len = sizeof(iodm_sas_stats_t);
         break;
         }
      case IODM_IOCTL_GET_HOST_ATTR: {
         iodm_sas_attrs_t *outAttr = (iodm_sas_attrs_t *)ioc->data;
         struct sas_phy *phy = NULL;

         /* sas_phy_alloc set up sas_phy parent dev as shost_gendev */
         device_for_each_child(&sh->shost_gendev, (void *)&phy,
                               find_initiator_sas_phy);
         if (NULL == phy) {
            VMKLNX_DEBUG(0, "Can not get sas_phy from scsi_host");
            ioc->len = 0;
            return VMK_OK;
         }
         outAttr->sas_address = phy->identify.sas_address;
         outAttr->phy_identifier = phy->identify.phy_identifier;
         outAttr->negotiated_linkrate = phy->negotiated_linkrate;
         outAttr->minimum_linkrate = phy->minimum_linkrate;
         outAttr->maximum_linkrate = phy->maximum_linkrate;
         ioc->len = sizeof(iodm_sas_attrs_t);
         break;
      }
      case IODM_IOCTL_ISSUE_RESET:
         ret = device_for_each_child(&sh->shost_gendev, NULL, sas_phy_reset);
         if (ret) {
            VMKLNX_DEBUG(0, "SAS PHY reset failed on %s", ioc->vmhba_name);
            return VMK_EXEC_FAILURE;
         } else {
            VMKLNX_DEBUG(0, "SAS PHY reset succeeded on %s", ioc->vmhba_name);
         }
         break;
      default:
         ioc->status = IODM_CMD_NOT_SUPPORTED;
      }

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 * iodmGetEvents
 *	This function sends the event list to the user space
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmGetEvents(struct iodm_ioc *ioc, struct Scsi_Host *sh)
{
   iodm_ScsiEventBuf_t *buf;
   uint32_t kcnt, cnt, cur;
   uint32_t eventsz = sizeof(iodm_ScsiEvent_t);


   buf = shost_to_eventbuf(sh);
   if (!buf) {
      return VMK_NOT_READY;
   }

   if (ioc->len < 0 || ioc->len > IODM_EVENT_LIMIT) {
      VMKLNX_WARN("%s Invalid IODM event count %d", ioc->vmhba_name, ioc->len);
      return VMK_BAD_PARAM;
   }
   cur = atomic_read(&buf->cur);
   kcnt = event_cnt(buf);
   VMKLNX_DEBUG(0, "Total events for %s: get %d from %d", ioc->vmhba_name,
                ioc->len, kcnt);
   /* ioc->len is the count of pre-allocated events memory in user space */
   if (buf->flags & IODM_EVENTBUF_FULL) {
      /* we need to copy buf in two pieces in this case */
      uint32_t sz1, sz2;
      sz1 = IODM_EVENT_LIMIT - cur;
      sz2 = cur;
      if (sz1 > ioc->len) {
         memcpy(ioc->data, &buf->event[cur],  ioc->len * eventsz);
         ioc->status = ioc->len;
      } else {
         memcpy(ioc->data, &buf->event[cur],  sz1 * eventsz);
         if (sz2 > ioc->len - sz1) {
            memcpy(ioc->data + sz1 * eventsz, buf->event,  (ioc->len - sz1) * eventsz);
            ioc->status = ioc->len;
         } else {
            memcpy(ioc->data + sz1 * eventsz, buf->event, sz2 * eventsz);
            ioc->status = IODM_EVENT_LIMIT;
         }
      }
   } else { /* eventbuf not full */
      cnt =  min(kcnt, ioc->len);
      memcpy(ioc->data, buf->event, cnt * eventsz);
      ioc->status = cnt;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 * iodmCharOpsIoctl
 *      Handles communication with UW daemon, which parses for
 *      packet content.
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side Effects:
 *      none.
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
iodmCharOpsIoctl(vmk_CharDevFdAttr *attr,
                unsigned int cmd,
                vmk_uintptr_t userData,
                vmk_IoctlCallerSize callerSize,
                vmk_int32 *result)
{
   struct iodm_ioc *ioc;
   struct Scsi_Host *sh = NULL;
   VMK_ReturnStatus status;
   int ioc_size = 0, usr_ioc_size = 0;

   vmk_WorldAssertIsSafeToBlock();
   
   if (cmd == IODM_IOCTL_GET_EVENTS) {
      int events_size = IODM_EVENT_LIMIT * sizeof(struct iodm_ScsiEvent);
      ioc_size = events_size + sizeof(*ioc) - MAX_IODM_IOCTL_CMD_SIZE;
   } else {
      ioc_size = usr_ioc_size = sizeof(*ioc);
   }
   ioc = (struct iodm_ioc *)kzalloc(ioc_size, GFP_KERNEL);

   if (!ioc) {
      return VMK_NO_MEMORY;
   }

   status = vmk_CopyFromUser((vmk_VA)ioc, (vmk_VA)userData, sizeof (*ioc));

   if (status != VMK_OK) {
      VMKLNX_WARN("Failed to copy iodm ioctl msg");
      goto iodm_ioctl_err;
   }
   if (cmd != ioc->cmd) {
      VMKLNX_WARN("received invalid iodm ioctl 0x%x", ioc->cmd);
      goto iodm_ioctl_err;
   }

   /* We don't need vmhba name for GET_IODMDEVS command */
   if (ioc->cmd == IODM_IOCTL_GET_IODMDEVS) {
      iodmGetDevices(ioc);
      goto done;
   }

   /*
    * If the vmhbaName is valid, go retrive the shost
    */
   if (ioc->vmhba_name[0]) {
      /* scsi_host_get is called inside getShostByVmhbaName */
      sh = getShostByVmhbaName(ioc->vmhba_name);
      if (!sh) {
         VMKLNX_WARN("Can not get shost for %s", ioc->vmhba_name);
         status = VMK_NOT_FOUND;
         goto iodm_ioctl_err;
      }
   } else {
      VMKLNX_WARN("Null vmhbaName %s", ioc->vmhba_name);
      status = VMK_INVALID_NAME;
      goto iodm_ioctl_err;
   }

   switch (ioc->cmd) {
      case IODM_IOCTL_GET_HOST_STATS:
      case IODM_IOCTL_GET_HOST_ATTR:
      case IODM_IOCTL_ISSUE_RESET: {
         if (sh->xportFlags == VMKLNX_SCSI_TRANSPORT_TYPE_FC) {
            status = iodmFcTransport(ioc, sh);
         } else if (sh->xportFlags == VMKLNX_SCSI_TRANSPORT_TYPE_FCOE) {
            status = iodmFcoeTransport(ioc, sh);
         } else if (sh->xportFlags == VMKLNX_SCSI_TRANSPORT_TYPE_SAS) {
            status = iodmSasTransport(ioc, sh);
         } else {
            status = VMK_NOT_SUPPORTED;
            goto iodm_ioctl_err;
         }
         if (status != VMK_OK) {
            goto iodm_ioctl_err;
         }
         break;
      }

      case IODM_IOCTL_GET_EVENTCNT: {
         iodm_ScsiEventBuf_t *buf = shost_to_eventbuf(sh);
         if (!buf) {
            status = VMK_NOT_READY;
            goto iodm_ioctl_err;
         }
         ioc->status = event_cnt(buf);
         break;
      }

      /*
       * User should retrieve event count first and allocate enough memory
       * for all the events from this ioctl
       */
      case IODM_IOCTL_GET_EVENTS: {
         usr_ioc_size = ioc->len * sizeof(struct iodm_ScsiEvent) +
                        sizeof(struct iodm_ioc) - MAX_IODM_IOCTL_CMD_SIZE;
         status = iodmGetEvents(ioc, sh);
         if (status != VMK_OK)
            goto iodm_ioctl_err;
         break;
      }

      case IODM_IOCTL_CLEAR_EVENTS: {
         iodm_ScsiEventBuf_t *buf = shost_to_eventbuf(sh);
         if (!buf) {
            status = VMK_NOT_READY;
            goto iodm_ioctl_err;
         }
         memset(buf->event, 0, IODM_EVENT_LIMIT * sizeof(iodm_ScsiEvent_t));
         ioc->status = 0;
         atomic_set(&buf->cur, 0);
         buf->flags &= ~IODM_EVENTBUF_FULL;
         break;
      }
  /* NOTE:  uncomment for event testing.
   *  case 99: {
   *     vmklnx_iodm_event(sh, IODM_LINKDOWN, NULL, 0);
   *     break;
   *  }
   */
   
     case 99: {
        vmklnx_iodm_event(sh, IODM_LINKDOWN, NULL, 0);
        break;
     }
   
      default:
         status = VMK_NOT_SUPPORTED;
         goto iodm_ioctl_err;
   }

done:
   status = vmk_CopyToUser((vmk_VA)userData, (vmk_VA)ioc, usr_ioc_size);
   if (status != VMK_OK) {
      VMKLNX_WARN("Failed to copy iodm ioctl response %x", cmd);
   }

iodm_ioctl_err:
   kfree(ioc);
   if (sh)
      scsi_host_put(sh);

   return status;
}

/*-----------------------------------------------------------------------------
 *  iodmEventCount-return the count of the event during a specified period of time
 *  @buf: pinter to the event buffer
 *  @id: an envent id
 *  @duration: most recent time duration in seconds for this event count
 *
 *  RETURN VALUE
 *  Number of the event happened during the most recent duration
 *-----------------------------------------------------------------------------
 */
uint32_t
iodmEventCount(iodm_ScsiEventBuf_t *buf, uint32_t id, uint32_t duration)
{
   int i, cur, iteration;
   uint32_t cnt = 0;
   iodm_ScsiEvent_t *t = NULL;
   int all = event_cnt(buf);   /* total number of events */

   cur = atomic_read(&buf->cur);
   /*
    * buf->cur already advanced by 1 when this function is called, so starts
    * from cur - 1
    */
   i = cur - 1;
   for (iteration = 0; iteration < all; iteration++) {
      if (i < 0)
         i = IODM_EVENT_LIMIT - 1;
      t = &buf->event[i];
      if (t->id == 0)
         break;
      /* break out if the event is not within the duration */
      if (t->sec < buf->event[cur - 1].sec - duration)
         break;
      if (id == t->id)
         cnt++;
      i--;
   }

   return cnt;
}

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_event - add an iodm event to event pool
 *  @sh: a pointer to scsi_host struct
 *  @id: an envent id
 *  @addr: a pointer related to this event
 *  @data: data that relates to this event
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_event(struct Scsi_Host *sh, unsigned int id, void *addr,
                       unsigned long data)
{
   iodm_ScsiEventBuf_t *buf;
   struct timeval ts;
   iodm_ScsiEvent_t *t = NULL;
   int cur, pre;

   if (sh == NULL)
      return;
   buf = shost_to_eventbuf(sh);
   if (!buf) {
      /* vmk_ScsiAdapter might not have been allocated at this point */
      VMKLNX_DEBUG(1, "No event buffer");
      return;
   }
   cur = atomic_read(&buf->cur);
   pre = (cur == 0) ? IODM_EVENT_LIMIT - 1 : cur - 1;
   if (log_ok(buf->log_ts, 1) || id != buf->event[pre].id) {
      if (id < IODM_MAX_EVENTID) 
         VMKLNX_DEBUG(0, "%s event %s to index %d", buf->vmhba_name,
                      iodm_event_str[id], cur);
      else
         VMKLNX_DEBUG(0, "%s event %d to index %d", buf->vmhba_name, id, cur);
      buf->log_ts = jiffies;
   }
   t = &buf->event[cur];
   if (cur == IODM_EVENT_LIMIT - 1) {
      atomic_set(&buf->cur, 0);  /* circle back */
      buf->flags |= IODM_EVENTBUF_FULL;
   } else {
      atomic_inc(&buf->cur);
   }
   t->id = id;
   do_gettimeofday(&ts);  /* get time stamp */
   t->sec = ts.tv_sec;
   t->usec = ts.tv_usec;
   switch (id) {
      case IODM_LINKDOWN: {
         uint32_t cnt = iodmEventCount(buf, IODM_LINKDOWN, 60);
         /* log a warning if link was down >10 time during 1 minute */
         if (cnt > 10 && log_ok(buf->warn_ts, 10)) {
            VMKLNX_WARN("%s: LINK DOWN %d times in 60s, unstable link.",
                        buf->vmhba_name, cnt);
            buf->warn_ts = jiffies;
         }
         break;
      }
      case IODM_FRAMEDROP: {
         uint32_t cnt;
         struct scsi_cmnd *scmd;

         if (addr == NULL) {
            VMKLNX_DEBUG(0, "NULL scsi_cmnd for IODM_FRAMEDROP on %s",
                         buf->vmhba_name);
            return;
         }
         scmd = (struct scsi_cmnd *)addr;
         t->cmd = scmd->cmnd[0];
         t->channel = scmd->device->channel;
         t->target = scmd->device->id;
         t->lun = scmd->device->lun;
         t->data[0] = (uint32_t)data;
         t->data[1] = scsi_bufflen(scmd);
         cnt = iodmEventCount(buf, IODM_FRAMEDROP, 60);
         /* log a warning if more than 5 framedrop events during 1 minute */
         if (cnt > 5 && log_ok(buf->warn_ts, 10)) {
            VMKLNX_WARN("%s: Frame Dropped %d times in 60s, SAN connection check required.",
                        buf->vmhba_name, cnt);
            buf->warn_ts = jiffies;
         }
         break;
      }
      case IODM_RSCN: {
         t->data[0] = (uint32_t)data;
         break;
      }
      case IODM_FCOE_CVL: {
         t->data[0] = (uint32_t)data;
         break;
      }
      default:
         /* log vendor-defined event ID info */
         *(unsigned long *)&t->data[0] = (unsigned long)addr;
         *(unsigned long *)&t->data[2] = data;
         break;
   }
}
EXPORT_SYMBOL(vmklnx_iodm_event);

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_enable_events - Enable iodm scsi events
 *  @sh: a pointer to scsi_host struct of the calling HBA
 *
 *  Enable iodm scsi envents tracking analysis for the calling HBA
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_enable_events(struct Scsi_Host *sh)
{
   iodm_ScsiEventBuf_t *eventBuf;

   VMK_ASSERT(sh);
   if (!sh->adapter) {
      VMKLNX_WARN("This func should be called after scsi_add_host!");
      return;
   }
   eventBuf = (struct iodm_ScsiEventBuf *)kzalloc(sizeof(*eventBuf), GFP_KERNEL);
   if (!eventBuf) {
      VMKLNX_WARN("Event buf allocation failed for %s", sh->hostt->name);
      return;
   }

   eventBuf->shost = sh;
   strncpy(eventBuf->vmhba_name, vmklnx_get_vmhba_name(sh), MAX_IODM_NAMELEN);
   if (sh->adapter) {
      ((struct vmklnx_ScsiAdapterInt *)sh->adapter)->iodmEventBuf = eventBuf;
   } else {
      VMKLNX_DEBUG(1, "Scsi_Host->adapter not set for %s", vmklnx_get_vmhba_name(sh));
   }

   VMKLNX_DEBUG(0, "Enabled IODM event for %s", vmklnx_get_vmhba_name(sh));
}
EXPORT_SYMBOL(vmklnx_iodm_enable_events);

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_disable_events - Disable iodm scsi events
 *  @sh: a pointer to scsi_host struct of the calling HBA
 *
 *  Disable iodm scsi envents for the calling HBA and dealloc event buffer
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_disable_events(struct Scsi_Host *sh)
{
   iodm_ScsiEventBuf_t *buf = shost_to_eventbuf(sh);

   if (buf) {
      kfree(buf);
      ((struct vmklnx_ScsiAdapterInt *)sh->adapter)->iodmEventBuf = NULL;
   } else {
      VMKLNX_DEBUG(1, "event buf was not allocated for %s", vmklnx_get_vmhba_name(sh));
   }

   return;
}
EXPORT_SYMBOL(vmklnx_iodm_disable_events);
