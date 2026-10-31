#pragma once
#include <algorithm>

namespace live {
inline constexpr int MaxClientCapacity = 64;
constexpr int CapacityForMenu(bool hosting, int current, int pending) {
    return std::clamp(hosting ? current : pending, 1, MaxClientCapacity);
}
constexpr int CapacityFromMenuIndex(int index) {
    return std::clamp(index, 0, MaxClientCapacity - 1) + 1;
}
}
