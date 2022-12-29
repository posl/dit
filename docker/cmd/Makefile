CC := gcc
CFLAGS ?= -O2 -march=native -Wall -Werror
LDFLAGS ?=

PROG := dit
DEST := /usr/local/bin/

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))


.PHONY: all clean

all: $(PROG)
	mv $< $(DEST)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJS)