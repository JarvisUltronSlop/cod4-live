// Hardware diagnostic using exactly the discovery/driver/normalization stack
// linked into cod4-live. It never injects keyboard, mouse or game input.
#include <live/iw4x/Device/Discovery.hpp>
#include <live/iw4x/Driver/Set.hpp>
#include <live/iw4x/Calibration/Normalize.hpp>
#include <live/iw4x/Calibration/Profile.hpp>
#include <live/iw4x/Aim/Deadzone.hpp>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>

namespace
{
using namespace Controller;
std::mutex printMutex;
volatile std::sig_atomic_t stopped = 0;
void Stop(int) { stopped = 1; }

void Print(const std::string& message)
{
    std::lock_guard lock(printMutex);
    std::cout << message << std::endl;
}
struct Diagnostics final : diagnostic_sink
{
    void consume(const diagnostic& event) override
    {
        std::ostringstream out;
        out << '[' << to_string(event.level) << "/" << to_string(event.origin)
            << "] " << event.message;
        Print(out.str());
    }
};
struct Observation
{
    family kind{};
    connection link{};
    unsigned samples{}, seenButtons{};
    float peakLeft{}, peakRight{}, peakLT{}, peakRT{};
    canonical_sample last{};
    timestamp printed{};
};
std::string Buttons(button_set buttons)
{
    std::string result;
    for (unsigned i = 0; i < static_cast<unsigned>(button::count); ++i)
        if (buttons.down(static_cast<button>(i)))
        {
            if (!result.empty()) result += ',';
            result += to_string(static_cast<button>(i));
        }
    return result.empty() ? "none" : result;
}
}

