// Settings and the VR camera: moves the first-person camera to the eye whose
// turn it is, and records which pose produced the frame.
#include "vr_internal.h"
#include "Engine/Core/Log/logging.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <format>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace dingosdk::vr {
namespace detail {
Shared& shared() {
    static auto* value = new Shared; // Lives until process exit; never torn down from DllMain.
    return *value;
}
Settings current_settings() {
    auto& s = shared();
    std::lock_guard lock(s.settings_mutex);
    return s.settings;
}
double seconds_now() noexcept {
    static const auto frequency = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return static_cast<double>(f.QuadPart); }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / frequency;
}
}

bool valid(const Settings& v) noexcept {
    const auto in = [](float value, float low, float high) { return std::isfinite(value) && value >= low && value <= high; };
    return in(v.turn_smoothing, 0, Limits::turn_smoothing_max) &&
        in(v.world_scale, Limits::world_scale_min, Limits::world_scale_max) &&
        in(v.fov_correction, Limits::fov_correction_min, Limits::fov_correction_max) &&
        in(v.seat_forward, Limits::seat_forward_min, Limits::seat_max) && in(v.seat_up, Limits::seat_min, Limits::seat_max) &&
        in(v.seat_side, Limits::seat_min, Limits::seat_max) &&
        in(v.chat_view_size, Limits::chat_view_size_min, Limits::chat_view_size_max) &&
        in(v.chat_view_x, -Limits::chat_view_offset_max, Limits::chat_view_offset_max) &&
        in(v.chat_view_y, -Limits::chat_view_offset_max, Limits::chat_view_offset_max) &&
        in(v.seat_forward_foot, Limits::seat_forward_min, Limits::seat_max) &&
        in(v.seat_up_foot, Limits::seat_min, Limits::seat_max) && in(v.seat_side_foot, Limits::seat_min, Limits::seat_max) &&
        in(v.fov_scale, Limits::fov_scale_min, Limits::fov_scale_max) &&
        v.frame_lag >= 0 && v.frame_lag <= Limits::frame_lag_max &&
        v.hide_body >= 0 && v.hide_body <= Limits::hide_body_max && v.hide_body_foot >= 0 &&
        v.hide_body_foot <= Limits::hide_body_max &&
        v.view_mode >= 0 && v.view_mode <= Limits::view_mode_max && in(v.tilt_filter, 0.0f, Limits::tilt_filter_max) &&
        v.stereo_mode >= 0 && v.stereo_mode <= Limits::stereo_mode_max && in(v.depth_strength, 0.0f, Limits::depth_strength_max) &&
        v.edge_widening >= 0 && v.edge_widening <= Limits::edge_widening_max && v.gap_fill >= 0 && v.gap_fill <= 1 &&
        v.look_turn_degrees >= 0 &&
        v.look_turn_degrees <= Limits::look_turn_max && v.look_turn_style >= 0 && v.look_turn_style <= 1 &&
        in(v.crouch_follow, 0.0f, 1.0f) &&
        in(v.smooth_turn_speed, Limits::smooth_turn_speed_min, Limits::smooth_turn_speed_max) && in(v.comfort_vignette, 0.0f, Limits::comfort_vignette_max) &&
        in(v.third_distance, Limits::third_distance_min, Limits::third_distance_max) &&
        in(v.third_height, Limits::third_height_min, Limits::third_height_max) &&
        in(v.theater_distance, Limits::theater_distance_min, Limits::theater_distance_max) &&
        in(v.theater_width, Limits::theater_width_min, Limits::theater_width_max) &&
        v.recenter_key >= 0 && v.recenter_key <= 0xfe;
}

Settings settings() noexcept { return detail::current_settings(); }

bool set_settings(const Settings& value) noexcept {
    if (!valid(value)) return false;
    auto& s = detail::shared();
    {
        std::lock_guard lock(s.settings_mutex);
        s.settings = value;
    }
    s.settings_changed.store(true, std::memory_order_release);
    return true;
}

bool take_settings_changed() noexcept { return detail::shared().settings_changed.exchange(false); }

void set_ui_open(bool open) noexcept { detail::shared().ui_open.store(open, std::memory_order_release); }
namespace {
struct PresetStore {
    std::mutex mutex;
    std::vector<Preset> saved;
    bool changed = false;
};
PresetStore& preset_store() {
    static auto* value = new PresetStore;
    return *value;
}
// Built-in presets for people trying the mod. First person is the defaults; the other
// two start from them.
std::vector<Preset> builtin_presets() {
    Preset third{"Third person", {}, true};
    // Close to the game's own camera: behind and a little above, level, following smoothly.
    third.settings.view_mode = 1;
    third.settings.third_distance = 3.0f;
    third.settings.third_height = 0.6f;
    third.settings.turn_smoothing = 0.6f;
    third.settings.world_scale = 1.0f;
    third.settings.seat_forward = third.settings.seat_side = third.settings.seat_up = 0.0f;
    third.settings.seat_forward_foot = third.settings.seat_side_foot = third.settings.seat_up_foot = 0.0f;
    Preset tiny{"Tiny world", {}, true};
    // Third person from far and high with a giant's eye separation: the skater looks like a
    // small figure roaming a model world, and leaning moves you around it at that scale.
    tiny.settings.view_mode = 1;
    tiny.settings.third_distance = 8.0f;
    tiny.settings.third_height = 3.0f;
    tiny.settings.turn_smoothing = 1.2f;
    tiny.settings.world_scale = 4.0f;
    tiny.settings.seat_forward = tiny.settings.seat_side = tiny.settings.seat_up = 0.0f;
    tiny.settings.seat_forward_foot = tiny.settings.seat_side_foot = tiny.settings.seat_up_foot = 0.0f;
    return {{"First person", Settings{}, true}, std::move(third), std::move(tiny)};
}
}

