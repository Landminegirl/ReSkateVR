#pragma once
#include <atomic>
#include <cstdint>
namespace dingosdk::vr {
// A posted startup handoff is retried after failure/lost foreground, never looped
// after delivery. The HWND is represented as an integer for portable policy tests.
class MenuActivation {
    std::atomic<std::uintptr_t> pending{}, completed{};
public:
    bool request(std::uintptr_t window, bool foreground) noexcept {
        if (!window || !foreground || completed.load() == window) return false;
        return pending.exchange(window) != window;
    }
    void cancel(std::uintptr_t window) noexcept { pending.compare_exchange_strong(window, 0); }
    bool deliver(std::uintptr_t window, bool foreground) noexcept {
        auto expected = window;
        if (!window || !pending.compare_exchange_strong(expected, 0) || !foreground || completed.load() == window) return false;
        completed.store(window);
        return true;
    }
    void destroyed(std::uintptr_t window) noexcept {
        cancel(window);
        completed.compare_exchange_strong(window, 0);
    }
};
}
