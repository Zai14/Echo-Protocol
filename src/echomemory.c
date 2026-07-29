/*
 * Echo Memory implementation.
 *
 * Each grid cell that a sonar pulse sweeps over is captured at
 * the moment of first contact.  The cell then renders through
 * two visual phases:
 *
 *   FULL VISIBILITY  — bright white-cyan outline (0–0.7 s)
 *   BLUE WIREFRAME   — fading blue outline      (0.7–3.7 s)
 *
 * After ~3.7 s the cell is pruned.  The effect makes the world
 * feel like it is "remembered" rather than always visible.
 */

#include "echomemory.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Total lifetime of one memory cell (seconds). */
#define CELL_LIFETIME  (ECHO_MEMORY_TRANSITION_TIME + ECHO_MEMORY_FADE_TIME)

/* ------------------------------------------------------------------ */
/*  Init                                                               */
/* ------------------------------------------------------------------ */

void EchoMemoryInit(EchoMemory *memory)
{
    if (memory == NULL) return;
    memset(memory, 0, sizeof(*memory));
}

/* ------------------------------------------------------------------ */
/*  Reveal — mark a cell at a world position                          */
/* ------------------------------------------------------------------ */

void EchoMemoryReveal(EchoMemory *memory, float worldX, float worldY, float gameTime)
{
    if (memory == NULL) return;

    /* Compute grid cell index from world position.
     * Use floorf to handle negative coordinates correctly. */
    int cx = (int)floorf(worldX / (float)ECHO_MEMORY_CELL_SIZE);
    int cy = (int)floorf(worldY / (float)ECHO_MEMORY_CELL_SIZE);

    /* Search for an existing cell at this index — if found, bump
     * its revealTime so the memory resets to full brightness. */
    for (int i = 0; i < memory->count; i++)
    {
        if (memory->cells[i].cellX == cx && memory->cells[i].cellY == cy)
        {
            memory->cells[i].revealTime = gameTime;
            memory->cells[i].active     = true;
            return;
        }
    }

    /* Not found — create a new cell if there is room. */
    if (memory->count >= ECHO_MAX_MEMORY_CELLS) return;

    memory->cells[memory->count].cellX       = cx;
    memory->cells[memory->count].cellY       = cy;
    memory->cells[memory->count].revealTime  = gameTime;
    memory->cells[memory->count].active      = true;
    memory->count++;
}

/* ------------------------------------------------------------------ */
/*  Cell colour / alpha helper                                        */
/* ------------------------------------------------------------------ */

float EchoMemoryGetCellColor(float age, Color *outColor)
{
    if (outColor == NULL) return 0.0f;

    if (age < 0.0f || age >= CELL_LIFETIME)
    {
        outColor->r = 0; outColor->g = 0; outColor->b = 0; outColor->a = 0;
        return 0.0f;
    }

    if (age < ECHO_MEMORY_TRANSITION_TIME)
    {
        /* — FULL VISIBILITY PHASE — */
        /* Lerp from bright white (age=0) to the wireframe blue (age=TRANSITION). */
        float t = age / ECHO_MEMORY_TRANSITION_TIME;
        outColor->r = (unsigned char)(255.0f * (1.0f - t) + 80.0f * t);
        outColor->g = (unsigned char)(255.0f * (1.0f - t) + 190.0f * t);
        outColor->b = 255;
        outColor->a = 255;
        return 1.0f;
    }

    /* — BLUE WIREFRAME PHASE (with #9 Memory Corruption) — */
    float fadeElapsed = age - ECHO_MEMORY_TRANSITION_TIME;
    float decayRatio  = fadeElapsed / ECHO_MEMORY_FADE_TIME;
    float alpha       = 1.0f - decayRatio;
    if (alpha < 0.0f) alpha = 0.0f;

    /* Purple corruption phase: in the last 40% of the fade,
     * the wireframe shifts from blue to purple before disappearing.
     * This subtly signals that the memory itself is degrading. */
    if (decayRatio > 0.6f) {
        float purpleT = (decayRatio - 0.6f) / 0.4f;
        outColor->r = (unsigned char)(60.0f + 140.0f * purpleT);  /* 60 → 200 */
        outColor->g = (unsigned char)(180.0f - 120.0f * purpleT); /* 180 → 60 */
        outColor->b = (unsigned char)(255.0f - 150.0f * purpleT); /* 255 → 105 */
    } else {
        outColor->r = 60;
        outColor->g = 180;
        outColor->b = 255;
    }
    outColor->a = (unsigned char)(alpha * 255.0f);
    return alpha;
}

/* ------------------------------------------------------------------ */
/*  Compact                                                           */
/* ------------------------------------------------------------------ */

void EchoMemoryCompact(EchoMemory *memory, float gameTime)
{
    if (memory == NULL) return;

    int writeIdx = 0;
    for (int i = 0; i < memory->count; i++)
    {
        float age = gameTime - memory->cells[i].revealTime;
        bool expired = (age < 0.0f || age >= CELL_LIFETIME);

        if (!expired && memory->cells[i].active)
        {
            if (writeIdx != i)
                memory->cells[writeIdx] = memory->cells[i];
            writeIdx++;
        }
    }
    memory->count = writeIdx;
}