std::vector<Preset> presets() {
    auto result = builtin_presets();
    auto& store = preset_store();
    std::lock_guard lock(store.mutex);
    result.insert(result.end(), store.saved.begin(), store.saved.end());
    return result;
}
bool apply_preset(const std::string& name) noexcept {
    try {
        for (const auto& preset : presets())
            if (preset.name == name) {
                const auto current = settings();
                auto next = preset.settings;
                next.enabled = current.enabled;
                next.recenter_key = current.recenter_key;
                return set_settings(next);
            }
    } catch (...) {}
    return false;
}
bool save_preset(const std::string& name) noexcept {
    try {
        if (name.empty() || name.size() > 40) return false;
        for (const auto& preset : presets())
            if (preset.builtin && preset.name == name) return false;
        auto& store = preset_store();
        std::lock_guard lock(store.mutex);
        const auto current = settings();
        for (auto& preset : store.saved)
            if (preset.name == name) {
                preset.settings = current;
                store.changed = true;
                return true;
            }
        store.saved.push_back({name, current, false});
        store.changed = true;
        return true;
    } catch (...) {
        return false;
    }
}
bool delete_preset(const std::string& name) noexcept {
    auto& store = preset_store();
    std::lock_guard lock(store.mutex);
    const auto before = store.saved.size();
    std::erase_if(store.saved, [&](const Preset& preset) { return preset.name == name; });
    store.changed = store.changed || store.saved.size() != before;
    return store.saved.size() != before;
}
std::vector<Preset> saved_presets() {
    auto& store = preset_store();
    std::lock_guard lock(store.mutex);
    return store.saved;
}
void set_saved_presets(std::vector<Preset> saved) noexcept {
    auto& store = preset_store();
    std::lock_guard lock(store.mutex);
    store.saved = std::move(saved);
}
bool take_presets_changed() noexcept {
    auto& store = preset_store();
    std::lock_guard lock(store.mutex);
    return std::exchange(store.changed, false);
}

void set_game_menu(bool open) noexcept { detail::shared().game_menu.store(open, std::memory_order_release); }
bool game_menu_open() noexcept { return detail::shared().game_menu.load(std::memory_order_acquire); }
std::atomic<int> right_stick_x{0};
std::array<std::atomic<int>, 4> pad_right_x{}, pad_trigger{};
std::atomic<int> trigger_level{0};
std::atomic<bool> body_showing{false};
bool wants_first_person(bool first_person_on) noexcept {
    try {
        if (first_person_on) return false;
        const auto options = settings();
        if (!options.enabled || !options.auto_first_person || !status().running) return false;
        // At most once a second: the game takes it only once a level is playable.
        static double next = 0;
        const double now = detail::seconds_now();
        if (now < next) return false;
        next = now + 1.0;
        return true;
    } catch (...) {
        return false;
    }
}
bool whole_body_view() noexcept { return body_showing.load(std::memory_order_relaxed); }
std::atomic<bool> last_on_board{true};
void note_on_board(bool on_board) noexcept { last_on_board.store(on_board, std::memory_order_relaxed); }
bool on_board_view() noexcept { return last_on_board.load(std::memory_order_relaxed); }
void observe_gamepad(std::uint32_t user, std::int16_t right_x, std::uint8_t trigger) noexcept {
    if (user >= pad_right_x.size()) return;
    pad_right_x[user].store(right_x, std::memory_order_relaxed);
    pad_trigger[user].store(trigger, std::memory_order_relaxed);
    int pressed = 0;
    for (const auto& t : pad_trigger) pressed = std::max(pressed, t.load(std::memory_order_relaxed));
    trigger_level.store(pressed, std::memory_order_relaxed);
    // The strongest of the XInput pads.
    int strongest = 0;
    for (const auto& x : pad_right_x)
        if (std::abs(x.load(std::memory_order_relaxed)) > std::abs(strongest)) strongest = x.load(std::memory_order_relaxed);
    right_stick_x.store(strongest, std::memory_order_relaxed);
}
void recenter() noexcept { detail::shared().recenter_generation.fetch_add(1, std::memory_order_acq_rel); }

