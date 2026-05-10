#include "gui.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NODE_RADIUS 25
#define WINDOW_W    800
#define WINDOW_H    600
#define PI          3.14159265358979323846f

// Arrange nodes evenly around a circle in the center of the window
static void get_node_positions(int n, Vector2* positions) {
    float cx = WINDOW_W / 2.0f;
    float cy = WINDOW_H / 2.0f;
    float radius = 200.0f;

    for (int i = 0; i < n; i++) {
        // Start from the top (-PI/2) and space evenly
        float angle = (2 * PI * i) / n - PI / 2;
        positions[i].x = cx + radius * cosf(angle);
        positions[i].y = cy + radius * sinf(angle);
    }
}

// Draw a directed arrow from one node to another, stopping at the node edges
static void draw_arrow(Vector2 start, Vector2 end, Color color) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);
    float ux = dx / len; // unit vector x
    float uy = dy / len; // unit vector y

    // Pull back both ends so the line sits between circle edges, not centers
    Vector2 line_start = { start.x + ux * NODE_RADIUS, start.y + uy * NODE_RADIUS };
    Vector2 line_end   = { end.x   - ux * NODE_RADIUS, end.y   - uy * NODE_RADIUS };

    DrawLineEx(line_start, line_end, 2.0f, color);

    // Draw the arrowhead as a small triangle at the end
    float arrow_size = 10.0f;
    float angle = atan2f(dy, dx);
    Vector2 tip   = line_end;
    Vector2 left  = { tip.x - arrow_size * cosf(angle - 0.4f),
                      tip.y - arrow_size * sinf(angle - 0.4f) };
    Vector2 right = { tip.x - arrow_size * cosf(angle + 0.4f),
                      tip.y - arrow_size * sinf(angle + 0.4f) };
    DrawTriangle(tip, left, right, color);
}

// Check if edge (i -> j) is part of the shortest path
static int is_path_edge(DijkstraResult* res, int i, int j) {
    for (int k = 0; k < res->path_len - 1; k++)
        if (res->path[k] == i && res->path[k + 1] == j)
            return 1;
    return 0;
}

void draw_graph(Graph* g, DijkstraResult* res) {
    Vector2 positions[MAX_NODES];
    get_node_positions(g->n, positions);

    InitWindow(WINDOW_W, WINDOW_H, "Graph Simulation - Milestone 2");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Green = source | Orange = destination | Red = shortest path", 10, 10, 15, DARKGRAY);

        // Draw all edges first (so nodes appear on top)
        for (int i = 0; i < g->n; i++) {
            for (int j = 0; j < g->n; j++) {
                if (g->matrix[i][j] == -1) continue;

                // Highlight edges that are part of the shortest path
                Color edge_color = LIGHTGRAY;
                if (res->found && is_path_edge(res, i, j))
                    edge_color = RED;

                draw_arrow(positions[i], positions[j], edge_color);

                // Draw the weight label at the midpoint of the edge
                float mx = (positions[i].x + positions[j].x) / 2;
                float my = (positions[i].y + positions[j].y) / 2;
                char weight_str[8];
                sprintf(weight_str, "%d", g->matrix[i][j]);
                DrawText(weight_str, (int)mx, (int)my, 16, DARKBLUE);
            }
        }

        // Draw nodes as circles with their ID number
        for (int i = 0; i < g->n; i++) {
            // Color source green, destination orange, others sky blue
            Color node_color = SKYBLUE;
            if (res->found && res->path_len > 0) {
                if (i == res->path[0])              node_color = GREEN;
                if (i == res->path[res->path_len-1]) node_color = ORANGE;
            }

            DrawCircleV(positions[i], NODE_RADIUS, node_color);
            DrawCircleLines(positions[i].x, positions[i].y, NODE_RADIUS, DARKGRAY);

            // Center the node ID label inside the circle
            char label[4];
            sprintf(label, "%d", i);
            int text_w = MeasureText(label, 18);
            DrawText(label, (int)(positions[i].x - text_w / 2),
                           (int)(positions[i].y - 9), 18, BLACK);
        }

        // Show path info at the bottom of the screen
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

        EndDrawing();
    }

    CloseWindow();
}