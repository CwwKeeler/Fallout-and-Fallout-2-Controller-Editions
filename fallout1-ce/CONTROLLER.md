# Controller support

This modified Community Edition adds native SDL controller input and a Fallout-style
control panel. Keep the original game data installed and use this executable in
place of the Community Edition executable. It does not contain the original game.

Connect a controller. Left stick moves your character in gameplay and the cursor
in menus. Click **L3** to toggle between character movement and point-and-click.
The bottom face button clicks.
Hold it to drag inventory items or open an object's action list. The right face
button right-clicks to change cursor modes. **Back / Share / Minus** opens the
control panel, and **F11** opens it from a keyboard.

**Alt+Enter** switches between borderless fullscreen and a window. You can also
change **Display Mode** on page 3 of the panel's Settings tab. The choice saves
automatically. Fullscreen uses your current desktop resolution and refresh
rate; Alt-Tab lets you leave and return without switching the monitor mode.
The picture keeps its proportions, with black bars where needed. Windows uses
Direct3D 11 and vertical sync where available, with a renderer fallback.

The saved `borderless` value in `controller.ini` overrides the initial window
mode in `f1_res.ini` / `f2_res.ini`: `1` is borderless, `0` is windowed. Without
that setting, the resolution file's `WINDOWED=0` selects borderless fullscreen.

| Input | Default action |
|---|---|
| Left stick | Character movement / cursor |
| Right stick | Scroll view or list |
| Left trigger | Precision cursor |
| Right trigger | Left click / drag |
| Bottom face: A / Cross / Nintendo B | Left click / drag |
| Right face: B / Circle / Nintendo A | Right click / cursor mode |
| Left face: X / Square / Nintendo Y | Enter / confirm / finish |
| Top face: Y / Triangle / Nintendo X | Actions panel |
| Left shoulder | Hold Shift: run/walk modifier |
| Right shoulder | Inventory |
| Left stick press | Toggle character movement / cursor |
| Right stick press | On-screen keyboard |
| Start / Options / Plus | Escape / close / game options |
| Back / Share / Minus | Controller panel |
| D-pad | Arrow keys |

In the panel, use the D-pad or left stick to select, bottom face to activate,
right face to close, and shoulders to change tabs. Settings use left/right to
adjust. Keyboard arrows, Enter, Escape, and Tab also work. Actions invoke the
original screen-specific hotkeys; gameplay shortcuts are intended for gameplay.

Partial stick deflection walks; full deflection runs. Releasing the stick stops
after the current hex step. The animation continues across hexes, the camera
follows gradually, and small changes in pressure do not repeatedly switch gait.
Movement uses native collision and combat AP. Menus automatically use cursor control and restore the chosen
gameplay mode on return. Center the stick after switching modes or closing a
menu. Settings includes Gameplay Stick Mode; Center View remains in Actions.

The right stick pans the world camera smoothly in either gameplay mode. Manual
panning takes priority over follow, including while you walk with the left stick.
Release it to ease to a stop. Follow resumes after a short pause if you keep
walking; standing still leaves the view where you put it. A parked edge cursor
does not scroll against the controller. Menus retain right-stick list scrolling.
Scroll Speed and Reverse Scroll also apply to world panning.

Direct controller movement hides the red movement marker, even after releasing
the stick. L3 or moving the real mouse restores it. Interaction and targeting
cursors remain available.

Settings cover speed, radial deadzone, precision, scrolling, stick swapping,
controller choice, prompts, button labels, and nine button bindings. Back is
always available to recover from custom bindings. Settings save to a local
`controller.ini` if present, otherwise SDL's per-user preference folder.

The on-screen keyboard opens automatically in text fields after controller use.
R3 opens it manually. `SEND` types the buffer; `DONE` also presses Enter. `MODE`
switches replace/append for a text field. `DEL` edits the buffer, and `BKSP`
backspaces in the game. Left face deletes; top face toggles case. ASCII text is
supported, with a 24-character buffer and the game's own field length limits.

SDL 2.32.10 and the included SDL_GameControllerDB cover many USB/Bluetooth Xbox,
PlayStation, Nintendo, Steam virtual, 8BitDo, and generic controllers. Support
depends on SDL and the operating system; unmapped hardware requires an SDL2
mapping in `gamecontrollerdb.txt` beside the executable. Restart after editing.
`SDL_GAMECONTROLLERCONFIG` environment mappings also work. Button label styles
can be chosen manually when automatic detection does not match a device.

Keyboard and mouse continue to work. Disconnecting or losing focus releases
controller inputs. Release held buttons and center the sticks/triggers before
resuming after a focus change. If another remapper duplicates input, configure
it to produce plain gamepad input for this executable.

Build with the repository's CMake process. Set `FALLOUT_BUILD_CONTROLLER_TESTS=ON`
and run CTest to exercise a virtual SDL controller without game assets. Normal
builds exclude the opt-in `FALLOUT_CONTROLLER_SMOKE_DRIVER` integration driver.
Only Windows x64 builds have been validated in this change; the SDL module is
portable but other operating systems still need build and hardware testing.

Mapping database: [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB),
with its license in `third_party/controller-db/LICENSE`. Original engine code
retains `LICENSE.md`. New controller visuals are drawn from code and need no
additional art assets.
