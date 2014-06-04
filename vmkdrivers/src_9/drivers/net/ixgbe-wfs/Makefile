################################################################################
#
# Intel 10 Gigabit PCI Express Linux driver
# Copyright (c) 1999 - 2014 Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information:
# e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
# Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
#
################################################################################

#-------------------------------------------------------------------------------
# Makefile
#
#    Provides a wrapper to compile drivers in DDK
#
#    This is a generic Makefile and no changes are required to compile 
#    drivers
#-------------------------------------------------------------------------------

include /opt/vmware/ddk/includes/ddk.inc

#-------------------------------------------------------------------------------
# Targets
#-------------------------------------------------------------------------------
default: vib

vib: driver-vib # Legacy target

driver-vib: clean
	@$(MAKE) -f $(RULES) driver-pre
	@$(MAKE) -f $(RULES) copy-sc
	@$(MAKE) -f $(RULES) SCONS_TARGET=$(DRIVER_TARGET) do-scons
	@$(MAKE) -f $(RULES) driver-copyout

ima-vib: clean ima-check
	@$(MAKE) -f $(RULES) copy-sc
	@$(MAKE) -f $(RULES) SCONS_TARGET=$(IMA_TARGET) do-scons
	@$(MAKE) -f $(RULES) ima-copyout

all-vib: clean ima-check
	@$(MAKE) -f $(RULES) driver-pre
	@$(MAKE) -f $(RULES) copy-sc
	@$(MAKE) -f $(RULES) SCONS_TARGET="$(DRIVER_TARGET) $(IMA_TARGET)" do-scons
	@$(MAKE) -f $(RULES) driver-copyout
	@$(MAKE) -f $(RULES) ima-copyout

driver-iso:
	@echo Using deprecated target '"driver-iso"'.
	@echo Please use '"driver-bundle"' instead.
	@exit 1

driver-bundle:
	@if [[ -f $(IMA_SC) ]]; then \
		$(MAKE) all-vib; \
		$(MKDIR) -p $(STAGE); \
		if [[ -z $${DDK_IMA_OVERRIDE} ]]; then \
			$(CP) $(CURRENT_DIR)/build/VIB/`$(IMACMD) | $(XARGS) $(BASENAME)` $(STAGE); \
		else \
			$(CP) $${DDK_IMA_OVERRIDE} $(STAGE); \
		fi; \
	else \
		$(MAKE) driver-vib; \
		$(MKDIR) -p $(STAGE); \
	fi;
	@if [[ -z $${DDK_DRIVER_OVERRIDE} ]]; then \
		$(CP) $(CURRENT_DIR)/build/VIB/`$(VIBCMD) | $(XARGS) $(BASENAME)` $(STAGE); \
	else \
		$(CP) $${DDK_DRIVER_OVERRIDE} $(STAGE); \
	fi;
	$(VIBPUBLISH) --group=$(BUNDLE)/metadata.zip \
		--create-offline-bundle=$(BUNDLE)/$(MODULE).zip \
		--target="ESXi,$(TARGET_ESX)" \
		--bulletin=$(BULLETINXML) \
		--vendor=`$(VNDCMD)` \
		--vendor-code=`$(VNDCODECMD)` \
		-o "$(CURRENT_DIR)/build/bundle" \
                -s "$(CURRENT_DIR)/build/VIB" \
                -f
	@echo
	@echo "Offline bundle $(MODULE).zip is available in directory $(BUNDLE)/"
	@echo

clean: 
	@$(RM) -rf $(CURRENT_DIR)/build
	@$(RM) -rf $(XMLPATH)/$(XML)
	@$(RM) -rf $(SCPATH)/$(DRIVER_SC)
	@$(RM) -rf $(IMASCPATH)/$(IMA_SC)
	@$(RM) -rf $(CURRENT_DIR)/$(LOG)
	@cd $(SRC); $(SCONS) PRODUCT=$(PRODUCT) \
           BUILDTYPE=$(BUILDTYPE) BUILD_NUMBER=$(BUILDNUMBER) \
           SKIP_BIG_BROTHER=1 clobber

ima-check:
	@if [[ ! -f $(IMA_SC) ]]; then \
		echo "No IMA scons file ($(IMA_SC))!"; \
		echo "Use another makefile target."; \
		exit 1; \
	fi

code-check:	   
	@if [ "$(TOCLEAN)" != "" ]; then \
		$(RM) -rf $(DRIVER_SRC)/$(DRIVER_DIR)/$(CCHECK); \
		$(MKDIR) -p $(DRIVER_SRC)/$(DRIVER_DIR)/$(CCHECK); \
		$(MKDIR) -p $(XMLPATH); \
		$(MKDIR) -p $(SCPATH); \
		$(CP) $(XML) $(XMLPATH); \
		$(CP) $(DRIVER_SC) $(SCPATH); \
		$(MKDIR) -p /opt/vmware/ddk/src/bora/build/scons/build/HEADERS/vmkapi-current-all-public-bincomp/vmkernel64/$(BUILDTYPE); \
		$(CP) -r /opt/vmware/ddk/src/bora/vmkernel/include/vmkapi/* /opt/vmware/ddk/src/bora/build/scons/build/HEADERS/vmkapi-current-all-public-bincomp/vmkernel64/$(BUILDTYPE)/; \
		cd $(SRC); $(SCONS) PRODUCT=$(PRODUCT) SKIP_BIG_BROTHER=1 BUILDTYPE=$(BUILDTYPE) VIBPATH=$(VIBPATH) $(CCTARGET); \
		cd $(SRC); for i in `ls $(DRIVER_SRC)/$(DRIVER_DIR)/$(CCHECK)/*.sh`; \
		do echo "Checking $$i"; . $$i 2>&1 | \
		$(TEE) -a $(DRIVER_SRC)/$(DRIVER_DIR)/$(CCHECK)/warnings.txt ; done; \
		if [ -d $(SCPATH) ]; then $(RM) -f $(SCPATH)/$(DRIVER_SC); fi; \
		echo "Results in $(DRIVER_SRC)/$(DRIVER_DIR)/$(CCHECK)/warnings.txt"; \
	else \
		echo "Driver source directory not found. Skipping code-check"; \
	fi


