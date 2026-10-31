#include "bot_menu.h"
#include "bot_warfare.h"
#include "server_capacity.h"
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <client_mp/client_mp.h>
#include <ui/ui_shared.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Domains for inline Host Game rows. controller_menu.cpp copies the stock
// gray-label/value row assets; drawing, focus and editing stay in the native UI.
namespace live::bots_ui {
namespace {
char countLabels[64][4]{},capacityLabels[64][4]{};
const char* countChoices[65]{};
const char* capacityChoices[65]{};
const char* difficulties[]{"Random","Recruit","Easy","Regular","Hardened","Veteran","Expert","Maximum",nullptr};
const dvar_s *capacityIndex{},*countDomain{},*enabled{};
int previousCapacityIndex=-1,previousCap=-1;
bool initialized{};
char countLabel[64]{};
std::vector<itemDef_s*> inlineItems;
int CapacityLimit() {
    const bool hosting=com_sv_running&&com_sv_running->current.enabled;
    const auto* capacity=Dvar_FindVar("sv_maxclients");
    if(capacity&&(capacity->type==DVAR_TYPE_INT||capacity->type==DVAR_TYPE_ENUM))
        return CapacityForMenu(hosting,capacity->current.integer,capacity->latched.integer)-1;
    const int value=capacity&&capacity->type==DVAR_TYPE_STRING?std::atoi(capacity->current.string):1;
    return CapacityForMenu(hosting,value,value)-1;
}
}
bool CanConfigure() {
    return (com_sv_running&&com_sv_running->current.enabled)||
        clientUIActives[0].connectionState==CA_DISCONNECTED;
}
void Initialize() {
    live::bots::Initialize();
    inlineItems.clear();
    for(int i=0;i<64;++i) {
        std::snprintf(countLabels[i],sizeof(countLabels[i]),"%d",i);
        countChoices[i]=countLabels[i];
        std::snprintf(capacityLabels[i],sizeof(capacityLabels[i]),"%d",i+1);
        capacityChoices[i]=capacityLabels[i];
    }
    countChoices[64]=capacityChoices[64]=nullptr;
    Dvar_RegisterEnum("ui_live_hostCapacityOptions",capacityChoices,0,DVAR_NOFLAG,"Host maximum player choices");
    capacityIndex=Dvar_RegisterInt("ui_live_hostCapacityIndex",0,0,MaxClientCapacity-1,DVAR_NOFLAG,"Host maximum players selection");
    countDomain=Dvar_RegisterEnum("ui_live_botCountOptions",countChoices,0,DVAR_NOFLAG,"Bot count menu choices");
    Dvar_RegisterEnum("ui_live_botDifficultyOptions",difficulties,0,DVAR_NOFLAG,"Bot difficulty menu choices");
    enabled=Dvar_RegisterBool("ui_live_botControls",true,DVAR_ROM,"Local bot configuration available");
    previousCap=previousCapacityIndex=-1;
    initialized=true;
    Refresh();
}
const char* CountLabel() { return countLabel; }
void ConfigureCapacityItem(itemDef_s* item) {
    item->type=item->dataType=13;
    item->dvar="ui_live_hostCapacityIndex";
    item->typeData.enumDvarName="ui_live_hostCapacityOptions";
    item->text=nullptr;
    item->textExp={};
}
void ConfigureBotItem(itemDef_s* item,bool difficulty) {
    item->type=item->dataType=13;
    item->dvar=difficulty?"live_botDifficulty":"live_botCount";
    item->typeData.enumDvarName=difficulty?"ui_live_botDifficultyOptions":"ui_live_botCountOptions";
    item->text=nullptr;
    item->textExp={};
    item->dvarTest="ui_live_botControls";
    item->enableDvar="1";
    item->dvarFlags=1;
    item->onKey=nullptr;
    inlineItems.push_back(item);
}
void ApplyHostCapacity() {
    const auto* capacity=Dvar_FindVar("sv_maxclients");
    if(!capacityIndex||!capacity||capacity->type!=DVAR_TYPE_INT) return;
    const int selected=capacityIndex->current.integer;
    if(previousCapacityIndex>=0&&selected!=previousCapacityIndex)
        Dvar_SetIntFromSource(const_cast<dvar_s*>(capacity),CapacityFromMenuIndex(selected),DVAR_SOURCE_EXTERNAL);
    const int pending=CapacityForMenu(false,capacity->current.integer,capacity->latched.integer);
    previousCapacityIndex=pending-1;
    Dvar_SetInt(capacityIndex,previousCapacityIndex);
}
void Refresh() {
    if(!initialized) return;
    ApplyHostCapacity();
    const int maximum=CapacityLimit();
    if(previousCap!=maximum) {
        for(int i=0;i<=maximum;++i) countChoices[i]=countLabels[i];
        countChoices[maximum+1]=nullptr;
        Dvar_UpdateEnumDomain(const_cast<dvar_s*>(countDomain),countChoices);
        previousCap=maximum;
    }
    const bool allowed=CanConfigure();
    Dvar_SetBool(enabled,allowed);
    if(allowed) {
        const int count=Dvar_GetInt("live_botCount");
        if(count<0||count>maximum) Dvar_SetIntByName("live_botCount",std::clamp(count,0,maximum));
    } else for(auto* item:inlineItems) item->window.dynamicFlags[0]&=~2;
    std::snprintf(countLabel,sizeof(countLabel),"BOTS (MAX %d)",maximum);
}
}
