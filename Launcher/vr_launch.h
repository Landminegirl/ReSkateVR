#pragma once
#include "Engine/Core/Platform/launcher_support.h"

namespace dingosdk::launcher_vr {
inline launcher::LaunchOptions launch_options(launcher::LaunchOptions saved, bool vr) {
    saved.vr = vr;
    if (vr) {
        saved.force_windowed = true;
        saved.width = saved.height = 2048;
    }
    return saved;
}
}
