#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "graph.h"
#include "dijkstra.h"
#include "gui.h"

#define MAX_TRAVELERS 10

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    //read graph 
    int n, m;
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);

    for (int i = 0; i < m; i++) {
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        if (weight < 0) {
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, src, dst, weight);
    }

    //read travelers 
    int num_travelers;
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g); fclose(f); return 1;
    }

    int t_src[MAX_TRAVELERS], t_dst[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        if (fscanf(f, "%d %d", &t_src[i], &t_dst[i]) != 2) {
            fprintf(stderr, "Error: invalid traveler %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
    }
    fclose(f);

    //parent computes all paths before forking 
    DijkstraResult results[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        results[i] = dijkstra(g, t_src[i], t_dst[i]);
    }

    //fork children 
    pid_t pids[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {
            /* --- child --- */
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);
            /* Sleep until parent sends SIGTERM when journey ends */
            while (1) pause();
            exit(0);
        }
        /* parent continues to next fork */
    }

    //parent: run GUI with all travelers 
    draw_graph_multi(g, results, pids, num_travelers);

    //wait for all children 
    for (int i = 0; i < num_travelers; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free_graph(g);
    return 0;
}