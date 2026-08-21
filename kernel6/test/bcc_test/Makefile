# Menu driven test/debug program for the BCC interface.
#
# Changes from the 2.6 era makefile: no -static, no hard-coded 2.4 kernel
# include path, and the warnings are actually enabled.

CC	?= gcc
CFLAGS	?= -O2 -g -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -DLINUX -I.
LDLIBS	:= -lm

bcc_test: bcc_test.c bcc_test.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ bcc_test.c $(LDLIBS)

.PHONY: clean
clean:
	rm -f bcc_test *.o