namespace {
using namespace detail;

// Engine-thread state of the VR camera.
struct Camera {
    std::mutex mutex;
    std::uint32_t next_view = 0;
    math::HeadingRig heading_rig; // third person: the followed travel direction
    // First person: the eyes' filters, the fading difference from the heading the view had when
    // they took over (from the on-foot heading), and whether they drove the view last update.
    math::EyesHeading eyes;
    math::EyesPosition eyes_place;
    math::HeadOrigin head_origin;
    math::PlayerHeading player_heading;
    std::optional<BodyPose> last_body;
    bool head_mode = false;
    float eyes_blend = 0;
    bool eyes_on = false;
    math::TiltRig tilt_rig;
    double rig_time = 0;
    bool turn_held = false; // look turn: the right stick must return before the next
    // Bail camera: tilted (or upright again) since; the fixed eye and its heading.
    bool bail = false;
    double tilted_since = 0, upright_since = 0;
    // Grabs: in the air with a trigger held, last seen at grab_seen; the outside view (flip &
    // bail, grab) shares bail_eye and bail_heading.
    bool grab = false, outside = false;
    double grab_seen = 0;
    // Where the skater has been (body origins about 0.3 m apart): ground the third-person and
    // outside cameras stay above, and how far the camera is lifted now.
    std::deque<std::array<float, 3>> trail;
    float lift = 0;
    float boom = 1;       // the share of the distance behind the skater the camera keeps now
    float seat_level = 1; // how much of the seat offset applies (fades out on walls)
    float seat_board = 1; // 1 the board's seat offset, 0 the on-foot one; blends between
    std::array<float, 3> bail_eye{};
    float bail_heading = 0;
    // Comfort vignette: the last turn the game applied (everything but the headset) and the level.
    math::Quat last_turn{0, 0, 0, 1};
    bool turn_ready = false;
    float vignette = 0;
    float vignette_setting = 0;
    double vignette_preview_until = 0;
    float travel = std::numeric_limits<float>::quiet_NaN();
    std::array<float, 2> velocity{};
    float climb = 0;
    std::array<float, 3> origin{};
    double origin_time = 0;
    double render_seen = -100, last_write = -100, last_log = -100, last_skip_log = -100;
    float heading = std::numeric_limits<float>::quiet_NaN();
    math::Anchor anchor;
    std::uint64_t recenter_applied = 0;
};
Camera& camera() {
    static auto* value = new Camera;
    return *value;
}

bool locate(Shared& s, XrTime display_time, std::array<XrView, 2>& views, math::Pose& head) {
    XrViewLocateInfo info{XR_TYPE_VIEW_LOCATE_INFO};
    info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    info.displayTime = display_time;
    info.space = s.local_space;
    XrViewState state{XR_TYPE_VIEW_STATE};
    views = {XrView{XR_TYPE_VIEW}, XrView{XR_TYPE_VIEW}};
    std::uint32_t count = 0;
    if (XR_FAILED(s.xr.locate_views(s.session, &info, &state, 2, &count, views.data())) || count != 2) return false;
    if (!(state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) return false;
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(s.xr.locate_space(s.view_space, s.local_space, display_time, &location)) ||
        !(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) return false;
    head = to_pose(location.pose);
    if (!(state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
        !(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
        // Rotation-only tracking: keep the eyes' offsets around a fixed head.
        for (auto& view : views) {
            view.pose.position.x -= location.pose.position.x;
            view.pose.position.y -= location.pose.position.y;
            view.pose.position.z -= location.pose.position.z;
        }
        head.position = {};
    }
    return true;
}
}

bool apply_camera(std::array<float, 16>& matrix, CameraWriter writer, const BodyPose* body) noexcept {
    auto& s = shared();
    if (!s.tracking.load(std::memory_order_acquire)) return false;
    const auto options = current_settings();
    if (!options.enabled) return false;
    try {
        std::shared_lock xr_lock(s.xr_mutex);
        if (!s.tracking.load(std::memory_order_acquire) || !s.session) return false;
        const XrTime predicted = s.predicted_display_time.load(std::memory_order_acquire);
        const XrDuration period = s.display_period.load(std::memory_order_acquire);
        if (predicted <= 0) return false;
        // This write's image reaches the screen `frame_lag` frames after the
        // frame the headset is waiting for now.
        // Image pairing is separate from head prediction. Use the runtime's current
        // predicted time in head-centred mode; old image poses remain attached to
        // their images so OpenXR can reproject them correctly.
        const XrTime display_time = options.head_centred ? predicted
            : predicted + period * static_cast<XrDuration>(options.frame_lag);
        std::array<XrView, 2> views{};
        math::Pose head;
        if (!locate(s, display_time, views, head)) return false;
        xr_lock.unlock();

        auto& c = camera();
        std::lock_guard lock(c.mutex);
        const double now = seconds_now();
        if (body) c.last_body = *body;
        else if (options.head_centred && options.view_mode == 0 && c.last_body) body = &*c.last_body;
        // The render handoff ends each frame. Without it (its hook unavailable),
        // the tick is the only writer and ends the frame itself.
        if (writer == CameraWriter::render) c.render_seen = now;
        const bool frame_end = writer == CameraWriter::render || now - c.render_seen > camera_timeout;

        const auto recenter = s.recenter_generation.load(std::memory_order_acquire);
        if (recenter != c.recenter_applied || now - c.last_write > 1.0) {
            // Recentre on request and whenever VR takes the camera back.
            c.anchor = math::anchor_from(head);
            if (now - c.last_log > 1.0) {
                c.last_log = now;
                logging::write(logging::Level::info, logging::Channel::graphics,
                    std::format("VR: recentred; head at ({:.2f}, {:.2f}, {:.2f}) m; head-centred {}, frame pairing {}, extra prediction {} frames.",
                        head.position[0], head.position[1], head.position[2], options.head_centred,
                        options.frame_lag, options.head_centred ? 0 : options.frame_lag));
            }
            c.recenter_applied = recenter;
            c.heading = std::numeric_limits<float>::quiet_NaN();
            c.travel = std::numeric_limits<float>::quiet_NaN();
            c.velocity = {};
            c.heading_rig = {};
            c.eyes = {};
            c.eyes_place = {};
            c.head_origin = {};
            c.player_heading = {};
            c.eyes_on = false;
            c.tilt_rig = {};
        }
        if (body && frame_end && c.origin_time > 0 &&
            first_person::length(first_person::subtract(body->origin, c.origin)) > 2.0f) {
            // A teleport (a session marker, a respawn): the origin jumps, faster than any skating.
            // The view starts again from the skater: first person through its eyes, third person
            // behind it, instead of keeping the old direction (which could face the skater).
            c.heading = options.view_mode == 0 ? std::numeric_limits<float>::quiet_NaN() : body->chest_heading; // NaN: the head's heading
            c.travel = std::numeric_limits<float>::quiet_NaN();
            c.velocity = {};
            c.heading_rig = {};
            c.eyes = {};
            c.eyes_place = {};
            c.head_origin = {};
            c.player_heading = {};
            c.eyes_on = false;
            c.tilt_rig = {};
            c.trail.clear();
            c.bail = c.outside = false;
            c.grab_seen = 0;
            c.origin_time = 0; // no velocity across the jump
        }
        if (body && frame_end) {
            // Direction of travel from the origin's velocity, averaged over a quarter second
            // and held while slower than 0.5 m/s (third person follows it), and the climb (the
            // third-person ground guard).
            const double dt = now - c.origin_time;
            if (dt > 0.001 && dt < 0.5) {
                const float seconds = static_cast<float>(dt), blend = 1 - std::exp(-seconds / 0.25f);
                c.velocity[0] += ((body->origin[0] - c.origin[0]) / seconds - c.velocity[0]) * blend;
                c.velocity[1] += ((body->origin[2] - c.origin[2]) / seconds - c.velocity[1]) * blend;
                if (std::hypot(c.velocity[0], c.velocity[1]) > 0.5f) c.travel = math::heading_of(c.velocity[0], c.velocity[1]);
                c.climb += ((body->origin[1] - c.origin[1]) / seconds - c.climb) * blend;
            }
            c.origin = body->origin;
            c.origin_time = now;
        }
        const float rig_elapsed = frame_end && c.rig_time > 0 ? static_cast<float>(now - c.rig_time) : 0.0f;
        if (frame_end) c.rig_time = now;
        const bool first = options.view_mode == 0;
        const bool head_centred = first && options.head_centred;
        if (c.head_mode != head_centred) {
            c.head_mode = head_centred;
            c.head_origin = {};
            c.player_heading = {};
            c.eyes_place = {};
        }

        // The view's heading.
        //  - First person: through the skater's eyes (math::eyes_heading): the head's turn,
        //    with slow wobble smoothed and spins, coffins and carves passed.
        //  - First person on foot with Walk where you look: the heading holds and the headset
        //    and the right stick turn it. The game moves the skater relative to the camera, so a
        //    view that followed the walk would feed back into the stick.
        //  - Third person: the direction of travel, followed lazily (Turn smoothing), held when
        //    standing, like the game's own camera.
        const bool look = body && first && options.walk_where_you_look && !body->on_board && std::isfinite(c.heading);
        if (body && frame_end) {
            // The right stick: turns on foot, orbits the outside views. Any XInput pad, the VR
            // controllers, or a DirectInput pad.
            int x = right_stick_x.load(std::memory_order_relaxed);
            if (const auto vr_pad = controller_pad(); vr_pad.active && std::abs(vr_pad.right_x) > std::abs(x)) x = vr_pad.right_x;
            if (look || c.outside) {
                for (const int other : {static_cast<int>(directinput_right_x()), static_cast<int>(xinput_peek().right_x)})
                    if (std::abs(other) > std::abs(x)) x = other;
            }
            float turn = 0;
            if ((look || c.outside) && options.look_turn_style == 1) {
                // Smooth: the stick past its dead zone sets the speed (right turns clockwise).
                constexpr float dead = 8000;
                if (std::abs(x) > dead)
                    turn = -std::copysign((std::abs(x) - dead) / (32767 - dead), static_cast<float>(x)) * options.smooth_turn_speed *
                        math::pi / 180 * rig_elapsed;
                c.turn_held = false;
            } else {
                const int flick = x > 22000 ? 1 : x < -22000 ? -1 : 0;
                if (flick && !c.turn_held && (look || c.outside) && options.look_turn_degrees > 0)
                    turn = -flick * static_cast<float>(options.look_turn_degrees) * math::pi / 180;
                c.turn_held = flick != 0 || (c.turn_held && std::abs(x) > 8000);
            }
            if (turn != 0 && c.outside) c.bail_heading = math::wrap(c.bail_heading + turn);
            else if (turn != 0 && look) c.heading = math::wrap(c.heading + turn);
        }
        if (body && head_centred) {
            // Locomotion turns the player rig; animated neck/head twists do not.
            if (!look) c.heading = math::player_heading(c.player_heading, body->trajectory_heading, body->chest_heading);
            c.eyes_on = false;
            c.heading_rig.value = c.heading;
        } else if (body && first) {
            const float eyes = math::eyes_heading(c.eyes, first_person::orientation(matrix), frame_end ? rig_elapsed : 0.0f);
            if (!look) {
                // Taking over from the on-foot heading (or at the start), the difference fades over 0.4 s.
                if (!c.eyes_on) c.eyes_blend = std::isfinite(c.heading) ? math::wrap(c.heading - eyes) : 0.0f;
                else if (frame_end) c.eyes_blend *= std::exp(-rig_elapsed / 0.4f);
                c.heading = math::wrap(eyes + c.eyes_blend);
            }
            c.eyes_on = !look;
            c.heading_rig.value = c.heading;
        } else if (body) {
            const float wanted = std::isfinite(c.travel) ? c.travel : std::isfinite(c.heading) ? c.heading : math::heading(matrix).value_or(0.0f);
            c.heading = math::follow_snap(c.heading_rig, wanted, rig_elapsed, options.turn_smoothing, 0.0f, 0.0f);
            c.eyes_on = false;
        } else if (const auto target = math::heading(matrix); target && (!std::isfinite(c.heading) || frame_end)) {
            // No body (the free camera's writer): the head camera's heading, followed lazily.
            c.heading = std::isfinite(c.heading) ? math::follow_snap(c.heading_rig, *target, rig_elapsed, options.turn_smoothing, 0.0f, 0.0f)
                                                 : (c.heading_rig.value = *target);
        }
        if (!std::isfinite(c.heading)) c.heading = 0;

        auto base = matrix;
        const bool body_near = body && std::abs(matrix[12] - body->origin[0]) < 3.0f &&
            std::abs(matrix[13] - body->origin[1]) < 3.0f && std::abs(matrix[14] - body->origin[2]) < 3.0f;
        if (body && head_centred && (body_near || c.head_origin.ready)) {
            const auto at = math::head_origin(c.head_origin, {matrix[12], matrix[13], matrix[14]},
                body->origin, c.heading);
            for (std::size_t i = 0; i < 3; ++i) base[12 + i] = at[i];
        } else if (body_near) {
            // The view's position: the body origin plus the head's offset from it through one
            // continuous filter (math::eyes_position): bob smoothed, ollies and flips passed,
            // crouches scaled by Crouch.
            const auto eyes_at = math::eyes_position(c.eyes_place, {matrix[12], matrix[13], matrix[14]}, body->origin,
                frame_end ? rig_elapsed : 0.0f, options.crouch_follow);
            for (std::size_t i = 0; i < 3; ++i) base[12 …13533 tokens truncated…f = f;
        }
    }
    float3 c = saturate(color.SampleLevel(linear_clamp, float2(x, scene_uv.y), 0).rgb);
    if (decode == 0) c = srgb_to_linear(c);
    return float4(comfort(c, v.uv, vignette), 1);
}
)";

template<class Function>
Function system_function(const wchar_t* module_name, const char* name) {
    HMODULE module = GetModuleHandleW(module_name);
    if (!module) module = LoadLibraryExW(module_name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return module ? reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(module, name))) : nullptr;
}
std::string hresult(const char* what, HRESULT result) {
    return std::format("{} failed (0x{:08x}).", what, static_cast<unsigned long>(result));
}
bool compile(pD3DCompile compiler, const char* entry, const char* target, ComPtr<ID3DBlob>& out, std::string& error) {
    ComPtr<ID3DBlob> messages;
    const HRESULT result = compiler(shader_source, sizeof(shader_source) - 1, "reskate_vr_stereo", nullptr, nullptr, entry,
        target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &out, &messages);
    if (SUCCEEDED(result)) return true;
    error = hresult("Compiling the VR stereo shader", result);
    if (messages) error += std::string(" ") + static_cast<const char*>(messages->GetBufferPointer());
    return false;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource, subresource, before, after};
    return barrier;
}
}

