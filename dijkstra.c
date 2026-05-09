#include "dijkstra.h"

DijkstraResult dijkstra(Graph* g, int src, int dst) {
    DijkstraResult result = {.found = 0, .path_len = 0, .total_weight = 0};

    // Special case: source == destination
    if (src == dst) {
        result.found = 1;
        result.path[0] = src;
        result.path_len = 1;
        result.total_weight = 0;
        return result;
    }

    int dist[MAX_NODES];
    int prev[MAX_NODES];
    int visited[MAX_NODES];

    for (int i = 0; i < g->n; i++) {
        dist[i]    = INF;
        prev[i]    = -1;
        visited[i] = 0;
    }
    dist[src] = 0;

    for (int iter = 0; iter < g->n; iter++) {
        // Pick unvisited node with smallest distance
        int u = -1;
        for (int i = 0; i < g->n; i++) {
            if (!visited[i] && (u == -1 || dist[i] < dist[u]))
                u = i;
        }
        if (u == -1 || dist[u] == INF) break;
        visited[u] = 1;

        // Relax neighbors
        for (int v = 0; v < g->n; v++) {
            if (g->matrix[u][v] != -1) {
                int new_dist = dist[u] + g->matrix[u][v];
                if (new_dist < dist[v]) {
                    dist[v] = new_dist;
                    prev[v] = u;
                }
            }
        }
    }

    if (dist[dst] == INF) return result; // No path

    // Reconstruct path by walking back through prev[]
    int tmp[MAX_NODES], len = 0, cur = dst;
    while (cur != -1) {
        tmp[len++] = cur;
        cur = prev[cur];
    }
    // Reverse into result.path
    for (int i = 0; i < len; i++)
        result.path[i] = tmp[len - 1 - i];

    result.path_len     = len;
    result.total_weight = dist[dst];
    result.found        = 1;
    return result;
}