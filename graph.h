#ifndef GRAPH_H                                     // guard: skip if already included
#define GRAPH_H                                     // mark as included

#define MAX_NODES 15                                // maximum number of nodes the graph can hold
#define INF 1000000                                 // stands for "infinity" — used when no path is known yet

typedef struct {                                    // define the Graph struct
    int n;                                          // number of nodes in this graph
    int m;                                          // number of edges in this graph
    int matrix[MAX_NODES][MAX_NODES];               // adjacency matrix: matrix[i][j] = weight of edge i→j, or -1 if no edge
} Graph;                                           // name of the struct type

Graph* create_graph(int n, int m);                 // allocate and initialise a new graph with n nodes and m edges
void   free_graph(Graph* g);                       // free the memory used by the graph
int    add_edge(Graph* g, int src, int dst, int weight); // add a directed edge from src to dst with the given weight

#endif                                             // end of include guard
