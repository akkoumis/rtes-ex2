SHELL := /bin/bash

CC       = gcc
FLAGS    = -pthread

BINFILES = prod-cons


all: $(BINFILES)

%: %.c
	$(CC) $(FLAGS) $+ -o $@ -lm

clean:
	$(RM) $(BINFILES)
