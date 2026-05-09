#include "gui.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

void draw_graph(Graph* g) {
    InitWindow(800, 600, "Graph");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph GUI Running", 200, 200, 20, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
}