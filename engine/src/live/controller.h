#pragma once
#include <cstdint>

struct usercmd_s;
struct AimInput;
struct AimOutput;

namespace live::controller
{
void Initialize();
void Shutdown();
void Frame(bool focused);
void Draw();
// True consumes a physical keyboard/mouse event before it can mutate gameplay.
bool FilterKey(int localClient, int key, bool down);
bool FilterMouse(int dx, int dy);
// Gate console/config commands separately from exempt menu/chat key events.
bool AllowCommand(const char* name);
void ResetDispatch();
bool InUse();
bool HasFocus();
bool NavigatingMenu();
bool MenuOpen();
bool GameplayContext();
const char* KeyName(int key, bool display);
int CommandKeys(const char* command, int* keys);
std::uint8_t BeginCommand();
void EndCommand();
void Move(int localClient, usercmd_s& cmd);
void LocationMove(int localClient, float& x, float& y, float dt, float aspect);
void ResetAim();
float Sensitivity();
bool InvertPitch();
float AdsSensitivity();
void UpdateAim(const AimInput& input, AimOutput& output);
}
