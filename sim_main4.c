#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free
#include <unistd.h>                                 // fork(), getpid(), pause()
#include <signal.h>                                 // SIGTERM (used to kill child processes when journey ends)
#include <sys/wait.h>                               // waitpid() to reap finished child processes
#include "graph.h"                                  // Graph struct and graph management
#include "dijkstra.h"                               // DijkstraResult and dijkstra()
#include "gui.h"                                    // draw_graph_multi() and MAX_TRAVELERS

int main(int argc, char* argv[]) {                  // program entry point
    if (argc != 2) {                               // exactly one argument (filename) required
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");                 // open input file for reading
    if (!f) {                                      // NULL = file could not be opened
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    int n, m;                                      // n = node count, m = edge count
    if (fscanf(f, "%d %d", &n, &m) != 2) {        // read graph dimensions from first line
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate and initialise the graph

    for (int i = 0; i < m; i++) {                 // read all m edges from the file
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) { // each edge needs exactly 3 values
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        if (weight < 0) {                          // Dijkstra cannot handle negative weights
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, src, dst, weight);             // insert the directed edge into the adjacency matrix
    }

    int num_travelers;                             // how many travelers are listed in the file
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) { // must be between 1 and MAX_TRAVELERS
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g); fclose(f); return 1;
    }

    int t_src[MAX_TRAVELERS], t_dst[MAX_TRAVELERS]; // arrays holding each traveler's source and destination
    for (int i = 0; i < num_travelers; i++) {      // read one source-destination pair per traveler
        if (fscanf(f, "%d %d", &t_src[i], &t_dst[i]) != 2) {
            fprintf(stderr, "Error: invalid traveler %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
    }
    fclose(f);                                     // finished reading the file

    DijkstraResult results[MAX_TRAVELERS];         // array to hold the shortest-path result for each traveler
    for (int i = 0; i < num_travelers; i++) {      // parent computes all paths BEFORE forking children
        results[i] = dijkstra(g, t_src[i], t_dst[i]); // compute shortest path for traveler i
    }

    pid_t pids[MAX_TRAVELERS];                     // stores the process ID of each child
    for (int i = 0; i < num_travelers; i++) {      // fork one child process per traveler
        pids[i] = fork();                          // create a new child process; returns 0 in child, child PID in parent
        if (pids[i] < 0) {                         // negative return value means fork() failed
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {                        // this block runs only in the child process
            printf("[%d] started\n", (int)getpid()); // child prints its own PID to show it is alive
            fflush(stdout);                        // flush so the print appears immediately
            while (1) pause();                     // child sleeps indefinitely; parent sends SIGTERM to wake/kill it
            exit(0);                               // exit cleanly (reached only after signal handling, not normally)
        }
        /* parent continues the loop to fork the next child */
    }

    draw_graph_multi(g, results, pids, num_travelers); // parent runs the GUI; sends SIGTERM to each child when it finishes

    for (int i = 0; i < num_travelers; i++) {      // parent waits for each child to terminate
        waitpid(pids[i], NULL, 0);                 // 0 = wait until this specific child exits; NULL = don't need exit status
    }

    free_graph(g);                                 // release graph memory
    return 0;                                      // success
}
