#ifndef ECHO_RENDERER_H
#define ECHO_RENDERER_H

#include "raylib.h"

/*
 * Renderer module.
 *
 * Owns the darkness rendering pipeline: the world is drawn into an
 * off-screen render texture, then composited to the screen through a
 * fragment shader (assets/shaders/darkness.fs) that crushes it to
 * near-black, keeps a faint personal-visibility bubble around the
 * player, and adds a subtle vignette + light CRT noise.
 *
 * Sonar reveal pulses are sent to the shader each frame via
 * RendererSetSonarPulses(). The sonarFlash uniform creates a brief
 * screen-wide brighten on pulse emit for immediate feedback.
 */

typedef struct SonarRevealPulse {
    float x;         /* screen-space pixels */
    float y;         /* screen-space pixels */
    float radius;    /* screen-space pixels */
    float strength;  /* 0..1, how much this pulse brightens what it reveals */
} SonarRevealPulse;

#define ECHO_MAX_SONAR_REVEAL_PULSES 8

typedef struct Renderer {
    int screenWidth;
    int screenHeight;

    RenderTexture2D sceneTexture;   /* world is drawn here first */
    Shader          darknessShader; /* composites sceneTexture -> screen */

    /* Cached uniform locations for darknessShader. */
    int locResolution;
    int locPlayerPos;
    int locVisibilityRadius;
    int locTime;
    int locSonarPulseCount;
    int locSonarPulses;
    int locSonarFlash;

    /* Sonar reveal pulses sent to the shader every frame. */
    SonarRevealPulse sonarPulses[ECHO_MAX_SONAR_REVEAL_PULSES];
    int              sonarPulseCount;

    /* Screen-wide flash intensity (0..1) when a pulse is emitted.
     * Decays to 0 over ~0.15s.  Written by GameUpdate, read by
     * RendererDrawDarkness. */
    float sonarFlash;
} Renderer;

void RendererInit(Renderer *renderer, int screenWidth, int screenHeight);

/* Call before drawing world-space content (map, entities, player).
 * Redirects drawing into the off-screen scene texture. */
void RendererBeginWorld(const Renderer *renderer);

/* Call after all world-space content has been drawn for this frame. */
void RendererEndWorld(const Renderer *renderer);

/* Composites the scene texture to the currently active render target
 * (the screen) through the darkness shader. playerScreenPos is the
 * player's position in screen-space pixels (e.g. from
 * GetWorldToScreen2D), visibilityRadius is how far the faint personal
 * glow reaches, and time should be a monotonically increasing seconds
 * value used to animate the noise. */
void RendererDrawDarkness(Renderer *renderer, Vector2 playerScreenPos,
                           float visibilityRadius, float time);

/* Push up to ECHO_MAX_SONAR_REVEAL_PULSES reveal circles into
 * the darkness shader. Called every frame from GameUpdate. */
void RendererSetSonarPulses(Renderer *renderer, const SonarRevealPulse *pulses, int count);

void RendererShutdown(Renderer *renderer);

#endif /* ECHO_RENDERER_H */
