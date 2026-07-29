#ifndef ECHO_AUDIO_H
#define ECHO_AUDIO_H

#include <stdbool.h>
#include "raylib.h"

/*
 * Ambient Audio System — procedural atmosphere generator.
 *
 * Every sound is generated at runtime using raylib's wave synthesis
 * (GenWaveSine, GenWaveNoise, GenWaveSquare).  No audio files are
 * loaded from disk.  The system uses sound to build atmosphere:
 *
 *   Electrical hum    — continuous 50 Hz sine, barely audible
 *   Static            — continuous white noise, the silence itself hums
 *   Metal footsteps   — short square-wave click, triggered by movement
 *   Heartbeat thump   — low-frequency sine thump, triggered by enemy proximity
 *   CRT interference  — noise burst, triggered on sonar pulse
 *   Distant machinery — occasional low rumble at random intervals
 *
 * The default state is silence.  Sounds are sparse and quiet — the
 * empty corridors should feel oppressive even when nothing is playing.
 */

/* ------------------------------------------------------------------ */
/*  Ambient Audio System                                              */
/* ------------------------------------------------------------------ */

#define AUDIO_SAMPLE_RATE   11025

typedef struct AmbientAudio {
    /* Continuous looping sounds. */
    Sound hum;              /* electrical hum — always playing while alive */
    Sound staticNoise;      /* white noise — always playing while alive */

    /* One-shot sounds (re-generated on each trigger). */
    Sound footstep;         /* metal footstep click */
    Sound heartbeatThump;   /* low heartbeat thump */
    Sound crtBurst;         /* CRT static burst on sonar */
    Sound mechanicalRumble; /* distant machinery rumble */

    /* Panic breathing — low modulated noise when Hunter is close. */
    Sound breathing;

    /* State. */
    bool   initialized;
    float  silenceTimer;      /* 3s countdown — no audio until this expires */
    float  mechanicalTimer;   /* countdown to next distant mechanical sound */
    float  heartbeatCooldown; /* prevent heartbeat overlapping itself */
    bool   heartbeatPlaying;
} AmbientAudio;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/* Initialise the audio device and generate all ambient sounds.
 * Must be called after InitAudioDevice(). */
void AmbientAudioInit(AmbientAudio *ambient);

/* Update: tick the mechanical timer, manage sound state. */
void AmbientAudioUpdate(AmbientAudio *ambient, float deltaTime,
                        float nearestEnemyDist, float footstepTrigger,
                        bool sonarTriggered,
                        bool relayActivated);

/* Shutdown: unload all generated sounds and close the audio context. */
void AmbientAudioShutdown(AmbientAudio *ambient);

#endif /* ECHO_AUDIO_H */
