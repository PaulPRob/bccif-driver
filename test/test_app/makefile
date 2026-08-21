# Makefile for various routines.
#-----------------------------------------------------------------------
#SHELL=/bin/sh
#
# C compiler and compilation flags
#
CCOMPL= gcc
CFLAGD= -O -c -DLINUX -Wall
KCFLAGD= -O3 -c -DLINUX -fomit-frame-pointer -Wall -I/usr2/users/isi_app/linux_pxe/tftp/root/root_development/usr/linux-2.4.25/include
INCLUDE_DIRS= -I.
#
#


bcc_test: bcc_test.c makefile
	$(CCOMPL) -O -DLINUX -Wall -s $(INCLUDE_DIRS) -o$@ -static $<
#
clean:
	rm -f *.o


