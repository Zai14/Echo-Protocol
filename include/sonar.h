#ifndef ECHO_SONAR_H
#define ECHO_SONAR_H

#include <stdbool.h>

/*
 * Sonar module — Echo Protocol's signature mechanic.
 *
 * Emits expanding sonar pulses that reveal the environment.
 * Each pulse expands from its origin; areas the wave has passed
 * over stay visible for a brief reveal window before fading.
 *
 * Controls (wired in game.c):
 *   SPACE  — emit a pulse (respects cooldown)
 *
 * Integration:
 *   SonarUpdate() steps all active pulses and cooldown.
 *   SonarPulseRevealStrength() returns 0..1 for shader input.
 *   game.c bridges these into RendererSetSonarPulses() each frame.
 */

#define ECHO_SONAR_MAX_PULSES      3
#define ECHO_SONAR_DEFAULT_SPEED   300.0f   /* px/s expansion speed */
#define ECHO_SONAR_MAX_RADIUS      400.0f   /* px maximum wave radius */
#define ECHO_SONAR_COOLDOWN         2.0f    /* seconds between pulses */
#define ECHO_SONAR_REVEAL_TIME      1.0f    /* seconds objects stay visible after wave passes */

typedef struct SonarPulse {
    float originX;
    float originY;
    float currentRadius;   /* expands from 0 → maxRadius */
    float maxRadius;
    float speed;
    float elapsed;         /* seconds since emission */
    float lifetime;        /* total lifetime before deactivation (expansion + reveal) */
    bool  active;
} SonarPulse;

typedef struct SonarSystem {
    SonarPulse pulses[ECHO_SONAR_MAX_PULSES];
    int        pulseCount;
    float      cooldownTimer;     /* counts down to 0; 0 means ready */
    float      cooldownDuration;
} SonarSystem;

void SonarInit(SonarSystem *sonar);
void SonarUpdate(SonarSystem *sonar, float deltaTime);

/* Attempt to emit a pulse at (originX, originY).
 * Returns true if the pulse was created, false if on cooldown / at capacity. */
bool SonarEmitPulse(SonarSystem *sonar, float originX, float originY);

/* Current reveal strength for a pulse (0..1).
 * Full strength (1.0) during expansion, then decays to 0 over
 * the reveal persistence window. */
float SonarPulseRevealStrength(const SonarPulse *pulse);

void SonarShutdown(SonarSystem *sonar);

#endif /* ECHO_SONAR_H */
