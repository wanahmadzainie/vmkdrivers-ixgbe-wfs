/* ****************************************************************
 * Copyright 2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include "vmkapi.h"
#include "vmklinux_version.h"
#include <linux/module.h>
/* NOTE: __namespace.h is generated by the build from the driver's .sc file. */
#include "__namespace.h"

#ifndef MODULE
#error "You can only compile and link vmklinux_module with modules, which" \
       "means that MODULE has to be defined when compiling it..."
#endif

VMK_LICENSE_INFO(VMK_MODULE_LICENSE_GPLV2);
MODULE_VERSION2(VMKLNX_STRINGIFY(PRODUCT_VERSION), VMKLNX_MY_NAMESPACE_VERSION);

/*
 * All vmkdriver modules are built using the latest vmakpi interface.
 */
VMK_NAMESPACE_REQUIRED(VMK_NAMESPACE_VMKAPI, VMK_NAMESPACE_CURRENT_VERSION);

vmk_ModuleID vmkshim_module_id;

int
vmk_early_init_module(void)
{
   VMK_ReturnStatus vmk_status;

   vmk_status = vmk_ModuleRegister(&vmkshim_module_id, VMKAPI_REVISION);
   if (vmk_status != VMK_OK) {
      vmk_WarningMessage("Registration failed (%#x): %s",
                          vmk_status, vmk_StatusToString(vmk_status));
      return vmk_status;
   }

   return 0;
}

int
vmk_late_cleanup_module(void)
{
   return 0;
}

/*
 * Symbols passed in directly from libata without the need for a shim function.
 */
