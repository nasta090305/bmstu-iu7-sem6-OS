CC := gcc

.PHONY: build
build: client.out server.out

%.out: %.c
	gcc -o $@ $^ -Wall -Werror
