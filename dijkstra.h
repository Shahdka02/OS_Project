#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

typedef struct {
    int path[MAX_NODES];
    int path_len;
    int total_weight;
    int found; // 1 = path found, 0 = no path
} DijkstraResult;

DijkstraResult dijkstra(Graph* g, int src, int dst);

#endif