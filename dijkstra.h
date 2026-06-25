#ifndef DIJKSTRA_H                          // guard: skip this file if already included
#define DIJKSTRA_H                          // mark the header as included

#include "graph.h"                          // need Graph*, MAX_NODES, and INF from graph.h

typedef struct {                            // define a struct to hold all Dijkstra output
    int path[MAX_NODES];                    // the sequence of node indices on the shortest path
    int path_len;                           // how many nodes are in path[]
    int total_weight;                       // sum of edge weights along the path
    int found;                              // 1 = a path was found, 0 = no path exists
} DijkstraResult;                          // name of the struct type

DijkstraResult dijkstra(Graph* g, int src, int dst); // declaration: run Dijkstra from src to dst on graph g

#endif                                      // end of include guard
