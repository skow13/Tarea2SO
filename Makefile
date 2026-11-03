CC=gcc
CFLAGS=-Wall -Wextra -std=c17 -O2 -pthread

torneo: Torneo.c
	$(CC) $(CFLAGS) Torneo.c -o torneo

clean:
	rm -f torneo torneo.log