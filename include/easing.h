#ifndef ECHO_EASING_H
#define ECHO_EASING_H

/*
 * Shared easing utilities.
 *
 * Consolidates the easing functions that were previously duplicated
 * across sonar.c and game.c into a single location.
 */

#include <math.h>

/* Ease-out cubic: fast start, gentle deceleration to a stop. */
static inline float EaseOutCubic(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return 1.0f - powf(1.0f - t, 3.0f);
}

#endif /* ECHO_EASING_H */
