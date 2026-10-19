// VR controllers as a gamepad: OpenXR actions (Touch and Index bindings) are read on
// the Present thread each headset frame and handed to the game as XInput pad 0 by
// ReSkate's XInput hook, merged with a physical pad.
//   sticks -> the same; grips -> LT / RT (grabs); left trigger -> LB
//   right A B -> A, X (both pushes on the right hand); left X Y -> Y (board on / off), B (stop)
//   (Index: a / b on each hand)
//   menu (Index: left trackpad press) -> Start on short press, ReSkate menu on hold
//   hold right index trigger (or touch right thumbrest) -> left stick is the D-pad
//   left stick click -> view mode; right stick click -> recenter
#include "vr_internal.h"
#include "Engine/Core/Log/logging.h"
#include "Extension/UI/Overlay/overlay.h"
#include <Xinput.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <initializer_list>
#include <mutex>
#include <utility>
#include <vector>

namespace dingosdk::vr {
namespace detail {
namespace {
void log_info(const std::string& text) { logging::write(logging::Level::info, logging::Channel::graphics, "VR: " + text); }

struct Input {
    PFN_xrStringToPath string_to_path{};
    PFN_xrCreateActionSet create_action_set{};
    PFN_xrDestroyActionSet destroy_action_set{};
    PFN_xrCreateAction create_action{};
    PFN_xrSuggestInteractionProfileBindings suggest_bindings{};
    PFN_xrAttachSessionActionSets attach{};
    PFN_xrSyncActions sync{};
    PFN_xrGetActionStateBoolean get_boolean{};
    PFN_xrGetActionStateFloat get_float{};
    PFN_xrGetActionStateVector2f get_vector{};
    PFN_xrGetActionStatePose get_pose{};
    PFN_xrCreateActionSpace create_action_space{};

    XrActionSet set = XR_NULL_HANDLE;
    std::array<XrPath, 2> hands{}; // left, right
    XrAction stick{}, stick_click{}, trigger{}, squeeze{}, menu{}, a{}, b{}, x{}, y{}, thumbrest{};
    XrAction left_grip{};                 // the left hand's pose, for the chat panel
    XrSpace left_space = XR_NULL_HANDLE, aim_space = XR_NULL_HANDLE;
    XrAction aim{};
    math::MenuPress menu_press;
    math::PressEdge dpad_press;
    bool dpad_held = false;
    double start_until = 0;
    bool focused = false;

    std::mutex mutex;
    VirtualPad pad;
    double updated = -100;
};
Input& input() {
    static auto* value = new Input;
    return *value;
}

template<class Function>
bool load(PFN_xrGetInstanceProcAddr get, XrInstance instance, const char* name, Function& out) {
    PFN_xrVoidFunction function = nullptr;
    if (XR_FAILED(get(instance, name, &function)) || !function) return false;
    out = reinterpret_cast<Function>(function);
    return true;
}

XrPath path(XrInstance instance, const char* text) {
    XrPath value = XR_NULL_PATH;
    (void)input().string_to_path(instance, text, &value);
    return value;
}

bool make_action(XrActionType type, const char* name, const char* label, bool per_hand, XrAction& out) {
    auto& in = input();
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    std::snprintf(info.actionName, sizeof(info.actionName), "%s", name);
    std::snprintf(info.localizedActionName, sizeof(info.localizedActionName), "%s", label);
    if (per_hand) {
        info.countSubactionPaths = 2;
        info.subactionPaths = in.hands.data();
    }
    return XR_SUCCEEDED(in.create_action(in.set, &info, &out));
}

bool suggest(XrInstance instance, const char* profile, std::initializer_list<std::pair<XrAction, const char*>> bindings) {
    std::vector<XrActionSuggestedBinding> list;
    for (const auto& [action, where] : bindings) list.push_back({action, path(instance, where)});
    XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    info.interactionProfile = path(instance, profile);
    info.countSuggestedBindings = static_cast<std::uint32_t>(list.size());
    info.suggestedBindings = list.data();
    return XR_SUCCEEDED(input().suggest_bindings(instance, &info));
}

XrActionStateGetInfo get_info(XrAction action, XrPath hand) {
    XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};
    info.action = action;
    info.subactionPath = hand;
    return info;
}
bool pressed(XrSession session, XrAction action, XrPath hand = XR_NULL_PATH) {
    const auto info = get_info(action, hand);
    XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
    return XR_SUCCEEDED(input().get_boolean(session, &info, &state)) && state.isActive && state.currentState;
}
float value(XrSession session, XrAction action, XrPath hand) {
    const auto info = get_info(action, hand);
    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
    return XR_SUCCEEDED(input().get_float(session, &info, &state)) && state.isActive ? std::clamp(state.currentState, 0.0f, 1.0f) : 0.0f;
}
std::array<std::int16_t, 2> stick(XrSession session, XrPath hand) {
    const auto info = get_info(input().stick, hand);
    XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_FAILED(input().get_vector(session, &info, &state)) || !state.isActive) return {};
    auto axis = [](float v) { return static_cast<std::int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f)); };
    return {axis(state.currentState.x), axis(state.currentState.y)};
}
}

