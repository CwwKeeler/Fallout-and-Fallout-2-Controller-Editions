// Opt-in, build-time-only integration test. Never included in normal builds.
#include "../src/gamepad_internal.h"
#include <SDL.h>
#include <cstdio>
#include <cmath>
#if __has_include("../src/game/combat.h")
#include "../src/game/combat.h"
#include "../src/game/anim.h"
#include "../src/game/gmouse.h"
#include "../src/game/map.h"
#include "../src/game/object.h"
#include "../src/game/tile.h"
#include "../src/plib/gnw/mouse.h"
#include "../src/plib/gnw/svga.h"
#define TEST_DUDE obj_dude
#define TEST_FIRST obj_find_first_at
#define TEST_NEXT obj_find_next_at
#define TEST_DISTANCE obj_dist
#define TEST_NEIGHBOR tile_num_in_direction
#define TEST_BLOCKED obj_blocking_at
#define TEST_COORD tile_coord
#define TEST_PLACE obj_move_to_tile
#define TEST_MOUSE mouse_get_position
#define TEST_MOUSE_PLACE mouse_set_position
#define TEST_SCROLL map_scroll
#define TEST_REFRESH tile_refresh_display
#define TEST_FREE_AP combat_free_move
#define TEST_ANIM_BUSY anim_busy
#define TEST_CURSOR_VISIBLE gmouse_3d_is_on
#define TEST_CURSOR_MODE gmouse_3d_get_mode
#define TEST_CURSOR_REFRESH gmouse_3d_refresh
#define TEST_MOUSE_EVENT gmouse_handle_event
#else
#include "../src/combat.h"
#include "../src/map.h"
#include "../src/animation.h"
#include "../src/game_mouse.h"
#include "../src/object.h"
#include "../src/tile.h"
#include "../src/mouse.h"
#include "../src/svga.h"
#define TEST_DUDE gDude
#define TEST_FIRST objectFindFirstAtElevation
#define TEST_NEXT objectFindNextAtElevation
#define TEST_DISTANCE objectGetDistanceBetween
#define TEST_NEIGHBOR tileGetTileInDirection
#define TEST_BLOCKED _obj_blocking_at
#define TEST_COORD tileToScreenXY
#define TEST_PLACE objectSetLocation
#define TEST_MOUSE mouseGetPosition
#define TEST_MOUSE_PLACE _mouse_set_position
#define TEST_SCROLL mapScroll
#define TEST_REFRESH tileWindowRefresh
#define TEST_FREE_AP _combat_free_move
#define TEST_ANIM_BUSY animationIsBusy
#define TEST_CURSOR_VISIBLE gameMouseObjectsIsVisible
#define TEST_CURSOR_MODE gameMouseGetMode
#define TEST_CURSOR_REFRESH _gmouse_3d_refresh
#define TEST_MOUSE_EVENT _gmouse_handle_event
#endif

