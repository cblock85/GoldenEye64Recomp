#include <atomic>
#include <cstdlib>
#include "zelda_debug.h"
#include "librecomp/helpers.hpp"
// #include "../patches/input.h"

std::atomic<uint16_t> pending_warp = 0xFFFF;
std::atomic<uint32_t> pending_set_time = 0xFFFF;

void zelda64::do_warp(int area, int scene, int entrance) {
    const zelda64::SceneWarps game_scene = zelda64::game_warps[area].scenes[scene];
    int game_scene_index = game_scene.index;
    pending_warp.store(((game_scene_index & 0xFF) << 8) | ((entrance & 0x0F) << 4));
}

extern "C" void recomp_get_pending_warp(uint8_t* rdram, recomp_context* ctx) {
    // Return the current warp value and reset it.
    _return(ctx, pending_warp.exchange(0xFFFF));
}

void zelda64::set_time(uint8_t day, uint8_t hour, uint8_t minute) {
    pending_set_time.store((day << 16) | (uint16_t(hour) << 8) | minute);
}

extern "C" void recomp_get_pending_set_time(uint8_t* rdram, recomp_context* ctx) {
    // Return the current set time value and reset it.
    _return(ctx, pending_set_time.exchange(0xFFFF));
}

// Test harness (tools/harness.sh). GE_HARNESS_STAGE=<level id> boots straight into that level,
// GE_HARNESS_LOOK=<degrees> skips the intro there and holds the vertical look angle. Unset: normal game.
static int32_t harness_env(const char* name, int32_t fallback) {
    const char* value = getenv(name);
    return value != nullptr ? (int32_t) strtol(value, nullptr, 0) : fallback;
}

extern "C" void recomp_get_harness_stage(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, harness_env("GE_HARNESS_STAGE", -1));
}

extern "C" void recomp_get_harness_look(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, harness_env("GE_HARNESS_LOOK", -1000));
}

extern "C" void recomp_get_harness_skip(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, harness_env("GE_HARNESS_SKIP", 1));
}

// Free-form integer for temporary A/B switches in patches while debugging. Unset: 0.
extern "C" void recomp_get_harness_dbg(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, harness_env("GE_HARNESS_DBG", 0));
}
