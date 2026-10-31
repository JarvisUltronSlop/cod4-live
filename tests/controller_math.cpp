#include <live/iw4x/Aim/Assist.hpp>
#include <live/iw4x/Mapping/StickLayout.hpp>
#include <live/controller_policy.h>
#include <live/controller_snap.h>
#include <live/controller_glyph.h>
#include <live/input_policy.h>
#include <live/iw4x/Mapping/Binding.hpp>
#include <live/iw4x/Mapping/Glyph.hpp>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void Check(bool success, const char* message)
{
    if (!success) { std::cerr << message << '\n'; std::exit(1); }
}
float Simulate(unsigned frames)
{
    using namespace Controller;
    using namespace Controller::aim;
    aim_processor::config config;
    config.hip.yaw_rate = {260}; config.hip.pitch_rate = {90};
    config.ads.yaw_rate = {90}; config.ads.pitch_rate = {55};
    // With acceleration disabled integration must be framerate independent.
    config.accel = {{0}, {0}};
    aim_processor processor(config);
    aim_frame_input input;
    input.look = {1, 0}; input.dt = seconds{1.0f / frames};
    float total = 0;
    for (unsigned i = 0; i < frames; ++i) total += processor.process(input).yaw_delta.value;
    return total;
}
}
int main()
{
    using namespace Controller;
    using namespace Controller::aim;
    const deadzone_params deadzone{{.2f}, {.01f}, {0}};
    const auto drift = apply(deadzone, {.1f, .1f});
    Check(drift.x == 0 && drift.y == 0, "drift must not become activity");
    const auto full = apply(deadzone, {1, 0});
    Check(std::abs(full.x - 1) < .00001f, "full stick reaches full range");
    const auto diagonal = apply(deadzone, {.70710678f, .70710678f});
    Check(std::abs(std::hypot(diagonal.x, diagonal.y) - 1) < .00001f, "diagonal stays radial");
    Check(slowdown_scale(false, .4f, .5f, 0) == 1, "no target means no slowdown");
    Check(std::abs(slowdown_scale(true, .4f, .5f, .5f) - .45f) < .00001f, "ADS slowdown interpolation");
    for (unsigned rate : {30u, 60u, 144u, 250u, 1000u})
        Check(std::abs(Simulate(rate) - 260) < .01f, "turn integration changes with framerate");
    lock_on_target target;
    target.distance = 100; target.target_velocity = {10, 0, 0}; target.view_yaw_axis = {1, 0, 0};
    const auto delta = lock_on(target, {.6f, .6f}, seconds{.016f});
    Check(delta.yaw_delta.value > 0, "relative target motion rotates toward target");
    target.player_velocity = target.target_velocity;
    Check(lock_on(target, {.6f, .6f}, seconds{.016f}).yaw_delta.value == 0,
        "identical velocities need no rotation");
    target.distance = 0;
    Check(lock_on(target, {.6f, .6f}, seconds{.016f}).yaw_delta.value == 0,
        "zero distance must not divide by zero");
    const auto southpaw = mapping::resolve(mapping::stick_layout::southpaw, {.7f, .2f}, {.3f, .8f});
    Check(std::abs(southpaw.forward - .8f * std::hypot(.3f, .8f)) < .00001f &&
        std::abs(southpaw.side - .3f * std::hypot(.3f, .8f)) < .00001f && southpaw.yaw == .7f,
        "southpaw swaps the sticks");
    aim_processor::config turn;
    turn.hip.pitch_rate = {90}; turn.accel = {{0}, {0}};
    aim_processor normal(turn), inverted(turn);
    aim_frame_input look; look.look = {0, 1}; look.dt = seconds{.016f};
    const auto up = normal.process(look).pitch_delta.value;
    look.invert_pitch = true;
    Check(up > 0 && std::abs(inverted.process(look).pitch_delta.value + up) < .00001f,
        "inverted pitch must reverse direction exactly once");
    using namespace live::controller;
    const auto blurred = Availability(false, true, true, 100, 100);
    Check(blurred.present && !blurred.dispatchInput,
        "background polling must report a connected controller without dispatching input");
    const auto unplugged = Availability(false, false, false, 100, 101);
    Check(!unplugged.present && !unplugged.dispatchInput,
        "unplug while alt-tabbed must immediately clear presence");
    Check(Availability(false, true, false, 100, 349).present && !Availability(false, true, false, 100, 350).present,
        "stale controller samples must expire while alt-tabbed");
    Check(Availability(true, true, true, 400, 400).dispatchInput,
        "focused fresh controller input must remain available");
    live::InputPolicy backgroundPolicy;
    Check(backgroundPolicy.Update(100, blurred.present ? live::ControllerConnected : 0) == live::InputAction::None,
        "focus loss alone must not start disconnect enforcement");
    Check(backgroundPolicy.Update(101, unplugged.present ? live::ControllerConnected : 0) == live::InputAction::WarnDisconnected,
        "background unplug must start the server disconnect grace");
    Check(backgroundPolicy.Update(30101, 0) == live::InputAction::KickDisconnected,
        "background unplug must not bypass the thirty-second server grace");
    Check(WeaponAllowsAssist(false, 1), "free-fire weapons, including RPGs, retain weapon-defined aim assist");
    Check(!WeaponAllowsAssist(true, 1), "lock-on launchers must not receive controller target assist");
    Check(!WeaponAllowsAssist(false, 0) && !WeaponAllowsAssist(true, 0), "empty weapons must not receive slowdown");
    Check(UsingOffhand(UsingOffhandFlag, true), "equipped offhand weapon must use offhand assist rules");
    Check(!UsingOffhand(0, true) && !UsingOffhand(UsingOffhandFlag, false),
        "weapon state or offhand flag alone must not identify an offhand weapon");
    Check(PreserveMenuFocus(true, true, 0, 0),
        "stationary mouse poll must not clear controller menu focus");
    Check(!PreserveMenuFocus(true, true, 1, 0) && !PreserveMenuFocus(true, true, 0, -1),
        "real mouse movement must return menu control to the mouse");
    Check(!PreserveMenuFocus(false, true, 0, 0) && !PreserveMenuFocus(true, false, 0, 0),
        "focus preservation only applies to controller menu navigation");
    for (const auto command : {"+attack", "-attack", "+SPEED", "+actionslot", "toggleads", "weapnext", "goprone"})
        Check(IsGameplayCommand(command), "console gameplay command escaped provenance gate");
    for (const auto command : {"+talk", "-scores", "messagemode", "exec", "bind", "controller_settings", ""})
        Check(!IsGameplayCommand(command), "ordinary console/UI command was classified as gameplay");
    ActionLatch tap;
    tap.Press();
    Check(!tap.RequestRelease(), "tap released before a command was built");
    Check(!tap.ReleaseReady(), "elapsed time alone cannot acknowledge a tap");
    tap.CommandBuilt();
    Check(tap.ReleaseReady(), "tap must release after appearing in a command");
    tap.Clear();
    Check(!tap.active && !tap.ReleaseReady(), "disconnect/menu must clear a pending tap");
    tap.Press(); tap.CommandBuilt();
    Check(tap.RequestRelease(), "an observed held button must release immediately");
    Check(MenuKeyForButton(1)==K_ENTER && MenuKeyForButton(14)==K_ENTER,
        "A and Start must select native menu items");
    Check(MenuKeyForButton(2)==K_ESCAPE && MenuKeyForButton(15)==K_ESCAPE,
        "B and Back must cancel native menus");
    Check(MenuKeyForButton(4)==K_F1 && MenuKeyForButton(3)==K_F2,
        "Y and X must retain native F1/F2 menu actions");
    Check(SnapRange(0,1500)==1500&&SnapRange(600,1500)==600&&SnapRange(0,0)==0,
        "PC zero autoAimRange must use only its own weapon ADS range");
    Check(SnapVisibilityEligible(true,.2f)&&SnapVisibilityEligible(true,1),
        "snap should accept a clear trace at the crosshair visibility threshold");
    Check(!SnapVisibilityEligible(true,.1999f)&&!SnapVisibilityEligible(true,0)&&
        !SnapVisibilityEligible(false,1)&&!SnapVisibilityEligible(true,std::nanf("")),
        "snap must reject blocked geometry, near-opaque smoke and non-finite visibility");
    SnapTarget snapTarget{7,true,true,10000,-.01f,-.02f,.01f,.02f,10,20};
    Check(SnapEligible(snapTarget,100,.1f,.1f)&&!SnapEligible(snapTarget,99,.1f,.1f),
        "snap range must compare world distance squared at the exact weapon boundary");
    auto rejected=snapTarget; rejected.enemy=false;
    Check(!SnapEligible(rejected,1000,.1f,.1f),"snap must not select a teammate");
    rejected=snapTarget; rejected.visible=false;
    Check(!SnapEligible(rejected,1000,.1f,.1f),"snap must not select an occluded or smoke-obscured aim point");
    rejected=snapTarget; rejected.minX=.11f; rejected.maxX=.12f;
    Check(!SnapEligible(rejected,1000,.1f,.1f),"snap must not select a target outside the native region");
    AdsSnap snap;
    auto step=[&](bool ads,std::span<const SnapTarget> targets,int weapon=1,bool allowed=true) {
        return snap.Update(ads,weapon,allowed,targets,1000,.1f,.1f,0,0,.016f);
    };
    const std::array<SnapTarget,1> initial{snapTarget};
    Check(!step(true,initial).applied,"reset must suppress an already-held ADS button");
    step(false,initial);
    const auto firstSnap=step(true,initial);
    Check(firstSnap.applied&&firstSnap.entity==7&&firstSnap.pitch>0&&firstSnap.yaw>0,
        "ADS rising edge must converge toward the visible enemy");
    snapTarget.entity=8;
    const std::array<SnapTarget,1> replacement{snapTarget};
    Check(!step(true,replacement).applied&&!step(true,replacement).applied,
        "losing the chosen target must not retarget while ADS remains held");
    step(false,replacement);
    Check(step(true,replacement).entity==8,"release and repress must allow a new target");
    rejected=replacement[0]; rejected.visible=false;
    const std::array<SnapTarget,1> hidden{rejected};
    Check(!step(true,hidden).applied&&!snap.Active(),"occlusion during convergence must cancel immediately");
    Check(!step(true,replacement).applied,"visibility recovery must not create another held-ADS snap");
    step(false,replacement); step(true,replacement);
    snap.Reset();
    Check(!step(true,replacement).applied&&!step(true,replacement).applied,
        "KBM block, menu, blur, frozen state or disconnect reset must require ADS release");
    step(false,replacement); step(true,replacement);
    Check(!step(true,replacement,2).applied&&!step(true,replacement,2).applied,
        "weapon change while ADS is held must cancel without creating a fresh snap");
    step(false,replacement,2);
    Check(!step(true,replacement,2,false).applied&&!step(true,replacement,2,true).applied,
        "becoming eligible while ADS remains held must not acquire a target");
    step(false,replacement,2); step(true,replacement,2);
    for(int i=0;i<20;++i) step(true,replacement,2);
    Check(!snap.Active()&&!step(true,replacement,2).applied,"snap must end within its fixed duration");
    snap.Reset(); step(false,{});
    Check(!step(true,{}).applied&&!step(true,initial).applied,
        "an ADS edge with no eligible target must be consumed");
    for(int frames:{3,6,12,25}) {
        AdsSnap timed; float pitch=0,yaw=0;
        auto moving=snapTarget; moving.pitch=40; moving.yaw=-179;
        const std::array<SnapTarget,1> target{moving};
        timed.Update(false,1,true,target,1000,.1f,.1f,0,179,.001f);
        yaw=179;
        for(int i=0;i<frames;++i) {
            const auto delta=timed.Update(true,1,true,target,1000,.1f,.1f,pitch,yaw,.1f/frames);
            pitch+=delta.pitch; yaw+=delta.yaw;
        }
        Check(std::abs(pitch-40*(1-std::exp(-4.f)))<.001f,
            "snap convergence must remain stable across frame rates");
        Check(yaw>179&&yaw<181.01f,"snap must take the short path across the yaw wrap");
    }
    const auto wrapper=NativeMenuLinks("main",true,false,false,false);
    const auto foreground=NativeMenuLinks("main_text",false,false,false,false);
    Check(!wrapper.controller&&!wrapper.bots&&foreground.controller&&foreground.bots,
        "CoD4 entries must belong to foreground main_text instead of its hidden wrapper");
    for(const auto name:{"main_controls","MAIN_OPTIONS"}) {
        const auto links=NativeMenuLinks(name,false,false,false,false);
        Check(links.controller&&!links.bots,"stock Controls and Options must expose controller settings");
    }
    Check(NativeMenuLinks("main",false,false,false,false).controller,
        "a mod menu without a main_text wrapper must retain its entry");
    using mapping::engine_key;
    for (const char* name : {"buttons_default","buttons_tactical","buttons_lefty","buttons_nomad"}) {
        mapping::binding_table normalLayout, flippedLayout;
        mapping::apply_button_layout(normalLayout,name);
        mapping::apply_button_layout(flippedLayout,std::string(name)+"_alt");
        Check(normalLayout.size()==16 && flippedLayout.size()==16,"all eight profiles must bind sixteen buttons");
        Check(*normalLayout.command_for(engine_key::button_rtrig)==*flippedLayout.command_for(engine_key::button_rshldr),
            "flipped profile must exchange trigger and shoulder actions");
        Check(*normalLayout.command_for(engine_key::button_ltrig)==*flippedLayout.command_for(engine_key::button_lshldr),
            "flipped left trigger must preserve its action");
    }
    unsigned glyphCount=0;
    for (auto key : mapping::keys()) for(auto family : {mapping::glyph_family::xbox,mapping::glyph_family::playstation}) {
        const char* source=mapping::glyph_for(key,family);
        if(!source) continue;
        ++glyphCount;
        std::string encoded(source); encoded[1]=3;
        Check(live::controller_glyph::Named(encoded.c_str()+1),"named glyph must parse its complete material token");
        Check(live::controller_glyph::Bytes(encoded.c_str()+1)+1==encoded.size(),"glyph length must consume exactly one inline token");
        encoded.pop_back();
        Check(!live::controller_glyph::Named(encoded.c_str()+1),"truncated material names must be rejected");
    }
    Check(glyphCount==32,"both families must provide all sixteen button glyphs");
    std::cout << "controller math/policy tests passed\n";
}
