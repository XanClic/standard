CC = gcc
CFLAGS = -Iinclude -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs -Wswitch-enum -Wsync-nand -Wunused -Wtrampolines -Wundef -Wno-endif-labels -Wshadow -Wunsafe-loop-optimizations -Wcast-align -Wwrite-strings -Wlogical-op -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-declarations -Wnormalized=nfc -Wnested-externs -Winvalid-pch -Wdisabled-optimization -Woverlength-strings -O3 -g2
LDFLAGS = -g2

OBJECTS = $(patsubst %.c,%.o,$(wildcard src/*.c))

RM = rm -f

.PHONY: all clean

all: std

std: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
