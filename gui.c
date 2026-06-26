#include "gui.h"                                    // PipeMsg, MsgType, MAX_TRAVELERS, function declarations
#include "raylib.h"                                 // raylib window, drawing, and input functions
#include <math.h>                                   // cosf(), sinf(), sqrtf(), atan2f()
#include <stdio.h>                                  // sprintf(), printf(), fflush()
#include <string.h>                                 // strcat(), strcmp(), snprintf()

#define NODE_RADIUS  25                             // radius in pixels of each drawn node circle
#define ENTITY_R     14                             // radius in pixels of the animated traveler dot
#define WINDOW_W     800                            // width of the raylib window in pixels
#define WINDOW_H     600                            // height of the raylib window in pixels
#define PI           3.14159265358979323846f        // pi constant used for arranging nodes in a circle

#define BTN_X        20                             // x position of the Play/Stop/Reset button
#define BTN_Y        50                             // y position of the Play/Stop/Reset button
#define BTN_W        100                            // width of the button in pixels
#define BTN_H        36                             // height of the button in pixels

#define NODE_WAIT_SEC  1.0f                         // seconds a traveler waits at each intermediate node
#define JUMP_SEC       0.3f                         // seconds each single jump along an edge takes (300 ms)

typedef enum {                                      // states for the single-traveler animation state machine
    ANIM_IDLE,                                      // animation has not started yet (or was reset)
    ANIM_AT_NODE,                                   // traveler is sitting at a node, waiting 1 second
    ANIM_MOVING,                                    // traveler is moving along an edge in discrete jumps
    ANIM_DONE                                       // traveler has reached the destination
} AnimState;

/* Compute screen positions for n nodes arranged evenly around a circle */
static void get_node_positions(int n, Vector2 *pos) {
    float cx = WINDOW_W / 2.0f, cy = WINDOW_H / 2.0f, r = 200.0f; // centre of window and circle radius
    for (int i = 0; i < n; i++) {                   // place each node at equal angular intervals
        float a = (2 * PI * i) / n - PI / 2;        // angle for node i; subtract PI/2 so node 0 is at the top
        pos[i].x = cx + r * cosf(a);               // x coordinate on the circle
        pos[i].y = cy + r * sinf(a);               // y coordinate on the circle
    }
}

/* Draw a directed arrow from point s to point e with the given color and thickness */
static void draw_arrow(Vector2 s, Vector2 e, Color c, float thick) {
    float dx = e.x - s.x, dy = e.y - s.y;         // vector from s to e
    float len = sqrtf(dx*dx + dy*dy);              // length of that vector
    if (len < 0.001f) return;                       // skip zero-length arrows to avoid division by zero
    float ux = dx/len, uy = dy/len;                // unit vector in the direction s→e
    Vector2 ls = { s.x + ux*NODE_RADIUS, s.y + uy*NODE_RADIUS }; // start of the line (offset past the source node circle)
    Vector2 le = { e.x - ux*NODE_RADIUS, e.y - uy*NODE_RADIUS }; // end of the line (offset before the destination circle)
    DrawLineEx(ls, le, thick, c);                  // draw the shaft of the arrow
    float as = 10.0f, ang = atan2f(dy, dx);        // arrowhead size and angle of the direction
    Vector2 tip   = le;                             // tip of the arrowhead is at the end of the line
    Vector2 left  = { tip.x - as*cosf(ang-0.4f), tip.y - as*sinf(ang-0.4f) }; // left wing of the arrowhead
    Vector2 right = { tip.x - as*cosf(ang+0.4f), tip.y - as*sinf(ang+0.4f) }; // right wing of the arrowhead
    DrawTriangle(tip, left, right, c);             // draw the filled triangle arrowhead
}

/* Returns 1 if the directed edge i→j is part of the shortest path stored in res */
static int is_path_edge(DijkstraResult *res, int i, int j) {
    for (int k = 0; k < res->path_len - 1; k++)   // check each consecutive pair in the path
        if (res->path[k] == i && res->path[k+1] == j) return 1; // this edge is on the path
    return 0;                                       // edge is not on the shortest path
}

/* Linear interpolation between two Vector2 points; t=0 → a, t=1 → b */
static Vector2 v2lerp(Vector2 a, Vector2 b, float t) {
    return (Vector2){ a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t }; // standard lerp formula applied to x and y
}

/* =====================================================================
 * Milestone 3 – single-traveler animated GUI
 * ===================================================================== */
