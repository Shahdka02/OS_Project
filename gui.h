#ifndef GUI_H
#define GUI_H

#include "graph.h"
#include "dijkstra.h"

// Draws the graph in a raylib window, highlighting the shortest path
void draw_graph(Graph* g, DijkstraResult* res);

#endif