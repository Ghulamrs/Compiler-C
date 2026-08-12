# Serial by design. The box this builds on has ~200 MB of RAM to spare and a
# single g++ pass can want most of it; -j2 is how you meet the OOM killer.
# Written in C partly so that ceiling stops mattering.

CC      = gcc
CFLAGS  = -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)
TARGET  = cc1

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c src/cc.h
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@./tests/run.sh

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf tests/out