void draw_graph(Graph *g, DijkstraResult *res) {
    Vector2 pos[MAX_NODES];                         // screen positions for each node
    get_node_positions(g->n, pos);                  // arrange them evenly around a circle

    int        playing     = 0;                     // 0 = paused, 1 = animation is running
    AnimState  state       = ANIM_IDLE;             // current state of the animation
    int        path_idx    = 0;                     // index into res->path of the node we are at right now
    int        jump        = 0;                     // how many discrete jumps we have taken on the current edge
    int        W           = 0;                     // weight (number of jumps) of the current edge
    float      timer       = 0.f;                   // accumulates elapsed time in the current state
    Vector2    entity      = (res->found && res->path_len > 0)
                             ? pos[res->path[0]]    // start the entity at the first node of the path
                             : (Vector2){0,0};      // fallback if no path was found

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 3"); // open the raylib window
    SetTargetFPS(60);                               // cap the frame rate at 60 frames per second

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H }; // rectangle used for hit-testing and drawing the button

    while (!WindowShouldClose()) {                  // main loop: runs once per frame until the user closes the window
        float dt = GetFrameTime();                  // time in seconds since the last frame

        /* ---- button click: play / stop / reset ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) { // check if the click landed on the button

            if (state == ANIM_DONE) {               // animation finished — button now acts as RESET
                playing   = 0;
                state     = ANIM_IDLE;
                path_idx  = 0;
                jump      = 0; W = 0; timer = 0.f; // reset all animation variables
                if (res->found && res->path_len > 0)
                    entity = pos[res->path[0]];     // move entity back to the starting node
            } else {
                playing = !playing;                 // toggle between playing and paused
                if (playing && state == ANIM_IDLE &&
                    res->found && res->path_len > 1) { // kick off the first edge when pressing play from idle
                    state = ANIM_MOVING;
                    timer = 0.f;
                    jump  = 0;
                    W     = g->matrix[res->path[0]][res->path[1]]; // weight of the first edge
                    entity = pos[res->path[0]];     // start position is node 0
                }
            }
        }

        /* ---- animation update (only when playing and a path exists) ---- */
        if (playing && res->found) {
            timer += dt;                            // accumulate time in the current state

            if (state == ANIM_MOVING) {
                if (timer >= JUMP_SEC) {            // enough time has passed for the next discrete jump
                    timer -= JUMP_SEC;              // subtract the jump duration (keeps leftover time)
                    jump++;                         // advance one jump along the edge

                    if (jump >= W) {                // completed all jumps on this edge: arrived at next node
                        path_idx++;                 // move to the next node in the path
                        entity = pos[res->path[path_idx]]; // snap entity to the node's screen position
                        jump   = 0; W = 0;

                        if (path_idx == res->path_len - 1) { // reached the destination node
                            state   = ANIM_DONE;
                            playing = 0;
                        } else {                    // intermediate node: pause for 1 second
                            state = ANIM_AT_NODE;
                            timer = 0.f;
                        }
                    } else {
                        /* snap entity to the exact discrete jump position on the edge */
                        Vector2 from = pos[res->path[path_idx]];
                        Vector2 to   = pos[res->path[path_idx + 1]];
                        entity = v2lerp(from, to, (float)jump / W); // position is jump/W fraction along the edge
                    }
                } else {
                    /* smoothly interpolate the entity within the 300ms jump window so it glides visually */
                    Vector2 from = pos[res->path[path_idx]];
                    Vector2 to   = pos[res->path[path_idx + 1]];
                    float   t    = ((float)jump + timer / JUMP_SEC) / (float)W; // fractional position including sub-jump progress
                    if (t > 1.f) t = 1.f;          // clamp to avoid overshooting the destination
                    entity = v2lerp(from, to, t);
                }

            } else if (state == ANIM_AT_NODE) {
                if (timer >= NODE_WAIT_SEC) {        // 1 second wait at the intermediate node is over
                    state = ANIM_MOVING;
                    timer = 0.f;
                    jump  = 0;
                    W     = g->matrix[res->path[path_idx]][res->path[path_idx + 1]]; // weight of the next edge
                    entity = pos[res->path[path_idx]]; // snap to node before starting the next edge
                }
            }
        }

        /* ---- drawing ---- */
        BeginDrawing();                             // start the raylib frame
        ClearBackground(RAYWHITE);                  // fill background with white

        DrawText("Green = source | Orange = destination | Red = shortest path",
                 10, 10, 15, DARKGRAY);             // legend at the top of the window

        /* draw all edges */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue; // skip non-existent edges
                Color ec    = LIGHTGRAY;             // default edge color
                float thick = 2.0f;                  // default edge thickness
                if (res->found && is_path_edge(res, i, j)) { // highlight edges that are on the shortest path
                    ec    = RED;
                    thick = 3.0f;
                }
                draw_arrow(pos[i], pos[j], ec, thick); // draw the directed edge as an arrow
                char ws[8];
                sprintf(ws, "%d", g->matrix[i][j]); // format edge weight as string
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2), // midpoint x of the edge
                    (int)((pos[i].y + pos[j].y) / 2), // midpoint y of the edge
                    16, DARKBLUE);                   // draw the weight label in dark blue
            }
        }

        /* draw all nodes */
        for (int i = 0; i < g->n; i++) {
            Color nc = SKYBLUE;                      // default node color
            if (res->found && res->path_len > 0) {
                if (i == res->path[0])               nc = GREEN;   // source node is green
                if (i == res->path[res->path_len-1]) nc = ORANGE;  // destination node is orange
            }
            DrawCircleV(pos[i], NODE_RADIUS, nc);   // draw the filled node circle
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY); // draw a dark outline around the node
            char lbl[4];
            sprintf(lbl, "%d", i);                  // format the node index as a string
            int tw = MeasureText(lbl, 18);          // measure text width to center it inside the circle
            DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 9), 18, BLACK); // draw the label centered in the node
        }

        /* draw the animated traveler entity on top of everything else */
        if (res->found && res->path_len > 0) {
            DrawCircleV(entity, ENTITY_R + 4, (Color){120, 50, 200, 80}); // faint glow ring around the entity
            DrawCircleV(entity, ENTITY_R,     (Color){160, 32, 240, 255}); // main entity circle (purple)
            DrawCircleLines(entity.x, entity.y, ENTITY_R, (Color){80, 0, 140, 255}); // dark outline
            DrawCircleV(entity, 4, WHITE);           // small white dot at the center of the entity
        }

        /* draw the Play / Stop / Reset button */
        {
            Color bc;
            const char *lbl;
            if (state == ANIM_DONE) {
                bc  = DARKGRAY;  lbl = "RESET";     // animation done: button resets
            } else if (playing) {
                bc  = (Color){210, 60, 60, 255}; lbl = "STOP"; // currently playing: button stops
            } else {
                bc  = (Color){60, 170, 60, 255}; lbl = "PLAY"; // paused/idle: button plays
            }
            DrawRectangleRec(btn, bc);              // draw the filled button rectangle
            DrawRectangleLinesEx(btn, 2, DARKGRAY); // draw a dark border around the button
            int lw = MeasureText(lbl, 20);          // measure label width to center it
            DrawText(lbl,
                     (int)(btn.x + btn.width/2  - lw/2),  // center horizontally in button
                     (int)(btn.y + btn.height/2 - 10),     // center vertically in button
                     20, WHITE);
        }

        /* draw status text next to the button */
        {
            const char *st = "";
            if (!res->found)                 st = "No path";
            else if (state == ANIM_IDLE)     st = "Press PLAY to start";
            else if (state == ANIM_AT_NODE)  st = "Waiting at node...";
            else if (state == ANIM_MOVING)   st = "Moving...";
            else if (state == ANIM_DONE)     st = "Arrived!";
            DrawText(st, BTN_X + BTN_W + 12, BTN_Y + 9, 16, DARKGRAY); // draw to the right of the button
        }

        /* draw path info strip at the bottom of the window */
        if (res->found) {
            char info[256] = "Path: ";
            char tmp[8];
            for (int i = 0; i < res->path_len; i++) {
                if (i > 0) strcat(info, " -> ");   // separator between nodes
                sprintf(tmp, "%d", res->path[i]);
                strcat(info, tmp);                  // append each node index to the string
            }
            char w[32];
            sprintf(w, "  |  Total weight: %d", res->total_weight);
            strcat(info, w);                        // append total weight at the end
            DrawText(info, 10, WINDOW_H - 30, 16, DARKGRAY);
        } else {
            DrawText("No path found", 10, WINDOW_H - 30, 16, RED);
        }

        /* draw "Destination reached!" banner when animation finishes */
        if (state == ANIM_DONE) {
            const char *msg = "  Destination reached!  ";
            int mw = MeasureText(msg, 28);
            DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 28,
                          mw + 20, 56, Fade(BLACK, 0.65f)); // semi-transparent dark background behind text
            DrawText(msg, WINDOW_W/2 - mw/2, WINDOW_H/2 - 14, 28, YELLOW);
        }

        EndDrawing();                               // end the frame (swaps buffers)
    }

    CloseWindow();                                  // close the raylib window and clean up
}

/* =====================================================================
 * Milestone 4 – multi-traveler GUI (parent pre-computes all paths)
 * ===================================================================== */
#include <signal.h>                                 // kill() to send SIGTERM to child processes
#include <sys/types.h>                             // pid_t

/* One distinct color per traveler (up to 10) */
static Color traveler_colors[] = {
    {160,  32, 240, 255},  /* purple        */
    { 30, 144, 255, 255},  /* dodger blue   */
    {255, 140,   0, 255},  /* orange        */
    { 50, 205,  50, 255},  /* lime green    */
    {220,  20,  60, 255},  /* crimson       */
    {  0, 206, 209, 255},  /* dark turquoise*/
    {255, 215,   0, 255},  /* gold          */
    {199,  21, 133, 255},  /* violet red    */
    {127, 255,   0, 255},  /* chartreuse    */
    {255, 105, 180, 255},  /* hot pink      */
};

