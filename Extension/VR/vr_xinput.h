#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Xinput.h>

namespace dingosdk::vr {
// Advertise the same virtual pad that GetState supplies during initial discovery.
// Keep real capabilities, other slots, invalid requests and standard starts intact.
inline DWORD virtual_controller_capabilities(DWORD user, DWORD flags, DWORD result, bool enabled, XINPUT_CAPABILITIES* output) noexcept {
    if (!enabled || user != 0 || !output || (flags & ~XINPUT_FLAG_GAMEPAD) || result != ERROR_DEVICE_NOT_CONNECTED) return result;
    *output = {};
    output->Type = XINPUT_DEVTYPE_GAMEPAD;
    output->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    output->Gamepad.wButtons = 0xf3ff;
    output->Gamepad.bLeftTrigger = output->Gamepad.bRightTrigger = 255;
    output->Gamepad.sThumbLX = output->Gamepad.sThumbLY = output->Gamepad.sThumbRX = output->Gamepad.sThumbRY = 32767;
    // No rumble advertised: OpenXR haptic output is not implemented here.
    return ERROR_SUCCESS;
}
}
