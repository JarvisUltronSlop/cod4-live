#pragma once
#include <cstdint>

namespace live::controller {
// The saved backend is unchanged during discovery. A failed/abandoned trial
// therefore cannot strand the next launch on an unavailable input provider.
struct BackendSwitch {
    enum class Result { None, Commit, Revert };
    bool pending{};
    int previous{},target{};
    std::uint32_t started{};
    bool Begin(int saved,int requested,std::uint32_t now) {
        if(pending||requested<0||requested>1||saved==requested) return false;
        previous=saved;target=requested;started=now;pending=true;return true;
    }
    int Effective(int saved) const { return pending?target:saved; }
    Result Observe(int saved,bool present,std::uint32_t now) {
        if(!pending) return Result::None;
        if(saved!=previous) { pending=false;return Result::Revert; }
        if(present) { pending=false;return Result::Commit; }
        if(now-started>=15000) { pending=false;return Result::Revert; }
        return Result::None;
    }
};
}
