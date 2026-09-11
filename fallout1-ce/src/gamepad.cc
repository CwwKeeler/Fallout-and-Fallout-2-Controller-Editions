#include "gamepad_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace fallout {
#ifdef FALLOUT_CONTROLLER_SMOKE_DRIVER
void gamepadSmokeInit();
void gamepadSmokeUpdate();
void gamepadSmokeRender(SDL_Renderer* renderer);
#endif
namespace pad {

State state;

void trace(const char* message)
{
    if (!SDL_getenv("FALLOUT_CONTROLLER_TRACE")) return;
    FILE* file = fopen("controller-trace.log", "a");
    if (!file) return;
    fprintf(file, "%u %s\n", SDL_GetTicks(), message);
    fclose(file);
}

const ActionInfo actions[ActionCount] = {
    { "UNASSIGNED", SDL_SCANCODE_UNKNOWN },
    { "CLICK / DRAG", SDL_SCANCODE_UNKNOWN },
    { "CURSOR MODE", SDL_SCANCODE_UNKNOWN },
    { "ENTER / DONE", SDL_SCANCODE_RETURN },
    { "ESC / OPTIONS", SDL_SCANCODE_ESCAPE },
    { "CONTROL PANEL", SDL_SCANCODE_UNKNOWN },
    { "KEYBOARD", SDL_SCANCODE_UNKNOWN },
    { "HOLD TO RUN", SDL_SCANCODE_LSHIFT },
    { "INVENTORY", SDL_SCANCODE_I },
    { "CHARACTER", SDL_SCANCODE_C },
    { "PIP-BOY", SDL_SCANCODE_P },
    { "AUTOMAP", SDL_SCANCODE_TAB },
    { "SKILLDEX", SDL_SCANCODE_S },
    { "REST", SDL_SCANCODE_Z },
    { "SWAP HANDS", SDL_SCANCODE_B },
    { "WEAPON MODE", SDL_SCANCODE_N },
    { "START COMBAT", SDL_SCANCODE_A },
    { "END TURN", SDL_SCANCODE_SPACE },
    { "CENTER VIEW", SDL_SCANCODE_HOME },
    { "SAVE GAME", SDL_SCANCODE_F4 },
    { "LOAD GAME", SDL_SCANCODE_F5 },
    { "SNEAK", SDL_SCANCODE_1 },
    { "LOCKPICK", SDL_SCANCODE_2 },
    { "STEAL", SDL_SCANCODE_3 },
    { "TRAPS", SDL_SCANCODE_4 },
    { "FIRST AID", SDL_SCANCODE_5 },
    { "DOCTOR", SDL_SCANCODE_6 },
    { "SCIENCE", SDL_SCANCODE_7 },
    { "REPAIR", SDL_SCANCODE_8 },
    { "MOVE / CURSOR", SDL_SCANCODE_UNKNOWN },
};

Settings defaults()
{
    Settings s;
    s.bindings[SDL_CONTROLLER_BUTTON_A] = Click;
    s.bindings[SDL_CONTROLLER_BUTTON_B] = RightClick;
    s.bindings[SDL_CONTROLLER_BUTTON_X] = Confirm;
    s.bindings[SDL_CONTROLLER_BUTTON_Y] = Panel;
    s.bindings[SDL_CONTROLLER_BUTTON_BACK] = Panel;
    s.bindings[SDL_CONTROLLER_BUTTON_START] = Cancel;
    s.bindings[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = Run;
    s.bindings[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = Inventory;
    s.bindings[SDL_CONTROLLER_BUTTON_LEFTSTICK] = ToggleMovement;
    s.bindings[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = Keyboard;
    return s;
}

void toggleDisplay()
{
    if (!state.window) return;
    bool borderless = (SDL_GetWindowFlags(state.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN_DESKTOP;
    state.displayFailed = SDL_SetWindowFullscreen(state.window, borderless ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0;
    if (state.displayFailed) { trace(SDL_GetError()); return; }
    state.settings.borderless = borderless;
    releaseInputs();
    state.waitNeutral = true;
    SDL_GetRelativeMouseState(nullptr, nullptr);
    saveSettings();
}

static void loadSettings()
{
    state.settings = defaults();
    FILE* file = fopen(state.configPath.c_str(), "r");
    if (!file) return;
    bool hasMovementSetting = false;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char name[64];
        int value;
        if (sscanf(line, " %63[^=]=%d", name, &value) != 2) continue;
        auto& s = state.settings;
        if (!strcmp(name, "speed")) s.speed = std::clamp(value, 100, 1200);
        else if (!strcmp(name, "deadzone")) s.deadzone = std::clamp(value, 5, 40);
        else if (!strcmp(name, "precision")) s.precision = std::clamp(value, 10, 60);
        else if (!strcmp(name, "scroll_speed")) s.scrollSpeed = std::clamp(value, 2, 20);
        else if (!strcmp(name, "invert_scroll")) s.invertScroll = value != 0;
        else if (!strcmp(name, "swap_sticks")) s.swapSticks = value != 0;
        else if (!strcmp(name, "labels")) s.labels = std::clamp(value, 0, 4);
        else if (!strcmp(name, "hints")) s.hints = value != 0;
        else if (!strcmp(name, "borderless")) s.borderless = std::clamp(value, -1, 1);
        else if (!strcmp(name, "direct_movement")) { s.directMovement = value != 0; hasMovementSetting = true; }
        else {
            int button = -1;
            if (sscanf(name, "button_%d", &button) == 1
                && button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX
                && button != SDL_CONTROLLER_BUTTON_BACK
                && value >= 0 && value < ActionCount) s.bindings[button] = value;
        }
    }
    fclose(file);
    // Migrate the old default L3 binding, preserving other custom bindings.
    if (!hasMovementSetting && state.settings.bindings[SDL_CONTROLLER_BUTTON_LEFTSTICK] == Center) {
        state.settings.bindings[SDL_CONTROLLER_BUTTON_LEFTSTICK] = ToggleMovement;
    }
}

void saveSettings()
{
    // Keep the previous configuration intact if writing fails.
    std::string temp = state.configPath + ".tmp";
    FILE* file = fopen(temp.c_str(), "w");
    state.saveFailed = true;
    if (!file) return;
    const auto& s = state.settings;
    fprintf(file, "# Fallout CE controller settings\nspeed=%d\ndeadzone=%d\nprecision=%d\nscroll_speed=%d\ninvert_scroll=%d\nswap_sticks=%d\nlabels=%d\nhints=%d\n",
        s.speed, s.deadzone, s.precision, s.scrollSpeed, s.invertScroll, s.swapSticks, s.labels, s.hints);
    fprintf(file, "direct_movement=%d\n", s.directMovement);
    fprintf(file, "borderless=%d\n", s.borderless);
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) fprintf(file, "button_%d=%d\n", i, s.bindings[i]);
    bool failed = ferror(file) != 0;
    if (fclose(file) != 0) failed = true;
    if (failed) return;
#ifdef _WIN32
    // C rename cannot replace a file on Windows. SDL's platform headers are
    // intentionally not exposed here; keep a recoverable backup instead.
    std::string backup = state.configPath + ".bak";
    remove(backup.c_str());
    rename(state.configPath.c_str(), backup.c_str());
    if (rename(temp.c_str(), state.configPath.c_str()) != 0) {
        rename(backup.c_str(), state.configPath.c_str());
        return;
    }
    remove(backup.c_str());
#else
    if (rename(temp.c_str(), state.configPath.c_str()) != 0) return;
#endif
    state.saveFailed = false;
}

void releaseInputs()
{
    for (int key = 0; key < SDL_NUM_SCANCODES; ++key) {
        if (state.keyDown[key] && state.keys) state.keys(static_cast<SDL_Scancode>(key), false);
        state.keyDown[key] = false;
    }
    state.mouseButtons = 0;
    state.triggerDown = false;
    state.moveX = state.moveY = state.wheelX = state.wheelY = 0;
    state.walkX = state.walkY = 0;
    state.cameraX = state.cameraY = 0;
    state.cameraUsed = false;
    state.running = false;
}

void setOpen(bool open, int tab)
{
    trace(open ? "Open controller panel" : "Close controller panel");
    releaseInputs();
    state.open = open;
    if (tab >= 0) state.tab = tab;
    state.remapButton = -1;
    state.navDirection = -1;
    state.waitNeutral = true;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) state.blocked[i] = state.buttons[i];
    state.lastUse = SDL_GetTicks();
    state.used = true;
}

void pulse(SDL_Scancode key)
{
    if (!state.keys || key == SDL_SCANCODE_UNKNOWN) return;
    state.keys(key, true);
    state.keys(key, false);
}

static SDL_GameController* activeHandle()
{
    if (state.active < 0 || state.active >= static_cast<int>(state.devices.size())) return nullptr;
    return state.devices[state.active].handle;
}

const char* deviceName()
{
    auto* handle = activeHandle();
    if (!handle) return state.unmapped ? "UNMAPPED DEVICE - SEE HELP" : "NO CONTROLLER CONNECTED";
    const char* name = SDL_GameControllerName(handle);
    return name ? name : "SDL CONTROLLER";
}

const char* buttonName(int button)
{
    int style = state.settings.labels;
    if (style == 0) {
        auto type = SDL_GameControllerGetType(activeHandle());
        if (type == SDL_CONTROLLER_TYPE_PS3 || type == SDL_CONTROLLER_TYPE_PS4 || type == SDL_CONTROLLER_TYPE_PS5) style = 2;
        else if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO
            || type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
            || type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
            || type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) style = 3;
        else if (type == SDL_CONTROLLER_TYPE_UNKNOWN) style = 4;
        else style = 1;
    }
    static const char* faces[4][4] = {
        { "A", "B", "X", "Y" },
        { "CROSS", "CIRCLE", "SQUARE", "TRIANGLE" },
        { "B", "A", "Y", "X" },
        { "SOUTH", "EAST", "WEST", "NORTH" },
    };
    if (button >= 0 && button <= 3) return faces[style - 1][button];
    switch (button) {
    case SDL_CONTROLLER_BUTTON_BACK: return style == 2 ? "SHARE" : style == 3 ? "MINUS" : "BACK";
    case SDL_CONTROLLER_BUTTON_START: return style == 2 ? "OPTIONS" : style == 3 ? "PLUS" : "START";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return style == 2 ? "L1" : "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return style == 2 ? "R1" : "RB";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
    default: return "BUTTON";
    }
}

static void addDevice(int index)
{
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    for (const auto& device : state.devices) if (device.id == id) return;
    if (!SDL_IsGameController(index)) return;
    SDL_GameController* handle = SDL_GameControllerOpen(index);
    if (!handle) return;
    state.devices.push_back({ handle, id });
    if (state.active < 0) {
        state.active = static_cast<int>(state.devices.size()) - 1;
        std::fill(std::begin(state.blocked), std::end(state.blocked), true);
        state.waitNeutral = true;
        state.lastUse = SDL_GetTicks();
        state.used = true;
    }
}

static void scanDevices()
{
    state.unmapped = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) ++state.unmapped;
        else addDevice(i);
    }
}

static void buttonEdge(int button, bool down)
{
    if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return;
    if (!down) state.blocked[button] = false;
    if (state.buttons[button] == down) return;
    state.buttons[button] = down;
    if (!down) {
        state.blocked[button] = false;
        return;
    }
    if (!state.focused || state.blocked[button]) return;
    state.pendingText.clear();
    state.lastUse = SDL_GetTicks();
    state.used = true;
    if (button == SDL_CONTROLLER_BUTTON_BACK) {
        setOpen(!state.open);
        return;
    }
    if (state.open) {
        switch (button) {
        case SDL_CONTROLLER_BUTTON_A: activate(); break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_START: setOpen(false); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: changeTab(-1); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: changeTab(1); break;
        case SDL_CONTROLLER_BUTTON_X:
            if (state.tab == 2) { if (!state.typed.empty()) state.typed.pop_back(); }
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            if (state.tab == 2) state.caps = !state.caps;
            break;
        default: break;
        }
        return;
    }
    int action = state.settings.bindings[button];
    if (action == Panel) setOpen(true, 0);
    else if (action == Keyboard) { state.typed.clear(); setOpen(true, 2); }
    else if (action == ToggleMovement) {
        state.settings.directMovement = !state.settings.directMovement;
        releaseInputs();
        state.waitNeutral = true;
        saveSettings();
    }
    else if (action != Run) pulse(actions[action].key);
}

static float normalized(Sint16 value)
{
    return value < 0 ? value / 32768.0f : value / 32767.0f;
}

static void stick(SDL_GameController* handle, SDL_GameControllerAxis axis, float& x, float& y)
{
    x = normalized(SDL_GameControllerGetAxis(handle, axis));
    y = normalized(SDL_GameControllerGetAxis(handle, static_cast<SDL_GameControllerAxis>(axis + 1)));
    float length = std::sqrt(x * x + y * y);
    float deadzone = state.settings.deadzone / 100.0f;
    if (length <= deadzone) { x = y = 0; return; }
    // Radial deadzone preserves diagonals; quadratic response keeps small
    // inventory targets reachable without sacrificing full-stick speed.
    float magnitude = (std::min(length, 1.0f) - deadzone) / (1.0f - deadzone);
    float scale = magnitude * magnitude / length;
    x *= scale;
    y *= scale;
}

static void updateNavigation(float x, float y, Uint32 now)
{
    int dx = (x > 0.3f) - (x < -0.3f);
    int dy = (y > 0.3f) - (y < -0.3f);
    if (state.buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT]) dx = -1;
    if (state.buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]) dx = 1;
    if (state.buttons[SDL_CONTROLLER_BUTTON_DPAD_UP]) dy = -1;
    if (state.buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN]) dy = 1;
    if (dy) dx = 0;
    int direction = dy < 0 ? 0 : dy > 0 ? 1 : dx < 0 ? 2 : dx > 0 ? 3 : -1;
    if (direction == -1) { state.navDirection = -1; return; }
    if (direction != state.navDirection || static_cast<Sint32>(now - state.navTick) >= 0) {
        navigate(dx, dy);
        state.navTick = now + (direction != state.navDirection ? 350 : 110);
        state.navDirection = direction;
    }
}

} // namespace pad

