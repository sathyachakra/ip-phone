all: cli ser
CC=gcc
CFLAGS=-I

cli: rec_client.c
	$(CC) -o $@ $? -lpulse -lpulse-simple -levent

ser: play_server.c
	$(CC) -o $@ $? -lpulse -lpulse-simple -levent

clean:
	rm -f cli ser