bool DepthStereo::create(ID3D12Device* device, std::string& error) {
    destroy();
    const auto compiler = system_function<pD3DCompile>(L"d3dcompiler_47.dll", "D3DCompile");
    const auto serialize = system_function<PFN_D3D12_SERIALIZE_ROOT_SIGNATURE>(L"d3d12.dll", "D3D12SerializeRootSignature");
    if (!compiler || !serialize) { error = "d3dcompiler_47.dll or d3d12.dll is unavailable."; return false; }
    if (!compile(compiler, "vs_main", "vs_5_0", vertex_, error) || !compile(compiler, "ps_main", "ps_5_0", pixel_, error) ||
        !compile(compiler, "ps_dilate", "ps_5_0", dilate_, error))
        return false;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 2;
    std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants = {0, 0, 9};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = static_cast<UINT>(parameters.size());
    root.pParameters = parameters.data();
    root.NumStaticSamplers = 1;
    root.pStaticSamplers = &sampler;
    ComPtr<ID3DBlob> blob, messages;
    HRESULT result = serialize(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &messages);
    if (FAILED(result)) { error = hresult("Serializing the VR stereo root signature", result); return false; }
    result = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_));
    if (FAILED(result)) { error = hresult("Creating the VR stereo root signature", result); return false; }
    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = slots * 4; // per slot: colour + depth for the eyes, (unused) + raw depth for the widening
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srvs_));
    if (FAILED(result)) { error = hresult("Creating the VR stereo SRV heap", result); return false; }
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    result = device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvs_));
    if (FAILED(result)) { error = hresult("Creating the VR stereo RTV heap", result); return false; }
    srv_stride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    rtv_stride_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (auto& allocator : allocators_) {
        result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(result)) { error = hresult("Creating a VR stereo command allocator", result); return false; }
    }
    result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(), nullptr, IID_PPV_ARGS(&list_));
    if (FAILED(result)) { error = hresult("Creating the VR stereo command list", result); return false; }
    list_->Close();
    list_->SetName(L"ReSkate VR depth stereo");
    result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(result)) { error = hresult("Creating the VR stereo fence", result); return false; }
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) { error = "Creating the VR stereo fence event failed."; return false; }
    device_ = device;
    return true;
}

