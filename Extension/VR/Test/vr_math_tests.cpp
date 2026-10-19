// Unit tests for Extension/VR/vr_math.h. Platform independent:
//   cl /std:c++20 /EHsc /I <repo> Extension/VR/Test/vr_math_tests.cpp
//   g++ -std=c++20 -I <repo> Extension/VR/Test/vr_math_tests.cpp
#include "Extension/VR/vr_math.h"
#include "Extension/VR/vr.h"
#include <cstdio>
#include "Extension/VR/vr_chat_text.h"
#ifdef _WIN32
#include "Extension/VR/vr_xinput.h"
#endif
#include <cstdlib>

namespace {
using namespace dingosdk::vr::math;
namespace first_person = dingosdk::first_person;
int failures = 0;

void check(bool condition, const char* what, int line) {
    if (condition) return;
    ++failures;
    std::printf("FAILED line %d: %s\n", line, what);
}
#define CHECK(condition) check((condition), #condition, __LINE__)

bool close_to(float a, float b, float tolerance = 1e-4f) { return std::abs(a - b) <= tolerance; }
bool close_to(const Vec3& a, const Vec3& b, float tolerance = 1e-4f) {
    return close_to(a[0], b[0], tolerance) && close_to(a[1], b[1], tolerance) && close_to(a[2], b[2], tolerance);
}
Vec3 row(const Matrix& m, unsigned r) { return {m[r * 4], m[r * 4 + 1], m[r * 4 + 2]}; }
float dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
bool orthonormal_right_handed(const Matrix& m) {
    const auto r = row(m, 0), u = row(m, 1), b = row(m, 2);
    return close_to(dot(r, r), 1) && close_to(dot(u, u), 1) && close_to(dot(b, b), 1) && close_to(dot(r, u), 0) &&
           close_to(dot(r, b), 0) && close_to(dot(u, b), 0) && close_to(cross(r, u), b);
}
Matrix camera(const Quat& q, const Vec3& position) {
    Matrix m{};
    m[3] = 7; m[7] = 8; m[11] = 9; m[15] = 1; // fourth lanes the camera owns
    first_person::write(m, q, position);
    return m;
}
Quat pitch(float radians) { return {std::sin(radians / 2), 0, 0, std::cos(radians / 2)}; }
}

int main() {
    {
        using namespace dingosdk::vr::chat_text;
        char text[6]{};
        CHECK(append(text, sizeof(text), "abc"));
        CHECK(!append(text, sizeof(text), "def") && std::string_view(text) == "abc");
        CHECK(append(text, sizeof(text), "\xc3\xa9"));
        backspace(text);
        CHECK(std::string_view(text) == "abc");
        backspace(text); backspace(text); backspace(text); backspace(text);
        CHECK(!text[0]);
        CHECK(!append(text, sizeof(text), "abcdef"));
        CHECK(!sendable(" \t\n") && sendable("hi"));
        char limited[201]{};
        for (int i = 0; i < 200; ++i) CHECK(append(limited, sizeof(limited), "a"));
        CHECK(!append(limited, sizeof(limited), "b") && std::strlen(limited) == 200);
        char emoji[8]{};
        CHECK(append(emoji, sizeof(emoji), "x\xf0\x9f\x98\x80"));
        backspace(emoji);
        CHECK(std::string_view(emoji) == "x");
    }
    {
        CHECK(dingosdk::vr::Settings{}.efficient_fov);
        CHECK(dingosdk::vr::virtual_controller_requested(false, true, false, '1'));
        CHECK(!dingosdk::vr::virtual_controller_requested(false, true, false, '0'));
        CHECK(!dingosdk::vr::virtual_controller_requested(true, true, false, '0'));
        CHECK(!dingosdk::vr::virtual_controller_requested(false, true, true, '1'));
        CHECK(!dingosdk::vr::virtual_controller_requested(true, false, true, '1'));
        CHECK(dingosdk::vr::virtual_controller_requested(true, true, true, '0')); // manually enabled after standard launch
#ifdef _WIN32
        XINPUT_CAPABILITIES caps{};
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 0, ERROR_DEVICE_NOT_CONNECTED, true, &caps) == ERROR_SUCCESS);
        CHECK(caps.Type == XINPUT_DEVTYPE_GAMEPAD && caps.SubType == XINPUT_DEVSUBTYPE_GAMEPAD);
        CHECK(caps.Gamepad.wButtons == 0xf3ff && caps.Gamepad.bLeftTrigger == 255);
        CHECK(caps.Vibration.wLeftMotorSpeed == 0 && caps.Vibration.wRightMotorSpeed == 0);
        caps.Type = 42;
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 0, ERROR_SUCCESS, true, &caps) == ERROR_SUCCESS && caps.Type == 42);
        CHECK(dingosdk::vr::virtual_controller_capabilities(1, 0, ERROR_DEVICE_NOT_CONNECTED, true, &caps) == ERROR_DEVICE_NOT_CONNECTED && caps.Type == 42);
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 0, ERROR_DEVICE_NOT_CONNECTED, false, &caps) == ERROR_DEVICE_NOT_CONNECTED && caps.Type == 42);
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 2, ERROR_DEVICE_NOT_CONNECTED, true, &caps) == ERROR_DEVICE_NOT_CONNECTED);
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 0, ERROR_BAD_ARGUMENTS, true, &caps) == ERROR_BAD_ARGUMENTS);
        CHECK(dingosdk::vr::virtual_controller_capabilities(0, 0, ERROR_DEVICE_NOT_CONNECTED, true, nullptr) == ERROR_DEVICE_NOT_CONNECTED);
