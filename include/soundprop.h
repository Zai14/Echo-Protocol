#ifndef ECHO_SOUND_PROP_H
#define ECHO_SOUND_PROP_H

#include <stdbool.h>

/*
 * Sound Propagation System.
 *
 * Models sound events propagating through the environment.
 * Each event has a world position, a propagation radius, an
 * intensity, and a lifetime.  Enemies will later query the
 * system to detect nearby sounds and react accordingly.
 *
 * Sound events (emitted from game.c):
 *   FOOTSTEP        — player walking, radius 80, short lifetime
 *   SONAR_PULSE     — sonar ping, radius 400, medium lifetime
 */

/* ------------------------------------------------------------------ */
/*  Event types                                                        */
/* ------------------------------------------------------------------ */

typedef enum SoundEventType {
    SOUND_EVENT_FOOTSTEP = 0,
    SOUND_EVENT_SONAR_PULSE,
    SOUND_EVENT_DOOR_CLANG,
    SOUND_EVENT_HAZARD,
    SOUND_EVENT_COUNT   /* sentinel, not a valid type */
} SoundEventType;

/* ------------------------------------------------------------------ */
/*  Per-event defaults (also usable from game.c)                       */
/* ------------------------------------------------------------------ */

#define SOUND_FOOTSTEP_RADIUS     80.0f
#define SOUND_FOOTSTEP_INTENSITY   0.30f
#define SOUND_FOOTSTEP_LIFETIME    0.30f

#define SOUND_SONAR_RADIUS       400.0f
#define SOUND_SONAR_INTENSITY      1.0f
#define SOUND_SONAR_LIFETIME       2.0f

#define SOUND_DOOR_CLANG_RADIUS  250.0f
#define SOUND_DOOR_CLANG_INTENSITY 0.80f
#define SOUND_DOOR_CLANG_LIFETIME  0.60f

#define SOUND_HAZARD_RADIUS      180.0f
#define SOUND_HAZARD_INTENSITY    0.50f
#define SOUND_HAZARD_LIFETIME     0.40f

/* ------------------------------------------------------------------ */
/*  Core data types                                                    */
/* ------------------------------------------------------------------ */

typedef struct SoundEvent {
    SoundEventType type;
    float          x, y;         /* world-space origin */
    float          radius;       /* max propagation distance (px) */
    float          intensity;    /* 0..1 – loudness / noticeability */
    float          lifetime;     /* total seconds this event lingers */
    float          elapsed;      /* seconds since emission */
    bool           active;
} SoundEvent;

#define ECHO_MAX_SOUND_EVENTS  16

typedef struct SoundPropagation {
    SoundEvent events[ECHO_MAX_SOUND_EVENTS];
    int        count;
} SoundPropagation;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/* Initialise the system (zero all events). */
void SoundPropInit(SoundPropagation *sp);

/* Emit a sound event.  Returns false if the event buffer is full. */
bool SoundPropEmit(SoundPropagation *sp, SoundEventType type,
                   float x, float y, float radius,
                   float intensity, float lifetime);

/* Step all active events (advance elapsed, deactivate expired ones)
 * and compact the list.  Call once per frame. */
void SoundPropUpdate(SoundPropagation *sp, float deltaTime);

/* Query: find active sound events whose propagation spheres
 * intersect the query circle centred at (qx, qy) with the given
 * queryRadius.  Matching events are written into outEvents[] (up to
 * maxOut).  Returns the number written. */
int SoundPropQuery(const SoundPropagation *sp,
                   float qx, float qy, float queryRadius,
                   SoundEvent *outEvents, int maxOut);

#endif /* ECHO_SOUND_PROP_H */
