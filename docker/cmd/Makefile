CC := gcc
CFLAGS ?= -O2 -march=native -Wall -Werror
LDFLAGS ?=

PROG := dit

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))


.PHONY: all clean

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(PROG) $(OBJS)