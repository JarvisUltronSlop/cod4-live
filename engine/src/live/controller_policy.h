#pragma once
#include <string_view>
#include <ui/keycodes.h>

namespace live::controller
{
struct DeviceAvailability
{
    bool present;
    bool dispatchInput;
};
// Presence is hardware freshness, independent of window focus. This lets an
// alt-tabbed client report unplug/stale-device state without emitting controls.
constexpr DeviceAvailability Availability(bool focused, bool selected, bool fresh,
    unsigned lastSample, unsigned now)
{
    const bool present = selected && (fresh || (lastSample && now - lastSample < 250));
    return {present, present && focused};
}
// IW4x Engine/Key.cpp menu mapping, expressed with CoD4 keyboard codes.
constexpr int MenuKeyForButton(int key)
{
    switch(key) {
    case 1: case 14: return K_ENTER;
    case 2: case 15: return K_ESCAPE;
    case 3: return K_F2;
    case 4: return K_F1;
    default: return 0;
    }
}
// The legacy mouse poll emits an event even when the cursor did not move.
// Routing that event into UI hover clears controller-selected item focus.
constexpr bool PreserveMenuFocus(bool controllerInUse, bool menu, int dx, int dy)
{
    return controllerInUse && menu && dx == 0 && dy == 0;
}

// Matches IW4x's weapon rule. A free-fire RPG is eligible when its weapon
// definition supplies range/ammunition; only weapons requiring lock-on opt out.
constexpr bool WeaponAllowsAssist(bool requireLockonToFire, int ammunition)
{
    return !requireLockonToFire && ammunition > 0;
}

inline constexpr unsigned UsingOffhandFlag = 0x2;
constexpr bool UsingOffhand(unsigned weaponFlags, bool offhandWeapon)
{
    return (weaponFlags & UsingOffhandFlag) != 0 && offhandWeapon;
}

// The physical source is attached to command dispatch, never inferred from the
// last UI device. This also covers config/console actions queued for later.
inline bool IsGameplayCommand(std::string_view name)
{
    auto equal = [name](std::string_view other)
    {
        if (name.size() != other.size()) return false;
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            const char c = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i];
            if (c != other[i]) return false;
        }
        return true;
    };
    if (name.empty()) return false;
    if (name[0] == '+' || name[0] == '-')
    {
        // All native action commands use +/-; voice and scoreboard are UI.
        return !equal("+talk") && !equal("-talk") && !equal("+scores") && !equal("-scores");
    }
    constexpr std::string_view actions[]{"centerview", "toggleads", "leaveads", "lowerstance",
        "raisestance", "togglecrouch", "toggleprone", "goprone", "gocrouch", "weapnext", "weapprev"};
    for (const auto action : actions) if (equal(action)) return true;
    return false;
}

struct ActionLatch
{
    bool active{}, observed{}, releasePending{};
    void Press() { active = true; observed = releasePending = false; }
    void Clear() { active = observed = releasePending = false; }
    void CommandBuilt() { if (active) observed = true; }
    bool RequestRelease()
    {
        if (!active || observed) return true;
        releasePending = true;
        return false;
    }
    bool ReleaseReady() const { return active && observed && releasePending; }
};
}
