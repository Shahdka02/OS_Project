#ifndef GUI_H
#define GUI_H
#define MAX_TRAVELERS 10
#include "graph.h"
#include "dijkstra.h"
#include <sys/types.h>

// Draws the graph in a raylib window, highlighting the shortest path (single traveler)
void draw_graph(Graph* g, DijkstraResult* res);

// Milestone 4: draws multiple travelers simultaneously.
// When a traveler finishes, the parent sends SIGTERM to the child and continues.
void draw_graph_multi(Graph* g, DijkstraResult* results, pid_t* pids, int num_travelers);

// IPC message sent from child to parent via pipe (milestone 5).
// current_node = -1 signals the traveler has finished.
// next_node    = -1 means current_node is the DESTINATION.
typedef struct {
    int current_node;
    int next_node;
} PipeMsg;

// Milestone 5: IPC-driven multi-traveler GUI.
// Each child computes its own path and reports position via pipe fds[i].
void draw_graph_multi5(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers);

#endif