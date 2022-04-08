CC=gcc
ALT_CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic
FILE=sps
all: $(FILE).c
	$(CC) $(CFLAGS) $(FILE).c -o $(FILE)
debug: $(FILE).c
	$(CC) $(CFLAGS) -g $(FILE).c -o $(FILE)
alt: $(FILE).c
	$(ALT_CC) $(CFLAGS) $(FILE).c -o $(FILE)