bool input_start(XrInstance instance, XrSession session, PFN_xrGetInstanceProcAddr get) noexcept {
    try {
        auto& in = input();
        if (!load(get, instance, "xrStringToPath", in.string_to_path) ||
            !load(get, instance, "xrCreateActionSet", in.create_action_set) ||
            !load(get, instance, "xrDestroyActionSet", in.destroy_action_set) ||
            !load(get, instance, "xrCreateAction", in.create_action) ||
            !load(get, instance, "xrSuggestInteractionProfileBindings", in.suggest_bindings) ||
            !load(get, instance, "xrAttachSessionActionSets", in.attach) || !load(get, instance, "xrSyncActions", in.sync) ||
            !load(get, instance, "xrGetActionStateBoolean", in.get_boolean) ||
            !load(get, instance, "xrGetActionStateFloat", in.get_float) ||
            !load(get, instance, "xrGetActionStateVector2f", in.get_vector) ||
            !load(get, instance, "xrGetActionStatePose", in.get_pose) ||
            !load(get, instance, "xrCreateActionSpace", in.create_action_space)) {
            log_info("Controllers unavailable: the runtime lacks an input function.");
            return false;
        }
        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::snprintf(set.actionSetName, sizeof(set.actionSetName), "gamepad");
        std::snprintf(set.localizedActionSetName, sizeof(set.localizedActionSetName), "Gamepad");
        if (XR_FAILED(in.create_action_set(instance, &set, &in.set))) {
            in.set = XR_NULL_HANDLE;
            log_info("Controllers unavailable: creating the action set failed.");
            return false;
        }
        in.hands = {path(instance, "/user/hand/left"), path(instance, "/user/hand/right")};
        if (!make_action(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Stick", true, in.stick) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Stick click", true, in.stick_click) ||
            !make_action(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger", true, in.trigger) ||
            !make_action(XR_ACTION_TYPE_FLOAT_INPUT, "grip", "Grip (bumper)", true, in.squeeze) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Game / ReSkate menu", false, in.menu) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "A", false, in.a) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "B", false, in.b) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "X", false, in.x) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Y", false, in.y) ||
            !make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_shift", "D-pad (left stick)", false, in.thumbrest) ||
            !make_action(XR_ACTION_TYPE_POSE_INPUT, "left_hand", "Left hand", false, in.left_grip) ||
            !make_action(XR_ACTION_TYPE_POSE_INPUT, "menu_aim", "Menu pointer", true, in.aim)) {
            log_info("Controllers unavailable: creating an action failed.");
            return false;
        }
        const bool touch = suggest(instance, "/interaction_profiles/oculus/touch_controller",
            {{in.stick, "/user/hand/left/input/thumbstick"}, {in.stick, "/user/hand/right/input/thumbstick"},
                {in.left_grip, "/user/hand/left/input/grip/pose"},
                {in.aim, "/user/hand/right/input/aim/pose"},
                {in.stick_click, "/user/hand/left/input/thumbstick/click"},
                {in.stick_click, "/user/hand/right/input/thumbstick/click"},
                {in.trigger, "/user/hand/left/input/trigger/value"}, {in.trigger, "/user/hand/right/input/trigger/value"},
                {in.squeeze, "/user/hand/left/input/squeeze/value"}, {in.squeeze, "/user/hand/right/input/squeeze/value"},
                {in.menu, "/user/hand/left/input/menu/click"}, {in.a, "/user/hand/right/input/a/click"},
                {in.b, "/user/hand/right/input/b/click"}, {in.x, "/user/hand/left/input/x/click"},
                {in.y, "/user/hand/left/input/y/click"}, {in.thumbrest, "/user/hand/right/input/thumbrest/touch"}});
        const bool index = suggest(instance, "/interaction_profiles/valve/index_controller",
            {{in.stick, "/user/hand/left/input/thumbstick"}, {in.stick, "/user/hand/right/input/thumbstick"},
                {in.left_grip, "/user/hand/left/input/grip/pose"},
                {in.aim, "/user/hand/right/input/aim/pose"},
                {in.stick_click, "/user/hand/left/input/thumbstick/click"},
                {in.stick_click, "/user/hand/right/input/thumbstick/click"},
                {in.trigger, "/user/hand/left/input/trigger/value"}, {in.trigger, "/user/hand/right/input/trigger/value"},
                {in.squeeze, "/user/hand/left/input/squeeze/value"}, {in.squeeze, "/user/hand/right/input/squeeze/value"},
                {in.menu, "/user/hand/left/input/trackpad/force"}, {in.a, "/user/hand/right/input/a/click"},
                {in.b, "/user/hand/right/input/b/click"}, {in.x, "/user/hand/left/input/a/click"},
                {in.y, "/user/hand/left/input/b/click"}, {in.thumbrest, "/user/hand/right/input/trackpad/touch"}});
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &in.set;
        if (XR_FAILED(in.attach(session, &attach))) {
            log_info("Controllers unavailable: attaching the actions failed.");
            return false;
        }
        XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        space.action = in.left_grip;
        space.poseInActionSpace.orientation.w = 1;
        if (XR_FAILED(in.create_action_space(session, &space, &in.left_space))) in.left_space = XR_NULL_HANDLE;
        space.action = in.aim;
        space.subactionPath = in.hands[1];
        if (XR_FAILED(in.create_action_space(session, &space, &in.aim_space))) in.aim_space = XR_NULL_HANDLE;
        log_info(std::format("Controllers ready (bindings: Touch {}, Index {}; left hand pose {}, right aim {}).", touch ? "yes" : "no",
            index ? "yes" : "no", in.left_space ? "yes" : "no", in.aim_space ? "yes" : "no"));
        return true;
    } catch (...) {
        return false;
    }
}

