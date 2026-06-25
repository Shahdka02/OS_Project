#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free (used indirectly through graph functions)
#include "graph.h"                                  // Graph struct and graph management functions
#include "dijkstra.h"                               // DijkstraResult struct and dijkstra() function
#include "gui.h"                                    // draw_graph() for the raylib window

int main(int argc, char* argv[]) {                  // program entry point; argc = argument count, argv = argument strings
    if (argc != 2) {                               // exactly one argument (the filename) is required
        fprintf(stderr, "Usage: ./dijkstra <file_name>\n"); // print usage to stderr if wrong number of args
        return 1;                                  // exit with error code 1
    }

    FILE* f = fopen(argv[1], "r");                 // open the input file in read mode
    if (!f) {                                      // fopen returns NULL if the file does not exist or can't be opened
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]); // report which file failed
        return 1;                                  // exit with error
    }

    int n, m;                                      // n = number of nodes, m = number of edges
    if (fscanf(f, "%d %d", &n, &m) != 2) {        // read the first line; fscanf returns the count of items successfully read
        fprintf(stderr, "Error: invalid file format\n"); // file does not start with two integers
        fclose(f);                                 // always close the file before returning
        return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate and initialise the graph with n nodes and m edges

    for (int i = 0; i < m; i++) {                 // read each of the m edges from the file
        int src, dst, weight;                      // source node, destination node, and edge weight for this edge
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) { // read three integers; fail if fewer
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g);                         // free already-allocated graph before exiting
            fclose(f);
            return 1;
        }
        if (weight < 0) {                          // Dijkstra's algorithm does not work with negative weights
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g);
            fclose(f);
            return 1;
        }
        add_edge(g, src, dst, weight);             // insert this directed edge into the adjacency matrix
    }

    int src, dst;                                  // the query: find the shortest path from src to dst
    if (fscanf(f, "%d %d", &src, &dst) != 2) {    // read the last line of the file
        fprintf(stderr, "Error: missing source/destination query\n");
        free_graph(g);
        fclose(f);
        return 1;
    }
    fclose(f);                                     // done reading; close the file

    DijkstraResult res = dijkstra(g, src, dst);    // run Dijkstra's algorithm and store the result

    if (src == dst) {                              // trivial case: source and destination are the same node
        printf("%d\n0\n", src);                   // print the single node and a cost of 0
    } else if (!res.found) {                       // no path exists between src and dst
        printf("No path found\n");                // report that no path was found
    } else {
        for (int i = 0; i < res.path_len; i++) {  // print each node in the shortest path
            if (i > 0) printf(" -> ");             // print arrow separator between nodes (not before the first)
            printf("%d", res.path[i]);             // print the node index
        }
        printf("\n%d\n", res.total_weight);        // print the total cost of the path on a new line
    }

    #ifdef MILESTONE1                              // compile-time switch: if MILESTONE1 is defined, stub out draw_graph
    void draw_graph(Graph* g, DijkstraResult* res) { } // empty stub so the linker doesn't complain about missing symbol
    #endif
    free_graph(g);                                 // release the graph's heap memory
    return 0;                                      // return 0 = success
}
