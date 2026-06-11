#include "gui.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NODE_RADIUS  25
#define ENTITY_R     14
#define WINDOW_W     800
#define WINDOW_H     600
#define PI           3.14159265358979323846f

#define BTN_X        20
#define BTN_Y        50
#define BTN_W        100
#define BTN_H        36

#define NODE_WAIT_SEC  1.0f   /* wait at intermediate nodes */
#define JUMP_SEC       0.3f   /* each jump on an edge takes 300 ms */

/* ---------- animation state machine ---------- */
typedef enum {
    ANIM_IDLE,      /* not started yet / paused before first move  */
    ANIM_AT_NODE,   /* waiting 1 s at an intermediate node          */
    ANIM_MOVING,    /* travelling along an edge in discrete jumps   */
    ANIM_DONE       /* reached destination                          */
} AnimState;

/* ---------- helpers ---------- */

static void get_node_positions(int n, Vector2 *pos) {
    float cx = WINDOW_W / 2.0f, cy = WINDOW_H / 2.0f, r = 200.0f;
    for (int i = 0; i < n; i++) {
        float a = (2 * PI * i) / n - PI / 2;
        pos[i].x = cx + r * cosf(a);
        pos[i].y = cy + r * sinf(a);
    }
}

static void draw_arrow(Vector2 s, Vector2 e, Color c, float thick) {
    float dx = e.x - s.x, dy = e.y - s.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) return;
    float ux = dx/len, uy = dy/len;
    Vector2 ls = { s.x + ux*NODE_RADIUS, s.y + uy*NODE_RADIUS };
    Vector2 le = { e.x - ux*NODE_RADIUS, e.y - uy*NODE_RADIUS };
    DrawLineEx(ls, le, thick, c);
    float as = 10.0f, ang = atan2f(dy, dx);
    Vector2 tip   = le;
    Vector2 left  = { tip.x - as*cosf(ang-0.4f), tip.y - as*sinf(ang-0.4f) };
    Vector2 right = { tip.x - as*cosf(ang+0.4f), tip.y - as*sinf(ang+0.4f) };
    DrawTriangle(tip, left, right, c);
}

static int is_path_edge(DijkstraResult *res, int i, int j) {
    for (int k = 0; k < res->path_len - 1; k++)
        if (res->path[k] == i && res->path[k+1] == j) return 1;
    return 0;
}

/* Lerp between two Vector2 */
static Vector2 v2lerp(Vector2 a, Vector2 b, float t) {
    return (Vector2){ a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t };
}

/* ---------- main entry point ---------- */

