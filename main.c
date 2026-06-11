#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "dijkstra.h"
#include "gui.h"

int main(int argc, char* argv[]) {
    // Make sure the user provided a filename
    if (argc != 2) {
        fprintf(stderr, "Usage: ./dijkstra <file_name>\n");
        return 1;
    }
    // Try to open the file
    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }
    // Read number of nodes and edges from the first line
    int n, m;
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f);
        return 1;
    }
    // Create the graph
    Graph* g = create_graph(n, m);
    // Read each edge and add it to the graph
    for (int i = 0; i < m; i++) {
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g);
            fclose(f);
            return 1;
        }
         // Negative weights are not allowed (Dijkstra doesn't handle them)
        if (weight < 0) {
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g);
            fclose(f);
            return 1;
        }
        add_edge(g, src, dst, weight);
    }
    // Read the source and destination for the Dijkstra query
    int src, dst;
    if (fscanf(f, "%d %d", &src, &dst) != 2) {
        fprintf(stderr, "Error: missing source/destination query\n");
        free_graph(g);
        fclose(f);
        return 1;
    }
    fclose(f);
    // Run Dijkstra
    DijkstraResult res = dijkstra(g, src, dst);
    // Print result based on what was found
    if (src == dst) {
        printf("%d\n0\n", src);
    } else if (!res.found) {
        // No path exists between src and dst
        printf("No path found\n");
    } else {
        // Print the path
        for (int i = 0; i < res.path_len; i++) {
            if (i > 0) printf(" -> ");
            printf("%d", res.path[i]);
        }
        printf("\n%d\n", res.total_weight);
    }
    // Free memory before exiting
    #ifdef MILESTONE1
    void draw_graph(Graph* g, DijkstraResult* res) { }
    #endif
    free_graph(g);
    return 0;
}