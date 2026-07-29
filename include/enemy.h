#ifndef ECHO_ENEMY_H
#define ECHO_ENEMY_H

#include <stdbool.h>
#include "collision.h"
#include "soundprop.h"

/*
 * Enemy module — two enemy types with improved behavioural AI.
 *
 * WATCHER:
 *   Patrols waypoints with natural pauses at each stop. Investigates
 *   nearby sound events (footsteps, doors, impacts) by moving to the
 *   origin, searching briefly, then returning to patrol.
 *   Slow (80 px/s), medium hearing range (200 px).
 *
 * HUNTER:
 *   Idles in the dark, dim and still. When it hears a SONAR_PULSE it
 *   enters an ALERT state — pauses for 0.6s, colour brightens as it
 *   "charges up", then aggressively RUSHES toward the pulse origin at
 *   high speed (150 px/s). Returns to idle after the rush subsides.
 *   Fast, long hearing range (450 px).
 *
 * GAME OVER on collision with the player (checked in game.c).
 */

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define ECHO_MAX_ENEMIES               12
#define ECHO_WATCHER_MAX_PATROL_POINTS  4

/* Watcher */
#define ENEMY_WATCHER_RADIUS           14.0f
#define ENEMY_WATCHER_SPEED            80.0f
#define ENEMY_WATCHER_HEARING_RADIUS  200.0f
#define ENEMY_WATCHER_PAUSE_MIN        0.6f   /* min seconds at a waypoint */
#define ENEMY_WATCHER_PAUSE_MAX        1.4f   /* max seconds at a waypoint */
#define ENEMY_WATCHER_SEARCH_TIME      2.0f   /* seconds spent searching */

/* Hunter */
#define ENEMY_HUNTER_RADIUS           12.0f
#define ENEMY_HUNTER_SPEED           150.0f
#define ENEMY_HUNTER_HEARING_RADIUS  450.0f
#define ENEMY_HUNTER_ALERT_TIME       0.6f   /* pause before rushing */

/* ------------------------------------------------------------------ */
/*  Types & state machine                                              */
/* ------------------------------------------------------------------ */

typedef enum EnemyType {
    ENEMY_TYPE_NONE = 0,
    ENEMY_TYPE_WATCHER,
    ENEMY_TYPE_HUNTER
} EnemyType;

typedef enum EnemyAIState {
    /* Shared */
    ENEMY_STATE_IDLE = 0,           /* Hunter — standing still */

    /* Watcher */
    ENEMY_STATE_PATROL,             /* moving between waypoints */
    ENEMY_STATE_PAUSE_WAYPOINT,     /* paused at a waypoint, looking */
    ENEMY_STATE_INVESTIGATE,        /* moving toward a sound */
    ENEMY_STATE_SEARCH,             /* arrived at sound, searching area */

    /* Hunter */
    ENEMY_STATE_ALERT,              /* heard a pulse — charging up */
    ENEMY_STATE_RUSH,               /* sprinting toward the player */
    ENEMY_STATE_HESITATE,           /* #8: paused at ping location, looking around */
    ENEMY_STATE_RETREAT             /* moving to a random room after losing the player */
} EnemyAIState;

/* ------------------------------------------------------------------ */
/*  Core data types                                                    */
/* ------------------------------------------------------------------ */

typedef struct Enemy {
    EnemyType     type;
    EnemyAIState  state;
    float         x, y;
    float         targetX, targetY;       /* current movement target */
    float         speed;
    float         radius;
    float         stateTimer;             /* general-purpose timer (s) */
    float         facingAngle;            /* radians, for draw direction */
    bool          alive;

    /* Patrol route (WATCHER only; patrolCount == 0 means stationary) */
    float         patrolX[ECHO_WATCHER_MAX_PATROL_POINTS];
    float         patrolY[ECHO_WATCHER_MAX_PATROL_POINTS];
    int           patrolCount;
    int           currentPatrolIdx;

    /* Pulse lifetime remaining when Hunter enters ALERT (captured so
     * it's available when ALERT timer expires and the pulse sound
     * may have already been removed from the buffer). */
    float         alertPulseLife;

    /* RETREAT target — random room centre the Hunter heads to when
     * it loses the player, instead of idling in place. */
    float         retreatX;
    float         retreatY;
} Enemy;

typedef struct EnemyManager {
    Enemy  enemies[ECHO_MAX_ENEMIES];
    int    count;
} EnemyManager;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void EnemyManagerInit(EnemyManager *manager);

/* Add one enemy at a specific world position.
 * Watcher gains a small patrol route around its spawn;
 * Hunter starts idle. */
void EnemyManagerAdd(EnemyManager *manager, EnemyType type, float x, float y);

/* Step AI for every alive enemy.  soundProp is queried for
 * nearby sound events; station is used by Hunters to pick a
 * random retreat room when they lose the player.
 * relayActivated: if true, Watchers guard the airlock and Hunters
 * gain +20% hearing radius (difficulty scaling).
 * hunterAlertMultiplier: 0.0-1.0, shortens Hunter ALERT charge-up
 * when the player's ping pattern is predictable (default 1.0).
 * predictedPingX/Y: if non-zero, the Hunter moves toward this
 * predicted position instead of the exact pulse origin (pattern learning). */
void EnemyManagerUpdate(EnemyManager *manager, float deltaTime,
                        const SoundPropagation *soundProp,
                        const void *station,
                        bool relayActivated,
                        float hunterAlertMultiplier,
                        float predictedPingX, float predictedPingY);

/* Draw every alive enemy. */
void EnemyManagerDraw(const EnemyManager *manager);

/* Get the collision shape for a specific enemy (for game-over
 * detection in game.c). */
CircleShape EnemyGetCollider(const Enemy *enemy);

void EnemyManagerShutdown(EnemyManager *manager);

#endif /* ECHO_ENEMY_H */
