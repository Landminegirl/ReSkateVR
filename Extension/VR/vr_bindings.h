#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace dingosdk::vr::controls {
enum Action : int { none, game_a, game_x, game_y, game_b, lb, rb, lt, rt, left_click, right_click,
    switch_view, recenter, dpad_shift, pause, reskate_menu, voice_talk, dpad_up, dpad_down, dpad_left, dpad_right, count };
inline constexpr std::array<const char*, count> action_names{"Not bound", "Push (game A)", "Other push (game X)",
    "Board on / off (game Y)", "Stop (game B)", "Left bumper (LB)", "Right bumper (RB)", "Left grab (LT)",
    "Right grab (RT)", "Game left stick click", "Game right stick click", "Switch first / third person", "Recenter view",
    "Hold for left-stick D-pad", "Game pause menu", "Open ReSkate menu", "Voice talk", "D-pad up", "D-pad down", "D-pad left", "D-pad right"};
inline constexpr std::array<const char*, 12> input_names{"Left X", "Left Y", "Right A", "Right B", "Left index trigger",
    "Right index trigger", "Left grip", "Right grip", "Left stick click", "Right stick click", "Menu short press", "Menu hold"};
using Bindings = std::array<int, input_names.size()>;
using Values = std::array<float, input_names.size()>;
inline constexpr Bindings defaults{game_y, game_b, game_a, game_x, lb, dpad_shift, lt, rt, switch_view, recenter, pause, reskate_menu};
inline bool valid(int action) { return action >= none && action < count; }
inline bool down(const Bindings& bindings, const Values& values, int action) {
    for (std::size_t i = 0; i < values.size(); ++i) if (bindings[i] == action && values[i] > .6f) return true;
    return false;
}
struct Routed {
    std::uint16_t buttons = 0;
    float left_trigger = 0, right_trigger = 0;
    bool view = false, center = false, menu = false, voice = false;
};
inline Routed route(const Bindings& bindings, Values values, bool pointer_active, int pointer_button) {
    if (pointer_active && pointer_button >= 0 && pointer_button < 10) values[static_cast<std::size_t>(pointer_button)] = 0;
    Routed out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto amount = std::clamp(values[i], 0.0f, 1.0f);
        const auto action = bindings[i];
        if (action == lt) out.left_trigger = std::max(out.left_trigger, amount);
        if (action == rt) out.right_trigger = std::max(out.right_trigger, amount);
        if (amount <= .6f) continue;
        switch (action) {
        case game_a: out.buttons |= 0x1000; break;
        case game_x: out.buttons |= 0x4000; break;
        case game_y: out.buttons |= 0x8000; break;
        case game_b: out.buttons |= 0x2000; break;
        case lb: out.buttons |= 0x100; break;
        case rb: out.buttons |= 0x200; break;
        case left_click: out.buttons |= 0x40; break;
        case right_click: out.buttons |= 0x80; break;
        case pause: out.buttons |= 0x10; break;
        case switch_view: out.view = true; break;
        case recenter: out.center = true; break;
        case reskate_menu: out.menu = true; break;
        case voice_talk: out.voice = true; break;
        case dpad_up: out.buttons |= 1; break;
        case dpad_down: out.buttons |= 2; break;
        case dpad_left: out.buttons |= 4; break;
        case dpad_right: out.buttons |= 8; break;
        default: break;
        }
    }
    return out;
}
// Voice latch is session-only and must see a release after activation/rebinding.
// The voice worker resets it on focus/session/host-policy loss and open mic mode.
struct TalkGate {
    bool configured = false, armed = false, previous = false, latched = false, mode = false;
    std::uint64_t revision = 0;
    bool update(bool active, bool down, bool toggle, std::uint64_t current_revision) {
        if (!active) { *this = {}; return false; }
        if (!configured || mode != toggle || revision != current_revision) {
            *this = {}; configured = true; mode = toggle; revision = current_revision; previous = down;
        }
        if (!down) armed = true;
        const bool pressed = down && !previous;
        previous = down;
        if (!armed) return false;
        if (toggle && pressed) latched = !latched;
        return toggle ? latched : down;
    }
};
}
