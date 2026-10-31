// cod4-live adapter for IW4x controller math, GPL-3.0.
// Target discovery, weapon data, FOV and melee stay in CoD4's own pipeline.
#include "iw4x/Aim/Assist.hpp"
#include "controller.h"
#include "controller_policy.h"
#include "controller_snap.h"
#include <universal/q_shared.h>
#include <aim_assist/aim_assist.h>
#include <bgame/bg_local.h>
#include <cgame_mp/cg_local_mp.h>
#include <script/scr_const.h>
#include <EffectsCore/fx_system.h>

extern AimAssistGlobals aaGlobArray[1];
extern GraphFloat aaInputGraph[4];

namespace live::controller
{
namespace
{
using namespace Controller::aim;
std::optional<aim_graph> graph;
std::optional<aim_processor> processor;
std::size_t previousSignature{};
AdsSnap adsSnap;

void EnsureProcessor()
{
    // IW4x likewise reads graph3 from its host engine. Preserve CoD4's owned
    // view_input_3.graph knots; the portable evaluator applies them identically.
    const auto& native = aaInputGraph[3];
    std::size_t signature = std::hash<float>{}(Sensitivity());
    for (int i = 0; i < native.knotCount && i < 32; ++i)
    {
        signature = signature * 31 + std::hash<float>{}(native.knots[i][0]);
        signature = signature * 31 + std::hash<float>{}(native.knots[i][1] * native.scale);
    }
    if (processor && signature == previousSignature) return;
    previousSignature = signature;
    std::array<knot, 32> knots{};
    const int count = std::clamp(native.knotCount, 0, 32);
    for (int i = 0; i < count; ++i)
        knots[i] = {native.knots[i][0], native.knots[i][1] * native.scale};
    std::string why;
    graph = aim_graph::make(std::span<const knot>(knots.data(), count), false, why);
    aim_processor::config config;
    config.hip.yaw_rate = {260.0f};
    config.hip.pitch_rate = {90.0f};
    config.ads.yaw_rate = {90.0f};
    config.ads.pitch_rate = {55.0f};
    config.accel = {{1200.0f * Sensitivity()}, {0.0f}};
    config.graph = graph ? &*graph : nullptr;
    processor.emplace(config);
}

const AimScreenTarget* Target(const AimAssistGlobals& aa, float range, bool lock, int previous)
{
    const float width = lock ? aa.tweakables.lockOnRegionWidth : aa.tweakables.slowdownRegionWidth;
    const float height = lock ? aa.tweakables.lockOnRegionHeight : aa.tweakables.slowdownRegionHeight;
    const AimScreenTarget* first = nullptr;
    for (int i = 0; i < aa.screenTargetCount; ++i)
    {
        const auto& target = aa.screenTargets[i];
        if (target.distSqr <= 0 || target.distSqr > range * range ||
            width < target.clipMins[0] || target.clipMaxs[0] < -width ||
            height < target.clipMins[1] || target.clipMaxs[1] < -height) continue;
        if (!first) first = &target;
        if (lock && target.entIndex == previous) return &target;
    }
    return first;
}

bool SnapPointVisible(const AimInput& input,const AimScreenTarget& target,const float* aimPos)
{
    const auto* cg=CG_GetLocalClientGlobals(input.localClientNum);
    const auto* entity=CG_GetEntity(input.localClientNum,target.entIndex);
    if(!entity->nextValid||entity->nextState.eType!=ET_PLAYER||
        (entity->nextState.lerp.eFlags&EF_DEAD)||!AimTarget_IsTargetValid(cg,entity)) return false;
    trace_t trace{};
    CG_TraceCapsule(&trace,aaGlobArray[input.localClientNum].viewOrigin,vec3_origin,vec3_origin,
        aimPos,input.ps->clientNum,MASK_AIMTARGET_VISIBILITY);
    const bool clear=trace.fraction==1.0f||Trace_GetEntityHitId(&trace)==target.entIndex;
    if(!clear) return false;
    return SnapVisibilityEligible(clear,
        FX_GetClientVisibility(input.localClientNum,aaGlobArray[input.localClientNum].viewOrigin,aimPos));
}

SnapDelta ApplyAdsSnap(const AimInput& input,const AimOutput& output,AimAssistGlobals& aa,const WeaponDef* weapon,bool allowed,unsigned weaponIndex)
{
    const bool ads=(input.buttons&BUTTON_ADS)!=0;
    const float range=weapon?SnapRange(weapon->autoAimRange,weapon->aimAssistRangeAds):0;
    std::array<SnapTarget,64> candidates{};
    std::size_t count=0;
    if(allowed&&adsSnap.WantsTargets(ads)) {
        for(int i=0;i<aa.screenTargetCount&&i<64;++i) {
            const auto& target=aa.screenTargets[i];
            // Only the chosen target is revisited during the short convergence.
            if(adsSnap.Active()&&target.entIndex!=adsSnap.Target()) continue;
            const auto* entity=CG_GetEntity(input.localClientNum,target.entIndex);
            if(!entity->nextValid||entity->nextState.eType!=ET_PLAYER||(entity->nextState.lerp.eFlags&EF_DEAD)) continue;
            float offset[3]{};
            Vec3Sub(entity->pose.origin,input.ps->origin,offset);
            SnapTarget candidate{target.entIndex,true,true,target.distSqr,
                target.clipMins[0],target.clipMins[1],target.clipMaxs[0],target.clipMaxs[1]};
            candidate.distanceSquared=Vec3LengthSq(offset);
            if(!SnapEligible(candidate,range,aa.tweakables.autoAimRegionWidth,aa.tweakables.autoAimRegionHeight)) continue;
            float aimPos[3]{},direction[3]{},angles[3]{};
            if(!AimTarget_GetTagPos(input.localClientNum,entity,scr_const.aim_bone,aimPos)||
                !SnapPointVisible(input,target,aimPos)) continue;
            Vec3Sub(aimPos,aa.viewOrigin,direction);
            vectoangles(direction,angles);
            candidate.pitch=angles[0]; candidate.yaw=angles[1];
            candidates[count++]=candidate;
            // Native list order is the selection order; one fresh visibility
            // trace on the first eligible enemy is enough to choose the press.
            if(!adsSnap.Active()) break;
        }
    }
    const auto result=adsSnap.Update(ads,static_cast<int>(weaponIndex),allowed,
        std::span<const SnapTarget>(candidates.data(),count),range,
        aa.tweakables.autoAimRegionWidth,aa.tweakables.autoAimRegionHeight,
        // Correct the command being built, including this frame's stick input.
        // Render-derived aa.viewAngles may lag and would repeat old corrections.
        output.pitch,output.yaw,input.deltaTime);
    aa.autoAimPressed=ads;
    aa.autoAimActive=adsSnap.Active();
    aa.autoAimTargetEnt=adsSnap.Active()?adsSnap.Target():ENTITYNUM_NONE;
    return result;
}
}

void ResetAim()
{
    if (processor) processor->reset();
    adsSnap.Reset();
    aaGlobArray[0].autoAimActive=false;
    aaGlobArray[0].autoAimPressed=true;
    aaGlobArray[0].autoAimTargetEnt=ENTITYNUM_NONE;
    aaGlobArray[0].lockOnTargetEnt = ENTITYNUM_NONE;
}

void UpdateAim(const AimInput& input, AimOutput& output)
{
    output = {};
    output.pitch = input.pitch;
    output.yaw = input.yaw;
    if (!input.ps || input.deltaTime <= 0 || !std::isfinite(input.deltaTime)) return;
    EnsureProcessor();
    auto& aa = aaGlobArray[input.localClientNum];
    const auto& ps = *input.ps;
    if (aa.initialized)
    {
        AimAssist_UpdateTweakables(&input);
        AimAssist_UpdateAdsLerp(&input);
    }
    const unsigned weapon = aa.initialized ? AimAssist_GetWeaponIndex(input.localClientNum, input.ps) : 0;
    const auto* definition = weapon ? BG_GetWeaponDef(weapon) : nullptr;
    static_assert(PWF_USING_OFFHAND == UsingOffhandFlag);
    const bool offhand = UsingOffhand(ps.weapFlags, definition && definition->offhandClass != OFFHAND_CLASS_NONE);
    const bool allowed = aa.initialized && AimTarget_PlayerInValidState(&ps) &&
        !(ps.pm_flags & PMF_FROZEN) && !(ps.eFlags & 0x300) && definition &&
        WeaponAllowsAssist(definition->requireLockonToFire != 0, BG_WeaponAmmo(&ps, weapon));
    const float range = definition ? std::lerp(definition->aimAssistRange,
        definition->aimAssistRangeAds, std::clamp(aa.adsLerp, 0.0f, 1.0f)) : 0.0f;

    aim_frame_input frame;
    frame.look = aa.initialized && aa.autoMeleeActive ? Controller::stick_vector{} :
        Controller::stick_vector{-input.yawAxis, -input.pitchAxis};
    frame.ads_lerp = aa.initialized ? aa.adsLerp : 0.0f;
    frame.fov_scale = aa.initialized ? aa.fovTurnRateScale : 1.0f;
    frame.sensitivity = Sensitivity() * std::lerp(1.0f, AdsSensitivity(), frame.ads_lerp);
    frame.scale_view_axis = true;
    frame.invert_pitch = InvertPitch();
    frame.dt = Controller::seconds{std::min(input.deltaTime, 0.1f)};
    if (input.yawMax > 0) frame.yaw_max = deg_per_s{input.yawMax};
    if (input.pitchMax > 0) frame.pitch_max = deg_per_s{input.pitchMax};
    if (allowed)
    {
        const bool target = Target(aa, range, false, ENTITYNUM_NONE) != nullptr;
        frame.slowdown_yaw = slowdown_scale(target, 0.4f, 0.5f, aa.adsLerp);
        frame.slowdown_pitch = offhand ? 1.0f : frame.slowdown_yaw;
    }
    const auto delta = processor->process(frame);
    output.yaw -= delta.yaw_delta.value;
    output.pitch -= delta.pitch_delta.value;
    if (aa.initialized)
    {
        // The stock mouse path is not called for this command: melee runs once.
        AimAssist_ApplyAutoMelee(&input, &output);
        AimAssist_ApplyMeleeCharge(&input, &output);
        const auto snap=ApplyAdsSnap(input,output,aa,definition,
            allowed&&!offhand&&!aa.autoMeleeActive&&definition->aimDownSight!=0,weapon);
        output.pitch+=snap.pitch;
        output.yaw+=snap.yaw;
        const int previous = aa.lockOnTargetEnt;
        aa.lockOnTargetEnt = ENTITYNUM_NONE;
        const bool deflected = std::abs(input.pitchAxis) >= 0.05f ||
            std::abs(input.yawAxis) >= 0.05f || std::abs(input.rightAxis) >= 0.05f;
        const auto* target = allowed && !offhand && !aa.autoMeleeActive && !snap.applied && deflected ?
            Target(aa, range, true, previous) : nullptr;
        if (target)
        {
            aa.lockOnTargetEnt = target->entIndex;
            lock_on_target tracking;
            tracking.target_velocity = {target->velocity[0], target->velocity[1], target->velocity[2]};
            tracking.player_velocity = {ps.velocity[0], ps.velocity[1], ps.velocity[2]};
            tracking.view_yaw_axis = {-aa.viewAxis[1][0], -aa.viewAxis[1][1], -aa.viewAxis[1][2]};
            tracking.view_pitch_axis = {aa.viewAxis[2][0], aa.viewAxis[2][1], aa.viewAxis[2][2]};
            tracking.distance = std::sqrt(target->distSqr);
            const auto rotation = lock_on(tracking, {0.6f, 0.6f}, frame.dt);
            output.yaw -= rotation.yaw_delta.value;
            output.pitch -= rotation.pitch_delta.value;
        }
    }
}
}
