CC     = gcc
CFLAGS = -Wall -Wextra -g

# OS Detection
ifeq ($(OS), Windows_NT)
    RAYLIB_INCLUDE = -I/mingw64/include
    RAYLIB_LIBS    = -L/mingw64/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    EXE            = .exe
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Darwin)
        RAYLIB_INCLUDE = -I/opt/homebrew/include
        RAYLIB_LIBS    = -L/opt/homebrew/lib -lraylib -lm
    else
        RAYLIB_INCLUDE = -I/usr/local/include
        RAYLIB_LIBS    = -L/usr/local/lib -lraylib \
                         -lGL -lm -lpthread -ldl -lrt \
                         -lX11 -lXrandr -lXi -lXcursor -lXinerama
    endif
    EXE =
endif

milestone1: main.o graph.o dijkstra.o
	$(CC) $(CFLAGS) -DMILESTONE1 -o dijkstra$(EXE) main.o graph.o dijkstra.o

milestone2: sim_main.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

milestone3: sim_main.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

milestone4: sim_main4.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main4.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

milestone5: sim_main5.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main5.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

milestone6: sim_main6.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main6.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

milestone7: sim_main7.o graph.o dijkstra.o gui.o
	$(CC) $(CFLAGS) -o sim$(EXE) sim_main7.o graph.o dijkstra.o gui.o $(RAYLIB_LIBS)

main.o: main.c graph.h dijkstra.h
	$(CC) $(CFLAGS) -c main.c

sim_main.o: sim_main.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main.c

sim_main4.o: sim_main4.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main4.c

sim_main5.o: sim_main5.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main5.c

sim_main6.o: sim_main6.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main6.c

sim_main7.o: sim_main7.c graph.h dijkstra.h gui.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c sim_main7.c

graph.o: graph.c graph.h
	$(CC) $(CFLAGS) -c graph.c

dijkstra.o: dijkstra.c dijkstra.h graph.h
	$(CC) $(CFLAGS) -c dijkstra.c

gui.o: gui.c gui.h graph.h dijkstra.h
	$(CC) $(CFLAGS) $(RAYLIB_INCLUDE) -c gui.c

clean:
	rm -f *.o dijkstra$(EXE) sim$(EXE)
