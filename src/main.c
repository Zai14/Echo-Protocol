/*
 * Echo Protocol - entry point.
 *
 * Boots the raylib window, hands control to the Game module's
 * update/draw loop, and performs a clean shutdown on exit.
 */

#include "raylib.h"
#include "game.h"

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(ECHO_WINDOW_WIDTH, ECHO_WINDOW_HEIGHT, ECHO_WINDOW_TITLE);
    SetTargetFPS(ECHO_TARGET_FPS);
    /* Borderless fullscreen at native resolution; logical resolution
     * stays 1280×720 via the render texture, scaled cleanly by raylib. */
    ToggleFullscreen();

    Game game;
    GameInit(&game);

    while (!WindowShouldClose() && !GameShouldClose(&game)) {
        game.deltaTime   = GetFrameTime();
        game.elapsedTime += game.deltaTime;

        GameUpdate(&game);

        BeginDrawing();
        GameDraw(&game);
        EndDrawing();
    }

    GameShutdown(&game);
    CloseWindow();

    return 0;
}
