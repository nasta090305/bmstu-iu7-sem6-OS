CC := gcc

.PHONY: build
build: client server

%.exe: %.c
	gcc -o $@ $^ -Wall -Werror
