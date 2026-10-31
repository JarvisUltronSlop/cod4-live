#pragma once

struct client_t;
struct scr_entref_t;

namespace live::bots
{
void Initialize();
void LoadScripts();
void StartScripts();
void Shutdown();
void Frame();
bool CanAdd();
void ResetClient(int index);
void UserMove(client_t* client);
bool SelectWeapon(int index, int weapon, bool offhand);
using Function = void (*)();
using Method = void (*)(scr_entref_t);
Function GetFunction(const char** name);
Method GetMethod(const char** name);
}