bool DepthStereo::wait(UINT64 value, DWORD timeout_ms) noexcept {
    if (!value || !fence_) return true;
    const auto done = fence_->GetCompletedValue();
    if (done == UINT64_MAX) return false;
    if (done >= value) return true;
    if (FAILED(fence_->SetEventOnCompletion(value, event_))) return false;
    if (WaitForSingleObject(event_, timeout_ms) != WAIT_OBJECT_0) return false;
    return fence_->GetCompletedValue() >= value;
}
bool DepthStereo::wait_idle(DWORD timeout_ms) noexcept { return wait(next_fence_, timeout_ms); }

void DepthStereo::destroy() noexcept {
    if (fence_) (void)wait_idle(2000);
    pipelines_.clear();
    list_.Reset();
    for (auto& allocator : allocators_) allocator.Reset();
    slot_fences_ = {};
    srvs_.Reset(); rtvs_.Reset(); root_.Reset(); vertex_.Reset(); pixel_.Reset(); dilate_.Reset(); fence_.Reset(); kept_.Reset();
    kept_depth_.Reset(); widened_.Reset();
    if (event_) CloseHandle(event_);
    event_ = nullptr;
    next_fence_ = 0;
    slot_ = 0;
    kept_format_ = DXGI_FORMAT_UNKNOWN;
    device_.Reset();
}