void gamepadInit(const char* gameId, GamepadKeyHandler keys, GamepadTextHandler text, SDL_Window* window)
{
    using namespace pad;
    if (state.initialized) return;
    state = State {};
    state.keys = keys;
    state.text = text;
    state.window = window;
    // Use consistent physical face-button positions; Nintendo labels are
    // translated separately when drawing prompts.
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) { trace(SDL_GetError()); return; }
    state.initialized = true;
    trace("Controller subsystem initialized");
    state.lastTick = SDL_GetTicks();
    char* prefPath = SDL_GetPrefPath("FalloutCE", gameId);
    state.configPath = prefPath ? std::string(prefPath) + "controller.ini" : "controller.ini";
    SDL_free(prefPath);
    // An explicit local file enables a portable, per-install configuration.
    FILE* portable = fopen("controller.ini", "r");
    if (portable) { fclose(portable); state.configPath = "controller.ini"; }
    loadSettings();
    if (window) {
        int wanted = state.settings.borderless;
        if (wanted >= 0) {
            state.displayFailed = SDL_SetWindowFullscreen(window, wanted ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0;
        }
        state.settings.borderless = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    // External mappings augment SDL's built-in database (including SDL's
    // SDL_GAMECONTROLLERCONFIG environment override).
    char* base = SDL_GetBasePath();
    if (base) {
        std::string path = std::string(base) + "gamecontrollerdb.txt";
        SDL_GameControllerAddMappingsFromFile(path.c_str());
        SDL_free(base);
    }
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    SDL_GameControllerEventState(SDL_ENABLE);
    scanDevices();
#ifdef FALLOUT_CONTROLLER_SMOKE_DRIVER
    gamepadSmokeInit();
#endif
}

void gamepadExit()
{
    using namespace pad;
    if (!state.initialized) return;
    releaseInputs();
    for (const auto& device : state.devices) SDL_GameControllerClose(device.handle);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    state = State {};
}

bool gamepadHandleEvent(const SDL_Event& event)
{
    using namespace pad;
    if (!state.initialized) return false;
    // Consume the entire shortcut, including repeats and its eventual release,
    // so Alt+Enter cannot also confirm a menu or end a combat encounter.
    if (event.type == SDL_KEYDOWN
        && (event.key.keysym.scancode == SDL_SCANCODE_RETURN || event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER)
        && ((event.key.keysym.mod & KMOD_ALT) || state.displayKey == event.key.keysym.scancode)) {
        if (!event.key.repeat) {
            state.displayKey = event.key.keysym.scancode;
            toggleDisplay();
        }
        return true;
    }
    if (event.type == SDL_KEYUP && event.key.keysym.scancode == state.displayKey) {
        state.displayKey = SDL_SCANCODE_UNKNOWN;
        return true;
    }
    switch (event.type) {
    case SDL_CONTROLLERDEVICEADDED: scanDevices(); return true;
    case SDL_CONTROLLERDEVICEREMOVED:
        for (size_t i = 0; i < state.devices.size(); ++i) {
            if (state.devices[i].id != event.cdevice.which) continue;
            bool active = state.active == static_cast<int>(i);
            if (active) {
                state.pendingText.clear();
                setOpen(false);
                std::fill(std::begin(state.buttons), std::end(state.buttons), false);
            }
            SDL_GameControllerClose(state.devices[i].handle);
            state.devices.erase(state.devices.begin() + i);
            if (active) state.active = state.devices.empty() ? -1 : 0;
            else if (state.active > static_cast<int>(i)) --state.active;
            break;
        }
        return true;
    case SDL_CONTROLLERDEVICEREMAPPED: releaseInputs(); state.waitNeutral = true; return true;
    case SDL_JOYDEVICEADDED:
    case SDL_JOYDEVICEREMOVED: scanDevices(); return false;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        if (state.active >= 0 && state.devices[state.active].id == event.cbutton.which) {
            buttonEdge(event.cbutton.button, event.cbutton.state == SDL_PRESSED);
        }
        return true;
    case SDL_CONTROLLERAXISMOTION: return true;
    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            state.displayKey = SDL_SCANCODE_UNKNOWN;
            trace("Window focus lost");
            state.focused = false;
            state.pendingText.clear();
            setOpen(false);
        } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            trace("Window focus gained");
            state.focused = true;
            std::fill(std::begin(state.blocked), std::end(state.blocked), true);
            state.waitNeutral = true;
            state.lastTick = SDL_GetTicks();
        }
        return false;
    case SDL_KEYDOWN:
        state.pendingText.clear();
        if (event.key.keysym.scancode == SDL_SCANCODE_F11 && !event.key.repeat) {
            setOpen(!state.open);
            return true;
        }
        if (!state.open) { state.used = false; return false; }
        switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE: setOpen(false); break;
        case SDL_SCANCODE_RETURN: activate(); break;
        case SDL_SCANCODE_UP: navigate(0, -1); break;
        case SDL_SCANCODE_DOWN: navigate(0, 1); break;
        case SDL_SCANCODE_LEFT: navigate(-1, 0); break;
        case SDL_SCANCODE_RIGHT: navigate(1, 0); break;
        case SDL_SCANCODE_TAB: changeTab(1); break;
        case SDL_SCANCODE_BACKSPACE: if (state.tab == 2 && !state.typed.empty()) state.typed.pop_back(); break;
        default: break;
        }
        return true;
    // Always allow real key releases through to the engine, even while the
    // overlay is open, so opening it cannot strand a held keyboard key.
    case SDL_KEYUP: return event.key.keysym.scancode == SDL_SCANCODE_F11;
    case SDL_MOUSEMOTION:
        if (event.motion.xrel || event.motion.yrel) state.used = false;
        return state.open;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL: return state.open;
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
    case SDL_TEXTINPUT: return state.open;
    default: return false;
    }
}