VMK_MODULE_EXPORT_ALIAS(ata_altstatus);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_drive_eh);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_error_handler);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_freeze);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_irq_clear);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_post_internal_cmd);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_setup);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_start);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_status);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_stop);
VMK_MODULE_EXPORT_ALIAS(ata_bmdma_thaw);
VMK_MODULE_EXPORT_ALIAS(ata_bus_reset);
VMK_MODULE_EXPORT_ALIAS(ata_busy_sleep);
VMK_MODULE_EXPORT_ALIAS(ata_cable_40wire);
VMK_MODULE_EXPORT_ALIAS(ata_cable_80wire);
VMK_MODULE_EXPORT_ALIAS(ata_cable_ignore);
VMK_MODULE_EXPORT_ALIAS(ata_cable_unknown);
VMK_MODULE_EXPORT_ALIAS(ata_check_status);
VMK_MODULE_EXPORT_ALIAS(ata_data_xfer);
VMK_MODULE_EXPORT_ALIAS(ata_data_xfer_noirq);
VMK_MODULE_EXPORT_ALIAS(ata_dev_classify);
VMK_MODULE_EXPORT_ALIAS(ata_dev_pair);
VMK_MODULE_EXPORT_ALIAS(ata_do_eh);
VMK_MODULE_EXPORT_ALIAS(ata_do_set_mode);
VMK_MODULE_EXPORT_ALIAS(ata_dumb_qc_prep);
VMK_MODULE_EXPORT_ALIAS(ata_dummy_port_info);
VMK_MODULE_EXPORT_ALIAS(ata_dummy_port_ops);
VMK_MODULE_EXPORT_ALIAS(ata_eh_freeze_port);
VMK_MODULE_EXPORT_ALIAS(ata_ehi_clear_desc);
VMK_MODULE_EXPORT_ALIAS(__ata_ehi_push_desc);
VMK_MODULE_EXPORT_ALIAS(ata_ehi_push_desc);
VMK_MODULE_EXPORT_ALIAS(ata_exec_command);
VMK_MODULE_EXPORT_ALIAS(ata_host_activate);
VMK_MODULE_EXPORT_ALIAS(ata_host_alloc_pinfo);
VMK_MODULE_EXPORT_ALIAS(ata_host_init);
VMK_MODULE_EXPORT_ALIAS(ata_host_intr);
VMK_MODULE_EXPORT_ALIAS(ata_hsm_move);
VMK_MODULE_EXPORT_ALIAS(ata_id_c_string);
VMK_MODULE_EXPORT_ALIAS(ata_id_string);
VMK_MODULE_EXPORT_ALIAS(ata_interrupt);
VMK_MODULE_EXPORT_ALIAS(ata_irq_on);
VMK_MODULE_EXPORT_ALIAS(ata_kfree);
VMK_MODULE_EXPORT_ALIAS(ata_link_abort);
VMK_MODULE_EXPORT_ALIAS(ata_link_offline);
VMK_MODULE_EXPORT_ALIAS(ata_link_online);
VMK_MODULE_EXPORT_ALIAS(ata_noop_dev_select);
VMK_MODULE_EXPORT_ALIAS(ata_noop_qc_prep);
VMK_MODULE_EXPORT_ALIAS(ata_pack_xfermask);
VMK_MODULE_EXPORT_ALIAS(ata_pci_activate_sff_host);
VMK_MODULE_EXPORT_ALIAS(ata_pci_clear_simplex);
VMK_MODULE_EXPORT_ALIAS(ata_pci_default_filter);
VMK_MODULE_EXPORT_ALIAS(ata_pci_init_bmdma);
VMK_MODULE_EXPORT_ALIAS(ata_pci_init_one);
VMK_MODULE_EXPORT_ALIAS(ata_pci_init_sff_host);
VMK_MODULE_EXPORT_ALIAS(ata_pci_prepare_sff_host);
VMK_MODULE_EXPORT_ALIAS(ata_pci_remove_one);
VMK_MODULE_EXPORT_ALIAS(ata_pio_need_iordy);
VMK_MODULE_EXPORT_ALIAS(ata_port_abort);
VMK_MODULE_EXPORT_ALIAS(ata_port_disable);
VMK_MODULE_EXPORT_ALIAS(ata_port_freeze);
VMK_MODULE_EXPORT_ALIAS(ata_port_pbar_desc);
VMK_MODULE_EXPORT_ALIAS(ata_port_probe);
VMK_MODULE_EXPORT_ALIAS(ata_port_start);
VMK_MODULE_EXPORT_ALIAS(ata_qc_complete);
VMK_MODULE_EXPORT_ALIAS(ata_qc_complete_multiple);
VMK_MODULE_EXPORT_ALIAS(ata_qc_issue_prot);
VMK_MODULE_EXPORT_ALIAS(ata_qc_prep);
VMK_MODULE_EXPORT_ALIAS(ata_ratelimit);
VMK_MODULE_EXPORT_ALIAS(ata_sas_port_alloc);
VMK_MODULE_EXPORT_ALIAS(ata_sas_port_destroy);
VMK_MODULE_EXPORT_ALIAS(ata_sas_port_init);
VMK_MODULE_EXPORT_ALIAS(ata_sas_port_start);
VMK_MODULE_EXPORT_ALIAS(ata_sas_port_stop);
VMK_MODULE_EXPORT_ALIAS(ata_sas_queuecmd);
VMK_MODULE_EXPORT_ALIAS(ata_sas_slave_configure);
VMK_MODULE_EXPORT_ALIAS(ata_scsi_change_queue_depth);
VMK_MODULE_EXPORT_ALIAS(ata_scsi_ioctl);
VMK_MODULE_EXPORT_ALIAS(ata_scsi_queuecmd);
VMK_MODULE_EXPORT_ALIAS(ata_scsi_slave_config);
VMK_MODULE_EXPORT_ALIAS(ata_scsi_slave_destroy);
VMK_MODULE_EXPORT_ALIAS(ata_sff_port_start);
VMK_MODULE_EXPORT_ALIAS(ata_sg_init);
VMK_MODULE_EXPORT_ALIAS(ata_std_bios_param);
VMK_MODULE_EXPORT_ALIAS(ata_std_dev_select);
VMK_MODULE_EXPORT_ALIAS(ata_std_ports);
VMK_MODULE_EXPORT_ALIAS(ata_std_postreset);
VMK_MODULE_EXPORT_ALIAS(ata_std_prereset);
VMK_MODULE_EXPORT_ALIAS(ata_std_qc_defer);
VMK_MODULE_EXPORT_ALIAS(ata_std_softreset);
VMK_MODULE_EXPORT_ALIAS(ata_tf_from_fis);
VMK_MODULE_EXPORT_ALIAS(ata_tf_load);
VMK_MODULE_EXPORT_ALIAS(ata_tf_read);
VMK_MODULE_EXPORT_ALIAS(ata_tf_to_fis);
VMK_MODULE_EXPORT_ALIAS(ata_timing_compute);
VMK_MODULE_EXPORT_ALIAS(ata_timing_merge);
VMK_MODULE_EXPORT_ALIAS(ata_wait_after_reset);
VMK_MODULE_EXPORT_ALIAS(ata_wait_ready);
VMK_MODULE_EXPORT_ALIAS(ata_wait_register);
VMK_MODULE_EXPORT_ALIAS(class_device_attr_link_power_management_policy);
VMK_MODULE_EXPORT_ALIAS(pci_test_config_bits);
VMK_MODULE_EXPORT_ALIAS(sata_async_notification);
VMK_MODULE_EXPORT_ALIAS(sata_deb_timing_hotplug);
VMK_MODULE_EXPORT_ALIAS(sata_deb_timing_long);
VMK_MODULE_EXPORT_ALIAS(sata_deb_timing_normal);
VMK_MODULE_EXPORT_ALIAS(sata_link_debounce);
VMK_MODULE_EXPORT_ALIAS(sata_link_hardreset);
VMK_MODULE_EXPORT_ALIAS(sata_pmp_do_eh);
VMK_MODULE_EXPORT_ALIAS(sata_pmp_qc_defer_cmd_switch);
VMK_MODULE_EXPORT_ALIAS(sata_pmp_std_hardreset);
VMK_MODULE_EXPORT_ALIAS(sata_pmp_std_postreset);
VMK_MODULE_EXPORT_ALIAS(sata_pmp_std_prereset);
VMK_MODULE_EXPORT_ALIAS(sata_scr_read);
VMK_MODULE_EXPORT_ALIAS(sata_scr_valid);
VMK_MODULE_EXPORT_ALIAS(sata_scr_write);
VMK_MODULE_EXPORT_ALIAS(sata_scr_write_flush);
VMK_MODULE_EXPORT_ALIAS(sata_set_spd);
VMK_MODULE_EXPORT_ALIAS(sata_std_hardreset);
