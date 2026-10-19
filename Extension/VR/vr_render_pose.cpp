// Observe the camera copied into native shader parameters, rather than guessing
// which 60 Hz simulation pose belongs to a presented image.
#include "vr_internal.h"
#include "Engine/Game/Build/20260929/skater_render.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Core/Log/logging.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace dingosdk::vr {
void set_render_pose_available(bool available) noexcept {
    detail::shared().render_pose_available.store(available, std::memory_order_release);
    logging::write(available ? logging::Level::info : logging::Level::warning, logging::Channel::graphics,
        available ? "VR: native rendered-camera pairing ready." : "VR: native rendered-camera pairing unavailable; using legacy delay.");
}
void observe_render_view(std::uintptr_t view) noexcept {
    using namespace detail;
    namespace native = game::build::v20260929::skater_render;
    auto& s = shared();
    if (!s.tracking.load(std::memory_order_acquire) || !camera_active(seconds_now())) return;
    try {
        const auto options = current_settings();
        if (!options.enabled || options.stereo_mode != 2) return;
        std::uint32_t kind{};
        math::Matrix world{};
        float vertical_fov{};
        if (view < native::view_kind_back || !memory::peek(view - native::view_kind_back, kind) || kind != native::main_view_kind ||
            !memory::peek(view + native::view_camera_offset, world) || !memory::peek(view + native::view_fov_offset, vertical_fov) ||
            !std::isfinite(vertical_fov) || vertical_fov < 0.1f || vertical_fov > 3.05f) return;
        for (unsigned row = 0; row < 4; ++row) {
            for (unsigned axis = 0; axis < 3; ++axis)
                if (!std::isfinite(world[row * 4 + axis]) || std::abs(world[row * 4 + axis]) > 1e7f) return;
            world[row * 4 + 3] = row == 3 ? 1.0f : 0.0f;
        }
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned other = row; other < 3; ++other) {
                float dot = 0;
                for (unsigned axis = 0; axis < 3; ++axis) dot += world[row * 4 + axis] * world[other * 4 + axis];
                if (std::abs(dot - (row == other ? 1.0f : 0.0f)) > 0.05f) return;
            }
        const double now = seconds_now();
        const auto generation = s.recenter_generation.load(std::memory_order_acquire);
        const auto actual = first_person::orientation(world);
        std::lock_guard lock(s.records_mutex);
        const EyeRecord* nearest = nullptr;
        float best = std::numeric_limits<float>::infinity();
        for (auto it = s.issued.rbegin(); it != s.issued.rend(); ++it) {
            if (now - it->sampled > 0.5) break;
            if (!it->both || it->generation != generation) continue;
            const auto placed = first_person::orientation(it->world);
            float dot = 0, distance = 0;
            for (unsigned axis = 0; axis < 4; ++axis) dot += actual[axis] * placed[axis];
            for (unsigned axis = 0; axis < 3; ++axis) {
                const float delta = world[12 + axis] - it->world[12 + axis];
                distance += delta * delta;
            }
            if (distance > 1.0f || std::abs(dot) < 0.98f) continue; // no mirror/reflection cameras
            const float score = distance + 16 * (1 - std::min(std::abs(dot), 1.0f));
            if (score < best) { best = score; nearest = &*it; }
        }
        if (!nearest) return;
        EyeRecord record = *nearest;
        const auto orientation = math::rendered_tracking_orientation(record.world, record.pose.orientation, world);
        const auto delta = first_person::multiply(orientation, first_person::conjugate(record.pose.orientation));
        for (auto* eye : {&record.left, &record.right}) {
            eye->orientation = first_person::normalized(first_person::multiply(delta, eye->orientation));
            eye->position = first_person::add(record.pose.position,
                first_person::rotate(delta, first_person::subtract(eye->position, record.pose.position)));
        }
        record.pose.orientation = orientation;
        record.world = world;
        // Native render view stores the vertical FOV in radians.
        record.fov = math::layer_angles(std::tan(vertical_fov * 0.5f), s.image_aspect.load(std::memory_order_acquire));
        record.sequence = ++s.render_sequence;
        record.sampled = now;
        s.rendered = record;
        s.rendered_frames.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {} // A lost observation must never interrupt rendering.
}
}
