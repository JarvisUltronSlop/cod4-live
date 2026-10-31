#pragma once
#include <algorithm>
#include <string_view>

namespace live::bots
{
constexpr int CountLimit(int maxClients, int humans)
{
    return std::max(0, std::min(maxClients, 64) - std::max(humans, 1));
}
constexpr bool SafeDataPath(std::string_view path)
{
    return path.starts_with("scriptdata/") && path.size() < 240 &&
        path.find("..") == path.npos && path.find(':') == path.npos &&
        path.find('\\') == path.npos;
}
}
