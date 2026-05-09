#ifndef GRAPH_H
#define GRAPH_H

#define MAX_NODES 15
#define INF 1000000

typedef struct {
    int n;                            // number of nodes
    int m;                            // number of edges
    int matrix[MAX_NODES][MAX_NODES]; // -1 = no edge
} Graph;

Graph* create_graph(int n, int m);
void   free_graph(Graph* g);
int    add_edge(Graph* g, int src, int dst, int weight);

#endif