void input_sync(XrSession session, bool focused) noexcept {
    try {
        auto& in = input();
        if (!in.set) return;
        in.focused = focused && current_settings().enabled && current_settings().controllers;
        if (!in.focused) {
            (void)in.menu_press.sample(false, false, seconds_now());
            in.start_until = 0;
            (void)in.dpad_press.sample(false, false);
            in.dpad_held = false;
            std::lock_guard lock(in.mutex);
            in.pad = {};
            in.updated = -100;
            return;
        }
        XrActiveActionSet active{in.set, XR_NULL_PATH};
        XrActionsSyncInfo info{XR_TYPE_ACTIONS_SYNC_INFO};
        info.countActiveActionSets = 1;
        info.activeActionSets = &active;
        if (XR_FAILED(in.sync(session, &info))) return;
        const double now = seconds_now();
        const auto [left, right] = in.hands;
        VirtualPad pad;
        pad.active = true;
        const auto ls = stick(session, left), rs = stick(session, right);
        pad.left_x = ls[0];
        pad.left_y = ls[1];
        const bool menus = shared().ui_open.load(std::memory_order_acquire) || shared().game_menu.load(std::memory_order_acquire);
        const bool right_trigger = value(session, in.trigger, right) > 0.6f;
        // Hold the right index trigger to use the left stick as the D-pad.
        // Menus reserve that trigger for the ray pointer. Thumbrest remains an alternative.
        if (in.dpad_press.sample(true, right_trigger) && !menus) in.dpad_held = true;
        if (!right_trigger) in.dpad_held = false;
        // Keep D-pad ownership until release, even if a direction opens a native menu.
        const bool dpad_shift = in.dpad_held || pressed(session, in.thumbrest);
        std::uint16_t dpad = 0;
        if (dpad_shift) {
            if (ls[1] > 16000) dpad |= XINPUT_GAMEPAD_DPAD_UP;
            if (ls[1] < -16000) dpad |= XINPUT_GAMEPAD_DPAD_DOWN;
            if (ls[0] < -16000) dpad |= XINPUT_GAMEPAD_DPAD_LEFT;
            if (ls[0] > 16000) dpad |= XINPUT_GAMEPAD_DPAD_RIGHT;
            pad.left_x = pad.left_y = 0;
        }
        pad.right_x = rs[0];
        pad.right_y = rs[1];
        // The grips grab (game triggers); the triggers are the bumpers.
        pad.left_trigger = static_cast<std::uint8_t>(std::lround(value(session, in.squeeze, left) * 255));
        pad.right_trigger = static_cast<std::uint8_t>(std::lround(value(session, in.squeeze, right) * 255));
        std::uint16_t buttons = 0;
        if (value(session, in.trigger, left) > 0.6f) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (menus && !in.dpad_held && !current_settings().menu_pointer && right_trigger) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        // Pushes on the right hand, board on / off and stop on the left.
        if (pressed(session, in.a)) buttons |= XINPUT_GAMEPAD_A;
        if (pressed(session, in.b)) buttons |= XINPUT_GAMEPAD_X;
        if (pressed(session, in.x)) buttons |= XINPUT_GAMEPAD_Y;
        if (pressed(session, in.y)) buttons |= XINPUT_GAMEPAD_B;
        // Left stick switches view; right stick recentres. Neither reserved click
        // reaches the game (right click used to activate native camera controls).
        pad.left_click = pressed(session, in.stick_click, left);
        pad.right_click = pressed(session, in.stick_click, right);
        if (pad.left_click && !current_settings().stick_view_switch) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
        const auto menu_action = in.menu_press.sample(true, pressed(session, in.menu), now);
        if (menu_action == math::MenuPress::Action::long_press) overlay::menu_open_flag().store(true);
        if (menu_action == math::MenuPress::Action::short_press) in.start_until = now + 0.15;
        if (now < in.start_until) buttons |= XINPUT_GAMEPAD_START;
        pad.buttons = static_cast<std::uint16_t>(buttons | dpad);
        std::lock_guard lock(in.mutex);
        in.pad = pad;
        in.updated = now;
    } catch (...) {}
}

