#pragma once
#include <array>
#include <cstdint>

namespace dingosdk::game::build::v20260929::vr_hud {
// Rime safe-area layout update. See tools/quest2/HUD_LAYOUT.md for evidence.
inline constexpr std::uintptr_t update = 0x43c7050;
inline constexpr std::array<unsigned char, 16> prefix{
    0x48,0x89,0x5c,0x24,0x20,0x57,0x48,0x83,0xec,0x20,0xc5,0xfa,0x10,0x02,0xc5,0xfa};
inline constexpr std::uintptr_t critical_section = 0x50, fractions = 0x1c, dirty = 0x30, pixels = 0x78;
}