void draw_graph(Graph *g, DijkstraResult *res) {
    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    /* --- animation variables --- */
    int        playing     = 0;
    AnimState  state       = ANIM_IDLE;
    int        path_idx    = 0;   /* index into res->path of current node    */
    int        jump        = 0;   /* current jump index on active edge (0..W)*/
    int        W           = 0;   /* weight (= total jumps) of active edge   */
    float      timer       = 0.f; /* counts time for current state           */
    Vector2    entity      = (res->found && res->path_len > 0)
                             ? pos[res->path[0]]
                             : (Vector2){0,0};

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 3");
    SetTargetFPS(60);

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- button click ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {

            if (state == ANIM_DONE) {
                /* reset */
                playing   = 0;
                state     = ANIM_IDLE;
                path_idx  = 0;
                jump      = 0; W = 0; timer = 0.f;
                if (res->found && res->path_len > 0)
                    entity = pos[res->path[0]];
            } else {
                playing = !playing;
                /* kick off first edge when pressing play from idle */
                if (playing && state == ANIM_IDLE &&
                    res->found && res->path_len > 1) {
                    state = ANIM_MOVING;
                    timer = 0.f;
                    jump  = 0;
                    W     = g->matrix[res->path[0]][res->path[1]];
                    entity = pos[res->path[0]];
                }
            }
        }

        /* ---- animation update ---- */
        if (playing && res->found) {
            timer += dt;

            if (state == ANIM_MOVING) {
                /* Each jump lasts JUMP_SEC seconds */
                if (timer >= JUMP_SEC) {
                    timer -= JUMP_SEC;
                    jump++;

                    if (jump >= W) {
                        /* finished crossing the edge: arrive at next node */
                        path_idx++;
                        entity = pos[res->path[path_idx]];
                        jump   = 0; W = 0;

                        if (path_idx == res->path_len - 1) {
                            /* reached destination */
                            state   = ANIM_DONE;
                            playing = 0;
                        } else {
                            /* intermediate node: wait 1 second */
                            state = ANIM_AT_NODE;
                            timer = 0.f;
                        }
                    } else {
                        /* snap to the discrete jump position on the edge */
                        Vector2 from = pos[res->path[path_idx]];
                        Vector2 to   = pos[res->path[path_idx + 1]];
                        entity = v2lerp(from, to, (float)jump / W);
                    }
                }
                /* also smoothly interpolate within the 300ms window
                   so the entity glides between discrete positions       */
                else {
                    Vector2 from = pos[res->path[path_idx]];
                    Vector2 to   = pos[res->path[path_idx + 1]];
                    float   t    = ((float)jump + timer / JUMP_SEC) / (float)W;
                    if (t > 1.f) t = 1.f;
                    entity = v2lerp(from, to, t);
                }

            } else if (state == ANIM_AT_NODE) {
                if (timer >= NODE_WAIT_SEC) {
                    /* done waiting: start next edge */
                    state = ANIM_MOVING;
                    timer = 0.f;
                    jump  = 0;
                    W     = g->matrix[res->path[path_idx]][res->path[path_idx + 1]];
                    entity = pos[res->path[path_idx]];
                }
            }
        }

        /* ---- draw ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Green = source | Orange = destination | Red = shortest path",
                 10, 10, 15, DARKGRAY);

        /* edges */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;
                Color ec    = LIGHTGRAY;
                float thick = 2.0f;
                if (res->found && is_path_edge(res, i, j)) {
                    ec    = RED;
                    thick = 3.0f;
                }
                draw_arrow(pos[i], pos[j], ec, thick);
                /* weight label at midpoint */
                char ws[8];
                sprintf(ws, "%d", g->matrix[i][j]);
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2),
                    (int)((pos[i].y + pos[j].y) / 2),
                    16, DARKBLUE);
            }
        }

        /* nodes */
        for (int i = 0; i < g->n; i++) {
            Color nc = SKYBLUE;
            if (res->found && res->path_len > 0) {
                if (i == res->path[0])               nc = GREEN;
                if (i == res->path[res->path_len-1]) nc = ORANGE;
            }
            DrawCircleV(pos[i], NODE_RADIUS, nc);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);
            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 9), 18, BLACK);
        }

        /* animated entity: drawn on top of the graph */
        if (res->found && res->path_len > 0) {
            /* glow ring */
            DrawCircleV(entity, ENTITY_R + 4, (Color){120, 50, 200, 80});
            DrawCircleV(entity, ENTITY_R,     (Color){160, 32, 240, 255});
            DrawCircleLines(entity.x, entity.y, ENTITY_R, (Color){80, 0, 140, 255});
            /* dot in centre */
            DrawCircleV(entity, 4, WHITE);
        }

        /* play / stop / reset button */
        {
            Color bc;
            const char *lbl;
            if (state == ANIM_DONE) {
                bc  = DARKGRAY;
                lbl = "RESET";
            } else if (playing) {
                bc  = (Color){210, 60, 60, 255};
                lbl = "STOP";
            } else {
                bc  = (Color){60, 170, 60, 255};
                lbl = "PLAY";
            }
            DrawRectangleRec(btn, bc);
            DrawRectangleLinesEx(btn, 2, DARKGRAY);
            int lw = MeasureText(lbl, 20);
            DrawText(lbl,
                     (int)(btn.x + btn.width/2  - lw/2),
                     (int)(btn.y + btn.height/2 - 10),
                     20, WHITE);
        }

        /* status text next to button */
        {
            const char *st = "";
            if (!res->found)                 st = "No path";
            else if (state == ANIM_IDLE)     st = "Press PLAY to start";
            else if (state == ANIM_AT_NODE)  st = "Waiting at node...";
            else if (state == ANIM_MOVING)   st = "Moving...";
            else if (state == ANIM_DONE)     st = "Arrived!";
            DrawText(st, BTN_X + BTN_W + 12, BTN_Y + 9, 16, DARKGRAY);
        }

        /* path info strip at the bottom */
        if (res->found) {
            char info[256] = "Path: ";
            char tmp[8];
            for (int i = 0; i < res->path_len; i++) {
                if (i > 0) strcat(info, " -> ");
                sprintf(tmp, "%d", res->path[i]);
                strcat(info, tmp);
            }
            char w[32];
            sprintf(w, "  |  Total weight: %d", res->total_weight);
            strcat(info, w);
            DrawText(info, 10, WINDOW_H - 30, 16, DARKGRAY);
        } else {
            DrawText("No path found", 10, WINDOW_H - 30, 16, RED);
        }

        /* destination-reached banner */
        if (state == ANIM_DONE) {
            const char *msg = "  Destination reached!  ";
            int mw = MeasureText(msg, 28);
            DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 28,
                          mw + 20, 56, Fade(BLACK, 0.65f));
            DrawText(msg, WINDOW_W/2 - mw/2, WINDOW_H/2 - 14, 28, YELLOW);
        }

        EndDrawing();
    }

    CloseWindow();
}