/* Animation state for one traveler */
typedef struct {
    int        active;      /* 1 = still moving, 0 = done or no path          */
    AnimState  state;       /* current stage in the animation state machine    */
    int        path_idx;    /* index of the node we are currently at           */
    int        jump;        /* how many jumps taken on the current edge        */
    int        W;           /* weight (total jumps) of the current edge        */
    float      timer;       /* elapsed time in the current state               */
    Vector2    entity;      /* current screen position of the traveler dot     */
    pid_t      pid;         /* PID of the child process representing this traveler */
} TravelerAnim;

void draw_graph_multi(Graph* g, DijkstraResult* results, pid_t* pids, int num_travelers) {
    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);                  // compute screen positions for all nodes

    TravelerAnim tv[MAX_TRAVELERS];                 // one animation struct per traveler
    int playing = 0;                                // 0 = paused, 1 = all travelers animating

    for (int i = 0; i < num_travelers; i++) {       // initialise each traveler's animation state
        tv[i].active    = 1;
        tv[i].state     = ANIM_IDLE;
        tv[i].path_idx  = 0;
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].pid       = pids[i];                  // remember the child PID so we can kill it later
        tv[i].entity    = (results[i].found && results[i].path_len > 0)
                          ? pos[results[i].path[0]] // start at the first node of this traveler's path
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

            int all_done = 1;                       // check whether every traveler has finished
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active && tv[i].state != ANIM_DONE) { all_done = 0; break; }

            if (all_done) {                         // all done: button acts as RESET
                playing = 0;
                for (int i = 0; i < num_travelers; i++) { // reset all traveler animation states
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
                playing = !playing;                 // toggle play/pause
                if (playing) {
                    for (int i = 0; i < num_travelers; i++) { // kick off every idle traveler simultaneously
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

        /* ---- animation update: advance each traveler independently ---- */
        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                if (!tv[i].active || !results[i].found) continue; // skip inactive or unreachable travelers
                if (tv[i].state == ANIM_DONE) continue;           // skip travelers who already arrived

                tv[i].timer += dt;
                DijkstraResult *res = &results[i]; // shorthand pointer to this traveler's path

                if (tv[i].state == ANIM_MOVING) {
                    if (tv[i].timer >= JUMP_SEC) {  // time for the next discrete jump
                        tv[i].timer -= JUMP_SEC;
                        tv[i].jump++;

                        if (tv[i].jump >= tv[i].W) { // arrived at the next node
                            tv[i].path_idx++;
                            tv[i].entity = pos[res->path[tv[i].path_idx]];
                            tv[i].jump   = 0;
                            tv[i].W      = 0;

                            if (tv[i].path_idx == res->path_len - 1) { // reached destination
                                tv[i].state  = ANIM_DONE;
                                kill(tv[i].pid, SIGTERM);               // signal the child process to terminate
                            } else {
                                tv[i].state = ANIM_AT_NODE;             // wait 1 second at intermediate node
                                tv[i].timer = 0.f;
                            }
                        } else {
                            Vector2 from = pos[res->path[tv[i].path_idx]];
                            Vector2 to   = pos[res->path[tv[i].path_idx + 1]];
                            tv[i].entity = v2lerp(from, to, (float)tv[i].jump / tv[i].W); // discrete jump position
                        }
                    } else {
                        /* smooth interpolation within the 300ms window */
                        Vector2 from = pos[res->path[tv[i].path_idx]];
                        Vector2 to   = pos[res->path[tv[i].path_idx + 1]];
                        float   t    = ((float)tv[i].jump + tv[i].timer / JUMP_SEC) / (float)tv[i].W;
                        if (t > 1.f) t = 1.f;
                        tv[i].entity = v2lerp(from, to, t);
                    }
                } else if (tv[i].state == ANIM_AT_NODE) {
                    if (tv[i].timer >= NODE_WAIT_SEC) { // done waiting at intermediate node
                        tv[i].state  = ANIM_MOVING;
                        tv[i].timer  = 0.f;
                        tv[i].jump   = 0;
                        tv[i].W      = g->matrix[res->path[tv[i].path_idx]][res->path[tv[i].path_idx + 1]];
                        tv[i].entity = pos[res->path[tv[i].path_idx]];
                    }
                }
            }
        }

        /* ---- drawing ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 4 - Multiple Travelers | Each color = one traveler",
                 10, 10, 15, DARKGRAY);

        /* draw all edges in light gray (no path highlight — too noisy with multiple paths) */
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

        /* highlight each traveler's path in their own semi-transparent color */
        for (int t = 0; t < num_travelers; t++) {
            if (!results[t].found) continue;
            Color pc = traveler_colors[t % 10];
            pc.a = 160;                             // semi-transparent so overlapping paths stay visible
            for (int k = 0; k < results[t].path_len - 1; k++) {
                int a = results[t].path[k], b = results[t].path[k+1];
                draw_arrow(pos[a], pos[b], pc, 3.0f); // draw each edge of this traveler's path
            }
        }

        /* draw nodes */
        for (int i = 0; i < g->n; i++) {
            DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);
            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2), (int)(pos[i].y - 9), 18, BLACK);
        }

        /* draw each traveler entity with a slight offset so they don't fully overlap */
        for (int t = 0; t < num_travelers; t++) {
            if (!results[t].found || !tv[t].active) continue;
            Color c = traveler_colors[t % 10];
            Vector2 ep = { tv[t].entity.x + (t % 2 == 0 ? -6.f : 6.f) * (t / 2), // stagger left/right by index
                           tv[t].entity.y + (t % 2 == 0 ? -6.f : 6.f) * (t / 2) }; // stagger up/down by index
            Color glow = c; glow.a = 80;
            DrawCircleV(ep, ENTITY_R + 4, glow);   // glow ring
            DrawCircleV(ep, ENTITY_R, c);           // main colored circle
            DrawCircleLines(ep.x, ep.y, ENTITY_R, DARKGRAY);
            DrawCircleV(ep, 4, WHITE);              // center dot
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, WHITE); // traveler index label
        }

        /* draw the Play / Stop / Reset button */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active && tv[i].state != ANIM_DONE) { all_done = 0; break; }

            Color bc; const char *lbl;
            if (all_done)       { bc = DARKGRAY;               lbl = "RESET"; }
            else if (playing)   { bc = (Color){210,60,60,255}; lbl = "STOP";  }
            else                { bc = (Color){60,170,60,255};  lbl = "PLAY";  }

            DrawRectangleRec(btn, bc);
            DrawRectangleLinesEx(btn, 2, DARKGRAY);
            int lw = MeasureText(lbl, 20);
            DrawText(lbl,
                     (int)(btn.x + btn.width/2  - lw/2),
                     (int)(btn.y + btn.height/2 - 10),
                     20, WHITE);
        }

        /* per-traveler status strip on the right side */
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
                          (Color){c.r, c.g, c.b, 40}); // faint background in traveler's color
            DrawText(line, WINDOW_W - 205, 52 + t * 22, 14, c);
        }

        EndDrawing();
    }

    /* if the window was closed before all travelers finished, kill remaining children */
    for (int i = 0; i < num_travelers; i++) {
        if (tv[i].active && tv[i].state != ANIM_DONE) {
            kill(tv[i].pid, SIGTERM);               // send SIGTERM to clean up the child process
        }
    }

    CloseWindow();
}