namespace fallout {
static SDL_Joystick* smokeJoystick = nullptr;
static Uint32 smokeStart;
static int smokeStep;
static int captureNumber;
static bool smokeReady;
static Uint32 nextIntroSkip;
static int enemyTurnsCompleted;
static bool motionRecording;
static bool motionHeld;
static int motionAnchor;
static int motionFrame;
#include "display_smoke.h"
#include "camera_smoke.h"

void gamepadSmokeEnemyCompleted()
{
    ++enemyTurnsCompleted;
}

static void gameplayNote(const char* text)
{
    FILE* file = fopen("gameplay-test.log", "a");
    if (file) { fprintf(file, "%s\n", text); fclose(file); }
}

static void gameplayFinish(bool pass)
{
    gameplayNote(pass && !cameraFailed ? "PASS: direct movement, camera panning, stop, L3 cursor toggle, three AP-exhaustion enemy cycles."
                      : "FAIL: gameplay regression; see prior checks.");
    SDL_Event quit {};
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
    smokeJoystick = nullptr;
}

static void gameplayUpdate(Uint32 elapsed)
{
    static int stage = 0, originalTile, stoppedTile, cursorX, cursorY;
    static int exhausted = 0, previousEnemies = 0;
    static bool panStarted = false, panReleased = false;
    static Uint32 panStartTime;
    static Uint32 last = 0;
    if (stage == 6 && !cameraRestUpdate(elapsed)) return;
    if (elapsed > 110000) { gameplayNote("Timed out"); gameplayFinish(false); return; }
    if (stage == 0 && elapsed >= 1000) { pad::pulse(SDL_SCANCODE_N); stage = 1; }
    else if (stage == 1 && elapsed >= 3500) { pad::pulse(SDL_SCANCODE_T); stage = 2; }
    else if (stage == 2 && elapsed >= 7000) { pad::pulse(SDL_SCANCODE_ESCAPE); stage = 3; }
    else if (stage == 3 && elapsed >= 11000 && pad::state.worldInput
        && pad::state.worldContext && !pad::state.waitNeutral && TEST_DUDE
        && TEST_ANIM_BUSY(TEST_DUDE) == 0) {
        int direction = -1;
        for (int r = 0; r < 6; ++r) {
            bool clear = true;
            for (int distance = 1; distance <= 5; ++distance) {
                int tile = TEST_NEIGHBOR(TEST_DUDE->tile, r, distance);
                if (tile == TEST_DUDE->tile || TEST_BLOCKED(TEST_DUDE, tile, TEST_DUDE->elevation)) clear = false;
            }
            if (clear) { direction = r; break; }
        }
        if (direction < 0) { gameplayNote("No clear movement lane"); gameplayFinish(false); return; }
        originalTile = TEST_DUDE->tile;
        motionAnchor = originalTile;
        motionRecording = motionHeld = true;
        cameraPhase = 1;
        int ox, oy, tx, ty;
        TEST_COORD(originalTile, &ox, &oy, TEST_DUDE->elevation);
        TEST_COORD(TEST_NEIGHBOR(originalTile, direction, 1), &tx, &ty, TEST_DUDE->elevation);
        float length = std::sqrt(float((tx-ox)*(tx-ox)+(ty-oy)*(ty-oy)));
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTX, Sint16(32767 * (tx-ox) / length));
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTY, Sint16(32767 * (ty-oy) / length));
        cameraWalkX = Sint16(32767 * (tx-ox) / length);
        cameraWalkY = Sint16(32767 * (ty-oy) / length);
        last = elapsed; stage = 4;
    } else if (stage == 4) {
        if (elapsed - last >= 400 && TEST_CURSOR_MODE() == GAME_MOUSE_MODE_MOVE
            && TEST_CURSOR_VISIBLE()) {
            gameplayNote("Movement marker visible during analogue walking"); gameplayFinish(false); return;
        }
        if (!panStarted && elapsed - last >= 600 && TEST_DUDE->tile != originalTile) {
            // Deliberately oppose follow, with the old cursor parked at the
            // other edge. Neither automatic camera may fight the right stick.
            SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTX, -cameraWalkX);
            SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTY, -cameraWalkY);
            TEST_MOUSE_PLACE(cameraWalkX >= 0 ? screenGetWidth() - 1 : 0, screenGetHeight() / 2);
            cameraPhase = 2;
            panStarted = true;
            panStartTime = elapsed;
        }
        if (panStarted && !panReleased && elapsed - panStartTime >= 900) {
            SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTX, 0);
            SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTY, 0);
            cameraPhase = 3;
            panReleased = true;
        }
        if (!panReleased || elapsed - last < 2200 || elapsed - panStartTime < 1600) return;
        motionHeld = false;
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTX, 0);
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTY, 0);
        last = elapsed; stage = 5;
    } else if (stage == 5 && elapsed - last >= 1300) {
        motionRecording = false;
        if (TEST_DUDE->tile == originalTile) { gameplayNote("Stick did not move character"); gameplayFinish(false); return; }
        gameplayNote("Character moved using analogue stick.");
        stoppedTile = TEST_DUDE->tile;
        captureNumber = 101;
        last = elapsed; stage = 6;
    } else if (stage == 6 && elapsed - last >= 1000) {
        if (TEST_DUDE->tile != stoppedTile) { gameplayNote("Character kept moving after release"); gameplayFinish(false); return; }
        gameplayNote("Character stopped after releasing stick.");
        TEST_CURSOR_REFRESH();
        if (TEST_CURSOR_VISIBLE()) {
            gameplayNote("Movement marker returned after stick release"); gameplayFinish(false); return;
        }
        int mx, my;
        TEST_MOUSE(&mx, &my);
        TEST_MOUSE_EVENT(mx, my, MOUSE_EVENT_RIGHT_BUTTON_DOWN);
        TEST_CURSOR_REFRESH();
        if (TEST_CURSOR_MODE() != GAME_MOUSE_MODE_ARROW || !TEST_CURSOR_VISIBLE()) {
            gameplayNote("Hidden marker prevented interaction cursor"); gameplayFinish(false); return;
        }
        TEST_MOUSE_EVENT(mx, my, MOUSE_EVENT_RIGHT_BUTTON_DOWN);
        TEST_CURSOR_REFRESH();
        if (isInCombat() && TEST_CURSOR_MODE() == GAME_MOUSE_MODE_CROSSHAIR) {
            if (!TEST_CURSOR_VISIBLE()) {
                gameplayNote("Targeting cursor hidden in combat"); gameplayFinish(false); return;
            }
            TEST_MOUSE_EVENT(mx, my, MOUSE_EVENT_RIGHT_BUTTON_DOWN);
            TEST_CURSOR_REFRESH();
        }
        if (TEST_CURSOR_MODE() != GAME_MOUSE_MODE_MOVE || TEST_CURSOR_VISIBLE()) {
            gameplayNote("Movement marker did not hide after cursor mode cycle"); gameplayFinish(false); return;
        }
        SDL_Event motion {};
        motion.type = SDL_MOUSEMOTION;
        motion.motion.xrel = 1;
        gamepadHandleEvent(motion);
        TEST_CURSOR_REFRESH();
        if (!TEST_CURSOR_VISIBLE()) {
            gameplayNote("Mouse motion did not restore movement marker"); gameplayFinish(false); return;
        }
        gameplayNote("Analogue marker hidden; interaction cursor and real mouse restore correctly.");
        SDL_JoystickSetVirtualButton(smokeJoystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, 1);
        last = elapsed; stage = 7;
    } else if (stage == 7 && elapsed - last >= 150) {
        SDL_JoystickSetVirtualButton(smokeJoystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, 0);
        last = elapsed; stage = 8;
    } else if (stage == 8 && elapsed - last >= 150) {
        if (!TEST_CURSOR_VISIBLE()) {
            gameplayNote("L3 did not restore movement marker"); gameplayFinish(false); return;
        }
        TEST_MOUSE(&cursorX, &cursorY);
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTX, 32767);
        last = elapsed; stage = 9;
    } else if (stage == 9 && elapsed - last >= 400) {
        int x, y;
        TEST_MOUSE(&x, &y);
        if ((x == cursorX && y == cursorY) || TEST_DUDE->tile != stoppedTile) {
            gameplayNote("L3 cursor mode failed"); gameplayFinish(false); return;
        }
        gameplayNote("L3 switched to cursor; stick moved cursor without moving character.");
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_LEFTX, 0);
        captureNumber = 102;
        last = elapsed; stage = 10;
    } else if (stage == 10 && elapsed - last >= 200) {
        SDL_JoystickSetVirtualButton(smokeJoystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, 1);
        last = elapsed; stage = 11;
    } else if (stage == 11 && elapsed - last >= 150) {
        SDL_JoystickSetVirtualButton(smokeJoystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, 0);
        last = elapsed; stage = 12;
    } else if (stage == 12 && elapsed - last >= 300 && pad::state.worldInput) {
        if (TEST_CURSOR_VISIBLE()) {
            gameplayNote("Returning to analogue mode did not hide movement marker"); gameplayFinish(false); return;
        }
        Object* enemy = nullptr;
        int nearest = 999999;
        for (Object* obj = TEST_FIRST(TEST_DUDE->elevation); obj; obj = TEST_NEXT()) {
            if (obj != TEST_DUDE && FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER
                && obj->data.critter.combat.team != TEST_DUDE->data.critter.combat.team
                && !(obj->data.critter.combat.results & DAM_DEAD)) {
                int distance = TEST_DISTANCE(obj, TEST_DUDE);
                if (distance < nearest) { enemy = obj; nearest = distance; }
            }
        }
        if (!enemy) { gameplayNote("No enemy on test map"); gameplayFinish(false); return; }
        // Prepare an encounter in this isolated test copy, never the user's saves.
        for (int r = 0; r < 6; ++r) {
            int tile = TEST_NEIGHBOR(TEST_DUDE->tile, r, 2);
            if (tile != TEST_DUDE->tile && !TEST_BLOCKED(enemy, tile, TEST_DUDE->elevation)) {
                TEST_PLACE(enemy, tile, TEST_DUDE->elevation, nullptr);
                break;
            }
        }
        enemy->data.critter.combat.whoHitMe = TEST_DUDE;
        enemy->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
        TEST_DUDE->data.critter.hp = 200;
        pad::pulse(SDL_SCANCODE_A);
        previousEnemies = enemyTurnsCompleted;
        last = elapsed; stage = 13;
    } else if (stage == 13 && isInCombat() && pad::state.worldInput && elapsed - last >= 1500) {
        if (exhausted && enemyTurnsCompleted <= previousEnemies) return;
        if (exhausted == 3) {
            captureNumber = 103;
            gameplayFinish(true);
            return;
        }
        previousEnemies = enemyTurnsCompleted;
        TEST_DUDE->data.critter.combat.ap = 0;
        TEST_FREE_AP = 0;
        ++exhausted;
        gameplayNote("Exhausted player AP; waiting for enemy AI to complete.");
        last = elapsed;
    }
}

