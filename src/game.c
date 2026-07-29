/*
 * Echo Protocol - core game module.
 *
 * Core mechanic: the only way to see is by emitting a sonar pulse,
 * but every pulse reveals the player to enemies.
 *
 * Systems that reinforce this:
 *   - Sonar pulse (reveal the environment)
 *   - Darkness shader (hide everything by default)
 *   - Echo Memory (fading traces of where you've been)
 *   - Sound propagation (footsteps + sonar pings attract enemies)
 *   - Watcher + Hunter enemies (sonar pulses attract Hunters)
 *   - Heartbeat (tension when enemies are close)
 *   - Fade transitions (smooth state changes)
 */

#include "game.h"
#include "easing.h"
#include "raylib.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define ECHO_PLAYER_VISIBILITY_RADIUS 90.0f

/* --- #17: Procedural Facility Names --- */
static const char *FACILITY_NAMES[] = {
    "ORPHEUS", "EREBUS", "NEMESIS", "VEGA", "TITAN",
    "IXION", "HADES", "LUX", "UMBRA", "SOL",
    "NOX", "LYNX", "AURORA", "HELIX", "KARMA"
};
#define FACILITY_NAME_COUNT ((int)(sizeof(FACILITY_NAMES) / sizeof(FACILITY_NAMES[0])))

/* --- #11: Station Motto --- */
static const char *STATION_MOTTOS[] = {
    "\"We Listen.\"", "\"No Signal Returns.\"", "\"Beneath the Static.\"",
    "\"Echoes Never Fade.\"", "\"Last Light.\"", "\"The Dark Remembers.\"",
    "\"Silence is Survival.\"", "\"Do Not Answer.\"", "\"Listen Closely.\"",
    "\"The Station Remembers.\""
};
#define STATION_MOTTO_COUNT ((int)(sizeof(STATION_MOTTOS) / sizeof(STATION_MOTTOS[0])))

/* --- #11: Announcement word pools --- */
static const char *ANNOUNCE_WORDS_A[] = {
    "Attention", "Warning", "Alert", "Notice", "System"
};
#define ANNOUNCE_WORDS_A_COUNT ((int)(sizeof(ANNOUNCE_WORDS_A) / sizeof(ANNOUNCE_WORDS_A[0])))
static const char *ANNOUNCE_WORDS_B[] = {
    "Sector", "Relay", "Power", "Hull", "Life", "Comms"
};
#define ANNOUNCE_WORDS_B_COUNT ((int)(sizeof(ANNOUNCE_WORDS_B) / sizeof(ANNOUNCE_WORDS_B[0])))
static const char *ANNOUNCE_WORDS_C[] = {
    "offline", "failed", "critical", "lost", "breach"
};
#define ANNOUNCE_WORDS_C_COUNT ((int)(sizeof(ANNOUNCE_WORDS_C) / sizeof(ANNOUNCE_WORDS_C[0])))

/* --- #18: Procedural Logs (lore fragments) --- */
static const char *LORE_LOGS[] = {
    "DAY 17 — We heard it again.",
    "DO NOT PING. It learns.",
    "Relay failure. Sector 4 lost.",
    "Do not answer the echoes.",
    "The dark moves.",
    "Last contact: 72 hours ago.",
    "It mimics our signals.",
    "Echo Protocol — last resort.",
    "They follow the light.",
    "Whoever reads this — run."
};
#define LORE_LOG_COUNT ((int)(sizeof(LORE_LOGS) / sizeof(LORE_LOGS[0])))

static void UpdateFollowCamera(Game *game)
{
    /* Base follow (centered on player). */
    game->camera.target = game->player.position;
    game->camera.offset = (Vector2){
        ECHO_WINDOW_WIDTH / 2.0f,
        ECHO_WINDOW_HEIGHT / 2.0f
    };
    game->camera.rotation = 0.0f;

    /* --- Camera zoom spring --- */
    {
        float springSpeed = 8.0f;
        game->camera.zoom += (game->cameraZoomTarget - game->camera.zoom)
                             * springSpeed * game->deltaTime;
        /* Decay zoom target back to 1.0. */
        if (game->cameraZoomTimer > 0.0f) {
            game->cameraZoomTimer -= game->deltaTime;
            if (game->cameraZoomTimer <= 0.0f) {
                game->cameraZoomTarget = 1.0f;
                game->cameraZoomTimer  = 0.0f;
            }
        }
        if (game->camera.zoom < 0.99f) game->camera.zoom = 1.0f;  // prevent drift
    }

    /* --- Screen shake offset --- */
    if (game->shakeTimer > 0.0f) {
        float intensity = game->shakeIntensity
                        * EaseOutCubic(game->shakeTimer / 0.12f);
        float sx = (float)GetRandomValue(-10000, 10000) / 10000.0f * intensity;
        float sy = (float)GetRandomValue(-10000, 10000) / 10000.0f * intensity;
        game->camera.offset.x += sx;
        game->camera.offset.y += sy;
        game->shakeTimer -= game->deltaTime;
        if (game->shakeTimer < 0.0f) game->shakeTimer = 0.0f;
    }
}

void GameInit(Game *game)
{
    if (game == NULL) return;

    game->state       = GAME_STATE_BOOT;
    game->isRunning   = true;
    game->elapsedTime = 0.0f;
    game->deltaTime   = 0.0f;

    PlayerInit(&game->player);
    RendererInit(&game->renderer, ECHO_WINDOW_WIDTH, ECHO_WINDOW_HEIGHT);
    AmbientAudioInit(&game->ambient);
    SonarInit(&game->sonar);
    EchoMemoryInit(&game->memory);
    SoundPropInit(&game->soundProp);
    game->footstepTimer    = 0.0f;
    game->shakeTimer       = 0.0f;
    game->shakeIntensity   = 5.0f;
    game->cameraZoomTimer  = 0.0f;
    game->cameraZoomTarget = 1.0f;
    game->fadeAlpha        = 1.0f;
    game->fadeTarget       = 0.0f;
    game->deathFlashTimer  = 0.0f;
    game->deathSlowMo      = false;
    game->airlockCountdown = 0.0f;
    game->airlockSequence  = false;
    game->relaysActivated        = 0;
    game->relayActivationProgress = 0.0f;
    game->alertsTriggered         = 0;
    game->escapeWhiteTimer        = 0.0f;
    game->escapeDoorOpen          = false;
    game->titleTimer              = 4.5f;
    game->relayGlowTimer          = 0.0f;
    game->flickerTimer            = 5.0f / fmaxf(game->ageGlitchMult * 0.5f + 0.5f, 0.5f);
    game->flickerType             = 0;
    game->flickerDuration         = 0.0f;
    game->pingIntervalCount       = 0;
    game->lastPingTime            = 0.0f;
    memset(game->pingIntervals, 0, sizeof(game->pingIntervals));
    game->sonarSaturation         = 0.0f;
    game->hunterMimicTimer        = 0.0f;
    game->nearMissTimer           = 0.0f;
    game->wasInCorridor           = false;
    memset(game->lastPingPositionsX, 0, sizeof(game->lastPingPositionsX));
    memset(game->lastPingPositionsY, 0, sizeof(game->lastPingPositionsY));
    game->predictedPingX          = 0.0f;
    game->predictedPingY          = 0.0f;
    game->ghostTimer              = 0.0f;
    game->ghostReplayX            = 0.0f;
    game->ghostReplayY            = 0.0f;
    game->falseRelayRoomIdx       = -1;
    game->falseRelayTimer         = 0.0f;
    game->hazardSoundTimer        = 3.0f;
    game->announcementTimer       = 8.0f;
    game->announcementDisplay     = 0.0f;
    game->announcementText[0]     = '\0';
    game->endingTension           = 0;
    game->facilityNameIdx         = 0;
    game->logRoomIdx              = -1;
    game->logDisplayTimer         = 0.0f;
    game->logText[0]              = '\0';
    game->attemptCount            = 1;
    strcpy(game->lastOutcome, "-");
    game->terminalMottoIdx         = 0;
    game->fakeFootstepTimer        = 0.0f;
    game->titleSubtitleIdx         = 0;
    game->impossibleRoomIdx        = -1;
    game->impossibleRoomRevealTimer = 0.0f;
    game->brokenSonarTimer          = 0.0f;
    game->wetRoomCount              = 0;
    game->rustyRoomCount            = 0;
    memset(game->wetRoomIdxs, 0, sizeof(game->wetRoomIdxs));
    memset(game->rustyRoomIdxs, 0, sizeof(game->rustyRoomIdxs));
    game->shadowTimer               = 15.0f;
    game->shadowX                   = 0.0f;
    game->shadowY                   = 0.0f;
    game->falseHeartbeatTimer       = 20.0f;
    game->radioTimer                = 25.0f;
    game->radioDisplay              = 0.0f;
    game->radioText[0]              = '\0';
    game->currentRoomIdx            = -1;
    game->roomNameTimer             = 0.0f;
    game->roomName[0]               = '\0';
    game->stationAge                = 0;
    game->ageGlitchMult             = 1.0f;
    game->hiddenRoomIdx             = -1;
    game->hiddenRevealTimer         = 0.0f;
    game->echoDelayTimer            = 0.0f;
    game->lastPingRoomIdx           = -1;
    game->emergencyLightTimer       = 30.0f;
    game->exitMistTimer             = 0.0f;
    game->reactorPulseTimer         = 12.0f;
    game->monitorRoomIdx            = -1;
    game->crtBurnTimer              = 0.0f;
    game->cableSparkTimer           = 8.0f;
    game->cableSparkX               = 0.0f;
    game->cableSparkY               = 0.0f;
    game->powerDrainTimer           = 0.0f;
    game->finalBlackTimer           = 0.0f;
    game->anomalySeed               = false;
    memset(game->playerTrailX, 0, sizeof(game->playerTrailX));
    memset(game->playerTrailY, 0, sizeof(game->playerTrailY));
    game->playerTrailCount = 0;
    game->alertFlashTimer  = 0.0f;
    game->alertFlashX      = 0.0f;
    game->alertFlashY      = 0.0f;
    memset(game->playerDustX, 0, sizeof(game->playerDustX));
    memset(game->playerDustY, 0, sizeof(game->playerDustY));
    memset(game->playerDustLife, 0, sizeof(game->playerDustLife));
    /* Generate the procedural station. */
    game->stationSeed = (uint64_t)GetRandomValue(1, 999999999);
    StationGenerate(&game->station, game->stationSeed);

    /* Pick terminal motto (#5) from seed. */
    game->terminalMottoIdx = (int)(game->stationSeed % 99999);

    /* #15: Impossible Room — 0.2% chance one room gets the easter egg label. */
    if (game->station.roomCount > 2 && GetRandomValue(0, 499) == 0) {
        int candidate = GetRandomValue(1, game->station.roomCount - 1);
        if (candidate != game->station.objectiveRoomIdx &&
            candidate != game->station.airlockRoomIdx &&
            candidate != game->station.relayRoomIdxs[0]) {
            game->impossibleRoomIdx = candidate;
        }
    }

    /* #19: Station Age — derived from seed, affects glitch frequency. */
    game->stationAge = (int)(game->stationSeed % 100) + 1;
    game->ageGlitchMult = 1.0f + (float)(game->stationAge / 100.0f);

    /* #10/#15: Flag 0-2 rooms as wet or rusty for louder footsteps. */
    for (int w = 0; w < 2 && w < game->station.roomCount - 1; w++) {
        if (GetRandomValue(0, 1)) {
            int ri = GetRandomValue(1, game->station.roomCount - 1);
            if (ri != game->station.startRoomIdx) {
                game->wetRoomIdxs[game->wetRoomCount++] = ri;
            }
        }
    }
    for (int r = 0; r < 2 && r < game->station.roomCount - 1; r++) {
        if (GetRandomValue(0, 1)) {
            int ri = GetRandomValue(1, game->station.roomCount - 1);
            if (ri != game->station.startRoomIdx) {
                game->rustyRoomIdxs[game->rustyRoomCount++] = ri;
            }
        }
    }

    /* #22: Hidden Observation Room — 0.5% chance, different from impossible room. */
    if (game->station.roomCount > 3 && GetRandomValue(0, 199) == 0) {
        game->hiddenRoomIdx = GetRandomValue(1, game->station.roomCount - 1);
        /* Not the same room as impossibleRom. */
        if (game->hiddenRoomIdx == game->impossibleRoomIdx) game->hiddenRoomIdx = -1;
    }

    /* #15: Broken Monitor — pick a random room for ERROR flicker. */
    if (game->station.roomCount > 2) {
        int candidate = GetRandomValue(1, game->station.roomCount - 1);
        if (candidate != game->station.startRoomIdx) {
            game->monitorRoomIdx = candidate;
        }
    }

    /* #26: Hidden Seed Challenge — seed divisible by 777 unlocks ANOMALY mode. */
    game->anomalySeed = ((game->stationSeed % 777) == 0);

    /* #24: Procedural Callsign — pick a callsign index from seed. */
    /* Pick facility name (#17) from seed. */
    game->facilityNameIdx = (int)(game->stationSeed % (uint64_t)FACILITY_NAME_COUNT);

    /* Pick a random room for lore terminal (#18). */
    {
        game->logRoomIdx = -1;
        if (game->station.roomCount > 3) {
            int nonImportant = 0;
            for (int ri = 1; ri < game->station.roomCount; ri++) {
                if (ri != game->station.objectiveRoomIdx &&
                    ri != game->station.airlockRoomIdx &&
                    ri != game->station.relayRoomIdxs[0]) {
                    if (GetRandomValue(0, nonImportant) == 0) {
                        game->logRoomIdx = ri;
                    }
                    nonImportant++;
                }
            }
        }
    }

    /* Place the player in the starting room. */
    {
        float sx, sy;
        StationRoomCentre(&game->station, game->station.startRoomIdx, &sx, &sy);
        game->player.position.x = sx;
        game->player.position.y = sy;
    }

    /* Store airlock position for win condition. */
    StationRoomCentre(&game->station, game->station.airlockRoomIdx,
                      &game->airlockX, &game->airlockY);
    game->gameWon     = false;
    game->scansUsed   = 0;
    game->runTimeDisplay = 0.0f;

    /* Spawn enemies at station's designated positions. */
    EnemyManagerInit(&game->enemies);
    for (int ei = 0; ei < game->station.enemySpawnCount; ei++)
    {
        EnemyType type = (ei == 0) ? ENEMY_TYPE_HUNTER : ENEMY_TYPE_WATCHER;
        EnemyManagerAdd(&game->enemies, type,
                        game->station.enemySpawnX[ei],
                        game->station.enemySpawnY[ei]);
    }

    /* Guarantee the Hunter (enemy 0) starts near the player so it
     * inevitably hears the first post-grace ping.  Pick the room
     * closest to the start room. */
    if (game->enemies.count > 0 && game->station.roomCount > 1) {
        int nearestIdx = 1;
        float nearestDistSq = 999999.0f;
        float sx, sy;
        StationRoomCentre(&game->station, 0, &sx, &sy);
        for (int ri = 1; ri < game->station.roomCount; ri++) {
            float rx, ry;
            StationRoomCentre(&game->station, ri, &rx, &ry);
            float dx = rx - sx, dy = ry - sy;
            float d = dx * dx + dy * dy;
            if (d < nearestDistSq) { nearestDistSq = d; nearestIdx = ri; }
        }
        float hx, hy;
        StationRoomCentre(&game->station, nearestIdx, &hx, &hy);
        game->enemies.enemies[0].x = hx;
        game->enemies.enemies[0].y = hy;
    }

    game->camera.target = game->player.position;
    game->camera.offset = (Vector2){ ECHO_WINDOW_WIDTH / 2.0f, ECHO_WINDOW_HEIGHT / 2.0f };
    game->camera.rotation = 0.0f;
    game->camera.zoom     = 1.0f;

    /* --- Pacing timers --- */
    game->graceTimer    = 8.0f;   /* 8s before sonar alerts enemies */
    game->hudFadeTimer  = 8.0f;   /* tutorial text visible for 8s */
    game->firstPingDone = false;
    game->firstPingTime = 0.0f;

    /* Initialise persistent achievement system (loads saved data). */
    AchievementInit(&game->achievements);

    /* Reset run-specific tracking. */
    memset(game->runVisitedRooms, 0, sizeof(game->runVisitedRooms));
    game->runVisitedRoomCount  = 0;
    game->runHadNearMiss       = false;
    game->runHadResonance      = false;
    game->runHadEchoGhost      = false;
    game->runHadShadow         = false;
    game->runHadRadioBurst     = false;
    game->runHadLoreTerminal   = false;
    game->runHadFalseRelay     = false;
    game->runHadHunterMimic    = false;
    game->runEscapeAchieved    = false;

    /* Stay in GAME_STATE_BOOT for the title screen. */
    /* Transitions to PLAYING after titleTimer expires. */
}

