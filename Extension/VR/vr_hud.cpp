// Native safe-area layout only: no scaling or cropping of the scene image.
#include "vr_internal.h"
#include "Engine/Core/Hooks/hooks.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Game/Build/20260929/vr_hud.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace dingosdk::vr {
namespace {
namespace native = game::build::v20260929::vr_hud;
using Update = void (*)(std::uintptr_t, const float*);
std::atomic<Update> original{};
std::atomic<bool> available{};
struct NativeLock {
    CRITICAL_SECTION* lock;
    explicit NativeLock(std::uintptr_t at) : lock(reinterpret_cast<CRITICAL_SECTION*>(at)) { EnterCriticalSection(lock); }
    ~NativeLock() { LeaveCriticalSection(lock); }
};
void update(std::uintptr_t object, const float* size) {
    const auto forward = original.load(std::memory_order_acquire);
    // The native caller owns the object and size for this synchronous invocation.
    // Its critical section is recursive; retain it over the original's nested lock.
    const auto options = detail::current_settings();
    auto& shared = detail::shared();
    const bool active = options.enabled && shared.tracking.load(std::memory_order_acquire) &&
        !shared.game_menu.load(std::memory_order_acquire);
    NativeLock lock(object + native::critical_section);
    std::array<float, 4> fractions{}, cached{};
    std::memcpy(fractions.data(), reinterpret_cast<const void*>(object + native::fractions), sizeof(fractions));
    std::memcpy(cached.data(), reinterpret_cast<const void*>(object + native::pixels), sizeof(cached));
    const bool sane = size && std::isfinite(size[0]) && std::isfinite(size[1]) && size[0] > 0 && size[1] > 0 &&
        size[0] < 32768 && size[1] < 32768 && std::all_of(fractions.begin(), fractions.end(),
            [](float v) { return std::isfinite(v) && v >= 0 && v < 0.5f; });
    if (!sane) { forward(object, size); return; }
    const auto desired = math::hud_margins(fractions, active ? options.hud_area : 1.0f);
    bool changed = false;
    for (std::size_t i = 0; i < desired.size(); ++i)
        changed = changed || !std::isfinite(cached[i]) || std::abs(cached[i] - desired[i] * size[i / 2]) > 0.01f;
    if (changed) *reinterpret_cast<unsigned char*>(object + native::dirty) = 1;
    std::memcpy(reinterpret_cast<void*>(object + native::fractions), desired.data(), sizeof(desired));
    forward(object, size);
    // Native fractions always remain intact; cached pixels restore on the next
    // layout update when VR stops, a menu opens, or the slider returns to 100%.
    std::memcpy(reinterpret_cast<void*>(object + native::fractions), fractions.data(), sizeof(fractions));
}
}
bool hud_layout_available() noexcept { return available.load(std::memory_order_acquire); }
void start_hud_layout(std::uintptr_t base) noexcept {
    if (available.load(std::memory_order_acquire)) return;
    std::array<unsigned char, native::prefix.size()> bytes{};
    if (!memory::read(base + native::update, bytes) || bytes != native::prefix) {
        logging::write(logging::Level::warning, logging::Channel::graphics, "VR: native HUD layout signature differs; control disabled.");
        return;
    }
    auto* target = reinterpret_cast<void*>(base + native::update);
    void* trampoline = nullptr;
    if (hook_prepare(target, reinterpret_cast<void*>(&update), &trampoline) != HookOk || !trampoline) return;
    original.store(reinterpret_cast<Update>(trampoline), std::memory_order_release);
    if (hook_enable(target) == HookOk) {
        available.store(true, std::memory_order_release);
        logging::write(logging::Level::info, logging::Channel::graphics, "VR: native HUD safe-area slider ready.");
    }
}
}
