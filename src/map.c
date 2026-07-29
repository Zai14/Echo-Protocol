/*
 * Procedural Station Generator implementation.
 *
 * Algorithm:
 *   1. Place the first room at world origin (start room).
 *   2. For each additional room, try random positions until one
 *      doesn't overlap any existing room (with padding).
 *   3. Connect the new room to its nearest neighbour with an
 *      L-shaped corridor (horizontal → vertical or vice versa).
 *   4. Determine the objective room (furthest from start).
 *   5. Pick random non-start rooms for enemy spawns.
 *
 * All randomness is driven by a seeded xorshift64* RNG so the
 * layout is fully deterministic from the seed.
 */

#include "map.h"
#include "procedural.h"
#include "raylib.h"
#include "collision.h"
#include <string.h>
#include <stddef.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Room helper                                                       */
/* ------------------------------------------------------------------ */

static float RoomCX(const Room *r) { return r->x + r->w * 0.5f; }
static float RoomCY(const Room *r) { return r->y + r->h * 0.5f; }

/* ------------------------------------------------------------------ */
/*  Overlap test — true if two rooms overlap (with padding)           */
/* ------------------------------------------------------------------ */

static bool RoomsOverlap(const Room *a, const Room *b, float pad)
{
    return (a->x - pad < b->x + b->w + pad &&
            a->x + a->w + pad > b->x - pad &&
            a->y - pad < b->y + b->h + pad &&
            a->y + a->h + pad > b->y - pad);
}

/* ------------------------------------------------------------------ */
/*  Distance squared between room centres                             */
/* ------------------------------------------------------------------ */

static float RoomDistSq(const Room *a, const Room *b)
{
    float dx = RoomCX(a) - RoomCX(b);
    float dy = RoomCY(a) - RoomCY(b);
    return dx * dx + dy * dy;
}

/* ------------------------------------------------------------------ */
/*  Generate a random float in [min, max)                             */
/* ------------------------------------------------------------------ */

static float RandRange(ProceduralGenerator *rng, float min, float max)
{
    return min + ProceduralNextFloat01(rng) * (max - min);
}

/* ------------------------------------------------------------------ */
/*  Add a corridor segment and union it with overlapping segments     */
/* ------------------------------------------------------------------ */

static void AddCorridor(Station *s, float cx, float cy, float cw, float ch)
{
    if (s->corridorCount >= MAX_CORRIDORS) return;

    Corridor *c = &s->corridors[s->corridorCount];
    c->x = cx; c->y = cy; c->w = cw; c->h = ch;
    s->corridorCount++;
}

/* Build an L-shaped corridor from room a's centre to room b's centre.
 * The bend direction is chosen by the RNG. */
static void LinkRooms(Station *s, int ai, int bi, ProceduralGenerator *rng)
{
    float ax = RoomCX(&s->rooms[ai]);
    float ay = RoomCY(&s->rooms[ai]);
    float bx = RoomCX(&s->rooms[bi]);
    float by = RoomCY(&s->rooms[bi]);

    float hw = CORRIDOR_HALF;

    /* Two possible L-shapes.  Pick one. */
    if (ProceduralNextUInt(rng) & 1u)
    {
        /* Horizontal then vertical (bend at x = bx, y = ay). */
        float cx1 = fminf(ax, bx);
        float cy1 = ay - hw;
        float cw1 = fabsf(bx - ax);
        float ch1 = CORRIDOR_WIDTH;
        AddCorridor(s, cx1, cy1, cw1, ch1);

        float cx2 = bx - hw;
        float cy2 = fminf(ay, by);
        float cw2 = CORRIDOR_WIDTH;
        float ch2 = fabsf(by - ay);
        AddCorridor(s, cx2, cy2, cw2, ch2);
    }
    else
    {
        /* Vertical then horizontal (bend at x = ax, y = by). */
        float cx1 = ax - hw;
        float cy1 = fminf(ay, by);
        float cw1 = CORRIDOR_WIDTH;
        float ch1 = fabsf(by - ay);
        AddCorridor(s, cx1, cy1, cw1, ch1);

        float cx2 = fminf(ax, bx);
        float cy2 = by - hw;
        float cw2 = fabsf(bx - ax);
        float ch2 = CORRIDOR_WIDTH;
        AddCorridor(s, cx2, cy2, cw2, ch2);
    }
}

