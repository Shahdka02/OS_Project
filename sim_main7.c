#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free
#include <string.h>                                 // strcmp() to parse scheduler name
#include <unistd.h>                                 // fork(), pipe(), read(), write(), close()
#include <fcntl.h>                                  // fcntl(), O_NONBLOCK
#include <time.h>                                   // nanosleep()
#include <signal.h>                                 // SIGTERM
#include <sys/wait.h>                               // waitpid()
#include "graph.h"                                  // Graph struct
#include "dijkstra.h"                               // DijkstraResult and dijkstra()
#include "gui.h"                                    // PipeMsg7, MsgType, draw_graph_multi7(), MAX_TRAVELERS

#define JUMP_MS 300                                 // milliseconds per edge-weight unit while travelling

/* Child process: walk the shortest path, asking the parent for permission
   before entering each node, then reporting when it leaves. Never returns. */
static void child_run7(int report_fd, int grant_fd, Graph* g, int src, int dst) {
    DijkstraResult res = dijkstra(g, src, dst);    // each child independently computes its own path

    if (!res.found || res.path_len == 0) {         // no path exists
        PipeMsg7 fin = { MSG_FINISHED, -1, -1, 0 }; // send finished with sentinel values
        write(report_fd, &fin, sizeof(fin));
        exit(0);
    }

    for (int k = 0; k < res.path_len; k++) {       // walk each node in the path
        int cur  = res.path[k];                     // current node
        int next = (k + 1 < res.path_len) ? res.path[k + 1] : -1; // next node; -1 at destination

        /* Compute remaining path weight from cur onward — used as SJF priority */
        int remaining = 0;
        for (int j = k; j < res.path_len - 1; j++) // sum up all edge weights still ahead
            remaining += g->matrix[res.path[j]][res.path[j + 1]];

        /* Step 1: request entry — tell parent we want to enter node cur */
        PipeMsg7 wmsg = { MSG_WAITING, cur, next, remaining };
        write(report_fd, &wmsg, sizeof(wmsg));      // parent will queue us and grant access when node is free

        /* Step 2: block until the parent grants access (parent writes 1 byte) */
        char go;
        read(grant_fd, &go, 1);                    // blocking read: child sleeps here until parent sends the grant

        /* Step 3: report that we entered the node */
        PipeMsg7 amsg = { MSG_AT_NODE, cur, next, remaining };
        write(report_fd, &amsg, sizeof(amsg));

        /* Step 4: stay 1 second in the node (the critical section) */
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);                      // sleep while "occupying" the node

        if (next >= 0) {
            /* Step 5: release the node — tell parent to schedule the next waiter */
            PipeMsg7 lmsg = { MSG_LEAVING, cur, next, remaining };
            write(report_fd, &lmsg, sizeof(lmsg));

            /* Step 6: sleep to simulate travel time along the edge */
            int w = g->matrix[cur][next];          // weight of the edge to the next node
            struct timespec tt = {
                (w * JUMP_MS) / 1000,              // whole seconds of travel time
                ((w * JUMP_MS) % 1000) * 1000000L  // remaining nanoseconds of travel time
            };
            nanosleep(&tt, NULL);
        }
        /* At the last node there is no MSG_LEAVING — the parent frees it on MSG_FINISHED */
    }

    /* All nodes visited: send final message so parent knows this traveler is done */
    PipeMsg7 fin = { MSG_FINISHED, res.path[res.path_len - 1], -1, 0 };
    write(report_fd, &fin, sizeof(fin));
    exit(0);
}

