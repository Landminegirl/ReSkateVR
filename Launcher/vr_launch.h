#pragma once
#include "Engine/Core/Platform/launcher_support.h"
#include <algorithm>

namespace dingosdk::launcher_vr {
inline launcher::LaunchOptions launch_options(launcher::LaunchOptions saved, bool vr, int square_size = 2048) {
    saved.vr = vr;
    if (vr) {
        saved.force_windowed = true;
        saved.width = saved.height = static_cast<unsigned>(std::clamp(square_size, 720, 3840));
    }
    return saved;
}
}