/* =====================================================================
 * Milestone 5 – IPC-driven multi-traveler GUI
 * Each child sends PipeMsg structs through a pipe; parent reads them
 * non-blocking each frame and drives the animation accordingly.
 * ===================================================================== */
#include <unistd.h>                                 // read()
#include <errno.h>                                  // EAGAIN (returned by non-blocking reads with no data)

void draw_graph_multi5(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers) {

    /* Per-traveler state for milestone 5 */
    typedef struct {
        int       active;       /* 1 = still travelling                            */
        AnimState state;        /* animation state machine stage                   */
        int       from_node;    /* node the traveler is currently moving away from */
        int       to_node;      /* node the traveler is currently moving toward    */
        int       jump;         /* jumps taken on the current edge                 */
        int       W;            /* total jumps for the current edge (= edge weight)*/
        float     timer;        /* time elapsed in the current state               */
        Vector2   entity;       /* current screen position                         */
        pid_t     pid;          /* PID of the child process                        */
        int       pipe_fd;      /* read end of this traveler's pipe                */
        int       src_node;     /* starting node (for the status strip)            */
        int       dst_node;     /* destination node (for the status strip)         */
        int       vis_from[MAX_NODES]; /* edge start nodes that have been traversed*/
        int       vis_to[MAX_NODES];   /* edge end nodes that have been traversed  */
        int       n_vis;        /* number of traversed edges recorded              */
    } Traveler5;

    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    Traveler5 tv[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {       // initialise each traveler
        tv[i].active    = 1;
        tv[i].state     = ANIM_IDLE;
        tv[i].from_node = t_src[i];                 // start at the source node
        tv[i].to_node   = -1;                       // no destination yet
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].entity    = pos[t_src[i]];            // screen position = source node
        tv[i].pid       = pids[i];
        tv[i].pipe_fd   = fds[i];                   // non-blocking read end of this traveler's pipe
        tv[i].src_node  = t_src[i];
        tv[i].dst_node  = t_dst[i];
        tv[i].n_vis     = 0;
    }

    int playing = 0;

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 5");
    SetTargetFPS(60);

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- button click: toggle play/pause ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {
            playing = !playing;                     // simple toggle; no reset needed (children run independently)
        }

        /* ---- read IPC messages and animate (only when playing) ---- */
        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                if (!tv[i].active) continue;        // skip travelers that already finished
                if (tv[i].state == ANIM_MOVING) continue; // skip mid-animation — wait until edge is done

                PipeMsg msg;
                ssize_t n = read(tv[i].pipe_fd, &msg, sizeof(msg)); // non-blocking read; returns -1 if no data
                if (n != (ssize_t)sizeof(msg)) continue;             // incomplete or no message; try again next frame

                if (msg.type == MSG_NO_PATH){
                    printf("[PID=%d] No path exists from %d to %d\n", (int)tv[i].pid, msg.current_node, msg.next_node);
                    fflush(stdout);
                }else if (msg.type == MSG_FINISHED) {      // child signalled it is done
                    printf("[PID=%d] finished\n", (int)tv[i].pid);
                    fflush(stdout);
                    tv[i].active = 0;               // mark traveler as inactive
                else if (msg.type == MSG_MOVING) {
                    printf("[PID=%d] moving from %d to %d\n",  (int)tv[i].pid, msg.current_node, msg.next_node);
                    fflush(stdout);
                } else if (msg.type == MSG_AT_NODE && msg.next_node < 0) { // arrived at destination
                    printf("[PID=%d] arrived at node %d | DESTINATION\n",
                           (int)tv[i].pid, msg.current_node);
                    fflush(stdout);
                    tv[i].from_node = msg.current_node;
                    tv[i].entity    = pos[msg.current_node]; // snap entity to destination
                    tv[i].state     = ANIM_DONE;
                    tv[i].active    = 0;
                } else if (msg.type == MSG_AT_NODE) { // at an intermediate node, will move to next_node
                    printf("[PID=%d] at node %d | next: %d\n",
                           (int)tv[i].pid, msg.current_node, msg.next_node);
                    fflush(stdout);
                    if (tv[i].n_vis < MAX_NODES) {  // record this edge as traversed for the colored path display
                        tv[i].vis_from[tv[i].n_vis] = msg.current_node;
                        tv[i].vis_to  [tv[i].n_vis] = msg.next_node;
                        tv[i].n_vis++;
                    }
                    tv[i].from_node = msg.current_node;
                    tv[i].to_node   = msg.next_node;
                    tv[i].W         = g->matrix[msg.current_node][msg.next_node]; // edge weight = number of jumps
                    if (tv[i].W < 1) tv[i].W = 1;  // guard against 0-weight edges to avoid division by zero
                    tv[i].jump      = 0;
                    tv[i].timer     = 0.f;
                    tv[i].entity    = pos[msg.current_node]; // start entity at the current node
                    tv[i].state     = ANIM_MOVING;  // begin the edge animation
                }
            }

            /* ---- advance the animation for each traveler that is moving ---- */
            for (int i = 0; i < num_travelers; i++) {
                if (tv[i].state != ANIM_MOVING) continue;

                tv[i].timer += dt;
                Vector2 from = pos[tv[i].from_node];
                Vector2 to   = pos[tv[i].to_node];

                if (tv[i].timer >= JUMP_SEC) {      // next discrete jump
                    tv[i].timer -= JUMP_SEC;
                    tv[i].jump++;
                    if (tv[i].jump >= tv[i].W) {    // finished crossing this edge
                        tv[i].entity    = to;
                        tv[i].from_node = tv[i].to_node;
                        tv[i].jump      = 0;
                        tv[i].state     = ANIM_AT_NODE; // wait for the next IPC message before moving again
                        tv[i].timer     = 0.f;
                    } else {
                        tv[i].entity = v2lerp(from, to, // discrete jump position
                            (float)tv[i].jump / (float)tv[i].W);
                    }
                } else {
                    float t = ((float)tv[i].jump + tv[i].timer / JUMP_SEC)
                              / (float)tv[i].W;     // smooth interpolation within the jump window
                    if (t > 1.f) t = 1.f;
                    tv[i].entity = v2lerp(from, to, t);
                }
            }
        }

        /* ---- drawing ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 5 - IPC pipes: each traveler computes its own path",
                 10, 10, 15, DARKGRAY);

        /* draw all edges */
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

        /* highlight edges already traversed by each traveler in their color */
        for (int t = 0; t < num_travelers; t++) {
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < tv[t].n_vis; k++)
                draw_arrow(pos[tv[t].vis_from[k]],
                           pos[tv[t].vis_to[k]], pc, 3.0f);
        }

        /* draw nodes */
        for (int i = 0; i < g->n; i++) {
            DrawCircleV(pos[i], NODE_RADIUS, SKYBLUE);
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);
            char lbl[4];
            sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2),
                     (int)(pos[i].y - 9), 18, BLACK);
        }

        /* draw each traveler entity */
        for (int t = 0; t < num_travelers; t++) {
            if (!tv[t].active && tv[t].state == ANIM_IDLE) continue; // not yet started

            Color c = traveler_colors[t % 10];
            if (!tv[t].active) c.a = 160;           // fade out travelers that have finished

            Vector2 ep = {
                tv[t].entity.x + (t % 2 == 0 ? -6.f : 6.f) * (t / 2), // stagger to reduce overlap
                tv[t].entity.y + (t % 2 == 0 ? -6.f : 6.f) * (t / 2)
            };
            Color glow = c; glow.a = 80;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R, DARKGRAY);
            DrawCircleV(ep, 4, WHITE);
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, WHITE);
        }

        /* draw button */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }

            Color bc; const char *lbl;
            if (all_done)     { bc = DARKGRAY;               lbl = "DONE";  }
            else if (playing) { bc = (Color){210,60,60,255}; lbl = "STOP";  }
            else              { bc = (Color){60,170,60,255};  lbl = "PLAY";  }

            DrawRectangleRec(btn, bc);
            DrawRectangleLinesEx(btn, 2, DARKGRAY);
            int lw = MeasureText(lbl, 20);
            DrawText(lbl,
                     (int)(btn.x + btn.width/2 - lw/2),
                     (int)(btn.y + btn.height/2 - 10),
                     20, WHITE);
        }

        /* per-traveler status strip */
        for (int t = 0; t < num_travelers; t++) {
            Color c = traveler_colors[t % 10];
            const char *status = "idle";
            if      (!tv[t].active)                  status = "arrived!";
            else if (tv[t].state == ANIM_MOVING)     status = "moving";
            else if (tv[t].state == ANIM_AT_NODE)    status = "at node";
            else if (tv[t].state == ANIM_DONE)       status = "arrived!";
            char line[64];
            sprintf(line, "T%d [%d->%d]: %s", t,
                    tv[t].src_node, tv[t].dst_node, status);
            DrawRectangle(WINDOW_W - 210, 50 + t * 22, 200, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 205, 52 + t * 22, 14, c);
        }

        /* all-done banner */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }
            if (all_done) {
                const char *banner = "  All travelers have arrived!  ";
                int mw = MeasureText(banner, 24);
                DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 24,
                              mw + 20, 48, Fade(BLACK, 0.65f));
                DrawText(banner, WINDOW_W/2 - mw/2,
                         WINDOW_H/2 - 12, 24, YELLOW);
            }
        }

        EndDrawing();
    }

    /* kill any children still running when the window is closed */
    for (int i = 0; i < num_travelers; i++)
        if (tv[i].active) kill(tv[i].pid, SIGTERM);

    CloseWindow();
}

