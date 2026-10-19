#pragma once
namespace dingosdk {
struct ChatAccess { bool session = false, passive = false; };
constexpr ChatAccess chat_access(bool connected, bool visible, bool game_menu) noexcept {
    return {connected, connected && visible && !game_menu};
}
}
