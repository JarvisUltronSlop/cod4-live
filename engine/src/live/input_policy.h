#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace live
{
inline constexpr int Protocol = 10003;
inline constexpr const char* DemoExtension = "dm_10003";
inline constexpr float BaseFov = 65.0f;
inline constexpr float MaxFov = 90.0f;
inline constexpr float MaxFovScale = MaxFov / BaseFov;

enum InputFlags : std::uint8_t
{
    ControllerConnected = 1 << 0,
    ControllerActive = 1 << 1,
    KeyboardMouseAttempt = 1 << 2,
};
inline constexpr std::uint8_t ValidInputFlags = ControllerConnected | ControllerActive | KeyboardMouseAttempt;

constexpr bool ValidInput(std::uint8_t flags)
{
    return !(flags & ~ValidInputFlags) && (!(flags & ControllerActive) || (flags & ControllerConnected));
}

// Preserve the full signed analog range. The old PC wire format retained only signs.
constexpr std::uint16_t PackMove(int forward, int right)
{
    return static_cast<std::uint8_t>(forward) | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(right)) << 8);
}
constexpr int UnpackForward(std::uint16_t value)
{
    const int byte = value & 255;
    return byte < 128 ? byte : byte - 256;
}
constexpr int UnpackRight(std::uint16_t value)
{
    const int byte = value >> 8;
    return byte < 128 ? byte : byte - 256;
}

inline float ClampFovScale(float value)
{
    return std::isfinite(value) ? std::clamp(value, 1.0f, MaxFovScale) : 1.0f;
}
inline float ClampFov(float value)
{
    return std::isfinite(value) ? std::clamp(value, 1.0f, MaxFov) : BaseFov;
}

enum class InputAction { None, WarnKeyboardMouse, KickKeyboardMouse, WarnDisconnected, KickDisconnected, Invalid };

// Called only for fresh commands, using the server clock, never the client's timestamp.
// Unsigned subtraction deliberately handles the engine millisecond clock wrapping.
class InputPolicy
{
public:
    static constexpr std::uint32_t KeyboardGraceMs = 5000;
    static constexpr std::uint32_t CleanResetMs = 500;
    static constexpr std::uint32_t DisconnectGraceMs = 30000;

    InputAction Update(std::uint32_t now, std::uint8_t flags)
    {
        if (!ValidInput(flags)) return InputAction::Invalid;
        InputAction action = InputAction::None;
        if (flags & KeyboardMouseAttempt)
        {
            clean_ = false;
            if (!attempting_)
            {
                attempting_ = true;
                attemptStart_ = now;
                action = InputAction::WarnKeyboardMouse;
            }
            if (now - attemptStart_ >= KeyboardGraceMs) return InputAction::KickKeyboardMouse;
        }
        else if (attempting_)
        {
            // A packet gap is not proof of clean input. Only a reported clean
            // interval can reset the warning; absence alone never issues a kick.
            if (!clean_)
            {
                clean_ = true;
                cleanStart_ = now;
            }
            if (now - cleanStart_ >= CleanResetMs) attempting_ = false;
        }
        if (flags & ControllerConnected)
        {
            disconnected_ = false;
        }
        else
        {
            if (!disconnected_)
            {
                disconnected_ = true;
                disconnectStart_ = now;
                if (action == InputAction::None) action = InputAction::WarnDisconnected;
            }
            if (now - disconnectStart_ >= DisconnectGraceMs) return InputAction::KickDisconnected;
        }
        return action;
    }

    static constexpr bool BlockGameplay(std::uint8_t flags)
    {
        return !ValidInput(flags) || !(flags & ControllerConnected) || (flags & KeyboardMouseAttempt);
    }

private:
    bool attempting_ = false;
    bool clean_ = false;
    bool disconnected_ = false;
    std::uint32_t attemptStart_ = 0;
    std::uint32_t cleanStart_ = 0;
    std::uint32_t disconnectStart_ = 0;
};
}
