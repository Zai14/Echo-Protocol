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
#include <stdint.h>

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
    { ACH_PHANTOM,        "Memory Leak",         "Encounter the Phantom in the station" },
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
    (void)gameTime; /* reserved for future use (e.g. elapsed time tracking) */
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

/* ------------------------------------------------------------------ */
/*  Reset                                                             */
/* ------------------------------------------------------------------ */

void AchievementReset(AchievementSystem *as)
{
    if (as == NULL) return;
    as->unlockedMask        = 0;
    as->totalUnlocked       = 0;
    as->lifetimeDeaths      = 0;
    as->lifetimeAttempts    = 0;
    as->lifetimeEscapes     = 0;
    as->firstDeathNotified  = 0;
    AchievementSave(as);
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

/* ------------------------------------------------------------------ */
/*  Toggle menu                                                       */
/* ------------------------------------------------------------------ */

void AchievementToggleMenu(AchievementSystem *as)
{
    if (as == NULL) return;
    as->showMenu = !as->showMenu;
    as->menuScroll = 0.0f;
    as->resetConfirm = false;
    as->resetConfirmTimer = 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Full achievements menu overlay                                    */
/* ------------------------------------------------------------------ */

void AchievementDrawMenu(const AchievementSystem *as)
{
    if (as == NULL || !as->showMenu) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* --- Dark backdrop --- */
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.82f));

    /* --- Outer panel border with subtle glow --- */
    int panelX = 80;
    int panelY = 50;
    int panelW = sw - 160;
    int panelH = sh - 100;

    /* Outer glow. */
    DrawRectangle(panelX - 2, panelY - 2, panelW + 4, panelH + 4,
                  Fade((Color){ 200, 170, 60, 255 }, 0.3f));
    /* Dark panel background. */
    DrawRectangle(panelX, panelY, panelW, panelH,
                  (Color){ 8, 8, 16, 240 });
    /* Thin top accent line. */
    DrawRectangle(panelX, panelY, panelW, 2,
                  Fade((Color){ 255, 220, 100, 255 }, 0.5f));

    /* --- Title --- */
    int titleY = panelY + 16;
    const char *title = "ACHIEVEMENTS";
    int titleW = MeasureText(title, 24);
    DrawText(title, (sw - titleW) / 2, titleY, 24,
             (Color){ 255, 215, 50, 255 });

    /* Counter: X / 20 */
    char counterStr[24];
    snprintf(counterStr, sizeof(counterStr), "%d / %d", as->totalUnlocked, ACH_COUNT);
    int counterW = MeasureText(counterStr, 14);
    DrawText(counterStr, (sw - counterW) / 2, titleY + 30, 14,
             (Color){ 180, 180, 200, 200 });

    /* Progress bar. */
    int barX = sw / 2 - 100;
    int barY = titleY + 52;
    int barW = 200;
    int barH = 6;
    DrawRectangle(barX, barY, barW, barH, (Color){ 40, 40, 50, 255 });
    float progress = (float)as->totalUnlocked / (float)ACH_COUNT;
    if (progress > 0.0f) {
        DrawRectangle(barX, barY, (int)(barW * progress), barH,
                      (Color){ 255, 215, 50, 220 });
    }

    /* --- Achievement list in two columns --- */
    int col1X = panelX + 24;
    int col2X = panelX + panelW / 2 + 12;
    int rowY0 = barY + 24;
    int rowH  = 36;  /* height per achievement row */
    int maxVisibleRows = (panelY + panelH - rowY0 - 16) / rowH;
    int splitIdx = maxVisibleRows;  /* achievements above this index go in col 0, rest in col 1 */

    for (int i = 0; i < ACH_COUNT; i++) {
        const AchievementDef *def = AchievementGetDef((AchievementId)i);
        bool unlocked = AchievementIsUnlocked(as, (AchievementId)i);

        int col = (i < splitIdx) ? 0 : 1;
        int row = (i < splitIdx) ? i : (i - splitIdx);

        int rx = (col == 0) ? col1X : col2X;
        int ry = rowY0 + row * rowH;

        /* Clamp to panel bounds. */
        if (ry + rowH > panelY + panelH - 8) continue;

        /* Unlocked colour scheme. */
        Color iconColor;
        Color titleColor;
        Color descColor;

        if (unlocked) {
            iconColor  = (Color){ 255, 215, 50, 255 };  /* gold */
            titleColor = (Color){ 255, 235, 180, 255 };
            descColor  = (Color){ 180, 180, 200, 220 };
        } else {
            iconColor  = (Color){ 60, 60, 70, 255 };    /* dimmed */
            titleColor = (Color){ 100, 100, 110, 255 };
            descColor  = (Color){ 70, 70, 80, 200 };
        }

        /* Icon: ★ for unlocked, ☆ for locked. */
        DrawText(unlocked ? "★" : "☆", rx, ry + 4, 16, iconColor);

        /* Title + description. */
        int tx = rx + 24;
        DrawText(def->title, tx, ry + 2, 13, titleColor);
        DrawText(def->description, tx, ry + 18, 10, descColor);
    }

    /* --- Hint text at bottom --- */
    const char *hintClose = "Press TAB to close";
    int hintCloseW = MeasureText(hintClose, 14);
    DrawText(hintClose, (sw - hintCloseW) / 2, panelY + panelH - 24, 14,
             (Color){ 120, 120, 140, 180 });

    /* Reset hint — show only if there's something to reset. */
    if (as->totalUnlocked > 0) {
        if (as->resetConfirm) {
            const char *hintReset2 = "Press X again to confirm";
            int hintReset2W = MeasureText(hintReset2, 12);
            DrawText(hintReset2, (sw - hintReset2W) / 2, panelY + panelH - 10, 12,
                     (Color){ 200, 80, 80, 200 });
        } else {
            const char *hintReset = "Press X to reset all progress";
            int hintResetW = MeasureText(hintReset, 12);
            DrawText(hintReset, (sw - hintResetW) / 2, panelY + panelH - 10, 12,
                     (Color){ 100, 80, 80, 160 });
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
