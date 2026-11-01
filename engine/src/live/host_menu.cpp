#include "host_menu.h"
#include <ui/ui_shared.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace live::host_ui {
namespace {
const dvar_s *selection{},*domain{};
std::vector<std::string> labels;
std::vector<const char*> choices;
int lastGameType=-1,lastMap=-1,mapCount{};
constexpr const char* mapIndex="ui_live_hostMapIndex";
}
void ConfigureMapItem(itemDef_s* item) {
    item->type=item->dataType=13;
    item->dvar=mapIndex;
    item->typeData.enumDvarName="ui_live_hostMapOptions";
    item->text=nullptr;item->textExp={};item->onKey=nullptr;
    item->dvarFlags=0;
}
void Initialize() {
    static const char* empty[]{"No maps",nullptr};
    selection=Dvar_RegisterInt(mapIndex,0,0,1023,DVAR_NOFLAG,"Host map selection");
    domain=Dvar_RegisterEnum("ui_live_hostMapOptions",empty,0,DVAR_NOFLAG,"Maps available for the selected game mode");
    lastGameType=lastMap=-1;
    Refresh();
}
void Refresh() {
    if(!selection) return;
    const int gameType=Dvar_GetInt("ui_netGametype");
    const bool rebuild=gameType!=lastGameType;
    if(rebuild) {
        mapCount=std::clamp(UI_FeederCount(0,4.f),0,1024);
        labels.clear();choices.clear();labels.reserve(std::max(mapCount,1));
        for(int i=0;i<mapCount;++i) {
            Material* unused=nullptr;
            labels.emplace_back(UI_FeederItemText(0,nullptr,4.f,i,0,&unused));
        }
        if(labels.empty()) labels.emplace_back("No maps");
        for(const auto& label:labels) choices.push_back(label.c_str());
        choices.push_back(nullptr);
        Dvar_UpdateEnumDomain(const_cast<dvar_s*>(domain),choices.data());
        lastGameType=gameType;
    }
    const int currentMap=Dvar_GetInt("ui_currentNetMap");
    if(rebuild||currentMap!=lastMap) {
        itemDef_s mapList{};mapList.special=4.f;
        if(mapCount) UI_OverrideCursorPos(0,&mapList);
        Dvar_SetInt(selection,std::clamp(mapList.cursorPos[0],0,std::max(0,mapCount-1)));
        lastMap=currentMap;
    }
}
void SettingChanged(const itemDef_s* item) {
    if(!selection||!item->dvar||std::strcmp(item->dvar,mapIndex)) return;
    const int index=selection->current.integer;
    if(index>=0&&index<UI_FeederCount(0,4.f)) UI_FeederSelection(0,4.f,index);
}
}