bool left_hand_pose(XrSpace base, XrTime time, math::Pose& out) noexcept {
    auto& in = input();
    if (!in.left_space || !base) return false;
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    constexpr XrSpaceLocationFlags tracked = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if (XR_FAILED(shared().xr.locate_space(in.left_space, base, time, &location)) ||
        (location.locationFlags & tracked) != tracked)
        return false;
    out = to_pose(location.pose);
    return true;
}

bool right_aim_pose(XrSpace base, XrTime time, math::Pose& out, bool& trigger) noexcept {
    auto& in = input();
    if (!in.focused || !in.aim_space || !base) return false;
    const auto info = get_info(in.aim, in.hands[1]);
    XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
    if (XR_FAILED(in.get_pose(shared().session, &info, &state)) || !state.isActive) return false;
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    constexpr auto valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if (XR_FAILED(shared().xr.locate_space(in.aim_space, base, time, &location)) || (location.locationFlags & valid) != valid)
        return false;
    out = to_pose(location.pose);
    trigger = !in.dpad_held && value(shared().session, in.trigger, in.hands[1]) > 0.6f;
    return true;
}

void input_stop() noexcept {
    auto& in = input();
    if (in.aim_space && shared().xr.destroy_space) shared().xr.destroy_space(in.aim_space);
    in.aim_space = XR_NULL_HANDLE;
    in.focused = false;
    in.dpad_held = false;
    in.dpad_press = {};
    if (in.left_space && shared().xr.destroy_space) shared().xr.destroy_space(in.left_space);
    in.left_space = XR_NULL_HANDLE;
    if (in.set && in.destroy_action_set) in.destroy_action_set(in.set);
    in.set = XR_NULL_HANDLE;
    std::lock_guard lock(in.mutex);
    in.pad = {};
    in.updated = -100;
}
}

bool controllers_connected() noexcept {
    try {
        return detail::current_settings().controllers;
    } catch (...) {
        return false;
    }
}

VirtualPad controller_pad() noexcept {
    try {
        auto& in = detail::input();
        if (!detail::current_settings().controllers) return {};
        std::lock_guard lock(in.mutex);
        if (detail::seconds_now() - in.updated > 0.25) return {};
        return in.pad;
    } catch (...) {
        return {};
    }
}
}
