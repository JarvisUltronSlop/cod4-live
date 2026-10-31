#include <qcommon/msg_mp.h>
#include <array>
#include <cstdlib>
#include <iostream>

void MyAssertHandler(const char* file, int line, int, const char* format, ...)
{
    std::cerr << "Engine assertion at " << file << ':' << line << '\n';
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::abort();
}
// Truncated packets also trigger engine diagnostics; the rejection assertion below
// checks the overflow flag, while suppressing thousands of expected log lines.
void Com_PrintError(int, const char*, ...) {}

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::uint32_t rng = 0xC0D40003;
std::uint32_t Random()
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}
msg_t Writer(std::array<std::uint8_t, 4096>& data)
{
    msg_t result{};
    result.data = data.data();
    result.maxsize = static_cast<int>(data.size());
    return result;
}
msg_t Reader(const msg_t& writer, int length)
{
    auto result = writer;
    result.readOnly = 1;
    result.cursize = length;
    result.readcount = result.bit = result.overflowed = 0;
    return result;
}
void Equal(const usercmd_s& expected, const usercmd_s& actual)
{
    Require(expected.serverTime == actual.serverTime, "timestamp round trip");
    Require(expected.buttons == actual.buttons, "buttons round trip");
    Require(expected.weapon == actual.weapon && expected.offHandIndex == actual.offHandIndex, "weapon round trip");
    for (int i = 0; i < 3; ++i) Require(expected.angles[i] == actual.angles[i], "angles round trip");
    Require(expected.forwardmove == actual.forwardmove && expected.rightmove == actual.rightmove, "analog movement round trip");
    Require(expected.inputFlags == actual.inputFlags, "input metadata round trip");
    if (expected.buttons & BUTTON_LOC_CONFIRM)
        Require(expected.selectedLocation[0] == actual.selectedLocation[0] && expected.selectedLocation[1] == actual.selectedLocation[1], "location selection round trip");
}
}

int main()
{
    static_assert(sizeof(usercmd_s) == 32 && offsetof(usercmd_s, inputFlags) == 31);
    std::array<std::uint8_t, 4096> bytes{};
    usercmd_s from{};
    from.weapon = 1;
    from.inputFlags = live::ControllerConnected;
    // Exercise unchanged, fast, full and metadata-only branches against actual engine routines.
    for (int iteration = 0; iteration < 10000; ++iteration)
    {
        auto to = from;
        to.serverTime += 1 + Random() % 700;
        to.inputFlags = live::ControllerConnected | ((Random() & 1) ? live::ControllerActive : 0)
            | ((Random() & 1) ? live::KeyboardMouseAttempt : 0);
        if (iteration % 4 >= 2)
        {
            to.forwardmove = static_cast<char>(Random() & 255);
            to.rightmove = static_cast<char>(Random() & 255);
            to.angles[0] = Random() & 65535;
            to.angles[1] = Random() & 65535;
            to.buttons ^= BUTTON_ATTACK;
        }
        if (iteration % 4 == 3)
        {
            to.angles[2] = Random() & 65535;
            to.buttons = (Random() & ((1 << BUTTON_BIT_COUNT) - 1)) & ~BUTTON_MELEE;
            to.weapon = Random() & 127;
            to.offHandIndex = Random() & 127;
            to.selectedLocation[0] = static_cast<char>(Random() & 255);
            to.selectedLocation[1] = static_cast<char>(Random() & 255);
        }
        const int key = static_cast<int>(Random());
        auto writer = Writer(bytes);
        MSG_WriteDeltaUsercmdKey(&writer, key, &from, &to);
        Require(!writer.overflowed, "valid command fits packet");
        auto reader = Reader(writer, writer.cursize);
        usercmd_s decoded{};
        MSG_ReadDeltaUsercmdKey(&reader, key, &from, &decoded);
        Require(!reader.overflowed, "valid command parses");
        Equal(to, decoded);
        // Every byte-truncated form of a complete command must fail closed.
        for (int length = 0; length < writer.cursize; ++length)
        {
            reader = Reader(writer, length);
            MSG_ReadDeltaUsercmdKey(&reader, key, &from, &decoded);
            Require(reader.overflowed, "truncated command must be rejected");
        }
        from = to;
    }
    // A packet contains a chain of command deltas, not independently aligned records.
    auto writer = Writer(bytes);
    std::array<usercmd_s, 32> commands{};
    usercmd_s baseline{};
    auto previous = baseline;
    for (auto& cmd : commands)
    {
        cmd = previous;
        cmd.serverTime += 50;
        cmd.forwardmove = static_cast<char>(Random() & 255);
        cmd.rightmove = static_cast<char>(Random() & 255);
        cmd.inputFlags = (Random() & 1) ? live::ControllerConnected : 0;
        MSG_WriteDeltaUsercmdKey(&writer, 7919, &previous, &cmd);
        previous = cmd;
    }
    Require(!writer.overflowed, "32 commands fit a packet");
    auto reader = Reader(writer, writer.cursize);
    previous = baseline;
    for (const auto& cmd : commands)
    {
        usercmd_s decoded{};
        MSG_ReadDeltaUsercmdKey(&reader, 7919, &previous, &decoded);
        Require(!reader.overflowed, "command chain parses");
        Equal(cmd, decoded);
        previous = decoded;
    }
    for (const auto flags : {128, static_cast<int>(live::ControllerActive)})
    {
        auto invalid = baseline;
        invalid.inputFlags = static_cast<std::uint8_t>(flags);
        writer = Writer(bytes);
        MSG_WriteDeltaUsercmdKey(&writer, 1, &baseline, &invalid);
        reader = Reader(writer, writer.cursize);
        usercmd_s decoded{};
        MSG_ReadDeltaUsercmdKey(&reader, 1, &baseline, &decoded);
        Require(reader.overflowed, "invalid provenance rejected");
    }
    std::cout << "PASS: production usercmd codec round trips, chained deltas, metadata-only updates and truncation\n";
}
