#include "graph.h"
#include <stdlib.h>
// Allocates a new graph and initializes the matrix to -1 (no edges)
Graph* create_graph(int n, int m) {
    Graph* g = malloc(sizeof(Graph));
    g->n = n;
    g->m = m;
    for (int i = 0; i < MAX_NODES; i++)
        for (int j = 0; j < MAX_NODES; j++)
            g->matrix[i][j] = -1; // -1 means no edge
    return g;
}
// Frees the memory allocated for the graph
void free_graph(Graph* g) {
    free(g);
}

int add_edge(Graph* g, int src, int dst, int weight) {
    if (weight < 0) return -1; // invalid
    g->matrix[src][dst] = weight;
    return 0;
}