/*
 * Renderer module implementation.
 * See include/renderer.h for the darkness pipeline overview.
 */

#include "renderer.h"
#include "raylib.h"
#include <stddef.h>

static const char *DARKNESS_SHADER_PATH = "assets/shaders/darkness.fs";

void RendererInit(Renderer *renderer, int screenWidth, int screenHeight)
{
    if (renderer == NULL) {
        return;
    }

    renderer->screenWidth  = screenWidth;
    renderer->screenHeight = screenHeight;

    renderer->sceneTexture = LoadRenderTexture(screenWidth, screenHeight);
    SetTextureFilter(renderer->sceneTexture.texture, TEXTURE_FILTER_BILINEAR);

    renderer->darknessShader = LoadShader(NULL, DARKNESS_SHADER_PATH);

    renderer->locResolution       = GetShaderLocation(renderer->darknessShader, "resolution");
    renderer->locPlayerPos        = GetShaderLocation(renderer->darknessShader, "playerPos");
    renderer->locVisibilityRadius = GetShaderLocation(renderer->darknessShader, "visibilityRadius");
    renderer->locTime             = GetShaderLocation(renderer->darknessShader, "time");
    renderer->locSonarPulseCount  = GetShaderLocation(renderer->darknessShader, "sonarPulseCount");
    renderer->locSonarPulses      = GetShaderLocation(renderer->darknessShader, "sonarPulses");
    renderer->locSonarFlash       = GetShaderLocation(renderer->darknessShader, "sonarFlash");

    float resolution[2] = { (float)screenWidth, (float)screenHeight };
    SetShaderValue(renderer->darknessShader, renderer->locResolution, resolution, SHADER_UNIFORM_VEC2);

    renderer->sonarPulseCount = 0;
    renderer->sonarFlash      = 0.0f;
}

void RendererBeginWorld(const Renderer *renderer)
{
    BeginTextureMode(renderer->sceneTexture);
    ClearBackground(BLACK);
}

void RendererEndWorld(const Renderer *renderer)
{
    (void)renderer;
    EndTextureMode();
}

void RendererDrawDarkness(Renderer *renderer, Vector2 playerScreenPos,
                           float visibilityRadius, float time)
{
    if (renderer == NULL) {
        return;
    }

    float playerPos[2] = { playerScreenPos.x, playerScreenPos.y };
    SetShaderValue(renderer->darknessShader, renderer->locPlayerPos, playerPos, SHADER_UNIFORM_VEC2);
    SetShaderValue(renderer->darknessShader, renderer->locVisibilityRadius, &visibilityRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(renderer->darknessShader, renderer->locTime, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(renderer->darknessShader, renderer->locSonarPulseCount, &renderer->sonarPulseCount, SHADER_UNIFORM_INT);
    SetShaderValue(renderer->darknessShader, renderer->locSonarFlash, &renderer->sonarFlash, SHADER_UNIFORM_FLOAT);

    if (renderer->sonarPulseCount > 0) {
        SetShaderValueV(renderer->darknessShader, renderer->locSonarPulses,
                         renderer->sonarPulses, SHADER_UNIFORM_VEC4, renderer->sonarPulseCount);
    }

    /* The scene texture is upside-down relative to the screen (render
     * textures are Y-flipped), so flip it back here on the way out. */
    Rectangle sourceRect = {
        0.0f, 0.0f,
        (float)renderer->sceneTexture.texture.width,
        -(float)renderer->sceneTexture.texture.height
    };

    BeginShaderMode(renderer->darknessShader);
        DrawTextureRec(renderer->sceneTexture.texture, sourceRect, (Vector2){ 0.0f, 0.0f }, WHITE);
    EndShaderMode();
}

void RendererSetSonarPulses(Renderer *renderer, const SonarRevealPulse *pulses, int count)
{
    if (renderer == NULL) {
        return;
    }

    if (count < 0) {
        count = 0;
    }
    if (count > ECHO_MAX_SONAR_REVEAL_PULSES) {
        count = ECHO_MAX_SONAR_REVEAL_PULSES;
    }

    for (int i = 0; i < count; i++) {
        renderer->sonarPulses[i] = pulses[i];
    }
    renderer->sonarPulseCount = count;
}

void RendererShutdown(Renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }

    UnloadShader(renderer->darknessShader);
    UnloadRenderTexture(renderer->sceneTexture);
}
