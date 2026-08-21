# Non-interactive read/write soak test for interfaces 1 and 2.

CC	?= gcc
CFLAGS	?= -O2 -g -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -DLINUX -I.

bcc_test: bcc_test.c Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ bcc_test.c

.PHONY: clean
clean:
	rm -f bcc_test *.o
