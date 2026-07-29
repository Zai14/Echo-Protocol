#ifndef ECHO_GAME_H
#define ECHO_GAME_H

#include <stdbool.h>
#include "raylib.h"
#include "player.h"
#include "renderer.h"
#include "audio.h"
#include "sonar.h"
#include "echomemory.h"
#include "soundprop.h"
#include "enemy.h"
#include "map.h"

/*
 * Echo Protocol - core game module.
 *
 * Owns the top level application state and the main update/draw loop.
 * The player controller, a follow camera, and the darkness rendering
 * pipeline are wired in.
 *
 * Core mechanic: the only way to see is by emitting a sonar pulse,
 * but every pulse reveals the player to enemies.
 */

#define ECHO_WINDOW_WIDTH  1280
#define ECHO_WINDOW_HEIGHT 720
#define ECHO_TARGET_FPS    60
#define ECHO_WINDOW_TITLE  "Echo Protocol"

typedef enum GameState {
    GAME_STATE_BOOT = 0,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
    GAME_STATE_WON,
    GAME_STATE_EXIT
} GameState;

typedef struct Game {
    GameState state;
    bool      isRunning;
    float     elapsedTime;
    float     deltaTime;

    Player       player;
    Camera2D     camera;
    Renderer     renderer;
    AmbientAudio ambient;
    SonarSystem  sonar;
    EchoMemory   memory;
    SoundPropagation soundProp;
    EnemyManager enemies;
    Station      station;

    /* Win condition: airlock position (from station generation). */
    float airlockX;
    float airlockY;
    bool  gameWon;
    int   scansUsed;
    float runTimeDisplay;

    /* Grace period: first 8s, sonar pulses are silent to enemies. */
    float graceTimer;

    /* HUD tutorial text fades after 8 seconds. */
    float hudFadeTimer;

    /* First ping tracking for dramatic moment + flash label. */
    bool  firstPingDone;
    float firstPingTime;

    /* Footstep timer. */
    float footstepTimer;

    /* Screen shake on sonar pulse. */
    float shakeTimer;
    float shakeIntensity;

    /* Camera zoom pulse on sonar pulse. */
    float cameraZoomTimer;
    float cameraZoomTarget;

    /* Cached nearest enemy distance (computed once per frame in update,
     * used by both audio heartbeat trigger and visual heartbeat ring). */
    float nearestEnemyDist;

    /* Death flash — red overlay with slow-motion before game-over. */
    float deathFlashTimer;
    float deathSlowMo;

    /* Relay activation — hold E to activate the console. */
    int   relaysActivated;
    float relayActivationProgress;  /* 0→1 while holding E near console */
    int   alertsTriggered;          /* sonar pulses after grace period */
    float escapeWhiteTimer;         /* white-out sequence for ending */
    bool  escapeDoorOpen;           /* airlock door has slid open */

    /* Airlock escape sequence countdown (when player reaches door). */
    float airlockCountdown;
    bool  airlockSequence;

    /* Title screen timer (GAME_STATE_BOOT). */
    float titleTimer;
    float titlePulseTimer;  /* interval between auto-sonar pulses on title */

    /* Relay room orange glow — triggered when a sonar pulse passes through
     * the relay room.  Gives the player a hint about where to go. */
    float relayGlowTimer;

    /* Failing electronics — random atmosphere events. */
    float flickerTimer;      /* countdown to next event */
    int   flickerType;       /* 0=none, 1=sparks, 2=door slam, 3=hum */
    float flickerDuration;   /* remaining effect time */

    /* Hunter pattern learning — tracks ping rhythm for prediction. */
    float lastPingTime;      /* time of most recent ping */
    float pingIntervals[4];  /* last 4 intervals between pings */
    int   pingIntervalCount; /* how many intervals recorded */

    /* Station seed for unique identifier display. */
    uint64_t stationSeed;

    /* Sonar saturation — spamming makes screen noisy (0..1). */
    float sonarSaturation;

    /* Hunter mimics sonar — countdown before Hunter emits a fake ping. */
    float hunterMimicTimer;

    /* Near Miss — Hunter rushed within 10px but missed → brief slow-mo. */
    float nearMissTimer;

    /* --- #2: Hunter Pattern Learning (positional prediction) --- */
    float lastPingPositionsX[2];  /* last 2 ping world X positions */
    float lastPingPositionsY[2];  /* last 2 ping world Y positions */
    float predictedPingX;         /* predicted next ping X */
    float predictedPingY;         /* predicted next ping Y */

    /* --- #3: Doors Amplify Sound --- */
    bool wasInCorridor;  /* true if player was in a corridor last frame */

    /* --- #5: Echo Ghosts --- */
    float ghostTimer;       /* countdown to next ghost replay */
    float ghostReplayX;     /* stored pulse origin for ghost */
    float ghostReplayY;

    /* --- #7: False Relay Signals --- */
    int   falseRelayRoomIdx;   /* room that gets fake orange glow */
    float falseRelayTimer;     /* timer for fake glow */

    /* --- #8: Environmental Hazards --- */
    float hazardSoundTimer; /* countdown to next hazard sound */

    /* --- #11: Station Announcements --- */
    float announcementTimer;       /* countdown to next announcement */
    float announcementDisplay;     /* remaining display time */
    char  announcementText[80];    /* current announcement text */

    /* --- #15: Perfect Silent Run Rank (computed on-the-fly) --- */

    /* --- #16: Dynamic Ending --- */
    int   endingTension;            /* 0=silent, 1=normal, 2=alarmed */

    /* --- #17: Procedural Station Name --- */
    int   facilityNameIdx;          /* index into facility name table */

    /* --- #18: Procedural Logs --- */
    int   logRoomIdx;               /* room containing a lore terminal */
    float logDisplayTimer;          /* remaining display time for log */
    char  logText[80];              /* current log text */

    /* --- #19: Adaptive Title Screen --- */
    int   attemptCount;
    char  lastOutcome[20];          /* "Lost Contact" or "Recovered" */

    /* --- #5: Terminal Numbers --- */
    int   terminalMottoIdx;         /* index into terminal number strings */

    /* --- #12: Fake Hunter Footsteps --- */
    float fakeFootstepTimer;

    /* --- #14: Random Subtitle --- */
    int   titleSubtitleIdx;         /* index into subtitle pool */

    /* --- #15: Impossible Room --- */
    int   impossibleRoomIdx;        /* -1 if none, room index if exists */
    float impossibleRoomRevealTimer;/* timer showing easter egg text */

    /* --- #3: Broken Sonar --- */
    float brokenSonarTimer;         /* countdown to delayed ping */

    /* --- #10/#15: Water Puddles / Damaged Floors --- */
    int   wetRoomCount;             /* rooms with water = louder footsteps */
    int   wetRoomIdxs[3];
    int   rustyRoomCount;           /* rooms with damaged floors = louder */
    int   rustyRoomIdxs[3];

    /* --- #12: Moving Shadows --- */
    float shadowTimer;              /* countdown to next one-frame shadow */
    float shadowX, shadowY;         /* screen position of shadow */

    /* --- #13: False Heartbeat --- */
    float falseHeartbeatTimer;      /* random heartbeat when no enemy */

    /* --- #14: Radio Burst --- */
    float radioTimer;               /* countdown to next radio burst */
    float radioDisplay;             /* remaining display time */
    char  radioText[80];            /* current radio fragment */

    /* --- #18: Dynamic Room Names --- */
    int   currentRoomIdx;           /* -1 if outside, room idx if inside */
    float roomNameTimer;            /* display timer for room name */
    char  roomName[48];             /* procedural room name */

    /* --- #19: Station Age --- */
    int   stationAge;               /* derived from seed */
    float ageGlitchMult;            /* 1.0-2.0, older = more glitches */

    /* --- #22: Hidden Observation Room --- */
    int   hiddenRoomIdx;            /* -1 if none */
    float hiddenRevealTimer;        /* timer showing silhouette */

    /* --- #3: Echo Delay — room size affects echo return timing --- */
    float echoDelayTimer;
    int   lastPingRoomIdx;

    /* --- #5: Emergency Lights — RED/BLACK flashes every ~30s --- */
    float emergencyLightTimer;

    /* --- #12: Exit Door Breathing — white mist at airlock --- */
    float exitMistTimer;

    /* --- #13: Reactor Pulse — global brightness modulation --- */
    float reactorPulseTimer;

    /* --- #15: Broken Monitor — random room with ERROR flicker --- */
    int   monitorRoomIdx;

    /* --- #16: CRT Burn-in — brief afterimage overlay --- */
    float crtBurnTimer;

    /* --- #18: Loose Cable Sparks --- */
    float cableSparkTimer;
    float cableSparkX;
    float cableSparkY;

    /* --- #20: Power Drain — each sonar dims the lights briefly --- */
    float powerDrainTimer;

    /* --- #25: Final Black Screen — 3s pause before stats --- */
    float finalBlackTimer;

    /* --- #26: Hidden Seed Challenge — ANOMALY on seed % 777 --- */
    bool  anomalySeed;

    /* --- #2: Player trail — last 5 positions for fading trail effect --- */
    float playerTrailX[5];
    float playerTrailY[5];
    int   playerTrailCount;

    /* --- #4: Enemy alert flash — brief ring when enemy transitions to ALERT --- */
    float alertFlashTimer;
    float alertFlashX;
    float alertFlashY;

    /* Footstep dust puffs — 3 particles with lifetimes. */
    float playerDustX[3];
    float playerDustY[3];
    float playerDustLife[3];

    /* Fade transition. */
    float fadeAlpha;
    float fadeTarget;
} Game;

void GameInit(Game *game);
void GameRestart(Game *game);
void GameUpdate(Game *game);
void GameDraw(Game *game);
void GameShutdown(Game *game);
bool GameShouldClose(const Game *game);

#endif /* ECHO_GAME_H */