#endif
    }
    {
        const Angles eye{-.7f, .8f, .8f, -.75f};
        for (const float tangent : {1.1f, 1.1f * 1.23f}) {
            const auto source = layer_angles(tangent, 1);
            const auto uv = eye_uv_mapping(source, eye);
            CHECK(uv.has_value());
            const float u = .7f, v = .3f;
            const float source_x = (uv->u_bias + u * uv->u_scale - .5f) * 2 * tangent;
            const float source_y = (.5f - uv->v_bias - v * uv->v_scale) * 2 * tangent;
            CHECK(close_to(source_x, std::tan(eye.left) + u * (std::tan(eye.right) - std::tan(eye.left))));
            CHECK(close_to(source_y, std::tan(eye.up) - v * (std::tan(eye.up) - std::tan(eye.down))));
        }
    }
    {
        CHECK(menu_wheel(.2f, .016f) == 0);
        CHECK(menu_wheel(0, .016f) == 0);
        CHECK(menu_wheel(1, .016f) > 0 && menu_wheel(-1, .016f) < 0);
        CHECK(close_to(menu_wheel(1, 1.f / 90) * 90, menu_wheel(1, 1.f / 72) * 72));
        CHECK(close_to(menu_wheel(1, 10), .3f)); // no wheel burst after a stalled frame
        CHECK(menu_wheel(1, -1) == 0);
        CHECK(menu_wheel(std::numeric_limits<float>::quiet_NaN(), .016f) == 0);
        CHECK(panel_uv_valid(0, 0) && panel_uv_valid(1, 1));
        CHECK(!panel_uv_valid(-.01f, .5f) && !panel_uv_valid(.5f, 1.01f));
        CHECK(!panel_uv_valid(std::numeric_limits<float>::quiet_NaN(), .5f));
    }
    {
        namespace control = dingosdk::vr::controls;
        auto bindings = control::defaults;
        control::Values raw{};
        raw[0] = raw[2] = 1;
        auto routed = control::route(bindings, raw, false, 5);
        CHECK(routed.buttons == (0x8000 | 0x1000));
        bindings[0] = control::voice_talk;
        routed = control::route(bindings, raw, false, 5);
        CHECK(routed.voice && routed.buttons == 0x1000);
        routed = control::route(bindings, raw, true, 0);
        CHECK(!routed.voice && routed.buttons == 0x1000); // menu click cannot also talk
        raw = {}; raw[6] = .75f;
        CHECK(close_to(control::route(bindings, raw, false, 5).left_trigger, .75f));
        bindings[6] = control::recenter;
        CHECK(control::route(bindings, raw, false, 5).center);
        CHECK(control::route(bindings, raw, false, 5).left_trigger == 0);
        raw = {}; raw[10] = 1;
        CHECK(control::route(bindings, raw, false, 5).buttons == 0x10);
        bindings[10] = control::none;
        CHECK(control::route(bindings, raw, false, 5).buttons == 0);
        raw = {}; raw[5] = raw[8] = raw[9] = 1;
        routed = control::route(control::defaults, raw, false, 5);
        CHECK(routed.view && routed.center && control::down(control::defaults, raw, control::dpad_shift));
        bindings = control::defaults; bindings[5] = control::game_a;
        CHECK(control::route(bindings, raw, false, 5).buttons == 0x1000);
        CHECK(control::route(bindings, raw, true, 5).buttons == 0);
        CHECK(control::route(bindings, raw, true, 10).buttons == 0x1000); // click unbound
        control::TalkGate gate;
        CHECK(!gate.update(true, true, true, 1)); // held on entry cannot start microphone
        CHECK(!gate.update(true, false, true, 1));
        CHECK(gate.update(true, true, true, 1));
        CHECK(gate.update(true, false, true, 1)); // toggle stays on after release
        CHECK(!gate.update(true, true, true, 1)); // next press stops
        CHECK(!gate.update(false, true, true, 1)); // focus/session loss clears latch
        CHECK(!gate.update(true, true, true, 1)); // release before rearming
        CHECK(!gate.update(true, false, true, 1));
        CHECK(gate.update(true, true, true, 1));
        CHECK(!gate.update(true, true, true, 2)); // rebind clears latch while held
        CHECK(!gate.update(true, false, false, 2)); // switch to hold
        CHECK(gate.update(true, true, false, 2));
        CHECK(!gate.update(true, false, false, 2));
        dingosdk::vr::Settings saved;
        saved.bindings[0] = control::voice_talk;
        CHECK(dingosdk::vr::quest2_balanced(saved).bindings[0] == control::voice_talk);
    }
    {
        DepthFramePairing pair;
        CHECK(!pair.accepts(0));
        pair.keep(10);
        CHECK(!pair.accepts(9)); // stale clear while the skater/camera moved
        CHECK(!pair.ready);
        CHECK(pair.accepts(10));
        pair.complete();
        CHECK(pair.ready && !pair.pending);
        CHECK(!pair.accepts(10)); // duplicate clears cannot replace a complete pair
        pair.keep(11);
        CHECK(!pair.ready && !pair.accepts(12)); // skipped clear: never borrow a later frame
        pair.keep(12);
        CHECK(pair.accepts(12)); // resume on a newly kept colour
        pair = {}; // resize, menu transition or recenter invalidates both
        CHECK(!pair.ready && !pair.accepts(12));
    }
    {
        MenuPress menu;
        CHECK(menu.sample(true, false, 0) == MenuPress::Action::none);
        CHECK(menu.sample(true, true, 1) == MenuPress::Action::none);
        CHECK(menu.sample(true, false, 1.2) == MenuPress::Action::short_press);
        CHECK(menu.sample(true, true, 2) == MenuPress::Action::none);
        CHECK(menu.sample(true, true, 2.7) == MenuPress::Action::long_press);
        CHECK(menu.sample(true, true, 3) == MenuPress::Action::none);
        CHECK(menu.sample(true, false, 3.1) == MenuPress::Action::none);
        (void)menu.sample(true, true, 4);
        (void)menu.sample(false, true, 4.2);
        CHECK(menu.sample(true, true, 5) == MenuPress::Action::none);
        CHECK(menu.sample(true, false, 6) == MenuPress::Action::none);
        PressEdge recenter;
        CHECK(!recenter.sample(true, false));
        CHECK(recenter.sample(true, true));
        CHECK(!recenter.sample(true, true));
        CHECK(!recenter.sample(true, false));
        CHECK(recenter.sample(true, true));
        PointerClick click;
        CHECK(!click.sample(true, true)); // entering a menu with trigger held
        CHECK(!click.sample(true, false));
        CHECK(click.sample(true, true));
        CHECK(click.sample(true, true)); // drag
        CHECK(!click.sample(false, true)); // tracking/panel loss releases
        CHECK(!click.sample(true, true));
        CHECK(!click.sample(true, false));
        CHECK(click.sample(true, true));
        const Pose aim{}, panel{{0,0,0,1}, {0,0,-2}};
        const auto hit = ray_panel(aim, panel, 2, 1);
        CHECK(hit && close_to(hit->u,.5f) && close_to(hit->v,.5f) && close_to(hit->distance,2));
        CHECK(!ray_panel(Pose{{0,1,0,0},{}}, panel, 2, 1)); // points away
        CHECK(!ray_panel(Pose{{0,0,0,1},{1.1f,0,0}}, panel, 2, 1)); // misses edge
        const auto edge = ray_panel(Pose{{0,0,0,1},{.8f,.2f,0}}, panel, 2, 1);
        CHECK(edge && close_to(edge->u,.9f) && close_to(edge->v,.3f));
        const auto rotate = yaw(.7f);
        const auto move_pose = [&](Pose p) {
            p.orientation = first_person::multiply(rotate, p.orientation);
            p.position = first_person::add({1,2,3}, first_person::rotate(rotate,p.position));
            return p;
        };
        const auto moved = ray_panel(move_pose(aim), move_pose(panel), 2, 1);
        CHECK(moved && close_to(moved->u,.5f) && close_to(moved->v,.5f));
        const auto beam = beam_pose({.3f,-.3f,0}, {0,0,-2}, {0,0,0});
        CHECK(beam && close_to(beam->position, Vec3{.15f,-.15f,-1}));
        CHECK(!beam_pose({0,0,0},{0,0,0},{0,0,1}));
        // Repeated view changes/recentres: the current HMD pose maps back to
        // the native eye, rather than retaining an earlier physical offset.
        Matrix base{}; first_person::write(base, yaw(0), {2,1.7f,3});
        for (int mode = 0; mode < 8; ++mode) {
            Pose head{yaw(mode*.1f), {.05f*mode,1.6f,.03f*mode}};
            const auto anchor = anchor_from(head);
            const auto view = compose(base, 0, true, head, head, anchor, true, 1);
            CHECK(close_to(Vec3{view[12],view[13],view[14]}, Vec3{2,1.7f,3}));
            CHECK(close_to(row(view,2),Vec3{0,0,1}));
        }
    }

    {
        CHECK(close_to(dingosdk::vr::Settings{}.hud_area, .5f));
        const std::array<float, 4> native{};
        const auto smaller = hud_margins(native, .1f);
        const auto half = hud_margins(native, .5f);
        CHECK(close_to(smaller[0], .45f) && close_to(half[0], .25f));
        // A player's 80-degree rig rotation must not leak into the tracking pose.
        Matrix issued{}, rendered{};
        const auto rig = yaw(80 * pi / 180);
        const auto old_head = yaw(20 * pi / 180), new_head = yaw(25 * pi / 180);
        first_person::write(issued, first_person::multiply(rig, old_head), {3, 2, 1});
        first_person::write(rendered, first_person::multiply(rig, new_head), {3, 2, 1});
        const auto recovered = rendered_tracking_orientation(issued, old_head, rendered);
        CHECK(close_to(first_person::rotate(recovered, {0,0,-1}), first_person::rotate(new_head, {0,0,-1})));
        // Simulate 50 Hz source images and 90 Hz head tracking. For a fixed world
        // landmark the late rotation must map each target ray into the source ray,
        // including pitch and roll. No smoothing delay or orientation retagging.
        const Vec3 landmark{.2f, .3f, -2};
        for (int frame = 0; frame < 180; ++frame) {
            const float time = frame / 90.0f;
            const float source_time = std::floor(time * 50) / 50;
            const auto pose = [&](float t) {
                const auto pitch = first_person::Quat{std::sin(t*.11f),0,0,std::cos(t*.11f)};
                const auto roll = first_person::Quat{0,0,std::sin(t*.07f),std::cos(t*.07f)};
                return first_person::multiply(yaw(t*.4f), first_person::multiply(pitch, roll));
            };
            const auto source = pose(source_time), target = pose(time + .04f);
            const auto target_ray = first_person::rotate(first_person::conjugate(target), landmark);
            const auto basis = late_rotation_basis(source, target);
            Vec3 warped{};
            for (unsigned axis = 0; axis < 3; ++axis)
                for (unsigned component = 0; component < 3; ++component)
                    warped[component] += basis[axis][component] * target_ray[axis];
            CHECK(close_to(warped, first_person::rotate(first_person::conjugate(source), landmark)));
        }
    }

    {
        StickViewSwitch click;
        CHECK(!click.sample(true, false, false));
        CHECK(!click.sample(true, true, false));
        CHECK(!click.sample(true, true, false));
        CHECK(click.sample(true, false, false));
        CHECK(!click.sample(true, false, false));
        CHECK(!click.sample(true, true, false));
        CHECK(!click.sample(true, true, true));
        CHECK(!click.sample(true, false, false)); // recenter chord cancels switch
        CHECK(!click.sample(true, true, false));
        CHECK(!click.sample(false, true, false)); // menu/focus loss cancels switch
        CHECK(!click.sample(true, true, false));
        CHECK(!click.sample(true, false, false));
        CHECK(!click.sample(true, true, false));
        CHECK(click.sample(true, false, false));
        const std::array<float, 4> native{.02f, .03f, .04f, .05f};
        CHECK(hud_margins(native, 1) == native);
        const auto inset = hud_margins(native, .6f);
        CHECK(close_to(inset[0], .212f) && close_to(inset[2], .224f));
        CHECK(inset[0] > native[0] && inset[1] > native[1]);
        CHECK(eye_extent(2048, .75f) == 1536);
        CHECK(eye_extent(1920, 1) == 1920);
        CHECK(eye_extent(1, .5f) == 1);
    }

    // wrap keeps angles in (-pi, pi].
    CHECK(close_to(wrap(0), 0));
    CHECK(close_to(wrap(3 * pi), pi));
    CHECK(close_to(wrap(-pi), pi));
    CHECK(close_to(wrap(2 * pi + 0.25f), 0.25f));

    // Heading of a camera turned by yaw(h) is h, from the matrix and the quaternion.
    for (float h : {0.0f, 0.5f, -1.2f, 2.9f}) {
        const auto m = camera(yaw(h), {});
        CHECK(heading(m).has_value() && close_to(*heading(m), h));
        CHECK(close_to(heading(yaw(h)), h));
    }
    // Looking straight down still has a heading (from the up row).
    {
        const auto down = first_person::multiply(yaw(0.7f), pitch(-pi / 2));
        const auto m = camera(down, {});
        CHECK(heading(m).has_value() && close_to(*heading(m), 0.7f, 1e-3f));
        const auto up = first_person::multiply(yaw(-0.4f), pitch(pi / 2));
        CHECK(heading(camera(up, {})).has_value() && close_to(*heading(camera(up, {})), -0.4f, 1e-3f));
    }
    // Yaw by +90 degrees faces -X and its right is -Z.
    {
        const auto m = camera(yaw(pi / 2), {});
        CHECK(close_to(row(m, 2), {1, 0, 0}));
        CHECK(close_to(row(m, 0), {0, 0, -1}));
    }

    const Vec3 skater{10, 2, -5};
    // Steady heading: smooth follow, snap past the snap angle, nothing inside the dead zone.
    {
        HeadingRig rig;
        CHECK(close_to(follow_snap(rig, 0.0f, 0, 0.2f, 0.8f, 0), 0.0f));
        const float small = follow_snap(rig, 0.3f, 1.0f / 60, 0.2f, 0.8f, 0);
        CHECK(small > 0.0f && small < 0.3f); // followed smoothly
        CHECK(close_to(follow_snap(rig, 2.0f, 1.0f / 60, 0.2f, 0.8f, 0), 2.0f)); // the target jumped: snapped
        HeadingRig lagging;
        float view = 0;
        for (int i = 0; i <= 60; ++i) view = follow_snap(lagging, i * 0.05f, 1.0f / 60, 0.5f, 0.8f, 0);
        CHECK(view > 0.5f && view < 3.0f); // a steady turn that the view lags is never snapped
        CHECK(close_to(follow_snap(rig, 2.2f, 1.0f / 60, 0.2f, 0.8f, 0.35f), 2.0f)); // inside the dead zone
    }
    // Eyes heading: a fast spin is followed closely, small quick wobble is smoothed, and a spin
    // keeps going through frames where the head is upside down (its turn undefined).
    {
        EyesHeading spin;
        float view = 0;
        for (int i = 0; i <= 60; ++i) view = eyes_heading(spin, yaw(wrap(6.0f * i / 60.0f)), 1.0f / 60);
        CHECK(std::abs(wrap(view - wrap(6.0f))) < 0.35f); // within 20 degrees at the end of a 6 rad/s spin
        EyesHeading wobble;
        float low = 1e9f, high = -1e9f;
        for (int i = 0; i <= 240; ++i) {
            const float out = eyes_heading(wobble, yaw(0.1f * std::sin(2 * pi * 2.0f * i / 60.0f)), 1.0f / 60);
            if (i > 120) { low = std::min(low, out); high = std::max(high, out); }
        }
        CHECK((high - low) < 0.1f); // a 0.2 rad, 2 Hz wobble comes out at less than half
        EyesHeading upside;
        for (int i = 0; i <= 20; ++i) (void)eyes_heading(upside, yaw(3.0f * i / 60.0f), 1.0f / 60);
        float before = eyes_heading(upside, yaw(1.0f), 1.0f / 60), after = before;
        for (int i = 0; i < 6; ++i) after = eyes_heading(upside, pitch(pi), 1.0f / 60);
        CHECK(wrap(after - before) > 0.05f); // still turning the same way
    }
    // Eyes position: a steady offset passes; a crouch is scaled; no reset for small moves.
    {
        EyesPosition e;
        Vec3 out{};
        for (int i = 0; i < 120; ++i) out = eyes_position(e, {0, 1.6f, 0}, {0, 0.9f, 0}, 1.0f / 60, 0.5f);
        CHECK(close_to(out, {0, 1.6f, 0}));
        for (int i = 0; i < 60; ++i) out = eyes_position(e, {0, 1.2f, 0}, {0, 0.9f, 0}, 1.0f / 60, 0.5f);
        CHECK(out[1] > 1.32f && out[1] < 1.48f); // a 0.4 m crouch shows as about 0.2 m
    }
    // Steady tilt: the twist about +Y is removed; small wobble is smoothed, a fast flip followed.
    {
        CHECK(close_to(heading(untwisted(first_person::multiply(yaw(1.2f), pitch(0.4f)))), 0.0f));
        // The twist heading holds through a flip (pitch past vertical), where the forward axis turns round.
        CHECK(close_to(twist_heading(first_person::multiply(yaw(1.2f), pitch(2.5f))), 1.2f));
        CHECK(close_to(twist_heading(yaw(-2.0f)), -2.0f));
        // Lying on the back (a coffin): the turn stays defined (twist size about 0.7) and right;
        // upside down it is lost (size near 0), which the spin counting skips.
        const auto lying = first_person::multiply(yaw(1.0f), pitch(-pi / 2));
        CHECK(std::hypot(lying[1], lying[3]) > 0.6f);
        CHECK(close_to(twist_heading(lying), 1.0f));
        const auto inverted = pitch(pi);
        CHECK(std::hypot(inverted[1], inverted[3]) < 0.1f);
        TiltRig rig;
        (void)steady_tilt(rig, pitch(0), 1.0f / 60, 0.2f, 4.0f);
        const auto wobble = steady_tilt(rig, pitch(0.03f), 1.0f / 60, 0.2f, 4.0f);
        CHECK(std::abs(wobble[0]) < std::sin(0.015f)); // less than the 0.03 rad step
        const auto flip = steady_tilt(rig, pitch(0.6f), 1.0f / 60, 0.2f, 4.0f);
        const auto expected = pitch(0.6f);
        CHECK(std::abs(flip[0] * expected[0] + flip[1] * expected[1] + flip[2] * expected[2] + flip[3] * expected[3]) > 0.9999f);
    }
    // The seat nudge moves along the heading (forward is -Z at heading 0) and up, unscaled.
    {
        const auto base = camera(yaw(0), skater);
        const auto out = compose(base, 0, true, {}, {}, {}, true, 2.0f, 0.1f, 0.2f);
        CHECK(close_to(row(out, 3), {10, 2.2f, -5.1f}));
        const auto right = compose(base, 0, true, {}, {}, {}, true, 1.0f, 0, 0, 0.3f);
        CHECK(close_to(row(right, 3), {10.3f, 2, -5}));
    }
    // Headset at the anchor, looking ahead: the camera is the level base heading at the head.
    {
        const auto base = camera(first_person::multiply(yaw(1.0f), pitch(0.3f)), skater);
        const auto out = compose(base, 1.0f, true, {}, {}, {}, true, 1.0f);
        CHECK(orthonormal_right_handed(out));
        CHECK(close_to(heading(first_person::orientation(out)), 1.0f));
        CHECK(close_to(row(out, 1), {0, 1, 0})); // level: the base pitch is ignored
        CHECK(close_to(row(out, 3), skater));
        CHECK(out[3] == 7 && out[7] == 8 && out[11] == 9 && out[15] == 1);
    }
    // Following the full head orientation keeps the base pitch.
    {
        const auto base = camera(first_person::multiply(yaw(1.0f), pitch(0.3f)), skater);
        const auto out = compose(base, 1.0f, false, {}, {}, {}, true, 1.0f);
        CHECK(close_to(row(out, 1), row(base, 1)));
    }
    // Eye offsets turn with the skater: a right eye 3.2 cm along +X, base facing -X.
    {
        const auto base = camera(yaw(pi / 2), skater);
        const Pose eye{{0, 0, 0, 1}, {0.032f, 0, 0}};
        const auto out = compose(base, pi / 2, true, eye, {}, {}, true, 1.0f);
        CHECK(close_to(row(out, 3), {skater[0], skater[1], skater[2] - 0.032f}));
    }
    // Turning the head adds to the base heading; the anchor heading is subtracted.
    {
        const auto base = camera(yaw(1.0f), skater);
        const Pose eye{yaw(0.5f), {}};
        CHECK(close_to(heading(first_person::orientation(compose(base, 1.0f, true, eye, {}, {}, true, 1.0f))), 1.5f));
        const Anchor anchor{0.5f, {}};
        CHECK(close_to(heading(first_person::orientation(compose(base, 1.0f, true, eye, {}, anchor, true, 1.0f))), 1.0f));
    }
    // Head pitch from the headset reaches the camera.
    {
        const auto base = camera(yaw(0), skater);
        const Pose eye{pitch(0.4f), {}};
        const auto out = compose(base, 0, true, eye, {}, {}, true, 1.0f);
        CHECK(close_to(row(out, 1), first_person::rotate(pitch(0.4f), {0, 1, 0})));
    }
    // Positional tracking is relative to the anchor, rotated into the anchor's frame and scaled.
    {
        const auto base = camera(yaw(0), skater);
        const Anchor anchor{pi / 2, {1, 1.6f, 1}}; // recentered while facing -X
        const Pose eye{yaw(pi / 2), {0, 1.6f, 1}};  // moved 1 m towards -X: forward
        const auto out = compose(base, 0, true, eye, eye, anchor, true, 2.0f);
        CHECK(close_to(row(out, 3), {skater[0], skater[1], skater[2] - 2.0f}));
        CHECK(close_to(heading(first_person::orientation(out)), 0));
        // Without positional tracking only the eye's offset from the head remains.
        const Pose head{yaw(pi / 2), {0, 1.6f, 1}};
        const Pose left{yaw(pi / 2), {0, 1.6f, 1.032f}}; // facing -X, left is +Z
        const auto still = compose(base, 0, true, left, head, anchor, false, 1.0f);
        CHECK(close_to(row(still, 3), {skater[0] - 0.032f, skater[1], skater[2]}));
    }
    // Projection: one symmetric frustum covering two asymmetric eyes.
    {
        const auto left = tangents_from_angles(-0.95f, 0.78f, 0.84f, -0.95f);
        const auto right = tangents_from_angles(-0.78f, 0.95f, 0.84f, -0.95f);
        const float aspect = 1.0f;
        const float t = render_tangent({left, right}, aspect, 1.0f);
        CHECK(close_to(t, std::tan(0.95f)));
        CHECK(close_to(native_fov_degrees(t, aspect, true), 2 * 0.95f * 180 / pi, 1e-2f));
        const auto angles = layer_angles(t, aspect);
        CHECK(close_to(angles.left, -0.95f) && close_to(angles.right, 0.95f) && close_to(angles.up, 0.95f) && close_to(angles.down, -0.95f));
        // A wide image needs less vertical FOV to cover the same horizontal tangent.
        const float wide = render_tangent({left, right}, 2.0f, 1.0f);
        CHECK(close_to(wide, std::tan(0.95f)));
        CHECK(close_to(layer_angles(wide, 2.0f).right, std::atan(2 * std::tan(0.95f))));
        // A tall image needs more vertical FOV to cover the horizontal.
        const float tall = render_tangent({left, right}, 0.5f, 1.0f);
        CHECK(close_to(tall, 2 * std::tan(0.95f)));
        CHECK(close_to(native_fov_degrees(tall, 0.5f, false), 2 * 0.95f * 180 / pi, 1e-2f));
        CHECK(close_to(render_tangent({left, right}, 1.0f, 1.1f), 1.1f * std::tan(0.95f)));
    }
    // A push bends/twists the torso and can make its heading invalid. Neither
    // may change the rig yaw; only the trajectory root's actual turn does.
    {
        PlayerHeading rig;
        CHECK(close_to(player_heading(rig, 0.7f, 1.2f), 1.2f));
        CHECK(close_to(player_heading(rig, 0.7f, NAN), 1.2f));
        CHECK(close_to(player_heading(rig, 0.7f, -1.8f), 1.2f));
        CHECK(close_to(player_heading(rig, 0.8f, NAN), 1.3f));
        CHECK(close_to(player_heading(rig, NAN, NAN), 1.3f));
        // Missing samples must hold the last valid orientation, never reset to zero.
        CHECK(close_to(player_heading(rig, NAN, -2.0f), 1.3f));
        PlayerHeading uncalibrated;
        CHECK(std::isfinite(player_heading(uncalibrated, NAN, NAN)));
        CHECK(close_to(player_heading(uncalibrated, 0.7f, 1.2f), 1.2f));
        for (float chest : {0.2f, NAN, -0.8f, 1.3f, NAN, 0.2f})
            CHECK(close_to(player_heading(rig, 0.8f, chest), 1.3f));
    }
    // Animation cannot move the rigid head origin after calibration; locomotion can.
    {
        HeadOrigin rig;
        const Vec3 origin{10, 2, -5}, eye{10.1f, 3.7f, -5.05f};
        CHECK(close_to(head_origin(rig, eye, origin, 0.7f), eye));
        CHECK(close_to(head_origin(rig, {10.5f, 3.0f, -4.2f}, origin, 0.7f), eye));
        const auto moved = head_origin(rig, {}, {11, 2.2f, -7}, 0.7f);
        CHECK(close_to(moved, {11.1f, 3.9f, -7.05f}));
        rig = {};
        CHECK(close_to(head_origin(rig, {11, 3.6f, -7}, {11, 2.2f, -7}, 0.7f), {11, 3.6f, -7}));
    }
    // Looking around rotates at the eye origin, including near-vertical pitch/roll.
    {
        const Vec3 eye_origin{4, 1.7f, -2};
        const auto base = camera(yaw(0.6f), eye_origin);
        const Pose head{};
        for (float angle : {0.0f, 0.7f, -1.0f, 1.55f, -1.55f}) {
            const Quat roll{0, 0, std::sin(angle / 2), std::cos(angle / 2)};
            const Pose eye{first_person::multiply(yaw(0.4f), first_person::multiply(pitch(angle), roll)), {}};
            const auto out = compose(base, 0.6f, true, eye, head, {}, true, 1.0f);
            CHECK(close_to(row(out, 3), eye_origin));
            CHECK(orthonormal_right_handed(out));
            const auto expected = first_person::multiply(yaw(0.6f), eye.orientation);
            CHECK(close_to(row(out, 0), first_person::rotate(expected, {1, 0, 0})));
            CHECK(close_to(row(out, 1), first_person::rotate(expected, {0, 1, 0})));
            CHECK(close_to(row(out, 2), first_person::rotate(expected, {0, 0, 1})));
        }
        // Physical lean is applied once, with no artificial neck pivot or smoothing.
        const Pose lean{pitch(0.8f), {0.1f, -0.2f, -0.3f}};
        const auto out = compose(base, 0.6f, true, lean, head, {}, true, 1.0f);
        const auto translation = first_person::rotate(yaw(0.6f), lean.position);
        CHECK(close_to(row(out, 3), first_person::add(eye_origin, translation)));
    }
    // Updating an existing install must not undo the eye alignment the player tuned.
    {
        dingosdk::vr::Settings saved;
        saved.depth_strength = 0.27f;
        saved.fov_correction = 1.23f;
        saved.fov_scale = 1.07f;
        saved.fov_vertical = false;
        saved.frame_lag = 1;
        const auto updated = dingosdk::vr::quest2_head_camera(saved);
        CHECK(close_to(updated.depth_strength, saved.depth_strength));
        CHECK(close_to(updated.fov_correction, saved.fov_correction) && close_to(updated.fov_scale, saved.fov_scale));
        CHECK(!updated.fov_vertical && updated.frame_lag == 1);
        CHECK(updated.head_centred && updated.level_horizon && updated.positional);
        CHECK(close_to(updated.seat_forward, 0) && close_to(updated.seat_side, 0));
    }
    // The one-click setup enables VR and keeps unrelated player calibration.
    {
        dingosdk::vr::Settings saved;
        saved.fov_correction = 1.17f;
        saved.frame_lag = 3;
        const auto balanced = dingosdk::vr::quest2_balanced(saved);
        CHECK(balanced.enabled && balanced.auto_first_person);
        CHECK(balanced.stereo_mode == 2 && close_to(balanced.depth_strength, 0.35f));
        CHECK(close_to(balanced.world_scale, 1.0f) && close_to(balanced.fov_scale, 1.10f));
        CHECK(balanced.edge_widening == 1 && balanced.gap_fill == 1 && balanced.lens_effects_off);
        CHECK(close_to(balanced.fov_correction, 1.17f) && balanced.frame_lag == 3);
        CHECK(balanced.head_centred && balanced.level_horizon && balanced.positional);
        CHECK(close_to(balanced.seat_forward, 0) && close_to(balanced.seat_side, 0) && close_to(balanced.seat_up, 0));
        CHECK(close_to(balanced.seat_forward_foot, 0) && close_to(balanced.seat_side_foot, 0) && close_to(balanced.seat_up_foot, 0));
        CHECK(!balanced.bail_camera && !balanced.grab_view && !balanced.grab_body);
        CHECK(close_to(balanced.crouch_follow, 0) && balanced.view_mode == 0);
    }
    // Test the production UV mapping: rays agree even when principal points differ.
    {
        const Angles source{-1.05f, 1.05f, 0.99f, -0.99f};
        const Angles eyes[]{{-0.89f, 0.68f, 0.79f, -0.69f}, { -0.68f, 0.89f, 0.79f, -0.69f }};
        for (const auto& eye : eyes) {
            const auto uv = eye_uv_mapping(source, eye);
            CHECK(uv.has_value());
            if (!uv) continue;
            for (float x : {0.0f, 0.2f, 0.5f, 1.0f})
                for (float y : {0.0f, 0.5f, 0.9f, 1.0f}) {
                    const float sx = x * uv->u_scale + uv->u_bias;
                    const float sy = y * uv->v_scale + uv->v_bias;
                    CHECK(close_to((sx - 0.5f) * 2 * std::tan(source.right),
                        (1 - x) * std::tan(eye.left) + x * std::tan(eye.right)));
                    CHECK(close_to((0.5f - sy) * 2 * std::tan(source.up),
                        (1 - y) * std::tan(eye.up) + y * std::tan(eye.down)));
                    CHECK(sx >= 0 && sx <= 1 && sy >= 0 && sy <= 1);
                }
        }
        const auto identity = eye_uv_mapping(source, source);
        CHECK(identity && close_to(identity->u_scale, 1) && close_to(identity->v_scale, 1));
        CHECK(identity && close_to(identity->u_bias, 0) && close_to(identity->v_bias, 0));
        CHECK(!eye_uv_mapping(Angles{}, eyes[0]));
        CHECK(!eye_uv_mapping(source, Angles{}));
        CHECK(!eye_uv_mapping(source, Angles{0.8f, -0.8f, 0.7f, -0.7f}));
        CHECK(!eye_uv_mapping(source, Angles{-0.8f, 0.8f, 0.0f, 0.0f}));
        CHECK(!eye_uv_mapping(source, Angles{NAN, 0.8f, 0.7f, -0.7f}));
    }
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("vr_math: all checks passed\n");
    return EXIT_SUCCESS;
}
