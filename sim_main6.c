#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free
#include <unistd.h>                                 // fork(), pipe(), read(), write(), close()
#include <signal.h>                                 // SIGTERM
#include <fcntl.h>                                  // fcntl(), O_NONBLOCK
#include <time.h>                                   // nanosleep()
#include <sys/wait.h>                               // waitpid()
#include <sys/mman.h>                               // mmap(), munmap() — shared memory between parent and children
#include <semaphore.h>                              // sem_t, sem_init(), sem_wait(), sem_post(), sem_destroy()
#include "graph.h"                                  // Graph struct
#include "dijkstra.h"                               // DijkstraResult and dijkstra()
#include "gui.h"                                    // PipeMsg, MsgType, draw_graph_multi6(), MAX_TRAVELERS

#define JUMP_MS      300                            // milliseconds per edge-weight unit while travelling
#define NODE_WAIT_MS 1000                           // milliseconds a child spends inside a node (critical section)

/* Shared memory layout: one binary semaphore per node.
   Initialized to 1 (unlocked). A child calls sem_wait() to enter a node
   and sem_post() to leave, enforcing mutual exclusion. */
typedef struct {
    sem_t node_mutex[MAX_NODES];                   // one semaphore (mutex) per node in the graph
} SharedMem;

static SharedMem* shm = NULL;                      // global pointer to the shared memory region (visible to all processes)

/* Each child calls this function — it never returns to main() */
static void child_run6(int fd, Graph* g, int src, int dst) { // fd = write end of this child's pipe
    DijkstraResult res = dijkstra(g, src, dst);    // child computes its own shortest path

    if (!res.found || res.path_len == 0) {         // no path: send finished immediately
        PipeMsg fin = { MSG_FINISHED, -1, -1 };
        write(fd, &fin, sizeof(fin));
        close(fd);
        exit(0);
    }

    for (int k = 0; k < res.path_len; k++) {       // walk through each node in the path
        int cur  = res.path[k];                     // current node index
        int next = (k < res.path_len - 1) ? res.path[k + 1] : -1; // next node; -1 at destination

        /* Step 1: tell the GUI we are waiting outside this node */
        PipeMsg wmsg = { MSG_WAITING, cur, next };
        write(fd, &wmsg, sizeof(wmsg));             // parent/GUI shows us as blocked (yellow)

        /* Step 2: acquire this node's mutex — blocks if another traveler is inside */
        sem_wait(&shm->node_mutex[cur]);            // decrement semaphore: 1→0 = acquired, 0 = blocks until released

        /* Step 3: tell the GUI we entered the node */
        PipeMsg amsg = { MSG_AT_NODE, cur, next };
        write(fd, &amsg, sizeof(amsg));             // parent/GUI shows us inside the node

        /* Step 4: stay 1 second inside the node (simulates work in the critical section) */
        struct timespec ts = { 1, 0 };             // 1 second, 0 nanoseconds
        nanosleep(&ts, NULL);                       // sleep while holding the mutex

        /* Step 5: release the mutex so another traveler can enter this node */
        sem_post(&shm->node_mutex[cur]);            // increment semaphore: 0→1 = released

        /* Step 6: if there is a next node, sleep to simulate travel time along the edge */
        if (next >= 0) {
            int w = g->matrix[cur][next];           // weight of the edge from cur to next
            long travel_ms = (long)w * JUMP_MS;    // total travel delay in milliseconds
            struct timespec tt = {
                travel_ms / 1000,                  // whole seconds portion of the delay
                (travel_ms % 1000) * 1000000L      // remaining milliseconds converted to nanoseconds
            };
            nanosleep(&tt, NULL);                  // sleep for the travel duration
        }
    }

    PipeMsg fin = { MSG_FINISHED, -1, -1 };        // all nodes visited; signal done
    write(fd, &fin, sizeof(fin));
    close(fd);
    exit(0);
}

int main(int argc, char* argv[]) {                 // program entry point
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim6 <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");                 // open input file
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    int n, m;                                      // n = node count, m = edge count
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate the graph

    for (int i = 0; i < m; i++) {                 // read and add each edge
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge on edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
        if (weight < 0) {
            fprintf(stderr, "Error: negative weight\n");
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, src, dst, weight);
    }

    int num_travelers;                             // number of travelers to read
    if (fscanf(f, "%d", &num_travelers) != 1 ||
        num_travelers < 1 || num_travelers > MAX_TRAVELERS) {
        fprintf(stderr, "Error: invalid travelers count\n");
        free_graph(g); fclose(f); return 1;
    }

    int t_src[MAX_TRAVELERS], t_dst[MAX_TRAVELERS]; // each traveler's source and destination
    for (int i = 0; i < num_travelers; i++) {
        if (fscanf(f, "%d %d", &t_src[i], &t_dst[i]) != 2) {
            fprintf(stderr, "Error: invalid traveler %d\n", i + 1);
            free_graph(g); fclose(f); return 1;
        }
    }
    fclose(f);                                     // done reading the file

    /* Allocate shared memory so all child processes share the same semaphore array */
    shm = mmap(NULL, sizeof(SharedMem),            // NULL = let the OS choose the address
               PROT_READ | PROT_WRITE,             // shared memory is both readable and writable
               MAP_SHARED | MAP_ANONYMOUS,         // shared between processes, not backed by a file
               -1, 0);                             // -1 fd and 0 offset (required for MAP_ANONYMOUS)
    if (shm == MAP_FAILED) {                       // mmap returns MAP_FAILED on error
        perror("mmap");
        free_graph(g); return 1;
    }

    for (int i = 0; i < n; i++) {                 // initialise one mutex semaphore for each node
        if (sem_init(&shm->node_mutex[i], 1 /*shared between processes*/, 1 /*starts unlocked*/) != 0) {
            perror("sem_init");
            free_graph(g); return 1;
        }
    }

    int pipes[MAX_TRAVELERS][2];                   // one pipe per traveler: [0]=read end, [1]=write end
    for (int i = 0; i < num_travelers; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free_graph(g); return 1;
        }
    }

    pid_t pids[MAX_TRAVELERS];                     // stores each child's process ID
    for (int i = 0; i < num_travelers; i++) {      // fork one child per traveler
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {                        // child process block
            for (int j = 0; j < num_travelers; j++) { // close file descriptors the child doesn't need
                close(pipes[j][0]);                // close all read ends (only parent reads)
                if (j != i) close(pipes[j][1]);    // close all other children's write ends
            }
            child_run6(pipes[i][1], g, t_src[i], t_dst[i]); // child starts walking its path
            /* child_run6 calls exit() — never returns */
        }
    }

    int fds[MAX_TRAVELERS];                        // parent's read ends of each pipe
    for (int i = 0; i < num_travelers; i++) {
        close(pipes[i][1]);                        // parent doesn't write; close write ends
        fds[i] = pipes[i][0];                      // save the read end
        fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK); // non-blocking so the GUI can poll without stalling
    }

    draw_graph_multi6(g, pids, fds, t_src, t_dst, num_travelers); // run the GUI; reads IPC messages each frame

    for (int i = 0; i < num_travelers; i++) {      // cleanup after the GUI window closes
        close(fds[i]);                             // close read end of each pipe
        waitpid(pids[i], NULL, 0);                 // wait for each child to terminate
    }
    for (int i = 0; i < n; i++)
        sem_destroy(&shm->node_mutex[i]);          // destroy each semaphore before unmapping shared memory
    munmap(shm, sizeof(SharedMem));                // release the shared memory region
    free_graph(g);                                 // free the graph
    return 0;                                      // success
}
