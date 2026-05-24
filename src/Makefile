CC      = gcc
CFLAGS  = -O2 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11
LDFLAGS = -lgmp

SRCS = main.c spill.c mul.c fib.c verify.c
OBJS = $(SRCS:.c=.o)

.PHONY: all trace clean

all: fib

trace: CFLAGS += -DFIB_TRACE
trace: fib

fib: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c fib.h trace.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) fib
	rm -rf /tmp/fib_spill
