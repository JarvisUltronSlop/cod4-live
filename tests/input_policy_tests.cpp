#include <live/input_policy.h>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
constexpr auto Pad = live::ControllerConnected | live::ControllerActive;
constexpr auto Mixed = Pad | live::KeyboardMouseAttempt;
}

int main()
{
    using live::InputAction;
    using live::InputPolicy;

    // Every representable axis pair must survive transport, including sub-deadzone values.
    for (int forward = -128; forward <= 127; ++forward)
    for (int right = -128; right <= 127; ++right)
    {
        const auto packed = live::PackMove(forward, right);
        Require(live::UnpackForward(packed) == forward, "forward analog precision lost");
        Require(live::UnpackRight(packed) == right, "right analog precision lost");
    }

    InputPolicy continuous;
    Require(continuous.Update(0, Mixed) == InputAction::WarnKeyboardMouse, "first attempt must warn");
    for (unsigned time = 50; time < 5000; time += 50)
        Require(continuous.Update(time, Mixed) == InputAction::None, "must not kick before five seconds");
    Require(continuous.Update(5000, Mixed) == InputAction::KickKeyboardMouse, "continued mixed input must kick at five seconds");

    InputPolicy accidental;
    Require(accidental.Update(1000, Mixed) == InputAction::WarnKeyboardMouse, "accidental mouse move warns");
    Require(accidental.Update(1499, Pad) == InputAction::None, "returning to pad does not kick");
    Require(accidental.Update(1500, Pad) == InputAction::None, "clean interval resets");
    Require(accidental.Update(10000, Pad) == InputAction::None, "single attempt must never cause delayed kick");
    Require(accidental.Update(10001, Mixed) == InputAction::WarnKeyboardMouse, "new attempt starts a new warning");

    InputPolicy held;
    held.Update(100, live::KeyboardMouseAttempt | live::ControllerConnected);
    for (unsigned time = 200; time < 5100; time += 100)
    {
        // A controller event between forbidden samples must not clear the KB+M window.
        held.Update(time - 1, Pad);
        Require(held.Update(time, Mixed) == InputAction::None, "controller wiggle must not reset attempt tracking");
    }
    Require(held.Update(5100, Mixed) == InputAction::KickKeyboardMouse, "held key mixed with controller still kicks");

    InputPolicy sparse;
    Require(sparse.Update(0, Mixed) == InputAction::WarnKeyboardMouse, "sparse first attempt warns");
    for (unsigned time = 1000; time < 5000; time += 1000)
        Require(sparse.Update(time, Mixed) == InputAction::None, "packet gaps must not reset warnings");
    Require(sparse.Update(5000, Mixed) == InputAction::KickKeyboardMouse, "sparse continuing attempts still kick");
    InputPolicy clean;
    clean.Update(0, Mixed);
    clean.Update(100, Pad);
    clean.Update(599, Pad);
    Require(clean.Update(600, Pad) == InputAction::None, "reported clean interval clears warning");
    Require(clean.Update(5000, Mixed) == InputAction::WarnKeyboardMouse, "clean interval earns a new grace window");

    InputPolicy unplugged;
    Require(unplugged.Update(400, 0) == InputAction::WarnDisconnected, "unplug must warn");
    Require(unplugged.Update(30399, 0) == InputAction::None, "allow full reconnect window");
    Require(unplugged.Update(30400, 0) == InputAction::KickDisconnected, "unplugged for thirty seconds must kick");
    InputPolicy reconnect;
    reconnect.Update(100, 0);
    Require(reconnect.Update(1000, live::ControllerConnected) == InputAction::None, "idle reconnected controller accepted");
    Require(reconnect.Update(40000, live::ControllerConnected) == InputAction::None, "connected idle never times out");
    Require(reconnect.Update(40001, 0) == InputAction::WarnDisconnected, "new disconnect gets a new grace period");

    InputPolicy wrap;
    const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - 2500;
    wrap.Update(start, Mixed);
    for (unsigned elapsed = 100; elapsed < 5000; elapsed += 100) wrap.Update(start + elapsed, Mixed);
    Require(wrap.Update(start + 5000, Mixed) == InputAction::KickKeyboardMouse, "timer must tolerate uint32 wrap");

    InputPolicy invalid;
    Require(invalid.Update(0, 128) == InputAction::Invalid, "reserved input flag rejected");
    Require(invalid.Update(0, live::ControllerActive) == InputAction::Invalid, "active absent device rejected");
    Require(InputPolicy::BlockGameplay(Mixed), "mixed gameplay blocked");
    Require(InputPolicy::BlockGameplay(0), "no controller gameplay blocked");
    Require(!InputPolicy::BlockGameplay(live::ControllerConnected), "idle controller stays valid");
    Require(!InputPolicy::BlockGameplay(Pad), "controller gameplay accepted");

    Require(live::ClampFovScale(0.5f) == 1.0f, "minimum scale");
    Require(live::ClampFovScale(2.0f) == live::MaxFovScale, "maximum scale");
    Require(live::ClampFovScale(std::numeric_limits<float>::quiet_NaN()) == 1.0f, "NaN scale falls back");
    Require(live::ClampFovScale(std::numeric_limits<float>::infinity()) == 1.0f, "infinite scale falls back");
    Require(live::ClampFov(65.0f * live::MaxFovScale) <= 90.0f, "hipfire FOV capped");
    Require(live::ClampFov(180.0f) == 90.0f, "final projection rejects expansion");
    Require(live::ClampFov(20.0f) == 20.0f, "ADS FOV retains zoom");
    Require(live::ClampFov(std::numeric_limits<float>::quiet_NaN()) == 65.0f, "NaN FOV safe fallback");
    std::cout << "PASS: analog precision, mixed-input policy, timers, reconnection, and FOV guards\n";
}
