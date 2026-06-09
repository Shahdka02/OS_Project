#ifndef GUI_H
#define GUI_H

#include "graph.h"
#include "dijkstra.h"
#include <sys/types.h>

// Draws the graph in a raylib window, highlighting the shortest path (single traveler)
void draw_graph(Graph* g, DijkstraResult* res);

// Milestone 4: draws multiple travelers simultaneously.
// When a traveler finishes, the parent sends SIGTERM to the child and continues.
void draw_graph_multi(Graph* g, DijkstraResult* results, pid_t* pids, int num_travelers);

#endif