/* =====================================================================
 * Milestone 6 – mutual exclusion: at most one traveler per node
 * MSG_WAITING → traveler shown in YELLOW outside the target node
 * MSG_AT_NODE → traveler shown in its color INSIDE the node
 * MSG_FINISHED → traveler removed from display
 * ===================================================================== */

void draw_graph_multi6(Graph* g, pid_t* pids, int* fds,
                       int* t_src, int* t_dst, int num_travelers) {

    /* Extended state for milestone 6 (adds WAITING and AT_NODE stages) */
    typedef enum {
        T6_IDLE,        /* not yet started                                   */
        T6_TRAVELING,   /* moving along an edge between nodes                */
        T6_WAITING,     /* blocked outside target node (sem_wait in progress)*/
        T6_AT_NODE,     /* inside the node (holds the mutex)                 */
        T6_DONE         /* reached destination                               */
    } T6State;

    typedef struct {
        int      active;
        T6State  state;
        int      from_node;
        int      to_node;
        int      jump;
        int      W;
        float    timer;
        Vector2  entity;
        pid_t    pid;
        int      pipe_fd;
        int      src_node;
        int      dst_node;
        int      vis_from[MAX_NODES]; /* traversed edge start nodes */
        int      vis_to[MAX_NODES];   /* traversed edge end nodes   */
        int      n_vis;
    } Traveler6;

    #define WAITING_COLOR ((Color){255, 220, 0, 255}) /* yellow = blocked/waiting */

    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    Traveler6 tv[MAX_TRAVELERS];
    for (int i = 0; i < num_travelers; i++) {       // initialise each traveler
        tv[i].active    = 1;
        tv[i].state     = T6_IDLE;
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

    int playing = 0;

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 6 (Mutex)");
    SetTargetFPS(60);

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- button click ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {
            playing = !playing;
        }

        /* ---- read IPC messages (only when playing) ---- */
        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                if (!tv[i].active) continue;
                if (tv[i].state == T6_TRAVELING) continue; // don't read messages mid-animation

                PipeMsg msg;
                ssize_t nr = read(tv[i].pipe_fd, &msg, sizeof(msg)); // non-blocking read
                if (nr != (ssize_t)sizeof(msg)) continue;

                switch (msg.type) {

                    case MSG_WAITING:               // child is blocked, wants to enter msg.current_node
                        tv[i].state     = T6_WAITING;
                        tv[i].to_node   = msg.current_node;
                        {
                            /* Position entity 70% of the way toward the blocked node */
                            Vector2 target = pos[msg.current_node];
                            tv[i].entity   = v2lerp(tv[i].entity, target, 0.7f);
                        }
                        printf("[PID=%d] WAITING outside node %d\n",
                               (int)tv[i].pid, msg.current_node);
                        fflush(stdout);
                        break;

                   case MSG_AT_NODE:                // child has entered node msg.current_node
                        printf("[DEBUG] AT_NODE %d, next=%d\n",
                            msg.current_node, msg.next_node);
                        fflush(stdout);

                        tv[i].entity = pos[msg.current_node]; // snap to node position
                        tv[i].from_node = msg.current_node;

                        if (msg.next_node >= 0) {   // there is a next node to travel to
                            if (tv[i].n_vis < MAX_NODES) { // record this edge as traversed
                                tv[i].vis_from[tv[i].n_vis] = msg.current_node;
                                tv[i].vis_to[tv[i].n_vis]   = msg.next_node;
                                tv[i].n_vis++;
                            }
                            tv[i].to_node = msg.next_node;
                            tv[i].W = g->matrix[msg.current_node][msg.next_node];
                            if (tv[i].W < 1)
                                tv[i].W = 1;
                            tv[i].jump  = 0;
                            tv[i].timer = 0.f;
                            tv[i].state = T6_TRAVELING; // start edge animation
                        } else {
                            tv[i].state  = T6_DONE; // current_node is the destination
                            tv[i].active = 0;
                        }

                        printf("[PID=%d] ENTERED node %d\n",
                            (int)tv[i].pid,
                            msg.current_node);
                        fflush(stdout);
                        break;

                    case MSG_FINISHED:              // child is completely done
                        tv[i].state  = T6_DONE;
                        tv[i].active = 0;
                        printf("[PID=%d] FINISHED\n", (int)tv[i].pid);
                        fflush(stdout);
                        break;

                    default: break;
                }
            }

            /* ---- animation update for traveling travelers ---- */
            for (int i = 0; i < num_travelers; i++) {
                if (tv[i].state != T6_TRAVELING) continue;

                tv[i].timer += dt;
                Vector2 from = pos[tv[i].from_node];
                Vector2 to   = pos[tv[i].to_node];

                if (tv[i].timer >= JUMP_SEC) {
                    tv[i].timer -= JUMP_SEC;
                    tv[i].jump++;
                    if (tv[i].jump >= tv[i].W) {    // finished crossing the edge
                        tv[i].entity    = to;
                        tv[i].from_node = tv[i].to_node;
                        tv[i].jump      = 0;
                        tv[i].state     = T6_IDLE;  // wait for next IPC message (MSG_WAITING or MSG_AT_NODE)
                        tv[i].timer     = 0.f;
                    } else {
                        tv[i].entity = v2lerp(from, to,
                            (float)tv[i].jump / (float)tv[i].W);
                    }
                } else {
                    float t = ((float)tv[i].jump + tv[i].timer / JUMP_SEC)
                              / (float)tv[i].W;
                    if (t > 1.f) t = 1.f;
                    tv[i].entity = v2lerp(from, to, t);
                }
            }
        }

        /* ---- drawing ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 6 - Mutex: at most one traveler per node | YELLOW = waiting",
                 10, 10, 15, DARKGRAY);

        /* draw all edges */
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

        /* traversed edges in traveler color */
        for (int t = 0; t < num_travelers; t++) {
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < tv[t].n_vis; k++)
                draw_arrow(pos[tv[t].vis_from[k]],
                           pos[tv[t].vis_to[k]], pc, 3.0f);
        }

        /* draw nodes — red if occupied, amber if contested by a waiting traveler */
        for (int i = 0; i < g->n; i++) {
            int occupied = 0, contested = 0;
            for (int t = 0; t < num_travelers; t++) {
                if (!tv[t].active) continue;
                if ((tv[t].state == T6_AT_NODE ||
                     tv[t].state == T6_TRAVELING) &&
                     tv[t].from_node == i) occupied = 1;  // a traveler is inside or just left this node
                if (tv[t].state == T6_WAITING &&
                    tv[t].to_node == i) contested = 1;    // a traveler is blocked waiting to enter
            }
            Color nc = SKYBLUE;
            if (occupied)        nc = (Color){255, 100, 100, 255}; // red = locked
            else if (contested)  nc = (Color){255, 200,  80, 255}; // amber = contested

            DrawCircleV(pos[i], NODE_RADIUS, nc);
            if (occupied)
                DrawCircleLines(pos[i].x, pos[i].y,
                                NODE_RADIUS + 5, RED);       // extra red ring on occupied nodes
            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);

            char lbl[4]; sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2),
                     (int)(pos[i].y - 9), 18, BLACK);
        }

        /* draw traveler entities */
        for (int t = 0; t < num_travelers; t++) {
            if (!tv[t].active && tv[t].state == T6_IDLE) continue;

            Color c = (tv[t].state == T6_WAITING)
                      ? WAITING_COLOR                        // yellow while blocked
                      : traveler_colors[t % 10];
            if (!tv[t].active) c.a = 160;                   // fade out finished travelers

            Vector2 ep = {
                tv[t].entity.x + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1),
                tv[t].entity.y + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1)
            };
            Color glow = c; glow.a = 80;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R,
                (tv[t].state == T6_WAITING) ? YELLOW : DARKGRAY); // yellow border while waiting
            DrawCircleV(ep, 4, WHITE);
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, BLACK);
        }

        /* draw button */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }

            Color bc; const char *lbl;
            if (all_done)     { bc = DARKGRAY;               lbl = "DONE"; }
            else if (playing) { bc = (Color){210,60,60,255}; lbl = "STOP"; }
            else              { bc = (Color){60,170,60,255};  lbl = "PLAY"; }

            DrawRectangleRec(btn, bc);
            DrawRectangleLinesEx(btn, 2, DARKGRAY);
            int lw = MeasureText(lbl, 20);
            DrawText(lbl,
                     (int)(btn.x + btn.width/2 - lw/2),
                     (int)(btn.y + btn.height/2 - 10),
                     20, WHITE);
        }

        /* per-traveler status strip */
        for (int t = 0; t < num_travelers; t++) {
            Color c = traveler_colors[t % 10];
            const char *status = "idle";
            if      (!tv[t].active)                   status = "arrived!";
            else if (tv[t].state == T6_TRAVELING)     status = "moving";
            else if (tv[t].state == T6_WAITING)       status = "WAITING (blocked)";
            else if (tv[t].state == T6_AT_NODE)       status = "in node";
            char line[64];
            sprintf(line, "T%d [%d->%d]: %s", t,
                    tv[t].src_node, tv[t].dst_node, status);
            DrawRectangle(WINDOW_W - 220, 50 + t * 22, 210, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 215, 52 + t * 22, 14, c);
        }

        /* legend */
        DrawCircle(20, WINDOW_H - 70, 8, (Color){255,100,100,255});
        DrawText("node locked",      34, WINDOW_H - 77, 13, DARKGRAY); // red dot = node is locked
        DrawCircle(20, WINDOW_H - 50, 8, WAITING_COLOR);
        DrawText("traveler waiting", 34, WINDOW_H - 57, 13, DARKGRAY); // yellow dot = traveler is waiting
        DrawCircle(20, WINDOW_H - 30, 8, traveler_colors[0]);
        DrawText("traveler moving",  34, WINDOW_H - 37, 13, DARKGRAY); // colored dot = traveler moving

        /* all-done banner */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }
            if (all_done) {
                const char *banner = "  All travelers have arrived!  ";
                int mw = MeasureText(banner, 24);
                DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 24,
                              mw + 20, 48, Fade(BLACK, 0.65f));
                DrawText(banner, WINDOW_W/2 - mw/2,
                         WINDOW_H/2 - 12, 24, YELLOW);
            }
        }

        EndDrawing();
    }

    /* kill any remaining active children when window closes */
    for (int i = 0; i < num_travelers; i++)
        if (tv[i].active) kill(tv[i].pid, SIGTERM);

    CloseWindow();
}

