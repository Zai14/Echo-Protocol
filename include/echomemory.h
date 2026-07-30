#ifndef ECHO_MEMORY_H
#define ECHO_MEMORY_H

/*
 * Echo Memory — the persistent trace a sonar pulse leaves behind.
 *
 * When a pulse reveals part of the world, those grid cells are
 * captured here. They render in two distinct phases:
 *
 *   1. FULL VISIBILITY (0 – TRANSITION_TIME seconds)
 *      The cell is drawn bright white-cyan, overlapping with the
 *      active shader reveal so the object appears fully lit.
 *
 *   2. BLUE WIREFRAME (TRANSITION_TIME – FADE_TIME seconds)
 *      The cell's outline is drawn as a glowing blue wireframe
 *      that smoothly fades to transparent over ~3 seconds.
 *
 * After the wireframe fades, the cell is pruned so it never
 * draws again.
 *
 * CORRUPTION: The Phantom enemy can mark cells as corrupted,
 * causing them to render purple/magenta with static noise
 * instead of the normal blue wireframe.
 *
 * Controls: none — driven automatically by SonarPulse expansion
 * in game.c.  Corruption is triggered by PhantomUpdate in game.c.
 */

#include <stdbool.h>
#include "raylib.h"

/* ------------------------------------------------------------------ */
/*  Tuning constants                                                   */
/* ------------------------------------------------------------------ */

#define ECHO_MEMORY_CELL_SIZE       64   /* px width/height of one grid cell */
#define ECHO_MEMORY_TRANSITION_TIME  0.7f /* seconds of full brightness before wireframe */
#define ECHO_MEMORY_FADE_TIME        3.0f /* seconds the blue wireframe takes to vanish */
/* Total memory lifetime = TRANSITION_TIME + FADE_TIME */

#define ECHO_MAX_MEMORY_CELLS       1024

/* ------------------------------------------------------------------ */
/*  Core data types                                                    */
/* ------------------------------------------------------------------ */

typedef struct EchoMemoryCell {
    int   cellX;          /* world-space grid column (worldX / CELL_SIZE) */
    int   cellY;          /* world-space grid row    (worldY / CELL_SIZE) */
    float revealTime;     /* game elapsedTime when first revealed by sonar */
    bool  active;
    bool  corrupted;      /* true if the Phantom has passed through this cell */
} EchoMemoryCell;

typedef struct EchoMemory {
    EchoMemoryCell cells[ECHO_MAX_MEMORY_CELLS];
    int            count;
} EchoMemory;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void EchoMemoryInit(EchoMemory *memory);

/* Mark the grid cell containing (worldX, worldY) as revealed at the
 * given gameTime.  The cell is created if it doesn't exist, or its
 * revealTime is bumped if it does (so the latest pulse keeps it alive). */
void EchoMemoryReveal(EchoMemory *memory, float worldX, float worldY, float gameTime);

/* Fill *outColor with the interpolated colour for a cell of the
 * given age.  Alpha is set according to the transition/fade phase.
 * If the cell is corrupted, colours shift to purple/magenta.
 * Returns the alpha value (0..1) for convenience. */
float EchoMemoryGetCellColor(float age, bool corrupted, Color *outColor);

/* Mark the grid cell at (worldX, worldY) as corrupted, causing it
 * to render with a purple/static overlay instead of blue.
 * Returns true if a cell was found and corrupted. */
bool EchoMemoryCorrupt(EchoMemory *memory, float worldX, float worldY);

/* Remove cells whose lifetime has expired and compact the list.
 * Call this once per frame after EchoMemoryDraw. */
void EchoMemoryCompact(EchoMemory *memory, float gameTime);

#endif /* ECHO_MEMORY_H */
