/*
 * Echo Protocol - Achievement System implementation.
 *
 * Manages 20 persistent achievements with:
 *   - Bitmask-based save/load to disk (achiev.dat)
 *   - Animated popup banner on unlock
 *   - Lifetime stats (deaths, attempts, escapes)
 *
 * Save file format: binary
 *   [4 bytes: magic 0x4543484F]
 *   [4 bytes: version]
 *   [4 bytes: unlockedMask (uint32_t)]
 *   [4 bytes: lifetimeDeaths]
 *   [4 bytes: lifetimeAttempts]
 *   [4 bytes: lifetimeEscapes]
 */

#include "achievements.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

#define ACH_SAVE_MAGIC       0x4543484F  /* "ECHO" */
#define ACH_SAVE_VERSION     1
#define ACH_POPUP_DURATION   4.5f        /* seconds to show popup */
#define ACH_POPUP_SLIDE_TIME 0.4f        /* seconds for slide animation */

/* ------------------------------------------------------------------ */
/*  Achievement definition table                                      */
/* ------------------------------------------------------------------ */

static const AchievementDef ACH_DEF_TABLE[ACH_COUNT] = {
    { ACH_FIRST_PING,     "Into the Dark",       "Emit your first sonar pulse" },
    { ACH_FINAL_ECHO,     "First Contact",       "Die for the first time" },
    { ACH_ECHOLESS,       "Ω ECHOLESS",          "Complete a run with zero alerts" },
    { ACH_SILENT_RUN,     "Ghost Protocol",      "Escape without alerting any enemies" },
    { ACH_DEATHS_DOOR,    "Barely Breathing",    "Survive a brush with death" },
    { ACH_RESONANCE,      "Something Answered",  "Receive a mysterious echo response" },
    { ACH_ECHO_GHOST,     "Echoes of the Past",  "Trigger an echo ghost event" },
    { ACH_SHADOW_WATCHER, "Peripheral Vision",   "See a fleeting shadow" },
    { ACH_RADIO_BURST,    "Last Transmission",   "Hear a corrupted radio burst" },
    { ACH_LORE_KEEPER,    "Archivist",           "Find a terminal with station lore" },
    { ACH_ANOMALY,        "ANOMALY DETECTED",    "Encounter a corrupted seed" },
    { ACH_FALSE_RELAY,    "False Signal",        "Chase a ghost relay signal" },
    { ACH_HUNTER_MIMIC,   "Deceived",            "Experience the Hunter's mimic pulse" },
    { ACH_TRIGGER_HAPPY,  "Resonance Cascade",   "Use 20+ sonar pulses in one run" },
    { ACH_EXPLORER,       "Cartographer",        "Visit every room in the facility" },
    { ACH_SPEED_DEMON,    "Speed of Dark",       "Escape in under 2 minutes" },
    { ACH_PERSISTENT,     "Persistent Signal",   "Die 5 times across all attempts" },
    { ACH_MASOCHIST,      "Unbroken",            "Die 10 times across all attempts" },
    { ACH_ESCAPED,        "Last Light",          "Successfully escape through the airlock" },
    { ACH_FACILITY_VET,   "Veteran Operator",    "Complete 3 total escape attempts" },
};

/* ------------------------------------------------------------------ */
/*  Save / Load                                                       */
/* ------------------------------------------------------------------ */

static const char *AchievementGetSavePath(void)
{
    /* Use a fixed path relative to the executable's working directory. */
    return "achiev.dat";
}

void AchievementSave(const AchievementSystem *as)
{
    if (as == NULL) return;

    const char *path = AchievementGetSavePath();
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;

    uint32_t magic = ACH_SAVE_MAGIC;
    uint32_t ver   = ACH_SAVE_VERSION;

    fwrite(&magic,              sizeof(magic), 1, f);
    fwrite(&ver,                sizeof(ver),   1, f);
    fwrite(&as->unlockedMask,   sizeof(as->unlockedMask),   1, f);
    fwrite(&as->lifetimeDeaths, sizeof(as->lifetimeDeaths), 1, f);
    fwrite(&as->lifetimeAttempts, sizeof(as->lifetimeAttempts), 1, f);
    fwrite(&as->lifetimeEscapes,  sizeof(as->lifetimeEscapes),  1, f);

    fclose(f);
}

void AchievementInit(AchievementSystem *as)
{
    if (as == NULL) return;

    memset(as, 0, sizeof(*as));

    const char *path = AchievementGetSavePath();
    strncpy(as->savePath, path, sizeof(as->savePath) - 1);
    as->savePath[sizeof(as->savePath) - 1] = '\0';

    FILE *f = fopen(path, "rb");
    if (f == NULL) return;  /* no file yet = fresh start */

    uint32_t magic, ver;
    size_t r = 0;
    r += fread(&magic,             sizeof(magic), 1, f);
    r += fread(&ver,               sizeof(ver),   1, f);
    r += fread(&as->unlockedMask,  sizeof(as->unlockedMask), 1, f);
    r += fread(&as->lifetimeDeaths, sizeof(as->lifetimeDeaths), 1, f);
    r += fread(&as->lifetimeAttempts, sizeof(as->lifetimeAttempts), 1, f);
    r += fread(&as->lifetimeEscapes,  sizeof(as->lifetimeEscapes),  1, f);

    if (r < 6 || magic != ACH_SAVE_MAGIC || ver != ACH_SAVE_VERSION) {
        /* Corrupted or incompatible — reset. */
        memset(as, 0, sizeof(*as));
    } else {
        /* Recompute totalUnlocked from bitmask. */
        uint32_t mask = as->unlockedMask;
        as->totalUnlocked = 0;
        while (mask) {
            as->totalUnlocked += mask & 1;
            mask >>= 1;
        }
    }

    fclose(f);
}