void gamepadUpdate()
{
    using namespace pad;
    if (!state.initialized) return;
#ifdef FALLOUT_CONTROLLER_SMOKE_DRIVER
    gamepadSmokeUpdate();
#endif
    Uint32 now = SDL_GetTicks();
    float dt = std::min(now - state.lastTick, Uint32(50)) / 1000.0f;
    state.lastTick = now;
    if (state.worldContext != state.worldInput) {
        state.worldContext = state.worldInput;
        releaseInputs();
        state.waitNeutral = true;
    }
    SDL_GameController* handle = activeHandle();
    if (!state.focused || !handle || !SDL_GameControllerGetAttached(handle)) { releaseInputs(); return; }
    // Polling also services the legacy mouse-only drag/hold loops.
    SDL_GameControllerUpdate();
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        buttonEdge(i, SDL_GameControllerGetButton(handle, static_cast<SDL_GameControllerButton>(i)) != 0);
    }
    float x, y, rx, ry;
    stick(handle, state.settings.swapSticks ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX, x, y);
    stick(handle, state.settings.swapSticks ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX, rx, ry);
    float trigger = normalized(SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    if (state.waitNeutral) {
        if (x == 0 && y == 0 && rx == 0 && ry == 0 && trigger < 0.2f) state.waitNeutral = false;
        else return;
    }
    if (state.open) {
        updateNavigation(x, y, now);
        return;
    }
    bool keys[SDL_NUM_SCANCODES] {};
    state.mouseButtons = 0;
    if (trigger > 0.55f) state.triggerDown = true;
    if (trigger < 0.35f) state.triggerDown = false;
    if (state.triggerDown) state.mouseButtons |= 1;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        if (!state.buttons[i] || state.blocked[i]) continue;
        int action = state.settings.bindings[i];
        if (action == Click) state.mouseButtons |= 1;
        else if (action == RightClick) state.mouseButtons |= 2;
        else if (action == Run) keys[SDL_SCANCODE_LSHIFT] = true;
    }
    static const SDL_Scancode directions[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT };
    for (int i = 0; i < 4; ++i) keys[directions[i]] = state.buttons[SDL_CONTROLLER_BUTTON_DPAD_UP + i] && !state.blocked[SDL_CONTROLLER_BUTTON_DPAD_UP + i];
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        if (keys[i] != state.keyDown[i] && state.keys) state.keys(static_cast<SDL_Scancode>(i), keys[i]);
        state.keyDown[i] = keys[i];
    }
    if (x || y || rx || ry || state.triggerDown) { state.lastUse = now; state.used = true; }
    float precision = SDL_GameControllerGetAxis(handle, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000 ? state.settings.precision / 100.0f : 1.0f;
    if (state.worldContext && state.settings.directMovement && !state.textInput) {
        state.walkX = x;
        state.walkY = y;
        float strength = x * x + y * y;
        if (keys[SDL_SCANCODE_LSHIFT] || strength > 0.55f) state.running = true;
        else if (strength < 0.40f) state.running = false;
    } else {
        state.walkX = state.walkY = 0;
        state.moveX += x * state.settings.speed * precision * dt;
        state.moveY += y * state.settings.speed * precision * dt;
    }
    float scroll = state.settings.scrollSpeed * (state.settings.invertScroll ? -1.0f : 1.0f);
    state.cameraX = state.cameraY = 0;
    if (state.worldContext && !state.textInput) {
        state.cameraX = rx * scroll * 32;
        state.cameraY = ry * scroll * 32;
        if (rx || ry) { state.cameraTick = now; state.cameraUsed = true; }
    } else {
        state.wheelX += rx * scroll * dt;
        state.wheelY -= ry * scroll * dt;
    }
}