/* ============================================================
 * Milestone 4 – multi-traveler GUI
 * ============================================================ */
#include <signal.h>
#include <sys/types.h>

/* One color per traveler (up to 10) */
static Color traveler_colors[] = {
    {160,  32, 240, 255},  /* purple  */
    { 30, 144, 255, 255},  /* dodger blue */
    {255, 140,   0, 255},  /* orange  */
    { 50, 205,  50, 255},  /* lime green */
    {220,  20,  60, 255},  /* crimson */
    {  0, 206, 209, 255},  /* dark turquoise */
    {255, 215,   0, 255},  /* gold */
    {199,  21, 133, 255},  /* medium violet red */
    {127, 255,   0, 255},  /* chartreuse */
    {255, 105, 180, 255},  /* hot pink */
};

typedef struct {
    int        active;      /* 1 = still travelling / at node */
    AnimState  state;
    int        path_idx;
    int        jump;
    int        W;
    float      timer;
    Vector2    entity;
    pid_t      pid;
} TravelerAnim;

void draw_graph_multi(Graph* g, DijkstraResult* results, pid_t* pids, int num_travelers) {
    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    TravelerAnim tv[MAX_TRAVELERS];
    int playing = 0;

    for (int i = 0; i < num_travelers; i++) {
        tv[i].active    = 1;
        tv[i].state     = ANIM_IDLE;
        tv[i].path_idx  = 0;
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].pid       = pids[i];
        tv[i].entity    = (results[i].found && results[i].path_len > 0)
                          ? pos[results[i].path[0]]
                          : (Vector2){0, 0};
    }

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 4");
    SetTargetFPS(60);

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- button click: play / stop / reset ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {

            /* check if ALL done → reset */
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active && tv[i].state != ANIM_DONE) { all_done = 0; break; }

            if (all_done) {
                playing = 0;
                for (int i = 0; i < num_travelers; i++) {
                    tv[i].active   = 1;
                    tv[i].state    = ANIM_IDLE;
                    tv[i].path_idx = 0;
                    tv[i].jump     = 0;
                    tv[i].W        = 0;
                    tv[i].timer    = 0.f;
                    tv[i].entity   = (results[i].found && results[i].path_len > 0)
                                     ? pos[results[i].path[0]]
                                     : (Vector2){0, 0};
                }
            } else {
                playing = !playing;
                if (playing) {
                    /* kick off every traveler that is still idle */
                    for (int i = 0; i < num_travelers; i++) {
                        if (tv[i].active && tv[i].state == ANIM_IDLE &&
                            results[i].found && results[i].path_len > 1) {
                            tv[i].state  = ANIM_MOVING;
                            tv[i].timer  = 0.f;
                            tv[i].jump   = 0;
                            tv[i].W      = g->matrix[results[i].path[0]][results[i].path[1]];
                            tv[i].entity = pos[results[i].path[0]];
                        }
                    }
                }
            }
        }

        /* ---- animation update for each traveler ---- */
        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                if (!tv[i].active || !results[i].found) continue;
                if (tv[i].state == ANIM_DONE) continue;

                tv[i].timer += dt;
                DijkstraResult *res = &results[i];

                if (tv[i].state == ANIM_MOVING) {
                    if (tv[i].timer >= JUMP_SEC) {
                        tv[i].timer -= JUMP_SEC;
                        tv[i].jump++;

                        if (tv[i].jump >= tv[i].W) {
                            tv[i].path_idx++;
                            tv[i].entity = pos[res->path[tv[i].path_idx]];
                            tv[i].jump   = 0;
                            tv[i].W      = 0;

                            if (tv[i].path_idx == res->path_len - 1) {
                                /* reached destination: signal the child */
                                tv[i].state  = ANIM_DONE;
                                kill(tv[i].pid, SIGTERM);
                            } else {
                                tv[i].state = ANIM_AT_NODE;
                                tv[i].timer = 0.f;
                            }
                        } else {
                            Vector2 from = pos[res->path[tv[i].path_idx]];
                            Vector2 to   = pos[res->path[tv[i].path_idx + 1]];
                            tv[i].entity = v2lerp(from, to, (float)tv[i].jump / tv[i].W);
                        }
                    } else {
                        Vector2 from = pos[res->path[tv[i].path_idx]];
                        Vector2 to   = pos[res->path[tv[i].path_idx + 1]];
                        float   t    = ((float)tv[i].jump + tv[i].timer / JUMP_SEC) / (float)tv[i].W;
                        if (t > 1.f) t = 1.f;
                        tv[i].entity = v2lerp(from, to, t);
                    }
                } else if (tv[i].state == ANIM_AT_NODE) {
                    if (tv[i].timer >= NODE_WAIT_SEC) {
                        tv[i].state  = ANIM_MOVING;
                        tv[i].timer  = 0.f;
                        tv[i].jump   = 0;
                        tv[i].W      = g->matrix[res->path[tv[i].path_idx]][res->path[tv[i].path_idx + 1]];
                        tv[i].entity = pos[res->path[tv[i].path_idx]];
                    }
                }
            }
        }

        /* ---- draw ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 4 - Multiple Travelers | Each color = one traveler",
                 10, 10, 15, DARKGRAY);

        /* edges (gray, no highlight — multiple paths make highlighting noisy) */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;
                draw_arrow(pos[i], pos[j], LIGHTGRAY, 2.0f);
                char ws[8];
                sprintf(ws, "%d", g->matrix[i][j]);
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2),
                    (int)((pos[i].y + pos[j].y) / 2),
                    16, DARKBLUE);
            }
        }

        /* highlight each traveler's path in their own color (thin) */
        for (int t = 0; t < num_travelers; t++) {
            if (!results[t].found) continue;
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < results[t].path_len - 1; k++) {
                int a = results[t].path[k], b = results[t].path[k+1];
                draw_arrow(pos[a], pos[b], pc, 3.0f);
            }
        }

        /* nodes */
        for (int i = 0; i < g->n; i++) {
            DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);
            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 9), 18, BLACK);
        }

        /* travelers */
        for (int t = 0; t < num_travelers; t++) {
            if (!results[t].found || !tv[t].active) continue;
            Color c = traveler_colors[t % 10];
            /* slight offset so overlapping travelers don't fully hide each other */
            Vector2 ep = { tv[t].entity.x + (t % 2 == 0 ? -6.f : 6.f) * (t / 2),
                           tv[t].entity.y + (t % 2 == 0 ? -6.f : 6.f) * (t / 2) };
            /* glow */
            Color glow = c; glow.a = 80;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R, DARKGRAY);
            DrawCircleV(ep, 4, WHITE);
            /* traveler index label */
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, WHITE);
        }

        /* button */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active && tv[i].state != ANIM_DONE) { all_done = 0; break; }

            Color bc; const char *lbl;
            if (all_done)       { bc = DARKGRAY;                    lbl = "RESET"; }
            else if (playing)   { bc = (Color){210,60,60,255};      lbl = "STOP";  }
            else                { bc = (Color){60,170,60,255};       lbl = "PLAY";  }

            DrawRectangleRec(btn, bc);
            DrawRectangleLinesEx(btn, 2, DARKGRAY);
            int lw = MeasureText(lbl, 20);
            DrawText(lbl,
                     (int)(btn.x + btn.width/2  - lw/2),
                     (int)(btn.y + btn.height/2 - 10),
                     20, WHITE);
        }

        /* per-traveler status strip on the right */
        for (int t = 0; t < num_travelers; t++) {
            Color c = traveler_colors[t % 10];
            char line[64];
            const char *status = "idle";
            if      (tv[t].state == ANIM_MOVING)  status = "moving";
            else if (tv[t].state == ANIM_AT_NODE)  status = "waiting";
            else if (tv[t].state == ANIM_DONE)     status = "arrived!";
            sprintf(line, "T%d [%d->%d]: %s", t,
                    results[t].path[0],
                    results[t].path[results[t].path_len - 1],
                    status);
            DrawRectangle(WINDOW_W - 210, 50 + t * 22, 200, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 205, 52 + t * 22, 14, c);
        }

        EndDrawing();
    }

    /* If the window was closed before animation ended, signal remaining children */
    for (int i = 0; i < num_travelers; i++) {
        if (tv[i].active && tv[i].state != ANIM_DONE) {
            kill(tv[i].pid, SIGTERM);
        }
    }

    CloseWindow();
}