ID3D12PipelineState* DepthStereo::pipeline(DXGI_FORMAT format, std::string& error, bool dilate) {
    const int key = static_cast<int>(format) | (dilate ? 0x10000 : 0);
    if (const auto found = pipelines_.find(key); found != pipelines_.end()) return found->second.Get();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root_.Get();
    desc.VS = {vertex_->GetBufferPointer(), vertex_->GetBufferSize()};
    const auto& shader = dilate ? dilate_ : pixel_;
    desc.PS = {shader->GetBufferPointer(), shader->GetBufferSize()};
    for (auto& target : desc.BlendState.RenderTarget) {
        target.SrcBlend = target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlend = target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOp = target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        target.LogicOp = D3D12_LOGIC_OP_NOOP;
        target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.FrontFace = desc.DepthStencilState.BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
        D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = format;
    desc.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> state;
    const HRESULT result = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state));
    if (FAILED(result)) { error = hresult("Creating the VR stereo pipeline", result); return nullptr; }
    return (pipelines_[key] = state).Get();
}

bool DepthStereo::begin(std::string& error) {
    if (!device_) { error = "The VR stereo pass is not initialised."; return false; }
    if (!wait(slot_fences_[slot_], 1000)) { error = "The GPU did not finish an earlier VR stereo pass."; return false; }
    HRESULT result = allocators_[slot_]->Reset();
    if (SUCCEEDED(result)) result = list_->Reset(allocators_[slot_].Get(), nullptr);
    if (FAILED(result)) { error = hresult("Resetting the VR stereo command list", result); return false; }
    return true;
}