/* =====================================================================
 * Milestone 7 – parent-driven scheduling (FCFS or SJF)
 *
 * Children report MSG_WAITING / MSG_AT_NODE / MSG_LEAVING / MSG_FINISHED.
 * Parent decides who enters each node (grants a 1-byte token via pipe).
 * FCFS = first arrival serial number wins.
 * SJF  = smallest remaining path weight wins.
 * ===================================================================== */

/* Scheduling state for one node — tracks who is inside and who is queued */
typedef struct {
    int occupant;                       /* -1 = node is free, else = traveler index currently inside */
    int queue_idx[MAX_TRAVELERS];       /* indices of travelers waiting to enter                      */
    int queue_pri[MAX_TRAVELERS];       /* remaining path weight for each queued traveler (SJF key)  */
    int queue_ser[MAX_TRAVELERS];       /* arrival serial number for each queued traveler (FCFS key) */
    int qsize;                          /* number of travelers currently in the queue                */
} M7NodeSched;

/* Grant access to the highest-priority traveler waiting for node n.
   For FCFS: pick the traveler with the smallest serial number (earliest arrival).
   For SJF: pick the traveler with the smallest remaining path weight. */
static void m7_grant_next(M7NodeSched* ns, int node,
                           int* grant_wfds, int is_fcfs) {
    if (ns[node].occupant != -1 || ns[node].qsize == 0) return; // node still occupied or no one waiting

    int best = 0;                                   // assume the first queued traveler is the best candidate
    for (int q = 1; q < ns[node].qsize; q++) {     // scan the rest of the queue for a better candidate
        int key_best = is_fcfs ? ns[node].queue_ser[best] // FCFS key: earlier arrival = lower serial
                               : ns[node].queue_pri[best]; // SJF key: shorter remaining weight
        int key_q    = is_fcfs ? ns[node].queue_ser[q]
                               : ns[node].queue_pri[q];
        if (key_q < key_best) best = q;            // update best if this traveler wins
    }

    int tidx = ns[node].queue_idx[best];            // index of the winning traveler
    ns[node].occupant = tidx;                       // mark the node as occupied by this traveler

    /* Remove the winning entry by shifting the rest of the queue down */
    for (int q = best; q < ns[node].qsize - 1; q++) {
        ns[node].queue_idx[q] = ns[node].queue_idx[q + 1];
        ns[node].queue_pri[q] = ns[node].queue_pri[q + 1];
        ns[node].queue_ser[q] = ns[node].queue_ser[q + 1];
    }
    ns[node].qsize--;                               // queue is one shorter

    char go = 1;
    write(grant_wfds[tidx], &go, 1);               // send 1-byte grant to unblock the winning child
}

