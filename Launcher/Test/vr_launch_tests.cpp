#include "Launcher/vr_launch.h"
#include "Extension/VR/vr.h"
#include <cstdio>

int main() {
    dingosdk::launcher::LaunchOptions saved;
    saved.width = 2560;
    saved.height = 1440;
    saved.force_windowed = false;
    saved.offline = true;
    saved.map = L"test-map";
    const auto standard = dingosdk::launcher_vr::launch_options(saved, false);
    const auto vr = dingosdk::launcher_vr::launch_options(saved, true);
    if (standard.vr || standard.width != 2560 || standard.height != 1440 || standard.force_windowed ||
        !standard.offline || standard.map != saved.map) return 1;
    if (!vr.vr || vr.width != 2048 || vr.height != 2048 || !vr.force_windowed ||
        !vr.offline || vr.map != saved.map) return 2;
    if (saved.width != 2560 || saved.height != 1440 || saved.force_windowed) return 3;
    dingosdk::vr::Settings profile;
    profile.enabled = true;
    profile.depth_strength = 0.27f;
    profile.frame_lag = 1;
    const auto flat = dingosdk::vr::apply_launch_choice(profile, '0');
    if (flat.enabled || flat.depth_strength != profile.depth_strength || flat.frame_lag != 1) return 4;
    if (!dingosdk::vr::apply_launch_choice(flat, '1').enabled) return 5;
    if (!dingosdk::vr::apply_launch_choice(profile, '\0').enabled) return 6;
    std::puts("vr_launch: standard display settings, VR startup and session-only mode checks passed");
    return 0;
}
