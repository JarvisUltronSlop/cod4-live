#include <live/host_menu.h>
#include <ui/ui_shared.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
dvar_s indexDvar{},domainDvar{};
int gameType{},currentMap=2,lastSelection=-1;
std::vector<int> available{2,5,9};
std::vector<std::string> displayed;
void Require(bool ok,const char* message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); } }
}
const dvar_s* Dvar_RegisterInt(const char*,int value,uint32_t,uint32_t,uint32_t,const char*) { indexDvar.current.integer=value;return &indexDvar; }
const dvar_s* Dvar_RegisterEnum(const char*,const char**,int,uint16_t,const char*) { return &domainDvar; }
void Dvar_UpdateEnumDomain(dvar_s*,const char** strings) { displayed.clear();for(;*strings;++strings) displayed.emplace_back(*strings); }
void Dvar_SetInt(dvar_s* dvar,int value) { dvar->current.integer=value; }
int Dvar_GetInt(const char* name) { return !std::strcmp(name,"ui_netGametype")?gameType:currentMap; }
int UI_FeederCount(int,float feeder) { Require(feeder==4,"native map feeder");return static_cast<int>(available.size()); }
const char* UI_FeederItemText(int,itemDef_s*,float,int index,uint32_t,Material**) {
    static std::string label;label="Map "+std::to_string(available.at(index));return label.c_str();
}
void UI_OverrideCursorPos(int,itemDef_s* item) {
    const auto it=std::find(available.begin(),available.end(),currentMap);
    item->cursorPos[0]=it==available.end()?0:static_cast<int>(it-available.begin());
}
void UI_FeederSelection(int,float,int index) { lastSelection=index;currentMap=available.at(index); }
int main() {
    using namespace live::host_ui;
    itemDef_s row{};ConfigureMapItem(&row);Initialize();
    Require(displayed==std::vector<std::string>{"Map 2","Map 5","Map 9"},"all native map names populate choices");
    Require(indexDvar.current.integer==0,"initial row matches native map selection");
    indexDvar.current.integer=2;SettingChanged(&row);
    Require(currentMap==9&&lastSelection==2,"row selection applies immediately before Start can run");
    currentMap=5;Refresh();Require(indexDvar.current.integer==1,"mouse list selection synchronizes to inline row");
    available={5,9};++gameType;Refresh();
    Require(displayed.size()==2&&indexDvar.current.integer==0,"game mode filters map names and remaps selection");
    indexDvar.current.integer=99;lastSelection=-1;SettingChanged(&row);
    Require(lastSelection==-1,"out-of-range selection cannot reach feeder");
    available.clear();++gameType;Refresh();SettingChanged(&row);
    Require(displayed==std::vector<std::string>{"No maps"}&&lastSelection==-1,"empty mode does not select a nonexistent map");
    available.resize(100);for(int i=0;i<100;++i) available[i]=i;
    currentMap=99;++gameType;Refresh();
    Require(displayed.size()==100&&indexDvar.current.integer==99,"custom map collections are not limited to 32 multi-choice entries");
    Initialize();Require(indexDvar.current.integer==99,"renderer/UI restart rebuilds current choices");
    std::puts("Host map menu synchronization passed");
}
