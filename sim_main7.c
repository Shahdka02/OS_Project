#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include "graph.h"
#include "dijkstra.h"
#include "gui.h"

#define JUMP_MS 300

/* ------------------------------------------------------------------ */
/* Child: walk the shortest path, requesting each node from parent     */
/* ------------------------------------------------------------------ */
static void child_run7(int report_fd, int grant_fd, Graph* g, int src, int dst) {
    DijkstraResult res = dijkstra(g, src, dst);

    if (!res.found || res.path_len == 0) {
        PipeMsg7 fin = { MSG_FINISHED, -1, -1, 0 };
        write(report_fd, &fin, sizeof(fin));
        exit(0);
    }

    for (int k = 0; k < res.path_len; k++) {
        int cur  = res.path[k];
        int next = (k + 1 < res.path_len) ? res.path[k + 1] : -1;

        /* Remaining path weight from cur onward (used for SJF priority). */
        int remaining = 0;
        for (int j = k; j < res.path_len - 1; j++)
            remaining += g->matrix[res.path[j]][res.path[j + 1]];

        /* 1. Request entry – parent decides when to grant. */
        PipeMsg7 wmsg = { MSG_WAITING, cur, next, remaining };
        write(report_fd, &wmsg, sizeof(wmsg));

        /* 2. Block until parent grants access to this node. */
        char go;
        read(grant_fd, &go, 1);

        /* 3. Report entry. */
        PipeMsg7 amsg = { MSG_AT_NODE, cur, next, remaining };
        write(report_fd, &amsg, sizeof(amsg));

        /* 4. Critical section: stay 1 second. */
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);

        if (next >= 0) {
            /* 5. Release node and inform parent to schedule next waiter. */
            PipeMsg7 lmsg = { MSG_LEAVING, cur, next, remaining };
            write(report_fd, &lmsg, sizeof(lmsg));

            /* 6. Travel time proportional to edge weight. */
            int w = g->matrix[cur][next];
            struct timespec tt = {
                (w * JUMP_MS) / 1000,
                ((w * JUMP_MS) % 1000) * 1000000L
            };
            nanosleep(&tt, NULL);
        }
        /* Last node: no MSG_LEAVING – parent frees it on MSG_FINISHED. */
    }

    PipeMsg7 fin = { MSG_FINISHED, res.path[res.path_len - 1], -1, 0 };
    write(report_fd, &fin, sizeof(fin));
    exit(0);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char* argv[]) {
    if (argc != 4 || strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr, "Usage: ./sim -schd fcfs|sjf <file_name>\n");
        return 1;
    }

    int is_fcfs;
    if      (strcmp(argv[2], "fcfs") == 0) is_fcfs = 1;
    else if (strcmp(argv[2], "sjf")  == 0) is_fcfs = 0;
    else {
        fprintf(stderr, "Unknown scheduler '%s'. Use 'fcfs' or 'sjf'.\n", argv[2]);
        return 1;
    }

    FILE* f = fopen(argv[3], "r");
    if (!f) { perror(argv[3]); return 1; }

    int n, m;
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);

    for (int i = 0; i < m; i++) {
        int s, d, w;
        if (fscanf(f, "%d %d %d", &s, &d, &w) != 3) {
            fprintf(stderr, "Error: invalid edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, s, d, w);
    }

    int num_travelers;
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid traveler count\n");
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

    /* Create one report pipe (child→parent) and one grant pipe
       (parent→child write end) per traveler. */
    int report_pipes[MAX_TRAVELERS][2];
    int grant_pipes[MAX_TRAVELERS][2];

    for (int i = 0; i < num_travelers; i++) {
        if (pipe(report_pipes[i]) < 0 || pipe(grant_pipes[i]) < 0) {
            perror("pipe"); free_graph(g); return 1;
        }
    }

    pid_t pids[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        pids[i] = fork();
        if (pids[i] < 0) { perror("fork"); free_graph(g); return 1; }

        if (pids[i] == 0) {
            /* Child: close unneeded pipe ends. */
            for (int j = 0; j < num_travelers; j++) {
                close(report_pipes[j][0]);        /* parent's read end */
                if (j != i) close(report_pipes[j][1]);
                close(grant_pipes[j][1]);         /* parent's write end */
                if (j != i) close(grant_pipes[j][0]);
            }
            child_run7(report_pipes[i][1], grant_pipes[i][0],
                       g, t_src[i], t_dst[i]);
            /* never returns */
        }
    }

    /* Parent: close the ends used only by children. */
    int report_fds[MAX_TRAVELERS], grant_wfds[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        close(report_pipes[i][1]);   /* child's write end */
        close(grant_pipes[i][0]);    /* child's read end  */
        report_fds[i]  = report_pipes[i][0];
        grant_wfds[i]  = grant_pipes[i][1];
        fcntl(report_fds[i], F_SETFL,
              fcntl(report_fds[i], F_GETFL) | O_NONBLOCK);
    }

    draw_graph_multi7(g, pids, report_fds, grant_wfds,
                      t_src, t_dst, num_travelers,
                      is_fcfs ? "FCFS" : "SJF");

    for (int i = 0; i < num_travelers; i++) {
        close(report_fds[i]);
        close(grant_wfds[i]);
        waitpid(pids[i], NULL, 0);
    }
    free_graph(g);
    return 0;
}