int main(int argc, char** argv)
{
    int duration = 15;
    bool rumble = false, native = true;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg(argv[i]);
        if (arg == "--rumble") rumble = true;
        else if (arg == "--steam") native = false;
        else if (arg == "--seconds" && i + 1 < argc)
        {
            char* end = nullptr;
            const long value = std::strtol(argv[++i], &end, 10);
            if (!end || *end || value < 1 || value > 120)
            {
                std::cerr << "--seconds must be between 1 and 120\n";
                return 3;
            }
            duration = static_cast<int>(value);
        }
        else
        {
            std::cout << "controller-probe [--seconds 1..120] [--steam] [--rumble]\n"
                         "Native mode is default. --steam reads XInput only.\n"
                         "--rumble sends one low-strength 250ms pulse to the first sampled device.\n"
                         "Close cod4-live while probing to avoid competing device output.\n";
            return arg == "--help" ? 0 : 3;
        }
    }
    std::signal(SIGINT, Stop);
    try
    {
        Diagnostics diagnostics;
        context ctx(diagnostics, false);
        transport::xinput_module xinput(ctx);
        registry devices(ctx);
        discovery discovery(ctx, devices, xinput, native);
        driver::set drivers(ctx, xinput);
        std::map<device_id, Observation> observations;
        auto started = Controller::clock::now();
        auto until = started + std::chrono::seconds(duration);
        std::uint64_t generation = ~std::uint64_t{};
        device_id rumbling{};
        timestamp rumbleUntil{};
        bool pulseSent = false;
        Print(std::string("Observing production controller stack for ") + std::to_string(duration) +
            " seconds; backend=" + (native ? "native HID + XInput" : "Steam/XInput only") +
            ". Move sticks, pull triggers and press buttons.");
        while (!stopped && Controller::clock::now() < until)
        {
            discovery.scan();
            drivers.reconcile(devices);
            driver::output_policy output;
            output.rumble = rumble;
            output.haptics = driver::haptic_mode::emulated;
            drivers.configure(output);
            const auto now = Controller::clock::now();
            if (generation != devices.generation())
            {
                generation = devices.generation();
                unsigned hid = 0, xbox = 0;
                devices.for_each([&](const device_connection& device)
                {
                    hid += device.transport == transport_kind::hid;
                    xbox += device.transport == transport_kind::xinput;
                    std::ostringstream out;
                    out << "DEVICE id=" << device.id << " family=" << device.identity.family
                        << " transport=" << device.transport << " link=" << device.link;
                    Print(out.str());
                });
                Print("DISCOVERY devices=" + std::to_string(devices.size()) + " bound=" +
                    std::to_string(drivers.size()) + " hid=" + std::to_string(hid) +
                    " xinput=" + std::to_string(xbox));
            }
            drivers.for_each([&](driver::driver& device, const device_connection& connection)
            {
                raw_sample raw;
                canonical_sample sample;
                if (!device.poll(raw, sample)) return;
                calibration::apply(calibration::default_profile(connection.identity.family), raw, sample);
                auto& seen = observations[connection.id];
                seen.kind = connection.identity.family;
                seen.link = connection.link;
                ++seen.samples;
                seen.seenButtons |= sample.buttons.value();
                const aim::deadzone_params deadzone{{.2f}, {.01f}, {0.0f}};
                const auto left = aim::apply(deadzone, sample.sticks[0].calibrated);
                const auto right = aim::apply(deadzone, sample.sticks[1].calibrated);
                seen.peakLeft = std::max(seen.peakLeft, left.magnitude());
                seen.peakRight = std::max(seen.peakRight, right.magnitude());
                seen.peakLT = std::max(seen.peakLT, sample.triggers[0].normalized);
                seen.peakRT = std::max(seen.peakRT, sample.triggers[1].normalized);
                const bool buttonChange = sample.buttons != seen.last.buttons;
                const bool analogChange = std::abs(sample.sticks[0].calibrated.x - seen.last.sticks[0].calibrated.x) > .025f ||
                    std::abs(sample.sticks[0].calibrated.y - seen.last.sticks[0].calibrated.y) > .025f ||
                    std::abs(sample.sticks[1].calibrated.x - seen.last.sticks[1].calibrated.x) > .025f ||
                    std::abs(sample.sticks[1].calibrated.y - seen.last.sticks[1].calibrated.y) > .025f ||
                    std::abs(sample.triggers[0].normalized - seen.last.triggers[0].normalized) > .025f ||
                    std::abs(sample.triggers[1].normalized - seen.last.triggers[1].normalized) > .025f;
                if (seen.samples == 1 || buttonChange || (analogChange && now - seen.printed >= std::chrono::milliseconds(100)) ||
                    now - seen.printed >= std::chrono::seconds(2))
                {
                    std::ostringstream out;
                    out << std::fixed << std::setprecision(3) << "SAMPLE id=" << connection.id
                        << " count=" << seen.samples << " left=" << left.x << ',' << left.y
                        << " right=" << right.x << ',' << right.y
                        << " LT=" << sample.triggers[0].normalized << " RT=" << sample.triggers[1].normalized
                        << " buttons=" << Buttons(sample.buttons);
                    Print(out.str());
                    seen.printed = now;
                    seen.last = sample;
                }
                if (rumble && !pulseSent)
                {
                    pulseSent = true;
                    rumbling = connection.id;
                    rumbleUntil = now + std::chrono::milliseconds(250);
                    drivers.submit(rumbling, driver::rumble_request{.15f, .25f});
                    Print("RUMBLE 250ms pulse submitted; confirm physical vibration separately.");
                }
            });
            if (rumbling && now >= rumbleUntil)
            {
                drivers.submit(rumbling, driver::rumble_request{});
                rumbling = {};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        drivers.for_each([&](driver::driver& device, const device_connection&)
        {
            device.submit(driver::rumble_request{});
        });
        for (const auto& [id, seen] : observations)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(3) << "RESULT id=" << id << " family=" << seen.kind
                << " link=" << seen.link << " decoded_samples=" << seen.samples
                << " peak_left=" << seen.peakLeft << " peak_right=" << seen.peakRight
                << " peak_LT=" << seen.peakLT << " peak_RT=" << seen.peakRT
                << " button_mask=0x" << std::hex << seen.seenButtons;
            Print(out.str());
        }
        Print(observations.empty() ? "No controller reports decoded." : "Controller reports decoded successfully.");
        return observations.empty() ? 2 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Controller probe failed: " << error.what() << '\n';
        return 1;
    }
}
