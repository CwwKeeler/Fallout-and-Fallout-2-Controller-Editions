#include "gamepad.h"
#include "gamepad_movement.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/intface.h"
#include "game/map.h"
#include "game/object.h"
#include "game/tile.h"
#include "plib/gnw/gnw.h"

namespace fallout {

static GamepadSteering steering;

static bool canMove()
{
    return obj_dude != nullptr && obj_dude->tile >= 0
        && !game_ui_is_disabled() && intface_is_enabled()
        && gmouse_get_cursor() < MOUSE_CURSOR_WAIT_PLANET
        && game_state() != GAME_STATE_5
        && (obj_dude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT | DAM_LOSE_TURN)) == 0;
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
    if (object != obj_dude || !canMove() || !gamepadMovement(x, y, running)) { steering.reset(); return -1; }
    if (isInCombat() && object->data.critter.combat.ap + combat_free_move <= 0) return -1;
    int ox, oy;
    if (tile_coord(object->tile, &ox, &oy, object->elevation) != 0) return -1;
    int tiles[6], dx[6] {}, dy[6] {};
    for (int i = 0; i < 6; ++i) {
        tiles[i] = tile_num_in_direction(object->tile, i, 1);
        int tx, ty;
        if (tiles[i] >= 0 && tile_coord(tiles[i], &tx, &ty, object->elevation) == 0) {
            dx[i] = tx - ox;
            dy[i] = ty - oy;
        }
    }
    int direction = steering.choose(x, y, dx, dy);
    if (direction < 0 || tiles[direction] == object->tile
        || obj_blocking_at(object, tiles[direction], object->elevation) != nullptr) return -1;
    steering.commit(dx[direction], dy[direction]);
    *animation = running ? ANIM_RUNNING : ANIM_WALK;
    return direction;
}

static void movePlayer()
{
    if (!canMove() || anim_busy(obj_dude) != 0) return;
    int animation = ANIM_WALK;
    int direction = nextStep(obj_dude, &animation);
    if (direction < 0) return;
    int tile = tile_num_in_direction(obj_dude->tile, direction, 1);
    int ap = isInCombat() ? obj_dude->data.critter.combat.ap + combat_free_move : -1;
    if (register_begin(ANIMATION_REQUEST_RESERVED) != 0) return;
    int result = animation == ANIM_RUNNING
        ? register_object_run_to_tile(obj_dude, tile, obj_dude->elevation, ap, 0)
        : register_object_move_to_tile(obj_dude, tile, obj_dude->elevation, ap, 0);
    if (result == 0 && register_end() == 0) {
        animationSetMoveContinuation(obj_dude, nextStep);
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
    bool following = gamepadMovement(x, y, running) || animationHasMoveContinuation(obj_dude);
    int px, py;
    if (tile_coord(obj_dude->tile, &px, &py, obj_dude->elevation) != 0) return;
    int dx, dy;
    camera.step(px + 16 + obj_dude->x - win_width(display_win) / 2,
        py + 8 + obj_dude->y - win_height(display_win) / 2, seconds, dx, dy, panX, panY, following);
    if (tileScrollPixels(dx, dy) != 0) camera.blocked();
}

void gamepadWorldEnd()
{
    movePlayer();
    updateCamera();
    gamepadSetWorldInput(false);
}

} // namespace fallout
