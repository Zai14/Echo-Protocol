/*
 * Sound Propagation implementation.
 *
 * Manages a pool of transient sound events that expand outward
 * and fade over time.  Enemies query the system each frame to
 * detect nearby sounds and decide what to investigate.
 *
 * See include/soundprop.h for the public API.
 */

#include "soundprop.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */

void SoundPropInit(SoundPropagation *sp)
{
    if (sp == NULL) return;
    memset(sp, 0, sizeof(*sp));
}

/* ------------------------------------------------------------------ */
/*  Emit                                                              */
/* ------------------------------------------------------------------ */

bool SoundPropEmit(SoundPropagation *sp, SoundEventType type,
                   float x, float y, float radius,
                   float intensity, float lifetime)
{
    if (sp == NULL) return false;
    if (sp->count >= ECHO_MAX_SOUND_EVENTS) return false;

    SoundEvent *ev = &sp->events[sp->count];
    ev->type      = type;
    ev->x         = x;
    ev->y         = y;
    ev->radius    = radius;
    ev->intensity = (intensity < 0.0f) ? 0.0f :
                    (intensity > 1.0f) ? 1.0f : intensity;
    ev->lifetime  = (lifetime < 0.0f) ? 0.0f : lifetime;
    ev->elapsed   = 0.0f;
    ev->active    = true;

    sp->count++;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Update + compact                                                  */
/* ------------------------------------------------------------------ */

void SoundPropUpdate(SoundPropagation *sp, float deltaTime)
{
    if (sp == NULL) return;

    int writeIdx = 0;

    for (int i = 0; i < sp->count; i++)
    {
        SoundEvent *ev = &sp->events[i];

        if (!ev->active)
        {
            /* Skip inactive — it will be overwritten or left behind. */
            continue;
        }

        /* Advance elapsed time. */
        ev->elapsed += deltaTime;

        /* Deactivate when lifetime expires. */
        if (ev->elapsed >= ev->lifetime)
        {
            ev->active = false;
            continue;
        }

        /* Keep this event — move it up if there was a gap. */
        if (writeIdx != i)
        {
            sp->events[writeIdx] = *ev;
        }
        writeIdx++;
    }

    sp->count = writeIdx;
}

/* ------------------------------------------------------------------ */
/*  Query                                                             */
/* ------------------------------------------------------------------ */

int SoundPropQuery(const SoundPropagation *sp,
                   float qx, float qy, float queryRadius,
                   SoundEvent *outEvents, int maxOut)
{
    if (sp == NULL || outEvents == NULL || maxOut <= 0) return 0;

    int found = 0;

    for (int i = 0; i < sp->count && found < maxOut; i++)
    {
        const SoundEvent *ev = &sp->events[i];

        if (!ev->active) continue;

        /* For an enemy to hear the sound, their hearing range
         * (queryRadius at qx,qy) must intersect the sound's
         * propagation sphere (radius at x,y).  We check if the
         * distance between their centres <= sum of the radii. */
        float dx    = ev->x - qx;
        float dy    = ev->y - qy;
        float dist  = sqrtf(dx * dx + dy * dy);
        float range = ev->radius + queryRadius;

        if (dist > range) continue;  /* too far away */

        /* Sound is within range — copy it out. */
        outEvents[found] = *ev;
        found++;
    }

    return found;
}
