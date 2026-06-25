#ifndef GUI_H                                       // guard: skip if already included
#define GUI_H                                       // mark as included
#define MAX_TRAVELERS 10                            // maximum number of simultaneous travelers the program supports
#include "graph.h"                                 // need Graph struct and MAX_NODES
#include "dijkstra.h"                              // need DijkstraResult struct
#include <sys/types.h>                             // need pid_t (process ID type)

// Milestone 3: single traveler. Opens a raylib window, draws the graph,
// and animates the entity moving along the shortest path.
void draw_graph(Graph* g, DijkstraResult* res);

// Milestone 4: multiple travelers whose paths are pre-computed by the parent.
// Each traveler is a child process; the parent sends SIGTERM when it finishes.
void draw_graph_multi(Graph* g, DijkstraResult* results, pid_t* pids, int num_travelers);

typedef enum {                                     // enum listing all IPC message types sent through pipes
    MSG_MOVING,                                    // child is currently travelling toward next_node
    MSG_WAITING,                                   // child is blocked outside next_node (mutex is taken)
    MSG_AT_NODE,                                   // child has acquired the mutex and entered current_node
    MSG_FINISHED,                                  // child is done; current_node = -1
    MSG_LEAVING                                    // child is releasing a node (used in milestone 7)
} MsgType;                                        // name of the enum type

typedef struct {                                   // IPC message struct used in milestones 5 and 6
    MsgType type;                                  // which event this message represents (see MsgType above)
    int current_node;                              // the node the child is at or requesting; -1 = finished
    int next_node;                                 // the next node on the path; -1 = current_node is destination
} PipeMsg;                                        // sent child→parent through a pipe

typedef struct {                                   // extended IPC message used in milestone 7
    MsgType type;                                  // same event types as above
    int current_node;                              // the node the child is at or requesting
    int next_node;                                 // the next node on the path; -1 = destination
    int priority;                                  // remaining total path weight — used by SJF to pick who enters first
} PipeMsg7;                                       // sent child→parent through a pipe in milestone 7

// Milestone 5: IPC-driven GUI. Each child computes its own path and sends
// PipeMsg structs through fds[i] (one non-blocking read-end per traveler).
void draw_graph_multi5(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers);

// Milestone 6: like milestone 5 but enforces mutual exclusion —
// at most one traveler may occupy a node at a time (semaphore-protected).
void draw_graph_multi6(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers);

// Milestone 7: parent-driven scheduling with FCFS or SJF.
// report_fds[i] — parent reads child status reports (child→parent read end).
// grant_wfds[i] — parent writes a 1-byte grant to unblock the chosen child.
// sched_name    — "FCFS" or "SJF" string that selects the algorithm.
void draw_graph_multi7(Graph* g, pid_t* pids,
                       int* report_fds, int* grant_wfds,
                       int* t_src, int* t_dst, int num_travelers,
                       const char* sched_name);

#endif                                             // end of include guard
