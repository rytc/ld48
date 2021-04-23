
#include <raylib.h>

int main(int argc, char **argv) {
    
    InitWindow(1280, 720, "ld48");
   
    Camera2D cam = {};
    cam.target = {0, 0};
    cam.rotation = 0.f;
    cam.zoom = 1.f;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(DARKPURPLE);
        BeginMode2D(cam);
        DrawText("Ludum Dare 48 PREP!", 100, 100, 16, WHITE);

        DrawRectangle(30, 300-32, 32,32, DARKGRAY);
        DrawRectangle(10, 300, 700, 100, BLACK);
        EndMode2D();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

