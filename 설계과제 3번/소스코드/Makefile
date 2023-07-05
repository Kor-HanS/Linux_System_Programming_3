CC = gcc
CFLAGS = -Wall -g

all: ssu_monitor

ssu_monitor : main.o ssu_monitor.o ssu_daemon.o
	$(CC) $(CFLAGS) $^ -o $@

main.o : main.c ssu_monitor.h ssu_daemon.h
	$(CC) $(CFLAGS) -c $< -o $@

ssu_monitor.o : ssu_monitor.c ssu_monitor.h ssu_daemon.h
	$(CC) $(CFLAGS) -c $< -o $@

ssu_daemon.o : ssu_daemon.c ssu_daemon.h ssu_monitor.h
	$(CC) $(CFLAGS) -c $< -o $@

clean : 
	rm -f *.o