bool DepthStereo::submit(ID3D12CommandQueue* queue, std::string& error) {
    HRESULT result = list_->Close();
    if (FAILED(result)) { error = hresult("Closing the VR stereo command list", result); return false; }
    ID3D12CommandList* lists[] = {list_.Get()};
    queue->ExecuteCommandLists(1, lists);
    const UINT64 signal = ++next_fence_;
    result = queue->Signal(fence_.Get(), signal);
    if (FAILED(result)) { error = hresult("Signalling the VR stereo fence", result); return false; }
    slot_fences_[slot_] = signal;
    slot_ = (slot_ + 1) % slots;
    return true;
}

bool DepthStereo::keep(ID3D12CommandQueue* queue, ID3D12Resource* frame, std::string& error) {
    const auto desc = frame->GetDesc();
    if (!kept_ || kept_->GetDesc().Width != desc.Width || kept_->GetDesc().Height != desc.Height || kept_format_ != desc.Format) {
        if (!wait_idle(2000)) { error = "The GPU did not finish an earlier VR stereo pass."; return false; }
        kept_.Reset();
        auto copy = desc;
        copy.Flags = D3D12_RESOURCE_FLAG_NONE;
        copy.MipLevels = 1;
        D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
        const HRESULT result = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &copy,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&kept_));
        if (FAILED(result)) { error = hresult("Creating the VR stereo colour copy", result); return false; }
        kept_format_ = desc.Format;
    }
    if (!begin(error)) return false;
    const std::array<D3D12_RESOURCE_BARRIER, 2> before{
        transition(frame, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE),
        transition(kept_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)};
    list_->ResourceBarrier(static_cast<UINT>(before.size()), before.data());
    list_->CopyResource(kept_.Get(), frame);
    const std::array<D3D12_RESOURCE_BARRIER, 2> after{
        transition(frame, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT),
        transition(kept_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
    list_->ResourceBarrier(static_cast<UINT>(after.size()), after.data());
    return submit(queue, error);
}

bool DepthStereo::keep_depth(ID3D12CommandQueue* queue, ID3D12Resource* depth, int widening, std::string& error) {
    const auto desc = depth->GetDesc();
    if (!kept_depth_ || kept_depth_->GetDesc().Width != desc.Width || kept_depth_->GetDesc().Height != desc.Height ||
        kept_depth_->GetDesc().Format != desc.Format) {
        if (!wait_idle(2000)) { error = "The GPU did not finish an earlier VR stereo pass."; return false; }
        kept_depth_.Reset();
        D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
        const HRESULT result = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&kept_depth_));
        if (FAILED(result)) { error = hresult("Creating the VR stereo depth copy", result); return false; }
    }
    if (!begin(error)) return false;
    const std::array<D3D12_RESOURCE_BARRIER, 2> before{
        transition(depth, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, 0),
        transition(kept_depth_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST, 0)};
    list_->ResourceBarrier(static_cast<UINT>(before.size()), before.data());
    D3D12_TEXTURE_COPY_LOCATION to{kept_depth_.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    D3D12_TEXTURE_COPY_LOCATION from{depth, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    list_->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    const std::array<D3D12_RESOURCE_BARRIER, 2> after{
        transition(depth, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST, 0),
        transition(kept_depth_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0)};
    list_->ResourceBarrier(static_cast<UINT>(after.size()), after.data());

    // Widen the near depth into an R32 target the eyes read.
    if (!widened_ || widened_->GetDesc().Width != desc.Width || widened_->GetDesc().Height != desc.Height) {
        widened_.Reset();
        D3D12_RESOURCE_DESC target{};
        target.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        target.Width = desc.Width;
        target.Height = desc.Height;
        target.DepthOrArraySize = target.MipLevels = 1;
        target.Format = DXGI_FORMAT_R32_FLOAT;
        target.SampleDesc.Count = 1;
        target.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
        const HRESULT result = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &target,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&widened_));
        if (FAILED(result)) { error = hresult("Creating the VR stereo widened depth", result); return false; }
    }
    auto* state = pipeline(DXGI_FORMAT_R32_FLOAT, error, true);
    if (!state) return false;
    list_->SetPipelineState(state);
    auto srv = srvs_->GetCPUDescriptorHandleForHeapStart();
    srv.ptr += static_cast<SIZE_T>(slot_ * 4 + 2) * srv_stride_;
    auto srv_gpu = srvs_->GetGPUDescriptorHandleForHeapStart();
    srv_gpu.ptr += static_cast<UINT64>(slot_ * 4 + 2) * srv_stride_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    view.Format = kept_format_ != DXGI_FORMAT_UNKNOWN ? kept_format_ : DXGI_FORMAT_R8G8B8A8_UNORM;
    if (kept_) device_->CreateShaderResourceView(kept_.Get(), &view, srv); // t0, unused by the widening
    view.Format = DXGI_FORMAT_R32_FLOAT; // the depth copy is R32_TYPELESS (vr_depth.cpp)
    auto depth_srv = srv;
    depth_srv.ptr += srv_stride_;
    device_->CreateShaderResourceView(kept_depth_.Get(), &view, depth_srv);
    auto rtv = rtvs_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(slot_ * 2) * rtv_stride_;
    D3D12_RENDER_TARGET_VIEW_DESC target_view{};
    target_view.Format = DXGI_FORMAT_R32_FLOAT;
    target_view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(widened_.Get(), &target_view, rtv);
    list_->ResourceBarrier(1, std::array{transition(widened_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET)}.data());
    ID3D12DescriptorHeap* heaps[] = {srvs_.Get()};
    list_->SetDescriptorHeaps(1, heaps);
    list_->SetGraphicsRootSignature(root_.Get());
    list_->SetGraphicsRootDescriptorTable(0, srv_gpu);
    const std::array<UINT, 4> radius{static_cast<UINT>(std::clamp(widening, 0, 3)), 0, 0, 0};
    list_->SetGraphicsRoot32BitConstants(1, static_cast<UINT>(radius.size()), radius.data(), 0);
    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0, 1};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
    list_->RSSetViewports(1, &viewport);
    list_->RSSetScissorRects(1, &scissor);
    list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    list_->DrawInstanced(3, 1, 0, 0);
    list_->ResourceBarrier(1, std::array{transition(widened_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)}.data());
    return submit(queue, error);
}

