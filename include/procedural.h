#ifndef ECHO_PROCEDURAL_H
#define ECHO_PROCEDURAL_H

#include <stdint.h>

/*
 * Procedural generation module (stub).
 *
 * Will drive procedural map/level generation (caves, tunnels, points
 * of interest) using a seeded RNG. Only the seed/RNG plumbing exists
 * for now.
 */

typedef struct ProceduralGenerator {
    uint64_t seed;
    uint64_t state;
} ProceduralGenerator;

void ProceduralInit(ProceduralGenerator *gen, uint64_t seed);
uint32_t ProceduralNextUInt(ProceduralGenerator *gen);
float ProceduralNextFloat01(ProceduralGenerator *gen);

#endif /* ECHO_PROCEDURAL_H */