void gamepadSmokeReady()
{
    if (!smokeJoystick || smokeReady) return;
    smokeReady = true;
    smokeStart = SDL_GetTicks();
}

void gamepadSmokeInit()
{
    if (!SDL_getenv("FALLOUT_CONTROLLER_SMOKE")) return;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, SDL_CONTROLLER_BUTTON_MAX, 0);
    if (index < 0) return;
    smokeJoystick = SDL_JoystickOpen(index);
    SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    smokeStart = SDL_GetTicks();
}

void gamepadSmokeUpdate()
{
    if (!smokeJoystick) return;
    for (size_t i = 0; i < pad::state.devices.size(); ++i) {
        if (pad::state.devices[i].id == SDL_JoystickInstanceID(smokeJoystick)
            && pad::state.active != static_cast<int>(i)) {
            pad::releaseInputs();
            pad::state.active = static_cast<int>(i);
            pad::state.waitNeutral = true;
        }
    }
    if (!smokeReady) {
        Uint32 now = SDL_GetTicks();
        if (now - smokeStart >= 2000 && now >= nextIntroSkip) {
            pad::pulse(SDL_SCANCODE_ESCAPE);
            nextIntroSkip = now + 1500;
        }
        return;
    }
    if (SDL_getenv("FALLOUT_CONTROLLER_DISPLAY_TEST")) {
        displayUpdate(SDL_GetTicks() - smokeStart);
        return;
    }
    if (SDL_getenv("FALLOUT_CONTROLLER_GAMEPLAY_TEST")) {
        gameplayUpdate(SDL_GetTicks() - smokeStart);
        return;
    }
    struct Step { Uint32 ms; int button; int down; SDL_Scancode key; int capture; };
    static const Step steps[] = {
        { 9000, SDL_CONTROLLER_BUTTON_BACK, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 9200, SDL_CONTROLLER_BUTTON_BACK, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 10000, -1, 0, SDL_SCANCODE_UNKNOWN, 1 },
        { 11000, SDL_CONTROLLER_BUTTON_B, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 11200, SDL_CONTROLLER_BUTTON_B, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 12000, -1, 0, SDL_SCANCODE_N, 0 },
        { 14000, -1, 0, SDL_SCANCODE_UNKNOWN, 2 },
        { 15000, -1, 0, SDL_SCANCODE_T, 0 },
        { 18000, SDL_CONTROLLER_BUTTON_START, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 18200, SDL_CONTROLLER_BUTTON_START, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 22000, -1, 0, SDL_SCANCODE_UNKNOWN, 3 },
        { 24000, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 24200, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 26000, -1, 0, SDL_SCANCODE_UNKNOWN, 4 },
        { 27000, SDL_CONTROLLER_BUTTON_START, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 27200, SDL_CONTROLLER_BUTTON_START, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 29000, SDL_CONTROLLER_BUTTON_BACK, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 29200, SDL_CONTROLLER_BUTTON_BACK, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 30000, -1, 0, SDL_SCANCODE_UNKNOWN, 5 },
        { 31000, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 31200, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 32000, -1, 0, SDL_SCANCODE_UNKNOWN, 6 },
        { 33000, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 33200, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 34000, -1, 0, SDL_SCANCODE_UNKNOWN, 7 },
        { 35000, SDL_CONTROLLER_BUTTON_B, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 35200, SDL_CONTROLLER_BUTTON_B, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 36000, -1, 0, SDL_SCANCODE_F4, 0 },
        { 38000, -1, 0, SDL_SCANCODE_RETURN, 0 },
        { 40000, -1, 0, SDL_SCANCODE_UNKNOWN, 8 },
        { 42000, SDL_CONTROLLER_BUTTON_A, 1, SDL_SCANCODE_UNKNOWN, 0 },
        { 42200, SDL_CONTROLLER_BUTTON_A, 0, SDL_SCANCODE_UNKNOWN, 0 },
        { 45000, -1, 0, SDL_SCANCODE_UNKNOWN, 9 },
        { 46000, -1, 0, SDL_SCANCODE_F5, 0 },
        { 48000, -1, 0, SDL_SCANCODE_UNKNOWN, 10 },
        { 50000, -1, 0, SDL_SCANCODE_ESCAPE, 0 },
    };
    Uint32 time = SDL_GetTicks() - smokeStart;
    if (time > 52000) {
        SDL_Event quit {};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
        smokeJoystick = nullptr;
        return;
    }
    if (smokeStep >= static_cast<int>(sizeof(steps) / sizeof(steps[0])) || time < steps[smokeStep].ms) return;
    auto step = steps[smokeStep++];
    if (step.capture == 8 && pad::state.textInput && pad::state.open) {
        pad::state.typed = "Controller Test";
        pad::state.selection[2] = 49;
    }
    if (step.button >= 0) SDL_JoystickSetVirtualButton(smokeJoystick, step.button, step.down);
    if (step.key != SDL_SCANCODE_UNKNOWN) pad::pulse(step.key);
    if (step.capture) captureNumber = step.capture;
}

