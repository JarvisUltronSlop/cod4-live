#pragma once
#include <string_view>

namespace live::controller
{
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
