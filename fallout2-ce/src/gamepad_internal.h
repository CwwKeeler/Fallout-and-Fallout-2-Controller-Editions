#ifndef FALLOUT_GAMEPAD_INTERNAL_H_
#define FALLOUT_GAMEPAD_INTERNAL_H_

#include "gamepad.h"

#include <array>
#include <string>
#include <vector>

namespace fallout {
namespace pad {

enum Action {
    None, Click, RightClick, Confirm, Cancel, Panel, Keyboard, Run,
    Inventory, Character, Pipboy, Automap, Skilldex, Rest, SwapHands,
    WeaponMode, Combat, EndTurn, Center, Save, Load, Sneak, Lockpick,
    Steal, Traps, FirstAid, Doctor, Science, Repair, ToggleMovement, ActionCount,
};

struct ActionInfo {
    const char* name;
    SDL_Scancode key;
};
extern const ActionInfo actions[ActionCount];

struct Settings {
    int speed = 500;
    int deadzone = 18;
    int precision = 25;
    int scrollSpeed = 9;
    int invertScroll = 0;
    int swapSticks = 0;
    int labels = 0; // Auto, Xbox, PlayStation, Nintendo, position.
    int hints = 1;
    int directMovement = 1;
    int borderless = -1; // Unset: use the engine's initial window mode.
    std::array<int, SDL_CONTROLLER_BUTTON_MAX> bindings {};
};

struct Device {
    SDL_GameController* handle;
    SDL_JoystickID id;
};

struct State {
    bool initialized = false;
    bool focused = true;
    bool textInput = false;
    bool used = false;
    bool open = false;
    bool caps = false;
    bool replaceText = true;
    bool saveFailed = false;
    bool displayFailed = false;
    SDL_Scancode displayKey = SDL_SCANCODE_UNKNOWN;
    SDL_Window* window = nullptr;
    bool waitNeutral = true;
    bool worldInput = false;
    bool worldContext = false;
    float walkX = 0;
    float walkY = 0;
    float cameraX = 0;
    float cameraY = 0;
    Uint32 cameraTick = 0;
    bool cameraUsed = false;
    bool running = false;
    int tab = 0;
    int selection[4] {};
    int remapButton = -1;
    int active = -1;
    int unmapped = 0;
    Uint32 lastTick = 0;
    Uint32 lastUse = 0;
    Uint32 navTick = 0;
    int navDirection = -1;
    float moveX = 0;
    float moveY = 0;
    float wheelX = 0;
    float wheelY = 0;
    int mouseButtons = 0;
    bool triggerDown = false;
    bool keyDown[SDL_NUM_SCANCODES] {};
    bool buttons[SDL_CONTROLLER_BUTTON_MAX] {};
    bool blocked[SDL_CONTROLLER_BUTTON_MAX] {};
    Settings settings;
    std::string configPath;
    std::string typed;
    std::string pendingText;
    std::vector<Device> devices;
    GamepadKeyHandler keys = nullptr;
    GamepadTextHandler text = nullptr;
};

extern State state;
Settings defaults();
void trace(const char* message);
void saveSettings();
void releaseInputs();
void setOpen(bool open, int tab = -1);
void navigate(int dx, int dy);
void activate();
void changeTab(int delta);
void adjust(int delta);
void toggleDisplay();
void pulse(SDL_Scancode key);
const char* buttonName(int button);
const char* deviceName();
void render(SDL_Renderer* renderer);

} // namespace pad
} // namespace fallout

#endif
