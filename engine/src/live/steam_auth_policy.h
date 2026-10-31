#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace cod4live::steam_auth {
constexpr std::uint64_t verification_timeout_ms = 30000;
constexpr std::size_t max_ticket_bytes = 1024;

inline bool parse_id(std::string_view text, std::uint64_t& id) {
    id = 0;
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0')) return false;
    for (char c : text) {
        if (c < '0' || c > '9' || id > ((std::numeric_limits<std::uint64_t>::max)() - (c - '0')) / 10)
            return false;
        id = id * 10 + (c - '0');
    }
    return id != 0;
}

// Reject malformed/padded/noncanonical input before any Steam API call.
inline bool decode_ticket(std::string_view text, unsigned char* out, std::size_t capacity,
                          std::size_t& size) {
    size = 0;
    if (text.empty() || text.size() % 4 || text.size() > ((capacity + 2) / 3) * 4) return false;
    auto digit = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const int a = digit(text[i]), b = digit(text[i + 1]);
        const bool p2 = text[i + 2] == '=', p3 = text[i + 3] == '=';
        const int c = p2 ? 0 : digit(text[i + 2]), d = p3 ? 0 : digit(text[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (p2 && !p3) ||
            ((p2 || p3) && i + 4 != text.size()) || (p2 && (b & 15)) || (!p2 && p3 && (c & 3)))
            return false;
        const std::size_t count = p2 ? 1 : p3 ? 2 : 3;
        if (size + count > capacity) return false;
        out[size++] = static_cast<unsigned char>((a << 2) | (b >> 4));
        if (count > 1) out[size++] = static_cast<unsigned char>((b << 4) | (c >> 2));
        if (count > 2) out[size++] = static_cast<unsigned char>((c << 6) | d);
    }
    return size != 0;
}

enum class State { pending, verified, rejected };
struct Verification {
    State state = State::pending;
    std::uint64_t started = 0;
    bool complete(bool ok, std::uint64_t user, std::uint64_t owner, std::uint64_t now) {
        if (state == State::rejected || (state == State::pending && now - started >= verification_timeout_ms)) {
            state = State::rejected;
            return false;
        }
        state = ok && user != 0 && user == owner ? State::verified : State::rejected;
        return state == State::verified;
    }
    bool expire(std::uint64_t now) {
        if (state == State::pending && now - started >= verification_timeout_ms) state = State::rejected;
        return state == State::rejected;
    }
};
}