void draw_graph_multi7(Graph* g, pid_t* pids,
                       int* report_fds, int* grant_wfds,
                       int* t_src, int* t_dst, int num_travelers,
                       const char* sched_name) {

    /* States for the milestone-7 animation */
    typedef enum {
        T7_IDLE,        /* not yet started                          */
        T7_WAITING,     /* blocked, waiting for a node grant        */
        T7_AT_NODE,     /* inside the node (has been granted entry) */
        T7_TRAVELING,   /* moving along an edge                     */
        T7_DONE         /* reached destination                      */
    } T7State;

    typedef struct {
        int      active;
        T7State  state;
        int      from_node;
        int      to_node;
        int      jump;
        int      W;
        float    timer;
        Vector2  entity;
        pid_t    pid;
        int      src_node;
        int      dst_node;
        int      vis_from[MAX_NODES]; /* traversed edge start nodes */
        int      vis_to  [MAX_NODES]; /* traversed edge end nodes   */
        int      n_vis;
    } Traveler7;

    #define WAITING7_COLOR ((Color){255, 220, 0, 255}) /* yellow while waiting for a grant */

    int is_fcfs = (strcmp(sched_name, "FCFS") == 0); // 1 = FCFS mode, 0 = SJF mode
    int serial_counter = 0;                         // monotonically increasing arrival counter for FCFS

    Vector2 pos[MAX_NODES];
    get_node_positions(g->n, pos);

    Traveler7    tv[MAX_TRAVELERS];
    M7NodeSched  ns[MAX_NODES];                     // scheduling state for each node

    for (int i = 0; i < num_travelers; i++) {       // initialise traveler states
        tv[i].active    = 1;
        tv[i].state     = T7_IDLE;
        tv[i].from_node = t_src[i];
        tv[i].to_node   = -1;
        tv[i].jump      = 0;
        tv[i].W         = 0;
        tv[i].timer     = 0.f;
        tv[i].entity    = pos[t_src[i]];
        tv[i].pid       = pids[i];
        tv[i].src_node  = t_src[i];
        tv[i].dst_node  = t_dst[i];
        tv[i].n_vis     = 0;
    }
    for (int i = 0; i < g->n; i++) {               // initialise node scheduling state
        ns[i].occupant = -1;                        // all nodes start free
        ns[i].qsize    = 0;                         // empty queue
    }

    int playing = 0;

    char title[64];
    snprintf(title, sizeof(title),
             "Graph Simulation - Milestone 7 (%s)", sched_name); // window title shows which scheduler is active
    InitWindow(WINDOW_W, WINDOW_H, title);
    SetTargetFPS(60);

    Rectangle btn = { BTN_X, BTN_Y, BTN_W, BTN_H };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- button click ---- */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), btn)) {
            playing = !playing;
        }

        /* ---- read IPC messages and apply scheduling decisions ---- */
        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                if (!tv[i].active) continue;
                if (tv[i].state == T7_TRAVELING) continue; // skip mid-animation traveler

                PipeMsg7 msg;
                ssize_t nr = read(report_fds[i], &msg, sizeof(msg)); // non-blocking read
                if (nr != (ssize_t)sizeof(msg)) continue;

                int node = msg.current_node;        // node this message is about

                switch (msg.type) {

                    case MSG_WAITING:               // child wants to enter 'node'
                        tv[i].state   = T7_WAITING;
                        tv[i].to_node = node;
                        /* Show traveler approaching the target node */
                        tv[i].entity  = (tv[i].from_node == node)
                            ? pos[node]
                            : v2lerp(pos[tv[i].from_node], pos[node], 0.8f);

                        printf("[PID=%d] WAITING for node %d (priority=%d)\n",
                               (int)tv[i].pid, node, msg.priority);
                        fflush(stdout);

                        if (ns[node].occupant == -1) { // node is free: grant immediately
                            ns[node].occupant = i;
                            char go = 1;
                            write(grant_wfds[i], &go, 1);
                        } else {                    // node is occupied: add to wait queue
                            int q = ns[node].qsize++;
                            ns[node].queue_idx[q] = i;
                            ns[node].queue_pri[q] = msg.priority;       // SJF key
                            ns[node].queue_ser[q] = serial_counter++;   // FCFS key
                        }
                        break;

                    case MSG_AT_NODE:               // child confirmed it entered the node
                        tv[i].state     = T7_AT_NODE;
                        tv[i].from_node = node;
                        tv[i].to_node   = msg.next_node;
                        tv[i].entity    = pos[node]; // snap to node position

                        if (msg.next_node >= 0 && tv[i].n_vis < MAX_NODES) { // record traversed edge
                            tv[i].vis_from[tv[i].n_vis] = node;
                            tv[i].vis_to  [tv[i].n_vis] = msg.next_node;
                            tv[i].n_vis++;
                        }
                        printf("[PID=%d] ENTERED node %d\n",
                               (int)tv[i].pid, node);
                        fflush(stdout);
                        break;

                    case MSG_LEAVING:               // child is leaving 'node' and moving to msg.next_node
                        ns[node].occupant = -1;     // free the node
                        m7_grant_next(ns, node, grant_wfds, is_fcfs); // grant access to next waiter

                        tv[i].state     = T7_TRAVELING; // start edge animation
                        tv[i].from_node = node;
                        tv[i].to_node   = msg.next_node;
                        tv[i].W         = g->matrix[node][msg.next_node];
                        if (tv[i].W < 1) tv[i].W = 1;
                        tv[i].jump      = 0;
                        tv[i].timer     = 0.f;
                        tv[i].entity    = pos[node];

                        printf("[PID=%d] LEAVING node %d -> %d\n",
                               (int)tv[i].pid, node, msg.next_node);
                        fflush(stdout);
                        break;

                    case MSG_FINISHED:              // child has completed its journey
                        if (node >= 0 && ns[node].occupant == i) { // free the last node
                            ns[node].occupant = -1;
                            m7_grant_next(ns, node, grant_wfds, is_fcfs);
                        }
                        tv[i].state  = T7_DONE;
                        tv[i].active = 0;
                        if (node >= 0) tv[i].entity = pos[node]; // keep entity at destination
                        printf("[PID=%d] FINISHED\n", (int)tv[i].pid);
                        fflush(stdout);
                        break;

                    default: break;
                }
            }

            /* ---- animation update for travelers currently crossing an edge ---- */
            for (int i = 0; i < num_travelers; i++) {
                if (tv[i].state != T7_TRAVELING) continue;

                tv[i].timer += dt;
                Vector2 from = pos[tv[i].from_node];
                Vector2 to   = pos[tv[i].to_node];

                if (tv[i].timer >= JUMP_SEC) {
                    tv[i].timer -= JUMP_SEC;
                    tv[i].jump++;
                    if (tv[i].jump >= tv[i].W) {    // finished the edge
                        tv[i].entity    = to;
                        tv[i].from_node = tv[i].to_node;
                        tv[i].jump      = 0;
                        tv[i].state     = T7_IDLE;  // wait for next IPC message (MSG_WAITING)
                        tv[i].timer     = 0.f;
                    } else {
                        tv[i].entity = v2lerp(from, to,
                            (float)tv[i].jump / (float)tv[i].W);
                    }
                } else {
                    float t = ((float)tv[i].jump + tv[i].timer / JUMP_SEC)
                              / (float)tv[i].W;
                    if (t > 1.f) t = 1.f;
                    tv[i].entity = v2lerp(from, to, t);
                }
            }
        }

        /* ---- drawing ---- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        char header[128];
        snprintf(header, sizeof(header),
                 "Milestone 7 - Scheduler: %s | YELLOW = waiting | RED node = occupied",
                 sched_name);
        DrawText(header, 10, 10, 14, DARKGRAY);    // show which scheduler is active in the header

        /* draw all edges */
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;
                draw_arrow(pos[i], pos[j], LIGHTGRAY, 2.0f);
                char ws[8]; sprintf(ws, "%d", g->matrix[i][j]);
                DrawText(ws,
                    (int)((pos[i].x + pos[j].x) / 2),
                    (int)((pos[i].y + pos[j].y) / 2),
                    16, DARKBLUE);
            }
        }

        /* traversed edges per traveler */
        for (int t = 0; t < num_travelers; t++) {
            Color pc = traveler_colors[t % 10];
            pc.a = 160;
            for (int k = 0; k < tv[t].n_vis; k++)
                draw_arrow(pos[tv[t].vis_from[k]],
                           pos[tv[t].vis_to[k]], pc, 3.0f);
        }

        /* draw nodes — color reflects scheduling state */
        for (int i = 0; i < g->n; i++) {
            Color nc = SKYBLUE;
            if      (ns[i].occupant != -1) nc = (Color){255, 100, 100, 255}; // red = occupied
            else if (ns[i].qsize    >  0)  nc = (Color){255, 200,  80, 255}; // amber = travelers waiting

            DrawCircleV(pos[i], NODE_RADIUS, nc);
            if (ns[i].occupant != -1)
                DrawCircleLines(pos[i].x, pos[i].y,
                                NODE_RADIUS + 5, RED); // extra red ring on occupied nodes

            DrawCircleLines(pos[i].x, pos[i].y, NODE_RADIUS, DARKGRAY);

            char lbl[4]; sprintf(lbl, "%d", i);
            int tw = MeasureText(lbl, 18);
            DrawText(lbl, (int)(pos[i].x - tw/2),
                     (int)(pos[i].y - 9), 18, BLACK);

            /* show queue depth above the node */
            if (ns[i].qsize > 0) {
                char qs[8]; sprintf(qs, "q:%d", ns[i].qsize); // e.g. "q:2" means 2 travelers waiting
                DrawText(qs, (int)(pos[i].x - 12),
                         (int)(pos[i].y - NODE_RADIUS - 18), 13, RED);
            }
        }

        /* draw traveler entities */
        for (int t = 0; t < num_travelers; t++) {
            if (!tv[t].active && tv[t].state == T7_IDLE) continue;

            Color c = (tv[t].state == T7_WAITING)
                      ? WAITING7_COLOR               // yellow while waiting for a grant
                      : traveler_colors[t % 10];
            if (!tv[t].active) c.a = 160;            // fade out finished travelers

            Vector2 ep = {
                tv[t].entity.x + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1),
                tv[t].entity.y + (t % 2 == 0 ? -7.f : 7.f) * (t / 2 + 1)
            };
            Color glow = c; glow.a = 80;
            DrawCircleV(ep, ENTITY_R + 4, glow);
            DrawCircleV(ep, ENTITY_R, c);
            DrawCircleLines(ep.x, ep.y, ENTITY_R,
                (tv[t].state == T7_WAITING) ? YELLOW : DARKGRAY);
            DrawCircleV(ep, 4, WHITE);
            char tl[4]; sprintf(tl, "%d", t);
            DrawText(tl, (int)(ep.x - 4), (int)(ep.y - 8), 14, BLACK);
        }

        /* centered scheduler label */
        {
            char label[32];
            snprintf(label, sizeof(label), "Scheduler: %s", sched_name);
            int lw = MeasureText(label, 20);
            DrawRectangle(WINDOW_W/2 - lw/2 - 10, BTN_Y,
                          lw + 20, BTN_H, (Color){40, 40, 120, 220}); // dark blue background
            DrawText(label, WINDOW_W/2 - lw/2, BTN_Y + 8, 20, WHITE);
        }

        /* PLAY / STOP button */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }

            Color bc; const char *lbl;
            if (all_done)     { bc = DARKGRAY;                 lbl = "DONE"; }
            else if (playing) { bc = (Color){210, 60, 60, 255}; lbl = "STOP"; }
            else              { bc = (Color){60, 170, 60, 255};  lbl = "PLAY"; }

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
            const char *status = "idle";
            if      (!tv[t].active)                  status = "arrived!";
            else if (tv[t].state == T7_WAITING)      status = "WAITING (blocked)";
            else if (tv[t].state == T7_AT_NODE)      status = "in node";
            else if (tv[t].state == T7_TRAVELING)    status = "moving";
            char line[64];
            sprintf(line, "T%d [%d->%d]: %s", t,
                    tv[t].src_node, tv[t].dst_node, status);
            DrawRectangle(WINDOW_W - 240, 50 + t * 22, 230, 20,
                          (Color){c.r, c.g, c.b, 40});
            DrawText(line, WINDOW_W - 235, 52 + t * 22, 14, c);
        }

        /* legend */
        DrawCircle(20, WINDOW_H - 70, 8, (Color){255, 100, 100, 255});
        DrawText("node locked",      34, WINDOW_H - 77, 13, DARKGRAY);
        DrawCircle(20, WINDOW_H - 50, 8, WAITING7_COLOR);
        DrawText("traveler waiting", 34, WINDOW_H - 57, 13, DARKGRAY);
        DrawCircle(20, WINDOW_H - 30, 8, traveler_colors[0]);
        DrawText("traveler moving",  34, WINDOW_H - 37, 13, DARKGRAY);

        /* all-done banner */
        {
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (tv[i].active) { all_done = 0; break; }
            if (all_done) {
                const char *banner = "  All travelers have arrived!  ";
                int mw = MeasureText(banner, 24);
                DrawRectangle(WINDOW_W/2 - mw/2 - 10, WINDOW_H/2 - 24,
                              mw + 20, 48, Fade(BLACK, 0.65f));
                DrawText(banner, WINDOW_W/2 - mw/2,
                         WINDOW_H/2 - 12, 24, YELLOW);
            }
        }

        EndDrawing();
    }

    /* kill any children still blocked waiting for a grant when window closes */
    for (int i = 0; i < num_travelers; i++)
        if (tv[i].active) kill(tv[i].pid, SIGTERM);

    CloseWindow();
}
