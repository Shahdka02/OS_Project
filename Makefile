CC     = gcc
CFLAGS = -Wall -Wextra -g

milestone1: main.o graph.o dijkstra.o
	$(CC) $(CFLAGS) -o dijkstra main.o graph.o dijkstra.o

main.o: main.c graph.h dijkstra.h
	$(CC) $(CFLAGS) -c main.c

graph.o: graph.c graph.h
	$(CC) $(CFLAGS) -c graph.c

dijkstra.o: dijkstra.c dijkstra.h graph.h
	$(CC) $(CFLAGS) -c dijkstra.c

clean:
	rm -f *.o dijkstra sim