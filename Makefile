CC     = gcc
CFLAGS = -Wall -Wextra -g
RAYLIB_FLAGS = -lraylib -lm

milestone1: main.o graph.o dijkstra.o
	$(CC) $(CFLAGS) -o dijkstra main.o graph.o dijkstra.o

milestone2: main.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim main.o graph.o dijkstra.o gui.o $(RAYLIB_FLAGS)

main.o: main.c graph.h dijkstra.h
	$(CC) $(CFLAGS) -c main.c

graph.o: graph.c graph.h
	$(CC) $(CFLAGS) -c graph.c

dijkstra.o: dijkstra.c dijkstra.h graph.h
	$(CC) $(CFLAGS) -c dijkstra.c

gui.o: gui.c gui.h graph.h
	$(CC) $(CFLAGS) -c gui.c

clean:
	rm -f *.o dijkstra sim