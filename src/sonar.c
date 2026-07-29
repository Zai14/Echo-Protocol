/*
 * Sonar module implementation.
 *
 * Manages expanding reveal pulses — the game's signature mechanic.
 *
 * Pulse expansion uses ease-out cubic so the wave starts fast and
 * gracefully slows down — feels like a real energy burst radiating
 * outward. The reveal decay is also ease-out: full visibility is
 * held for a brief moment, then the world fades back into darkness
 * with a smooth tail rather than a hard cut.
 *
 * See include/sonar.h for the public API description.
 */

#include "sonar.h"
#include "easing.h"
#include <string.h>
#include <math.h>

void SonarInit(SonarSystem *sonar)
{
    if (sonar == NULL) {
        return;
    }

    memset(sonar, 0, sizeof(*sonar));
    sonar->cooldownDuration = ECHO_SONAR_COOLDOWN;
    sonar->cooldownTimer    = 0.0f;
    sonar->pulseCount       = 0;
}

void SonarUpdate(SonarSystem *sonar, float deltaTime)
{
    if (sonar == NULL) {
        return;
    }

    /* --- Cooldown --- */
    if (sonar->cooldownTimer > 0.0f) {
        sonar->cooldownTimer -= deltaTime;
        if (sonar->cooldownTimer < 0.0f) {
            sonar->cooldownTimer = 0.0f;
        }
    }

    /* --- Step active pulses --- */
    for (int i = 0; i < sonar->pulseCount; i++) {
        SonarPulse *pulse = &sonar->pulses[i];
        if (!pulse->active) {
            continue;
        }

        pulse->elapsed += deltaTime;

        /* Expand radius with ease-out cubic: starts fast, slows gracefully. */
        float expansionDuration = pulse->maxRadius / pulse->speed;
        if (pulse->elapsed < expansionDuration) {
            float t = pulse->elapsed / expansionDuration;
            pulse->currentRadius = pulse->maxRadius * EaseOutCubic(t);
        } else {
            pulse->currentRadius = pulse->maxRadius;
        }

        /* Deactivate when lifetime is exhausted. */
        if (pulse->elapsed >= pulse->lifetime) {
            pulse->active = false;
        }
    }

    /* --- Compact the list (remove inactive pulses) --- */
    int writeIdx = 0;
    for (int i = 0; i < sonar->pulseCount; i++) {
        if (sonar->pulses[i].active) {
            if (writeIdx != i) {
                sonar->pulses[writeIdx] = sonar->pulses[i];
            }
            writeIdx++;
        }
    }
    sonar->pulseCount = writeIdx;
}

bool SonarEmitPulse(SonarSystem *sonar, float originX, float originY)
{
    if (sonar == NULL) {
        return false;
    }
    if (sonar->cooldownTimer > 0.0f) {
        return false;   /* still on cooldown */
    }
    if (sonar->pulseCount >= ECHO_SONAR_MAX_PULSES) {
        return false;   /* at capacity */
    }

    float expansionDuration = ECHO_SONAR_MAX_RADIUS / ECHO_SONAR_DEFAULT_SPEED;

    SonarPulse *pulse = &sonar->pulses[sonar->pulseCount];
    pulse->originX       = originX;
    pulse->originY       = originY;
    pulse->currentRadius = 0.0f;
    pulse->maxRadius     = ECHO_SONAR_MAX_RADIUS;
    pulse->speed         = ECHO_SONAR_DEFAULT_SPEED;
    pulse->elapsed       = 0.0f;
    pulse->lifetime      = expansionDuration + ECHO_SONAR_REVEAL_TIME;
    pulse->active        = true;

    sonar->pulseCount++;
    sonar->cooldownTimer = sonar->cooldownDuration;

    return true;
}

float SonarPulseRevealStrength(const SonarPulse *pulse)
{
    if (pulse == NULL || !pulse->active) {
        return 0.0f;
    }

    float expansionDuration = pulse->maxRadius / pulse->speed;
    float revealDuration    = pulse->lifetime - expansionDuration;

    /* During expansion — full reveal strength. */
    if (pulse->elapsed < expansionDuration) {
        return 1.0f;
    }

    /* After expansion — ease-out cubic decay for a smooth, natural fade.
     * Stays bright for a moment then fades with a soft tail.
     * At t=0.3 strength=0.34, at t=0.5 strength=0.125 —
     * gentler than quart (0.062 at t=0.5) for better readability. */
    if (revealDuration <= 0.0f) {
        return 1.0f;
    }

    float decayTime = pulse->elapsed - expansionDuration;
    float t = fminf(decayTime / revealDuration, 1.0f);
    return 1.0f - EaseOutCubic(t);
}

void SonarShutdown(SonarSystem *sonar)
{
    (void)sonar;
    /* Nothing to free — all data is stack-allocated in the Game struct. */
}