/* ============================================================
 * Milestone 5 – IPC-driven multi-traveler GUI
 *
 * Each child sends PipeMsg structs as it walks its own path.
 * The parent reads them non-blocking each frame and drives
 * the animation accordingly. Log lines are printed here so
 * only the parent process writes to the terminal.
 * ============================================================ */
#include <unistd.h>
#include <errno.h>

void draw_graph_multi5(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers) {

    typedef struct {
        int       active;        /* 1 until "finished" message received   */
        AnimState state;
        int       from_node;     /* currently animating FROM this node    */
        int       to_node;       /* currently animating TO  this node     */
        int       jump;
        int       W;
        float     timer;
        Vector2   entity;
        pid_t     pid;
        int       pipe_fd;
        int       src_node;
        int       dst_node;
        int       vis_from[MAX_NODES]; /* edges already traversed */
        int       vis_to[MAX_NODES];
        int       n_vis;
    } Traveler5;

    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    Traveler5 tv[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        tv[i].active    = 1;
        tv[i].state     = ANIM_IDLE;
        tv[i].from_node = t_src[i];
        tv[i].to_node   = -1;
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].entity    = pos[t_src[i]];
        tv[i].pid       = pids[i];
        tv[i].pipe_fd   = fds[i];
        tv[i].src_node  = t_src[i];
        tv[i].dst_node  = t_dst[i];
        tv[i].n_vis     = 0;
    }

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 5");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* --- read one IPC message per traveler per frame (skip if MOVING) --- */
        for (int i = 0; i < num_travelers; i++) {
            if (!tv[i].active || tv[i].state == ANIM_MOVING) continue;

            PipeMsg msg;
            ssize_t n = read(tv[i].pipe_fd, &msg, sizeof(msg));

            if (n == sizeof(msg)) {
                if (msg.current_node < 0) {
                    /* finished signal */
                    printf("[PID=%d] finished\n", (int)tv[i].pid);
                    fflush(stdout);
                    tv[i].active = 0;
                } else if (msg.next_node < 0) {
                    /* arrived at destination */
                    printf("[PID=%d] arrived at node %d | DESTINATION\n",
                           (int)tv[i].pid, msg.current_node);
                    fflush(stdout);
                    tv[i].from_node = msg.current_node;
                    tv[i].entity    = pos[msg.current_node];
                    tv[i].state     = ANIM_DONE;
                } else {
                    /* arrived at intermediate node – start moving to next */
                    printf("[PID=%d] arrived at node %d | next node: %d\n",
                           (int)tv[i].pid, msg.current_node, msg.next_node);
                    fflush(stdout);
                    if (tv[i].n_vis < MAX_NODES) {
                        tv[i].vis_from[tv[i].n_vis] = msg.current_node;
                        tv[i].vis_to  [tv[i].n_vis] = msg.next_node;
                        tv[i].n_vis++;
                    }
                    tv[i].from_node = msg.current_node;
                    tv[i].to_node   = msg.next_node;
                    tv[i].W         = g->matrix[msg.current_node][msg.next_node];
                    tv[i].jump      = 0;
                    tv[i].timer     = 0.f;
                    tv[i].entity    = pos[msg.current_node];
                    tv[i].state     = ANIM_MOVING;
                }
            } else if (n == 0) {
                /* pipe closed unexpectedly – treat as finished */
                tv[i].active = 0;
            }
        }

        /* --- animation update (same jump-based logic as milestone 4) --- */
        for (int i = 0; i < num_travelers; i++) {
            if (tv[i].state != ANIM_MOVING) continue;

            tv[i].timer += dt;

            if (tv[i].timer >= JUMP_SEC) {
                tv[i].timer -= JUMP_SEC;
                tv[i].jump++;

                if (tv[i].jump >= tv[i].W) {
                    /* finished crossing the edge visually */
                    tv[i].entity    = pos[tv[i].to_node];
                    tv[i].from_node = tv[i].to_node;
                    tv[i].state     = ANIM_AT_NODE; /* wait for next IPC msg */
                    tv[i].jump      = 0;
                    tv[i].timer     = 0.f;
                } else {
                    Vector2 from = pos[tv[i].from_node];
                    Vector2 to   = pos[tv[i].to_node];
                    tv[i].entity = v2lerp(from, to,
                        (float)tv[i].jump / (float)tv[i].W);
                }
            } else {
                Vector2 from = pos[tv[i].from_node];
                Vector2 to   = pos[tv[i].to_node];
                float t = ((float)tv[i].jump + tv[i].timer / JUMP_SEC)
                          / (float)tv[i].W;
                if (t > 1.f) t = 1.f;
                tv[i].entity = v2lerp(from, to, t);
            }
        }

        /* --- draw --- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 5 - IPC pipes: each traveler computes its own path",
                 10, 10, 15, DARKGRAY);

        /* base edges */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;
                draw_arrow(pos[i], pos[j], LIGHTGRAY, 2.0f);
                char ws[8];
                sprintf(ws, "%d", g->matrix[i][j]);
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2),
                    (int)((pos[i].y + pos[j].y) / 2),
                    16, DARKBLUE);
            }
        }

        /* highlight edges each traveler has already crossed */
        for (int t = 0; t < num_travelers; t++) {
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < tv[t].n_vis; k++) {
                draw_arrow(pos[tv[t].vis_from[k]],
                           pos[tv[t].vis_to[k]], pc, 3.0f);
            }
        }

        /* nodes */
        for (int i = 0; i < g->n; i++) {
            DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);
            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 9), 18, BLACK);
        }

        /* traveler circles */
        for (int t = 0; t < num_travelers; t++) {
            /* skip only if finished with no path (never moved) */
            if (!tv[t].active && tv[t].state == ANIM_IDLE) continue;

            Color c = traveler_colors[t % 10];
            if (!tv[t].active) c.a = 160; /* faded when fully done */

            Vector2 ep = {
                tv[t].entity.x + (t % 2 == 0 ? -6.f : 6.f) * (t / 2),
                tv[t].entity.y + (t % 2 == 0 ? -6.f : 6.f) * (t / 2)
            };
            Color glow = c; glow.a = tv[t].active ? 80 : 40;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R, DARKGRAY);
            DrawCircleV(ep, 4, WHITE);
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, WHITE);
        }

        /* per-traveler status strip */
        for (int t = 0; t < num_travelers; t++) {
            Color c = traveler_colors[t % 10];
            const char *status = "waiting";
            if (!tv[t].active)                      status = "done";
            else if (tv[t].state == ANIM_MOVING)    status = "moving";
            else if (tv[t].state == ANIM_AT_NODE)   status = "at node";
            else if (tv[t].state == ANIM_DONE)      status = "arrived!";
            char line[64];
            sprintf(line, "T%d [%d->%d]: %s", t,
                    tv[t].src_node, tv[t].dst_node, status);
            DrawRectangle(WINDOW_W - 210, 50 + t * 22, 200, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 205, 52 + t * 22, 14, c);
        }

        /* banner when all travelers have finished */
        int all_done = 1;
        for (int i = 0; i < num_travelers; i++)
            if (tv[i].active) { all_done = 0; break; }

        if (all_done) {
            const char *banner = "  All travelers have arrived!  ";
            int mw = MeasureText(banner, 24);
            DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 24,
                          mw + 20, 48, Fade(BLACK, 0.65f));
            DrawText(banner, WINDOW_W/2 - mw/2, WINDOW_H/2 - 12, 24, YELLOW);
        }

        EndDrawing();
    }

    /* signal any children still running (e.g. window closed early) */
    for (int i = 0; i < num_travelers; i++) {
        if (tv[i].active) kill(tv[i].pid, SIGTERM);
    }

    CloseWindow();
}

