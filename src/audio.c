/*
 * Ambient Audio System — procedural atmosphere generator.
 *
 * All sounds are synthesised at runtime by manually filling sample
 * buffers with sine, noise, and square-wave data.  No disk assets.
 * The system is designed around silence — sounds are sparse, quiet,
 * and atmospheric.
 *
 * Raylib 5.5 removed the GenWave* family of helpers from the public
 * API, so we build the Wave structs ourselves.
 *
 * See include/audio.h for the full design rationale.
 */

#include "audio.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Manual wave generation helpers                                     */
/* (raylib 5.5 removed GenWaveSine / GenWaveNoise / GenWaveSquare,     */
/*  so we build 16-bit mono sample buffers with standard C maths.)    */
/* ------------------------------------------------------------------ */

/* Allocate and fill a 16-bit sine wave buffer. */
static Wave MakeSineWave(unsigned int samples, float freqHz)
{
    short *data = (short *)malloc(samples * sizeof(short));
    if (data == NULL) return (Wave){ 0 };

    for (unsigned int i = 0; i < samples; i++)
    {
        float t = (float)i / (float)AUDIO_SAMPLE_RATE;
        data[i] = (short)(sinf(2.0f * 3.14159265f * freqHz * t) * 32767.0f);
    }

    Wave w = { samples, AUDIO_SAMPLE_RATE, 16, 1, data };
    return w;
}

/* Allocate and fill a 16-bit white-noise buffer. */
static Wave MakeNoiseWave(unsigned int samples)
{
    short *data = (short *)malloc(samples * sizeof(short));
    if (data == NULL) return (Wave){ 0 };

    for (unsigned int i = 0; i < samples; i++)
        data[i] = (short)GetRandomValue(-32767, 32767);

    Wave w = { samples, AUDIO_SAMPLE_RATE, 16, 1, data };
    return w;
}

/* Allocate and fill a 16-bit square wave buffer (duty 0..1). */
static Wave MakeSquareWave(unsigned int samples, float freqHz, float duty)
{
    short *data = (short *)malloc(samples * sizeof(short));
    if (data == NULL) return (Wave){ 0 };

    float halfPeriod = 1.0f / (freqHz * 2.0f);
    for (unsigned int i = 0; i < samples; i++)
    {
        float t = (float)i / (float)AUDIO_SAMPLE_RATE;
        /* 0.5 duty = true square; < 0.5 = narrow pulse. */
        data[i] = (fmodf(t, halfPeriod * 2.0f) < halfPeriod * duty * 2.0f)
                  ? 32767 : -32767;
    }

    Wave w = { samples, AUDIO_SAMPLE_RATE, 16, 1, data };
    return w;
}

/* Apply a linear fade-out envelope to the tail of a wave. */
static void ApplyFadeOut(Wave *w, float fadeSec)
{
    if (w == NULL || w->data == NULL || fadeSec <= 0.0f) return;

    unsigned int fadeSamples = (unsigned int)((float)w->sampleRate * fadeSec);
    if (fadeSamples > w->frameCount) fadeSamples = w->frameCount;

    short *data  = (short *)w->data;
    unsigned int start = w->frameCount - fadeSamples;

    for (unsigned int i = 0; i < fadeSamples; i++)
    {
        float t = (float)i / (float)fadeSamples;
        data[start + i] = (short)((float)data[start + i] * (1.0f - t));
    }
}

/* ------------------------------------------------------------------ */
/*  Sound-from-wave helper                                             */
/* ------------------------------------------------------------------ */

