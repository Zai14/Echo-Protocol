#ifndef ECHO_MAP_H
#define ECHO_MAP_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Procedural Station Generator.
 *
 * Generates a randomised facility layout of 15–20 rectangular rooms
 * connected by L-shaped corridors.  The layout is deterministic from
 * a uint64_t seed so the same seed always produces the same station.
 *
 * The generator also picks random rooms for enemy spawns and marks
 * one room as the objective (the room furthest from the start).
 */

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define MAX_ROOMS             20
#define MAX_CORRIDORS         38      /* 2 × (MAX_ROOMS - 1) = 38 */
#define MIN_ROOM_SIZE         80.0f
#define MAX_ROOM_SIZE        180.0f
#define ROOM_PADDING          30.0f   /* minimum gap between rooms */
#define CORRIDOR_WIDTH        48.0f
#define CORRIDOR_HALF         24.0f
#define ROOM_PLACE_ATTEMPTS   50

/* ------------------------------------------------------------------ */
/*  Core types                                                        */
/* ------------------------------------------------------------------ */

typedef struct Room {
    float x, y;   /* top-left */
    float w, h;   /* width, height */
} Room;

typedef struct Corridor {
    float x, y;   /* top-left */
    float w, h;   /* width, height — a rectangle the player can walk in */
} Corridor;

#define STATION_MAX_ENEMY_SPAWNS  3

typedef struct Station {
    Room      rooms[MAX_ROOMS];
    int       roomCount;

    Corridor  corridors[MAX_CORRIDORS];
    int       corridorCount;

    uint64_t  seed;               /* seed used to generate this layout */

    int       startRoomIdx;       /* room where the player begins */
    int       objectiveRoomIdx;   /* room furthest from the start */

    /* Room indices for the objective chain. */
    int       relayRoomIdxs[3];   /* rooms containing communication relays */
    int       airlockRoomIdx;     /* escape airlock room */

    /* Sonar interference zones — rooms where electrical equipment
     * distorts sonar: reveal radius reduced, static increased. */
    int       interferenceRoomIdxs[2];
    int       interferenceRoomCount;

    /* Enemy spawn positions (random rooms ≠ startRoom). */
    float     enemySpawnX[STATION_MAX_ENEMY_SPAWNS];
    float     enemySpawnY[STATION_MAX_ENEMY_SPAWNS];
    int       enemySpawnCount;
} Station;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/* Generate a complete station layout from seed.
 * seed == 0 is replaced with a non-zero default so the RNG works. */
void StationGenerate(Station *station, uint64_t seed);

/* Draw the station (room floors, corridor floors, wall outlines,
 * and the objective marker) in the currently active render target.
 * Call inside BeginMode2D(). */
void StationDraw(const Station *station);

/* Convenience: get the world-space centre of a room. */
void StationRoomCentre(const Station *station, int idx, float *ox, float *oy);

/* Check if a circle at (cx, cy) with the given radius overlaps any
 * walkable area (room floor or corridor floor) in the station.
 * Returns true if the circle is touching at least one walkable rect.
 * Used by game.c for player wall collision. */
bool StationIsWalkable(const Station *station, float cx, float cy, float radius);

#endif /* ECHO_MAP_H */