void gamepadMouse(int& x, int& y, int& buttons, int& wheelX, int& wheelY)
{
    using namespace pad;
    gamepadUpdate();
    if (state.open) { x = y = buttons = wheelX = wheelY = 0; return; }
    int dx = static_cast<int>(state.moveX);
    int dy = static_cast<int>(state.moveY);
    int wx = static_cast<int>(state.wheelX);
    int wy = static_cast<int>(state.wheelY);
    x += dx; y += dy; wheelX += wx; wheelY += wy;
    buttons |= state.mouseButtons;
    state.moveX -= dx; state.moveY -= dy;
    state.wheelX -= wx; state.wheelY -= wy;
}

bool gamepadOverlayOpen() { return pad::state.open; }

void gamepadSetWorldInput(bool active) { pad::state.worldInput = active; }

bool gamepadMovement(float& x, float& y, bool& running)
{
    const auto& s = pad::state;
    x = y = 0;
    running = false;
    if (!s.worldInput || !s.worldContext || !s.settings.directMovement
        || s.open || s.textInput || !s.focused || s.waitNeutral || s.active < 0) return false;
    x = s.walkX;
    y = s.walkY;
    running = s.running;
    return x != 0 || y != 0;
}

bool gamepadCameraInput(float& x, float& y)
{
    const auto& s = pad::state;
    x = y = 0;
    if (!s.worldInput || !s.worldContext || s.open || s.textInput
        || !s.focused || s.waitNeutral || s.active < 0) return false;
    x = s.cameraX;
    y = s.cameraY;
    return true;
}

