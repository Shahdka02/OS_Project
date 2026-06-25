#include <stdio.h>                                  // printf, fprintf, fopen, fscanf, fclose
#include <stdlib.h>                                 // malloc, free (used by graph functions)
#include "graph.h"                                  // Graph struct and graph management functions
#include "dijkstra.h"                               // DijkstraResult and dijkstra()
#include "gui.h"                                    // draw_graph() for the raylib window

int main(int argc, char* argv[]) {                  // program entry point
    if (argc != 2) {                               // exactly one argument (the filename) is required
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");                 // open the input file for reading
    if (!f) {                                      // NULL means the file could not be opened
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    int n, m;                                      // n = node count, m = edge count
    if (fscanf(f, "%d %d", &n, &m) != 2) {        // read the first two integers from the file
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f);
        return 1;
    }

    Graph* g = create_graph(n, m);                 // allocate and initialise the adjacency-matrix graph

    for (int i = 0; i < m; i++) {                 // read each of the m edges
        int src, dst, weight;                      // edge: directed from src to dst with this weight
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) { // must read exactly 3 values
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g); fclose(f); return 1;   // clean up and exit on bad input
        }
        if (weight < 0) {                          // Dijkstra requires non-negative weights
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g); fclose(f); return 1;
        }
        add_edge(g, src, dst, weight);             // insert the edge into the matrix
    }

    int src, dst;                                  // the single-traveler query: shortest path from src to dst
    if (fscanf(f, "%d %d", &src, &dst) != 2) {    // read the query line at the end of the file
        fprintf(stderr, "Error: missing source/destination query\n");
        free_graph(g); fclose(f); return 1;
    }
    fclose(f);                                     // finished reading the file

    DijkstraResult res = dijkstra(g, src, dst);    // compute the shortest path

    draw_graph(g, &res);                           // open a raylib window and animate the result

    free_graph(g);                                 // release graph memory
    return 0;                                      // success
}
