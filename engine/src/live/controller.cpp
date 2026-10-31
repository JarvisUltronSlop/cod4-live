// cod4-live native engine adapter. IW4x portable code lives in iw4x/ (GPL-3.0).
#include "iw4x/Device/Discovery.hpp"
#include "iw4x/Driver/Set.hpp"
#include "iw4x/Calibration/Normalize.hpp"
#include "iw4x/Calibration/Profile.hpp"
#include "iw4x/Aim/Deadzone.hpp"
#include "iw4x/Mapping/Binding.hpp"
#include "iw4x/Mapping/Physical.hpp"
#include "iw4x/Mapping/StickLayout.hpp"
#include "iw4x/Mapping/Glyph.hpp"
#include "controller_menu.h"
#include "controller_glyph.h"
#include "controller.h"
#include "input_policy.h"
#include "controller_policy.h"
#include "controller_prompt.h"
#include <universal/q_shared.h>
#include <client/client.h>
#include <client_mp/client_mp.h>
#include <cgame_mp/cg_local_mp.h>
#include <aim_assist/aim_assist.h>
#include <qcommon/cmd.h>
#include <ui/keycodes.h>
#include <ui/ui_shared.h>
#include <win32/win_local.h>

namespace live::controller
{
namespace
{
using namespace Controller;
using mapping::engine_key;

struct Devices
{
    logging_sink log;
    context ctx{log, false};
    transport::xinput_module xinput{ctx};
    registry registry_{ctx};
    discovery discovery_;
    driver::set drivers{ctx, xinput};
    explicit Devices(bool native) : discovery_(ctx, registry_, xinput, native) {}
};
std::unique_ptr<Devices> devices;
device_id active{};
family activeFamily{family::unknown};
canonical_sample sample{};
mapping::resolved_axes axes{};
button_set previousButtons{};
std::array<bool, 256> heldKeyboard{};
std::array<ActionLatch, 256> actionLatches{};
std::array<std::string, 256> releaseCommands;
std::array<unsigned, 4> repeatAt{}, repeatStarted{};
std::array<bool, 4> previousDirections{};
std::array<bool, 2> triggers{};
std::array<unsigned, 256> pressedAt{};
bool ready{}, focused{}, connected{}, inUse{}, injecting{}, navigating{}, pendingMouse{}, pendingKeyboard{};
bool usedController{}, previousGameplay{}, dispatching{};
unsigned lastSample{}, lastWarning{}, disconnectedAt{};
bool disconnectPrompt{};
int mode{-1};
std::string configuredLayout;
unsigned rumbleEnd{};

const dvar_t *backend{}, *present{}, *inputUsed{}, *style{}, *sensitivity{}, *adsSensitivity{}, *invert{},
    *inner{}, *outer{}, *triggerDeadzone{}, *buttonConfig{}, *stickConfig{}, *rumble{},
    *antiDeadzone{}, *triggerHysteresis{}, *stickPressed{}, *stickHysteresis{}, *scrollFirst{}, *scrollRest{}, *scrollMin{}, *scrollAcceleration{};
cmd_function_s settingsCommand{}, menuDumpCommand{};
constexpr std::array<engine_key, 16> buttonKeys{
    engine_key::button_a, engine_key::button_b, engine_key::button_x, engine_key::button_y,
    engine_key::dpad_up, engine_key::dpad_down, engine_key::dpad_left, engine_key::dpad_right,
    engine_key::button_lshldr, engine_key::button_rshldr, engine_key::button_ltrig, engine_key::button_rtrig,
    engine_key::button_lstick, engine_key::button_rstick, engine_key::button_start, engine_key::button_back};
constexpr std::array<button, 16> physicalButtons{
    button::face_south, button::face_east, button::face_west, button::face_north,
    button::dpad_up, button::dpad_down, button::dpad_left, button::dpad_right,
    button::l1, button::r1, button::l2, button::r2, button::l3, button::r3, button::start, button::back};
void SetInUse(bool value)
{
    inUse = value;
    if (inputUsed) Dvar_SetBool(inputUsed, value);
}

bool MenuContext()
{
    int catchers = clientUIActives[0].keyCatchers;
    // A held scoreboard remains gameplay, matching CoD4 and IW4x. Otherwise
    // opening it would release +scores immediately and close it again.
    if (UI_GetActiveMenu(0) == UIMENU_SCOREBOARD) catchers &= ~KEYCATCH_UI;
    return CL_GetLocalClientConnectionState(0) != CA_ACTIVE ||
        (catchers & (KEYCATCH_UI | KEYCATCH_CONSOLE | KEYCATCH_MESSAGE | KEYCATCH_SCRIPT)) != 0;
}

void UpdateDisconnectPrompt()
{
    const auto state = CL_GetLocalClientConnectionState(0);
    const bool missing = !connected && (state == CA_PRIMED || state == CA_ACTIVE) &&
        !CL_GetLocalClientConnection(0)->demoplaying;
    if (missing && !disconnectPrompt) disconnectedAt = Sys_Milliseconds();
    disconnectPrompt = missing;
}

void Execute(const std::string& command)
{
    if (command.empty()) return;
    std::vector<char> text(command.begin(), command.end());
    text.push_back(0);
    const bool previous = dispatching;
    dispatching = true;
    Cmd_ExecuteSingleCommand(0, 0, text.data());
    dispatching = previous;
}

void Release(int key)
{
    if (!releaseCommands[key].empty())
        Execute(releaseCommands[key] + " " + std::to_string(Sys_Milliseconds()));
    releaseCommands[key].clear();
    actionLatches[key].Clear();
    // Direct command dispatch intentionally doesn't mark keyboard keys down.
}

void ReleaseAll()
{
    for (int key = 0; key < 256; ++key)
        if (actionLatches[key].active) Release(key);
    previousButtons = {};
    previousDirections = {};
    triggers = {};
    axes = {};
    rumbleEnd = 0;
    ResetAim();
    if (devices && active) devices->drivers.submit(active, driver::rumble_request{});
}

void MenuKey(int key, bool down)
{
    injecting = true;
    navigating = true;
    CL_KeyEvent(0, key, down, Sys_Milliseconds());
    navigating = false;
    injecting = false;
}

void OpenSettings() { ReleaseAll(); controller_ui::Open(); }

void PulseMenuKey(int key)
{
    MenuKey(key, true); MenuKey(key, false);
}

void ButtonEvent(engine_key value, bool down)
{
    const int key = static_cast<int>(value);
    if (!down)
    {
        // A tap must be represented in a built command, even below 30 fps.
        if (actionLatches[key].RequestRelease()) Release(key);
        return;
    }
    SetInUse(true);
    usedController = true;
    if (MenuContext())
    {
        if (Display_KeyBindPending() || controller_ui::HandlesKey(key)) { PulseMenuKey(key); return; }
        const int uiKey=MenuKeyForButton(key);
        if (!uiKey) return; // directional repeat is handled separately
        PulseMenuKey(uiKey);
        return;
    }
    if (value == engine_key::button_start) { PulseMenuKey(K_ESCAPE); return; }
    if (Key_IsCatcherActive(0, KEYCATCH_LOCATION_SELECTION))
    {
        if (value == engine_key::button_a || value == engine_key::button_rtrig)
            playerKeys[0].locSelInputState = LOC_SEL_INPUT_CONFIRM;
        else if (value == engine_key::button_b)
            playerKeys[0].locSelInputState = LOC_SEL_INPUT_CANCEL;
        return;
    }
    const char* binding = Key_GetBinding(0, key);
    if (!binding || !binding[0]) return;
    actionLatches[key].Press();
    pressedAt[key] = Sys_Milliseconds();
    if (binding[0] == '+')
    {
        const std::string arguments = " " + std::to_string(key) + " " + std::to_string(pressedAt[key]);
        releaseCommands[key] = "-" + std::string(binding + 1) + " " + std::to_string(key);
        Execute(std::string(binding) + arguments);
    }
    else Execute(binding);
}

void ApplyLayout()
{
    const std::string layout = buttonConfig->current.string;
    if (layout == configuredLayout) return;
    bool haveBindings = false;
    for (auto key : buttonKeys)
    {
        const auto* binding = Key_GetBinding(0, static_cast<int>(key));
        haveBindings |= binding && *binding;
    }
    // On startup retain custom bindings read from the player's config.
    if (!configuredLayout.empty() || !haveBindings)
    {
        ReleaseAll();
        mapping::binding_table table;
        mapping::apply_button_layout(table, layout);
        table.for_each([](engine_key key, const std::string& binding)
        {
            std::string value = binding;
            Key_SetBinding(0, static_cast<int>(key), value.data());
        });
    }
    configuredLayout = layout;
}

bool PhysicalGameplayKey(int key)
{
    if (key == K_ESCAPE || CL_IsConsoleKey(key)) return false;
    const char* binding = Key_GetBinding(0, key);
    if (!binding || !*binding) return key >= K_MOUSE1 && key <= K_MWHEELUP;
    const std::string_view command(binding);
    // Chat, voice, console and menus remain available. Everything else bound to
    // a key is blocked during play, including aliases and compound bindings.
    return command != "messagemode" && command != "messagemode2" && command != "+talk" &&
        command != "toggleconsole" && command != "togglemenu" && command != "+scores" &&
        command != "screenshot" && command != "screenshotJPEG";
}

bool KeyboardAttempt()
{
    if (!GameplayContext()) return false;
    for (int key = 0; key < 256; ++key)
        if (heldKeyboard[key] && PhysicalGameplayKey(key)) return true;
    return pendingMouse || pendingKeyboard;
}
}

void Initialize()
{
    if (ready) return;
    ResetDispatch();
    backend = Dvar_RegisterInt("gpad_backend", 0, 0, 1, DVAR_ARCHIVE,
        "0: native Xbox/PlayStation; 1: Steam Input gamepad emulation (XInput only)");
    present = Dvar_RegisterBool("gpad_present", false, DVAR_ROM, "A controller is connected");
    inputUsed = Dvar_RegisterBool("gpad_in_use", false, DVAR_ROM, "Last menu input was a controller");
    style = Dvar_RegisterInt("gpad_style", 0, 0, 2, DVAR_ARCHIVE, "Glyphs: 0 automatic, 1 PlayStation, 2 Xbox");
    sensitivity = Dvar_RegisterFloat("input_viewSensitivity", 1.0f, 0.0001f, 5.0f, DVAR_ARCHIVE, "Controller sensitivity");
    adsSensitivity = Dvar_RegisterFloat("gpad_adsSensitivity", 1.0f, 0.25f, 2.0f, DVAR_ARCHIVE, "Controller ADS multiplier");
    invert = Dvar_RegisterBool("input_invertPitch", false, DVAR_ARCHIVE, "Invert controller vertical look");
    inner = Dvar_RegisterFloat("gpad_stick_deadzone_min", 0.2f, 0.0f, 0.9f, DVAR_ARCHIVE, "Inner stick deadzone");
    outer = Dvar_RegisterFloat("gpad_stick_deadzone_max", 0.01f, 0.0f, 0.5f, DVAR_ARCHIVE, "Outer stick deadzone");
    triggerDeadzone = Dvar_RegisterFloat("gpad_button_deadzone", 0.13f, 0.0f, 0.9f, DVAR_ARCHIVE, "Trigger threshold");
    buttonConfig = Dvar_RegisterString("gpad_buttonConfig", "buttons_default", DVAR_ARCHIVE, "Controller button layout");
    stickConfig = Dvar_RegisterString("gpad_sticksConfig", "thumbstick_default", DVAR_ARCHIVE, "Controller stick layout");
    rumble = Dvar_RegisterBool("gpad_rumble", true, DVAR_ARCHIVE, "Controller vibration");
    antiDeadzone = Dvar_RegisterFloat("gpad_stick_anti_deadzone", 0, 0, .9f, DVAR_ARCHIVE, "Stick anti-deadzone");
    triggerHysteresis = Dvar_RegisterFloat("gpad_button_deadzone_hysteresis", .05f, 0, .5f, DVAR_ARCHIVE, "Trigger release hysteresis");
    stickPressed = Dvar_RegisterFloat("gpad_stick_pressed", .4f, .05f, 1, DVAR_ARCHIVE, "Menu stick threshold");
    stickHysteresis = Dvar_RegisterFloat("gpad_stick_pressed_hysteresis", .1f, 0, .5f, DVAR_ARCHIVE, "Menu stick hysteresis");
    scrollFirst = Dvar_RegisterInt("gpad_menu_scroll_delay_first", 420, 0, 1000, DVAR_ARCHIVE, "First menu repeat delay");
    scrollRest = Dvar_RegisterInt("gpad_menu_scroll_delay_rest", 210, 0, 1000, DVAR_ARCHIVE, "Menu repeat delay");
    scrollMin = Dvar_RegisterInt("gpad_menu_scroll_delay_min", 50, 0, 1000, DVAR_ARCHIVE, "Fastest menu repeat delay");
    scrollAcceleration = Dvar_RegisterInt("gpad_menu_scroll_accel_time", 1500, 0, 5000, DVAR_ARCHIVE, "Menu scroll acceleration time");
    Cmd_AddCommandInternal("controller_settings", OpenSettings, &settingsCommand);
    Cmd_AddCommandInternal("controller_menu_dump", controller_ui::Dump, &menuDumpCommand);
    ready = true;
}

void Shutdown()
{
    ResetDispatch();
    if (!ready) return;
    ReleaseAll();
    devices.reset();
    active = {};
    connected = false;
    disconnectPrompt = false;
    focused = false;
    mode = -1;
    ready = false;
    Cmd_RemoveCommand("controller_settings");
    Cmd_RemoveCommand("controller_menu_dump");
}

bool GameplayContext()
{
    return ready && focused && !MenuContext() && !CL_GetLocalClientConnection(0)->demoplaying;
}
bool InUse() { return ready && inUse; }
bool PromptsActive() {
    const bool liveGameplay=CL_GetLocalClientConnectionState(0)==CA_ACTIVE &&
        !CL_GetLocalClientConnection(0)->demoplaying &&
        (!Key_IsCatcherActive(0,KEYCATCH_UI) || UI_GetActiveMenu(0)==UIMENU_SCOREBOARD);
    return UseControllerPrompts(InUse(),ready && connected,liveGameplay);
}
bool HasFocus() { return ready && focused; }
bool NavigatingMenu() { return navigating; }
bool MenuOpen() { return controller_ui::IsOpen(); }
float Sensitivity() { return sensitivity ? sensitivity->current.value : 1.0f; }
float AdsSensitivity() { return adsSensitivity ? adsSensitivity->current.value : 1.0f; }
bool InvertPitch() { return invert && invert->current.enabled; }

bool FilterKey(int localClient, int key, bool down)
{
    if (!ready || injecting || key < 0 || key >= 256) return false;
    if (!focused)
    {
        // Focus-loss releases may still clear accepted native key state, but
        // background key-down messages cannot become a future gameplay attempt.
        heldKeyboard[key] = false;
        return down || !playerKeys[localClient].keys[key].down;
    }
    heldKeyboard[key] = down;
    if (down) SetInUse(false);
    // Only pass a release to KeyState if its matching down reached KeyState.
    // This clears held chat/menu keys without decrementing unrelated keys when
    // a blocked gameplay key is released.
    if (!down) return !playerKeys[localClient].keys[key].down;
    if (down && MenuContext() && key == K_F7) { OpenSettings(); return true; }
    if (GameplayContext() && PhysicalGameplayKey(key))
    {
        pendingKeyboard |= down;
        return down; // native key-up still clears menu/chat KeyState and repeats
    }
    return false;
}

void ResetDispatch()
{
    // Com_Error unwinds via longjmp, so scoped provenance requires this hook.
    dispatching = injecting = navigating = false;
}

bool AllowCommand(const char* name)
{
    if (dispatching || !IsGameplayCommand(name)) return true;
    // Console/chat use is exempt, but must not arm gameplay buttons or toggles
    // which survive closing those interfaces. Unauthorized releases are also
    // denied; they must not cancel a controller-owned hold or toggle stance.
    if (GameplayContext() && name[0] != '-') pendingKeyboard = true;
    return false;
}

bool FilterMouse(int dx, int dy)
{
    if (!ready) return false;
    if (!focused) return true;
    if (PreserveMenuFocus(inUse, MenuContext(), dx, dy)) return true;
    if (dx || dy)
    {
        SetInUse(false);
        if (GameplayContext()) pendingMouse = true;
    }
    return GameplayContext();
}

void ClearBackgroundInput()
{
    axes = {};
    usedController = false;
    previousGameplay = false;
    heldKeyboard.fill(false);
    pendingMouse = pendingKeyboard = false;
}

void PollFrame(bool hasFocus)
{
    if (!ready || !cls.uiStarted) return;
    const bool resumed = hasFocus && !focused;
    const bool lostFocus = !hasFocus && focused;
    focused = hasFocus;
    if (!focused)
    {
        if (lostFocus) ReleaseAll();
        ClearBackgroundInput();
    }
    ApplyLayout();
    if (mode != backend->current.integer)
    {
        ReleaseAll();
        devices.reset();
        active = {};
        connected = false;
        mode = backend->current.integer;
        devices = std::make_unique<Devices>(mode == 0);
    }
    auto& d = *devices;
    d.discovery_.scan();
    d.drivers.reconcile(d.registry_);
    std::optional<device_connection> selected;
    d.drivers.for_each([&](driver::driver&, const device_connection& candidate)
    {
        if (!selected || (candidate.transport == transport_kind::hid && selected->transport != transport_kind::hid) ||
            (candidate.transport == selected->transport && candidate.id == active)) selected = candidate;
    });
    if (!selected || selected->id != active)
    {
        ReleaseAll();
        active = selected ? selected->id : no_device;
        sample = {};
        lastSample = 0;
    }
    const unsigned now = Sys_Milliseconds();
    bool fresh = false;
    d.drivers.for_each([&](driver::driver& device, const device_connection& connection)
    {
        if (!selected || connection.id != active) return;
        raw_sample raw;
        canonical_sample state;
        if (!device.poll(raw, state)) return;
        calibration::apply(calibration::default_profile(connection.identity.family), raw, state);
        sample = std::move(state);
        activeFamily = connection.identity.family;
        lastSample = now;
        fresh = true;
    });
    const bool wasConnected = connected;
    const auto availability = Availability(focused, selected.has_value(), fresh, lastSample, now);
    connected = availability.present;
    Dvar_SetBool(present, connected);
    UpdateDisconnectPrompt();
    // HID reports arrive continuously; XInput poll was adapted to publish idle
    // samples too. A stale stream never keeps a stuck trigger or approval alive.
    if (!connected)
    {
        if (wasConnected) ReleaseAll();
        return;
    }
    // Discovery, reconciliation and samples above keep running on blur. Only
    // physical presence reaches commands; no button/axis/menu dispatch follows.
    if (!availability.dispatchInput) return;
    const float safeOuter = std::min(outer->current.value, .99f - inner->current.value);
    const aim::deadzone_params deadzone{{inner->current.value}, {safeOuter}, {antiDeadzone->current.value}};
    const auto left = aim::apply(deadzone, sample.sticks[0].calibrated);
    const auto right = aim::apply(deadzone, sample.sticks[1].calibrated);
    axes = mapping::resolve(mapping::stick_layout_from_name(stickConfig->current.string), left, right);
    button_set buttons = sample.buttons;
    for (int i = 0; i < 2; ++i)
    {
        const float threshold = triggerDeadzone->current.value - (triggers[i] ? triggerHysteresis->current.value : 0.0f);
        triggers[i] = sample.triggers[i].normalized > std::max(0.001f, threshold);
        buttons.set(i ? button::r2 : button::l2, triggers[i]);
    }
    // A button pressed while another application had focus needs a new press
    // after returning; it must not select a menu item or resume firing.
    if (resumed) previousButtons = buttons;
    if (left.x || left.y || right.x || right.y || buttons.any())
    {
        SetInUse(true);
        usedController = true;
    }
    const bool gameplay = GameplayContext();
    if (gameplay != previousGameplay)
    {
        // Never resume a held fire/grenade from a menu. Require a fresh press.
        for (int key = 0; key < 256; ++key) if (actionLatches[key].active) Release(key);
        previousButtons = buttons;
        previousDirections = {};
        ResetAim();
        previousGameplay = gameplay;
    }
    for (size_t i = 0; i < physicalButtons.size(); ++i)
    {
        const bool down = buttons.down(physicalButtons[i]);
        if (down != previousButtons.down(physicalButtons[i])) ButtonEvent(buttonKeys[i], down);
    }
    previousButtons = buttons;
    for (int key = 0; key < 256; ++key)
        if (actionLatches[key].ReleaseReady()) Release(key);

    if (MenuContext())
    {
        const float values[]{std::max(left.y,right.y), std::max(-left.y,-right.y),
            std::max(-left.x,-right.x), std::max(left.x,right.x)};
        constexpr button dpad[]{button::dpad_up,button::dpad_down,button::dpad_left,button::dpad_right};
        constexpr int uiKeys[]{K_UPARROW, K_DOWNARROW, K_LEFTARROW, K_RIGHTARROW};
        for (int i = 0; i < 4; ++i)
        {
            const float threshold=std::max(.01f,stickPressed->current.value+(previousDirections[i]?-stickHysteresis->current.value:stickHysteresis->current.value));
            const bool down=buttons.down(dpad[i]) || values[i]>threshold;
            if (down && (!previousDirections[i] || static_cast<int>(now-repeatAt[i])>=0))
            {
                const float leftValues[]{left.y,-left.y,-left.x,left.x};
                const int physical=buttons.down(dpad[i])?20+i:leftValues[i]>threshold?28+i:224+i;
                PulseMenuKey(controller_ui::HandlesKey(physical) || (Display_KeyBindPending() && buttons.down(dpad[i])) ? physical : uiKeys[i]);
                if (!previousDirections[i]) repeatStarted[i]=now;
                const float progress=scrollAcceleration->current.integer>0 ? std::min(1.0f,float(now-repeatStarted[i])/scrollAcceleration->current.integer) : 1.0f;
                const int repeat=static_cast<int>(scrollRest->current.integer+(scrollMin->current.integer-scrollRest->current.integer)*progress);
                repeatAt[i]=now+std::max(1,previousDirections[i]?repeat:scrollFirst->current.integer);
            }
            previousDirections[i]=down;
        }
    }
    driver::output_policy output;
    output.rumble = rumble->current.enabled;
    output.haptics = driver::haptic_mode::emulated;
    d.drivers.configure(output);
    if (rumbleEnd && static_cast<int>(now - rumbleEnd) >= 0)
    {
        rumbleEnd = 0;
        d.drivers.submit(active, driver::rumble_request{});
    }
}

void Frame(bool hasFocus)
{
    static unsigned retryAfter = 0;
    const unsigned now = Sys_Milliseconds();
    if (retryAfter && static_cast<int>(now - retryAfter) < 0)
    {
        // A device failure already released actions and cleared presence. Focus
        // changes must still gate keyboard/mouse events during the retry delay.
        if (!hasFocus) { focused = false; ClearBackgroundInput(); }
        return;
    }
    try
    {
        PollFrame(hasFocus);
        retryAfter = 0;
    }
    catch (const std::exception& error)
    {
        ReleaseAll();
        devices.reset();
        active = {};
        connected = false;
        mode = -1;
        if (present) Dvar_SetBool(present, false);
        Com_Printf(CON_CHANNEL_SYSTEM, "Controller device error: %s\n", error.what());
        retryAfter = now + 1000;
    }
}

std::uint8_t BeginCommand()
{
    UpdateDisconnectPrompt();
    const bool attempt = KeyboardAttempt();
    std::uint8_t flags = connected ? ControllerConnected : 0;
    if (connected && usedController && GameplayContext()) flags |= ControllerActive;
    if (attempt) { flags |= KeyboardMouseAttempt; lastWarning = Sys_Milliseconds(); }
    pendingMouse = pendingKeyboard = false;
    usedController = false;
    if (InputPolicy::BlockGameplay(flags) || MenuContext() || !focused) ResetAim();
    return flags;
}

void EndCommand()
{
    for (auto& latch : actionLatches) latch.CommandBuilt();
}

void Move(int localClient, usercmd_s& command)
{
    if (!connected || !GameplayContext() || InputPolicy::BlockGameplay(command.inputFlags)) return;
    auto* client = CL_GetLocalClientGlobals(localClient);
    if (client->snap.ps.pm_flags & PMF_FROZEN) { ResetAim(); return; }
    const float largest = std::max(std::abs(axes.forward), std::abs(axes.side));
    const float magnitude = std::sqrt(axes.forward * axes.forward + axes.side * axes.side);
    const float scale = largest > 0 ? 127.0f * magnitude / largest : 127.0f;
    command.forwardmove = static_cast<char>(std::clamp(static_cast<int>(std::round(axes.forward * scale)), -127, 127));
    command.rightmove = static_cast<char>(std::clamp(static_cast<int>(std::round(axes.side * scale)), -127, 127));
    AimInput input{};
    input.localClientNum = localClient;
    input.ps = CG_GetPredictedPlayerState(localClient);
    input.deltaTime = std::clamp(cls.frametime * 0.001f, 0.0f, 0.1f);
    input.pitch = client->viewangles[0];
    input.yaw = client->viewangles[1];
    input.pitchAxis = -axes.pitch;
    input.yawAxis = -axes.yaw;
    input.forwardAxis = axes.forward;
    input.rightAxis = axes.side;
    input.pitchMax = client->cgameMaxPitchSpeed;
    input.yawMax = client->cgameMaxYawSpeed;
    input.buttons = command.buttons;
    AimOutput output{};
    UpdateAim(input, output);
    client->viewangles[0] = output.pitch;
    client->viewangles[1] = output.yaw;
    command.meleeChargeYaw = output.meleeChargeYaw;
    command.meleeChargeDist = output.meleeChargeDist;
    if ((command.buttons & BUTTON_ATTACK) && rumble->current.enabled && !rumbleEnd)
    {
        devices->drivers.submit(active, driver::rumble_request{0.2f, 0.35f});
        rumbleEnd = Sys_Milliseconds() + 60;
    }
}

void LocationMove(int, float& x, float& y, float dt, float aspect)
{
    if (!connected || !focused || KeyboardAttempt()) return;
    x = std::clamp(x + axes.yaw * dt * 0.6f, 0.0f, 1.0f);
    y = std::clamp(y - axes.pitch * dt * 0.6f * aspect, 0.0f, 1.0f);
}

const char* KeyName(int key, bool display)
{
    // IW4x's reserved controller range includes 24/25, which have no named
    // key/glyph. Only expose actual table entries to CoD4's 256-key scans.
    std::optional<engine_key> mapped;
    for (const auto candidate:mapping::keys())
        if (static_cast<int>(candidate)==key) { mapped=candidate; break; }
    if (!mapped) return nullptr;
    if (!display) return mapping::key_name(*mapped);
    std::optional<mapping::glyph_family> override;
    if(style && style->current.integer==1) override=mapping::glyph_family::playstation;
    else if(style && style->current.integer==2) override=mapping::glyph_family::xbox;
    const auto* glyph=mapping::glyph_for(*mapped,mapping::glyph_family_for(activeFamily,override));
    return glyph?controller_glyph::Encode(glyph):mapping::key_name(*mapped);
}

int CommandKeys(const char* command, int* keys)
{
    std::array<PromptBinding,buttonKeys.size()> bindings{};
    for (std::size_t i=0;i<buttonKeys.size();++i) {
        const int key=static_cast<int>(buttonKeys[i]);
        bindings[i]={key,Key_GetBinding(0,key)};
    }
    return ResolveCommandKeys(command ? command : "",bindings,keys);
}

void Draw()
{
    if (!ready || !cls.uiStarted || !cls.rendererStarted) return;
    UpdateDisconnectPrompt();
    const ScreenPlacement* placement = &scrPlaceFull;
    const float white[]{.94f, .94f, .90f, 1.0f}, gold[]{.78f, .66f, .35f, 1.0f};
    auto text = [&](const char* message, float x, float y, float scale, const float* color)
    {
        UI_DrawText(placement, message, 1024, UI_GetFontHandle(placement, 0, scale), x, y, 0, 0, scale, color, 0);
    };
    auto drawConnectionPrompt = [&](bool compact)
    {
        if (!disconnectPrompt) return;
        const unsigned elapsed = Sys_Milliseconds() - disconnectedAt;
        const unsigned remaining = elapsed >= InputPolicy::DisconnectGraceMs ? 0 :
            (InputPolicy::DisconnectGraceMs - elapsed + 999) / 1000;
        const bool joining = CL_GetLocalClientConnectionState(0) == CA_PRIMED;
        char message[160];
        sprintf_s(message, "%s - disconnect in %u seconds",
            joining ? "Connect a controller to join" : "Reconnect a controller to play", remaining);
        text(message, compact ? 65.0f : 108.0f, compact ? 442.0f : 185.0f,
            compact ? .20f : .28f, gold);
    };
    drawConnectionPrompt(false);
    if (CL_GetLocalClientConnectionState(0) == CA_ACTIVE && connected &&
        lastWarning && Sys_Milliseconds() - lastWarning < 1500)
        text("Controller required - keyboard/mouse gameplay blocked", 95, 185, .27f, gold);
}
}