/* ------------------------------------------------------------------ */
/*  StationGenerate                                                   */
/* ------------------------------------------------------------------ */

void StationGenerate(Station *station, uint64_t seed)
{
    if (station == NULL) return;
    memset(station, 0, sizeof(*station));

    /* Seed the RNG (0 → default non-zero). */
    station->seed = (seed != 0) ? seed : 0x9E3779B97F4A7C15ULL;
    ProceduralGenerator rng;
    ProceduralInit(&rng, station->seed);

    /* ----- Step 1: first room at world origin ----- */
    {
        Room *r = &station->rooms[station->roomCount++];
        r->w = RandRange(&rng, 100.0f, 150.0f);
        r->h = RandRange(&rng, 100.0f, 150.0f);
        r->x = -r->w * 0.5f;
        r->y = -r->h * 0.5f;
    }

    /* ----- Step 2: place remaining rooms ----- */
    for (int i = 1; i < MAX_ROOMS; i++)
    {
        Room candidate;
        bool placed = false;

        for (int attempt = 0; attempt < ROOM_PLACE_ATTEMPTS; attempt++)
        {
            candidate.w = RandRange(&rng, MIN_ROOM_SIZE, MAX_ROOM_SIZE);
            candidate.h = RandRange(&rng, MIN_ROOM_SIZE, MAX_ROOM_SIZE);

            /* Random position centred on one of the existing rooms. */
            int anchorIdx = ProceduralNextUInt(&rng) % station->roomCount;
            const Room *anchor = &station->rooms[anchorIdx];

            float anchorCX = RoomCX(anchor);
            float anchorCY = RoomCY(anchor);

            candidate.x = anchorCX + RandRange(&rng, -300.0f, 300.0f) - candidate.w * 0.5f;
            candidate.y = anchorCY + RandRange(&rng, -300.0f, 300.0f) - candidate.h * 0.5f;

            /* Check overlap against all existing rooms. */
            bool overlaps = false;
            for (int j = 0; j < station->roomCount; j++)
            {
                if (RoomsOverlap(&candidate, &station->rooms[j], ROOM_PADDING))
                {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) continue;

            placed = true;
            break;
        }

        if (!placed)
        {
            /* Couldn't fit this room — stop adding more. */
            break;
        }

        /* Add the room. */
        int newIdx = station->roomCount;
        station->rooms[newIdx] = candidate;
        station->roomCount++;

        /* ----- Step 3: connect to nearest existing room ----- */
        int nearestIdx = 0;
        float nearestDist = RoomDistSq(&candidate, &station->rooms[0]);
        for (int j = 1; j < newIdx; j++)
        {
            float d = RoomDistSq(&candidate, &station->rooms[j]);
            if (d < nearestDist) { nearestDist = d; nearestIdx = j; }
        }
        LinkRooms(station, newIdx, nearestIdx, &rng);

        /* If we have enough rooms, stop. */
        if (station->roomCount >= MAX_ROOMS) break;
    }

    /* ----- Step 4: pick objective / transmitter room (furthest from start) ----- */
    {
        float bestDist = 0.0f;
        station->objectiveRoomIdx = 0;
        for (int i = 1; i < station->roomCount; i++)
        {
            float d = RoomDistSq(&station->rooms[i], &station->rooms[0]);
            if (d > bestDist) { bestDist = d; station->objectiveRoomIdx = i; }
        }
    }

    /* ----- Step 5: pick relay rooms (3 random non-start, non-transmitter) ----- */
    {
        /* Build a list of candidate room indices. */
        int candidates[MAX_ROOMS];
        int candidateCount = 0;
        for (int i = 1; i < station->roomCount; i++)
        {
            if (i != station->objectiveRoomIdx)
                candidates[candidateCount++] = i;
        }

        /* Shuffle (Fisher–Yates). */
        for (int i = candidateCount - 1; i > 0; i--)
        {
            int j = ProceduralNextUInt(&rng) % (i + 1);
            int tmp = candidates[i]; candidates[i] = candidates[j]; candidates[j] = tmp;
        }

        /* Take up to 3 for relays. */
        int relayPick = (candidateCount < 3) ? candidateCount : 3;
        for (int i = 0; i < relayPick; i++)
            station->relayRoomIdxs[i] = candidates[i];

        /* The next unused candidate becomes the airlock (if available). */
        if (relayPick < candidateCount)
            station->airlockRoomIdx = candidates[relayPick];
        else
            station->airlockRoomIdx = station->objectiveRoomIdx; /* fallback */
    }

    /* ----- Step 6: place enemy spawns in remaining non-start rooms ----- */
    {
        station->enemySpawnCount = 0;

        /* Build a list of candidate room indices (everything except start). */
        int candidates[MAX_ROOMS];
        int candidateCount = 0;
        for (int i = 1; i < station->roomCount; i++)
            candidates[candidateCount++] = i;

        /* Shuffle (Fisher–Yates). */
        for (int i = candidateCount - 1; i > 0; i--)
        {
            int j = ProceduralNextUInt(&rng) % (i + 1);
            int tmp = candidates[i]; candidates[i] = candidates[j]; candidates[j] = tmp;
        }

        /* Take up to STATION_MAX_ENEMY_SPAWNS rooms. */
        int spawnCount = (candidateCount < 3) ? candidateCount : 3;
        for (int i = 0; i < spawnCount; i++)
        {
            int ri = candidates[i];
            station->enemySpawnX[i] = RoomCX(&station->rooms[ri]);
            station->enemySpawnY[i] = RoomCY(&station->rooms[ri]);
            station->enemySpawnCount++;
        }
    }

    /* ----- Step 7: pick interference zones (rooms that distort sonar) ----- */
    {
        station->interferenceRoomCount = 0;
        /* Re-shuffle candidate rooms and pick 1-2 that are NOT start,
         * objective, relay, or airlock. */
        int candidates[MAX_ROOMS];
        int candidateCount = 0;
        for (int i = 1; i < station->roomCount; i++)
        {
            if (i == station->objectiveRoomIdx) continue;
            if (i == station->airlockRoomIdx) continue;
            bool isRelay = false;
            for (int r = 0; r < 3; r++) {
                if (station->relayRoomIdxs[r] == i) { isRelay = true; break; }
            }
            if (isRelay) continue;
            candidates[candidateCount++] = i;
        }
        
        for (int i = candidateCount - 1; i > 0; i--)
        {
            int j = ProceduralNextUInt(&rng) % (i + 1);
            int tmp = candidates[i]; candidates[i] = candidates[j]; candidates[j] = tmp;
        }
        
        int take = (candidateCount < 2) ? candidateCount : 1 + (int)(ProceduralNextUInt(&rng) % 2);
        for (int i = 0; i < take && i < 2; i++) {
            station->interferenceRoomIdxs[i] = candidates[i];
            station->interferenceRoomCount++;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  StationDraw                                                       */
/* ------------------------------------------------------------------ */

static const Color COLOR_FLOOR = { 15, 20, 30, 255 };
static const Color COLOR_WALL  = { 60, 80, 110, 255 };

/* Subtle colour variation by room distance from start.
   Rooms further away shift slightly toward a colder blue tone,
   giving the player a subtle sense of progression/depth. */
static Color FloorTint(int roomIdx, int totalRooms)
{
    float t = (totalRooms <= 1) ? 0.0f : (float)roomIdx / (float)(totalRooms - 1);
    unsigned char shift = (unsigned char)(t * 12.0f);
    return (Color){
        (unsigned char)(COLOR_FLOOR.r - shift),
        (unsigned char)(COLOR_FLOOR.g - shift / 2),
        (unsigned char)(COLOR_FLOOR.b + shift),
        255
    };
}

static Color WallTint(int roomIdx, int totalRooms)
{
    float t = (totalRooms <= 1) ? 0.0f : (float)roomIdx / (float)(totalRooms - 1);
    unsigned char shift = (unsigned char)(t * 15.0f);
    return (Color){
        (unsigned char)(COLOR_WALL.r - shift),
        (unsigned char)(COLOR_WALL.g - shift / 3),
        (unsigned char)(COLOR_WALL.b + shift),
        255
    };
}

void StationDraw(const Station *station)
{
    if (station == NULL) return;

    /* --- Corridor floors (slightly darker than rooms for depth) --- */
    static const Color COLOR_CORRIDOR_FLOOR = { 10, 14, 22, 255 };
    for (int i = 0; i < station->corridorCount; i++)
    {
        const Corridor *c = &station->corridors[i];
        DrawRectangleRec((Rectangle){ c->x, c->y, c->w, c->h }, COLOR_CORRIDOR_FLOOR);
    }

    /* --- Room floors (subtly tinted by distance from start) --- */
    for (int i = 0; i < station->roomCount; i++)
    {
        const Room *r = &station->rooms[i];
        DrawRectangleRec((Rectangle){ r->x, r->y, r->w, r->h },
                         FloorTint(i, station->roomCount));
    }

    /* --- Room wall outlines (subtly tinted by distance from start) --- */
    for (int i = 0; i < station->roomCount; i++)
    {
        const Room *r = &station->rooms[i];
        DrawRectangleLinesEx((Rectangle){ r->x, r->y, r->w, r->h }, 2.0f,
                             WallTint(i, station->roomCount));
    }

    /* --- Corridor wall outlines (dimmer — narrower passages feel tighter) --- */
    static const Color COLOR_CORRIDOR_WALL = { 40, 55, 75, 255 };
    for (int i = 0; i < station->corridorCount; i++)
    {
        const Corridor *c = &station->corridors[i];
        DrawRectangleLinesEx((Rectangle){ c->x, c->y, c->w, c->h }, 1.5f, COLOR_CORRIDOR_WALL);
    }

    /* --- Corridor light strips: faint blue lines along corridor floors --- */
    static const Color COLOR_LIGHT_STRIP = { 80, 130, 200, 255 };
    for (int i = 0; i < station->corridorCount; i++)
    {
        const Corridor *c = &station->corridors[i];
        float stripAlpha = 0.08f;
        /* Horizontal corridor: draw a thin line along the centre. */
        if (c->w > c->h) {
            float lx = c->x + 4.0f;
            float ly = c->y + c->h * 0.5f;
            float lw = c->w - 8.0f;
            DrawLineEx((Vector2){ lx, ly }, (Vector2){ lx + lw, ly }, 1.0f,
                       Fade(COLOR_LIGHT_STRIP, stripAlpha));
        }
        /* Vertical corridor: draw a thin line along the centre. */
        else {
            float lx = c->x + c->w * 0.5f;
            float ly = c->y + 4.0f;
            float lh = c->h - 8.0f;
            DrawLineEx((Vector2){ lx, ly }, (Vector2){ lx, ly + lh }, 1.0f,
                       Fade(COLOR_LIGHT_STRIP, stripAlpha));
        }
    }

}

/* ------------------------------------------------------------------ */
/*  StationRoomCentre                                                 */
/* ------------------------------------------------------------------ */

void StationRoomCentre(const Station *station, int idx, float *ox, float *oy)
{
    if (station == NULL || idx < 0 || idx >= station->roomCount) return;
    *ox = RoomCX(&station->rooms[idx]);
    *oy = RoomCY(&station->rooms[idx]);
}

/* ------------------------------------------------------------------ */
/*  StationIsWalkable                                                 */
/* ------------------------------------------------------------------ */

bool StationIsWalkable(const Station *station, float cx, float cy, float radius)
{
    if (station == NULL) return false;

    CircleShape c = { cx, cy, radius };

    /* Check against all rooms. */
    for (int i = 0; i < station->roomCount; i++)
    {
        RectShape r = {
            station->rooms[i].x,
            station->rooms[i].y,
            station->rooms[i].w,
            station->rooms[i].h
        };
        if (CollisionCircleRect(c, r)) return true;
    }

    /* Check against all corridors. */
    for (int i = 0; i < station->corridorCount; i++)
    {
        RectShape r = {
            station->corridors[i].x,
            station->corridors[i].y,
            station->corridors[i].w,
            station->corridors[i].h
        };
        if (CollisionCircleRect(c, r)) return true;
    }

    return false;
}
