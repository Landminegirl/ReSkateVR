#pragma once
#include <imgui.h>
#include <algorithm>
namespace dingosdk::overlay::detail {
struct ChatShortcut { bool clicked; ImVec2 low, high; };
inline ChatShortcut vr_chat_button(bool connected, ImVec2 display) {
    const float scale = std::clamp(display.y / 1080.0f, 0.75f, 2.0f);
    ImGui::BeginDisabled(!connected);
    const bool clicked = ImGui::Button("CHAT", ImVec2(std::min(display.x * .7f, 360.0f * scale), 88.0f * scale));
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    ImGui::EndDisabled();
    if (!connected) ImGui::TextUnformatted("Join a multiplayer session to chat.");
    return {clicked, low, high};
}
}
