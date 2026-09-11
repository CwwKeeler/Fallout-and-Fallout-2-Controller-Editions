#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_movement.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace fallout;
static std::vector<std::pair<SDL_Scancode, bool>> keys;
static std::string typed;
static int checks = 0;
static SDL_JoystickID testController = -1;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "FAIL line %d: %s (SDL: %s)\n", __LINE__, #x, SDL_GetError()); exit(1); } } while (0)

static void key(SDL_Scancode code, bool down) { keys.emplace_back(code, down); }
static void character(int ch) { typed += static_cast<char>(ch); }
static void pump()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) gamepadHandleEvent(e);
    // A real controller may be plugged into the developer's PC during tests.
    if (testController >= 0) {
        auto found = std::find_if(pad::state.devices.begin(), pad::state.devices.end(),
            [](const pad::Device& d) { return d.id == testController; });
        pad::state.active = found == pad::state.devices.end() ? -1 : static_cast<int>(found - pad::state.devices.begin());
    }
    gamepadUpdate();
}
static void button(SDL_Joystick* joy, int index, bool down)
{
    CHECK(SDL_JoystickSetVirtualButton(joy, index, down ? SDL_PRESSED : SDL_RELEASED) == 0);
    SDL_JoystickUpdate();
    pump();
}
static void axis(SDL_Joystick* joy, int index, Sint16 value)
{
    CHECK(SDL_JoystickSetVirtualAxis(joy, index, value) == 0);
    SDL_JoystickUpdate();
    pump();
}
static int mouse(int* deltaX = nullptr, int* scrollY = nullptr, int native = 0)
{
    int x = 0, y = 0, b = native, wx = 0, wy = 0;
    gamepadMouse(x, y, b, wx, wy);
    if (deltaX) *deltaX = x;
    if (scrollY) *scrollY = wy;
    return b;
}
static bool saw(SDL_Scancode code, bool down)
{
    return std::find(keys.begin(), keys.end(), std::make_pair(code, down)) != keys.end();
}

