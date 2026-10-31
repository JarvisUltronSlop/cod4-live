#include <live/bot_policy.h>
#include <cassert>
#include <iostream>

int main()
{
    using namespace live::bots;
    assert(CountLimit(0, 0) == 0);
    assert(CountLimit(1, 0) == 0);
    assert(CountLimit(18, 0) == 17);
    assert(CountLimit(18, 1) == 17);
    assert(CountLimit(18, 3) == 15);
    assert(CountLimit(18, 18) == 0);
    assert(CountLimit(100, 0) == 63);
    for (int capacity = 1; capacity <= 64; ++capacity)
        for (int humans = 0; humans <= capacity; ++humans)
        {
            const int bots = CountLimit(capacity, humans);
            assert(bots >= 0 && bots <= capacity - 1);
            assert(bots + humans <= capacity);
        }
    assert(SafeDataPath("scriptdata/waypoints/mp_crash_wp.csv"));
    assert(!SafeDataPath("scriptdata/../players/config.cfg"));
    assert(!SafeDataPath("scriptdata/C:/config.cfg"));
    assert(!SafeDataPath("scriptdata/..\\config.cfg"));
    assert(!SafeDataPath("players/config.cfg"));
    assert(!SafeDataPath("/scriptdata/waypoints.csv"));
    std::cout << "Bot capacity and data path checks passed\n";
}
