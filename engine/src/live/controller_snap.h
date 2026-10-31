#pragma once
#include <algorithm>
#include <cmath>
#include <span>

namespace live::controller {
// CoD4 PC retained the auto-aim data but not a controller auto-aim updater.
// This fixed, bounded ADS-press addition uses those native regions/ranges;
// it is not a claim of byte-for-byte console auto-aim behavior.
inline constexpr float SnapConvergence = 40.0f;
inline constexpr float SnapDuration = 0.2f;
inline constexpr float SnapTolerance = 0.05f;
inline constexpr float SnapVisibilityThreshold = 0.2f;
inline bool SnapVisibilityEligible(bool unobstructed,float visibility) {
    return unobstructed&&std::isfinite(visibility)&&visibility>=SnapVisibilityThreshold;
}
inline float SnapRange(float autoAimRange,float adsAssistRange) {
    // Retail PC infantry weapons have autoAimRange=0. Their ADS assist range
    // is the explicit fallback, preserving each weapon's existing reach.
    return std::isfinite(autoAimRange)&&autoAimRange>0 ? autoAimRange :
        std::isfinite(adsAssistRange)&&adsAssistRange>0 ? adsAssistRange : 0;
}
struct SnapTarget {
    int entity{-1};
    bool enemy{}, visible{};
    float distanceSquared{}, minX{}, minY{}, maxX{}, maxY{}, pitch{}, yaw{};
};
inline bool SnapEligible(const SnapTarget& target,float range,float width,float height) {
    return target.entity>=0&&target.enemy&&target.visible&&std::isfinite(range)&&range>0&&
        std::isfinite(target.distanceSquared)&&target.distanceSquared>0&&target.distanceSquared<=range*range&&
        std::isfinite(target.pitch)&&std::isfinite(target.yaw)&&
        width>=target.minX&&target.maxX>=-width&&height>=target.minY&&target.maxY>=-height;
}
inline float SnapAngleDelta(float target,float current) {
    return std::remainder(target-current,360.0f);
}
inline float SnapCommandWorldAngle(float command,float playerDelta,float kickAndOffset) {
    // CL_FinishMove adds cgameKickAngles; PM_UpdateViewAngles then adds the
    // player's spawn/view delta. Snap targets are already in that world basis.
    return command+playerDelta+kickAndOffset;
}
struct SnapDelta { float pitch{},yaw{}; bool applied{}; int entity{-1}; };
class AdsSnap {
    bool held_{true},active_{};
    int weapon_{-1},target_{-1};
    float elapsed_{};
    void Cancel() { active_=false; target_=-1; elapsed_=0; }
public:
    // After a blocked command, menu, blur or disconnect, held ADS must first
    // be released. Resetting to an unpressed edge would grant a delayed snap.
    void Reset() { Cancel(); held_=true; weapon_=-1; }
    bool WantsTargets(bool ads) const { return ads&&(!held_||active_); }
    bool Active() const { return active_; }
    int Target() const { return target_; }
    SnapDelta Update(bool ads,int weapon,bool allowed,std::span<const SnapTarget> targets,
        float range,float width,float height,float pitch,float yaw,float dt) {
        const bool rising=ads&&!held_;
        held_=ads;
        if(weapon_!=-1&&weapon_!=weapon) { weapon_=weapon; Cancel(); return {}; }
        weapon_=weapon;
        if(!ads||!allowed||!std::isfinite(dt)||dt<=0||!std::isfinite(pitch)||!std::isfinite(yaw)) {
            Cancel(); return {};
        }
        if(rising) {
            Cancel();
            // Native screenTargets are already sorted by crosshair proximity.
            for(const auto& candidate:targets) if(SnapEligible(candidate,range,width,height)) {
                target_=candidate.entity; active_=true; break;
            }
        }
        if(!active_) return {};
        const SnapTarget* selected=nullptr;
        for(const auto& candidate:targets)
            if(candidate.entity==target_&&SnapEligible(candidate,range,width,height)) { selected=&candidate; break; }
        if(!selected) { Cancel(); return {}; }
        const float step=std::min(dt,SnapDuration-elapsed_);
        const float fraction=1.0f-std::exp(-SnapConvergence*step);
        const float pitchError=SnapAngleDelta(selected->pitch,pitch);
        const float yawError=SnapAngleDelta(selected->yaw,yaw);
        SnapDelta result{pitchError*fraction,yawError*fraction,true,target_};
        elapsed_+=step;
        if(elapsed_>=SnapDuration||
            (std::abs(pitchError-result.pitch)<=SnapTolerance&&std::abs(yawError-result.yaw)<=SnapTolerance)) Cancel();
        return result;
    }
};
}
