CC := gcc
CFLAGS ?= -O2 -march=native -Wall -Werror
LDFLAGS ?=

PROG := dit
EXTRA := srcglob

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

EXOBJ := $(EXTRA).o
PROBJS := $(filter-out $(EXOBJ),$(OBJS))

.PHONY: all clean

all: $(PROG) $(EXTRA)

$(PROG): $(PROBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(EXTRA): $(EXOBJ)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(PROG) $(EXTRA) $(OBJS)