int main(int argc, char* argv[]) {                 // program entry point
    if (argc != 4 || strcmp(argv[1], "-schd") != 0) { // expects: ./sim -schd fcfs|sjf <file>
        fprintf(stderr, "Usage: ./sim -schd fcfs|sjf <file_name>\n");
        return 1;
    }

    int is_fcfs;                                   // 1 = use FCFS scheduling, 0 = use SJF
    if      (strcmp(argv[2], "fcfs") == 0) is_fcfs = 1; // first-come-first-served
    else if (strcmp(argv[2], "sjf")  == 0) is_fcfs = 0; // shortest-job-first (by remaining path weight)
    else {
        fprintf(stderr, "Unknown scheduler '%s'. Use 'fcfs' or 'sjf'.\n", argv[2]);
        return 1;
    }

    FILE* f = fopen(argv[3], "r");                 // open the input file (argv[3] = filename)
    if (!f) { perror(argv[3]); return 1; }

    int n, m;                                      // n = node count, m = edge count
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate and initialise the graph

    for (int i = 0; i < m; i++) {                 // read and insert each edge
        int s, d, w;                               // source, destination, weight
        if (fscanf(f, "%d %d %d", &s, &d, &w) != 3) {
            fprintf(stderr, "Error: invalid edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, s, d, w);                      // add the directed edge to the adjacency matrix
    }

    int num_travelers;                             // number of travelers
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid traveler count\n");
        free_graph(g); fclose(f); return 1;
    }

    int t_src[MAX_TRAVELERS], t_dst[MAX_TRAVELERS]; // source and destination for each traveler
    for (int i = 0; i < num_travelers; i++) {
        if (fscanf(f, "%d %d", &t_src[i], &t_dst[i]) != 2) {
            fprintf(stderr, "Error: invalid traveler %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
    }
    fclose(f);                                     // done reading the input file

    /* Two pipes per traveler:
       report_pipes[i]: child writes status → parent reads (child→parent)
       grant_pipes[i]:  parent writes grant → child reads  (parent→child) */
    int report_pipes[MAX_TRAVELERS][2];
    int grant_pipes[MAX_TRAVELERS][2];

    for (int i = 0; i < num_travelers; i++) {
        if (pipe(report_pipes[i]) < 0 || pipe(grant_pipes[i]) < 0) { // create both pipes
            perror("pipe"); free_graph(g); return 1;
        }
    }

    pid_t pids[MAX_TRAVELERS];                     // stores each child's process ID
    for (int i = 0; i < num_travelers; i++) {      // fork one child per traveler
        pids[i] = fork();
        if (pids[i] < 0) { perror("fork"); free_graph(g); return 1; }

        if (pids[i] == 0) {                        // child process block
            for (int j = 0; j < num_travelers; j++) { // close all file descriptors the child doesn't own
                close(report_pipes[j][0]);         // close all parent-side read ends of report pipes
                if (j != i) close(report_pipes[j][1]); // close other children's report write ends
                close(grant_pipes[j][1]);          // close all parent-side write ends of grant pipes
                if (j != i) close(grant_pipes[j][0]);  // close other children's grant read ends
            }
            child_run7(report_pipes[i][1], grant_pipes[i][0],
                       g, t_src[i], t_dst[i]);     // child writes reports and reads grants on its own pipes
            /* child_run7 calls exit() — never returns */
        }
    }

    /* Parent keeps only the ends it uses */
    int report_fds[MAX_TRAVELERS], grant_wfds[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        close(report_pipes[i][1]);                 // parent does not write to report pipe; close child's write end
        close(grant_pipes[i][0]);                  // parent does not read from grant pipe; close child's read end
        report_fds[i]  = report_pipes[i][0];       // parent reads child status from here
        grant_wfds[i]  = grant_pipes[i][1];        // parent writes grants to here
        fcntl(report_fds[i], F_SETFL,
              fcntl(report_fds[i], F_GETFL) | O_NONBLOCK); // non-blocking so GUI can poll each frame
    }

    /* GUI runs the scheduler: reads report messages, applies FCFS or SJF,
       writes grants to unblock the chosen child, and animates everything */
    draw_graph_multi7(g, pids, report_fds, grant_wfds,
                      t_src, t_dst, num_travelers,
                      is_fcfs ? "FCFS" : "SJF");

    for (int i = 0; i < num_travelers; i++) {      // cleanup after the GUI window closes
        close(report_fds[i]);                      // close report pipe read ends
        close(grant_wfds[i]);                      // close grant pipe write ends
        waitpid(pids[i], NULL, 0);                 // wait for each child to finish
    }
    free_graph(g);                                 // release graph memory
    return 0;                                      // success
}
