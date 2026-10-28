# Makefile for makelnx tool

CC = gcc
CFLAGS = -O2

makelnx: Source/make_lnx.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f makelnx

.PHONY: clean
