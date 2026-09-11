// Compile the target-selection functions extracted from the actual engine.
#include "game/object_types.h"
#include "game/combat_defs.h"
#include <cstdlib>
#include <cstdio>

namespace fallout {
static int curr_crit_num;
static Object** curr_crit_list;
static Object* combat_obj;
int obj_dist(Object* a, Object* b) { return abs(a->tile - b->tile); }
bool is_within_perception(Object*, Object*) { return true; }
#include "combat_ai_targets.inc"
}

int main()
{
    using namespace fallout;
    Object critter {}, enemy {}, ally {};
    critter.data.critter.combat.team = 1;
    ally.data.critter.combat.team = 1;
    enemy.data.critter.combat.team = 2;
    Object* a = &enemy;
    Object* b = &enemy;
    Object* c = &enemy;
    curr_crit_num = 0;
    ai_find_attackers(&critter, &a, &b, &c);
    if (a || b || c) return 1;
    // The old implementation dereferenced the optional null output address.
    ai_find_attackers(&critter, nullptr, nullptr, nullptr);
    if (ai_danger_source(&critter) != nullptr) return 2;
    Object* list[] = { &critter, &enemy, &ally };
    curr_crit_num = 3;
    curr_crit_list = list;
    enemy.data.critter.combat.whoHitMe = &critter;
    if (ai_danger_source(&critter) != &enemy) return 3;
    enemy.data.critter.combat.results = DAM_DEAD;
    if (ai_danger_source(&critter) != nullptr) return 4;
    puts("PASS: enemy target selection with absent, live, and dead attackers; optional outputs.");
    return 0;
}
