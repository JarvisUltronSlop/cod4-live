#pragma once
struct menuDef_t;
struct itemDef_s;
namespace live::bots_ui {
void Initialize();
void Refresh();
bool CanConfigure();
void ConfigureCapacityItem(itemDef_s* item);
void ConfigureBotItem(itemDef_s* item,bool difficulty);
const char* CountLabel();
void ApplyHostCapacity();
}
