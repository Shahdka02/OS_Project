#include "graph.h"                                  // brings in Graph struct, MAX_NODES, INF, and function declarations
#include <stdlib.h>                                 // needed for malloc() and free()

Graph* create_graph(int n, int m) {                // allocates a new Graph on the heap and returns a pointer to it
    Graph* g = malloc(sizeof(Graph));              // allocate exactly enough memory to hold one Graph struct
    g->n = n;                                      // store the number of nodes
    g->m = m;                                      // store the number of edges
    for (int i = 0; i < MAX_NODES; i++)            // loop over every row of the matrix
        for (int j = 0; j < MAX_NODES; j++)        // loop over every column of the matrix
            g->matrix[i][j] = -1;                  // -1 means no edge exists between node i and node j
    return g;                                      // return the pointer to the newly created graph
}

void free_graph(Graph* g) {                        // releases the heap memory used by the graph
    free(g);                                       // free the single malloc'd block; g is no longer valid after this
}

int add_edge(Graph* g, int src, int dst, int weight) { // adds a directed edge from src to dst with the given weight
    if (weight < 0) return -1;                     // reject negative weights (Dijkstra requires non-negative weights)
    g->matrix[src][dst] = weight;                  // store the weight in the adjacency matrix at row src, column dst
    return 0;                                      // return 0 to indicate success
}
