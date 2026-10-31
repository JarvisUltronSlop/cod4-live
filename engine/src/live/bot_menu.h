#pragma once
struct menuDef_t;
namespace live::bots_ui {
void Initialize();
menuDef_t* Menu();
void Refresh();
bool CanConfigure();
}
