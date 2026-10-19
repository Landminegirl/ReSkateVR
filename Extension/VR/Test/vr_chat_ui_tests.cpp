#include "Engine/Game/Multiplayer/chat_access.h"
#include "Extension/UI/Overlay/vr_chat_widgets.h"
#include <cstdio>
#include <stdexcept>
using dingosdk::overlay::detail::ChatShortcut;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
ChatShortcut frame(bool down, ImVec2 point, bool connected, ImVec2 display = {1280, 1080}) {
    auto& io = ImGui::GetIO();
    io.DisplaySize = display;
    io.DeltaTime = 1.0f / 90.0f;
    io.AddFocusEvent(true);
    io.AddMousePosEvent(point.x, point.y);
    io.AddMouseButtonEvent(0, down);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({20,20}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x - 40, display.y - 40}, ImGuiCond_Always);
    ImGui::Begin("Chat menu test", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    const auto result = dingosdk::overlay::detail::vr_chat_button(connected, display);
    ImGui::End();
    ImGui::Render();
    return result;
}
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    unsigned char* pixels{}; int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    try {
        const auto access = dingosdk::chat_access(true, true, true); // actual pause-menu availability regression
        check(!access.passive && access.session, "pause menu must retain explicit chat access");
        auto button = frame(false, {0,0}, access.session);
        check(button.high.x - button.low.x >= 360 && button.high.y - button.low.y >= 88, "large chat target");
        const ImVec2 center{(button.low.x + button.high.x)*.5f, (button.low.y + button.high.y)*.5f};
        frame(false, center, access.session);
        check(!frame(true, center, access.session).clicked, "press waits for release");
        check(frame(false, center, access.session).clicked, "connected pause-menu click must open chat");
        frame(false, center, false);
        frame(true, center, false);
        check(!frame(false, center, false).clicked, "disconnected chat must remain disabled");
        button = frame(false, {0,0}, true, {2560,2160});
        check(button.high.x-button.low.x >= 720 && button.high.y-button.low.y >= 176, "large render sizes retain target proportions");
        std::puts("Connected pause-menu Chat clicks, offline disabling and scaled target sizes passed.");
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); ImGui::DestroyContext(); return 1; }
    ImGui::DestroyContext();
}
