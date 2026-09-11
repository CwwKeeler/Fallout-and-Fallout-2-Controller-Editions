#ifndef FALLOUT_GAMEPAD_H_
#define FALLOUT_GAMEPAD_H_

#include <SDL.h>

namespace fallout {

// SDL-only input/overlay module, shared by both Community Editions.
using GamepadKeyHandler = void (*)(SDL_Scancode key, bool down);
using GamepadTextHandler = void (*)(int character);
void gamepadInit(const char* gameId, GamepadKeyHandler keys, GamepadTextHandler text, SDL_Window* window = nullptr);
void gamepadExit();
bool gamepadHandleEvent(const SDL_Event& event);
void gamepadUpdate();
void gamepadMouse(int& x, int& y, int& buttons, int& wheelX, int& wheelY);
void gamepadRender(SDL_Renderer* renderer);
void gamepadTextInput(bool active);
bool gamepadOverlayOpen();
bool gamepadDispatchText();
// World input is enabled only around player input polls, never enemy AI or UI.
void gamepadSetWorldInput(bool active);
bool gamepadMovement(float& x, float& y, bool& running);
// Right-stick world camera velocity in logical pixels per second. Menus keep
// their wheel input; world panning never emits synthetic wheel events. Returns
// whether world camera control is available (including a centered stick).
bool gamepadCameraInput(float& x, float& y);
bool gamepadOwnsCamera();
// Hide only the world movement marker; targeting and UI cursors stay native.
bool gamepadHidesMovementCursor();
void gamepadWorldBegin();
void gamepadWorldEnd();

} // namespace fallout

#endif
