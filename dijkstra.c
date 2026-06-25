#include "dijkstra.h"                                          // brings in Graph, DijkstraResult, MAX_NODES, INF declarations

DijkstraResult dijkstra(Graph* g, int src, int dst) {          // function takes the graph pointer, source node, and destination node
    DijkstraResult result = {.found = 0, .path_len = 0, .total_weight = 0}; // initialise result struct: no path found yet, empty path, zero weight

    // Special case: source == destination
    if (src == dst) {                                          // if start and end are the same node, no traversal needed
        result.found = 1;                                      // mark a valid path as found
        result.path[0] = src;                                  // the path contains just the single source node
        result.path_len = 1;                                   // path length is 1 (only the source)
        result.total_weight = 0;                               // cost is 0 because we didn't move
        return result;                                         // return early, nothing else to compute
    }

    int dist[MAX_NODES];                                       // dist[i] = shortest known distance from src to node i
    int prev[MAX_NODES];                                       // prev[i] = the node that comes before i on the shortest path
    int visited[MAX_NODES];                                    // visited[i] = 1 once node i has been finalised (popped from the set)

    for (int i = 0; i < g->n; i++) {                          // loop over every node in the graph to initialise arrays
        dist[i]    = INF;                                      // start with infinite distance (node not yet reached)
        prev[i]    = -1;                                       // -1 means no predecessor is known yet
        visited[i] = 0;                                        // 0 means the node has not been visited yet
    }
    // Distance from source to itself is 0
    dist[src] = 0;                                             // the source node is 0 distance from itself
    // Main loop - runs once per node
    for (int iter = 0; iter < g->n; iter++) {                  // outer loop runs at most n times, once for each node
        // Pick unvisited node with smallest distance
        int u = -1;                                            // u will hold the index of the chosen node; -1 means none found yet
        for (int i = 0; i < g->n; i++) {                      // scan every node to find the minimum-distance unvisited one
            if (!visited[i] && (u == -1 || dist[i] < dist[u])) // node must be unvisited AND have a smaller distance than current best
                u = i;                                         // update u to this node
        }
        // If no reachable unvisited nodes remain, stop early
        if (u == -1 || dist[u] == INF) break;                 // if all remaining nodes are unreachable, terminate the loop early
        visited[u] = 1;                                        // mark u as finalised; its shortest distance will not change again

        // Relax neighbors
        for (int v = 0; v < g->n; v++) {                      // check every potential neighbour of u
            if (g->matrix[u][v] != -1) {                       // -1 in the adjacency matrix means no edge exists; skip those
                int new_dist = dist[u] + g->matrix[u][v];     // candidate distance = distance to u plus edge weight u→v
                if (new_dist < dist[v]) {                      // if this candidate is shorter than the currently known distance to v
                    dist[v] = new_dist;                        // update v's shortest distance
                    prev[v] = u;                               // record that u is the predecessor of v on this better path
                }
            }
        }
    }

    if (dist[dst] == INF) return result;                       // dst was never reached; return the default "not found" result

    // Reconstruct path by walking back through prev[]
    int tmp[MAX_NODES], len = 0, cur = dst;                    // tmp holds the path in reverse; len counts nodes; start walking from dst
    while (cur != -1) {                                        // keep walking back until we reach a node with no predecessor (-1)
        tmp[len++] = cur;                                      // store current node and advance the length counter
        cur = prev[cur];                                       // move to the predecessor of cur
    }
    // Reverse into result.path
    for (int i = 0; i < len; i++)                             // loop to copy tmp into result.path in the correct (forward) order
        result.path[i] = tmp[len - 1 - i];                    // mirror index: last element of tmp becomes first element of result.path

    result.path_len     = len;                                 // store the total number of nodes in the path
    result.total_weight = dist[dst];                           // store the total cost of the shortest path
    result.found        = 1;                                   // flag that a valid path was found
    return result;                                             // return the completed result to the caller
}
