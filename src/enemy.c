/*
 * Enemy module — improved AI implementation.
 *
 * WATCHER:
 *   PATROL → (arrive) → PAUSE_WAYPOINT → (timer) → PATROL
 *   PATROL → (hear sound) → INVESTIGATE → (arrive) → SEARCH → (timer) → PATROL
 *
 * HUNTER:
 *   IDLE → (hear sonar) → ALERT [0.6s, colour brightens] → RUSH → (arrive/timeout) → IDLE
 *
 * See include/enemy.h for the full state machine overview.
 */

/* _USE_MATH_DEFINES ensures M_PI is available on MSVC. */
#define _USE_MATH_DEFINES
#include "enemy.h"
#include "map.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static float DistSq(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

static void MoveToward(Enemy *e, float tx, float ty, float dt)
{
    float dx = tx - e->x;
    float dy = ty - e->y;
    float d  = sqrtf(dx * dx + dy * dy);

    if (d < 1.0f)
    {
        e->x = tx;
        e->y = ty;
        return;
    }

    /* Update facing direction. */
    e->facingAngle = atan2f(dy, dx);

    float step = e->speed * dt;
    if (step >= d) step = d;

    e->x += (dx / d) * step;
    e->y += (dy / d) * step;
}

/* Utility: pick a uniform random float in [min, max]. */
static float RandRange(float min, float max)
{
    return min + (float)GetRandomValue(0, 10000) / 10000.0f * (max - min);
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */

void EnemyManagerInit(EnemyManager *manager)
{
    if (manager == NULL) return;
    memset(manager, 0, sizeof(*manager));
}

/* ------------------------------------------------------------------ */
/*  Add one enemy at a world position                                 */
/* ------------------------------------------------------------------ */

void EnemyManagerAdd(EnemyManager *manager, EnemyType type, float x, float y)
{
    if (manager == NULL) return;
    if (manager->count >= ECHO_MAX_ENEMIES) return;

    Enemy *e = &manager->enemies[manager->count++];
    memset(e, 0, sizeof(*e));
    e->type  = type;
    e->x     = x;
    e->y     = y;
    e->alive = true;

    /* Facing initially toward positive X (right). */
    e->facingAngle = 0.0f;

    if (type == ENEMY_TYPE_WATCHER)
    {
        e->state            = ENEMY_STATE_PATROL;
        e->speed            = ENEMY_WATCHER_SPEED;
        e->radius           = ENEMY_WATCHER_RADIUS;
        /* 150px square patrol route — Watchers roam widely now. */
        e->patrolCount      = 4;
        e->currentPatrolIdx = 0;
        e->patrolX[0] = x - 150.0f;  e->patrolY[0] = y - 150.0f;
        e->patrolX[1] = x + 150.0f;  e->patrolY[1] = y - 150.0f;
        e->patrolX[2] = x + 150.0f;  e->patrolY[2] = y + 150.0f;
        e->patrolX[3] = x - 150.0f;  e->patrolY[3] = y + 150.0f;
    }
    else if (type == ENEMY_TYPE_HUNTER)
    {
        /* Hunter starts still and dim. */
        e->state       = ENEMY_STATE_IDLE;
        e->speed       = ENEMY_HUNTER_SPEED;
        e->radius      = ENEMY_HUNTER_RADIUS;
        e->patrolCount = 0;
    }
    else if (type == ENEMY_TYPE_PHANTOM)
    {
        /* Phantom starts wandering toward a random room centre. */
        e->state       = ENEMY_STATE_PHANTOM_WANDER;
        e->speed       = ENEMY_PHANTOM_SPEED;
        e->radius      = ENEMY_PHANTOM_RADIUS;
        e->patrolCount = 0;
        e->phantomTargetRoom = -1;
        e->phantomPrevRoom   = -1;
    }
}

/* ------------------------------------------------------------------ */
/*  Update AI                                                         */
/* ------------------------------------------------------------------ */

void EnemyManagerUpdate(EnemyManager *manager, float deltaTime,
                        const SoundPropagation *sp,
                        const void *station,
                        bool relayActivated,
                        float hunterAlertMultiplier,
                        float predictedPingX, float predictedPingY)
{
    if (manager == NULL || sp == NULL) return;
    const Station *st = (const Station *)station;

    /* Difficulty scaling: after relay activation, Hunter hearing is +20%. */
    float hunterHearing = ENEMY_HUNTER_HEARING_RADIUS
                        * (relayActivated ? 1.20f : 1.0f);
    /* Pattern learning: shorter ALERT when player pings predictably. */
    float alertTime = ENEMY_HUNTER_ALERT_TIME * hunterAlertMultiplier;

    for (int i = 0; i < manager->count; i++)
    {
        Enemy *e = &manager->enemies[i];
        if (!e->alive) continue;

        /* ---- Timer ticks for every state. ---- */
        e->stateTimer -= deltaTime;
        if (e->stateTimer < 0.0f) e->stateTimer = 0.0f;

        switch (e->type)
        {
            /* ==============================================================
             *  WATCHER  — patrol with pauses, investigate sounds, search
             * ============================================================ */
            case ENEMY_TYPE_WATCHER:
            {
                /* --- Query nearby sound events. --- */
                SoundEvent heard[4];
                int heardCount = SoundPropQuery(sp, e->x, e->y,
                                                ENEMY_WATCHER_HEARING_RADIUS,
                                                heard, 4);

                bool heardInteresting = false;
                float soundTargetX = 0.0f, soundTargetY = 0.0f;

                for (int s = 0; s < heardCount; s++)
                {
                    /* Watchers ignore sonar pulses — too loud, too fast.
                     * They also ignore phantom whispers — that's between you and the station. */
                    if (heard[s].type != SOUND_EVENT_SONAR_PULSE &&
                        heard[s].type != SOUND_EVENT_PHANTOM_WHISPER)
                    {
                        heardInteresting = true;
                        soundTargetX = heard[s].x;
                        soundTargetY = heard[s].y;
                        break;
                    }
                }

                switch (e->state)
                {
                    case ENEMY_STATE_PATROL:
                    {
                        /* Interrupt patrol to investigate new sounds. */
                        if (heardInteresting)
                        {
                            e->state       = ENEMY_STATE_INVESTIGATE;
                            e->targetX     = soundTargetX;
                            e->targetY     = soundTargetY;
                            e->stateTimer  = 0.0f;   /* not used in INVESTIGATE */
                            break;
                        }

                        /* No route — stationary. */
                        if (e->patrolCount == 0) break;

                        /* Move toward current waypoint. */
                        float px = e->patrolX[e->currentPatrolIdx];
                        float py = e->patrolY[e->currentPatrolIdx];
                        MoveToward(e, px, py, deltaTime);

                        /* Arrived at waypoint — pause and look around. */
                        if (DistSq(e->x, e->y, px, py) < 16.0f)
                        {
                            e->state       = ENEMY_STATE_PAUSE_WAYPOINT;
                            e->stateTimer  = RandRange(ENEMY_WATCHER_PAUSE_MIN,
                                                       ENEMY_WATCHER_PAUSE_MAX);
                            /* Face outward from the patrol route (alternating). */
                            float angleOffset = (e->currentPatrolIdx % 2 == 0)
                                              ? 0.0f : (float)M_PI;
                            e->facingAngle = angleOffset;
                        }
                        break;
                    }

                    case ENEMY_STATE_PAUSE_WAYPOINT:
                    {
                        /* If something interesting happens while paused,
                         * go investigate. */
                        if (heardInteresting)
                        {
                            e->state       = ENEMY_STATE_INVESTIGATE;
                            e->targetX     = soundTargetX;
                            e->targetY     = soundTargetY;
                            e->stateTimer  = 0.0f;
                            break;
                        }

                        /* Pause expired — 25% chance to guard a corridor (doorway)
                         * instead of continuing patrol.  This makes Watchers
                         * occasionally block paths. */
                        if (e->stateTimer <= 0.0f)
                        {
                            if (st != NULL && st->corridorCount > 0 && (GetRandomValue(0, 99) < 25))
                            {
                                /* Move to a random corridor centre to "guard" it. */
                                int ci = GetRandomValue(0, st->corridorCount - 1);
                                const Corridor *cor = &st->corridors[ci];
                                e->state   = ENEMY_STATE_INVESTIGATE;
                                e->targetX = cor->x + cor->w * 0.5f;
                                e->targetY = cor->y + cor->h * 0.5f;
                            }
                            else
                            {
                                e->state = ENEMY_STATE_PATROL;
                                e->currentPatrolIdx = (e->currentPatrolIdx + 1)
                                                      % e->patrolCount;
                            }
                        }
                        break;
                    }

                    case ENEMY_STATE_INVESTIGATE:
                    {
                        /* If a newer, more interesting sound appears,
                         * re-target. */
                        if (heardInteresting)
                        {
                            e->targetX = soundTargetX;
                            e->targetY = soundTargetY;
                        }

                        MoveToward(e, e->targetX, e->targetY, deltaTime);

                        /* Arrived — enter search phase. */
                        if (DistSq(e->x, e->y, e->targetX, e->targetY) < 100.0f)
                        {
                            e->state      = ENEMY_STATE_SEARCH;
                            e->stateTimer = ENEMY_WATCHER_SEARCH_TIME;
                            /* Face the spot we just investigated. */
                            e->facingAngle = atan2f(e->targetY - e->y,
                                                    e->targetX - e->x);
                        }
                        break;
                    }

                    case ENEMY_STATE_SEARCH:
                    {
                        /* Stand still, searching. Slowly rotate
                         * as if scanning the area. */
                        e->facingAngle += deltaTime * 1.2f;

                        if (e->stateTimer <= 0.0f)
                        {
                            /* 30% chance to roam to a random room instead of
                             * returning to patrol — Watchers feel alive and
                             * occasionally cross into neighbouring rooms. */
                            if (st != NULL && st->roomCount > 1 && (GetRandomValue(0, 99) < 30))
                            {
                                int ri;
                                /* After relay activation, Watchers move toward the airlock area. */
                                if (relayActivated && st->airlockRoomIdx > 0) {
                                    ri = st->airlockRoomIdx;
                                } else {
                                    /* Otherwise, sometimes investigate the relay room. */
                                    int relayRi = st->relayRoomIdxs[0];
                                    if (relayRi > 0 && (GetRandomValue(0, 99) < 40)) {
                                        ri = relayRi;
                                    } else {
                                        ri = GetRandomValue(1, st->roomCount - 1);
                                    }
                                }
                                e->state       = ENEMY_STATE_INVESTIGATE;
                                e->targetX     = st->rooms[ri].x + st->rooms[ri].w * 0.5f;
                                e->targetY     = st->rooms[ri].y + st->rooms[ri].h * 0.5f;
                            }
                            else
                            {
                                /* Done searching — return to patrol. */
                                e->state = ENEMY_STATE_PATROL;
                                /* Face toward next waypoint. */
                                if (e->patrolCount > 0)
                                {
                                    float px = e->patrolX[e->currentPatrolIdx];
                                    float py = e->patrolY[e->currentPatrolIdx];
                                    e->facingAngle = atan2f(py - e->y, px - e->x);
                                }
                            }
                        }
                        break;
                    }

                    default: break;
                }
                break;
            }

            /* ==============================================================
             *  PHANTOM — slow wander between rooms, corrupting echo memory
             * ============================================================ */
            case ENEMY_TYPE_PHANTOM:
            {
                switch (e->state)
                {
                    case ENEMY_STATE_PHANTOM_WANDER:
                    {
                        /* Move toward the current room target. */
                        MoveToward(e, e->targetX, e->targetY, deltaTime);

                        /* Arrived — pause and drift. */
                        if (DistSq(e->x, e->y, e->targetX, e->targetY) < 400.0f)
                        {
                            e->state      = ENEMY_STATE_PHANTOM_PAUSE;
                            e->stateTimer = RandRange(ENEMY_PHANTOM_PAUSE_MIN,
                                                      ENEMY_PHANTOM_PAUSE_MAX);
                        }
                        break;
                    }

                    case ENEMY_STATE_PHANTOM_PAUSE:
                    {
                        /* Subtle bobbing while paused — the phantom seems to drift. */
                        e->facingAngle += deltaTime * 0.3f;

                        /* Pause expired — pick a new room to wander to. */
                        if (e->stateTimer <= 0.0f)
                        {
                            e->phantomPrevRoom = e->phantomTargetRoom;
                            /* Pick a random room different from the previous one. */
                            if (st != NULL && st->roomCount > 2)
                            {
                                int ri;
                                do {
                                    ri = GetRandomValue(0, st->roomCount - 1);
                                } while (ri == e->phantomPrevRoom && st->roomCount > 2);

                                e->phantomTargetRoom  = ri;
                                e->targetX = st->rooms[ri].x + st->rooms[ri].w * 0.5f;
                                e->targetY = st->rooms[ri].y + st->rooms[ri].h * 0.5f;
                                e->state   = ENEMY_STATE_PHANTOM_WANDER;
                            }
                        }
                        break;
                    }

                    default: break;
                }
                break;
            }

            /* ==============================================================
             *  HUNTER  — idle → alert (anticipation) → rush
             * ============================================================ */
            case ENEMY_TYPE_HUNTER:
            {
                /* --- Query for SONAR_PULSE events only. --- */
                SoundEvent heard[4];
                int heardCount = SoundPropQuery(sp, e->x, e->y,
                                                hunterHearing,
                                                heard, 4);

                bool heardPulse = false;
                float pulseX = 0.0f, pulseY = 0.0f, pulseLife = 0.0f;

                for (int s = 0; s < heardCount; s++)
                {
                    if (heard[s].type == SOUND_EVENT_SONAR_PULSE)
                    {
                        heardPulse = true;
                        pulseX     = heard[s].x;
                        pulseY     = heard[s].y;
                        pulseLife  = heard[s].lifetime - heard[s].elapsed;
                        break;
                    }
                }

                switch (e->state)
                {
                    case ENEMY_STATE_IDLE:
                    {
                        /* Heard a pulse — snap to attention.
                         * alertTime is shorter when the player pings rhythmically. */
                        if (heardPulse)
                        {
                            e->state      = ENEMY_STATE_ALERT;
                            e->stateTimer = alertTime;
                            /* Turn to face the pulse origin. */
                            e->facingAngle = atan2f(pulseY - e->y,
                                                    pulseX - e->x);
                            /* Store pulse info for the rush phase. */
                            e->targetX    = pulseX;
                            e->targetY    = pulseY;
                            e->alertPulseLife = pulseLife;
                        }
                        break;
                    }

                    case ENEMY_STATE_ALERT:
                    {
                        /* Standing still, charging up. */
                        if (e->stateTimer <= 0.0f)
                        {
                            /* Charge complete — RUSH! */
                            e->state      = ENEMY_STATE_RUSH;
                            /* Rush until the pulse would have expired,
                             * plus time to reach the spot. */
                            e->stateTimer = e->alertPulseLife + 1.0f;
                        }
                        break;
                    }

                    case ENEMY_STATE_RUSH:
                    {
                        /* Can be re-triggered by a fresh pulse mid-rush. */
                        if (heardPulse)
                        {
                            e->targetX    = pulseX;
                            e->targetY    = pulseY;
                            e->stateTimer = pulseLife + 1.0f;
                            e->alertPulseLife = pulseLife;
                        }

                        /* #2: Hunter Pattern Learning — if a predicted position
                         * is set, the Hunter moves toward the predicted next ping
                         * location instead of the current ping origin.
                         * This makes rhythmic pingers vulnerable to prediction. */
                        float rushTargetX = e->targetX;
                        float rushTargetY = e->targetY;
                        /* Only use predicted position if it's significantly
                         * different from the actual ping (distance > 50px).
                         * This prevents false triggers near world origin. */
                        if ((predictedPingX != 0.0f || predictedPingY != 0.0f)) {
                            float predDx = predictedPingX - e->targetX;
                            float predDy = predictedPingY - e->targetY;
                            if ((predDx * predDx + predDy * predDy) > 2500.0f) {
                                rushTargetX = predictedPingX;
                                rushTargetY = predictedPingY;
                            }
                        }
                        MoveToward(e, rushTargetX, rushTargetY, deltaTime);

                        if (e->stateTimer <= 0.0f ||
                            DistSq(e->x, e->y, e->targetX, e->targetY) < 100.0f)
                        {
                            /* #8: Hunter Hesitation — 15% chance to pause, scan, then re-engage.
                             * Makes the Hunter feel intelligent instead of robotic. */
                            if (GetRandomValue(0, 99) < 15 && e->stateTimer > 0.0f) {
                                e->state      = ENEMY_STATE_HESITATE;
                                e->stateTimer = 0.6f;
                                /* Face the last known player position with a slow scan. */
                                e->facingAngle = atan2f(e->targetY - e->y, e->targetX - e->x);
                            } else {
                            /* Rush expired — retreat to a random room instead
                             * of idling in place.  Pick a room that is NOT
                             * the start room so the Hunter doesn't camp the
                             * player's spawn. */
                            if (st != NULL && st->roomCount > 1)
                            {
                                int ri;
                                do {
                                    ri = GetRandomValue(1, st->roomCount - 1);
                                } while (ri == st->startRoomIdx && st->roomCount > 2);

                                e->retreatX = st->rooms[ri].x + st->rooms[ri].w * 0.5f;
                                e->retreatY = st->rooms[ri].y + st->rooms[ri].h * 0.5f;
                                e->state    = ENEMY_STATE_RETREAT;
                            }
                            else
                            {
                                /* Fallback: no station data — just idle. */
                                e->state = ENEMY_STATE_IDLE;
                            }
                            }
                        }
                        break;
                    }

                    case ENEMY_STATE_HESITATE:
                    {
                        /* Hunter pauses, scans, then continues the rush.
                         * Can be interrupted by a fresh pulse to re-target. */
                        if (heardPulse)
                        {
                            e->targetX    = pulseX;
                            e->targetY    = pulseY;
                            e->stateTimer = pulseLife + 1.0f;
                            e->alertPulseLife = pulseLife;
                            e->state      = ENEMY_STATE_RUSH;
                            break;
                        }
                        /* Slow scan — rotate facing direction. */
                        e->facingAngle += deltaTime * 1.8f;

                        if (e->stateTimer <= 0.0f)
                        {
                            /* Hesitation complete — resume the rush! */
                            e->state      = ENEMY_STATE_RUSH;
                            e->stateTimer = e->alertPulseLife + 0.5f;
                        }
                        break;
                    }

                    case ENEMY_STATE_RETREAT:
                    {
                        /* Can re-engage if a fresh pulse is heard mid-retreat. */
                        if (heardPulse)
                        {
                            e->speed        = ENEMY_HUNTER_SPEED;
                            e->state        = ENEMY_STATE_ALERT;
                            e->stateTimer   = ENEMY_HUNTER_ALERT_TIME;
                            e->facingAngle  = atan2f(pulseY - e->y, pulseX - e->x);
                            e->targetX      = pulseX;
                            e->targetY      = pulseY;
                            e->alertPulseLife = pulseLife;
                            break;
                        }

                        /* Move toward the retreat room.  Speed is slower
                         * than RUSH — the Hunter is no longer in pursuit. */
                        e->speed = ENEMY_HUNTER_SPEED * 0.6f;
                        MoveToward(e, e->retreatX, e->retreatY, deltaTime);

                        if (DistSq(e->x, e->y, e->retreatX, e->retreatY) < 100.0f)
                        {
                            e->state = ENEMY_STATE_IDLE;
                            e->speed = ENEMY_HUNTER_SPEED; /* restore */
                        }
                        break;
                    }

                    default: break;
                }
                break;
            }

            default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Draw — state-reactive visuals with facing indicator               */
/* ------------------------------------------------------------------ */

/* Watcher colours. */
static const Color W_COLOR_PATROL  = { 200, 50,  50,  255 };
static const Color W_COLOR_PAUSE   = { 180, 60,  60,  255 };
static const Color W_COLOR_INVEST  = { 230, 70,  70,  255 }; /* brighter when hunting */
static const Color W_COLOR_SEARCH  = { 160, 50,  50,  255 };

/* Hunter colours. */
static const Color H_COLOR_IDLE    = { 80,  25,  25,  255 }; /* dim in the dark */
static const Color H_COLOR_RUSH    = { 240, 60,  60,  255 }; /* blazing */

void EnemyManagerDraw(const EnemyManager *manager)
{
    if (manager == NULL) return;

    for (int i = 0; i < manager->count; i++)
    {
        const Enemy *e = &manager->enemies[i];
        if (!e->alive) continue;

        /* Phantoms are NEVER drawn directly — they only appear through
         * echo memory corruption and as a faint silhouette in the
         * echo memory overlay pass. */
        if (e->type == ENEMY_TYPE_PHANTOM) continue;

        Color baseColor;
        Color coreColor;

        if (e->type == ENEMY_TYPE_WATCHER)
        {
            switch (e->state)
            {
                case ENEMY_STATE_PATROL:
                    baseColor = W_COLOR_PATROL;  break;
                case ENEMY_STATE_PAUSE_WAYPOINT:
                    baseColor = W_COLOR_PAUSE;   break;
                case ENEMY_STATE_INVESTIGATE:
                    baseColor = W_COLOR_INVEST;  break;
                case ENEMY_STATE_SEARCH:
                    baseColor = W_COLOR_SEARCH;  break;
                default:
                    baseColor = W_COLOR_PATROL;  break;
            }
            coreColor = baseColor;
        }
        else
        {
            /* Hunter — dynamic colour based on alert state progress. */
            switch (e->state)
            {
                case ENEMY_STATE_IDLE:
                    baseColor = H_COLOR_IDLE;
                    coreColor = (Color){ 100, 30, 30, 255 };
                    break;

                case ENEMY_STATE_ALERT:
                {
                    /* Brighten from idle→alert over the 0.6s window. */
                    float t = (e->stateTimer > 0.0f)
                            ? 1.0f - e->stateTimer / ENEMY_HUNTER_ALERT_TIME
                            : 1.0f;
                    t = fminf(t, 1.0f);
                    /* Pulse subtly during charge-up. */
                    float pulse = sinf(t * 18.0f) * 0.15f + 0.85f;

                    unsigned char r = (unsigned char)((80 + t * 155.0f) * pulse);
                    unsigned char g = (unsigned char)((25 + t * 30.0f) * pulse);
                    unsigned char b = (unsigned char)((25 + t * 30.0f) * pulse);
                    baseColor = (Color){ r, g, b, 255 };
                    /* +20 safely, max r is ~235, 235+20 = 255 = fits. */
                    coreColor = (Color){ (unsigned char)(r + 20), g, b, 255 };
                    break;
                }

                case ENEMY_STATE_RUSH:
                    baseColor = H_COLOR_RUSH;
                    coreColor = (Color){ 255, 80, 80, 255 };
                    break;

                case ENEMY_STATE_RETREAT:
                    /* Dimming down — slowly returning to idle colour. */
                    baseColor = (Color){ 120, 35, 35, 255 };
                    coreColor = (Color){ 140, 45, 45, 255 };
                    break;

                case ENEMY_STATE_HESITATE:
                    /* #8: Hesitation — paused, scanning, mid-brightness with pulse. */
                {
                    float ht = sinf(e->stateTimer * 12.0f) * 0.2f + 0.8f;
                    baseColor = (Color){ (unsigned char)(200 * ht), 50, 50, 255 };
                    coreColor = (Color){ (unsigned char)(220 * ht), 60, 60, 255 };
                    break;
                }

                default:
                    baseColor = H_COLOR_IDLE;
                    coreColor = (Color){ 100, 30, 30, 255 };
                    break;
            }
        }

        Vector2 pos = { e->x, e->y };

        /* --- Glow --- */
        BeginBlendMode(BLEND_ADDITIVE);
        for (int layer = 3; layer >= 1; layer--)
        {
            float t          = (float)layer / 3.0f;
            float layerR     = e->radius + (e->radius * 3.0f - e->radius) * t;
            unsigned char a  = (unsigned char)(50.0f * (1.0f - t) + 8.0f);
            DrawCircleV(pos, layerR, Fade(baseColor, a / 255.0f));
        }
        EndBlendMode();

        /* --- Core --- */
        DrawCircleV(pos, e->radius, coreColor);
        DrawCircleLines((int)e->x, (int)e->y, e->radius, Fade(baseColor, 0.4f));

        /* --- Facing direction indicator --- */
        float dirLen = e->radius + 8.0f;
        float ex = e->x + cosf(e->facingAngle) * dirLen;
        float ey = e->y + sinf(e->facingAngle) * dirLen;
        DrawLineEx(pos, (Vector2){ ex, ey }, 2.5f, Fade(WHITE, 0.5f));
    }
}

/* ------------------------------------------------------------------ */
/*  Collision helper                                                  */
/* ------------------------------------------------------------------ */

CircleShape EnemyGetCollider(const Enemy *enemy)
{
    if (enemy == NULL)
        return (CircleShape){ 0.0f, 0.0f, 0.0f };

    return (CircleShape){
        .x      = enemy->x,
        .y      = enemy->y,
        .radius = enemy->radius
    };
}

/* ------------------------------------------------------------------ */
/*  Shutdown                                                          */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  Get phantom (returns NULL if no phantom exists)                   */
/* ------------------------------------------------------------------ */

const Enemy *EnemyManagerGetPhantom(const EnemyManager *manager)
{
    if (manager == NULL) return NULL;
    for (int i = 0; i < manager->count; i++) {
        if (manager->enemies[i].type == ENEMY_TYPE_PHANTOM && manager->enemies[i].alive)
            return &manager->enemies[i];
    }
    return NULL;
}

void EnemyManagerShutdown(EnemyManager *manager)
{
    (void)manager;
}
