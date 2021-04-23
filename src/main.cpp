
#include <raylib.h>

int main(int argc, char **argv) {
    
    InitWindow(800, 450, "ld48");

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLUE);
        DrawText("Ludum Dare 48 PREP!", 100, 100, 20, WHITE);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

