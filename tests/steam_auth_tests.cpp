#include <live/steam_auth_policy.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace cod4live::steam_auth;
static void check(bool value, const char* label) {
    if (!value) { std::cerr << label << '\n'; std::exit(1); }
}
int main() {
    std::uint64_t id;
    check(parse_id("76561198000000001", id) && id == 76561198000000001ULL, "valid Steam ID");
    for (const char* invalid : {"", "0", "01", "-1", "+1", " 1", "1x", "18446744073709551616", "123456789012345678901"})
        check(!parse_id(invalid, id), "malformed ID must fail");
    std::array<unsigned char, 1024> output{};
    std::size_t size;
    check(decode_ticket("AAEC", output.data(), output.size(), size) && size == 3 && output[0] == 0 && output[2] == 2,
        "binary ticket may start with zero");
    check(decode_ticket("TQ==", output.data(), output.size(), size) && size == 1 && output[0] == 'M', "one byte");
    check(decode_ticket("TWE=", output.data(), output.size(), size) && size == 2, "two bytes");
    for (const char* invalid : {"", "A", "AAA", "====", "A===", "AA=A", "AB==", "AAB=", "AA==AAAA", "AA A", "AA-_"})
        check(!decode_ticket(invalid, output.data(), output.size(), size), "malformed base64 must fail");
    const std::string full_ticket = std::string(1364, 'A') + "AA==";
    check(decode_ticket(full_ticket, output.data(), output.size(), size) && size == 1024, "full 1024-byte ticket fits");
    check(!decode_ticket(std::string(1368, 'A'), output.data(), output.size(), size), "decoded overflow rejected");
    check(!decode_ticket("AAAA", output.data(), 2, size), "small destination rejected");
    Verification good{State::pending, 100};
    check(good.state != State::verified, "synchronous acceptance is pending");
    check(good.complete(true, 42, 42, 200), "direct owner verifies");
    check(!good.complete(false, 42, 42, 210) && good.state == State::rejected, "revocation fails closed");
    check(!good.complete(true, 42, 42, 220), "late success cannot resurrect rejected session");
    Verification shared{State::pending, 100};
    check(!shared.complete(true, 42, 43, 200), "family sharing rejected");
    Verification pending{State::pending, 100};
    check(!pending.expire(30099), "still pending before 30 seconds");
    check(pending.expire(30100), "expires at exactly 30 seconds");
    Verification late{State::pending, 100};
    check(!late.complete(true, 42, 42, 30100), "late callback cannot admit timed out client");
    Verification active{State::pending, 100};
    check(active.complete(true, 42, 42, 200) && !active.expire(50000), "verified session survives map-time duration");
    std::cout << "Steam ownership policy and bounded parser tests passed\n";
}