bool DepthStereo::render(ID3D12CommandQueue* queue, const std::array<ID3D12Resource*, 2>& targets, DXGI_FORMAT target_format,
    const StereoParams& params, bool raw, std::string& error) {
    if (!kept_ || !widened_) { error = "No frame kept for the VR stereo pass yet."; return false; }
    auto* state = pipeline(target_format, error);
    if (!state || !begin(error)) return false;
    list_->SetPipelineState(state);
    auto srv = srvs_->GetCPUDescriptorHandleForHeapStart();
    srv.ptr += static_cast<SIZE_T>(slot_) * 4 * srv_stride_;
    auto srv_gpu = srvs_->GetGPUDescriptorHandleForHeapStart();
    srv_gpu.ptr += static_cast<UINT64>(slot_) * 4 * srv_stride_;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = kept_format_;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(kept_.Get(), &view, srv);
    view.Format = DXGI_FORMAT_R32_FLOAT;
    auto depth_srv = srv;
    depth_srv.ptr += srv_stride_;
    device_->CreateShaderResourceView(widened_.Get(), &view, depth_srv);

    ID3D12DescriptorHeap* heaps[] = {srvs_.Get()};
    list_->SetDescriptorHeaps(1, heaps);
    list_->SetGraphicsRootSignature(root_.Get());
    list_->SetGraphicsRootDescriptorTable(0, srv_gpu);
    list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT mode = (raw ? 2u : kept_format_ == DXGI_FORMAT_R16G16B16A16_FLOAT ? 1u : 0u) | (params.background_fill ? 0x100u : 0u);
    for (std::size_t eye = 0; eye < targets.size(); ++eye) {
        auto rtv = rtvs_->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(slot_ * 2 + eye) * rtv_stride_;
        D3D12_RENDER_TARGET_VIEW_DESC target_view{};
        target_view.Format = target_format;
        target_view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device_->CreateRenderTargetView(targets[eye], &target_view, rtv);
        // Left eye (+1): near points move right in its image; right eye (-1): left.
        const float side = eye == 0 ? 1.0f : -1.0f;
        std::array<UINT, 9> constants{mode, 0, 0, 0, 0, 0, 0, 0, 0};
        std::memcpy(&constants[1], &side, sizeof(float));
        std::memcpy(&constants[2], &params.scale, sizeof(float));
        std::memcpy(&constants[3], &params.max_shift, sizeof(float));
        std::memcpy(&constants[4], &params.vignette, sizeof(float));
        std::memcpy(&constants[5], &params.u_scale[eye], sizeof(float));
        std::memcpy(&constants[6], &params.v_scale[eye], sizeof(float));
        std::memcpy(&constants[7], &params.u_bias[eye], sizeof(float));
        std::memcpy(&constants[8], &params.v_bias[eye], sizeof(float));
        list_->SetGraphicsRoot32BitConstants(1, static_cast<UINT>(constants.size()), constants.data(), 0);
        const auto desc = targets[eye]->GetDesc();
        const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height), 0, 1};
        const D3D12_RECT scissor{0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
        list_->RSSetViewports(1, &viewport);
        list_->RSSetScissorRects(1, &scissor);
        list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        list_->DrawInstanced(3, 1, 0, 0);
    }
    return submit(queue, error);
}
}
