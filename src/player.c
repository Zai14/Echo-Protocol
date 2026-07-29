/*
 * Player module implementation.
 *
 * Movement: constant-speed WASD, delta-time scaled, diagonal-normalized.
 * No sprint - player->speed is fixed and nothing in this file scales it.
 */

#include "player.h"
#include "raymath.h"
#include <stddef.h>
#include <string.h>

#define PLAYER_DEFAULT_SPEED  220.0f  /* pixels per second, constant */
#define PLAYER_DEFAULT_RADIUS 8.0f

static const Color PLAYER_CORE_COLOR = { 120, 200, 255, 255 };
static const Color PLAYER_GLOW_COLOR = { 60, 160, 255, 255 };

void PlayerInit(Player *player)
{
    if (player == NULL) {
        return;
    }

    memset(player, 0, sizeof(*player));
    player->position = (Vector2){ 0.0f, 0.0f };
    player->velocity = (Vector2){ 0.0f, 0.0f };
    player->speed    = PLAYER_DEFAULT_SPEED;
    player->radius   = PLAYER_DEFAULT_RADIUS;
}

void PlayerHandleInput(Player *player)
{
    if (player == NULL) {
        return;
    }

    Vector2 direction = { 0.0f, 0.0f };

    if (IsKeyDown(KEY_W)) direction.y -= 1.0f;
    if (IsKeyDown(KEY_S)) direction.y += 1.0f;
    if (IsKeyDown(KEY_A)) direction.x -= 1.0f;
    if (IsKeyDown(KEY_D)) direction.x += 1.0f;

    /* Normalize so diagonal movement isn't faster than axis movement,
     * keeping speed constant in every direction. */
    if (direction.x != 0.0f || direction.y != 0.0f) {
        direction = Vector2Normalize(direction);
    }

    /* Constant movement speed - no sprint modifier is ever applied. */
    player->velocity = Vector2Scale(direction, player->speed);
}

/*
 * Collision-ready seam: computes where the player *wants* to go this
 * frame. Right now the desired position is always committed as-is
 * (no obstacles exist yet). Once map/enemy collision queries exist,
 * this is where the desired position gets clamped or rejected using
 * PlayerGetCollider() against those systems, before writing back to
 * player->position.
 */
static Vector2 ResolveMovement(const Player *player, Vector2 desiredPosition)
{
    (void)player;

    /* TODO(collision): check desiredPosition (as a CircleShape via
     * PlayerGetCollider) against map geometry and enemies here, and
     * adjust it before it's committed in PlayerUpdate(). */

    return desiredPosition;
}

void PlayerUpdate(Player *player, float deltaTime)
{
    if (player == NULL) {
        return;
    }

    Vector2 desiredPosition = Vector2Add(
        player->position,
        Vector2Scale(player->velocity, deltaTime)
    );

    player->position = ResolveMovement(player, desiredPosition);
}

void PlayerDraw(const Player *player)
{
    if (player == NULL) {
        return;
    }

    /* Glow: a handful of soft, translucent rings behind the solid
     * core, no texture assets involved. */
    const int   glowLayers    = 5;
    const float glowMaxRadius = player->radius * 3.0f;

    for (int i = glowLayers; i >= 1; i--) {
        float t          = (float)i / (float)glowLayers;       /* 1..0 */
        float layerRadius = player->radius + (glowMaxRadius - player->radius) * t;
        unsigned char alpha = (unsigned char)(70.0f * (1.0f - t) + 10.0f);

        DrawCircleV(player->position, layerRadius, Fade(PLAYER_GLOW_COLOR, alpha / 255.0f));
    }

    /* Solid core circle — subtly breathing with a slow, time-based pulse. */
    float breathe = sinf((float)GetTime() * 2.0f) * 0.08f + 0.92f;
    Color corePulse = {
        (unsigned char)(PLAYER_CORE_COLOR.r * breathe),
        (unsigned char)(PLAYER_CORE_COLOR.g * breathe),
        (unsigned char)(PLAYER_CORE_COLOR.b * breathe),
        255
    };
    DrawCircleV(player->position, player->radius, corePulse);
    DrawCircleLines((int)player->position.x, (int)player->position.y, player->radius, Fade(WHITE, 0.6f));
}

void PlayerShutdown(Player *player)
{
    (void)player;
}

CircleShape PlayerGetCollider(const Player *player)
{
    if (player == NULL) {
        return (CircleShape){ 0.0f, 0.0f, 0.0f };
    }

    return (CircleShape){
        .x      = player->position.x,
        .y      = player->position.y,
        .radius = player->radius
    };
}