/* ============================================================
 * Milestone 6 – mutual exclusion: at most one traveler per node
 *
 * MSG_WAITING  → traveler drawn in YELLOW outside target node
 * MSG_AT_NODE  → traveler drawn in its color INSIDE the node
 * MSG_FINISHED → traveler removed from display
 * ============================================================ */

typedef enum {
    T6_IDLE,      /* not started yet                  */
    T6_TRAVELING, /* moving along an edge             */
    T6_WAITING,   /* blocked outside a node           */
    T6_AT_NODE,   /* inside the node (critical sec.)  */
    T6_DONE       /* reached destination              */
} T6State;

typedef struct {
    int      active;
    T6State  state;
    int      cur_node;   /* node currently at / waiting for  */
    int      next_node;
    int      jump;
    int      W;
    float    timer;
    Vector2  entity;
    pid_t    pid;
    int      pipe_fd;
    int      src_node;
    int      dst_node;
    /* trail of edges already traversed */
    int      vis_from[MAX_NODES];
    int      vis_to[MAX_NODES];
    int      n_vis;
} Traveler6;

/* Yellow color for waiting travelers */
#define WAITING_COLOR  ((Color){255, 220, 0, 255})
#define WAITING_GLOW   ((Color){255, 220, 0, 80})

void draw_graph_multi6(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers) {

    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    Traveler6 tv[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {
        tv[i].active    = 1;
        tv[i].state     = T6_IDLE;
        tv[i].cur_node  = t_src[i];
        tv[i].next_node = -1;
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].entity    = pos[t_src[i]];
        tv[i].pid       = pids[i];
        tv[i].pipe_fd   = fds[i];
        tv[i].src_node  = t_src[i];
        tv[i].dst_node  = t_dst[i];
        tv[i].n_vis     = 0;
    }

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 6 (Mutex)");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* --- read IPC messages from each traveler --- */
        for (int i = 0; i < num_travelers; i++) {
            if (!tv[i].active) continue;
            /* read all available messages this frame */
            PipeMsg msg;
            while (read(tv[i].pipe_fd, &msg, sizeof(msg)) == sizeof(msg)) {
                switch (msg.type) {

                    case MSG_WAITING:
                        /* blocked outside msg.current_node */
                        tv[i].state     = T6_WAITING;
                        tv[i].cur_node  = msg.current_node;
                        tv[i].next_node = msg.next_node;
                        /* position slightly outside the target node */
                        {
                            Vector2 target = pos[msg.current_node];
                            /* offset toward previous node so it looks
                               like it stopped just before entering    */
                            int prev = tv[i].cur_node;
                            Vector2 from = (tv[i].n_vis > 0)
                                ? pos[tv[i].vis_from[tv[i].n_vis - 1]]
                                : pos[tv[i].src_node];
                            tv[i].entity = v2lerp(target, from, 0.3f);
                        }
                        printf("[PID=%d] WAITING outside node %d\n",
                               (int)tv[i].pid, msg.current_node);
                        fflush(stdout);
                        break;

                    case MSG_AT_NODE:
                        /* acquired mutex — now inside the node */
                        tv[i].state    = T6_AT_NODE;
                        tv[i].cur_node = msg.current_node;
                        tv[i].entity   = pos[msg.current_node];
                        /* record traversed edge */
                        if (tv[i].n_vis > 0) {
                            int last = tv[i].vis_from[tv[i].n_vis - 1];
                            if (last != msg.current_node &&
                                tv[i].n_vis < MAX_NODES) {
                                tv[i].vis_from[tv[i].n_vis] = last;
                                tv[i].vis_to  [tv[i].n_vis] = msg.current_node;
                                tv[i].n_vis++;
                            }
                        } else {
                            /* first node: just record position */
                        }
                        /* start edge travel animation toward next node */
                        if (msg.next_node >= 0) {
                            tv[i].next_node = msg.next_node;
                            tv[i].W         = g->matrix[msg.current_node]
                                                       [msg.next_node];
                            tv[i].jump      = 0;
                            tv[i].timer     = 0.f;
                            tv[i].state     = T6_TRAVELING;
                        }
                        printf("[PID=%d] ENTERED node %d\n",
                               (int)tv[i].pid, msg.current_node);
                        fflush(stdout);
                        break;

                    case MSG_FINISHED:
                        /* traveler is done */
                        tv[i].state  = T6_DONE;
                        tv[i].active = 0;
                        printf("[PID=%d] FINISHED\n", (int)tv[i].pid);
                        fflush(stdout);
                        break;

                    default:
                        break;
                }
            }
        }

        /* --- animation: smooth movement along edges --- */
        for (int i = 0; i < num_travelers; i++) {
            if (tv[i].state != T6_TRAVELING) continue;

            tv[i].timer += dt;
            Vector2 from = pos[tv[i].cur_node];
            Vector2 to   = pos[tv[i].next_node];

            if (tv[i].W <= 0) tv[i].W = 1;

            if (tv[i].timer >= JUMP_SEC) {
                tv[i].timer -= JUMP_SEC;
                tv[i].jump++;
                if (tv[i].jump >= tv[i].W) {
                    /* arrived visually — wait for MSG_WAITING or MSG_AT_NODE */
                    tv[i].entity    = to;
                    tv[i].cur_node  = tv[i].next_node;
                    tv[i].jump      = 0;
                    tv[i].state     = T6_IDLE;
                } else {
                    tv[i].entity = v2lerp(from, to,
                        (float)tv[i].jump / tv[i].W);
                }
            } else {
                float t = ((float)tv[i].jump + tv[i].timer / JUMP_SEC)
                          / (float)tv[i].W;
                if (t > 1.f) t = 1.f;
                tv[i].entity = v2lerp(from, to, t);
            }
        }

        /* --- draw --- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 6 – Mutex: at most one traveler per node | YELLOW = waiting",
                 10, 10, 15, DARKGRAY);

        /* base edges */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;
                draw_arrow(pos[i], pos[j], LIGHTGRAY, 2.0f);
                char ws[8];
                sprintf(ws, "%d", g->matrix[i][j]);
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2),
                    (int)((pos[i].y + pos[j].y) / 2),
                    16, DARKBLUE);
            }
        }

        /* highlight traversed edges per traveler */
        for (int t = 0; t < num_travelers; t++) {
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < tv[t].n_vis; k++) {
                draw_arrow(pos[tv[t].vis_from[k]],
                           pos[tv[t].vis_to[k]], pc, 3.0f);
            }
        }

        /* nodes — highlight occupied nodes with a red ring */
        for (int i = 0; i < g->n; i++) {
            /* check if any traveler is AT this node */
            int occupied = 0;
            for (int t = 0; t < num_travelers; t++) {
                if (tv[t].active &&
                    tv[t].state == T6_AT_NODE &&
                    tv[t].cur_node == i) { occupied = 1; break; }
            }
            /* check if any traveler is WAITING for this node */
            int contested = 0;
            for (int t = 0; t < num_travelers; t++) {
                if (tv[t].active &&
                    tv[t].state == T6_WAITING &&
                    tv[t].cur_node == i) { contested = 1; break; }
            }

            Color nc = SKYBLUE;
            if (occupied)  nc = (Color){255, 100, 100, 255}; /* red: locked  */
            if (contested && !occupied) nc = (Color){255, 200, 80, 255}; /* amber: contested */

            DrawCircleV(pos[i], NODE_RADIUS, nc);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);

            /* extra ring when occupied */
            if (occupied)
                DrawCircleLines(pos[i].x, pos[i].y,
                                NODE_RADIUS + 5, RED);

            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl,
                     (int)(pos[i].x - tw/2),
                     (int)(pos[i].y - 9),
                     18, BLACK);
        }

        /* traveler circles */
        for (int t = 0; t < num_travelers; t++) {
            if (!tv[t].active && tv[t].state != T6_DONE) continue;
            if (tv[t].state == T6_IDLE && !tv[t].active) continue;

            /* waiting travelers are drawn in yellow */
            Color c = (tv[t].state == T6_WAITING)
                      ? WAITING_COLOR
                      : traveler_colors[t % 10];

            if (!tv[t].active) c.a = 140; /* faded when done */

            /* small offset so overlapping travelers are visible */
            Vector2 ep = {
                tv[t].entity.x + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1),
                tv[t].entity.y + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1)
            };

            Color glow = c; glow.a = tv[t].active ? 80 : 30;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R,
                            (tv[t].state == T6_WAITING) ? YELLOW : DARKGRAY);
            DrawCircleV(ep, 4, WHITE);

            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, BLACK);
        }

        /* per-traveler status strip */
        for (int t = 0; t < num_travelers; t++) {
            Color c = traveler_colors[t % 10];
            const char* status = "idle";
            if      (tv[t].state == T6_TRAVELING) status = "moving";
            else if (tv[t].state == T6_WAITING)   status = "WAITING (blocked)";
            else if (tv[t].state == T6_AT_NODE)   status = "in node (locked)";
            else if (tv[t].state == T6_DONE)       status = "arrived!";
            char line[64];
            sprintf(line, "T%d [%d->%d]: %s", t,
                    tv[t].src_node, tv[t].dst_node, status);
            DrawRectangle(WINDOW_W - 220, 50 + t * 22, 210, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 215, 52 + t * 22, 14, c);
        }

        /* legend */
        DrawCircle(20, WINDOW_H - 70, 8, (Color){255,100,100,255});
        DrawText("node locked",      34, WINDOW_H - 77, 13, DARKGRAY);
        DrawCircle(20, WINDOW_H - 50, 8, WAITING_COLOR);
        DrawText("traveler waiting", 34, WINDOW_H - 57, 13, DARKGRAY);
        DrawCircle(20, WINDOW_H - 30, 8, traveler_colors[0]);
        DrawText("traveler moving",  34, WINDOW_H - 37, 13, DARKGRAY);

        EndDrawing();
    }

    /* signal any children still running */
    for (int i = 0; i < num_travelers; i++)
        if (tv[i].active) kill(tv[i].pid, SIGTERM);

    CloseWindow();
}