void GameRestart(Game *game)
{
    if (game == NULL) return;

    /* Reset all gameplay state. The renderer and audio device are
     * kept alive — no need to reload shaders or re-init the mixer. */

    game->elapsedTime = 0.0f;
    game->deltaTime   = 0.0f;

    PlayerInit(&game->player);
    SonarInit(&game->sonar);
    EchoMemoryInit(&game->memory);
    SoundPropInit(&game->soundProp);

    game->footstepTimer    = 0.0f;
    game->shakeTimer       = 0.0f;
    game->shakeIntensity   = 5.0f;
    game->cameraZoomTimer  = 0.0f;
    game->cameraZoomTarget = 1.0f;
    game->fadeAlpha        = 0.0f;
    game->fadeTarget       = 0.0f;
    game->deathFlashTimer  = 0.0f;
    game->deathSlowMo      = false;
    game->airlockCountdown = 0.0f;
    game->airlockSequence  = false;
    game->relaysActivated        = 0;
    game->relayActivationProgress = 0.0f;
    game->alertsTriggered         = 0;
    game->escapeWhiteTimer        = 0.0f;
    game->escapeDoorOpen          = false;
    game->titleTimer              = 4.5f;
    game->titlePulseTimer         = 0.0f;
    game->relayGlowTimer          = 0.0f;
    game->sonarSaturation         = 0.0f;
    game->hunterMimicTimer        = 0.0f;
    game->nearMissTimer           = 0.0f;
    game->wasInCorridor           = false;
    memset(game->lastPingPositionsX, 0, sizeof(game->lastPingPositionsX));
    memset(game->lastPingPositionsY, 0, sizeof(game->lastPingPositionsY));
    game->predictedPingX          = 0.0f;
    game->predictedPingY          = 0.0f;
    game->ghostTimer              = 0.0f;
    game->ghostReplayX            = 0.0f;
    game->ghostReplayY            = 0.0f;
    game->falseRelayRoomIdx       = -1;
    game->falseRelayTimer         = 0.0f;
    game->hazardSoundTimer        = 3.0f;
    game->announcementTimer       = 8.0f;
    game->announcementDisplay     = 0.0f;
    game->announcementText[0]     = '\0';
    game->endingTension           = 0;
    game->facilityNameIdx         = 0;
    game->logRoomIdx              = -1;
    game->logDisplayTimer         = 0.0f;
    game->logText[0]              = '\0';
    game->attemptCount++;
    game->terminalMottoIdx         = 0;
    game->fakeFootstepTimer        = 0.0f;
    game->titleSubtitleIdx         = 0;
    game->impossibleRoomIdx        = -1;
    game->impossibleRoomRevealTimer = 0.0f;
    game->brokenSonarTimer          = 0.0f;
    game->wetRoomCount              = 0;
    game->rustyRoomCount            = 0;
    memset(game->wetRoomIdxs, 0, sizeof(game->wetRoomIdxs));
    memset(game->rustyRoomIdxs, 0, sizeof(game->rustyRoomIdxs));
    game->shadowTimer               = 15.0f;
    game->shadowX                   = 0.0f;
    game->shadowY                   = 0.0f;
    game->falseHeartbeatTimer       = 20.0f;
    game->radioTimer                = 25.0f;
    game->radioDisplay              = 0.0f;
    game->radioText[0]              = '\0';
    game->currentRoomIdx            = -1;
    game->roomNameTimer             = 0.0f;
    game->roomName[0]               = '\0';
    game->stationAge                = 0;
    game->ageGlitchMult             = 1.0f;
    game->hiddenRoomIdx             = -1;
    game->hiddenRevealTimer         = 0.0f;
    game->echoDelayTimer            = 0.0f;
    game->lastPingRoomIdx           = -1;
    game->emergencyLightTimer       = 30.0f;
    game->exitMistTimer             = 0.0f;
    game->reactorPulseTimer         = 12.0f;
    game->monitorRoomIdx            = -1;
    game->crtBurnTimer              = 0.0f;
    game->cableSparkTimer           = 8.0f;
    game->cableSparkX               = 0.0f;
    game->cableSparkY               = 0.0f;
    game->powerDrainTimer           = 0.0f;
    game->finalBlackTimer           = 0.0f;
    game->anomalySeed               = false;
    /* Generate a new station (new seed = new layout). */
    game->stationSeed = (uint64_t)GetRandomValue(1, 999999999);
    StationGenerate(&game->station, game->stationSeed);

    /* Pick terminal motto (#5) from seed. */
    game->terminalMottoIdx = (int)(game->stationSeed % 99999);

    /* #15: Impossible Room — 0.2% chance one room gets the easter egg label. */
    if (game->station.roomCount > 2 && GetRandomValue(0, 499) == 0) {
        int candidate = GetRandomValue(1, game->station.roomCount - 1);
        if (candidate != game->station.objectiveRoomIdx &&
            candidate != game->station.airlockRoomIdx &&
            candidate != game->station.relayRoomIdxs[0]) {
            game->impossibleRoomIdx = candidate;
        }
    }

    /* #19: Station Age — derived from seed, affects glitch frequency. */
    game->stationAge = (int)(game->stationSeed % 100) + 1;
    game->ageGlitchMult = 1.0f + (float)(game->stationAge / 100.0f);

    /* #10/#15: Flag 0-2 rooms as wet or rusty for louder footsteps. */
    for (int w = 0; w < 2 && w < game->station.roomCount - 1; w++) {
        if (GetRandomValue(0, 1)) {
            int ri = GetRandomValue(1, game->station.roomCount - 1);
            if (ri != game->station.startRoomIdx) {
                game->wetRoomIdxs[game->wetRoomCount++] = ri;
            }
        }
    }
    for (int r = 0; r < 2 && r < game->station.roomCount - 1; r++) {
        if (GetRandomValue(0, 1)) {
            int ri = GetRandomValue(1, game->station.roomCount - 1);
            if (ri != game->station.startRoomIdx) {
                game->rustyRoomIdxs[game->rustyRoomCount++] = ri;
            }
        }
    }

    /* #22: Hidden Observation Room — 0.5% chance, different from impossible room. */
    if (game->station.roomCount > 3 && GetRandomValue(0, 199) == 0) {
        game->hiddenRoomIdx = GetRandomValue(1, game->station.roomCount - 1);
        /* Not the same room as impossibleRom. */
        if (game->hiddenRoomIdx == game->impossibleRoomIdx) game->hiddenRoomIdx = -1;
    }

    /* #15: Broken Monitor — pick a random room for ERROR flicker. */
    if (game->station.roomCount > 2) {
        int candidate = GetRandomValue(1, game->station.roomCount - 1);
        if (candidate != game->station.startRoomIdx) {
            game->monitorRoomIdx = candidate;
        }
    }

    /* #26: Hidden Seed Challenge — seed divisible by 777 unlocks ANOMALY mode. */
    game->anomalySeed = ((game->stationSeed % 777) == 0);

    /* #24: Procedural Callsign — pick a callsign index from seed. */
    /* Pick facility name (#17) from seed. */
    game->facilityNameIdx = (int)(game->stationSeed % (uint64_t)FACILITY_NAME_COUNT);

    /* Pick a random room for lore terminal (#18). */
    {
        game->logRoomIdx = -1;
        if (game->station.roomCount > 3) {
            int nonImportant = 0;
            for (int ri = 1; ri < game->station.roomCount; ri++) {
                if (ri != game->station.objectiveRoomIdx &&
                    ri != game->station.airlockRoomIdx &&
                    ri != game->station.relayRoomIdxs[0]) {
                    if (GetRandomValue(0, nonImportant) == 0) {
                        game->logRoomIdx = ri;
                    }
                    nonImportant++;
                }
            }
        }
    }

    /* Place player in the new start room. */
    {
        float sx, sy;
        StationRoomCentre(&game->station, game->station.startRoomIdx, &sx, &sy);
        game->player.position.x = sx;
        game->player.position.y = sy;
    }

    /* Store new airlock position. */
    StationRoomCentre(&game->station, game->station.airlockRoomIdx,
                      &game->airlockX, &game->airlockY);
    game->gameWon     = false;
    game->scansUsed   = 0;
    game->runTimeDisplay = 0.0f;

    /* Spawn fresh enemies in the new layout. */
    EnemyManagerInit(&game->enemies);
    for (int ei = 0; ei < game->station.enemySpawnCount; ei++)
    {
        EnemyType type = (ei == 0) ? ENEMY_TYPE_HUNTER : ENEMY_TYPE_WATCHER;
        EnemyManagerAdd(&game->enemies, type,
                        game->station.enemySpawnX[ei],
                        game->station.enemySpawnY[ei]);
    }

    /* Guarantee the Hunter starts near the player. */
    if (game->enemies.count > 0 && game->station.roomCount > 1) {
        int nearestIdx = 1;
        float nearestDistSq = 999999.0f;
        float sx2, sy2;
        StationRoomCentre(&game->station, 0, &sx2, &sy2);
        for (int ri = 1; ri < game->station.roomCount; ri++) {
            float rx, ry;
            StationRoomCentre(&game->station, ri, &rx, &ry);
            float dx = rx - sx2, dy = ry - sy2;
            float d = dx * dx + dy * dy;
            if (d < nearestDistSq) { nearestDistSq = d; nearestIdx = ri; }
        }
        float hx, hy;
        StationRoomCentre(&game->station, nearestIdx, &hx, &hy);
        game->enemies.enemies[0].x = hx;
        game->enemies.enemies[0].y = hy;
    }

    /* Reset camera. */
    game->camera.target = game->player.position;
    game->camera.zoom   = 1.0f;

    /* Reset pacing timers. */
    game->graceTimer       = 8.0f;
    game->hudFadeTimer     = 8.0f;
    game->firstPingDone    = false;
    game->firstPingTime    = 0.0f;
    game->nearestEnemyDist = 99999.0f;

    /* Clear any lingering shader effects. */
    game->renderer.sonarFlash = 0.0f;
    game->renderer.sonarPulseCount = 0;

    /* Reset run-specific tracking. */
    memset(game->runVisitedRooms, 0, sizeof(game->runVisitedRooms));
    game->runVisitedRoomCount  = 0;
    game->runHadNearMiss       = false;
    game->runHadResonance      = false;
    game->runHadEchoGhost      = false;
    game->runHadShadow         = false;
    game->runHadRadioBurst     = false;
    game->runHadLoreTerminal   = false;
    game->runHadFalseRelay     = false;
    game->runHadHunterMimic    = false;
    game->runEscapeAchieved    = false;

    /* Track total attempts for FACILITY VETERAN achievement. */
    game->achievements.lifetimeAttempts++;
    AchievementSave(&game->achievements);
    if (game->achievements.lifetimeAttempts >= 3) {
        AchievementUnlock(&game->achievements, ACH_FACILITY_VET, game->elapsedTime);
    }

    game->state = GAME_STATE_BOOT;
}

