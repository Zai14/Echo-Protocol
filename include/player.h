#ifndef ECHO_PLAYER_H
#define ECHO_PLAYER_H

#include "raylib.h"
#include "collision.h"

/*
 * Player module.
 *
 * Owns the player's position/velocity and drives WASD movement at a
 * constant speed (no sprint), scaled by delta time for smooth,
 * frame-rate-independent motion.
 *
 * Collision-ready architecture:
 *   - PlayerGetCollider() exposes the player's current collision shape
 *     so other systems (map, enemies) can query it without reaching
 *     into Player internals.
 *   - Movement is split into "compute desired move" and "commit move"
 *     steps (see PlayerUpdate / ResolveMovement in player.c). Right
 *     now nothing blocks the desired move, but the seam is already in
 *     place: once MapCollidesCircle()/EnemyCollidesCircle()-style
 *     queries exist, they slot into ResolveMovement() without
 *     changing this header's public interface.
 */

typedef struct Player {
    Vector2 position;
    Vector2 velocity;
    float   speed;
    float   radius;
} Player;

void PlayerInit(Player *player);

/* Reads WASD input and updates player->velocity. Pure input -> intent,
 * does not touch position. */
void PlayerHandleInput(Player *player);

/* Advances player->position from player->velocity, scaled by
 * deltaTime. This is where future collision resolution will clamp/
 * redirect the move before it's committed. */
void PlayerUpdate(Player *player, float deltaTime);

void PlayerDraw(const Player *player);

void PlayerShutdown(Player *player);

/* Current collision shape for this player, for use by collision
 * queries elsewhere (map walls, enemies, sonar pulses, ...). */
CircleShape PlayerGetCollider(const Player *player);

#endif /* ECHO_PLAYER_H */
