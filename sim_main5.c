#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free
#include <unistd.h>                                 // fork(), pipe(), read(), write(), close()
#include <signal.h>                                 // SIGTERM
#include <fcntl.h>                                  // fcntl(), O_NONBLOCK — set pipe read ends to non-blocking
#include <time.h>                                   // nanosleep() for timing delays inside child
#include <sys/wait.h>                               // waitpid()
#include "graph.h"                                  // Graph struct
#include "dijkstra.h"                               // DijkstraResult and dijkstra()
#include "gui.h"                                    // PipeMsg, MsgType, draw_graph_multi5(), MAX_TRAVELERS

/* Must match JUMP_SEC and NODE_WAIT_SEC in gui.c — keeps child timing in sync with animation */
#define JUMP_MS       300                           // milliseconds the child waits per edge-weight unit while travelling
#define NODE_WAIT_MS  1000                          // milliseconds the child spends sitting at each intermediate node

/* Each child calls this function — it never returns to main() */
static void child_run(int fd, Graph* g, int src, int dst) { // fd = write end of this child's pipe
    DijkstraResult res = dijkstra(g, src, dst);    // each child independently computes its own shortest path

    if (!res.found || res.path_len == 0) {         // no path exists for this traveler
        PipeMsg msg = { MSG_NO_PATH, src, dst};
        write(fd, &msg, sizeof(msg));

        PipeMsg fin = { MSG_FINISHED, -1, -1 };    // compose a "finished" message with sentinel values
        write(fd, &fin, sizeof(fin));              // send the message to the parent through the pipe
        close(fd);                                 // close the write end before exiting
        exit(0);                                   // child exits
    }

    for (int k = 0; k < res.path_len; k++) {      // walk along each node in the computed path
        int next = (k < res.path_len - 1) ? res.path[k + 1] : -1; // next node; -1 if at destination
        PipeMsg msg = { MSG_AT_NODE, res.path[k], next };           // tell the parent which node we are at
        write(fd, &msg, sizeof(msg));              // send message through the pipe

        if (next >= 0) {                           // if there is a next node, sleep to simulate travel time
            PipeMsg moving = { MSG_MOVING, res.path[k], next};
            write(fd, &moving, sizeof(moving));
            int w = g->matrix[res.path[k]][next]; // weight of the edge from current node to next
            long ms = (long)NODE_WAIT_MS + (long)w * JUMP_MS; // total delay: 1 s at node + weight * 300 ms per jump
            struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L }; // convert ms to seconds + nanoseconds
            nanosleep(&ts, NULL);                  // sleep for the computed duration
        }
    }

    PipeMsg fin = { MSG_FINISHED, -1, -1 };        // all nodes visited; send finished message
    write(fd, &fin, sizeof(fin));                  // send to parent
    close(fd);                                     // close the pipe write end
    exit(0);                                       // child exits cleanly
}

int main(int argc, char* argv[]) {                 // program entry point
    if (argc != 2) {                               // requires exactly one argument (the input filename)
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");                 // open the input file for reading
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    int n, m;                                      // n = node count, m = edge count
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f);
        return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate the graph

    for (int i = 0; i < m; i++) {                 // read and insert each edge
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

    int num_travelers;                             // number of traveler entries to read
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g); fclose(f); return 1;
    }

    int t_src[MAX_TRAVELERS], t_dst[MAX_TRAVELERS]; // source and destination for each traveler
    for (int i = 0; i < num_travelers; i++) {
        if (fscanf(f, "%d %d", &t_src[i], &t_dst[i]) != 2) {
            fprintf(stderr, "Error: invalid traveler %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
    }
    fclose(f);                                     // done reading the file

    int pipes[MAX_TRAVELERS][2];                   // pipes[i][0] = read end, pipes[i][1] = write end for traveler i
    for (int i = 0; i < num_travelers; i++) {      // create one pipe per traveler
        if (pipe(pipes[i]) < 0) {                  // pipe() returns -1 on failure
            perror("pipe");
            free_graph(g); return 1;
        }
    }

    pid_t pids[MAX_TRAVELERS];                     // stores the PID of each child process
    for (int i = 0; i < num_travelers; i++) {      // fork one child per traveler
        pids[i] = fork();                          // create child; fork returns 0 in child, child PID in parent
        if (pids[i] < 0) {
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {                        // child process block
            for (int j = 0; j < num_travelers; j++) { // close all file descriptors the child doesn't own
                close(pipes[j][0]);                // close every read end (only parent reads)
                if (j != i) close(pipes[j][1]);    // close all other children's write ends
            }
            child_run(pipes[i][1], g, t_src[i], t_dst[i]); // child writes to its own pipe write end
            /* child_run calls exit() — this line is never reached */
        }
    }

    int fds[MAX_TRAVELERS];                        // convenience array: read ends of all pipes for the parent
    for (int i = 0; i < num_travelers; i++) {
        close(pipes[i][1]);                        // parent does not write; close write ends
        fds[i] = pipes[i][0];                      // save the read end for this traveler's pipe
        fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK); // make reads non-blocking so the GUI doesn't stall
    }

    draw_graph_multi5(g, pids, fds, t_src, t_dst, num_travelers); // GUI reads IPC messages and drives animation each frame

    for (int i = 0; i < num_travelers; i++) {
        close(fds[i]);                             // close each read end after the GUI exits
        waitpid(pids[i], NULL, 0);                 // wait for each child process to finish
    }

    free_graph(g);                                 // free graph memory
    return 0;                                      // success
}