void GameUpdate(Game *game)
{
    if (game == NULL || !game->isRunning) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        /* If achievements menu is open, close it first. */
        if (game->achievements.showMenu) {
            AchievementToggleMenu(&game->achievements);
        } else {
            game->isRunning = false;
            game->state     = GAME_STATE_EXIT;
        }
        return;
    }

    /* TAB toggles the achievements menu (only during gameplay or game-over). */
    if (IsKeyPressed(KEY_TAB) &&
        (game->state == GAME_STATE_PLAYING ||
         game->state == GAME_STATE_GAME_OVER ||
         game->state == GAME_STATE_WON)) {
        AchievementToggleMenu(&game->achievements);
        return;
    }

    /* If achievements menu is open, freeze all gameplay updates. */
    if (game->achievements.showMenu) {
        return;
    }

    /* --- Title screen: auto-sonar pulses reveal station behind logo --- */
    if (game->state == GAME_STATE_BOOT)
    {
        game->titleTimer -= game->deltaTime;

        /* Auto-emit sonar pulses from random room centres to reveal the station. */
        game->titlePulseTimer -= game->deltaTime;
        if (game->titlePulseTimer <= 0.0f && game->station.roomCount > 0) {
            int ri = GetRandomValue(0, game->station.roomCount - 1);
            float ox = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
            float oy = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
            SonarEmitPulse(&game->sonar, ox, oy);
            game->titlePulseTimer = 1.5f + (float)GetRandomValue(0, 15) / 10.0f; /* 1.5-3s */
        }
        SonarUpdate(&game->sonar, game->deltaTime);

        if (game->titleTimer <= 0.0f || IsKeyPressed(KEY_SPACE))
        {
            game->titleTimer = 0.0f;
            game->state      = GAME_STATE_PLAYING;
            game->fadeAlpha  = 0.0f;
            /* Clear title pulses. */
            SonarInit(&game->sonar);
        }
        return;
    }

    /* Freeze on game-over or win — only ESC works. */
    if (game->state == GAME_STATE_GAME_OVER || game->state == GAME_STATE_WON)
    {
        game->fadeTarget = 0.75f;
        /* Death flash timer still decays while the game-over screen is up. */
        if (game->deathFlashTimer > 0.0f) {
            game->deathFlashTimer -= game->deltaTime;
            if (game->deathFlashTimer < 0.0f) game->deathFlashTimer = 0.0f;
        }
        /* #25: Final Black Screen — countdown, then fade to reveal stats. */
        if (game->finalBlackTimer > 0.0f) {
            game->finalBlackTimer -= game->deltaTime;
            if (game->finalBlackTimer < 0.0f) game->finalBlackTimer = 0.0f;
        }
        /* R to restart (instant, with fade transition). */
        if (IsKeyPressed(KEY_R)) {
            GameRestart(game);
            return;
        }
        return;
    }

    /* --- Pacing timers --- */
    game->graceTimer   -= game->deltaTime;
    if (game->graceTimer < 0.0f) game->graceTimer = 0.0f;
    game->hudFadeTimer -= game->deltaTime;
    if (game->hudFadeTimer < 0.0f) game->hudFadeTimer = 0.0f;

    /* --- Sonar pulse --- */
    bool spacePressed = IsKeyPressed(KEY_SPACE);
    if (spacePressed) {
        /* Track ping interval for Hunter pattern learning.
         * Uses lastPingTime so each interval is the time SINCE the
         * previous ping, not cumulative from firstPingTime. */
        if (game->firstPingDone && game->lastPingTime > 0.0f) {
            float interval = game->elapsedTime - game->lastPingTime;
            for (int pi = 3; pi > 0; pi--) game->pingIntervals[pi] = game->pingIntervals[pi-1];
            game->pingIntervals[0] = interval;
            if (game->pingIntervalCount < 4) game->pingIntervalCount++;
        }
        game->lastPingTime = game->elapsedTime;
        bool wasFirstPing = !game->firstPingDone;

        /* #1: Echo Drift — 2% chance sonar origin drifts ±2-6px (independent X/Y). */
        float pulseX = game->player.position.x;
        float pulseY = game->player.position.y;
        if (GetRandomValue(0, 49) == 0) {
            pulseX += (float)(GetRandomValue(2, 6)) * ((GetRandomValue(0, 1) ? 1.0f : -1.0f));
            pulseY += (float)(GetRandomValue(2, 6)) * ((GetRandomValue(0, 1) ? 1.0f : -1.0f));
        }            SonarEmitPulse(&game->sonar, pulseX, pulseY);

        /* Achievement: first ping. */
        if (wasFirstPing) {
            AchievementUnlock(&game->achievements, ACH_FIRST_PING, game->elapsedTime);
            /* Achievement: "ANOMALY DETECTED" — check seed at game start. */
            if (game->anomalySeed) {
                AchievementUnlock(&game->achievements, ACH_ANOMALY, game->elapsedTime);
            }
        }

        /* #20: Rare Event — 1% chance the ping gets a mysterious response.
         * Another sonar pulse echoes back from a random room centre.
         * No explanation. No enemy shown. Just... a response. */
        if (GetRandomValue(0, 99) == 0 && game->station.roomCount > 1) {
            /* Achievement: "Something Answered" — mysterious echo response. */
            if (!game->runHadResonance) {
                game->runHadResonance = true;
                AchievementUnlock(&game->achievements, ACH_RESONANCE, game->elapsedTime);
            }
            int ri = GetRandomValue(0, game->station.roomCount - 1);
            float mx = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
            float my = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
            SonarEmitPulse(&game->sonar, mx, my);
            SoundPropEmit(&game->soundProp, SOUND_EVENT_SONAR_PULSE,
                          mx, my, SOUND_SONAR_RADIUS * 0.5f, 0.5f, 1.0f);
        }

        /* Track ping positions for Hunter pattern learning (#2).
         * Shift stored positions and store the new one. */
        game->lastPingPositionsX[1] = game->lastPingPositionsX[0];
        game->lastPingPositionsY[1] = game->lastPingPositionsY[0];
        game->lastPingPositionsX[0] = game->player.position.x;
        game->lastPingPositionsY[0] = game->player.position.y;

        /* Predict next ping position: extrapolate movement vector.
         * If the last 2 pings form a consistent direction, the Hunter
         * moves toward the predicted position instead of the exact ping. */
        game->predictedPingX = 0.0f;
        game->predictedPingY = 0.0f;
        if (game->lastPingPositionsX[1] != 0.0f || game->lastPingPositionsY[1] != 0.0f) {
            float dx = game->lastPingPositionsX[0] - game->lastPingPositionsX[1];
            float dy = game->lastPingPositionsY[0] - game->lastPingPositionsY[1];
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 10.0f && game->pingIntervalCount >= 1 && game->pingIntervals[0] < 3.0f) {
                /* Extrapolate: next position continues in same direction. */
                float predDist = d * 1.2f;  /* assume slightly larger step */
                game->predictedPingX = game->lastPingPositionsX[0] + (dx / d) * predDist;
                game->predictedPingY = game->lastPingPositionsY[0] + (dy / d) * predDist;
            }
        }

        /* During grace period the pulse is "silent" — enemies don't hear it.
         * Only after 15s does a ping attract Hunters. */
        if (game->graceTimer <= 0.0f) {
            SoundPropEmit(&game->soundProp, SOUND_EVENT_SONAR_PULSE,
                          game->player.position.x, game->player.position.y,
                          SOUND_SONAR_RADIUS, SOUND_SONAR_INTENSITY,
                          SOUND_SONAR_LIFETIME);
            game->alertsTriggered++;
        }
        game->scansUsed++;

        /* #10: Last Heartbeat — one more thump on death (triggered in death transition). */

        /* Feel: first ping is MEMORABLE (2× shake, 2× zoom). */
        if (wasFirstPing) {
            game->shakeTimer       = 0.20f;
            game->shakeIntensity   = 10.0f;
            game->cameraZoomTimer  = 0.40f;
            game->cameraZoomTarget = 1.04f;
        } else {
            game->shakeTimer       = 0.12f;
            game->shakeIntensity   = 5.0f;
            game->cameraZoomTimer  = 0.30f;
            game->cameraZoomTarget = 1.025f;
        }

        /* Shader flash: full brightness for one frame, then decays. */
        game->renderer.sonarFlash = 1.0f;

        /* Sonar saturation: track ping frequency.
         * Each ping increases saturation; it decays over time.
         * Spamming makes vision worse (reduces reveal strength in shader). */
        game->sonarSaturation = fminf(game->sonarSaturation + 0.25f, 1.0f);

        /* Hunter mimics sonar: after ping, start a ~2s timer.
         * When it fires, the Hunter emits its own fake sonar pulse
         * from its current position. Only activates late-game (after relay). */
        if (game->relaysActivated >= 1) {
            game->hunterMimicTimer = 1.8f + (float)GetRandomValue(0, 40) / 100.0f; /* 1.8-2.2s */
        }

        /* #3: Broken Sonar — 5% chance future ping is delayed by 0.5s. */
        if (GetRandomValue(0, 99) < 5 && game->brokenSonarTimer <= 0.0f) {
            game->brokenSonarTimer = 0.5f;
        }

        /* #20: Power Drain — sonar dims lights for 3s. */
        game->powerDrainTimer = 3.0f;

        /* #3: Echo Delay — track which room the player is in. */
        {
            game->lastPingRoomIdx = -1;
            for (int ri = 0; ri < game->station.roomCount; ri++) {
                const Room *r = &game->station.rooms[ri];
                if (game->player.position.x >= r->x && game->player.position.x <= r->x + r->w &&
                    game->player.position.y >= r->y && game->player.position.y <= r->y + r->h) {
                    game->lastPingRoomIdx = ri;
                    break;
                }
            }
        }

        game->firstPingDone = true;
        game->firstPingTime = game->elapsedTime;
    }

    /* #3: Broken Sonar delay — countdown, then emit delayed ping. */
    if (game->brokenSonarTimer > 0.0f) {
        game->brokenSonarTimer -= game->deltaTime;
        if (game->brokenSonarTimer <= 0.0f) {
            game->brokenSonarTimer = 0.0f;
            /* Create a brief, weak pulse to simulate delayed echo. */
            SonarEmitPulse(&game->sonar, game->player.position.x, game->player.position.y);
            game->renderer.sonarFlash = 1.0f;
        }
    }

    /* --- #3: Sonar Echo Delay — large rooms return a delayed echo. --- */
    {
        game->echoDelayTimer -= game->deltaTime;
        if (game->echoDelayTimer <= 0.0f && game->firstPingDone && game->state == GAME_STATE_PLAYING) {
            game->echoDelayTimer = 0.0f;
        }
        /* When the delayed echo fires, emit a weak secondary pulse at the room centre.
         * Set lastPingRoomIdx = -1 so it fires only once per ping. */
        if (game->echoDelayTimer > 0.0f && game->echoDelayTimer <= 0.05f && game->lastPingRoomIdx >= 0) {
            float rx = game->station.rooms[game->lastPingRoomIdx].x +
                       game->station.rooms[game->lastPingRoomIdx].w * 0.5f;
            float ry = game->station.rooms[game->lastPingRoomIdx].y +
                       game->station.rooms[game->lastPingRoomIdx].h * 0.5f;
            if (game->state == GAME_STATE_PLAYING) {
                SonarEmitPulse(&game->sonar, rx, ry);
                game->renderer.sonarFlash = 0.4f;
            }
            game->lastPingRoomIdx = -1; /* prevent re-firing */
        }
        /* Detect when current ping is in a large room and schedule delayed echo. */
        if (game->lastPingRoomIdx >= 0 && game->state == GAME_STATE_PLAYING) {
            const Room *pr = &game->station.rooms[game->lastPingRoomIdx];
            float area = pr->w * pr->h;
            if (area > 20000.0f && game->echoDelayTimer <= 0.0f) {
                game->echoDelayTimer = area / 40000.0f; /* ~0.5-1.6s for large rooms */
            }
        }
    }

    /* --- #20: Power Drain — each sonar briefly dims the station lights. --- */
    {
        if (game->powerDrainTimer > 0.0f) {
            game->powerDrainTimer -= game->deltaTime;
            float drain = fminf(game->powerDrainTimer / 2.0f, 1.0f) * 0.15f;
            game->renderer.sonarFlash = fminf(game->renderer.sonarFlash, 0.0f - drain * 0.3f);
            if (game->powerDrainTimer <= 0.0f) {
                game->powerDrainTimer = 0.0f;
            }
        }
    }

    /* --- Sonar saturation decay --- */
    if (game->sonarSaturation > 0.0f) {
        game->sonarSaturation -= game->deltaTime * 0.08f;
        if (game->sonarSaturation < 0.0f) game->sonarSaturation = 0.0f;
    }

    /* --- Hunter mimics sonar timer --- */
    if (game->hunterMimicTimer > 0.0f) {
        game->hunterMimicTimer -= game->deltaTime;
        if (game->hunterMimicTimer <= 0.0f && game->enemies.count > 0) {
            game->hunterMimicTimer = 0.0f;
            /* Achievement: "Deceived" — experience hunter mimic ping. */
            if (!game->runHadHunterMimic) {
                game->runHadHunterMimic = true;
                AchievementUnlock(&game->achievements, ACH_HUNTER_MIMIC, game->elapsedTime);
            }
            /* Emit a fake sonar pulse from the Hunter's position. */
            for (int hi = 0; hi < game->enemies.count; hi++) {
                const Enemy *he = &game->enemies.enemies[hi];
                if (!he->alive || he->type != ENEMY_TYPE_HUNTER) continue;
                if (he->state == ENEMY_STATE_IDLE || he->state == ENEMY_STATE_RETREAT) continue;
                SoundPropEmit(&game->soundProp, SOUND_EVENT_SONAR_PULSE,
                              he->x, he->y,
                              SOUND_SONAR_RADIUS * 0.6f, SOUND_SONAR_INTENSITY * 0.5f,
                              SOUND_SONAR_LIFETIME * 0.5f);
                break;
            }
        }
    }

    PlayerHandleInput(&game->player);

    /* --- Wall collision: slide player along room/corridor walls --- */
    {
        Vector2 oldPos = game->player.position;
        PlayerUpdate(&game->player, game->deltaTime);

        float r = game->player.radius;
        float newX = game->player.position.x;
        float newY = game->player.position.y;

        /* Check if the full desired position is walkable. */
        if (!StationIsWalkable(&game->station, newX, newY, r)) {
            /* Blocked — try sliding along each axis independently. */
            float slideX = oldPos.x;
            float slideY = oldPos.y;

            /* Try X only (keep old Y). */
            if (StationIsWalkable(&game->station, newX, oldPos.y, r)) {
                slideX = newX;
                slideY = oldPos.y;
            }
            /* Try Y only (use updated X if sliding, otherwise old X). */
            else if (StationIsWalkable(&game->station, oldPos.x, newY, r)) {
                slideX = oldPos.x;
                slideY = newY;
            }
            /* Both axes blocked — stay at original position. */

            game->player.position.x = slideX;
            game->player.position.y = slideY;
        }

        /* #3: Doors Amplify Sound — detect corridor transitions.
         * If the player just entered or exited a corridor, emit a DOOR_CLANG. */
        {
            bool nowInCorridor = false;
            for (int ci = 0; ci < game->station.corridorCount; ci++) {
                const Corridor *c = &game->station.corridors[ci];
                if (game->player.position.x >= c->x && game->player.position.x <= c->x + c->w &&
                    game->player.position.y >= c->y && game->player.position.y <= c->y + c->h) {
                    nowInCorridor = true;
                    break;
                }
            }
            if (nowInCorridor != game->wasInCorridor) {
                SoundPropEmit(&game->soundProp, SOUND_EVENT_DOOR_CLANG,
                              game->player.position.x, game->player.position.y,
                              SOUND_DOOR_CLANG_RADIUS, SOUND_DOOR_CLANG_INTENSITY,
                              SOUND_DOOR_CLANG_LIFETIME);
            }
            game->wasInCorridor = nowInCorridor;
        }

        /* --- #18: Dynamic Room Names — detect which room the player is in. --- */
        {
            int newRoom = -1;
            for (int ri = 0; ri < game->station.roomCount; ri++) {
                const Room *r = &game->station.rooms[ri];
                float margin = 8.0f;
                if (game->player.position.x >= r->x + margin && game->player.position.x <= r->x + r->w - margin &&
                    game->player.position.y >= r->y + margin && game->player.position.y <= r->y + r->h - margin) {
                    newRoom = ri;
                    break;
                }
            }
            if (newRoom != game->currentRoomIdx && newRoom >= 0) {
                game->currentRoomIdx = newRoom;
                game->roomNameTimer = 3.0f;
                /* Track visited rooms for Cartographer achievement. */
                if (newRoom < (int)(sizeof(game->runVisitedRooms) / sizeof(game->runVisitedRooms[0])) &&
                    !game->runVisitedRooms[newRoom]) {
                    game->runVisitedRooms[newRoom] = true;
                    game->runVisitedRoomCount++;
                }
                static const char *ROOM_PREFIXES[] = {
                    "SECTOR A-", "SECTOR B-", "SECTOR C-", "SECTOR D-", "SECTOR E-"
                };
                static const char *ROOM_TYPES[] = {
                    "Corridor", "Maintenance", "Storage", "Habitation", "Cryogenics",
                    "Hydroponics", "Comms", "Engineering", "Medbay", "Observatory"
                };
                int prefixIdx = (game->stationSeed + (uint64_t)newRoom) % 5;
                int typeIdx = (game->stationSeed * (uint64_t)(newRoom + 1)) % 10;
                snprintf(game->roomName, sizeof(game->roomName), "%s%02d — %s",
                         ROOM_PREFIXES[prefixIdx],
                         (int)((game->stationSeed + (uint64_t)newRoom) % 99) + 1,
                         ROOM_TYPES[typeIdx]);
            } else if (newRoom < 0) {
                game->currentRoomIdx = -1;
            }
        }
        if (game->roomNameTimer > 0.0f) {
            game->roomNameTimer -= game->deltaTime;
            if (game->roomNameTimer < 0.0f) game->roomNameTimer = 0.0f;
        }
    }

    SonarUpdate(&game->sonar, game->deltaTime);
    SoundPropUpdate(&game->soundProp, game->deltaTime);

    /* --- Relay room orange glow: trigger when a sonar pulse passes through --- */
    if (game->relayGlowTimer > 0.0f) {
        game->relayGlowTimer -= game->deltaTime;
        if (game->relayGlowTimer <= 0.0f) {
            game->relayGlowTimer = 0.0f;
            game->falseRelayRoomIdx = -1;   /* reset for next glow */
        }
    }
    if (game->relaysActivated < 1) {
        int ri = game->station.relayRoomIdxs[0];
        if (ri > 0 && ri < game->station.roomCount) {
            float rx = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
            float ry = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
            for (int p = 0; p < game->sonar.pulseCount; p++) {
                const SonarPulse *sp = &game->sonar.pulses[p];
                if (!sp->active) continue;
                float dx = sp->originX - rx;
                float dy = sp->originY - ry;
                float dist = sqrtf(dx * dx + dy * dy);
                /* Trigger when the wavefront is ~60px from the relay room centre
                 * (roughly one room-half).  Large margin = forgiving detection. */
                if (fabsf(dist - sp->currentRadius) < 60.0f) {
                    game->relayGlowTimer = 2.5f;
                    break;
                }
            }
        }
    }

    /* --- Failing electronics: random atmosphere events --- */
    {
        if (game->flickerDuration > 0.0f) {
            game->flickerDuration -= game->deltaTime;
            if (game->flickerDuration <= 0.0f) {
                game->flickerDuration = 0.0f;
                game->flickerType    = 0;
                game->flickerTimer   = 5.0f + (float)GetRandomValue(0, 80) / 10.0f; /* 5-13s */
            }
            
            /* #4: Power failure — type 4 makes the screen go very dark.
             * The darkness shader flash uniform goes negative to crush light. */

            /* #4: Machinery Wake-up — after relay activation, lights flicker more. */
            if (game->relaysActivated >= 1 && game->state == GAME_STATE_PLAYING) {
                /* Existing flicker events become more intense after relay activation. */
                if (game->flickerType >= 1 && game->flickerType <= 3) {
                    game->renderer.sonarFlash += 0.15f; /* brighter flickers */
                }
                /* Power failures are 30% longer after relay activation. */
                if (game->flickerType == 4 && game->flickerDuration > 0.0f) {
                    float extra = 0.3f * game->deltaTime;
                    game->flickerDuration += extra;
                    if (game->flickerDuration > 4.0f) game->flickerDuration = 4.0f;
                }
            }

            /* #5: Emergency Lights — RED/BLACK flashes every ~30s after first ping. */
            if (game->firstPingDone && game->state == GAME_STATE_PLAYING) {
                game->emergencyLightTimer -= game->deltaTime;
                if (game->emergencyLightTimer <= 0.0f) {
                    game->emergencyLightTimer = 25.0f + (float)GetRandomValue(0, 100) / 10.0f; /* 25-35s */
                }
            }

            /* #12: Fake Hunter Footsteps — after relay, random footstep sounds
             * from random room positions with no Hunter present. */
            if (game->relaysActivated >= 1 && game->state == GAME_STATE_PLAYING) {
                game->fakeFootstepTimer -= game->deltaTime;
                if (game->fakeFootstepTimer <= 0.0f && GetRandomValue(0, 99) < 5) {
                    if (game->station.roomCount > 1) {
                        int ri = GetRandomValue(0, game->station.roomCount - 1);
                        float fx = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
                        float fy = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
                        SoundPropEmit(&game->soundProp, SOUND_EVENT_FOOTSTEP,
                                      fx, fy, SOUND_FOOTSTEP_RADIUS, 0.5f, SOUND_FOOTSTEP_LIFETIME);
                    }
                    game->fakeFootstepTimer = 5.0f + (float)GetRandomValue(0, 30) / 10.0f;
                }
            }
            if (game->flickerType == 4) {
                game->renderer.sonarFlash = -0.4f;  /* negative = darken */
            }
            
            /* #13: CRT Failure — type 8, very short (30ms) screen glitch. */
            if (game->flickerType == 8) {
                game->renderer.sonarFlash = sinf(game->elapsedTime * 300.0f) * 0.8f;
            }
            
            /* #8: Hazard sounds — types 5-7 emit environmental noise events. */
            if (game->flickerType >= 5 && game->flickerType <= 7) {
                game->hazardSoundTimer -= game->deltaTime;
                if (game->hazardSoundTimer <= 0.0f) {
                    /* Emit a hazard sound at a random room position. */
                    if (game->station.roomCount > 1) {
                        int ri = GetRandomValue(0, game->station.roomCount - 1);
                        float hx = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
                        float hy = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
                        SoundPropEmit(&game->soundProp, SOUND_EVENT_HAZARD,
                                      hx, hy,
                                      SOUND_HAZARD_RADIUS, SOUND_HAZARD_INTENSITY,
                                      SOUND_HAZARD_LIFETIME);
                    }
                    game->hazardSoundTimer = 1.5f + (float)GetRandomValue(0, 30) / 10.0f;
                }
            }

            /* #18: Loose Cable Sparks — random zzzt spark at a random wall position. */
            if (game->firstPingDone && game->state == GAME_STATE_PLAYING) {
                game->cableSparkTimer -= game->deltaTime;
                if (game->cableSparkTimer <= 0.0f && game->station.roomCount > 1) {
                    int ri = GetRandomValue(1, game->station.roomCount - 1);
                    const Room *cr = &game->station.rooms[ri];
                    game->cableSparkX = cr->x + (float)GetRandomValue(10, (int)cr->w - 10);
                    game->cableSparkY = cr->y + (float)GetRandomValue(10, (int)cr->h - 10);
                    game->cableSparkTimer = 6.0f + (float)GetRandomValue(0, 60) / 10.0f;
                }
            }
        } else {
            game->flickerTimer -= game->deltaTime;
            if (game->flickerTimer <= 0.0f) {
                game->flickerTimer   = 0.0f;
                /* Random type: 1-3 are existing, 4 is power failure (rare, ~15%),
                 * 5-7 are hazard sounds (vent, steam, pipe). */
                /* #19: Older stations flicker more (+ageGlitchMult). */
                float ageFreq = 1.0f + (game->ageGlitchMult - 1.0f) * 0.5f;
                game->flickerTimer = (5.0f + (float)GetRandomValue(0, 80) / 10.0f) / ageFreq;
                game->flickerType = GetRandomValue(1, 8);
                /* Power failures are shorter (2s), CRT is very brief (0.1s), hazards longer. */
                game->flickerDuration = (game->flickerType == 4) ? 2.0f
                    : (game->flickerType == 8) ? 0.1f
                    : 1.0f + (float)GetRandomValue(0, 30) / 10.0f; /* 1-4s */
            }
        }
    }

    /* --- #13: Reactor Pulse — global brightness modulation for atmosphere. --- */
    {
        if (game->state == GAME_STATE_PLAYING) {
            game->reactorPulseTimer -= game->deltaTime;
            if (game->reactorPulseTimer <= 0.0f) {
                game->reactorPulseTimer = 12.0f;
                /* Brief subtle brightness pulse: combine with flash if no other flash. */
                if (game->renderer.sonarFlash >= 0.0f && game->renderer.sonarFlash < 0.03f) {
                    game->renderer.sonarFlash = 0.03f;
                }
            }
        }
    }

    /* --- #16: CRT Burn-in — brief afterimage after sonar or flicker. --- */
    {
        if (game->crtBurnTimer > 0.0f) {
            game->crtBurnTimer -= game->deltaTime;
            if (game->crtBurnTimer <= 0.0f) game->crtBurnTimer = 0.0f;
        }
        /* Trigger on sonar ping or CRT flicker type 8. */
        if (game->state == GAME_STATE_PLAYING) {
            bool triggered = (spacePressed) || (game->flickerType == 8 && game->flickerDuration > 0.05f);
            if (triggered && game->crtBurnTimer <= 0.0f) {
                game->crtBurnTimer = 0.2f; /* brief afterimage */
            }
        }
    }

    /* --- #12: Exit Door Breathing — white mist pulses at the airlock. --- */
    if (game->state == GAME_STATE_PLAYING && game->firstPingDone && !game->escapeDoorOpen) {
        game->exitMistTimer -= game->deltaTime;
        if (game->exitMistTimer <= 0.0f) {
            if (fabsf(game->player.position.x - game->airlockX) < 500.0f &&
                fabsf(game->player.position.y - game->airlockY) < 500.0f) {
                game->exitMistTimer = 2.0f + (float)GetRandomValue(0, 30) / 10.0f; /* 2-5s */
            } else {
                game->exitMistTimer = 5.0f; /* not near, check less often */
            }
        }
    }

    /* --- Echo ghosts (#5): periodically replay old sonar memories --- */
    {
        game->ghostTimer -= game->deltaTime;
        if (game->ghostTimer <= 0.0f && game->firstPingDone) {
            game->ghostReplayX = game->lastPingPositionsX[0]
                               + (float)GetRandomValue(-200, 200);
            game->ghostReplayY = game->lastPingPositionsY[0]
                               + (float)GetRandomValue(-200, 200);
            game->ghostTimer = 8.0f + (float)GetRandomValue(0, 60) / 10.0f; /* 8-14s */
            /* Achievement: "Echoes of the Past" — trigger echo ghost. */
            if (!game->runHadEchoGhost) {
                game->runHadEchoGhost = true;
                AchievementUnlock(&game->achievements, ACH_ECHO_GHOST, game->elapsedTime);
            }
        }
    }

    /* --- #12: Moving Shadows — one-frame shadow illusion near screen edge. --- */
    {
        game->shadowTimer -= game->deltaTime;
        if (game->shadowTimer <= 0.0f && game->firstPingDone) {
            float angle = (float)GetRandomValue(0, 6283) / 1000.0f;
            float dist = 80.0f + (float)GetRandomValue(0, 120);
            game->shadowX = GetScreenWidth() / 2.0f + cosf(angle) * dist;
            game->shadowY = GetScreenHeight() / 2.0f + sinf(angle) * dist;
            game->shadowTimer = 12.0f + (float)GetRandomValue(0, 60) / 10.0f;
            /* Achievement: "Peripheral Vision" — see a fleeting shadow. */
            if (!game->runHadShadow) {
                game->runHadShadow = true;
                AchievementUnlock(&game->achievements, ACH_SHADOW_WATCHER, game->elapsedTime);
            }
        }
    }

    /* --- #13: False Heartbeat — fake enemy proximity when none is near. --- */
    {
        game->falseHeartbeatTimer -= game->deltaTime;
        if (game->falseHeartbeatTimer <= 0.0f && game->firstPingDone && game->nearestEnemyDist > 300.0f) {
            game->nearestEnemyDist = 50.0f; /* triggers one heartbeat thump */
            game->falseHeartbeatTimer = 30.0f + (float)GetRandomValue(0, 60) / 10.0f; /* 30-36s */
        }
    }

    /* --- #14: Radio Burst — random distorted transmission fragments. --- */
    {
        if (game->radioDisplay > 0.0f) {
            game->radioDisplay -= game->deltaTime;
            if (game->radioDisplay <= 0.0f) {
                game->radioDisplay = 0.0f;
                game->radioText[0] = '\0';
            }
        } else {
            game->radioTimer -= game->deltaTime;
            if (game->radioTimer <= 0.0f && game->firstPingDone) {
                static const char *RADIO_FRAGS[] = {
                    "...HEL—", "...DON'T—", "...IS ANYONE—", "...THEY'RE—",
                    "...RUN—", "...ECHO—", "...SECTOR—", "...LAST—"
                };
                int fragIdx = GetRandomValue(0, 7);
                strncpy(game->radioText, RADIO_FRAGS[fragIdx], sizeof(game->radioText) - 1);
                game->radioText[sizeof(game->radioText) - 1] = '\0';
                game->radioDisplay = 2.0f;
                game->radioTimer = 35.0f + (float)GetRandomValue(0, 50) / 10.0f; /* 35-40s */
                /* Achievement: "Last Transmission" — hear a radio burst. */
                if (!game->runHadRadioBurst) {
                    game->runHadRadioBurst = true;
                    AchievementUnlock(&game->achievements, ACH_RADIO_BURST, game->elapsedTime);
                }
            }
        }
    }

    /* --- #11: Station Announcements (rare, distorted subtitle) --- */
    {
        if (game->announcementDisplay > 0.0f) {
            game->announcementDisplay -= game->deltaTime;
            if (game->announcementDisplay <= 0.0f) {
                game->announcementDisplay = 0.0f;
                game->announcementText[0] = '\0';
            }
        } else {
            game->announcementTimer -= game->deltaTime;
            if (game->announcementTimer <= 0.0f && game->firstPingDone) {
                /* Generate procedural announcement from word pools. */
                int wa = GetRandomValue(0, ANNOUNCE_WORDS_A_COUNT - 1);
                int wb = GetRandomValue(0, ANNOUNCE_WORDS_B_COUNT - 1);
                int wc = GetRandomValue(0, ANNOUNCE_WORDS_C_COUNT - 1);
                snprintf(game->announcementText, sizeof(game->announcementText),
                         "... %s ... %s ... %s ...",
                         ANNOUNCE_WORDS_A[wa],
                         ANNOUNCE_WORDS_B[wb],
                         ANNOUNCE_WORDS_C[wc]);
                game->announcementDisplay = 3.0f;
                game->announcementTimer = 20.0f + (float)GetRandomValue(0, 40); /* 20-60s */
            }
        }
    }

    /* --- #18: Procedural Logs — trigger when pulse reveals log room --- */
    if (game->logRoomIdx > 0 && game->logDisplayTimer <= 0.0f && game->firstPingDone) {
        for (int p = 0; p < game->sonar.pulseCount; p++) {
            const SonarPulse *sp = &game->sonar.pulses[p];
            if (!sp->active) continue;
            float lx = game->station.rooms[game->logRoomIdx].x + game->station.rooms[game->logRoomIdx].w * 0.5f;
            float ly = game->station.rooms[game->logRoomIdx].y + game->station.rooms[game->logRoomIdx].h * 0.5f;
            float dx = sp->originX - lx;
            float dy = sp->originY - ly;
            if (sqrtf(dx * dx + dy * dy) <= sp->currentRadius + 60.0f) {
                int logIdx = GetRandomValue(0, LORE_LOG_COUNT - 1);
                strncpy(game->logText, LORE_LOGS[logIdx], sizeof(game->logText) - 1);
                game->logText[sizeof(game->logText) - 1] = '\0';
                game->logDisplayTimer = 4.0f;
                /* Achievement: Lore Keeper — found a terminal with station lore. */
                if (!game->runHadLoreTerminal) {
                    game->runHadLoreTerminal = true;
                    AchievementUnlock(&game->achievements, ACH_LORE_KEEPER, game->elapsedTime);
                }
                break;
            }
        }
    }
    if (game->logDisplayTimer > 0.0f) {
        game->logDisplayTimer -= game->deltaTime;
        if (game->logDisplayTimer < 0.0f) game->logDisplayTimer = 0.0f;
    }

    /* --- #15: Impossible Room — detect when a pulse reveals the easter egg room --- */
    if (game->impossibleRoomIdx > 0 && game->impossibleRoomRevealTimer <= 0.0f && game->firstPingDone) {
        for (int p = 0; p < game->sonar.pulseCount; p++) {
            const SonarPulse *sp = &game->sonar.pulses[p];
            if (!sp->active) continue;
            float ix = game->station.rooms[game->impossibleRoomIdx].x + game->station.rooms[game->impossibleRoomIdx].w * 0.5f;
            float iy = game->station.rooms[game->impossibleRoomIdx].y + game->station.rooms[game->impossibleRoomIdx].h * 0.5f;
            float idx = sp->originX - ix;
            float idy = sp->originY - iy;
            if (sqrtf(idx * idx + idy * idy) <= sp->currentRadius + 30.0f) {
                game->impossibleRoomRevealTimer = 4.0f;
                break;
            }
        }
    }
    if (game->impossibleRoomRevealTimer > 0.0f) {
        game->impossibleRoomRevealTimer -= game->deltaTime;
        if (game->impossibleRoomRevealTimer < 0.0f) game->impossibleRoomRevealTimer = 0.0f;
    }

    /* --- #22: Hidden Observation Room — pulse reveals a silhouette behind glass. --- */
    if (game->hiddenRoomIdx > 0 && game->hiddenRevealTimer <= 0.0f && game->firstPingDone && game->state == GAME_STATE_PLAYING) {
        for (int p = 0; p < game->sonar.pulseCount; p++) {
            const SonarPulse *sp = &game->sonar.pulses[p];
            if (!sp->active) continue;
            float hx = game->station.rooms[game->hiddenRoomIdx].x + game->station.rooms[game->hiddenRoomIdx].w * 0.5f;
            float hy = game->station.rooms[game->hiddenRoomIdx].y + game->station.rooms[game->hiddenRoomIdx].h * 0.5f;
            float hdx = sp->originX - hx;
            float hdy = sp->originY - hy;
            if (sqrtf(hdx * hdx + hdy * hdy) <= sp->currentRadius + 30.0f) {
                game->hiddenRevealTimer = 3.0f;
                break;
            }
        }
    }
    if (game->hiddenRevealTimer > 0.0f) {
        game->hiddenRevealTimer -= game->deltaTime;
        if (game->hiddenRevealTimer < 0.0f) game->hiddenRevealTimer = 0.0f;
    }

    /* --- #2: Player trail — store last 5 positions --- */
    {
        if (game->state == GAME_STATE_PLAYING) {
            float speed = sqrtf(game->player.velocity.x * game->player.velocity.x +
                                game->player.velocity.y * game->player.velocity.y);
            if (speed > 1.0f) {
                for (int ti = 4; ti > 0; ti--) {
                    game->playerTrailX[ti] = game->playerTrailX[ti-1];
                    game->playerTrailY[ti] = game->playerTrailY[ti-1];
                }
                game->playerTrailX[0] = game->player.position.x;
                game->playerTrailY[0] = game->player.position.y;
                if (game->playerTrailCount < 5) game->playerTrailCount++;
            }
        }
    }

    /* Footstep dust decay. */
    for (int fd = 0; fd < 3; fd++) {
        if (game->playerDustLife[fd] > 0.0f) {
            game->playerDustLife[fd] -= game->deltaTime;
            if (game->playerDustLife[fd] < 0.0f) game->playerDustLife[fd] = 0.0f;
        }
    }

    /* --- #4: Enemy alert flash — detect ALERT transitions --- */
    {
        if (game->state == GAME_STATE_PLAYING && game->firstPingDone) {
            for (int ei = 0; ei < game->enemies.count; ei++) {
                const Enemy *e = &game->enemies.enemies[ei];
                if (!e->alive) continue;
                /* Check if enemy just went into ALERT state by looking at
                 * sound query results — we approximate by checking if an
                 * alert happened very recently (state timer near 0.6s). */
                if (e->type == ENEMY_TYPE_HUNTER && e->state == ENEMY_STATE_ALERT &&
                    game->alertFlashTimer <= 0.0f) {
                    game->alertFlashX = e->x;
                    game->alertFlashY = e->y;
                    game->alertFlashTimer = 0.5f;
                }
            }
        }
        if (game->alertFlashTimer > 0.0f) {
            game->alertFlashTimer -= game->deltaTime;
            if (game->alertFlashTimer < 0.0f) game->alertFlashTimer = 0.0f;
        }
    }

    UpdateFollowCamera(game);

    /* --- Decay the shader flash --- */
    if (game->renderer.sonarFlash > 0.0f) {
        game->renderer.sonarFlash -= game->deltaTime * 6.0f;
        if (game->renderer.sonarFlash < 0.0f)
            game->renderer.sonarFlash = 0.0f;
    }

    /* --- Fade transition --- */
    {
        float speed = 1.5f;
        if (game->fadeAlpha < game->fadeTarget) {
            game->fadeAlpha += game->deltaTime * speed;
            if (game->fadeAlpha > game->fadeTarget)
                game->fadeAlpha = game->fadeTarget;
        } else if (game->fadeAlpha > game->fadeTarget) {
            game->fadeAlpha -= game->deltaTime * speed;
            if (game->fadeAlpha < game->fadeTarget)
                game->fadeAlpha = game->fadeTarget;
        }
    }

    /* --- Footstep sound --- */
    bool footstepTriggered = false;
    {
        float speed = sqrtf(game->player.velocity.x * game->player.velocity.x +
                            game->player.velocity.y * game->player.velocity.y);
        if (speed > 0.0f) {
            /* #10/#15: Water puddles & rusty floors = louder footsteps. */
            float footstepMult = 1.0f;
            for (int w = 0; w < game->wetRoomCount; w++) {
                int ri = game->wetRoomIdxs[w];
                if (ri > 0 && ri < game->station.roomCount) {
                    const Room *wr = &game->station.rooms[ri];
                    if (game->player.position.x >= wr->x && game->player.position.x <= wr->x + wr->w &&
                        game->player.position.y >= wr->y && game->player.position.y <= wr->y + wr->h) {
                        footstepMult = 1.8f;
                        break;
                    }
                }
            }
            if (footstepMult < 1.5f) {
                for (int r = 0; r < game->rustyRoomCount; r++) {
                    int ri = game->rustyRoomIdxs[r];
                    if (ri > 0 && ri < game->station.roomCount) {
                        const Room *rr = &game->station.rooms[ri];
                        if (game->player.position.x >= rr->x && game->player.position.x <= rr->x + rr->w &&
                            game->player.position.y >= rr->y && game->player.position.y <= rr->y + rr->h) {
                            footstepMult = 2.0f;
                            break;
                        }
                    }
                }
            }
            float stepInterval = 0.35f / footstepMult;
            game->footstepTimer += game->deltaTime;
            if (game->footstepTimer >= stepInterval) {
                SoundPropEmit(&game->soundProp, SOUND_EVENT_FOOTSTEP,
                              game->player.position.x, game->player.position.y,
                              SOUND_FOOTSTEP_RADIUS * footstepMult,
                              SOUND_FOOTSTEP_INTENSITY * footstepMult,
                              SOUND_FOOTSTEP_LIFETIME);
                game->footstepTimer = 0.0f;
                footstepTriggered = true;
                /* Footstep dust puffs — 3 tiny grey particles at player feet. */
                for (int fd = 0; fd < 3; fd++) {
                    game->playerDustX[fd] = game->player.position.x + (float)GetRandomValue(-80, 80) / 10.0f;
                    game->playerDustY[fd] = game->player.position.y + 4.0f;
                    game->playerDustLife[fd] = 0.3f + (float)GetRandomValue(0, 10) / 100.0f;
                }
            }
        } else {
            game->footstepTimer = 0.0f;
        }
    }

    /* --- Cached nearest enemy distance (used by audio + heartbeat draw) --- */
    {
        game->nearestEnemyDist = 99999.0f;
        for (int i = 0; i < game->enemies.count; i++) {
            if (!game->enemies.enemies[i].alive) continue;
            float dx = game->enemies.enemies[i].x - game->player.position.x;
            float dy = game->enemies.enemies[i].y - game->player.position.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < game->nearestEnemyDist) game->nearestEnemyDist = d;
        }

        AmbientAudioUpdate(&game->ambient, game->deltaTime,
                           game->nearestEnemyDist,
                           footstepTriggered ? 1.0f : 0.0f,
                           spacePressed,
                           game->relaysActivated >= 1);
    }

    /* --- Sonar bridge to renderer (with echo distortion + sonar reflection) --- */
    {
        SonarRevealPulse reveals[ECHO_MAX_SONAR_REVEAL_PULSES];
        int revealCount = 0;
        for (int i = 0; i < game->sonar.pulseCount && revealCount < ECHO_MAX_SONAR_REVEAL_PULSES; i++) {
            const SonarPulse *sp = &game->sonar.pulses[i];
            if (!sp->active) continue;
            
            float baseStrength = SonarPulseRevealStrength(sp);
            
            /* --- Sonar Reflection: room type affects echo strength --- */
            float reflectionMult = 1.0f;
            bool inCorridor = false;
            float corrAxisX = 0.0f, corrAxisY = 0.0f;
            
            /* Check if pulse origin is inside a corridor (stretched echo). */
            for (int ci = 0; ci < game->station.corridorCount; ci++) {
                const Corridor *c = &game->station.corridors[ci];
                if (sp->originX >= c->x && sp->originX <= c->x + c->w &&
                    sp->originY >= c->y && sp->originY <= c->y + c->h) {
                    inCorridor = true;
                    /* Determine corridor axis: longer dimension = stretch direction. */
                    if (c->w > c->h) {
                        corrAxisX = 1.0f; corrAxisY = 0.0f;
                    } else {
                        corrAxisX = 0.0f; corrAxisY = 1.0f;
                    }
                    /* Concrete: corridors return weak, stretched echoes. */
                    reflectionMult = 0.7f;
                    break;
                }
            }
            
            /* Check if pulse origin is inside a room. */
            if (!inCorridor) {
                for (int ri = 0; ri < game->station.roomCount; ri++) {
                    const Room *r = &game->station.rooms[ri];
                    if (sp->originX >= r->x && sp->originX <= r->x + r->w &&
                        sp->originY >= r->y && sp->originY <= r->y + r->h) {
                        /* Metal rooms (important: objective, relay, airlock) = bright echo.
                         * All important rooms have index >= 1 (start room is 0), so
                         * we guard against unset relayRoomIdxs entries (which default to 0). */
                        if (ri == game->station.objectiveRoomIdx ||
                            ri == game->station.airlockRoomIdx ||
                            (game->station.relayRoomIdxs[0] > 0 && ri == game->station.relayRoomIdxs[0]) ||
                            (game->station.relayRoomIdxs[1] > 0 && ri == game->station.relayRoomIdxs[1]) ||
                            (game->station.relayRoomIdxs[2] > 0 && ri == game->station.relayRoomIdxs[2])) {
                            reflectionMult = 1.35f;  /* +35% brighter echo */
                        } else if (game->firstPingDone && game->state == GAME_STATE_PLAYING) {
                            /* #24: Environmental Resonance — modulate echo by room type. */
                            bool isWet = false, isRusty = false;
                            for (int wi = 0; wi < game->wetRoomCount; wi++) {
                                if (game->wetRoomIdxs[wi] == ri) { isWet = true; break; }
                            }
                            for (int ri2 = 0; ri2 < game->rustyRoomCount; ri2++) {
                                if (game->rustyRoomIdxs[ri2] == ri) { isRusty = true; break; }
                            }
                            if (isWet) {
                                reflectionMult = 0.80f;  /* water absorbs = dimmer echo */
                            } else if (isRusty) {
                                reflectionMult = 1.15f;  /* rust reflects = brighter echo */
                            } else {
                                /* Concrete rooms (outer rooms) = dull, quick fade. */
                                float roomDist = (float)ri / (float)game->station.roomCount;
                                reflectionMult = 1.0f - roomDist * 0.35f;  /* 1.0 down to 0.65 */
                            }
                        } else {
                            /* Concrete rooms (outer rooms) = dull, quick fade. */
                            float roomDist = (float)ri / (float)game->station.roomCount;
                            reflectionMult = 1.0f - roomDist * 0.35f;  /* 1.0 down to 0.65 */
                        }
                        break;
                    }
                }
            }
            
            float finalStrength = fminf(baseStrength * reflectionMult, 1.0f);
            
            reveals[revealCount].x        = sp->originX;
            reveals[revealCount].y        = sp->originY;
            reveals[revealCount].radius   = sp->currentRadius;
            reveals[revealCount].strength = finalStrength;
            revealCount++;
            
            /* Corridor stretch: add a secondary reveal pulse along the corridor axis
             * to simulate a stretched, distorted echo. */
            /* #10: Corridor Shape Echo — vary echo count by shape.
             * Long/narrow corridors get 3 echoes; short corridors get 1.
             * Room shape is determined while we already know it's a corridor. */
            if (inCorridor && baseStrength > 0.2f) {
                /* Find the corridor dimensions for shape classification. */
                float corrW = 0.0f, corrH = 0.0f;
                for (int ci = 0; ci < game->station.corridorCount; ci++) {
                    const Corridor *c = &game->station.corridors[ci];
                    if (sp->originX >= c->x && sp->originX <= c->x + c->w &&
                        sp->originY >= c->y && sp->originY <= c->y + c->h) {
                        corrW = c->w; corrH = c->h;
                        break;
                    }
                }
                float aspect = (corrH > 0.0f) ? corrW / corrH : 1.0f;
                int echoCount = (aspect > 3.0f || aspect < 0.33f) ? 3 : 2;
                
                for (int echo = 0; echo < echoCount && revealCount < ECHO_MAX_SONAR_REVEAL_PULSES; echo++) {
                    float sign = (echo % 2 == 0) ? 1.0f : -1.0f;
                    float offsetFrac = 0.2f + (float)echo * 0.15f;
                    float stretchDist = sp->currentRadius * offsetFrac * sign;
                    reveals[revealCount].x = sp->originX + corrAxisX * stretchDist;
                    reveals[revealCount].y = sp->originY + corrAxisY * stretchDist;
                    reveals[revealCount].radius = sp->currentRadius * (1.0f - (float)echo * 0.15f);
                    reveals[revealCount].strength = finalStrength * (0.55f - (float)echo * 0.10f);
                    revealCount++;
                }
            }        /* #1: Interference zones — check if pulse origin overlaps
         * an interference room.  Reduce reveal by 40% and add static. */
            if (!inCorridor) {
                for (int iz = 0; iz < game->station.interferenceRoomCount; iz++) {
                    int iri = game->station.interferenceRoomIdxs[iz];
                    if (iri > 0 && iri < game->station.roomCount) {
                        const Room *ir = &game->station.rooms[iri];
                        if (sp->originX >= ir->x && sp->originX <= ir->x + ir->w &&
                            sp->originY >= ir->y && sp->originY <= ir->y + ir->h) {
                            finalStrength *= 0.6f;  /* -40% reveal */
                            game->sonarSaturation = fminf(game->sonarSaturation + 0.15f, 1.0f);
                            break;
                        }
                    }
                }
            }
            
            /* Echo distortion: 20% chance to inject a false reveal circle
             * near the wavefront — misleads the player about the environment. */
            if (baseStrength > 0.3f && revealCount < ECHO_MAX_SONAR_REVEAL_PULSES
                && (GetRandomValue(0, 99) < 20))
            {
                float angle = (float)GetRandomValue(0, 6283) / 1000.0f; /* 0..2PI */
                float offset = sp->currentRadius * (0.6f + (float)GetRandomValue(0, 40) / 100.0f);
                reveals[revealCount].x      = sp->originX + cosf(angle) * offset;
                reveals[revealCount].y      = sp->originY + sinf(angle) * offset;
                reveals[revealCount].radius = 30.0f + (float)GetRandomValue(0, 60);
                reveals[revealCount].strength = finalStrength * 0.6f;
                revealCount++;
            }
        }

        /* #5: Echo Ghosts — inject a ghost reveal from a past pulse position.
         * Only when ghostTimer == 0 (active ghost frame). */
        if (game->ghostReplayX != 0.0f || game->ghostReplayY != 0.0f) {
            if (revealCount < ECHO_MAX_SONAR_REVEAL_PULSES) {
                reveals[revealCount].x        = game->ghostReplayX;
                reveals[revealCount].y        = game->ghostReplayY;
                reveals[revealCount].radius   = 80.0f + (float)GetRandomValue(0, 60);
                reveals[revealCount].strength = 0.35f;
                revealCount++;
            }
            /* Clear ghost so it only fires once per timer. */
            game->ghostReplayX = 0.0f;
            game->ghostReplayY = 0.0f;
        }

        RendererSetSonarPulses(&game->renderer, reveals, revealCount);
    }

    /* --- Echo memory capture --- */
    {
        const float cellSize = (float)ECHO_MEMORY_CELL_SIZE;
        const float halfCell = cellSize * 0.5f;
        for (int p = 0; p < game->sonar.pulseCount; p++) {
            const SonarPulse *sp = &game->sonar.pulses[p];
            if (!sp->active) continue;
            float radiusSq = sp->currentRadius * sp->currentRadius;
            int steps = (int)(sp->currentRadius / cellSize) + 2;
            for (int dx = -steps; dx <= steps; dx++) {
                for (int dy = -steps; dy <= steps; dy++) {
                    float cx = sp->originX + (float)dx * cellSize + halfCell;
                    float cy = sp->originY + (float)dy * cellSize + halfCell;
                    float ox = cx - sp->originX;
                    float oy = cy - sp->originY;
                    if ((ox * ox + oy * oy) <= radiusSq) {
                        EchoMemoryReveal(&game->memory, cx, cy, game->elapsedTime);
                    }
                }
            }
        }
    }

    /* --- Compute Hunter pattern multiplier --- */
    float hunterAlertMult = 1.0f;
    if (game->pingIntervalCount >= 3) {
        /* Check if all intervals are ~2.0s (within ±0.5s). */
        bool predictable = true;
        for (int pi = 0; pi < game->pingIntervalCount && pi < 4; pi++) {
            if (fabsf(game->pingIntervals[pi] - 2.0f) > 0.5f) {
                predictable = false;
                break;
            }
        }
        if (predictable) hunterAlertMult = 0.4f;  /* 60% shorter alert */
    }

    /* --- Enemy AI --- */
    EnemyManagerUpdate(&game->enemies, game->deltaTime, &game->soundProp,
                       &game->station,
                       game->relaysActivated >= 1,
                       hunterAlertMult,
                       game->predictedPingX, game->predictedPingY);

    /* --- Near Miss: Hunter rushed within 10px but missed → 0.2s slow-mo --- */
    if (game->nearMissTimer > 0.0f) {
        game->nearMissTimer -= game->deltaTime;
        if (game->nearMissTimer <= 0.0f) {
            game->nearMissTimer = 0.0f;
        }
    }

    /* --- Game-over: enemy caught the player (with slow-mo death) --- */
    if (!game->deathSlowMo && game->nearMissTimer <= 0.0f)
    {
        CircleShape pc = PlayerGetCollider(&game->player);
        for (int i = 0; i < game->enemies.count; i++) {
            if (!game->enemies.enemies[i].alive) continue;
            if (CollisionCircleCircle(pc, EnemyGetCollider(&game->enemies.enemies[i]))) {
                game->deathSlowMo      = true;
                game->deathFlashTimer  = 0.6f;
                game->runTimeDisplay   = game->elapsedTime;
                /* Shader flash for CRT distortion effect. */
                game->renderer.sonarFlash = 1.0f;
                break;
            }
        }
        
        /* Near Miss: check if any rushing Hunter just missed the player. */
        if (!game->deathSlowMo) {
            for (int i = 0; i < game->enemies.count; i++) {
                const Enemy *e = &game->enemies.enemies[i];
                if (!e->alive || e->type != ENEMY_TYPE_HUNTER) continue;
                if (e->state != ENEMY_STATE_RUSH) continue;
                float dx = e->x - game->player.position.x;
                float dy = e->y - game->player.position.y;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < 10.0f) {
                    game->nearMissTimer = 0.2f;  /* brief slow-mo burst */
                    game->renderer.sonarFlash = 0.6f;
                    /* Near miss heartbeat thump (use existing audio system). */
                    game->nearestEnemyDist = 0.0f; /* triggers immediate heartbeat */
                    /* Achievement: "Barely Breathing" — survive a near miss. */
                    if (!game->runHadNearMiss) {
                        game->runHadNearMiss = true;
                        AchievementUnlock(&game->achievements, ACH_DEATHS_DOOR, game->elapsedTime);
                    }
                    break;
                }
            }
        }
    }
    else
    {
        /* Slow-motion death sequence — can skip with ESC. */
        if (IsKeyPressed(KEY_ESCAPE)) {
            game->isRunning = false;
            game->state     = GAME_STATE_EXIT;
            return;
        }
        game->deathFlashTimer -= game->deltaTime;
        if (game->deathFlashTimer <= 0.0f)
        {
            game->deathFlashTimer = 0.0f;
            game->deathSlowMo     = false;
            /* #25: Final Black Screen — brief delay before game-over screen. */
            game->finalBlackTimer = 1.0f;

            game->state           = GAME_STATE_GAME_OVER;
            strcpy(game->lastOutcome, "Lost Contact");
            /* #10: Last Heartbeat — one final thump after death. */
            game->nearestEnemyDist = 0.0f;

            /* --- Death-related achievements --- */
            game->achievements.lifetimeDeaths++;
            AchievementSave(&game->achievements);
            /* ACH_FINAL_ECHO: "First Contact" — die for the first time. */
            if (game->achievements.lifetimeDeaths == 1) {
                AchievementUnlock(&game->achievements, ACH_FINAL_ECHO, game->elapsedTime);
            }
            /* ACH_PERSISTENT: "Persistent Signal" — die 5 times. */
            if (game->achievements.lifetimeDeaths >= 5) {
                AchievementUnlock(&game->achievements, ACH_PERSISTENT, game->elapsedTime);
            }
            /* ACH_MASOCHIST: "Unbroken" — die 10 times. */
            if (game->achievements.lifetimeDeaths >= 10) {
                AchievementUnlock(&game->achievements, ACH_MASOCHIST, game->elapsedTime);
            }
        }
        /* Decay the shader flash during slow-mo. */
        if (game->renderer.sonarFlash > 0.0f) {
            game->renderer.sonarFlash -= game->deltaTime * 3.0f;
            if (game->renderer.sonarFlash < 0.0f)
                game->renderer.sonarFlash = 0.0f;
        }
        
        /* Last Echo: sonar pulses keep expanding during death sequence.
         * The player sees the last ping ripple past their dead body. */
        SonarUpdate(&game->sonar, game->deltaTime * 0.5f);  /* half speed */
        /* Still emit sonar reveal data for the draw pass. */
        SonarRevealPulse lastReveals[ECHO_MAX_SONAR_REVEAL_PULSES];
        int lastRC = 0;
        for (int i = 0; i < game->sonar.pulseCount && lastRC < ECHO_MAX_SONAR_REVEAL_PULSES; i++) {
            const SonarPulse *sp = &game->sonar.pulses[i];
            if (!sp->active) continue;
            lastReveals[lastRC].x        = sp->originX;
            lastReveals[lastRC].y        = sp->originY;
            lastReveals[lastRC].radius   = sp->currentRadius;
            lastReveals[lastRC].strength = SonarPulseRevealStrength(sp);
            lastRC++;
        }
        RendererSetSonarPulses(&game->renderer, lastReveals, lastRC);
        
        return;  /* freeze gameplay during death sequence */
    }

    /* --- Relay activation: hold E to activate the console --- */
    if (game->relaysActivated < 1 && game->relayActivationProgress < 1.0f && game->station.roomCount > 1)
    {
        int ri = game->station.relayRoomIdxs[0];
        if (ri > 0 && ri < game->station.roomCount) {
            float rx = game->station.rooms[ri].x + game->station.rooms[ri].w * 0.5f;
            float ry = game->station.rooms[ri].y + game->station.rooms[ri].h * 0.5f;
            float dx = game->player.position.x - rx;
            float dy = game->player.position.y - ry;
            bool nearConsole = ((dx * dx + dy * dy) <= (60.0f * 60.0f));
            
            if (nearConsole && IsKeyDown(KEY_E)) {
                game->relayActivationProgress += game->deltaTime / 2.0f;  /* 2s hold */
                if (game->relayActivationProgress >= 1.0f) {
                    game->relayActivationProgress = 1.0f;
                    game->relaysActivated = 1;
                    /* Activation flash + shake + alarm sound. */
                    game->renderer.sonarFlash = 0.8f;
                    game->shakeTimer       = 0.25f;
                    game->shakeIntensity   = 8.0f;
                    /* #5: Relay activation spark burst */
                    game->relayActivationProgress = -0.5f;
                    game->cameraZoomTimer  = 0.40f;
                    game->cameraZoomTarget = 1.04f;
                    SoundPropEmit(&game->soundProp, SOUND_EVENT_SONAR_PULSE,
                                  game->player.position.x, game->player.position.y,
                                  300.0f, 0.8f, 1.0f);
                }
            } else if (!nearConsole || !IsKeyDown(KEY_E)) {
                /* Progress decays when not holding E. */
                game->relayActivationProgress -= game->deltaTime * 1.5f;                if (game->relayActivationProgress < -1.0f) game->relayActivationProgress = -1.0f;
                if (game->relayActivationProgress < 0.0f && game->relayActivationProgress >= -1.0f) {
                    game->relayActivationProgress += game->deltaTime * 2.0f;
                    if (game->relayActivationProgress >= 0.0f) game->relayActivationProgress = 0.0f;
                }
        }
    }
    }

    /* --- Win: reached the airlock (requires relay activated, then escape countdown) --- */
    if (!game->gameWon && !game->airlockSequence && game->relaysActivated >= 1)
    {
        float dx = game->player.position.x - game->airlockX;
        float dy = game->player.position.y - game->airlockY;
        if ((dx * dx + dy * dy) <= (80.0f * 80.0f)) {
            game->airlockSequence  = true;
            game->airlockCountdown = 3.0f;
            game->runTimeDisplay   = game->elapsedTime;
            /* #16: Dynamic Ending — determine tension level from alerts. */
            if (game->alertsTriggered <= 1) {
                game->endingTension = 0;  /* silent escape */
            } else if (game->alertsTriggered >= 3) {
                game->endingTension = 2;  /* full alarm */
            } else {
                game->endingTension = 1;  /* normal */
            }
            /* Emit a dramatic final sonar pulse on airlock arrival. */
            SonarEmitPulse(&game->sonar,
                           game->player.position.x,
                           game->player.position.y);
            game->scansUsed++;
            game->renderer.sonarFlash = 1.0f;
        }
    }
    if (game->airlockSequence)
    {
        game->airlockCountdown -= game->deltaTime;
        if (game->airlockCountdown <= 0.0f && !game->escapeDoorOpen)
        {
            game->airlockCountdown = 0.0f;
            game->escapeDoorOpen  = true;
            game->escapeWhiteTimer = 2.5f;
            game->runTimeDisplay   = game->elapsedTime;
        }
        
        /* White-out escape sequence. */
        if (game->escapeDoorOpen)
        {
            game->escapeWhiteTimer -= game->deltaTime;
            /* Trigger sonar flash at the start of door opening. */
            if (game->escapeWhiteTimer > 2.3f) {
                game->renderer.sonarFlash = 1.0f;
            }
            /* Player walks toward the airlock door. */
            float walkT = 1.0f - game->escapeWhiteTimer / 2.5f;
            if (walkT < 1.0f) {
                float ex = game->airlockX;
                float ey = game->airlockY;
                game->player.position.x += (ex - game->player.position.x) * game->deltaTime * 2.0f;
                game->player.position.y += (ey - game->player.position.y) * game->deltaTime * 2.0f;
            }
            /* Fade to black during the last 0.5s of the white-out. */
            if (game->escapeWhiteTimer < 0.5f) {
                game->fadeTarget = 1.0f;
            }
            if (game->escapeWhiteTimer <= 0.0f)
            {
                game->escapeWhiteTimer = 0.0f;
                game->gameWon         = true;
                game->state           = GAME_STATE_WON;
                strcpy(game->lastOutcome, "Recovered");
                
                /* --- Escaped! Unlock escape-related achievements. --- */
                game->runEscapeAchieved = true;
                game->achievements.lifetimeEscapes++;
                AchievementSave(&game->achievements);
                
                /* ACH_ESCAPED: "Last Light" — successfully escape. */
                AchievementUnlock(&game->achievements, ACH_ESCAPED, game->elapsedTime);
                
                /* ACH_ECHOLESS: "Ω ECHOLESS" — 0 alerts. */
                if (game->alertsTriggered == 0) {
                    AchievementUnlock(&game->achievements, ACH_ECHOLESS, game->elapsedTime);
                }
                
                /* ACH_SILENT_RUN: "Ghost Protocol" — 0 alerts when escaping. */
                if (game->alertsTriggered == 0) {
                    AchievementUnlock(&game->achievements, ACH_SILENT_RUN, game->elapsedTime);
                }
                
                /* ACH_SPEED_DEMON: "Speed of Dark" — escape in < 2 min. */
                if (game->runTimeDisplay < 120.0f) {
                    AchievementUnlock(&game->achievements, ACH_SPEED_DEMON, game->elapsedTime);
                }
            }
        }
    }

    /* --- Achievement detection during gameplay --- */
    if (game->state == GAME_STATE_PLAYING) {
        /* ACH_TRIGGER_HAPPY: "Resonance Cascade" — 20+ pings one run. */
        if (game->scansUsed >= 20) {
            AchievementUnlock(&game->achievements, ACH_TRIGGER_HAPPY, game->elapsedTime);
        }

        /* ACH_EXPLORER: "Cartographer" — visit every room. */
        if (game->runVisitedRoomCount >= game->station.roomCount) {
            AchievementUnlock(&game->achievements, ACH_EXPLORER, game->elapsedTime);
        }
    }

    /* --- Update achievement popup animation --- */
    AchievementUpdatePopup(&game->achievements, game->deltaTime);
}

