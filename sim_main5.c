#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include "graph.h"
#include "dijkstra.h"
#include "gui.h"

/* Must match JUMP_SEC and NODE_WAIT_SEC in gui.c */
#define JUMP_MS       300
#define NODE_WAIT_MS  1000

/* Child: compute own Dijkstra path and walk it, sending IPC messages */
static void child_run(int fd, Graph* g, int src, int dst) {
    DijkstraResult res = dijkstra(g, src, dst);

    if (!res.found || res.path_len == 0) {
        PipeMsg fin = { -1, 0 };
        write(fd, &fin, sizeof(fin));
        close(fd);
        exit(0);
    }

    for (int k = 0; k < res.path_len; k++) {
        int next = (k < res.path_len - 1) ? res.path[k + 1] : -1;
        PipeMsg msg = { res.path[k], next };
        write(fd, &msg, sizeof(msg));

        if (next >= 0) {
            int w = g->matrix[res.path[k]][next];
            long ms = (long)NODE_WAIT_MS + (long)w * JUMP_MS;
            struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
            nanosleep(&ts, NULL);
        }
    }

    PipeMsg fin = { -1, 0 };
    write(fd, &fin, sizeof(fin));
    close(fd);
    exit(0);
}

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

    int n, m;
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f);
        return 1;
    }

    Graph* g = create_graph(n, m);

    for (int i = 0; i < m; i++) {
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        if (weight < 0) {
            fprintf(stderr, "Error: negative weight is invalid\n");
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, src, dst, weight);
    }

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

    /* one pipe per traveler: child writes, parent reads */
    int pipes[MAX_TRAVELERS][2];
    for (int i = 0; i < num_travelers; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free_graph(g); return 1;
        }
    }

    /* fork children – each computes its own path and reports via pipe */
    pid_t pids[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {
            /* child: close all read ends and every other child's write end */
            for (int j = 0; j < num_travelers; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            child_run(pipes[i][1], g, t_src[i], t_dst[i]);
            /* never returns */
        }
    }

    /* parent: close write ends, set read ends non-blocking */
    int fds[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        close(pipes[i][1]);
        fds[i] = pipes[i][0];
        fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK);
    }

    /* GUI reads IPC messages and drives animation */
    draw_graph_multi5(g, pids, fds, t_src, t_dst, num_travelers);

    for (int i = 0; i < num_travelers; i++) {
        close(fds[i]);
        waitpid(pids[i], NULL, 0);
    }

    free_graph(g);
    return 0;
}
