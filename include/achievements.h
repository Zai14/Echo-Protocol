/*
 * Echo Protocol - Achievement System.
 *
 * Tracks persistent achievements across play sessions.
 * Achievements are saved to a file and loaded on startup.
 * When an achievement is unlocked, a popup notification appears
 * at the top of the screen for ~4 seconds.
 */

#ifndef ECHO_ACHIEVEMENTS_H
#define ECHO_ACHIEVEMENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

/* ------------------------------------------------------------------ */
/*  Achievement IDs                                                   */
/* ------------------------------------------------------------------ */

typedef enum AchievementId {
    ACH_FIRST_PING     = 0,   /* "Into the Dark" — emit first sonar pulse */
    ACH_FINAL_ECHO     = 1,   /* "First Contact" — die for the first time */
    ACH_ECHOLESS       = 2,   /* "Ω ECHOLESS" — complete run with 0 alerts */
    ACH_SILENT_RUN     = 3,   /* "Ghost Protocol" — escape without alerts */
    ACH_DEATHS_DOOR    = 4,   /* "Barely Breathing" — survive a near miss */
    ACH_RESONANCE      = 5,   /* "Something Answered" — 1% mysterious echo */
    ACH_ECHO_GHOST     = 6,   /* "Echoes of the Past" — trigger a ghost */
    ACH_SHADOW_WATCHER = 7,   /* "Peripheral Vision" — see a moving shadow */
    ACH_RADIO_BURST    = 8,   /* "Last Transmission" — hear radio burst */
    ACH_LORE_KEEPER    = 9,   /* "Archivist" — find a lore terminal */
    ACH_ANOMALY        = 10,  /* "ANOMALY DETECTED" — seed % 777 == 0 */
    ACH_FALSE_RELAY    = 11,  /* "False Signal" — experience fake relay */
    ACH_HUNTER_MIMIC   = 12,  /* "Deceived" — experience Hunter mimic ping */
    ACH_TRIGGER_HAPPY  = 13,  /* "Resonance Cascade" — 20+ pings one run */
    ACH_EXPLORER       = 14,  /* "Cartographer" — visit every room */
    ACH_SPEED_DEMON    = 15,  /* "Speed of Dark" — escape in < 2 minutes */
    ACH_PERSISTENT     = 16,  /* "Persistent Signal" — die 5 total times */
    ACH_MASOCHIST      = 17,  /* "Unbroken" — die 10 total times */
    ACH_ESCAPED        = 18,  /* "Last Light" — successfully escape */
    ACH_FACILITY_VET   = 19,  /* "Veteran Operator" — 3 total attempts */
    ACH_PHANTOM        = 20,  /* "Memory Leak" — encounter the Phantom */
    ACH_COUNT          = 21   /* total number of achievements */
} AchievementId;

/* ------------------------------------------------------------------ */
/*  Achievement metadata                                              */
/* ------------------------------------------------------------------ */

typedef struct AchievementDef {
    AchievementId id;
    const char   *title;        /* short display title */
    const char   *description;  /* unlock condition description */
} AchievementDef;

/* ------------------------------------------------------------------ */
/*  Popup notification state                                          */
/* ------------------------------------------------------------------ */

typedef struct AchievementPopup {
    float   timer;          /* remaining display time (0 = hidden) */
    float   slideOffset;    /* y-offset for slide-in animation */
    char    title[64];      /* cached popup title */
    char    desc[96];       /* cached popup description */
} AchievementPopup;

/* ------------------------------------------------------------------ */
/*  Achievement system state                                          */
/* ------------------------------------------------------------------ */

typedef struct AchievementSystem {
    uint32_t unlockedMask;           /* bitmask of unlocked achievements */
    int      totalUnlocked;          /* count of unlocked achievements */
    int      lifetimeDeaths;         /* total deaths across all sessions */
    int      lifetimeAttempts;       /* total attempts across all sessions */
    int      lifetimeEscapes;        /* total escapes across all sessions */
    int      firstDeathNotified;     /* flag: has first death been detected */
    AchievementPopup popup;          /* current popup notification */
    char     savePath[260];          /* path to save file */
    bool     showMenu;               /* true when achievements menu is open */
    float    menuScroll;             /* scroll offset for menu list */
    bool     resetConfirm;           /* true after first X press, waiting for confirmation */
    float    resetConfirmTimer;      /* countdown before confirmation expires */
} AchievementSystem;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/* Initialise the system and load previously unlocked achievements. */
void AchievementInit(AchievementSystem *as);

/* Save current unlocked bitmask to disk. */
void AchievementSave(const AchievementSystem *as);

/* Unlock an achievement: marks it, saves, triggers popup.
 * Returns true if this is a newly-unlocked achievement. */
bool AchievementUnlock(AchievementSystem *as, AchievementId id, float gameTime);

/* Update popup animation. Call once per frame from GameUpdate. */
void AchievementUpdatePopup(AchievementSystem *as, float deltaTime);

/* Draw the achievement popup banner (if active). Call once per frame
 * from GameDraw after the main HUD layer. */
void AchievementDrawPopup(const AchievementSystem *as);

/* Draw the full achievements menu overlay. Should cover the entire
 * screen with a dark backdrop and show all achievements in a grid.
 * Call from GameDraw when showMenu is true. */
void AchievementDrawMenu(const AchievementSystem *as);

/* Toggle the achievements menu open/closed. */
void AchievementToggleMenu(AchievementSystem *as);

/* Returns true if the given achievement has been unlocked. */
bool AchievementIsUnlocked(const AchievementSystem *as, AchievementId id);

/* Returns a pointer to the static definition table. */
const AchievementDef *AchievementGetDef(AchievementId id);

/* Reset all achievements and lifetime stats to zero, then save. */
void AchievementReset(AchievementSystem *as);

#endif /* ECHO_ACHIEVEMENTS_H */