/* ------------------------------------------------------------------ */
/*  Unlock logic                                                      */
/* ------------------------------------------------------------------ */

bool AchievementUnlock(AchievementSystem *as, AchievementId id, float gameTime)
{
    if (as == NULL || id >= ACH_COUNT) return false;

    uint32_t bit = (uint32_t)(1u << (int)id);

    /* Already unlocked — ignore. */
    if (as->unlockedMask & bit) return false;

    as->unlockedMask |= bit;
    as->totalUnlocked++;

    /* Cache the popup text. */
    const AchievementDef *def = AchievementGetDef(id);
    strncpy(as->popup.title, "ACHIEVEMENT UNLOCKED", sizeof(as->popup.title) - 1);
    as->popup.title[sizeof(as->popup.title) - 1] = '\0';
    snprintf(as->popup.desc, sizeof(as->popup.desc), "%s — %s",
             def->title, def->description);

    /* Start popup with slide animation. */
    as->popup.timer       = ACH_POPUP_DURATION;
    as->popup.slideOffset = -60.0f;  /* start above screen, slides down */

    /* Persist immediately. */
    AchievementSave(as);

    return true;
}

bool AchievementIsUnlocked(const AchievementSystem *as, AchievementId id)
{
    if (as == NULL || id >= ACH_COUNT) return false;
    return (as->unlockedMask & (uint32_t)(1u << (int)id)) != 0;
}

const AchievementDef *AchievementGetDef(AchievementId id)
{
    if (id >= ACH_COUNT) return NULL;
    return &ACH_DEF_TABLE[(int)id];
}

/* ------------------------------------------------------------------ */
/*  Popup update & draw                                               */
/* ------------------------------------------------------------------ */

void AchievementUpdatePopup(AchievementSystem *as, float deltaTime)
{
    if (as == NULL) return;

    if (as->popup.timer > 0.0f) {
        as->popup.timer -= deltaTime;

        /* Slide-in animation: approaches 0 (rest position). */
        float slideTarget = 0.0f;
        float slideSpeed  = 120.0f;  /* pixels per second */

        if (as->popup.slideOffset < slideTarget) {
            as->popup.slideOffset += slideSpeed * deltaTime;
            if (as->popup.slideOffset > slideTarget)
                as->popup.slideOffset = slideTarget;
        }

        if (as->popup.timer <= 0.0f) {
            as->popup.timer = 0.0f;
        }
    } else {
        /* Slide-out when timer expires. */
        if (as->popup.slideOffset > -60.0f) {
            as->popup.slideOffset -= 200.0f * deltaTime;
            if (as->popup.slideOffset < -60.0f)
                as->popup.slideOffset = -60.0f;
        }
    }
}

void AchievementDrawPopup(const AchievementSystem *as)
{
    if (as == NULL) return;
    if (as->popup.timer <= 0.0f && as->popup.slideOffset >= -5.0f) return;

    int sw = GetScreenWidth();

    /* Banner background — dark panel with gold border. */
    float bannerW = 520.0f;
    float bannerH = 72.0f;
    float bannerX = (sw - bannerW) / 2.0f;
    float bannerY = 16.0f + as->popup.slideOffset;

    /* Draw border with gold glow. */
    DrawRectangle(bannerX - 2, bannerY - 2, bannerW + 4, bannerH + 4,
                  (Color){ 200, 170, 60, 255 });

    /* Dark panel. */
    DrawRectangle(bannerX, bannerY, bannerW, bannerH,
                  (Color){ 10, 10, 20, 235 });

    /* Inner glow line (top edge). */
    DrawRectangle(bannerX, bannerY, bannerW, 2,
                  (Color){ 255, 220, 100, 200 });

    /* Icon area — star/circle on the left. */
    float iconCX = bannerX + 28.0f;
    float iconCY = bannerY + bannerH / 2.0f;
    DrawCircle((int)iconCX, (int)iconCY, 14.0f,
               (Color){ 255, 215, 50, 200 });
    DrawText("★", (int)iconCX - 7, (int)iconCY - 8, 18,
             (Color){ 10, 10, 20, 255 });

    /* Title text — "ACHIEVEMENT UNLOCKED". */
    int titleX = (int)(bannerX + 52.0f);
    int titleY = (int)(bannerY + 10.0f);
    DrawText(as->popup.title, titleX, titleY, 14,
             (Color){ 255, 215, 50, 255 });

    /* Description text — title + description. */
    int descY = (int)(bannerY + 34.0f);
    DrawText(as->popup.desc, titleX, descY, 12,
             (Color){ 200, 200, 220, 255 });
}
