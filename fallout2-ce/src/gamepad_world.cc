#include "gamepad.h"
#include "gamepad_movement.h"
#include "animation.h"
#include "combat.h"
#include "game.h"
#include "game_mouse.h"
#include "interface.h"
#include "map.h"
#include "object.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

static GamepadSteering steering;

static bool canMove()
{
    return gDude != nullptr && gDude->tile >= 0
        && !gameUiIsDisabled() && interfaceBarEnabled()
        && gameMouseGetCursor() < MOUSE_CURSOR_WAIT_PLANET
        && gameGetState() != GAME_STATE_5
        && (gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT | DAM_LOSE_TURN)) == 0;
}

void gamepadWorldBegin()
{
    // The caller identifies a player gameplay poll. Cursor visibility changes
    // at map edges and over the HUD; it must not turn this into a menu poll.
    gamepadSetWorldInput(true);
}

// Called at hex boundaries by the current animation; it must not register or
// interrupt animations. Returning -1 lets the normal engine stop the sequence.
static int nextStep(Object* object, int* animation)
{
    float x, y;
    bool running;
    if (object != gDude || !canMove() || !gamepadMovement(x, y, running)) { steering.reset(); return -1; }
    if (isInCombat() && object->data.critter.combat.ap + _combat_free_move <= 0) return -1;
    int ox, oy;
    if (tileToScreenXY(object->tile, &ox, &oy, object->elevation) != 0) return -1;
    int tiles[6], dx[6] {}, dy[6] {};
    for (int i = 0; i < 6; ++i) {
        tiles[i] = tileGetTileInDirection(object->tile, i, 1);
        int tx, ty;
        if (tiles[i] >= 0 && tileToScreenXY(tiles[i], &tx, &ty, object->elevation) == 0) {
            dx[i] = tx - ox;
            dy[i] = ty - oy;
        }
    }
    int direction = steering.choose(x, y, dx, dy);
    if (direction < 0 || tiles[direction] == object->tile
        || _obj_blocking_at(object, tiles[direction], object->elevation) != nullptr) return -1;
    steering.commit(dx[direction], dy[direction]);
    *animation = running ? ANIM_RUNNING : ANIM_WALK;
    return direction;
}

static void movePlayer()
{
    if (!canMove() || animationIsBusy(gDude) != 0) return;
    int animation = ANIM_WALK;
    int direction = nextStep(gDude, &animation);
    if (direction < 0) return;
    int tile = tileGetTileInDirection(gDude->tile, direction, 1);
    int ap = isInCombat() ? gDude->data.critter.combat.ap + _combat_free_move : -1;
    if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) != 0) return;
    int result = animation == ANIM_RUNNING
        ? animationRegisterRunToTile(gDude, tile, gDude->elevation, ap, 0)
        : animationRegisterMoveToTile(gDude, tile, gDude->elevation, ap, 0);
    if (result == 0 && reg_anim_end() == 0) {
        animationSetMoveContinuation(gDude, nextStep);
    }
}

static void updateCamera()
{
    static GamepadCamera camera;
    static Uint32 lastTick;
    Uint32 now = SDL_GetTicks();
    float seconds = std::min(now - lastTick, Uint32(50)) / 1000.0f;
    lastTick = now;
    float x, y;
    bool running;
    if (!canMove() || !gmouse_scrolling_is_enabled() || gamepadOverlayOpen()) {
        camera.reset();
        return;
    }
    float panX, panY;
    if (!gamepadCameraInput(panX, panY)) { camera.reset(); return; }
    bool following = gamepadMovement(x, y, running) || animationHasMoveContinuation(gDude);
    int px, py;
    if (tileToScreenXY(gDude->tile, &px, &py, gDude->elevation) != 0) return;
    int dx, dy;
    camera.step(px + 16 + gDude->x - windowGetWidth(gIsoWindow) / 2,
        py + 8 + gDude->y - windowGetHeight(gIsoWindow) / 2, seconds, dx, dy, panX, panY, following);
    if (tileScrollPixels(dx, dy) != 0) camera.blocked();
}

void gamepadWorldEnd()
{
    movePlayer();
    updateCamera();
    gamepadSetWorldInput(false);
}

} // namespace fallout
