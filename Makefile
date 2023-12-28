CC = cc
CFLAGS = -Wall -Wextra -Wpedantic

main: main.c cbs.h
	$(CC) $(CFLAGS) -o $@ $<
