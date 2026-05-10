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
