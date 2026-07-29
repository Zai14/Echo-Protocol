/*
 * Procedural generation module implementation (stub).
 * See include/procedural.h for intent. A basic xorshift RNG is
 * implemented so downstream systems can already draw seeded random
 * numbers; content generation itself is not implemented yet.
 */

#include "procedural.h"
#include <stddef.h>
#include <stdint.h>

void ProceduralInit(ProceduralGenerator *gen, uint64_t seed)
{
    if (gen == NULL) {
        return;
    }

    gen->seed  = seed;
    gen->state = seed != 0 ? seed : 0x9E3779B97F4A7C15ULL;
}

uint32_t ProceduralNextUInt(ProceduralGenerator *gen)
{
    if (gen == NULL) {
        return 0;
    }

    /* xorshift64* */
    uint64_t x = gen->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    gen->state = x;

    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

float ProceduralNextFloat01(ProceduralGenerator *gen)
{
    return (float)ProceduralNextUInt(gen) / (float)UINT32_MAX;
}
