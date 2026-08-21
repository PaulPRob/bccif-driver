# Makefile for the BCCIF driver, Linux 6.8+ (Ubuntu 24.04)
#
# Build:      make                       (against the running kernel)
#             make KVER=6.8.0-51-generic
#             make KERNELDIR=/lib/modules/6.8.0-51-generic/build
# Static:     make sparse                (needs the sparse package)
# Install:    sudo make install          (into /lib/modules/<rel>/extra + depmod)
# Load:       sudo make load / sudo make unload
#
# The 2.6 Makefile passed -I$(KERNELDIR) and -O3 into the kbuild invocation and
# hand-copied the .ko afterwards; modern kbuild owns the compiler flags, so all
# of that is gone.

ifneq ($(KERNELRELEASE),)

# ---- kbuild pass -----------------------------------------------------------
obj-m		:= bccif.o
bccif-objs	:= main.o pci.o interrupts.o file_ops.o bccif_serial.o

ccflags-y	+= -Wall -Wextra -Wno-unused-parameter

else

# ---- command line pass -----------------------------------------------------
KVER		?= $(shell uname -r)
KERNELDIR	?= /lib/modules/$(KVER)/build
PWD		:= $(shell pwd)
MODDESTDIR	:= /lib/modules/$(KVER)/extra

.PHONY: all modules clean sparse install uninstall load unload test

all: modules

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# Sparse checks the __user / __iomem annotations the 2.6 driver had none of.
sparse:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=2 CF="-D__CHECK_ENDIAN__" modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(MAKE) -C test/bcc_test clean
	$(MAKE) -C test/test_app clean

test:
	$(MAKE) -C test/bcc_test
	$(MAKE) -C test/test_app

install: modules
	install -d $(MODDESTDIR)
	install -m 644 bccif.ko $(MODDESTDIR)
	install -d /etc/udev/rules.d
	install -m 644 99-bccif.rules /etc/udev/rules.d/
	depmod -a $(KVER)
	@echo "Installed. Load with 'modprobe bccif' (see README.md for Secure Boot)."

uninstall:
	rm -f $(MODDESTDIR)/bccif.ko /etc/udev/rules.d/99-bccif.rules
	depmod -a $(KVER)

load:
	insmod ./bccif.ko $(PARAMS)

unload:
	rmmod bccif

endif
