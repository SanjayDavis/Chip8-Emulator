CFLAGS = -std=c17 -Wall -Wextra -Werror $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)

all:
	gcc chip8.c -o chip8 $(CFLAGS) $(LDFLAGS)
