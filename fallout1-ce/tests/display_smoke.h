// Included only by the opt-in live-game driver, inside namespace fallout.
static int displayStage;
static int displayCapture;
static int displayChecks;
static bool displayFailed;
static SDL_DisplayMode displayDesktop;
static SDL_Renderer* displayRenderer;
static SDL_Surface* displayFrame;

static void displayNote(const char* text)
{
    FILE* file = fopen("display-test.log", "a");
    if (file) { fprintf(file, "%s\n", text); fclose(file); }
}

static void displayCheck(bool pass, const char* text)
{
    ++displayChecks;
    if (!pass) { displayFailed = true; displayNote(text); }
}

static void displayShortcut()
{
    SDL_Event e {};
    e.type = SDL_KEYDOWN;
    e.key.windowID = SDL_GetWindowID(gSdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.keysym.scancode = SDL_SCANCODE_RETURN;
    e.key.keysym.mod = KMOD_ALT;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

static void displayUpdate(Uint32 elapsed)
{
    // Actual window transitions, using the real engine event and render paths.
    if (displayStage == 0) {
        SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(gSdlWindow), &displayDesktop);
        displayRenderer = gSdlRenderer;
        displayFrame = gSdlTextureSurface;
        pad::state.settings.hints = 0;
        // The test launcher starts hidden; establish actual foreground focus
        // before measuring its loss, rather than assuming launch activated it.
        SDL_RestoreWindow(gSdlWindow);
        SDL_RaiseWindow(gSdlWindow);
        displayStage = 1;
    }
    if (elapsed < Uint32(displayStage * 1000) || displayCapture) return;
    switch (displayStage++) {
    case 1: displayCapture = 1; break;
    case 2: displayShortcut(); break;
    case 3: displayCapture = 2; break;
    case 4:
        pad::state.selection[1] = 20;
        pad::adjust(1); // The controller panel's Display Mode control.
        break;
    case 5: displayCapture = 3; break;
    case 6:
        displayCheck((SDL_GetWindowFlags(gSdlWindow) & SDL_WINDOW_INPUT_FOCUS) != 0,
            "FAIL: test window was not focused before minimize");
        SDL_MinimizeWindow(gSdlWindow);
        break;
    case 7:
        displayCheck(!pad::state.focused, "FAIL: minimize did not release focus");
        SDL_RestoreWindow(gSdlWindow);
        SDL_RaiseWindow(gSdlWindow);
        break;
    case 8:
        displayCheck(pad::state.focused, "FAIL: restore did not regain focus");
        displayCapture = 4;
        break;
    case 9: {
        SDL_Event e {};
        e.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&e);
        break;
    }
    case 10: displayCapture = 5; break;
    case 11: displayShortcut(); break;
    case 12: SDL_SetWindowSize(gSdlWindow, 1171, 803); break;
    case 13: displayCapture = 6; break;
    case 14: displayShortcut(); break;
    case 15: displayCapture = 7; break;
    case 16: {
        char line[160];
        snprintf(line, sizeof(line), "%s: %d display checks; borderless/windowed, resize, focus recovery, device reset, pixels and desktop mode.",
            displayFailed ? "FAIL" : "PASS", displayChecks);
        displayNote(line);
        SDL_Event e {};
        e.type = SDL_QUIT;
        SDL_PushEvent(&e);
        smokeJoystick = nullptr;
        break;
    }
    }
}

static void displayRender(SDL_Renderer* renderer)
{
    if (!displayCapture) return;
    const int capture = displayCapture;
    displayCapture = 0;
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(gSdlWindow), &current);
    displayCheck(current.w == displayDesktop.w && current.h == displayDesktop.h
        && current.refresh_rate == displayDesktop.refresh_rate, "FAIL: desktop display mode changed");
    bool fullscreen = capture != 2 && capture != 6;
    Uint32 flags = SDL_GetWindowFlags(gSdlWindow);
    displayCheck(fullscreen ? (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP
                            : (flags & SDL_WINDOW_FULLSCREEN) == 0, "FAIL: wrong window mode");
    displayCheck(renderer == displayRenderer && gSdlTextureSurface == displayFrame,
        "FAIL: window transition discarded the renderer or CPU frame");
    int w, h, windowW, windowH;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_GetWindowSize(gSdlWindow, &windowW, &windowH);
    displayCheck(!fullscreen || (windowW == displayDesktop.w && windowH == displayDesktop.h),
        "FAIL: borderless window does not fill the desktop");
    SDL_Surface* output = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGB888);
    if (!output) { displayCheck(false, "FAIL: readback allocation"); return; }
    // Read the entire output, including black bars outside the logical viewport.
    SDL_Rect viewport;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderFlush(renderer);
    SDL_RenderSetViewport(renderer, nullptr);
    displayCheck(SDL_RenderReadPixels(renderer, nullptr, output->format->format, output->pixels, output->pitch) == 0,
        "FAIL: render readback");
    SDL_RenderSetViewport(renderer, &viewport);
    auto pixel = [](SDL_Surface* surface, int x, int y) {
        return *(reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels) + y * surface->pitch) + x) & 0xFFFFFF;
    };
    int sampled = 0, mismatches = 0;
    // Flat 3x3 source regions avoid ambiguous fractional nearest-pixel edges.
    // Compare the actual GPU output to the authoritative converted game frame.
    for (int y = 2; y < gSdlTextureSurface->h - 2; y += 11) {
        for (int x = 2; x < gSdlTextureSurface->w - 2; x += 11) {
            Uint32 expected = pixel(gSdlTextureSurface, x, y);
            bool flat = true;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox)
                    if (pixel(gSdlTextureSurface, x + ox, y + oy) != expected) flat = false;
            if (!flat) continue;
            int wx, wy;
            SDL_RenderLogicalToWindow(renderer, x + 0.5f, y + 0.5f, &wx, &wy);
            int px = wx * w / windowW, py = wy * h / windowH;
            if (px >= 0 && px < w && py >= 0 && py < h) {
                ++sampled;
                if (pixel(output, px, py) != expected) ++mismatches;
            }
        }
    }
    displayCheck(sampled >= 100 && mismatches == 0, "FAIL: scaled output differs from current game image");
    // Logical aspect ratio must be preserved, with clean unused margins.
    float sx, sy;
    SDL_RenderGetScale(renderer, &sx, &sy);
    displayCheck(std::fabs(sx - sy) < 0.001f, "FAIL: stretched game aspect ratio");
    if (w - gSdlTextureSurface->w * sx > 4) {
        displayCheck(pixel(output, 1, h / 2) == 0 && pixel(output, w - 2, h / 2) == 0,
            "FAIL: stale pixels in pillarbox bars");
    }
    if (h - gSdlTextureSurface->h * sy > 4) {
        displayCheck(pixel(output, w / 2, 1) == 0 && pixel(output, w / 2, h - 2) == 0,
            "FAIL: stale pixels in letterbox bars");
    }
    char name[80], line[240];
    snprintf(name, sizeof(name), "display-%02d.bmp", capture);
    SDL_SaveBMP(output, name);
    snprintf(line, sizeof(line), "Capture %d: output %dx%d, desktop %dx%d @ %d Hz, %d sampled pixels, %d mismatches",
        capture, w, h, current.w, current.h, current.refresh_rate, sampled, mismatches);
    displayNote(line);
    SDL_FreeSurface(output);
}
