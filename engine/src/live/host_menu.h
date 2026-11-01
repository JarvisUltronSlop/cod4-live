#pragma once
struct itemDef_s;
namespace live::host_ui {
void Initialize();
void Refresh();
void ConfigureMapItem(itemDef_s* item);
void SettingChanged(const itemDef_s* item);
}
