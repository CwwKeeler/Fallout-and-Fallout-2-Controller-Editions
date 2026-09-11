"""Keep the SDL-only controller module identical in both engines."""
from pathlib import Path

root = Path(__file__).resolve().parent
for name in ("gamepad.h", "gamepad_internal.h", "gamepad.cc", "gamepad_ui.cc", "gamepad_movement.h"):
    (root / "fallout2-ce/src" / name).write_bytes((root / "fallout1-ce/src" / name).read_bytes())
for name in ("CMakeLists.txt", "gamepad_test.cc", "gamepad_smoke_driver.cc", "display_smoke.h", "camera_smoke.h"):
    target = root / "fallout2-ce/tests" / name
    target.parent.mkdir(exist_ok=True)
    target.write_bytes((root / "fallout1-ce/tests" / name).read_bytes())

def edit(path, before, after):
    p = root / path
    text = p.read_text()
    if after in text:
        return
    if before not in text:
        raise RuntimeError(f"Cannot find integration point in {path}: {before}")
    p.write_text(text.replace(before, after, 1), newline="\n")

for game, prefix, init, shutdown, clear, process, disabled, enqueue in (
    ("fallout1-ce", "plib/gnw/", "GNW_input_init", "GNW_input_exit", "GNW95_clear_time_stamps", "GNW95_process_key", "kb_is_disabled", "GNW_add_input_buffer"),
    ("fallout2-ce", "", "inputInit", "inputExit", "_GNW95_clear_time_stamps", "_GNW95_process_key", "keyboardIsDisabled", "enqueueInputEvent"),
):
    input_file = f"{game}/src/{prefix}input.cc"
    edit(input_file, '#include "audio_engine.h"', '#include "audio_engine.h"\n#include "gamepad.h"')
    # Callbacks inject normalized keys into the existing game keyboard logic.
    # Controller releases must not release a key still held on a real keyboard.
    bridge = f'''static bool gamepadKeys[SDL_NUM_SCANCODES] {{}};

static void gamepadKey(SDL_Scancode key, bool down)
{{
    if (down && {disabled}()) return;
    gamepadKeys[key] = down;
    if (SDL_GetKeyboardState(nullptr)[key]) return;
    KeyboardData data;
    data.key = key;
    data.down = down ? 1 : 0;
    {process}(&data);
}}

static void gamepadCharacter(int character)
{{
    if (!{disabled}()) {enqueue}(character);
}}

'''
    edit(input_file, f"static void {process}(KeyboardData* data);", f"static void {process}(KeyboardData* data);\n\n{bridge}")
    edit(input_file, f"    {clear}();", f'    {clear}();\n    gamepadInit("{game}", gamepadKey, gamepadCharacter, gSdlWindow);')
    edit(input_file, f"void {shutdown}()\n{{", f"void {shutdown}()\n{{\n    gamepadExit();")
    edit(input_file, "    while (SDL_PollEvent(&e)) {\n        switch (e.type)", "    while (SDL_PollEvent(&e)) {\n        if (gamepadHandleEvent(e)) continue;\n        switch (e.type)")
    edit(input_file, f"            if (!{disabled}()) {{\n                keyboardData.key", f"            if (!{disabled}() && !(e.type == SDL_KEYUP && gamepadKeys[e.key.keysym.scancode])) {{\n                keyboardData.key")
    edit(input_file, "    touch_process_gesture();", "    gamepadUpdate();\n    if (!gamepadOverlayOpen()) touch_process_gesture();")
    edit(input_file, "    SDL_StartTextInput();", "    SDL_StartTextInput();\n    gamepadTextInput(true);")
    edit(input_file, "    SDL_StopTextInput();", "    gamepadTextInput(false);\n    SDL_StopTextInput();")

    svga = f"{game}/src/{prefix}svga.cc"
    edit(svga, f'#include "{prefix}svga.h"', f'#include "{prefix}svga.h"\n#include "gamepad.h"')
    edit(svga, "    SDL_RenderPresent(gSdlRenderer);", "    gamepadRender(gSdlRenderer);\n    SDL_RenderPresent(gSdlRenderer);")

    # Merge at the raw mouse layer so inventory drag loops also see the pad.
    dinput = f"{game}/src/{prefix}{'dxinput' if prefix else 'dinput'}.cc"
    header = f'{prefix}{"dxinput" if prefix else "dinput"}.h'
    edit(dinput, f'#include "{header}"', f'#include "{header}"\n#include "gamepad.h"')
    edit(dinput, "    gMouseWheelDeltaX = 0;\n    gMouseWheelDeltaY = 0;", """    int mergedButtons = (mouseState->buttons[0] ? 1 : 0) | (mouseState->buttons[1] ? 2 : 0);
    gamepadMouse(mouseState->x, mouseState->y, mergedButtons, mouseState->wheelX, mouseState->wheelY);
    mouseState->buttons[0] = (mergedButtons & 1) != 0;
    mouseState->buttons[1] = (mergedButtons & 2) != 0;

    gMouseWheelDeltaX = 0;
    gMouseWheelDeltaY = 0;""")
    cmake = f"{game}/CMakeLists.txt"
    edit(cmake, '    "src/audio_engine.cc"', '    "src/gamepad.cc"\n    "src/gamepad_world.cc"\n    "src/gamepad_movement.h"\n    "src/gamepad_ui.cc"\n    "src/gamepad.h"\n    "src/gamepad_internal.h"\n    "src/audio_engine.cc"')

print("Integrated controller support in both engines.")
