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
    bool target_valid = active && hit && math::panel_uv_valid(hit->u, hit->v);
    POINT point{};
    const auto root = window ? GetAncestor(window, GA_ROOT) : nullptr;
    if (target_valid && !overlay) {
        RECT client{};
        if (!GetClientRect(window, &client) || client.right < 2 || client.bottom < 2) target_valid = false;
        else {
            point.x = static_cast<LONG>(std::lround(hit->u * (client.right - 1)));
            point.y = static_cast<LONG>(std::lround(hit->v * (client.bottom - 1)));
            if (!ClientToScreen(window, &point)) target_valid = false;
            else {
                const auto under = WindowFromPoint(point);
                // An image coordinate can fall beyond the visible desktop when the
                // square client is taller than its monitor. Never click that desktop
                // location, another program, or a covering window.
                target_valid = under && GetAncestor(under, GA_ROOT) == root;
            }
        }
    }
    bool down = s.click.sample(target_valid, trigger);
    if (s.native_down && (overlay || !down) && native_button(false)) s.native_down = false;
    if (target_valid && !overlay) {
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        const int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        if (width > 1 && height > 1 && point.x >= x && point.y >= y && point.x < x + width && point.y < y + height) {
            INPUT move{};
            move.type = INPUT_MOUSE;
            move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
            move.mi.dx = static_cast<LONG>(std::lround((point.x - x) * 65535.0 / (width - 1)));
            move.mi.dy = static_cast<LONG>(std::lround((point.y - y) * 65535.0 / (height - 1)));
            POINT actual{};
            const auto under = GetCursorPos(&actual) ? WindowFromPoint(actual) : nullptr;
            const bool owned = under && GetAncestor(under, GA_ROOT) == root &&
                GetAncestor(GetForegroundWindow(), GA_ROOT) == root;
            INPUT events[2]{move, {}};
            const bool press = owned && down && !s.native_down;
            events[1].type = INPUT_MOUSE;
            events[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            // Submit the validated move+press together. Windows keeps this input
            // batch ordered, without a physical mouse event slipping between them.
            // If the cursor starts outside the game, first move it inside and defer
            // a press until a later frame confirms game ownership.
            const UINT count = press ? 2u : 1u;
            const UINT sent = SendInput(count, events, sizeof(INPUT));
            if (press && sent == 2) s.native_down = true;
            if (!sent) {
                down = s.click.sample(false, trigger);
                if (s.native_down && native_button(false)) s.native_down = false;
            }
        } else {
            down = s.click.sample(false, trigger);
            if (s.native_down && native_button(false)) s.native_down = false;
        }
    }
    s.latest = {active && overlay, target_valid, down, hit ? hit->u : 0, hit ? hit->v : 0};
    s.updated = seconds_now();
}
void release_menu_pointer() noexcept { publish_menu_pointer(nullptr, false, std::nullopt, false, false); }
}
}
