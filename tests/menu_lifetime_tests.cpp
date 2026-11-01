#include <ui/ui_shared.h>
#include <live/bot_menu.h>
#include <live/host_menu.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The fixture owns retail menu storage; the extracted production code owns
// runtime additions. No renderer/database is needed to exercise the boundary.
int I_stricmp(const char* a,const char* b) { return _stricmp(a,b); }
void Com_DPrintf(int,const char*,...) {}
void Com_Error(errorParm_t,const char*,...) { std::abort(); }
namespace live::bots_ui {
void ConfigureCapacityItem(itemDef_s* item) { item->dvar="capacity_index"; }
void ConfigureBotItem(itemDef_s* item,bool difficulty) { item->dvar=difficulty?"difficulty":"bots"; }
const char* CountLabel() { return "BOTS"; }
}
namespace live::host_ui { void ConfigureMapItem(itemDef_s* item) { item->dvar="map"; } }
namespace live::controller_ui {
UiContext* context{};
int inspected{};
char footer[512]{};
bool Contains(const char* text,const char* fragment) { return text&&std::strstr(text,fragment); }
void Color(float (&c)[4],float r,float g,float b,float a=1) { c[0]=r;c[1]=g;c[2]=b;c[3]=a; }
#include "generated_menu_lifetime.inc"
}
void Require(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main() {
    using namespace live::controller_ui;
    menuDef_t shared{},host{},mainMenu{};
    auto fixtureContext=std::make_unique<UiContext>();
    Append(fixtureContext.get(),&shared);
    Append(fixtureContext.get(),&host);
    Append(fixtureContext.get(),&shared);
    Require(fixtureContext->menuCount==2&&fixtureContext->Menus[0]==&shared&&fixtureContext->Menus[1]==&host,
        "late in-game style rebuild keeps one context entry per page");
    itemDef_s original{},capacity{};
    itemDef_s* sharedItems[]{&original};
    itemDef_s* hostItems[]{&capacity};
    shared.window.name="popup_unlock_page2";shared.items=sharedItems;shared.itemCount=1;
    host.window.name="create_server";host.items=hostItems;host.itemCount=1;
    capacity.parent=&host;capacity.dvar="sv_maxclients";
    capacity.window.rectClient={0,40,300,24,0,0};
    itemDef_s control{},option{},quit{},notice{};
    itemDef_s* mainItems[]{&control,&option,&quit,&notice};
    mainMenu.window.name="main_text";mainMenu.items=mainItems;mainMenu.itemCount=4;
    control.parent=option.parent=quit.parent=notice.parent=&mainMenu;
    control.window.rectClient={0,284,220,22,0,0};control.action="open main_controls ;";
    option.window.rectClient={0,308,220,22,0,0};quit.window.rectClient={0,390,220,22,0,0};
    notice.window.rectClient={40,420,240,60,0,0};
    for(int cycle=0;cycle<8;++cycle) {
        Extend(&shared,false,false);
        auto* extended=shared.items;
        Require(shared.itemCount==2 && shared.items[0]==&original,"shared menu retains original row and appends help");
        Extend(&host,true,true);
        Require(host.items[0]!=&capacity && capacity.dvar==std::string("sv_maxclients"),"host extension copies retail settings");
        // code.txt and in-game menus both reference the same DB menu asset.
        Extend(&shared,false,false);
        Extend(&host,true,true);
        Require(shared.items==extended && shared.itemCount==2,"duplicate asset reference must not extend twice");
        Require(extensions.size()==2,"one owner per unique DB menu");
        Extend(&mainMenu,true,false);
        Require(mainMenu.items[0]->window.rectClient.y==260,"Controls moves up to make room for inline Controller");
        Require(!std::strcmp(mainMenu.items[1]->window.name,"live_controller_entry")&&mainMenu.items[1]->window.rectClient.x==0&&mainMenu.items[1]->window.rectClient.y==284,"Controller occupies the native list column");
        Require(mainMenu.items[2]==&option&&mainMenu.items[3]==&quit&&mainMenu.items[4]==&notice,"new row preserves lower menu and footer positions");
        BeforeInitialize();
        Require(mainMenu.items==mainItems&&control.window.rectClient.y==284,"main menu retail rows restored");
        Require(shared.items==sharedItems && shared.itemCount==1 && shared.items[0]==&original,"shared asset restored before freeing additions");
        Require(host.items==hostItems && host.itemCount==1 && host.items[0]==&capacity,"host asset restored after extension teardown");
        Require(extensions.empty(),"all runtime additions released");
        BeforeInitialize();
        Require(shared.items==sharedItems && host.items==hostItems,"repeated shutdown is harmless");
    }
    std::puts("Native menu lifetime regressions passed");
}
