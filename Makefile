CC=gcc
CFLAGS=-Wall -Wextra -std=c17 -O2 -pthread

torneo:torneo.c
	$(CC) $(CFLAGS) torneo.c -o torneo

clean:
	rm -f torneo torneo.log