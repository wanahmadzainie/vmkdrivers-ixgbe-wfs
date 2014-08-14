
#CC = gcc
CC = /opt/vmware-toolchain/lin32/gcc-4.1.2-9/bin/x86_64-linux-gcc

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
export OS_TYPE=64
endif
ifeq ($(LBITS),32)
export OS_TYPE=32
endif

#DRIVER_INC = $(PWD)/../src
DRIVER_INC = ../vmkdrivers/src_9/drivers/net/ixgbe-wfs
ifeq ($(OS_TYPE),32)
CFLAGS += -m32
endif
ifeq ($(OS_TYPE),64)
CFLAGS += -m64
endif
CFLAGS += -I. -I$(DRIVER_INC) -D__VMWARE__
#LFLAGS = -lpthread
DEPS = Makefile $(DRIVER_INC)/wfsctl.h wfsctl_cmd.h utils.h
OBJS = wfsctl.o wfsctl_cmd.o utils.o

APP =  wfsctl

# Add to EXTRA_CFLAGS, based on the logging level desired:
#CFLAGS += -DDEBUG

INSTDIR := /usr/bin

.PHONY:	clean install

all:	$(APP)
	@echo "***** App Compiled *****"

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LFLAGS) -o $(APP)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
	
install: $(APP)
	# remove all old versions of the driver
	find $(INSTDIR) -name $(APP) -exec rm -f {} \; || true
	install -D -m 755 $(APP) $(INSTDIR)/$(APP)

uninstall:
	if [ -e $(INSTDIR)/$(APP) ] ; then \
	    rm -f $(INSTDIR)/$(APP) ; \
	fi

clean:
	rm -f $(OBJS) $(APP)
