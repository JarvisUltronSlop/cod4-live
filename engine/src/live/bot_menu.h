#pragma once
struct menuDef_t;
struct itemDef_s;
namespace live::bots_ui {
void Initialize();
menuDef_t* Menu();
void Refresh();
bool CanConfigure();
void ConfigureCapacityItem(itemDef_s* item);
void ApplyHostCapacity();
}