bool gamepadOwnsCamera()
{
    const auto& s = pad::state;
    // Keep a parked edge cursor inert between player polls (including enemy
    // animations). Actual mouse movement gives control back via state.used.
    return s.used && !s.open && !s.textInput && s.focused
        && s.active >= 0 && (s.settings.directMovement
            || (s.worldContext && s.cameraUsed && SDL_GetTicks() - s.cameraTick < 600));
}

bool gamepadHidesMovementCursor()
{
    const auto& s = pad::state;
    // Retain this between input polls and after releasing the stick. L3 or
    // actual mouse movement returns the normal movement pointer immediately.
    return s.used && s.worldContext && s.settings.directMovement
        && !s.open && !s.textInput && s.focused && s.active >= 0;
}

bool gamepadDispatchText()
{
    auto& s = pad::state;
    if (s.open || s.pendingText.empty()) return false;
    unsigned char ch = s.pendingText.front();
    s.pendingText.erase(0, 1);
    if (s.text) s.text(ch);
    return true;
}

void gamepadTextInput(bool active)
{
    using namespace pad;
    state.textInput = active;
    if (!active) state.pendingText.clear();
    if (active && state.used && activeHandle()) {
        state.typed.clear();
        setOpen(true, 2);
    } else if (!active && state.open && state.tab == 2) setOpen(false);
}

void gamepadRender(SDL_Renderer* renderer)
{
    if (pad::state.initialized && renderer) pad::render(renderer);
#ifdef FALLOUT_CONTROLLER_SMOKE_DRIVER
    gamepadSmokeRender(renderer);
#endif
}

} // namespace fallout
