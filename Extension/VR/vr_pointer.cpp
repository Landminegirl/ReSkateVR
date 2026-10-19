#include "vr_internal.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace dingosdk::vr {
namespace {
struct PointerState {
    std::mutex mutex;
    MenuPointer latest;
    double updated = -100;
    math::PointerClick click;
    bool context_ready = false, overlay = false, native_down = false;
};
PointerState& pointer() { static auto* value = new PointerState; return *value; }
bool native_button(bool down) {
    INPUT event{};
    event.type = INPUT_MOUSE;
    event.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    return SendInput(1, &event, sizeof(event)) == 1;
}
}
void cancel_menu_pointer() noexcept { detail::release_menu_pointer(); }
MenuPointer menu_pointer() noexcept {
    auto& s = pointer();
    std::lock_guard lock(s.mutex);
    return detail::seconds_now() - s.updated < 0.25 ? s.latest : MenuPointer{};
}
namespace detail {
void publish_menu_pointer(HWND window, bool active, const std::optional<math::PanelHit>& hit, bool trigger, bool overlay) noexcept {
    auto& s = pointer();
    std::lock_guard lock(s.mutex);
    const bool foreground = window && GetAncestor(GetForegroundWindow(), GA_ROOT) == GetAncestor(window, GA_ROOT);
    active = active && foreground;
    if (!s.context_ready || s.overlay != overlay) {
        s.click = {};
        s.overlay = overlay;
        s.context_ready = true;
    }
    const bool down = s.click.sample(active && hit.has_value(), trigger);
    if (s.native_down && (overlay || !down) && native_button(false)) s.native_down = false;
    if (active && hit && !overlay) {
        RECT client{};
        POINT point{};
        if (GetClientRect(window, &client) && client.right > 1 && client.bottom > 1) {
            point.x = static_cast<LONG>(std::lround(hit->u * (client.right - 1)));
            point.y = static_cast<LONG>(std::lround(hit->v * (client.bottom - 1)));
            if (ClientToScreen(window, &point)) {
                const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                if (width > 1 && height > 1) {
                    INPUT move{};
                    move.type = INPUT_MOUSE;
                    move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
                    move.mi.dx = static_cast<LONG>(std::lround((point.x - GetSystemMetrics(SM_XVIRTUALSCREEN)) * 65535.0 / (width - 1)));
                    move.mi.dy = static_cast<LONG>(std::lround((point.y - GetSystemMetrics(SM_YVIRTUALSCREEN)) * 65535.0 / (height - 1)));
                    (void)SendInput(1, &move, sizeof(move));
                    if (down && !s.native_down) s.native_down = native_button(true);
                }
            }
        }
    }
    s.latest = {active && overlay, hit.has_value(), down, hit ? hit->u : 0, hit ? hit->v : 0};
    s.updated = seconds_now();
}
void release_menu_pointer() noexcept { publish_menu_pointer(nullptr, false, std::nullopt, false, false); }
}
}