void GameDraw(Game *game)
{
    if (game == NULL) return;

    /* --- Title screen (GAME_STATE_BOOT) with dynamic station reveal --- */
    if (game->state == GAME_STATE_BOOT)
    {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        ClearBackground(BLACK);

        /* Draw the station layout dimly behind the title so sonar pulses
         * reveal it — immediately teaches the core mechanic. */
        game->camera.zoom = 0.35f;
        game->camera.target = (Vector2){
            game->station.rooms[0].x + game->station.rooms[0].w * 0.5f,
            game->station.rooms[0].y + game->station.rooms[0].h * 0.5f
        };
        game->camera.offset = (Vector2){ sw / 2.0f, sh / 2.0f };
        game->camera.rotation = 0.0f;

        /* Draw station and sonar pulses in world space. */
        BeginMode2D(game->camera);
            StationDraw(&game->station);
            /* Sonar pulse rings. */
            for (int i = 0; i < game->sonar.pulseCount; i++) {
                const SonarPulse *sp = &game->sonar.pulses[i];
                if (!sp->active) continue;
                float s = SonarPulseRevealStrength(sp);
                if (s <= 0.0f) continue;
                DrawRing((Vector2){ sp->originX, sp->originY },
                         sp->currentRadius - 2.0f, sp->currentRadius + 2.0f,
                         0.0f, 360.0f, 0,
                         Fade((Color){ 100, 200, 255, 255 }, s * 0.8f));
            }
        EndMode2D();

        /* Semi-transparent overlay so the title text is readable. */
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55f));

        /* Pulsing glow ring behind the title. */
        float ringPulse = sinf(game->elapsedTime * 1.2f) * 0.15f + 0.85f;
        float ringR = 180.0f + ringPulse * 30.0f;
        DrawRing((Vector2){ (float)sw / 2.0f, (float)sh / 2.0f - 30.0f },
                 ringR - 3.0f, ringR + 3.0f, 0.0f, 360.0f, 0,
                 Fade((Color){ 70, 160, 255, 255 }, 0.10f * ringPulse));

        /* Expand/contract outer glow (sonar-like). */
        float expand = sinf(game->elapsedTime * 0.6f) * 0.5f + 0.5f;
        float glowR = 100.0f + expand * 140.0f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleV((Vector2){ (float)sw / 2.0f, (float)sh / 2.0f - 30.0f },
                    glowR, Fade((Color){ 40, 120, 200, 255 }, 0.04f));
        EndBlendMode();

        /* Title particles — 6 drifting cyan dots behind logo. */
        if (game->state == GAME_STATE_BOOT) {
            float titleAlpha = fminf(game->titleTimer / 2.0f, 1.0f);
            for (int tp = 0; tp < 6; tp++) {
                float tpAngle = game->elapsedTime * 0.5f + (float)tp * 1.047f;
                float tpDist = 100.0f + sinf(game->elapsedTime * 0.8f + (float)tp) * 30.0f;
                float tpx = (float)sw / 2.0f + cosf(tpAngle) * tpDist;
                float tpy = (float)sh / 2.0f - 30.0f + sinf(tpAngle * 0.7f) * 40.0f;
                DrawCircleV((Vector2){ tpx, tpy }, 2.0f,
                    Fade((Color){ 120, 200, 255, 255 }, 0.15f * titleAlpha));
            }
        }

        /* Title text — "ECHO PROTOCOL" with soft glow. */
        const char *title = "ECHO PROTOCOL";
        int titleSize = 52;
        int tw = MeasureText(title, titleSize);
        float titleAlpha = fminf(game->elapsedTime * 2.0f, 1.0f);
        Color titleColor = (Color){ 160, 220, 255, (unsigned char)(titleAlpha * 255.0f) };
        DrawText(title, (sw - tw) / 2, sh / 2 - 90, titleSize, titleColor);

        /* Subtitle. */
        const char *sub = "Navigate the dark. Survive the echoes.";
        int subSize = 16;
        int subW = MeasureText(sub, subSize);
        DrawText(sub, (sw - subW) / 2, sh / 2 - 30, subSize,
                 Fade(LIGHTGRAY, titleAlpha * 0.6f));

        /* #26: Hidden Seed Challenge — ANOMALY label. */
        if (game->anomalySeed) {
            const char *anomalyLabel = "[ANOMALY]";
            DrawText(anomalyLabel, (sw - MeasureText(anomalyLabel, 11)) / 2,
                     sh / 2 - 38, 11,
                     Fade((Color){ 200, 50, 200, 255 }, titleAlpha * 0.5f));
        }

        /* #19: Adaptive Title Screen — attempt count & outcome. */
        /* --- #14: Random Subtitle --- */
        {
            static const char *SUBTITLES[] = {
                "No Response", "Signal Lost", "Listen Carefully",
                "Do Not Answer", "Last Contact", "Echo Chamber",
                "Beneath the Static", "We Hear You"
            };
            int subtitleCount = (int)(sizeof(SUBTITLES) / sizeof(SUBTITLES[0]));
            if (game->titleSubtitleIdx < subtitleCount) {
                const char *subtitle = SUBTITLES[game->titleSubtitleIdx];
                int subW2 = MeasureText(subtitle, 14);
                DrawText(subtitle, (sw - subW2) / 2, sh / 2 - 55, 14,
                         Fade((Color){ 120, 180, 220, 255 }, titleAlpha * 0.35f));
            }
        }

        if (game->attemptCount > 1) {
            char attemptStr[48];
            snprintf(attemptStr, 48, "Attempt %d  —  %s", game->attemptCount, game->lastOutcome);
            int aW = MeasureText(attemptStr, 12);
            DrawText(attemptStr, (sw - aW) / 2, sh / 2 + 10, 12,
                     Fade((Color){ 180, 180, 200, 255 }, titleAlpha * 0.4f));
        }

        /* "Press SPACE" prompt (pulses). */
        float blink = sinf(game->elapsedTime * 2.5f) * 0.4f + 0.6f;
        const char *prompt = "Press SPACE to begin";
        int promptSize = 18;
        int pw = MeasureText(prompt, promptSize);
        DrawText(prompt, (sw - pw) / 2, sh / 2 + 30, promptSize,
                 Fade((Color){ 100, 200, 255, 255 }, blink * titleAlpha * 0.8f));

        /* Version / contest tagline. */
        const char *ver = "Made for 2P GAME ARCADE 1.44 MB";
        int verW = MeasureText(ver, 12);
        DrawText(ver, (sw - verW) / 2, sh - 50, 12, Fade(GRAY, 0.4f));

        /* Created by credit with pulsing procedural heart icon. */
        float heartPulse = sinf(game->elapsedTime * 2.5f) * 0.35f + 0.65f;
        const char *credit = "Created by Zaid Shabir";
        int creditSize = 12;
        int creditW = MeasureText(credit, creditSize);
        int creditX = (sw - creditW) / 2 + 12;
        DrawText(credit, creditX, sh - 28, creditSize,
                 Fade((Color){ 200, 200, 220, 255 }, 0.45f * heartPulse));

        /* Procedural heart icon. */
        Color heartColor = Fade((Color){ 255, 60, 80, 255 }, 0.6f * heartPulse);
        float hCx = (float)creditX - 16.0f;
        float hCy = (float)sh - 24.0f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleV((Vector2){ hCx - 3.5f, hCy - 1.5f }, 4.0f, heartColor);
        DrawCircleV((Vector2){ hCx + 3.5f, hCy - 1.5f }, 4.0f, heartColor);
        DrawTriangle((Vector2){ hCx - 6.0f, hCy + 1.0f },
                     (Vector2){ hCx + 6.0f, hCy + 1.0f },
                     (Vector2){ hCx, hCy + 8.0f },
                     heartColor);
        EndBlendMode();

        /* Fade overlay — fades from black on launch. */
        float fadeIn = (game->elapsedTime < 1.0f)
                     ? 1.0f - game->elapsedTime
                     : 0.0f;
        if (fadeIn > 0.0f)
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, fadeIn));
        return;
    }

    /* --- Pass 1: world into scene texture --- */
    RendererBeginWorld(&game->renderer);
    BeginMode2D(game->camera);

        StationDraw(&game->station);

        /* Footstep dust puffs — 3 small grey circles at player feet. */
        if (game->state == GAME_STATE_PLAYING) {
            for (int fd = 0; fd < 3; fd++) {
                if (game->playerDustLife[fd] > 0.0f) {
                    float dA = game->playerDustLife[fd] / 0.3f;
                    DrawCircleV((Vector2){ game->playerDustX[fd], game->playerDustY[fd] },
                        1.5f, Fade((Color){ 120, 120, 130, 255 }, dA * 0.08f));
                }
            }
        }

        /* #2: Fading player trail — 3 circles behind player. */
        if (game->state == GAME_STATE_PLAYING && game->playerTrailCount > 1) {
            for (int ti = 1; ti < game->playerTrailCount && ti < 4; ti++) {
                float tA = 1.0f - (float)ti / (float)game->playerTrailCount;
                DrawCircleV((Vector2){ game->playerTrailX[ti], game->playerTrailY[ti] },
                    3.0f + tA * 3.0f,
                    Fade((Color){ 160, 200, 255, 255 }, tA * 0.04f));
            }
        }

        /* #1: Room wall glow — cyan walls when sonar overlaps room. */
        if (game->state == GAME_STATE_PLAYING && game->firstPingDone) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int ri = 0; ri < game->station.roomCount; ri++) {
                const Room *rw = &game->station.rooms[ri];
                float rcx = rw->x + rw->w * 0.5f, rcy = rw->y + rw->h * 0.5f;
                for (int pi = 0; pi < game->sonar.pulseCount; pi++) {
                    const SonarPulse *sp = &game->sonar.pulses[pi];
                    if (!sp->active) continue;
                    float dx = sp->originX - rcx, dy = sp->originY - rcy;
                    float d = sqrtf(dx * dx + dy * dy);
                    if (fabsf(d - sp->currentRadius) < 40.0f) {
                        DrawRectangleLinesEx((Rectangle){ rw->x, rw->y, rw->w, rw->h }, 2.0f,
                            Fade((Color){ 150, 210, 255, 255 }, 0.15f));
                        break;
                    }
                }
            }
            EndBlendMode();
        }

        /* #4: Enemy alert flash — white ring when enemy detects player. */
        if (game->alertFlashTimer > 0.0f) {
            float flashT = game->alertFlashTimer / 0.5f;
            float flashR = (1.0f - flashT) * 60.0f;
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRing((Vector2){ game->alertFlashX, game->alertFlashY },
                     flashR - 1.0f, flashR + 1.0f, 0.0f, 360.0f, 0,
                     Fade((Color){ 255, 255, 255, 255 }, flashT * 0.4f));
            EndBlendMode();
        }

        /* #3: Pulse edge particles — dots scattering from sonar ring edge. */
        if (game->state == GAME_STATE_PLAYING && game->sonar.pulseCount > 0) {
            for (int pi = 0; pi < game->sonar.pulseCount; pi++) {
                const SonarPulse *sp = &game->sonar.pulses[pi];
                if (!sp->active || sp->currentRadius < 20.0f) continue;
                float ratio = sp->currentRadius / sp->maxRadius;
                if (ratio < 0.95f) {
                    float particleAlpha = (1.0f - ratio) * 0.6f;
                    for (int pd = 0; pd < 8; pd++) {
                        float ang = (float)(pd * 45) * DEG2RAD + game->elapsedTime * 1.5f;
                        float off = sinf(game->elapsedTime * 3.0f + (float)pd) * 8.0f;
                        float px = sp->originX + cosf(ang) * (sp->currentRadius + off);
                        float py = sp->originY + sinf(ang) * (sp->currentRadius + off);
                        DrawCircleV((Vector2){ px, py }, 1.5f,
                            Fade((Color){ 200, 230, 255, 255 }, 0.08f * particleAlpha));
                    }
                }
            }
        }

        /* #6: Ceiling Pipes — procedural thin lines above rooms. */
        if (game->state == GAME_STATE_PLAYING) {
            for (int ri = 0; ri < game->station.roomCount; ri++) {
                const Room *pr = &game->station.rooms[ri];
                /* Ensure we don't draw outside the world view. */
                Vector2 rp = GetWorldToScreen2D((Vector2){ pr->x, pr->y }, game->camera);
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                if (rp.x < -200 || rp.x > sw + 200 || rp.y < -200 || rp.y > sh + 200) continue;
                /* Draw 2-3 thin horizontal lines near the top of each room. */
                int nPipes = (ri % 2) + 2;
                for (int pi = 0; pi < nPipes; pi++) {
                    float pipeY = pr->y + 6.0f + (float)pi * 5.0f;
                    DrawLineEx((Vector2){ pr->x + 4.0f, pipeY },
                               (Vector2){ pr->x + pr->w - 4.0f, pipeY },
                               1.0f, Fade((Color){ 60, 65, 70, 255 }, 0.20f));
                }
                /* Occasional steam puff (1-2 per room). */
                for (int s = 0; s < 2; s++) {
                    float phase = sinf(game->elapsedTime * 1.5f + (float)(ri * 5 + s * 3)) * 0.5f + 0.5f;
                    if (phase > 0.85f) {
                        float sx = pr->x + 8.0f + (float)s * pr->w * 0.4f;
                        float sy = pr->y + 4.0f + phase * 8.0f;
                        DrawCircleV((Vector2){ sx, sy }, 2.0f + phase * 2.0f,
                            Fade((Color){ 180, 190, 200, 255 }, 0.04f * (phase - 0.85f) * 6.0f));
                    }
                }
                /* #7: Floor Numbers — tiny painted text in bottom corner. */
                {
                    static const char *FLOOR_CODES[] = {
                        "B12", "A07", "HX4", "C09", "D03", "E11", "F08", "G02"
                    };
                    int fIdx = (game->stationSeed + (uint64_t)ri) % 8;
                    float fnX = pr->x + pr->w - 22.0f;
                    float fnY = pr->y + pr->h - 10.0f;
                    DrawText(FLOOR_CODES[fIdx], (int)fnX, (int)fnY, 7,
                        Fade((Color){ 100, 105, 110, 255 }, 0.15f));
                }
            }
        }

        /* #15: Broken Monitor — ERROR flicker in a random room. */
        if (game->monitorRoomIdx > 0 && game->monitorRoomIdx < game->station.roomCount &&
            game->state == GAME_STATE_PLAYING) {
            const Room *mr = &game->station.rooms[game->monitorRoomIdx];
            float monX = mr->x + mr->w * 0.5f - 12.0f;
            float monY = mr->y + mr->h * 0.5f - 8.0f;
            float mBlink = sinf(game->elapsedTime * 7.0f + (float)game->monitorRoomIdx) * 0.5f + 0.5f;
            if (mBlink > 0.6f) {
                DrawRectangle((int)monX, (int)monY, 24, 10,
                    Fade((Color){ 150, 150, 180, 255 }, 0.3f * (mBlink - 0.6f) * 2.5f));
                DrawText("ERROR", (int)(monX + 2), (int)(monY + 1), 7,
                    Fade((Color){ 200, 50, 50, 255 }, 0.6f * (mBlink - 0.6f) * 2.5f));
            }
        }

        /* Relay room orange glow — hints the player where to go.
         * Triggered when a sonar pulse "reflects" off the relay room.
         * Drawn inside the world pass so the darkness shader covers it. */
        if (game->relayGlowTimer > 0.0f && game->relaysActivated < 1) {
            int ri = game->station.relayRoomIdxs[0];
            if (ri > 0 && ri < game->station.roomCount) {
                const Room *r = &game->station.rooms[ri];
                float glow = fminf(game->relayGlowTimer / 2.5f, 1.0f);
                float pulse = sinf(game->elapsedTime * 3.0f) * 0.25f + 0.75f;
                Color wallGlow = Fade((Color){ 255, 150, 50, 255 }, glow * 0.55f * pulse);
                Color fillGlow = Fade((Color){ 255, 120, 30, 255 }, glow * 0.04f * pulse);
                BeginBlendMode(BLEND_ADDITIVE);
                DrawRectangleRec((Rectangle){ r->x + 2.0f, r->y + 2.0f,
                                              r->w - 4.0f, r->h - 4.0f }, fillGlow);
                DrawRectangleLinesEx((Rectangle){ r->x, r->y, r->w, r->h }, 3.0f, wallGlow);
                EndBlendMode();
            }

            /* #7: False Relay Signals — when the real relay glows,
             * also draw a faint orange glow on a random non-relay room.
             * Makes the player question every echo. */
            if (game->falseRelayRoomIdx < 0) {
                /* Pick a random non-relay room on first glow. */
                int nonRelayCandidates[MAX_ROOMS];
                int nrcCount = 0;
                for (int cri = 1; cri < game->station.roomCount; cri++) {
                    if (cri == ri) continue; /* skip actual relay */
                    bool isRelay = false;
                    for (int rr = 0; rr < 3; rr++) {
                        if (game->station.relayRoomIdxs[rr] == cri) { isRelay = true; break; }
                    }
                    if (!isRelay) nonRelayCandidates[nrcCount++] = cri;
                }
                if (nrcCount > 0) {
                    game->falseRelayRoomIdx = nonRelayCandidates[GetRandomValue(0, nrcCount - 1)];
                    /* Achievement: "False Signal" — follow a ghost relay. */
                    if (!game->runHadFalseRelay) {
                        game->runHadFalseRelay = true;
                        AchievementUnlock(&game->achievements, ACH_FALSE_RELAY, game->elapsedTime);
                    }
                }
                game->falseRelayTimer = 0.8f;
            }
            if (game->falseRelayRoomIdx > 0 && game->falseRelayTimer > 0.0f) {
                int fri = game->falseRelayRoomIdx;
                if (fri < game->station.roomCount) {
                    const Room *fr = &game->station.rooms[fri];
                    float fGlow = fminf(game->falseRelayTimer / 0.8f, 1.0f);
                    float fPulse = sinf(game->elapsedTime * 4.0f) * 0.3f + 0.7f;
                    Color fWallGlow = Fade((Color){ 255, 120, 30, 255 }, fGlow * 0.20f * fPulse);
                    Color fFillGlow = Fade((Color){ 255, 100, 20, 255 }, fGlow * 0.015f * fPulse);
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawRectangleRec((Rectangle){ fr->x + 2.0f, fr->y + 2.0f,
                                                  fr->w - 4.0f, fr->h - 4.0f }, fFillGlow);
                    DrawRectangleLinesEx((Rectangle){ fr->x, fr->y, fr->w, fr->h }, 2.0f, fWallGlow);
                    EndBlendMode();
                }
                game->falseRelayTimer -= game->deltaTime * 0.5f;
                if (game->falseRelayTimer < 0.0f) game->falseRelayTimer = 0.0f;
            }
        }

        /* Relay console: blinking orange when inactive, solid green when active. */
        {
            int ri = game->station.relayRoomIdxs[0];
            if (ri > 0 && ri < game->station.roomCount) {
                const Room *r = &game->station.rooms[ri];
                float cx = r->x + r->w * 0.5f;
                float cy = r->y + r->h * 0.5f;
                
                if (game->relaysActivated >= 1) {
                    /* Activated: solid green console with steady glow. */
                    float pulse = sinf(game->elapsedTime * 2.0f) * 0.15f + 0.85f;
                    Rectangle console = { cx - 14.0f, cy - 18.0f, 28.0f, 36.0f };
                    DrawRectangleRec(console, (Color){ 10, 30, 15, 255 });
                    DrawRectangleLinesEx(console, 1.5f, (Color){ 50, 255, 100, 255 });
                    /* Steady green screen. */
                    DrawRectangleRec((Rectangle){ cx - 8.0f, cy - 12.0f, 16.0f, 12.0f },
                        Fade((Color){ 50, 255, 100, 255 }, 0.6f * pulse));
                    /* Small green dot on top. */
                    DrawCircleV((Vector2){ cx, cy - 16.0f }, 2.0f,
                        Fade((Color){ 100, 255, 150, 255 }, 0.5f * pulse));
                    /* #5: Terminal Numbers — atmosphere text on activated console. */
                    {
                        static const char *TL[] = { "NODE 14", "SECTOR 07", "TEMP 281K", "STATUS ONLINE" };
                        int tIdx = game->terminalMottoIdx % 4;
                        DrawText(TL[tIdx], (int)(cx - 6.0f), (int)(cy - 10.0f), 8,
                            Fade((Color){ 50, 255, 100, 255 }, 0.3f * pulse));
                    }
                } else {
                float blink = sinf(game->elapsedTime * 4.0f) * 0.5f + 0.5f;
                /* Console panel. */
                Rectangle console = { cx - 14.0f, cy - 18.0f, 28.0f, 36.0f };
                DrawRectangleRec(console, (Color){ 30, 20, 10, 255 });
                DrawRectangleLinesEx(console, 1.5f, (Color){ 255, 160, 50, 255 });
                /* Blinking screen. */
                if (blink > 0.4f) {
                    DrawRectangleRec((Rectangle){ cx - 8.0f, cy - 12.0f, 16.0f, 12.0f },
                        Fade((Color){ 255, 120, 20, 255 }, (blink - 0.4f) * 1.5f));
                }                /* #5: Relay activation spark burst — orange particles burst from console. */
                if (game->relayActivationProgress < 0.0f && game->state == GAME_STATE_PLAYING) {
                    float burstTime = -game->relayActivationProgress;
                    if (burstTime < 0.6f) {
                        float intensity = (0.6f - burstTime) / 0.6f;
                        for (int s = 0; s < 6; s++) {
                            float ang = (float)(s * 60) * DEG2RAD + game->elapsedTime * 2.0f;
                            float dist = burstTime * 60.0f + (float)s * 4.0f;
                            float sx = cx + cosf(ang) * dist;
                            float sy = cy + sinf(ang) * dist;
                            DrawCircleV((Vector2){ sx, sy }, 2.0f + intensity * 2.0f,
                                Fade((Color){ 255, 180, 50, 255 }, intensity * 0.6f));
                        }
                    }
                }

                /* #5: Security Cameras — red blinking LED dots on revealed rooms. */
        if (game->state == GAME_STATE_PLAYING && game->firstPingDone) {
            for (int ri = 0; ri < game->station.roomCount; ri++) {
                if (ri == game->station.startRoomIdx) continue;
                const Room *rr = &game->station.rooms[ri];
                float cx = rr->x + rr->w * 0.5f;
                float cy = rr->y + 10.0f;
                Vector2 csp = GetWorldToScreen2D((Vector2){ cx, cy }, game->camera);
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                if (csp.x >= 0 && csp.x <= sw && csp.y >= 0 && csp.y <= sh) {
                    float camBlink = sinf(game->elapsedTime * 2.0f + (float)ri * 1.7f) * 0.5f + 0.5f;
                    if (camBlink > 0.3f) {
                        DrawCircleV((Vector2){ cx, cy }, 3.0f,
                            Fade((Color){ 200, 30, 30, 255 }, (camBlink - 0.3f) * 1.5f * 0.6f));
                        DrawCircleV((Vector2){ cx, cy }, 1.0f,
                            Fade((Color){ 255, 80, 80, 255 }, (camBlink - 0.3f) * 1.5f));
                    }
                }
            }
        }
        /* #5: Terminal Numbers — atmosphere text on unactivated console. */
                {
                    static const char *TL[] = { "NODE 14", "SECTOR 07", "TEMP 281K", "STATUS UNKNOWN" };
                    int tIdx = game->terminalMottoIdx % 4;
                    if (blink > 0.4f) {
                        DrawText(TL[tIdx], (int)(cx - 6.0f), (int)(cy - 10.0f), 8,
                            Fade((Color){ 255, 160, 50, 255 }, blink * 0.4f));
                    }
                }
                /* Tiny sparks (procedural dots near console). */
                for (int s = 0; s < 3; s++) {
                    float phase = sinf(game->elapsedTime * 5.0f + (float)s * 2.1f);
                    if (phase > 0.3f) {
                        float sx = cx + 18.0f + sinf(game->elapsedTime * 3.0f + (float)s) * 6.0f;
                        float sy = cy - 8.0f + cosf(game->elapsedTime * 4.0f + (float)s * 1.7f) * 10.0f;
                        DrawCircleV((Vector2){ sx, sy }, 1.5f,
                            Fade((Color){ 255, 200, 100, 255 }, phase * 0.6f));
                    }
                }
                /* "Activation progress bar" when player is holding E. */
                if (game->relayActivationProgress > 0.0f && !game->relaysActivated) {
                    float pw = 28.0f * fminf(game->relayActivationProgress, 1.0f);
                    DrawRectangleRec((Rectangle){ cx - 14.0f, cy + 20.0f, pw, 3.0f },
                        Fade((Color){ 100, 255, 100, 255 }, 0.8f));
                }
                /* "Hold E" hint near relay. */
                float distToPlayer = sqrtf((game->player.position.x - cx) * (game->player.position.x - cx) +
                                           (game->player.position.y - cy) * (game->player.position.y - cy));
                if (distToPlayer < 120.0f && !game->relaysActivated) {
                    const char *hint = game->relayActivationProgress > 0.0f ? "ACTIVATING..." : "Hold E to activate";
                    int hintW = MeasureText(hint, 12);
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawText(hint, (int)(cx - hintW / 2), (int)(cy + 28), 12,
                             Fade((Color){ 255, 200, 100, 255 }, 0.7f + blink * 0.3f));
                    EndBlendMode();
                }
                }
            }
        }

        PlayerDraw(&game->player);

        /* Sonar pulse ring: multi-layer glow with thickness animation. */
        for (int i = 0; i < game->sonar.pulseCount; i++) {
            const SonarPulse *sp = &game->sonar.pulses[i];
            if (!sp->active) continue;
            float strength = SonarPulseRevealStrength(sp);
            if (strength <= 0.0f) continue;

            Vector2 center = { sp->originX, sp->originY };
            float r = sp->currentRadius;

            /* Animate ring thickness: thick when close, thin when far.
             * Starts ~5px, shrinks to ~1.5px as the ring expands. */
            float expandRatio = (sp->maxRadius > 0.0f)
                              ? fminf(r / sp->maxRadius, 1.0f)
                              : 0.0f;
            float thickness = 5.0f - expandRatio * 3.5f;

            BeginBlendMode(BLEND_ADDITIVE);

            /* Layer 1 — outer aura: wide, soft, trailing. */
            DrawCircleV(center, r + 12.0f,
                        Fade((Color){ 70, 160, 255, 255 }, 0.06f * strength));

            /* Layer 2 — mid glow: medium radius, brighter. */
            DrawCircleV(center, r + 6.0f,
                        Fade((Color){ 100, 200, 255, 255 }, 0.14f * strength));

            /* Layer 3 — inner glow: tight around the ring, warm cyan. */
            DrawCircleV(center, r + 2.0f,
                        Fade((Color){ 160, 230, 255, 255 }, 0.30f * strength));

            EndBlendMode();

            /* Ring with variable thickness via DrawRing (raylib 5.0+).
             * Thick when the pulse is young, thin when fully expanded. */
            float halfThick = thickness * 0.5f;
            /* #6: Ring Distortion — sin modulation makes the ring feel organic. */
            float distort = 1.0f + sinf(game->elapsedTime * 120.0f + expandRatio * 6.28f) * 0.015f;
            float dHalf = halfThick * distort;
            DrawRing(center, r - dHalf, r + dHalf, 0.0f, 360.0f, 0,
                     Fade((Color){ 210, 245, 255, 255 }, 0.90f * strength));

            /* Trailing wake: a faint ring behind the wavefront that
             * recedes as the wave slows — reinforces radial motion. */
            if (expandRatio < 0.85f) {
                float trailStrength = strength * 0.20f * (1.0f - expandRatio);
                float trailR = r - 6.0f + expandRatio * 4.0f;
                DrawRing(center, trailR - 1.0f, trailR + 1.0f, 0.0f, 360.0f, 0,
                         Fade((Color){ 100, 190, 255, 255 }, trailStrength));
            }

            /* Sonar wave ripple — a second faint ring at 0.9x radius with 0.3x alpha. */
            float rippleR = r * 0.90f;
            float rippleAlpha = 0.30f * strength * (1.0f - expandRatio * 0.5f);
            DrawRing(center, rippleR - 0.5f, rippleR + 0.5f, 0.0f, 360.0f, 0,
                     Fade((Color){ 150, 200, 255, 255 }, rippleAlpha));
        }

        /* Enemies. */
        EnemyManagerDraw(&game->enemies);

        /* Airlock marker: opens during escape sequence. */
        {
            Rectangle ar = { game->airlockX - 12.0f, game->airlockY - 20.0f, 24.0f, 40.0f };
            
            if (game->escapeDoorOpen) {
                /* Door sliding open — outer frame + bright white interior. */
                float openT = 1.0f - game->escapeWhiteTimer / 2.5f;
                float slide = fminf(openT * 3.0f, 1.0f) * 10.0f; /* slides apart */
                DrawRectangleLinesEx((Rectangle){ ar.x - 2.0f, ar.y - 2.0f, ar.width + 4.0f, ar.height + 4.0f },
                                     2.0f, (Color){ 100, 255, 150, 255 });
                /* Inner bright light. */
                DrawRectangleRec((Rectangle){ ar.x + slide, ar.y + 2.0f, ar.width - slide * 2.0f, ar.height - 4.0f },
                                 Fade((Color){ 255, 255, 255, 255 }, fminf(openT * 2.0f, 1.0f)));
            } else {
            DrawRectangleRec(ar, (Color){ 20, 50, 30, 255 });
            DrawRectangleLinesEx(ar, 2.0f, (Color){ 50, 255, 100, 255 });

            /* Beacon glow — soft green ring that pulses outward. */
            float beaconPulse = sinf(game->elapsedTime * 1.8f) * 0.5f + 0.5f;
            float glowRadius = 30.0f + beaconPulse * 20.0f;
            Vector2 bc = { game->airlockX, game->airlockY + 6.0f };
            DrawRing(bc, glowRadius - 2.0f, glowRadius + 2.0f, 0.0f, 360.0f, 0,
                     Fade((Color){ 50, 255, 100, 255 }, 0.12f * (1.0f - beaconPulse * 0.5f)));

            /* #7: Light Leak — tiny white flicker under the door (hope in the dark). */
            if (game->firstPingDone) {
                float leak = sinf(game->elapsedTime * 3.0f) * 0.5f + 0.5f;
                if (leak > 0.6f) {
                    DrawRectangleRec((Rectangle){ game->airlockX - 4.0f, game->airlockY + 18.0f, 8.0f, 2.0f },
                        Fade((Color){ 255, 255, 200, 255 }, (leak - 0.6f) * 1.5f * 0.3f));
                }
            }
            }
        }

    EndMode2D();
    RendererEndWorld(&game->renderer);

    /* --- Pass 2: darkness composite --- */
    ClearBackground(BLACK);
    Vector2 playerScreenPos = GetWorldToScreen2D(game->player.position, game->camera);
    /* #12: Panic Vision — dynamic glow radius shrinks when Hunter is close. */
    float visRadius = ECHO_PLAYER_VISIBILITY_RADIUS;
    if (game->nearestEnemyDist < 150.0f) {
        visRadius *= game->nearestEnemyDist / 150.0f;
        if (visRadius < 25.0f) visRadius = 25.0f;
    }
    RendererDrawDarkness(&game->renderer, playerScreenPos, visRadius, game->elapsedTime);

    /* --- Pass 3: Echo Memory screen-space overlay --- */
    {
        const float cellSize = (float)ECHO_MEMORY_CELL_SIZE;
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < game->memory.count; i++) {
            const EchoMemoryCell *cell = &game->memory.cells[i];
            if (!cell->active) continue;
            float age = game->elapsedTime - cell->revealTime;
            Color color;
            float alpha = EchoMemoryGetCellColor(age, &color);
            if (alpha <= 0.0f) continue;

            float wx = (float)(cell->cellX) * cellSize;
            float wy = (float)(cell->cellY) * cellSize;
            Vector2 tl = GetWorldToScreen2D((Vector2){ wx, wy }, game->camera);
            Vector2 tr = GetWorldToScreen2D((Vector2){ wx + cellSize, wy }, game->camera);
            Vector2 br = GetWorldToScreen2D((Vector2){ wx + cellSize, wy + cellSize }, game->camera);
            Vector2 bl = GetWorldToScreen2D((Vector2){ wx, wy + cellSize }, game->camera);

            float screenAlpha = (float)color.a / 255.0f;
            screenAlpha *= (age < ECHO_MEMORY_TRANSITION_TIME) ? 0.85f : 0.65f;
            color.a = (unsigned char)(screenAlpha * 255.0f);
            if (color.a <= 0) continue;

            /* Single 1.5px line per edge — thinner = less visual clutter. */
            DrawLineEx(tl, tr, 1.5f, color);
            DrawLineEx(tr, br, 1.5f, color);
            DrawLineEx(br, bl, 1.5f, color);
            DrawLineEx(bl, tl, 1.5f, color);
        }
        EndBlendMode();
        EchoMemoryCompact(&game->memory, game->elapsedTime);
    }

    /* --- Heartbeat: dual pulsing rings when enemies are close --- */
    if (game->state == GAME_STATE_PLAYING)
    {
        float nd = game->nearestEnemyDist;
        float intensity = (nd < 220.0f) ? (1.0f - nd / 220.0f) * 0.6f : 0.0f;
        if (intensity > 0.01f) {
            float pulse = sinf(game->elapsedTime * 9.42f) * 0.5f + 0.5f;
            float antiPulse = 1.0f - pulse;
            float alpha = intensity * (0.2f + pulse * 0.3f);
        int sw2 = GetScreenWidth() / 2, sh2 = GetScreenHeight() / 2;
        /* Outer ring (contralateral pulse — creates a "double beat" feel). */
        DrawCircleLines(sw2, sh2,
                        120.0f + antiPulse * 20.0f,
                        Fade((Color){ 180, 40, 40, 255 }, alpha * 0.35f));
            /* Inner/main ring. */
        DrawCircleLines(sw2, sh2,
                        80.0f + pulse * 30.0f,
                        Fade((Color){ 200, 60, 60, 255 }, alpha));
        }
    }

    /* --- Airlock escape sequence: alarm lights + countdown --- */
    if (game->airlockSequence && game->state == GAME_STATE_PLAYING)
    {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        /* Flashing red border (alarm lights). */
        float alarm = sinf(game->elapsedTime * 8.0f) * 0.5f + 0.5f;
        float borderW = 8.0f + alarm * 4.0f;
        Color alarmColor = (Color){ 200, 30, 20, (unsigned char)(alarm * 200.0f) };
        DrawRectangle(0, 0, sw, (int)borderW, alarmColor);
        DrawRectangle(0, sh - (int)borderW, sw, (int)borderW, alarmColor);
        DrawRectangle(0, 0, (int)borderW, sh, alarmColor);
        DrawRectangle(sw - (int)borderW, 0, (int)borderW, sh, alarmColor);

        /* Countdown text. */
        int secs = (int)ceilf(game->airlockCountdown);
        if (secs > 0) {
            const char *cd = TextFormat("DOOR OPENS IN %d", secs);
            DrawText(cd, (sw - MeasureText(cd, 28)) / 2, sh / 2 - 60, 28,
                     Fade((Color){ 255, 60, 40, 255 }, 0.7f + alarm * 0.3f));
        }
        /* "HOLD POSITION" prompt. */
        const char *hold = "HOLD POSITION";
        DrawText(hold, (sw - MeasureText(hold, 16)) / 2, sh / 2 - 20, 16,
                 Fade(RAYWHITE, 0.5f + alarm * 0.3f));
    }

    /* --- Airlock compass (golden direction hint after first ping) --- */
    if (game->firstPingDone && game->state == GAME_STATE_PLAYING)
    {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        Vector2 as = GetWorldToScreen2D((Vector2){ game->airlockX, game->airlockY },
                                        game->camera);
        float margin = 50.0f;

        /* #3: Corrupted Compass — jitter when Hunter is very close (panic). */
        if (game->nearestEnemyDist < 120.0f && game->state == GAME_STATE_PLAYING) {
            float jit = (1.0f - game->nearestEnemyDist / 120.0f) * 10.0f;
            as.x += (float)GetRandomValue(-1000, 1000) / 1000.0f * jit;
            as.y += (float)GetRandomValue(-1000, 1000) / 1000.0f * jit;
        }

        bool onScreen = (as.x >= 0 && as.x <= sw && as.y >= 0 && as.y <= sh);

        float pulse = sinf(game->elapsedTime * 2.0f) * 0.15f + 0.65f;
        Color gold = (Color){ 255, 210, 80, (unsigned char)(pulse * 255.0f) };

        if (onScreen)
        {
            /* Subtle golden circle at airlock screen position. */
            DrawCircleV(as, 4.0f, Fade(gold, 0.5f));
            DrawCircleLines((int)as.x, (int)as.y, 4.0f, Fade(gold, 0.25f));
        }
        else
        {
            /* Off screen: golden dot at screen edge pointing toward airlock. */
            float angle = atan2f(as.y - sh / 2.0f, as.x - sw / 2.0f);
            float ex = sw / 2.0f + cosf(angle) * (sw / 2.0f - margin);
            float ey = sh / 2.0f + sinf(angle) * (sh / 2.0f - margin);
            ex = fmaxf(margin, fminf((float)sw - margin, ex));
            ey = fmaxf(margin, fminf((float)sh - margin, ey));

            float a1 = angle + 2.356f; /* 135 degrees */
            float a2 = angle - 2.356f;
            float asz = 7.0f;
            Vector2 c = { ex, ey };
            DrawCircleV(c, 3.0f, Fade(gold, 0.6f));
            DrawLineEx(c, (Vector2){ ex + cosf(a1) * asz, ey + sinf(a1) * asz },
                       2.0f, Fade(gold, 0.6f));
            DrawLineEx(c, (Vector2){ ex + cosf(a2) * asz, ey + sinf(a2) * asz },
                       2.0f, Fade(gold, 0.6f));
        }
    }

    /* --- Hunter proximity edge glow --- */
    if (game->state == GAME_STATE_PLAYING)
    {
        for (int i = 0; i < game->enemies.count; i++)
        {
            const Enemy *e = &game->enemies.enemies[i];
            if (!e->alive || e->type != ENEMY_TYPE_HUNTER) continue;
            if (e->state != ENEMY_STATE_ALERT && e->state != ENEMY_STATE_RUSH) continue;

            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float cx = (float)sw / 2.0f, cy = (float)sh / 2.0f;
            Vector2 hs = GetWorldToScreen2D((Vector2){ e->x, e->y }, game->camera);
            float dx = hs.x - cx, dy = hs.y - cy;
            float angle = atan2f(dy, dx);

            /* Edge position in the hunter's direction. */
            float ex = cx + cosf(angle) * cx;
            float ey = cy + sinf(angle) * cy;
            ex = fmaxf(0.0f, fminf((float)sw, ex));
            ey = fmaxf(0.0f, fminf((float)sh, ey));

            float alarmPulse = sinf(game->elapsedTime * 4.0f) * 0.25f + 0.75f;
            float intensity = (e->state == ENEMY_STATE_RUSH) ? 0.30f : 0.15f;
            float radius = (e->state == ENEMY_STATE_RUSH) ? 80.0f : 50.0f;
            Color color = (e->state == ENEMY_STATE_RUSH)
                        ? (Color){ 255, 50, 50, (unsigned char)(intensity * alarmPulse * 255.0f) }
                        : (Color){ 200, 80, 30, (unsigned char)(intensity * alarmPulse * 255.0f) };

            BeginBlendMode(BLEND_ADDITIVE);
            for (int layer = 4; layer >= 1; layer--)
            {
                float t = (float)layer / 4.0f;
                DrawCircleV((Vector2){ ex, ey }, radius * t,
                            Fade(color, (0.15f * (1.0f - t) + 0.02f) / t));
            }
            EndBlendMode();
        }
    }

    /* --- Sonar cooldown ring (bottom-right) --- */
    if (game->state == GAME_STATE_PLAYING)
    {
        float cd = game->sonar.cooldownTimer / ECHO_SONAR_COOLDOWN;
        if (cd > 0.0f) {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float cx = (float)sw - 50.0f, cy = (float)sh - 50.0f;
            float r = 14.0f;
            float remainingAngle = (1.0f - cd) * 360.0f;

            /* Background ring (faint). */
            DrawRing((Vector2){ cx, cy }, r - 2.0f, r + 2.0f, 0.0f, 360.0f, 0,
                     Fade((Color){ 100, 180, 255, 255 }, 0.10f));

            /* Fill ring (shows remaining cooldown). */
            if (remainingAngle > 0.0f) {
                float startA = -90.0f;
                DrawRing((Vector2){ cx, cy }, r - 2.0f, r + 2.0f,
                         startA, startA + remainingAngle, 0,
                         Fade((Color){ 100, 200, 255, 255 }, 0.50f));
            }

            /* Small center dot — pulses brighter as cooldown nears end. */
            float dotAlpha = 0.20f + 0.30f * (1.0f - cd);
            DrawCircleV((Vector2){ cx, cy }, 2.5f,
                        Fade((Color){ 130, 210, 255, 255 }, dotAlpha));
        }
    }

    /* --- #12: Moving Shadows — one-frame CRT shadow illusion near screen edge. --- */
    if (game->state == GAME_STATE_PLAYING && game->firstPingDone &&
        game->shadowX != 0.0f && game->shadowY != 0.0f &&
        (game->flickerType == 1 || game->flickerType == 8)) {
        DrawCircleV((Vector2){ game->shadowX, game->shadowY },
                    18.0f, Fade((Color){ 80, 40, 40, 255 }, 0.12f));
        game->shadowX = 0.0f; /* one frame only */
        game->shadowY = 0.0f;
    }

    /* --- #14: Radio Burst overlay --- */
    if (game->radioDisplay > 0.0f && game->radioText[0] != '\0') {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float rA = fminf(game->radioDisplay * 0.8f, 0.5f);
        float rStatic = sinf(game->elapsedTime * 30.0f) * 0.1f;
        DrawText(game->radioText, (sw - MeasureText(game->radioText, 16)) / 2,
                 sh / 2 + 135, 16,
                 Fade((Color){ 200, 180, 200, 255 }, rA + rStatic));
    }

    /* --- #4: Room entry colour pulse — brief colour wash on entering new room --- */
    if (game->roomNameTimer > 2.5f && game->roomName[0] != '\0' && game->state == GAME_STATE_PLAYING) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float pulseA = (game->roomNameTimer - 2.5f) / 0.5f;
        DrawRectangle(0, 0, sw, sh,
            Fade((Color){ 80, 120, 180, 255 }, pulseA * 0.03f));
    }

    /* --- #3: Sonar charge-up ring — brief pulsing ring on SPACE press --- */
    if (game->cameraZoomTimer > 0.20f && game->state == GAME_STATE_PLAYING) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float charge = (game->cameraZoomTimer - 0.25f) / 0.15f;
        if (charge > 1.0f) charge = 1.0f;
        float r = 20.0f + charge * 30.0f;
        DrawRing((Vector2){ sw / 2.0f, sh / 2.0f }, r - 1.0f, r + 1.0f,
                 0.0f, 360.0f * charge, 0,
                 Fade((Color){ 180, 220, 255, 255 }, 0.20f * (1.0f - charge)));
    }

    /* --- #18: Dynamic Room Name overlay --- */
    if (game->roomNameTimer > 0.0f && game->roomName[0] != '\0') {
        float rA = fminf(game->roomNameTimer * 0.8f, 0.4f);
        DrawText(game->roomName, 16, 4, 10,
                 Fade((Color){ 120, 160, 200, 255 }, rA));
    }

    /* --- #22: Hidden Observation Room overlay --- */
    if (game->hiddenRevealTimer > 0.0f && game->hiddenRoomIdx > 0) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float hA = fminf(game->hiddenRevealTimer * 0.5f, 0.7f);
        float hPulse = sinf(game->elapsedTime * 3.0f) * 0.2f + 0.8f;
        DrawText("Behind the glass... a silhouette.",
                 (sw - MeasureText("Behind the glass... a silhouette.", 14)) / 2,
                 sh / 2 + 50, 14,
                 Fade((Color){ 150, 140, 180, 255 }, hA * hPulse * 0.5f));
    }

    /* --- #11: Dust Floating — tiny particles only visible inside sonar zone. --- */
    if (game->state == GAME_STATE_PLAYING && game->firstPingDone) {
        int sw = GetScreenWidth();
        for (int d = 0; d < 12; d++) {
            float px = fmodf(game->elapsedTime * 30.0f + (float)d * 47.0f, (float)sw);
            float py = fmodf(game->elapsedTime * 20.0f + (float)d * 91.0f, 60.0f) + 20.0f;
            float dp = sinf(game->elapsedTime * 1.5f + (float)d) * 0.3f + 0.7f;
            DrawCircleV((Vector2){ px, py }, 0.8f,
                Fade((Color){ 200, 220, 255, 255 }, 0.04f * dp));
        }
    }

    /* --- #9: Echo Trails — tiny particles trailing each sonar pulse ring. --- */
    if (game->state == GAME_STATE_PLAYING && game->sonar.pulseCount > 0) {
        for (int p = 0; p < game->sonar.pulseCount && p < 3; p++) {
            const SonarPulse *sp = &game->sonar.pulses[p];
            if (!sp->active || sp->currentRadius < 10.0f) continue;
            Vector2 origin = GetWorldToScreen2D((Vector2){ sp->originX, sp->originY }, game->camera);
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            if (origin.x < -50 || origin.x > sw + 50 || origin.y < -50 || origin.y > sh + 50) continue;
            float ratio = sp->currentRadius / sp->maxRadius;
            if (ratio > 0.05f && ratio < 0.95f) {
                for (int t = 0; t < 4; t++) {
                    float ang = sinf(game->elapsedTime * 2.0f + (float)(p * 7 + t * 3)) * 0.5f + 0.5f;
                    float rOff = sp->currentRadius * (0.8f + ang * 0.4f);
                    float tAngle = (float)(t * 90) * DEG2RAD;
                    float tx = sp->originX + cosf(tAngle + game->elapsedTime * 0.5f) * rOff;
                    float ty = sp->originY + sinf(tAngle + game->elapsedTime * 0.5f) * rOff;
                    Vector2 td = GetWorldToScreen2D((Vector2){ tx, ty }, game->camera);
                    if (td.x >= 0 && td.x <= sw && td.y >= 0 && td.y <= sh) {
                        DrawCircleV(td, 1.5f,
                            Fade((Color){ 150, 200, 255, 255 }, 0.12f * (1.0f - ratio)));
                    }
                }
            }
        }
    }

    /* --- #10: Player Shadow — long shadow cast opposite nearest wall. --- */
    if (game->state == GAME_STATE_PLAYING && game->firstPingDone) {
        /* Cast shadow in direction away from nearest wall (invert movement vector). */
        float mvLen = sqrtf(game->player.velocity.x * game->player.velocity.x +
                            game->player.velocity.y * game->player.velocity.y);
        if (mvLen > 0.5f) {
            float nx = -game->player.velocity.x / mvLen;
            float ny = -game->player.velocity.y / mvLen;
            float shadowLen = fminf(mvLen / 50.0f, 1.0f) * 50.0f;
            float sx = game->player.position.x + nx * shadowLen;
            float sy = game->player.position.y + ny * shadowLen;
            Vector2 sd = GetWorldToScreen2D((Vector2){ sx, sy }, game->camera);
            Vector2 pd = GetWorldToScreen2D(game->player.position, game->camera);
            DrawLineEx(pd, sd, 3.0f, Fade((Color){ 30, 30, 50, 255 }, 0.15f * (1.0f - mvLen / 150.0f)));
            DrawCircleV(sd, 4.0f, Fade((Color){ 20, 20, 40, 255 }, 0.10f * (1.0f - mvLen / 150.0f)));
        }
    }

    /* --- #8: Ceiling Dust — falling particles when Hunter rushes nearby. --- */
    if (game->state == GAME_STATE_PLAYING) {
        bool hunterRushing = false;
        for (int i = 0; i < game->enemies.count; i++) {
            const Enemy *e = &game->enemies.enemies[i];
            if (!e->alive || e->type != ENEMY_TYPE_HUNTER) continue;
            if (e->state == ENEMY_STATE_RUSH) {
                float dx = e->x - game->player.position.x;
                float dy = e->y - game->player.position.y;
                if (dx * dx + dy * dy < 40000.0f) { /* within ~200px */
                    hunterRushing = true;
                    break;
                }
            }
        }
        if (hunterRushing) {
            int sw = GetScreenWidth();
            for (int d = 0; d < 6; d++) {
                float dx = (float)((game->elapsedTime * 80.0f + (float)d * 73.0f) * 0.1f);
                dx = fmodf(dx, 1.0f) * (float)sw;
                float dy = fmodf(game->elapsedTime * 60.0f + (float)d * 29.0f, 1.0f) * 60.0f;
                DrawCircleV((Vector2){ dx, dy }, 1.5f,
                    Fade((Color){ 180, 170, 150, 255 }, 0.15f * (1.0f - dy / 60.0f)));
            }
        }
    }



    /* --- #18: Loose Cable Sparks — zzzt flash from the cable spark position. --- */
    if (game->cableSparkTimer <= 1.5f && game->cableSparkTimer > 0.0f &&
        game->state == GAME_STATE_PLAYING && game->firstPingDone) {
        float sparkIntensity = (1.5f - game->cableSparkTimer) / 1.5f;
        float zzzt = sinf(game->elapsedTime * 50.0f) * 0.5f + 0.5f;
        if (zzzt > 0.5f) {
            Vector2 sparkPos = GetWorldToScreen2D(
                (Vector2){ game->cableSparkX, game->cableSparkY }, game->camera);
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            if (sparkPos.x >= 0 && sparkPos.x <= sw && sparkPos.y >= 0 && sparkPos.y <= sh) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCircleV((Vector2){ game->cableSparkX, game->cableSparkY },
                    15.0f + zzzt * 10.0f,
                    Fade((Color){ 200, 220, 255, 255 }, 0.025f * sparkIntensity * zzzt));
                DrawCircleV((Vector2){ game->cableSparkX, game->cableSparkY },
                    2.0f,
                    Fade((Color){ 220, 240, 255, 255 }, 0.4f * sparkIntensity * zzzt));
                EndBlendMode();
            }
        }
    }

    /* --- Failing electronics: sparks illumination in world space. */
    if (game->state == GAME_STATE_PLAYING && game->flickerType == 1)
    {
        float intensity = game->flickerDuration / 2.0f;
        float sparkPulse = sinf(game->elapsedTime * 15.0f) * 0.5f + 0.5f;
        if (sparkPulse > 0.6f) {
            int ri = GetRandomValue(0, game->station.roomCount - 1);
            if (ri >= 0 && ri < game->station.roomCount) {
                const Room *r = &game->station.rooms[ri];
                float sx = r->x + (float)GetRandomValue(10, (int)r->w - 10);
                float sy = r->y + (float)GetRandomValue(10, (int)r->h - 10);
                Vector2 sparkPos = GetWorldToScreen2D((Vector2){ sx, sy }, game->camera);
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                if (sparkPos.x >= 0 && sparkPos.x <= sw && sparkPos.y >= 0 && sparkPos.y <= sh) {
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawCircleV((Vector2){ sx, sy }, 20.0f + sparkPulse * 15.0f,
                        Fade((Color){ 255, 200, 100, 255 }, 0.03f * intensity));
                    DrawCircleV((Vector2){ sx, sy }, 2.0f,
                        Fade((Color){ 255, 220, 150, 255 }, 0.5f * intensity * sparkPulse));
                    EndBlendMode();
                }
            }
        }
    }

    /* --- Death slow-mo sequence (before game-over screen) --- */
    if (game->deathSlowMo)
    {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        /* Growing red vignette that fills the screen. */
        float t = 1.0f - game->deathFlashTimer / 1.2f;
        float vigR = sqrtf((float)(sw * sw + sh * sh)) * 0.5f;
        DrawCircleGradient(sw / 2, sh / 2, vigR * (0.2f + t * 0.8f),
                           Fade((Color){ 180, 20, 20, 255 }, t * 0.40f), BLANK);
        /* Bright flash on first frame of death. */
        if (t < 0.15f) {
            float flash = 1.0f - t / 0.15f;
            DrawRectangle(0, 0, sw, sh, Fade(WHITE, flash * 0.3f));
        }
    }

    /* --- HUD (atmospheric, minimal, evolves over time) --- */
    if (game->state == GAME_STATE_GAME_OVER) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));
        /* Red vignette — blood-pooling edge effect on death. */
        int halfW = sw / 2, halfH = sh / 2;
        float vigR = sqrtf((float)(halfW * halfW + halfH * halfH));
        DrawCircleGradient(halfW, halfH, vigR * 0.3f, BLANK, Fade((Color){ 80, 0, 0, 255 }, 0.25f));
        int y0 = sh / 2 - 70;
        const char *go = "GAME OVER";
        DrawText(go, (sw - MeasureText(go, 34)) / 2, y0, 34, RED); y0 += 48;
        const char *caught = "You were caught.";
        DrawText(caught, (sw - MeasureText(caught, 18)) / 2, y0, 18, RAYWHITE); y0 += 34;
        int mins = (int)(game->runTimeDisplay / 60.0f);
        int secs = (int)game->runTimeDisplay % 60;
        const char *tm = TextFormat("Survived:  %02d:%02d", mins, secs);
        DrawText(tm, (sw - MeasureText(tm, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        const char *sc = TextFormat("Scans used: %d", game->scansUsed);
        DrawText(sc, (sw - MeasureText(sc, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        const char *al = TextFormat("Alerts: %d", game->alertsTriggered);
        DrawText(al, (sw - MeasureText(al, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        /* Rank on death screen too. */
        int mins2 = (int)(game->runTimeDisplay / 60.0f);
        int rankScore2 = game->scansUsed * 2 + game->alertsTriggered * 3 + mins2 * 2;
        const char *rankStr2;
        Color rankColor2;
        bool isOmega2 = (game->alertsTriggered == 0 && game->scansUsed <= 3 && mins2 < 2);
        if (isOmega2) { rankStr2 = "Ω ECHOLESS"; rankColor2 = (Color){ 200, 255, 255, 255 }; }
        else if (rankScore2 <= 6)  { rankStr2 = "RANK: S"; rankColor2 = (Color){ 255, 215, 50, 255 }; }
        else if (rankScore2 <= 14) { rankStr2 = "RANK: A"; rankColor2 = (Color){ 100, 255, 100, 255 }; }
        else if (rankScore2 <= 24) { rankStr2 = "RANK: B"; rankColor2 = (Color){ 100, 200, 255, 255 }; }
        else                     { rankStr2 = "RANK: C"; rankColor2 = (Color){ 180, 180, 180, 255 }; }
        DrawText(rankStr2, (sw - MeasureText(rankStr2, 22)) / 2, y0, 22, rankColor2); y0 += 36;
        /* #25: Final Black Screen — 3s black before game-over stats. */
        if (game->finalBlackTimer > 0.0f) {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, fminf(game->finalBlackTimer * 2.0f, 1.0f)));
        }

        const char *ex = "Press R to restart  |  ESC to exit";
        DrawText(ex, (sw - MeasureText(ex, 16)) / 2, y0, 16, GRAY);
    } else if (game->state == GAME_STATE_WON) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));
        /* Green glow — warm rising light on the escape screen. */
        float glowPulse = sinf(game->elapsedTime * 0.6f) * 0.15f + 0.85f;
        DrawCircleGradient(sw / 2, sh / 2, (float)fmax(sw, sh) * 0.4f * glowPulse,
                           Fade((Color){ 30, 80, 50, 255 }, 0.15f), BLANK);
        int y0 = sh / 2 - 90;
        const char *esc = "ESCAPED";
        DrawText(esc, (sw - MeasureText(esc, 40)) / 2, y0, 40, GREEN); y0 += 52;
        const char *msg = "You reached the airlock.";
        DrawText(msg, (sw - MeasureText(msg, 18)) / 2, y0, 18, RAYWHITE); y0 += 34;
        int mins = (int)(game->runTimeDisplay / 60.0f);
        int secs = (int)game->runTimeDisplay % 60;
        const char *tm = TextFormat("Time:  %02d:%02d", mins, secs);
        DrawText(tm, (sw - MeasureText(tm, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        const char *sc = TextFormat("Scans: %d", game->scansUsed);
        DrawText(sc, (sw - MeasureText(sc, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        const char *al = TextFormat("Alerts: %d", game->alertsTriggered);
        DrawText(al, (sw - MeasureText(al, 18)) / 2, y0, 18, LIGHTGRAY); y0 += 28;
        /* Rank. */
        int rankScore = game->scansUsed * 2 + game->alertsTriggered * 3 + mins * 2;
        const char *rankStr;
        Color rankColor;
        bool isOmega = (game->alertsTriggered == 0 && game->scansUsed <= 3 && mins < 2);
        if (isOmega) { rankStr = "Ω ECHOLESS"; rankColor = (Color){ 200, 255, 255, 255 }; }
        else if (rankScore <= 6)  { rankStr = "RANK: S"; rankColor = (Color){ 255, 215, 50, 255 }; }
        else if (rankScore <= 14) { rankStr = "RANK: A"; rankColor = (Color){ 100, 255, 100, 255 }; }
        else if (rankScore <= 24) { rankStr = "RANK: B"; rankColor = (Color){ 100, 200, 255, 255 }; }
        else                     { rankStr = "RANK: C"; rankColor = (Color){ 180, 180, 180, 255 }; }
        DrawText(rankStr, (sw - MeasureText(rankStr, 22)) / 2, y0, 22, rankColor); y0 += 36;
        const char *ex = "Press R to restart  |  ESC to exit";
        DrawText(ex, (sw - MeasureText(ex, 16)) / 2, y0, 16, GRAY);
    } else {
        int sw = GetScreenWidth(), sh = GetScreenHeight();

        /* Tutorial text — visible for 8s, then fades over 3s. */
        float hudAlpha = (game->hudFadeTimer > 3.0f) ? 1.0f
                        : (game->hudFadeTimer > 0.0f) ? (game->hudFadeTimer / 3.0f)
                        : 0.0f;
        if (hudAlpha > 0.01f) {
            DrawText("WASD to move  |  SPACE to ping",
                     (sw - MeasureText("WASD to move  |  SPACE to ping", 16)) / 2,
                     76, 16, Fade(GRAY, hudAlpha));
            DrawText("Find the relay room, then escape through the airlock.",
                     (sw - MeasureText("Find the relay room, then escape through the airlock.", 14)) / 2,
                     100, 14, Fade((Color){ 50, 255, 100, 255 }, hudAlpha));
        }

        /* First-ping flash label: "SONAR PING" appears for 1.5s after first use. */
        if (game->firstPingDone) {
            float pingAge = game->elapsedTime - game->firstPingTime;
            if (pingAge < 1.5f) {
                float fa = 1.0f - pingAge / 1.5f;
                DrawText("SONAR PING",
                         (sw - MeasureText("SONAR PING", 20)) / 2,
                         60, 20, Fade((Color){ 100, 200, 255, 255 }, fa));
            }
        }

        /* "Silent" indicator during grace period — subtle. */
        if (game->graceTimer > 0.0f && game->firstPingDone) {
            float ga = fminf(game->graceTimer / 5.0f, 0.5f);
            DrawText("PULSE IS SILENT — ENEMIES CANNOT HEAR",
                     (sw - MeasureText("PULSE IS SILENT — ENEMIES CANNOT HEAR", 12)) / 2,
                     138, 12, Fade(GRAY, ga));
        }

        /* Relay status indicator (appears after first ping). */
        if (game->firstPingDone) {
            const char *relayStatus = (game->relaysActivated >= 1)
                ? "RELAY ONLINE  |  AIRLOCK UNLOCKED"
                : "RELAY OFFLINE  |  FIND THE RELAY ROOM";
            Color relayColor = (game->relaysActivated >= 1)
                ? (Color){ 50, 255, 100, 200 }
                : (Color){ 255, 150, 50, 180 };
            int relayH = game->hudFadeTimer > 3.0f ? 100 : 120;
            DrawText(relayStatus,
                     (sw - MeasureText(relayStatus, 12)) / 2,
                     sh - relayH, 12, relayColor);
        }

        /* --- Station ID + facility name (#17) --- */
        if (game->hudFadeTimer > 0.0f) {
            int seedPart = (int)(game->stationSeed % 10000);
            const char *stationName = TextFormat("STATION KX-%04d", seedPart);
            int sw2 = GetScreenWidth();
            DrawText(stationName, sw2 - MeasureText(stationName, 10) - 12, 12, 10,
                     Fade((Color){ 100, 180, 255, 255 }, 0.3f));
            if (game->facilityNameIdx >= 0 && game->facilityNameIdx < FACILITY_NAME_COUNT) {
                const char *facName = TextFormat("FACILITY %s", FACILITY_NAMES[game->facilityNameIdx]);
                DrawText(facName, sw2 - MeasureText(facName, 10) - 12, 24, 10,
                         Fade((Color){ 100, 180, 255, 255 }, 0.2f));
                /* #11: Station Motto — displayed below facility name. */
                {
                    int mottoIdx = game->facilityNameIdx % STATION_MOTTO_COUNT;
                    const char *motto = STATION_MOTTOS[mottoIdx];
                    DrawText(motto, sw2 - MeasureText(motto, 9) - 12, 35, 9,
                        Fade((Color){ 120, 160, 200, 255 }, 0.15f));
                }
            }
        }

        /* --- #11: Station Announcement overlay --- */
        if (game->announcementDisplay > 0.0f && game->announcementText[0] != '\0') {
            int sw3 = GetScreenWidth(), sh3 = GetScreenHeight();
            float aA = fminf(game->announcementDisplay * 0.8f, 0.6f);
            int aW = MeasureText(game->announcementText, 14);
            DrawText(game->announcementText, (sw3 - aW) / 2, sh3 / 2 + 80, 14,
                     Fade((Color){ 180, 200, 220, 255 }, aA));
        }

        /* --- #15: Impossible Room overlay --- */
        if (game->impossibleRoomRevealTimer > 0.0f && game->impossibleRoomIdx > 0) {
            int sw4 = GetScreenWidth(), sh4 = GetScreenHeight();
            float iA = fminf(game->impossibleRoomRevealTimer * 0.5f, 0.8f);
            const char *impText = "This room should not exist.";
            int iW = MeasureText(impText, 16);
            DrawText(impText, (sw4 - iW) / 2, sh4 / 2 + 75, 16,
                     Fade((Color){ 200, 180, 255, 255 }, iA * 0.7f));
            const char *impSub = "— Echo Anomaly Detected —";
            int iW2 = MeasureText(impSub, 11);
            DrawText(impSub, (sw4 - iW2) / 2, sh4 / 2 + 95, 11,
                     Fade((Color){ 150, 140, 200, 255 }, iA * 0.4f));
        }

        /* --- #18: Procedural Log overlay --- */
        if (game->logDisplayTimer > 0.0f && game->logText[0] != '\0') {
            int sw4 = GetScreenWidth(), sh4 = GetScreenHeight();
            float lA = fminf(game->logDisplayTimer * 0.5f, 0.7f);
            float lPulse = sinf(game->elapsedTime * 2.0f) * 0.1f + 0.9f;
            int lW = MeasureText(game->logText, 13);
            DrawText(game->logText, (sw4 - lW) / 2, sh4 / 2 + 110, 13,
                     Fade((Color){ 200, 230, 255, 255 }, lA * lPulse * 0.5f));
        }

        /* --- Panic breathing overlay --- */
        {
            float breath = (game->nearestEnemyDist < 150.0f)
                         ? (1.0f - game->nearestEnemyDist / 150.0f) * 0.20f
                         : 0.0f;
            if (breath > 0.01f) {
                float breathe = sinf(game->elapsedTime * 3.0f) * 0.3f + 0.7f;
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                DrawRectangle(0, 0, sw, sh,
                    Fade((Color){ 20, 10, 10, 255 }, breath * breathe * 0.15f));
            }
        }

        /* --- Sonar saturation overlay --- */
        if (game->sonarSaturation > 0.01f && game->state == GAME_STATE_PLAYING) {
            float noiseIntensity = game->sonarSaturation * 0.06f;
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            for (int n = 0; n < 40; n++) {
                float nx = (float)GetRandomValue(0, sw);
                float ny = (float)GetRandomValue(0, sh);
                float ns = (float)GetRandomValue(10, 30) / 10.0f;
                DrawCircleV((Vector2){ nx, ny }, ns,
                    Fade((Color){ 120, 120, 120, 255 }, noiseIntensity * 0.3f));
            }
        }
    }

    /* --- #16: CRT Burn-in — brief afterimage overlay. --- */
    if (game->crtBurnTimer > 0.0f && game->state == GAME_STATE_PLAYING) {
        float burnAlpha = fminf(game->crtBurnTimer / 0.2f, 0.06f);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade((Color){ 180, 200, 220, 255 }, burnAlpha));
    }

    /* --- #5: Emergency Lights — RED/BLACK flashes. --- */
    if (game->firstPingDone && game->state == GAME_STATE_PLAYING &&
        game->emergencyLightTimer < 2.0f && game->emergencyLightTimer > 0.0f) {
        float eFlash = sinf(game->elapsedTime * 15.0f) * 0.5f + 0.5f;
        if (eFlash > 0.5f) {
            float eA = (2.0f - game->emergencyLightTimer) / 2.0f;
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                Fade((Color){ 200, 30, 20, 255 }, eA * 0.15f * (eFlash - 0.5f) * 2.0f));
        }
    }

    /* --- #12: Exit Door Breathing — white mist pulses at the airlock. --- */
    if (game->exitMistTimer > 0.0f && game->exitMistTimer < 1.0f &&
        game->state == GAME_STATE_PLAYING && !game->escapeDoorOpen && game->firstPingDone) {
        float mistAlpha = (1.0f - game->exitMistTimer) * 0.5f;
        float mistPulse = sinf(game->elapsedTime * 4.0f) * 0.3f + 0.7f;
        Vector2 alPos = GetWorldToScreen2D((Vector2){
            game->airlockX, game->airlockY + 16.0f }, game->camera);
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (alPos.x >= 0 && alPos.x <= sw && alPos.y >= 0 && alPos.y <= sh) {
            DrawCircleV(alPos, 6.0f + mistPulse * 4.0f,
                Fade((Color){ 220, 230, 240, 255 }, mistAlpha * 0.08f * mistPulse));
        }
    }

    /* --- #4: Power failure overlay (flickerType == 4) --- */
    if (game->state == GAME_STATE_PLAYING && game->flickerType == 4 && game->flickerDuration > 0.0f) {
        float powerDark = fminf(game->flickerDuration / 2.0f, 1.0f) * 0.55f;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade(BLACK, powerDark));
    }

    /* --- Achievement menu overlay --- */
    if (game->achievements.showMenu) {
        AchievementDrawMenu(&game->achievements);
    }

    /* --- Achievement popup notification --- */
    AchievementDrawPopup(&game->achievements);

    /* --- Fade overlay --- */
    if (game->fadeAlpha > 0.005f)
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade(BLACK, game->fadeAlpha));
}

void GameShutdown(Game *game)
{
    if (game == NULL) return;
    AmbientAudioShutdown(&game->ambient);
    RendererShutdown(&game->renderer);
    PlayerShutdown(&game->player);
    SonarShutdown(&game->sonar);
    EnemyManagerShutdown(&game->enemies);
    AchievementSave(&game->achievements);
    game->isRunning = false;
    game->state     = GAME_STATE_EXIT;
}

bool GameShouldClose(const Game *game)
{
    if (game == NULL) return true;
    return !game->isRunning;
}
