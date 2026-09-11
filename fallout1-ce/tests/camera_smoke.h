// Included by the opt-in gameplay driver; never compiled into normal builds.
static int cameraPhase;
static Sint16 cameraWalkX, cameraWalkY;
static bool cameraFailed;

static Uint32 cameraFrameHash()
{
    Uint32 hash = 2166136261u;
    for (int y = 0; y < gSdlSurface->h; ++y) {
        auto row = static_cast<Uint8*>(gSdlSurface->pixels) + y * gSdlSurface->pitch;
        for (int x = 0; x < gSdlSurface->w; ++x) hash = (hash ^ row[x]) * 16777619u;
    }
    return hash;
}

static bool cameraRestUpdate(Uint32 elapsed)
{
    static int stage;
    static Uint32 start;
    if (stage == 0) {
        start = elapsed;
        cameraPhase = 4;
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTX, cameraWalkX);
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTY, cameraWalkY);
        stage = 1;
    } else if (stage == 1 && elapsed - start >= 600) {
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTX, 0);
        SDL_JoystickSetVirtualAxis(smokeJoystick, SDL_CONTROLLER_AXIS_RIGHTY, 0);
        cameraPhase = 5;
        start = elapsed;
        stage = 2;
    } else if (stage == 2 && elapsed - start >= 1000) {
        cameraPhase = 0;
        int beforeX, beforeY, afterX, afterY;
        TEST_COORD(motionAnchor, &beforeX, &beforeY, TEST_DUDE->elevation);
        int result = TEST_SCROLL(-1, 0);
        TEST_COORD(motionAnchor, &afterX, &afterY, TEST_DUDE->elevation);
        // Native wheel/key scrolling after pixel panning must translate the
        // actual camera by its specified amount, or leave it still if blocked.
        cameraFailed |= result == 0 ? (afterX - beforeX != 32 || afterY != beforeY)
                                   : (afterX != beforeX || afterY != beforeY);
        Uint32 before = cameraFrameHash();
        TEST_REFRESH();
        cameraFailed |= before != cameraFrameHash();
        FILE* log = fopen("camera-legacy-check.log", "w");
        if (log) {
            fprintf(log, "%s: native scroll result %d, camera displacement %d,%d, full redraw comparison.\n",
                cameraFailed ? "FAIL" : "PASS", result, afterX - beforeX, afterY - beforeY);
            fclose(log);
        }
        TEST_MOUSE_PLACE(screenGetWidth() / 2, screenGetHeight() / 2);
        stage = 3;
        return true;
    } else if (stage == 3) return true;
    return false;
}

static void cameraRender(SDL_Renderer* renderer)
{
    if (!cameraPhase || !TEST_DUDE) return;
    int x, y;
    TEST_COORD(motionAnchor, &x, &y, TEST_DUDE->elevation);
    FILE* file = fopen("camera-trace.csv", "a");
    if (file) {
        fprintf(file, "%u,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n", SDL_GetTicks(), cameraPhase,
            TEST_DUDE->tile, x, y, pad::state.cameraX, pad::state.cameraY,
            pad::state.walkX, pad::state.walkY, pad::state.worldContext,
            pad::state.waitNeutral, TEST_ANIM_BUSY(TEST_DUDE));
        fclose(file);
    }
    static Uint32 nextCapture;
    static int frame;
    if (SDL_GetTicks() < nextCapture) return;
    nextCapture = SDL_GetTicks() + 100;
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_Surface* output = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGB888);
    if (!output) return;
    SDL_Rect viewport;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderFlush(renderer);
    SDL_RenderSetViewport(renderer, nullptr);
    if (SDL_RenderReadPixels(renderer, nullptr, output->format->format, output->pixels, output->pitch) == 0) {
        char name[80];
        snprintf(name, sizeof(name), "camera-%04d.bmp", frame++);
        SDL_SaveBMP(output, name);
    }
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_FreeSurface(output);
}
