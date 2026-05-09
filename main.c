#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "dijkstra.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./dijkstra <file_name>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    int n, m;
    if (fscanf(f, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error: invalid file format\n");
        fclose(f);
        return 1;
    }

    Graph* g = create_graph(n, m);

    for (int i = 0; i < m; i++) {
        int src, dst, weight;
        if (fscanf(f, "%d %d %d", &src, &dst, &weight) != 3) {
            fprintf(stderr, "Error: invalid edge format on edge %d\n", i + 1);
            free_graph(g);
            fclose(f);
            return 1;
        }
        if (weight < 0) {
            fprintf(stderr, "Error: negative weight is invalid input\n");
            free_graph(g);
            fclose(f);
            return 1;
        }
        add_edge(g, src, dst, weight);
    }

    int src, dst;
    if (fscanf(f, "%d %d", &src, &dst) != 2) {
        fprintf(stderr, "Error: missing source/destination query\n");
        free_graph(g);
        fclose(f);
        return 1;
    }
    fclose(f);

    DijkstraResult res = dijkstra(g, src, dst);

    if (src == dst) {
        printf("%d\n0\n", src);
    } else if (!res.found) {
        printf("No path found\n");
    } else {
        for (int i = 0; i < res.path_len; i++) {
            if (i > 0) printf(" -> ");
            printf("%d", res.path[i]);
        }
        printf("\n%d\n", res.total_weight);
    }

    free_graph(g);
    return 0;
}