static Sound MakeSound(Wave w, float volume)
{
    if (w.data == NULL) return (Sound){ 0 };

    Sound s = LoadSoundFromWave(w);
    /* LoadSoundFromWave copies the data into the audio device's
     * internal buffers, so we can free our malloc'd buffer now. */
    free(w.data);
    SetSoundVolume(s, volume);
    return s;
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */

void AmbientAudioInit(AmbientAudio *ambient)
{
    if (ambient == NULL) return;
    memset(ambient, 0, sizeof(*ambient));

    InitAudioDevice();

    unsigned int loopLen = (unsigned int)((float)AUDIO_SAMPLE_RATE * 2.0f); /* 2 s */

    /* Continuous ambient: hum (50 Hz sine) + static (white noise).
     * Start at volume 0 — silence fades in over 3 s. */
    {
        Wave w = MakeSineWave(loopLen, 50.0f);
        ambient->hum = MakeSound(w, 0.0f);
        PlaySound(ambient->hum);
    }
    {
        Wave w = MakeNoiseWave(loopLen);
        ambient->staticNoise = MakeSound(w, 0.0f);
        PlaySound(ambient->staticNoise);
    }

    /* Footstep — 600 Hz sine, 60 ms, with fast fade-out. */
    {
        Wave w = MakeSineWave((unsigned int)((float)AUDIO_SAMPLE_RATE * 0.06f), 600.0f);
        ApplyFadeOut(&w, 0.03f);
        ambient->footstep = MakeSound(w, 0.04f);
    }

    /* Heartbeat thump — 40 Hz sine, 120 ms, with gentle fade-out. */
    {
        Wave w = MakeSineWave((unsigned int)((float)AUDIO_SAMPLE_RATE * 0.12f), 40.0f);
        ApplyFadeOut(&w, 0.06f);
        ambient->heartbeatThump = MakeSound(w, 0.03f);
    }

    /* CRT interference — 150 ms noise burst with fade-out. */
    {
        Wave w = MakeNoiseWave((unsigned int)((float)AUDIO_SAMPLE_RATE * 0.15f));
        ApplyFadeOut(&w, 0.08f);
        ambient->crtBurst = MakeSound(w, 0.05f);
    }

    /* Mechanical rumble — 1 s 60 Hz square wave, 0.7 duty, fade-out. */
    {
        Wave w = MakeSquareWave((unsigned int)AUDIO_SAMPLE_RATE, 60.0f, 0.7f);
        ApplyFadeOut(&w, 0.30f);
        ambient->mechanicalRumble = MakeSound(w, 0.035f);
    }

    /* Panic breathing — 1.5 s modulated noise, very quiet.
     * We use a sine-modulated envelope to create a breathing rhythm. */
    {
        unsigned int breathSamples = (unsigned int)((float)AUDIO_SAMPLE_RATE * 1.5f);
        short *data = (short *)malloc(breathSamples * sizeof(short));
        if (data != NULL) {
            for (unsigned int i = 0; i < breathSamples; i++) {
                float t = (float)i / (float)AUDIO_SAMPLE_RATE;
                /* Modulate amplitude with slow sine — simulates inhale/exhale. */
                float envelope = sinf(3.14159265f * t / 1.5f);
                float noise = (float)GetRandomValue(-32767, 32767) / 32767.0f;
                data[i] = (short)(noise * envelope * 8000.0f);
            }
            Wave w = { breathSamples, AUDIO_SAMPLE_RATE, 16, 1, data };
            ApplyFadeOut(&w, 0.20f);
            ambient->breathing = MakeSound(w, 0.0f);  /* start silent */
        }
    }

    /* Phantom whisper — 0.5 s modulated noise with higher frequency bias
     * to create a sibilant, breathy whisper.  The envelope uses a faster
     * oscillation than breathing, creating a subtle "shhh" pattern. */
    {
        unsigned int whisperSamples = (unsigned int)((float)AUDIO_SAMPLE_RATE * 0.5f);
        short *data = (short *)malloc(whisperSamples * sizeof(short));
        if (data != NULL) {
            for (unsigned int i = 0; i < whisperSamples; i++) {
                float t = (float)i / (float)AUDIO_SAMPLE_RATE;
                /* Quick envelope: rapid attack, slower decay — like a gasp. */
                float attack  = fminf(t * 20.0f, 1.0f);
                float decay   = 1.0f - fmaxf((t - 0.15f) / 0.35f, 0.0f);
                float envelope = attack * decay;
                /* Band-passed noise: wrap noise with a 2kHz sine to give
                 * it a sibilant "shh" quality instead of raw static. */
                float noise = (float)GetRandomValue(-10000, 10000) / 10000.0f;
                float sibiliance = sinf(2.0f * 3.14159265f * 2000.0f * t) * 0.5f + 0.5f;
                data[i] = (short)(noise * sibiliance * envelope * 6000.0f);
            }
            Wave w = { whisperSamples, AUDIO_SAMPLE_RATE, 16, 1, data };
            ApplyFadeOut(&w, 0.15f);
            ambient->phantomWhisper = MakeSound(w, 0.0f);  /* start silent */
        }
    }

    /* --- State --- */
    ambient->silenceTimer      = 3.0f;   /* 3 s of pure silence */
    ambient->mechanicalTimer   = 8.0f + (float)GetRandomValue(0, 7000) / 1000.0f;
    ambient->heartbeatCooldown = 0.0f;
    ambient->heartbeatPlaying  = false;
    ambient->phantomWhisperCooldown = 0.0f;
    ambient->initialized       = true;
}

/* ------------------------------------------------------------------ */
/*  Update                                                            */
/* ------------------------------------------------------------------ */

void AmbientAudioUpdate(AmbientAudio *ambient, float deltaTime,
                        float nearestEnemyDist, float footstepTrigger,
                        bool sonarTriggered,
                        bool relayActivated,
                        float phantomProximity)
{
    if (ambient == NULL || !ambient->initialized) return;

    /* Difficulty scaling: after relay activation, heartbeat is faster
     * and machinery is louder for heightened tension. */
    float diffMultiplier = relayActivated ? 0.70f : 1.0f;

    /* --- Re-trigger \"looping\" sounds when they finish (SetSoundLooping
     * was removed in raylib 5.5).  These are 2s waves; once they stop
     * we re-launch them from the beginning. --- */
    if (!IsSoundPlaying(ambient->hum))
        PlaySound(ambient->hum);
    if (!IsSoundPlaying(ambient->staticNoise))
        PlaySound(ambient->staticNoise);

    /* --- Silence fade-in: 3s of pure silence, then hum/static ramp up. --- */
    if (ambient->silenceTimer > 0.0f)
    {
        ambient->silenceTimer -= deltaTime;
        if (ambient->silenceTimer <= 0.0f)
        {
            ambient->silenceTimer = 0.0f;
            SetSoundVolume(ambient->hum,          0.020f);
            SetSoundVolume(ambient->staticNoise,  0.008f);
        }
        else
        {
            float t = 1.0f - ambient->silenceTimer / 3.0f;
            SetSoundVolume(ambient->hum,          0.020f * t);
            SetSoundVolume(ambient->staticNoise,  0.008f * t);
        }
    }

    /* --- Footstep (with random pitch variation ±10%) --- */
    if (footstepTrigger > 0.0f)
    {
        if (!IsSoundPlaying(ambient->footstep))
        {
            SetSoundVolume(ambient->footstep, 0.04f);
            float pitchVar = 1.0f + (float)GetRandomValue(-10, 10) / 100.0f;
            SetSoundPitch(ambient->footstep, pitchVar);
            PlaySound(ambient->footstep);
        }
    }

    /* --- Heartbeat thump (triggered by nearby enemies) --- */
    {
        ambient->heartbeatCooldown -= deltaTime;
        if (ambient->heartbeatCooldown < 0.0f)
            ambient->heartbeatCooldown = 0.0f;

        float heartIntensity = (nearestEnemyDist < 220.0f)
                             ? (1.0f - nearestEnemyDist / 220.0f)
                             : 0.0f;

        if (heartIntensity > 0.01f && ambient->heartbeatCooldown <= 0.0f)
        {
            float vol = 0.015f + heartIntensity * 0.035f;
            if (!IsSoundPlaying(ambient->heartbeatThump))
            {
                SetSoundVolume(ambient->heartbeatThump, vol);
                PlaySound(ambient->heartbeatThump);
            }
            /* Heartbeat varies with proximity: 0.5s (very close) to 0.8s (far).
             * Closer = faster, more urgent. */
            /* After relay activation, heartbeat is faster (multiplied by diffMultiplier). */
            ambient->heartbeatCooldown = (0.50f + (1.0f - heartIntensity) * 0.30f) * diffMultiplier;
        }
    }

    /* --- CRT interference burst on sonar ping --- */
    if (sonarTriggered)
    {
        if (!IsSoundPlaying(ambient->crtBurst))
        {
            SetSoundVolume(ambient->crtBurst, 0.05f);
            PlaySound(ambient->crtBurst);
        }
    }

    /* --- Panic breathing: triggered when Hunter is very close --- */
    {
        float breathIntensity = (nearestEnemyDist < 180.0f)
                              ? (1.0f - nearestEnemyDist / 180.0f)
                              : 0.0f;
        if (breathIntensity > 0.01f) {
            if (!IsSoundPlaying(ambient->breathing)) {
                SetSoundVolume(ambient->breathing, 0.008f + breathIntensity * 0.020f);
                SetSoundPitch(ambient->breathing, 0.9f + breathIntensity * 0.2f);  /* faster = higher pitch */
                PlaySound(ambient->breathing);
            }
        }
    }

    /* --- Phantom whisper: triggered when Phantom is nearby --- */
    {
        ambient->phantomWhisperCooldown -= deltaTime;
        if (ambient->phantomWhisperCooldown < 0.0f)
            ambient->phantomWhisperCooldown = 0.0f;

        if (phantomProximity > 0.01f && ambient->phantomWhisperCooldown <= 0.0f)
        {
            if (!IsSoundPlaying(ambient->phantomWhisper))
            {
                float vol = 0.006f + phantomProximity * 0.012f;
                SetSoundVolume(ambient->phantomWhisper, vol);
                float pitchVar = 0.95f + (float)GetRandomValue(-5, 5) / 100.0f;
                SetSoundPitch(ambient->phantomWhisper, pitchVar);
                PlaySound(ambient->phantomWhisper);
            }
            /* Re-trigger every 1.5-3s while phantom is nearby. */
            ambient->phantomWhisperCooldown = 1.5f + (float)GetRandomValue(0, 15) / 10.0f;
        }
    }

    /* --- Adaptive hum: electrical hum changes with danger level --- */
    {
        float humIntensity = (nearestEnemyDist < 350.0f)
                           ? (1.0f - nearestEnemyDist / 350.0f)
                           : 0.0f;
        float baseHum = relayActivated ? 0.025f : 0.020f;
        float humVol = baseHum + humIntensity * 0.015f;
        SetSoundVolume(ambient->hum, humVol);
        SetSoundPitch(ambient->hum, 1.0f + humIntensity * 0.08f);
        SetSoundPitch(ambient->staticNoise, 1.0f + humIntensity * 0.05f);
    }

    /* --- Distant mechanical sounds (random interval) --- */
    {
        ambient->mechanicalTimer -= deltaTime;
        if (ambient->mechanicalTimer <= 0.0f)
        {
            if (!IsSoundPlaying(ambient->mechanicalRumble))
            {
                /* Louder after relay activation. */
                float baseVol = relayActivated ? 0.050f : 0.025f;
                float vol = baseVol + (float)GetRandomValue(0, 30) / 1000.0f;
                SetSoundVolume(ambient->mechanicalRumble, vol);
                PlaySound(ambient->mechanicalRumble);
            }
            /* Next rumble in 8–18 seconds. Slightly faster after relay. */
            float interval = relayActivated ? 6.0f : 8.0f;
            ambient->mechanicalTimer = interval
                                     + (float)GetRandomValue(0, 10000) / 1000.0f;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Shutdown                                                          */
/* ------------------------------------------------------------------ */

void AmbientAudioShutdown(AmbientAudio *ambient)
{
    if (ambient == NULL || !ambient->initialized) return;

    StopSound(ambient->hum);
    StopSound(ambient->staticNoise);

    UnloadSound(ambient->hum);
    UnloadSound(ambient->staticNoise);
    UnloadSound(ambient->footstep);
    UnloadSound(ambient->heartbeatThump);
    UnloadSound(ambient->crtBurst);
    UnloadSound(ambient->mechanicalRumble);
    UnloadSound(ambient->breathing);
    UnloadSound(ambient->phantomWhisper);

    CloseAudioDevice();
    memset(ambient, 0, sizeof(*ambient));
}
