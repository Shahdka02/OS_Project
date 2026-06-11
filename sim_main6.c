#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "graph.h"
#include "dijkstra.h"
#include "gui.h"

#define JUMP_MS      300
#define NODE_WAIT_MS 1000

/*
 * Shared memory layout:
 * One semaphore per node — binary semaphore (mutex).
 * Initialized to 1 (unlocked). A child does sem_wait before
 * entering a node and sem_post when it leaves.
 */
typedef struct {
    sem_t node_mutex[MAX_NODES];
} SharedMem;

static SharedMem* shm = NULL;

/* ------------------------------------------------------------------ */
/* Child process: walk the path, locking each node before entering    */
/* ------------------------------------------------------------------ */
static void child_run6(int fd, Graph* g, int src, int dst) {
    DijkstraResult res = dijkstra(g, src, dst);

    if (!res.found || res.path_len == 0) {
        PipeMsg fin = { MSG_FINISHED, -1, -1 };
        write(fd, &fin, sizeof(fin));
        close(fd);
        exit(0);
    }

    for (int k = 0; k < res.path_len; k++) {
        int cur  = res.path[k];
        int next = (k < res.path_len - 1) ? res.path[k + 1] : -1;

        /* --- notify parent: waiting to enter cur --- */
        PipeMsg wmsg = { MSG_WAITING, cur, next };
        write(fd, &wmsg, sizeof(wmsg));

        /* --- acquire mutex for this node (blocks if occupied) --- */
        sem_wait(&shm->node_mutex[cur]);

        /* --- notify parent: inside node now --- */
        PipeMsg amsg = { MSG_AT_NODE, cur, next };
        write(fd, &amsg, sizeof(amsg));

        /* --- stay in node for 1 second (critical section) --- */
        struct timespec ts = { NODE_WAIT_MS / 1000,
                               (NODE_WAIT_MS % 1000) * 1000000L };
        nanosleep(&ts, NULL);

        /* --- release mutex --- */
        sem_post(&shm->node_mutex[cur]);

        /* --- if there is a next node, also wait for travel time --- */
        if (next >= 0) {
            int w = g->matrix[cur][next];
            long travel_ms = (long)w * JUMP_MS;
            struct timespec tt = { travel_ms / 1000,
                                   (travel_ms % 1000) * 1000000L };
            nanosleep(&tt, NULL);
        }
    }

    PipeMsg fin = { MSG_FINISHED, -1, -1 };
    write(fd, &fin, sizeof(fin));
    close(fd);
    exit(0);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim6 <file_name>\n");
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
        fclose(f); return 1;
    }

    Graph* g = create_graph(n, m);

    for (int i = 0; i < m; i++) {
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

    /* --- create shared memory --- */
    shm = mmap(NULL, sizeof(SharedMem),
               PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        free_graph(g); return 1;
    }

    /* --- initialize one mutex semaphore per node --- */
    for (int i = 0; i < n; i++) {
        if (sem_init(&shm->node_mutex[i], 1 /*shared*/, 1 /*unlocked*/) != 0) {
            perror("sem_init");
            free_graph(g); return 1;
        }
    }

    /* --- create one pipe per traveler --- */
    int pipes[MAX_TRAVELERS][2];
    for (int i = 0; i < num_travelers; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free_graph(g); return 1;
        }
    }

    /* --- fork children --- */
    pid_t pids[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            free_graph(g); return 1;
        }
        if (pids[i] == 0) {
            /* child: close all read ends + other write ends */
            for (int j = 0; j < num_travelers; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            child_run6(pipes[i][1], g, t_src[i], t_dst[i]);
            /* never returns */
        }
    }

    /* --- parent: close write ends, set non-blocking --- */
    int fds[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        close(pipes[i][1]);
        fds[i] = pipes[i][0];
        fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK);
    }

    /* --- run GUI --- */
    draw_graph_multi6(g, pids, fds, t_src, t_dst, num_travelers);

    /* --- cleanup --- */
    for (int i = 0; i < num_travelers; i++) {
        close(fds[i]);
        waitpid(pids[i], NULL, 0);
    }
    for (int i = 0; i < n; i++)
        sem_destroy(&shm->node_mutex[i]);
    munmap(shm, sizeof(SharedMem));
    free_graph(g);
    return 0;
}