void gamepadSmokeRender(SDL_Renderer* renderer)
{
    displayRender(renderer);
    cameraRender(renderer);
    if (motionRecording && TEST_DUDE) {
        int terrainX, terrainY;
        TEST_COORD(motionAnchor, &terrainX, &terrainY, TEST_DUDE->elevation);
        FILE* trace = fopen("motion-trace.csv", "a");
        if (trace) {
            fprintf(trace, "%u,%d,%d,%d,%d,%d,%d,%d,%d\n", SDL_GetTicks(), motionHeld,
                TEST_DUDE->tile, TEST_DUDE->x, TEST_DUDE->y, FID_ANIM_TYPE(TEST_DUDE->fid),
                TEST_DUDE->frame, terrainX, terrainY);
            fclose(trace);
        }
        static Uint32 nextFrame;
        if (SDL_GetTicks() >= nextFrame) {
            nextFrame = SDL_GetTicks() + 33;
            int w, h;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_Surface* frame = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            if (frame) {
                if (SDL_RenderReadPixels(renderer, nullptr, frame->format->format, frame->pixels, frame->pitch) == 0) {
                    char filename[64];
                    snprintf(filename, sizeof(filename), "motion-%04d.bmp", motionFrame++);
                    SDL_SaveBMP(frame, filename);
                }
                SDL_FreeSurface(frame);
            }
        }
    }
    if (!captureNumber) return;
    int width, height;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        if (SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0) {
            char name[80];
            snprintf(name, sizeof(name), "controller-smoke-%02d.bmp", captureNumber);
            SDL_SaveBMP(surface, name);
        }
        SDL_FreeSurface(surface);
    }
    captureNumber = 0;
}
} // namespace fallout
