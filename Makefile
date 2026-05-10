CC     = gcc
CFLAGS = -Wall -Wextra -g
RAYLIB_INCLUDE = -I/opt/homebrew/Cellar/raylib/5.5/include
RAYLIB_LIBS    = -L/opt/homebrew/Cellar/raylib/5.5/lib -lraylib -lm

milestone1: main.o graph.o dijkstra.o
	$(CC) $(CFLAGS) -o dijkstra main.o graph.o dijkstra.o

milestone2: sim_main.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim sim_main.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

main.o: main.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) -c main.c

sim_main.o: sim_main.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main.c

graph.o: graph.c graph.h
	$(CC) $(CFLAGS) -c graph.c

dijkstra.o: dijkstra.c dijkstra.h graph.h
	$(CC) $(CFLAGS) -c dijkstra.c

gui.o: gui.c gui.h graph.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c gui.c

clean:
	rm -f *.o dijkstra sim