int main(int, char**)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0);
    FILE* config = fopen("controller.ini", "w");
    CHECK(config);
    fputs("speed=500\ndeadzone=18\n", config);
    fclose(config);
    gamepadInit("controller-tests", key, character);

    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, SDL_CONTROLLER_BUTTON_MAX, 0);
    CHECK(index >= 0);
    SDL_Joystick* joy = SDL_JoystickOpen(index);
    CHECK(joy);
    CHECK(SDL_IsGameController(index));
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    pump();
    SDL_JoystickID id = SDL_JoystickInstanceID(joy);
    testController = id;
    auto found = std::find_if(pad::state.devices.begin(), pad::state.devices.end(), [id](const pad::Device& device) { return device.id == id; });
    CHECK(found != pad::state.devices.end());
    pad::state.active = static_cast<int>(found - pad::state.devices.begin());
    pump();

    // Radial deadzone, fractional motion and frame-rate-independent velocity.
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 2000);
    pad::state.lastTick -= 50;
    int dx;
    mouse(&dx);
    CHECK(dx == 0);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    pad::state.lastTick -= 40;
    mouse(&dx);
    CHECK(dx >= 19 && dx <= 22);
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767);
    pad::state.lastTick -= 40;
    mouse(&dx);
    CHECK(dx >= 4 && dx <= 7);
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);
    mouse();

    // Held drag plus real mouse input; releasing one source preserves the other.
    button(joy, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(mouse() & 1);
    CHECK(mouse() & 1);
    CHECK(mouse(nullptr, nullptr, 2) == 3);
    button(joy, SDL_CONTROLLER_BUTTON_A, false);
    CHECK(mouse() == 0);
    button(joy, SDL_CONTROLLER_BUTTON_B, true);
    CHECK(mouse() == 2);
    button(joy, SDL_CONTROLLER_BUTTON_B, false);

    // Key taps, held run modifier and held/released D-pad navigation.
    keys.clear();
    button(joy, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, true);
    button(joy, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, false);
    CHECK(saw(SDL_SCANCODE_I, true) && saw(SDL_SCANCODE_I, false));
    keys.clear();
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, true);
    button(joy, SDL_CONTROLLER_BUTTON_DPAD_UP, true);
    CHECK(saw(SDL_SCANCODE_LSHIFT, true) && saw(SDL_SCANCODE_UP, true));

    // Losing focus releases all controller keys/buttons; held controls may
    // not fire again until released after focus returns.
    SDL_Event focus {};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    gamepadHandleEvent(focus);
    CHECK(saw(SDL_SCANCODE_LSHIFT, false) && saw(SDL_SCANCODE_UP, false));
    button(joy, SDL_CONTROLLER_BUTTON_A, true);
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    gamepadHandleEvent(focus);
    pump();
    CHECK(mouse() == 0);
    button(joy, SDL_CONTROLLER_BUTTON_A, false);
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, false);
    button(joy, SDL_CONTROLLER_BUTTON_DPAD_UP, false);

    // Controller panel consumes navigation and confirm without click-through.
    button(joy, SDL_CONTROLLER_BUTTON_BACK, true);
    CHECK(gamepadOverlayOpen());
    button(joy, SDL_CONTROLLER_BUTTON_BACK, false);
    keys.clear();
    button(joy, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(!gamepadOverlayOpen());
    CHECK(saw(SDL_SCANCODE_I, true));
    CHECK(mouse() == 0);
    button(joy, SDL_CONTROLLER_BUTTON_A, false);
    button(joy, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(mouse() == 1);
    button(joy, SDL_CONTROLLER_BUTTON_A, false);

    // Text entry retains case and ordering; DONE appends Enter after text.
    pad::state.used = true;
    gamepadTextInput(true);
    CHECK(gamepadOverlayOpen() && pad::state.tab == 2);
    pad::state.typed = "Vault 13";
    pad::state.selection[2] = 49;
    keys.clear();
    pad::activate();
    while (gamepadDispatchText()) { }
    CHECK(typed == std::string(64, '\b') + "Vault 13\r");
    gamepadTextInput(false);

    // Cancelling or leaving a text field must stop pending text, so remaining
    // letters cannot trigger shortcuts on the next game screen.
    pad::state.pendingText = "remaining";
    gamepadTextInput(false);
    CHECK(!gamepadDispatchText());
    pad::state.pendingText = "remaining";
    button(joy, SDL_CONTROLLER_BUTTON_START, true);
    CHECK(!gamepadDispatchText());
    button(joy, SDL_CONTROLLER_BUTTON_START, false);

    // F11 is available without using the gamepad and captures keydown only.
    SDL_Event f11 {};
    f11.type = SDL_KEYDOWN;
    f11.key.keysym.scancode = SDL_SCANCODE_F11;
    CHECK(gamepadHandleEvent(f11));
    CHECK(gamepadOverlayOpen());
    f11.type = SDL_KEYUP;
    CHECK(gamepadHandleEvent(f11));
    f11.type = SDL_KEYDOWN;
    CHECK(gamepadHandleEvent(f11));
    CHECK(!gamepadOverlayOpen());
    pump();

    // Right stick maps to wheel, trigger has hysteresis, no runaway motion
    // after a long stall.
    axis(joy, SDL_CONTROLLER_AXIS_RIGHTY, -32768);
    pad::state.lastTick -= 1000;
    int wy;
    mouse(nullptr, &wy);
    CHECK(wy >= 0 && wy <= 1);
    for (int i = 0; i < 3; ++i) { pad::state.lastTick -= 50; mouse(nullptr, &wy); }
    CHECK(pad::state.wheelY < 1.0f);
    axis(joy, SDL_CONTROLLER_AXIS_RIGHTY, 0);
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
    CHECK(mouse() & 1);
    axis(joy, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    CHECK(mouse() == 0);

    // Direct movement is confined to gameplay; L3 toggles safely while held.
    CHECK(pad::state.settings.bindings[SDL_CONTROLLER_BUTTON_LEFTSTICK] == pad::ToggleMovement);
    gamepadSetWorldInput(true);
    pump();
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    pad::state.lastTick -= 40;
    mouse(&dx);
    float walkX, walkY;
    bool running;
    CHECK(dx == 0);
    CHECK(gamepadMovement(walkX, walkY, running) && walkX > 0.99f && running);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 28700);
    CHECK(gamepadMovement(walkX, walkY, running) && running);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 25000);
    CHECK(gamepadMovement(walkX, walkY, running) && !running);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 28700);
    CHECK(gamepadMovement(walkX, walkY, running) && !running);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSTICK, true);
    CHECK(!gamepadMovement(walkX, walkY, running));
    CHECK(pad::state.settings.directMovement == 0);
    pump();
    CHECK(pad::state.settings.directMovement == 0);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSTICK, false);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    pad::state.lastTick -= 40;
    mouse(&dx);
    CHECK(dx >= 19 && dx <= 22);
    CHECK(!gamepadMovement(walkX, walkY, running));
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSTICK, true);
    button(joy, SDL_CONTROLLER_BUTTON_LEFTSTICK, false);
    CHECK(pad::state.settings.directMovement == 1);
    gamepadSetWorldInput(false);
    pump();
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    pad::state.lastTick -= 40;
    mouse(&dx);
    CHECK(dx >= 19 && dx <= 22); // Inventory/menu fallback.
    CHECK(!gamepadMovement(walkX, walkY, running));
    gamepadSetWorldInput(true);
    pump();
    CHECK(!gamepadMovement(walkX, walkY, running)); // Release before leaving menu.
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTY, -20000);
    CHECK(gamepadMovement(walkX, walkY, running) && walkY < 0 && !running);
    pad::setOpen(true);
    CHECK(!gamepadMovement(walkX, walkY, running));
    pad::setOpen(false);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTY, 0);
    gamepadSetWorldInput(false);
    pump();

    // Gameplay pans in pixels in either stick mode; it must not emit wheel
    // events that trigger the engine's old stepped scrolling path.
    gamepadSetWorldInput(true);
    pump();
    axis(joy, SDL_CONTROLLER_AXIS_RIGHTX, 32767);
    pad::state.lastTick -= 50;
    mouse(nullptr, &wy);
    float panX, panY;
    CHECK(gamepadCameraInput(panX, panY) && panX > 280 && panY == 0);
    CHECK(wy == 0 && pad::state.wheelX == 0 && pad::state.wheelY == 0);
    CHECK(gamepadOwnsCamera());
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, -32768);
    CHECK(gamepadMovement(walkX, walkY, running) && walkX < 0);
    CHECK(gamepadCameraInput(panX, panY) && panX > 280);
    axis(joy, SDL_CONTROLLER_AXIS_LEFTX, 0);
    pad::state.settings.directMovement = 0;
    CHECK(gamepadCameraInput(panX, panY) && panX > 280);
    pad::state.settings.invertScroll = 1;
    pump();
    CHECK(gamepadCameraInput(panX, panY) && panX < -280);
    axis(joy, SDL_CONTROLLER_AXIS_RIGHTX, 0);
    pad::state.settings.invertScroll = 0;
    pad::state.settings.directMovement = 1;
    gamepadSetWorldInput(false);
    pump();
    CHECK(!gamepadCameraInput(panX, panY) && panX == 0 && panY == 0);
    CHECK(gamepadOwnsCamera()); // A parked cursor stays inert between polls.
    SDL_Event nativeMotion {};
    nativeMotion.type = SDL_MOUSEMOTION;
    nativeMotion.motion.xrel = 1;
    gamepadHandleEvent(nativeMotion);
    CHECK(!gamepadOwnsCamera()); // Deliberate physical mouse movement wins.

    // Right-stick input wins even if follow wants to move the opposite way.
    // Releasing it coasts briefly, then stays put for a stationary player.
    for (int rate : {30, 60, 144}) {
        GamepadCamera camera;
        int sx, sy;
        for (int i = 0; i < rate; ++i) {
            camera.step(-500, 200, 1.0f / rate, sx, sy, 288, 0, true);
            CHECK(sx >= 0 && sy == 0);
            CHECK(sx <= 288.0f / rate + 1);
        }
        for (int i = 0; i < rate; ++i) {
            camera.step(-500, 200, 1.0f / rate, sx, sy, 0, 0, false);
            CHECK(sx >= 0 && sy == 0);
        }
        camera.step(-500, 200, 1.0f / rate, sx, sy, 0, 0, false);
        CHECK(sx == 0 && sy == 0);
        camera.step(-500, 200, 1.0f / rate, sx, sy, 0, 0, true);
        CHECK(sx <= 0 && sy >= 0);
        int resumedX = sx;
        for (int i = 0; i < 3; ++i) {
            camera.step(-500, 200, 1.0f / rate, sx, sy, 0, 0, true);
            resumedX += sx;
        }
        CHECK(resumedX < 0); // Fractional pixels accumulate at high frame rates.
        camera.reset();
        camera.step(500, 200, 0.05f, sx, sy, -288, -288, true);
        CHECK(sx < 0 && sy < 0);
        camera.blocked();
        CHECK(camera.panVelocityX == 0 && camera.panVelocityY == 0 && camera.followDelay > 0);
        camera.reset();
        camera.step(500, 200, 0.05f, sx, sy, 0, 0, false);
        CHECK(sx == 0 && sy == 0);
    }

    // Hex steering must not drift sideways when holding cardinal directions.
    const int hx[6] = {16, 32, 16, -16, -32, -16};
    const int hy[6] = {-12, 0, 12, 12, 0, -12};
    for (auto direction : {std::make_pair(0.0f, -1.0f), std::make_pair(1.0f, 0.0f),
             std::make_pair(0.0f, 1.0f), std::make_pair(-1.0f, 0.0f), std::make_pair(0.6f, 0.8f)}) {
        GamepadSteering steering;
        int totalX = 0, totalY = 0;
        for (int i = 0; i < 40; ++i) {
            int chosen = steering.choose(direction.first, direction.second, hx, hy);
            CHECK(chosen >= 0 && chosen < 6);
            steering.commit(hx[chosen], hy[chosen]);
            totalX += hx[chosen]; totalY += hy[chosen];
        }
        CHECK(std::abs(totalX * direction.second - totalY * direction.first) <= 40);
        CHECK(totalX * direction.first + totalY * direction.second > 400);
    }

    // Camera follows smoothly at different frame rates, with bounded speed
    // and no drift within its center region.
    float finalCameraX[3], finalCameraY[3];
    int cameraCase = 0;
    for (int rate : {30, 60, 144}) {
        GamepadCamera camera;
        float errorX = 300, errorY = -120;
        for (int i = 0; i < rate * 3; ++i) {
            int sx, sy;
            camera.step(errorX, errorY, 1.0f / rate, sx, sy);
            CHECK(std::sqrt(float(sx*sx + sy*sy)) <= 240.0f / rate + 2);
            errorX -= sx; errorY -= sy;
        }
        CHECK(errorX >= 23 && errorX <= 26 && errorY >= -20 && errorY <= -17);
        finalCameraX[cameraCase] = errorX;
        finalCameraY[cameraCase++] = errorY;
        camera.reset();
        int sx, sy;
        camera.step(20, -15, 0.016f, sx, sy);
        CHECK(sx == 0 && sy == 0);
        camera.step(10000, 0, 5.0f, sx, sy);
        CHECK(sx <= 12 && sy == 0); // No jump after a stalled frame.
    }
    CHECK(std::abs(finalCameraX[0] - finalCameraX[2]) <= 2);
    CHECK(std::abs(finalCameraY[0] - finalCameraY[2]) <= 2);

    // Alt+Enter must never leak Enter (including repeats) into gameplay/UI.
    SDL_Event shortcut {};
    shortcut.type = SDL_KEYDOWN;
    shortcut.key.keysym.scancode = SDL_SCANCODE_RETURN;
    shortcut.key.keysym.mod = KMOD_ALT;
    pad::setOpen(true, 0);
    int selectedAction = pad::state.selection[0];
    CHECK(gamepadHandleEvent(shortcut));
    shortcut.key.repeat = 1;
    CHECK(gamepadHandleEvent(shortcut));
    CHECK(pad::state.open && pad::state.selection[0] == selectedAction);
    shortcut.type = SDL_KEYUP;
    shortcut.key.keysym.mod = KMOD_NONE;
    CHECK(gamepadHandleEvent(shortcut));
    CHECK(pad::state.displayKey == SDL_SCANCODE_UNKNOWN);
    pad::state.settings.borderless = 1;
    pad::state.selection[1] = 18;
    pad::adjust(1);
    CHECK(pad::state.settings.borderless == 1); // Reset controls preserves display.
    pad::setOpen(false);

    // Configuration survives restart and rejects dangerous/out-of-range data.
    pad::state.settings.speed = 750;
    pad::state.settings.bindings[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = pad::Pipboy;
    pad::saveSettings();
    CHECK(!pad::state.saveFailed);
    gamepadExit();
    gamepadInit("controller-tests", key, character);
    CHECK(pad::state.settings.speed == 750);
    CHECK(pad::state.settings.borderless == 1);
    CHECK(pad::state.settings.bindings[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] == pad::Pipboy);

    // Bad values cannot create out-of-bounds bindings, runaway cursor speed,
    // or disable the fixed recovery button.
    gamepadExit();
    config = fopen("controller.ini", "w");
    CHECK(config);
    fputs("speed=999999\ndeadzone=-500\nprecision=0\nlabels=50\nbutton_0=99999\nbutton_4=0\nbutton_999=3\n", config);
    fclose(config);
    gamepadInit("controller-tests", key, character);
    CHECK(pad::state.settings.speed == 1200);
    CHECK(pad::state.settings.deadzone == 5);
    CHECK(pad::state.settings.precision == 10);
    CHECK(pad::state.settings.labels == 4);
    CHECK(pad::state.settings.bindings[SDL_CONTROLLER_BUTTON_A] == pad::Click);
    CHECK(pad::state.settings.bindings[SDL_CONTROLLER_BUTTON_BACK] == pad::Panel);
    pad::state.settings = pad::defaults();

    // Render every page with no game assets. Pixel captures also allow visual
    // review of the actual production overlay at native and wide resolutions.
    for (auto size : { std::make_pair(640, 480), std::make_pair(1280, 720) }) {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, size.first, size.second, 32, SDL_PIXELFORMAT_ARGB8888);
        CHECK(surface);
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        CHECK(renderer);
        SDL_RenderSetLogicalSize(renderer, size.first, size.second);
        for (int page = 0; page < 4; ++page) {
            SDL_SetRenderDrawColor(renderer, 32, 29, 23, 255);
            SDL_RenderClear(renderer);
            pad::setOpen(true, page);
            gamepadRender(renderer);
            SDL_RenderPresent(renderer);
            char filename[80];
            snprintf(filename, sizeof(filename), "controller-%dx%d-page%d.bmp", size.first, size.second, page);
            CHECK(SDL_SaveBMP(surface, filename) == 0);
            float sx, sy;
            SDL_RenderGetScale(renderer, &sx, &sy);
            CHECK(sx == 1 && sy == 1);
        }
        pad::setOpen(true, 1);
        pad::state.selection[1] = 20;
        gamepadRender(renderer);
        SDL_RenderPresent(renderer);
        char displayFile[80];
        snprintf(displayFile, sizeof(displayFile), "display-%dx%d.bmp", size.first, size.second);
        CHECK(SDL_SaveBMP(surface, displayFile) == 0);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
    }

    pad::setOpen(false);
    pump();
    button(joy, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(mouse() == 1);
    // Unplug during a drag: release and keep the keyboard available.
    SDL_JoystickClose(joy);
    CHECK(SDL_JoystickDetachVirtual(index) == 0);
    pump();
    CHECK(mouse() == 0);
    CHECK(!gamepadOverlayOpen());
    gamepadExit();
    SDL_Quit();
    printf("PASS: %d controller checks (SDL virtual device, focus, drag, shortcuts, text, settings, render, unplug).\n", checks);
    return 0;
}
