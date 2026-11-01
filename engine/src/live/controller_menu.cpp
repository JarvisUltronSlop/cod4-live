#include "controller_menu.h"
#include "controller.h"
#include "controller_policy.h"
#include "controller_glyph.h"
#include "bot_menu.h"
#include "host_menu.h"
#include "backend_switch.h"
#include "input_policy.h"
#include <universal/q_shared.h>
#include <client_mp/client_mp.h>
#include <client/client.h>
#include <ui/ui_shared.h>
#include <qcommon/cmd.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Runtime menuDef/itemDef objects, painted and controlled by the stock UI.
// Layout names and sensitivity presets match pinned IW4x rawfiles. No MW2
// compiled menus, localization tables, fonts or retail material bundles needed.
namespace live::controller_ui {
namespace {
struct Page {
    menuDef_t menu{};
    std::array<itemDef_s, 192> rows{};
    std::array<itemDef_s*, 192> pointers{};
    std::array<multiDef_s, 16> choices{};
    std::array<editFieldDef_s, 16> sliders{};
    std::array<std::array<char, 64>, 16> values{};
    std::array<const char*, 16> valueDvars{};
    std::string escape, focus;
    std::array<std::string,8> navigation;
    struct Highlight { itemDef_s* background; itemDef_s* control; bool selected; };
    std::vector<Highlight> highlights;
    bool native{};
    int choiceCount{}, sliderCount{}, valueCount{};
};
Page general, response, sticks, timing, bindings, backendConfirm;
UiContext* context{};
int inspected{};
char footer[512]{}, deviceStatus[160]{}, backendStatus[160]{};
std::array<std::array<char, 160>, 16> bindingText{};
std::array<std::array<char, 80>, 16> bindingLabels{};
controller::BackendSwitch backendSwitch;
const dvar_s* pendingBackend{};
unsigned backendNoticeUntil{};
struct NativeStyle {
    menuDef_t *background{},*options{},*graphics{};
    int choice=-1,slider=-1;
} nativeStyle;
struct Extension {
    struct Expression {
        std::vector<expressionEntry> entries;
        std::vector<expressionEntry*> pointers;
    };
    menuDef_t* menu{};
    itemDef_s** original{};
    int originalCount{};
    std::vector<itemDef_s*> pointers;
    std::vector<std::unique_ptr<itemDef_s>> items;
    std::vector<std::unique_ptr<Expression>> expressions;
    std::vector<std::unique_ptr<std::string>> scripts;
};
std::vector<Extension> extensions;
constexpr const char* layoutValues[]{"buttons_default", "buttons_tactical", "buttons_lefty", "buttons_nomad",
    "buttons_default_alt", "buttons_tactical_alt", "buttons_lefty_alt", "buttons_nomad_alt"};
constexpr const char* layoutNames[]{"Default", "Tactical", "Lefty", "Nomad", "Default flipped", "Tactical flipped", "Lefty flipped", "Nomad flipped"};
constexpr const char* stickValues[]{"thumbstick_default", "thumbstick_southpaw", "thumbstick_legacy", "thumbstick_legacysouthpaw"};
constexpr const char* stickNames[]{"Default", "Southpaw", "Legacy", "Legacy southpaw"};
void Color(float (&c)[4], float r, float g, float b, float a = 1) { c[0]=r; c[1]=g; c[2]=b; c[3]=a; }
itemDef_s& Item(Page& p, const char* name, const char* text, float x, float y, float width, bool interactive=false) {
    if(p.menu.itemCount>=static_cast<int>(p.rows.size())) Com_Error(ERR_DROP,"Controller page item limit");
    auto& i=p.rows[p.menu.itemCount];
    p.pointers[p.menu.itemCount++]=&i;
    i={}; i.parent=&p.menu; i.window.name=name;
    i.window.rect=i.window.rectClient={x,y,width,25,0,0};
    i.window.dynamicFlags[0]=4|0x10000;
    i.window.staticFlags=interactive ? 0 : 0x100000;
    Color(i.window.foreColor,.86f,.87f,.81f);
    Color(i.window.outlineColor,0,0,0,.75f);
    i.fontEnum=4; i.textscale=.32f; i.textStyle=3; i.textaligny=18;
    i.text=text; i.imageTrack=3; i.type=interactive ? 1 : 0; i.action="";
    return i;
}
itemDef_s& Copy(Page& p,const itemDef_s& source,float dy=0) {
    auto& i=Item(p,"","",0,0,0);i=source;i.parent=&p.menu;
    i.window.rect.y+=dy;i.window.rectClient.y+=dy;
    i.window.dynamicFlags[0]&=~3;i.textRect[0]={};
    return i;
}
void StaticLayout(itemDef_s& i) {
    i.visibleExp={};i.textExp={};i.rectXExp={};i.rectYExp={};i.rectWExp={};i.rectHExp={};i.forecolorAExp={};
    i.onKey=nullptr;i.mouseEnter=i.mouseExit=i.mouseEnterText=i.mouseExitText=nullptr;
    i.onFocus=i.leaveFocus=i.onAccept=nullptr;i.dvarFlags=0;
    i.window.dynamicFlags[0]=4|0x10000;
}
itemDef_s& NativeRow(Page& p,const char* name,const char* label,int row,bool navigation=false,bool selected=false) {
    const auto* menu=navigation?nativeStyle.options:nativeStyle.graphics;
    const int valueIndex=navigation?14:nativeStyle.choice;
    const int backgroundIndex=navigation?10:valueIndex-5;
    const float dy=row*(navigation?24.f:22.f);
    itemDef_s* highlights[2]{};
    for(int n=0;n<4;++n) {
        auto& bg=Copy(p,*menu->items[backgroundIndex+n],dy);StaticLayout(bg);
        if(n>=2) highlights[n-2]=&bg;
    }
    if(!navigation) {
        auto& text=Copy(p,*menu->items[valueIndex-1],dy);StaticLayout(text);
        text.window.name="live_label";text.text=label;
    }
    auto& value=Copy(p,*menu->items[valueIndex],dy);StaticLayout(value);
    value.window.name=name;value.text=navigation?label:"";
    value.action="play mouse_click ;";value.onFocus="play mouse_over ;";
    value.window.staticFlags&=~0x100000;
    for(auto* bg:highlights) p.highlights.push_back({bg,&value,selected});
    return value;
}
void Init(Page& p,const char* name,const char* title,const char* first,bool navigation=true) {
    p=Page{}; p.menu.window.name=name;
    p.menu.window.rect=p.menu.window.rectClient={0,0,640,480,0,0};
    p.menu.window.style=1;
    Color(p.menu.window.foreColor,1,1,1); Color(p.menu.window.backColor,.045f,.06f,.05f,.98f);
    Color(p.menu.focusColor,.94f,.87f,.50f); Color(p.menu.disableColor,.4f,.4f,.4f);
    p.menu.fullScreen=1; p.menu.imageTrack=3; p.menu.fadeClamp=1;
    p.menu.fadeAmount=p.menu.fadeInAmount=.1f; p.menu.fadeCycle=1;
    p.menu.items=p.pointers.data(); p.menu.cursorItem[0]=-1;
    p.escape="execnow controller_back ;"; p.menu.onESC=p.escape.c_str();
    p.focus="setfocus "+std::string(first)+" ;"; p.menu.onOpen=p.focus.c_str();
    p.native=nativeStyle.options&&nativeStyle.graphics&&nativeStyle.choice>=5;
    if(p.native) {
        p.menu.window.style=0;
        // Keep underlying team/pause menu text out of this settings page.
        p.menu.fullScreen=1;
        // Draw the player's own CoD4 materials and the same frame/row assets as
        // Video Settings. These copies never enter or mutate the asset database.
        if(nativeStyle.background) for(int i=0;i<std::min(10,nativeStyle.background->itemCount);++i) Copy(p,*nativeStyle.background->items[i]);
        for(int i=0;i<8;++i) Copy(p,*nativeStyle.options->items[i]);
        auto& heading=Copy(p,*nativeStyle.options->items[9]);StaticLayout(heading);heading.text="Controller";
        auto& back=Copy(p,*nativeStyle.options->items[8]);StaticLayout(back);
        back.window.name="live_back";back.text="@MENU_BACK";back.action=p.escape.c_str();
        auto& section=Item(p,"live_section",title,350,12,280);
        section.fontEnum=heading.fontEnum;section.textscale=.4f;section.textAlignMode=1;
        if(navigation) {
            constexpr const char* names[]{"live_controller","live_controller_response","live_controller_sticks","live_controller_timing","live_controller_bindings"};
            constexpr const char* labels[]{"General","Look / Sensitivity","Sticks / Triggers","Menu Scroll","Button Mapping"};
            for(int n=0;n<5;++n) {
                p.navigation[n]="close "+std::string(name)+" ; open "+names[n]+" ;";
                auto& link=NativeRow(p,names[n],labels[n],n,true,!std::strcmp(name,names[n]));
                link.action=p.navigation[n].c_str();
            }
        }
        Item(p,"live_status",deviceStatus,24,200,210).textscale=.25f;
        Item(p,"live_backend_status",backendStatus,24,235,210).textscale=.25f;
        Item(p,"live_footer",footer,200,448,410).textscale=.23f;
        return;
    }
    auto& heading=Item(p,"live_heading",title,48,30,544); heading.textscale=.56f;
    Color(heading.window.foreColor,.94f,.87f,.5f);
    Item(p,"live_status",deviceStatus,48,66,544).textscale=.25f;
    Item(p,"live_footer",footer,48,445,544).textscale=.26f;
    auto& back=Item(p,"live_back","BACK",48,407,120,true); back.action=p.escape.c_str();
}
itemDef_s& Label(Page& p,const char* name,const char* label,int row) {
    if(p.native) return NativeRow(p,name,label,row);
    Item(p,"live_label",label,64,100+row*27.f,285);
    return Item(p,name,"",355,100+row*27.f,240,true);
}
void Strings(Page& p,const char* name,const char* label,int row,const char* dvar,const char* const* labels,const char* const* values,int count) {
    auto& i=Label(p,name,label,row); auto& choice=p.choices[p.choiceCount++];
    i.type=i.dataType=12; i.dvar=dvar; i.typeData.multi=&choice; choice.strDef=1; choice.count=count;
    for(int n=0;n<count;++n) { choice.dvarList[n]=labels[n]; choice.dvarStr[n]=values[n]; }
}
void Floats(Page& p,const char* name,const char* label,int row,const char* dvar,const char* const* labels,const float* values,int count) {
    auto& i=Label(p,name,label,row); auto& choice=p.choices[p.choiceCount++];
    i.type=i.dataType=12; i.dvar=dvar; i.typeData.multi=&choice; choice.count=count;
    for(int n=0;n<count;++n) { choice.dvarList[n]=labels[n]; choice.dvarValue[n]=values[n]; }
}
void Toggle(Page& p,const char* name,const char* label,int row,const char* dvar) {
    constexpr const char* labels[]{"Off","On"}; constexpr float values[]{0,1};
    Floats(p,name,label,row,dvar,labels,values,2);
}
void Slider(Page& p,const char* name,const char* label,int row,const char* dvar,float minimum,float maximum,float def) {
    auto& i=Label(p,name,label,row); auto& slider=p.sliders[p.sliderCount++];
    i.type=i.dataType=10; i.dvar=dvar; i.typeData.editField=&slider; i.textaligny=3;
    if(p.native&&nativeStyle.slider>=0) {
        const auto* native=nativeStyle.graphics->items[nativeStyle.slider];
        i.textalignx=native->textalignx;i.textaligny=native->textaligny;i.textAlignMode=native->textAlignMode;
    }
    slider.minVal=minimum; slider.maxVal=maximum; slider.defVal=def;
    const int value=p.valueCount++; p.valueDvars[value]=dvar;
    if(!p.native) Item(p,name,p.values[value].data(),470,100+row*27.f,120);
}
void Link(Page& p,const char* name,const char* label,int row,const char* action) {
    if(p.native) {
        auto& i=NativeRow(p,name,label,row);i.type=i.dataType=1;i.dvar=nullptr;i.typeData={};i.text="Select";i.action=action;return;
    }
    Item(p,name,label,64,100+row*27.f,520,true).action=action;
}
void Append(UiContext* dc,menuDef_t* menu) {
    for(int i=0;i<dc->menuCount;++i) if(dc->Menus[i]==menu) return;
    if (dc->menuCount >= static_cast<int>(std::size(dc->Menus))) Com_Error(ERR_DROP,"Too many native cod4-live menus");
    // Runtime additions are not DB assets; UI_AddMenu's fastfile identity check
    // intentionally applies only to loaded retail assets.
    dc->Menus[dc->menuCount++]=menu;
}
bool Contains(const char* text,const char* fragment) {
    if (!text) return false;
    std::string lower(text);
    for(auto& c:lower) if(c>='A'&&c<='Z') c+='a'-'A';
    return lower.find(fragment)!=std::string::npos;
}
void Extend(menuDef_t* menu,bool controller,bool bots) {
    // The code and in-game menu lists can refer to the same DB menu asset.
    // Extending it twice saves our first allocation as the "original"; UI
    // shutdown would then leave that asset pointing at freed item storage.
    if(std::any_of(extensions.begin(),extensions.end(),[menu](const Extension& extension) { return extension.menu==menu; })) {
        Com_DPrintf(CON_CHANNEL_UI,"Controller menu already extended: %s\n",menu->window.name);
        return;
    }
    Extension extension; extension.menu=menu; extension.original=menu->items; extension.originalCount=menu->itemCount;
    const itemDef_s* capacity=nullptr;
    const itemDef_s* controls=nullptr;
    for(int i=0;i<menu->itemCount;++i)
    {
        const auto* item=menu->items[i];
        if(item->dvar&&!I_stricmp(item->dvar,"sv_maxclients")) capacity=item;
        if(controller&&!I_stricmp(menu->window.name,"main_text")&&Contains(item->action,"main_controls")) controls=item;
    }
    const bool inlineBots=bots&&capacity;
    auto copy=[&](const itemDef_s* source,float dy) {
        auto item=std::make_unique<itemDef_s>(*source);
        item->window.rect.y+=dy; item->window.rectClient.y+=dy;
        item->window.dynamicFlags[0]&=~3;
        item->textRect[0]={};
        auto* result=item.get();
        extension.items.push_back(std::move(item));
        return result;
    };
    auto copyHighlight=[&](itemDef_s* item,const itemDef_s* source,int oldIndex,int newIndex) {
        if(!source->visibleExp.numEntries) return;
        auto expr=std::make_unique<Extension::Expression>();
        for(int e=0;e<source->visibleExp.numEntries;++e) {
            auto entry=*source->visibleExp.entries[e];
            if(entry.type==1&&entry.data.operand.dataType==VAL_INT&&entry.data.operand.internals.intVal==oldIndex)
                entry.data.operand.internals.intVal=newIndex;
            expr->entries.push_back(entry);
        }
        for(auto& entry:expr->entries) expr->pointers.push_back(&entry);
        item->visibleExp={static_cast<int>(expr->pointers.size()),expr->pointers.data()};
        extension.expressions.push_back(std::move(expr));
    };
    auto appendControllerRow=[&] {
        for(int n=0;n<menu->itemCount;++n) {
            const auto* source=menu->items[n];
            if(source->window.rectClient.horzAlign!=controls->window.rectClient.horzAlign||
                source->window.rectClient.y!=controls->window.rectClient.y||
                source->window.rectClient.x<0||source->window.rectClient.x>=controls->window.rectClient.w) continue;
            auto* item=copy(source,0);item->rectYExp={};
            // The pinned main menu uses highlight 7 for Controls. Preserve the
            // stock row's normal/highlight expressions with a new row identity.
            copyHighlight(item,source,7,64);
            if(source==controls) {
                item->window.name="live_controller_entry";
                item->text="Controller";item->textExp={};
                item->action="play mouse_click ; open live_controller ;";
                item->onFocus="play mouse_over ; setLocalVarInt ui_highlight 64 ; setLocalVarString ui_choicegroup main ;";
            }
            extension.pointers.push_back(item);
        }
    };
    auto appendBotRows=[&] {
        for(int row=0;row<3;++row) {
            const int highlight=64+row;
            auto focus=std::make_unique<std::string>("play mouse_over ; setLocalVarInt ui_highlight "+
                std::to_string(highlight)+" ; setLocalVarString ui_choicegroup "+menu->window.name+" ;");
            const char* onFocus=focus->c_str(); extension.scripts.push_back(std::move(focus));
            // Copy the six stock row parts in their original order: normal
            // gray background, highlight, label, then the value control.
            for(int n=0;n<menu->itemCount;++n) {
                const auto* source=menu->items[n];
                if(source->window.rectClient.horzAlign!=capacity->window.rectClient.horzAlign||
                    source->window.rectClient.y!=capacity->window.rectClient.y||
                    source->window.rectClient.x<0||source->window.rectClient.x>=capacity->window.rectClient.w) continue;
                auto* item=copy(source,24.f*(row+1));
                item->rectYExp={};
                if(source==capacity) {
                    item->window.name=row==2?"live_host_map":row?"live_bot_difficulty":"live_bot_count";
                    item->onFocus=onFocus;
                    if(row==2) host_ui::ConfigureMapItem(item);
                    else bots_ui::ConfigureBotItem(item,row!=0);
                } else if(source->textExp.numEntries||(source->text&&*source->text)) {
                    item->window.name=row==2?"live_host_map_label":row?"live_bot_difficulty_label":"live_bot_count_label";
                    item->text=row==2?"MAP":row?"BOT DIFFICULTY":bots_ui::CountLabel();
                    item->textExp={};
                    item->window.staticFlags|=0x100000;
                } else if(source->visibleExp.numEntries) {
                    // Retail highlight expressions compare ui_highlight to the
                    // source row number. Own the copy and give each new row its
                    // own index; never mutate the fastfile expression storage.
                    auto expr=std::make_unique<Extension::Expression>();
                    for(int e=0;e<source->visibleExp.numEntries;++e) {
                        auto entry=*source->visibleExp.entries[e];
                        if(entry.type==1&&entry.data.operand.dataType==VAL_INT)
                            entry.data.operand.internals.intVal=highlight;
                        expr->entries.push_back(entry);
                    }
                    for(auto& entry:expr->entries) expr->pointers.push_back(&entry);
                    item->visibleExp={static_cast<int>(expr->pointers.size()),expr->pointers.data()};
                    extension.expressions.push_back(std::move(expr));
                }
                extension.pointers.push_back(item);
            }
        }
    };
    for(int n=0;n<menu->itemCount;++n) {
        auto* original=menu->items[n];
        auto* item=original;
        if(controls&&original->window.rectClient.horzAlign==controls->window.rectClient.horzAlign&&
            original->window.rectClient.y>=120&&original->window.rectClient.y<=controls->window.rectClient.y&&
            original->window.rectClient.x>=0&&original->window.rectClient.x<300) {
            // Use the spare space above the list, keeping Quit and the native
            // footer at their original positions after inserting a full row.
            item=copy(original,-24);item->rectYExp={};
        } else if(original==capacity) {
            item=copy(original,0);
            bots_ui::ConfigureCapacityItem(item);
        } else if(inlineBots&&original->window.rectClient.horzAlign==capacity->window.rectClient.horzAlign&&
            original->window.rectClient.vertAlign==capacity->window.rectClient.vertAlign&&
            original->window.rectClient.y>capacity->window.rectClient.y&&
            original->window.rectClient.y<=400&&original->window.rectClient.x>=0&&
            original->window.rectClient.x<capacity->window.rectClient.w) {
            item=copy(original,72);
        }
        extension.pointers.push_back(item);
        if(original==controls) appendControllerRow();
        if(inlineBots&&original==capacity) appendBotRows();
    }
    // Controller navigation retains its existing narrow right-column entry.
    auto add=[&](const char* name,const char* text,const char* action,float y) {
        auto item=std::make_unique<itemDef_s>(); auto& i=*item;
        i.parent=menu; i.window.name=name;
        // A wide hitbox extends across the Host Game map list despite the
        // short visible label. Keep the click target within its own column.
        i.window.rect=i.window.rectClient={380-menu->window.rect.x,y-menu->window.rect.y,96,26,0,0};
        i.window.dynamicFlags[0]=4|0x10000; Color(i.window.foreColor,.88f,.89f,.81f);
        Color(i.window.outlineColor,0,0,0,.8f); i.type=1; i.fontEnum=4; i.textscale=.34f;
        i.textStyle=3; i.textaligny=20; i.text=text; i.action=action; i.imageTrack=3;
        extension.pointers.push_back(item.get()); extension.items.push_back(std::move(item));
    };
    if(controller&&!controls) add("live_controller_entry","CONTROLLER","open live_controller ;",355);
    add("live_controller_help",footer,"",453);
    auto& help=*extension.items.back();
    help.window.staticFlags=0x100000;
    help.window.rect=help.window.rectClient={32-menu->window.rect.x,453-menu->window.rect.y,576,25,0,0};
    help.textscale=.25f;
    menu->itemCount=static_cast<int>(extension.pointers.size()); menu->items=extension.pointers.data();
    extensions.push_back(std::move(extension));
}
void Inspect(UiContext* dc) {
    while(inspected<dc->menuCount) {
        auto* m=dc->Menus[inspected++]; const char* n=m->window.name;
        if(!n||!std::strncmp(n,"live_",5)) continue;
        bool options=false,host=false,pause=false;
        for(int i=0;i<m->itemCount;++i) {
            const auto* item=m->items[i]; if(!item->action||!*item->action) continue;
            options |= Contains(item->text,"menu_options")||Contains(item->text,"menu_controls")||Contains(item->text,"options_controls");
            host |= Contains(item->text,"start_new_server")||Contains(item->text,"startserver")||Contains(item->action,"startserver");
            pause |= Contains(item->text,"resume_game")||Contains(item->text,"endgame")||Contains(item->action,"closeingame");
        }
        host |= Contains(n,"create_server")||Contains(n,"createserver")||Contains(n,"startserver");
        const auto links=controller::NativeMenuLinks(n,Contains(m->onOpen,"main_text"),options,host,pause);
        // Controller entries remain on the existing navigation pages; bot
        // settings are inline only on Host Game, alongside its match settings.
        Extend(m,links.controller,links.bots);
    }
}
const char* ActionName(const char* command) {
    struct Entry {const char* command;const char* label;};
    constexpr Entry names[]{{"+gostand","Jump / stand"},{"+stance","Change stance"},{"+usereload","Use / reload"},
        {"weapnext","Change weapon"},{"+attack","Fire"},{"+speed_throw","Aim down sight"},{"+toggleads_throw","Toggle aim down sight"},
        {"+frag","Frag grenade"},{"+smoke","Special grenade"},{"+breath_sprint","Sprint / hold breath"},{"+melee","Melee"},
        {"+scores","Scoreboard"},{"togglemenu","Pause menu"},{"+actionslot 1","Action slot 1"},{"+actionslot 2","Action slot 2"},
        {"+actionslot 3","Action slot 3"},{"+actionslot 4","Action slot 4"}};
    for(const auto& e:names) if(command&&!std::strcmp(command,e.command)) return e.label;
    return command&&*command?command:"Unbound";
}
}
void BeforeInitialize() {
    for(auto& extension:extensions) { extension.menu->items=extension.original; extension.menu->itemCount=extension.originalCount; }
    extensions.clear(); inspected=0; context=nullptr;
}
static void BuildPages(UiContext* dc) {
    nativeStyle={};
    nativeStyle.background=Menus_FindByName(dc,"main_text");
    nativeStyle.options=Menus_FindByName(dc,"main_options");
    nativeStyle.graphics=Menus_FindByName(dc,"options_graphics");
    if(nativeStyle.options&&nativeStyle.options->itemCount<15) nativeStyle.options=nullptr;
    if(nativeStyle.graphics) for(int i=0;i<nativeStyle.graphics->itemCount;++i) {
        const auto* item=nativeStyle.graphics->items[i];
        if(item->dvar&&!I_stricmp(item->dvar,"ui_r_mode")) nativeStyle.choice=i;
        if(item->dvar&&!I_stricmp(item->dvar,"r_gamma")) nativeStyle.slider=i;
    }
    pendingBackend=Dvar_RegisterInt("ui_live_gpadBackend",Dvar_GetInt("gpad_backend"),0,1,DVAR_NOFLAG,"Pending input backend; apply to test and save");
    Dvar_SetInt(pendingBackend,backendSwitch.Effective(Dvar_GetInt("gpad_backend")));
    Init(general,"live_controller","General","controller_backend");
    constexpr const char* modes[]{"Native","Steam Input"}; constexpr float binary[]{0,1};
    Floats(general,"controller_backend","Input Backend",0,"ui_live_gpadBackend",modes,binary,2);
    Strings(general,"controller_buttons","Button Layout",1,"gpad_buttonConfig",layoutNames,layoutValues,8);
    Strings(general,"controller_sticks","Stick Layout",2,"gpad_sticksConfig",stickNames,stickValues,4);
    constexpr const char* prompts[]{"Automatic","PlayStation","Xbox"}; constexpr float styles[]{0,1,2};
    Floats(general,"controller_prompts","Button Prompts",3,"gpad_style",prompts,styles,3);
    Toggle(general,"controller_rumble","Vibration",4,"gpad_rumble");
    Slider(general,"controller_fov","Field of View Scale",5,"cg_fovScale",1,MaxFovScale,1);
    Link(general,"controller_apply","Apply Input Backend",6,"open live_controller_backend_confirm ;");
    if(!general.native) {
        Link(general,"controller_response","Look / Sensitivity",7,"open live_controller_response ;");
        Link(general,"controller_deadzone","Sticks / Triggers",8,"open live_controller_sticks ;");
        Link(general,"controller_timing","Menu Scroll",9,"open live_controller_timing ;");
        Link(general,"controller_mapping","Button Mapping",10,"open live_controller_bindings ;");
    }
    Init(backendConfirm,"live_controller_backend_confirm","Apply Input Backend","backend_cancel",false);
    Item(backendConfirm,"backend_question","Test the selected input backend?",270,75,340).textscale=.30f;
    Item(backendConfirm,"backend_help","If no controller is detected within 15 seconds,",270,105,340).textscale=.26f;
    Item(backendConfirm,"backend_help2","the previous backend is restored automatically.",270,125,340).textscale=.26f;
    Link(backendConfirm,"backend_cancel","Cancel",7,"execnow controller_cancel_backend ; close live_controller_backend_confirm ;");
    Link(backendConfirm,"backend_apply","Apply",8,"execnow controller_apply_backend ; close live_controller_backend_confirm ;");
    backendConfirm.escape="execnow controller_cancel_backend ; close live_controller_backend_confirm ;";
    backendConfirm.menu.onESC=backendConfirm.escape.c_str();
    for(int i=0;i<backendConfirm.menu.itemCount;++i) {
        auto* item=backendConfirm.menu.items[i];
        if(item->window.name&&!std::strcmp(item->window.name,"live_back")) item->action=backendConfirm.escape.c_str();
    }
    Init(response,"live_controller_response","Look / Sensitivity","controller_preset");
    constexpr const char* presets[]{"1 (Low)","2 (Medium)","3","4 (High)","5","6","7 (Very high)","8","9","10 (Insane)"};
    constexpr float multipliers[]{.6f,1,1.4f,1.8f,2,2.2f,2.6f,3,3.5f,4};
    Floats(response,"controller_preset","Sensitivity Preset",0,"input_viewSensitivity",presets,multipliers,10);
    Slider(response,"controller_sensitivity","Look Sensitivity",1,"input_viewSensitivity",.05f,5,1);
    Slider(response,"controller_ads","ADS Sensitivity",2,"gpad_adsSensitivity",.25f,2,1);
    Toggle(response,"controller_invert","Invert Look",3,"input_invertPitch");
    Item(response,"controller_fixed","Aim assistance and response tuning are fixed for everyone.",64,270,520).textscale=.27f;
    Init(sticks,"live_controller_sticks","Sticks / Triggers","controller_inner");
    Slider(sticks,"controller_inner","Inner Deadzone",0,"gpad_stick_deadzone_min",0,.9f,.2f);
    Slider(sticks,"controller_outer","Outer Deadzone",1,"gpad_stick_deadzone_max",0,.5f,.01f);
    Slider(sticks,"controller_anti","Anti-Deadzone",2,"gpad_stick_anti_deadzone",0,.9f,0);
    Slider(sticks,"controller_press","Menu Stick Threshold",3,"gpad_stick_pressed",.05f,1,.4f);
    Slider(sticks,"controller_hysteresis","Menu Hysteresis",4,"gpad_stick_pressed_hysteresis",0,.5f,.1f);
    Slider(sticks,"controller_trigger","Trigger Threshold",5,"gpad_button_deadzone",0,.9f,.13f);
    Slider(sticks,"controller_trigger_hysteresis","Trigger Hysteresis",6,"gpad_button_deadzone_hysteresis",0,.5f,.05f);
    Init(timing,"live_controller_timing","Menu Scroll","controller_repeat_first");
    Slider(timing,"controller_repeat_first","First Delay (ms)",0,"gpad_menu_scroll_delay_first",0,1000,420);
    Slider(timing,"controller_repeat_rest","Repeat Delay (ms)",1,"gpad_menu_scroll_delay_rest",0,1000,210);
    Slider(timing,"controller_repeat_min","Fastest Repeat (ms)",2,"gpad_menu_scroll_delay_min",0,1000,50);
    Slider(timing,"controller_repeat_accel","Acceleration (ms)",3,"gpad_menu_scroll_accel_time",0,5000,1500);
    Init(bindings,"live_controller_bindings","Button Mapping","live_back");
    for(int i=0;i<16;++i) {
        if(bindings.native) {
            auto& value=NativeRow(bindings,"controller_binding",bindingLabels[i].data(),i);
            value.type=value.dataType=0;value.dvar=nullptr;value.typeData={};value.text=bindingText[i].data();
            value.window.staticFlags|=0x100000;value.onFocus=nullptr;value.action=nullptr;
        } else Item(bindings,"controller_binding",bindingText[i].data(),48+(i/8)*290.f,107+(i%8)*34.f,285).textscale=.30f;
    }
    for(auto* p:{&general,&response,&sticks,&timing,&bindings,&backendConfirm}) Append(dc,&p->menu);
}
void Initialize(UiContext* dc) {
    context=dc;
    controller_glyph::Initialize();
    BuildPages(dc);
    bots_ui::Initialize();
    host_ui::Initialize();
    Refresh(dc);
}
void MenusLoaded(UiContext* dc) {
    // In-game settings arrive after UI_Init's code.txt menus. Rebuild our
    // still-closed pages once those native templates are available, retaining
    // the same menu addresses and a single entry per page in the context.
    if(context==dc&&!general.native&&Menus_FindByName(dc,"options_graphics")) BuildPages(dc);
}
void Refresh(UiContext* dc) {
    if(!context) return;
    bots_ui::Refresh(); host_ui::Refresh(); Inspect(dc);
    std::snprintf(deviceStatus,sizeof(deviceStatus),"%s",Dvar_GetBool("gpad_present")?"Controller connected":"Connect a controller to play");
    const unsigned now=Sys_Milliseconds();
    if(backendSwitch.pending) std::snprintf(backendStatus,sizeof(backendStatus),"Testing input: %u seconds",(15000-std::min(15000u,now-backendSwitch.started)+999)/1000);
    else if(!backendNoticeUntil||static_cast<int>(now-backendNoticeUntil)>=0) {
        if(pendingBackend->current.integer!=Dvar_GetInt("gpad_backend")) std::snprintf(backendStatus,sizeof(backendStatus),"Backend change needs Apply");
        else std::snprintf(backendStatus,sizeof(backendStatus),"Active: %s",Dvar_GetInt("gpad_backend")?"Steam Input":"Native");
    }
    for(auto* p:{&general,&response,&sticks,&timing,&bindings,&backendConfirm}) for(const auto& h:p->highlights) {
        if(h.selected||(h.control->window.dynamicFlags[0]&2)) h.background->window.dynamicFlags[0]|=4;
        else h.background->window.dynamicFlags[0]&=~4;
    }
    if(controller::InUse()) std::snprintf(footer,sizeof(footer),"%s Select   %s Back   %s %s Adjust   %s %s Change control",controller::KeyName(1,true),controller::KeyName(2,true),controller::KeyName(22,true),controller::KeyName(23,true),controller::KeyName(5,true),controller::KeyName(6,true));
    else std::snprintf(footer,sizeof(footer),"Enter: Select    Escape: Back    Left / Right: Adjust");
    for(auto* p:{&general,&response,&sticks,&timing}) for(int i=0;i<p->valueCount;++i) {
        const auto* value=Dvar_FindVar(p->valueDvars[i]);
        if(value && value->type==DVAR_TYPE_INT)
            std::snprintf(p->values[i].data(),p->values[i].size(),"%d",value->current.integer);
        else
            std::snprintf(p->values[i].data(),p->values[i].size(),"%.2f",value?value->current.value:0.f);
    }
    auto& sensitivityPresets=response.choices[0];
    sensitivityPresets.count=10;
    const float sensitivity=Dvar_GetFloat("input_viewSensitivity");
    bool preset=false;
    for(int i=0;i<10;++i) preset |= sensitivityPresets.dvarValue[i]==sensitivity;
    if(!preset) {
        sensitivityPresets.dvarList[10]="Custom";
        sensitivityPresets.dvarValue[10]=sensitivity;
        sensitivityPresets.count=11;
    }
    constexpr int keys[]{1,2,3,4,18,19,5,6,16,17,14,15,20,21,22,23};
    for(int i=0;i<16;++i) {
        const char* action=ActionName(Key_GetBinding(0,keys[i]));
        std::snprintf(bindingLabels[i].data(),bindingLabels[i].size(),"%s",action);
        std::snprintf(bindingText[i].data(),bindingText[i].size(),bindings.native?"%s":"%s  %s",controller::KeyName(keys[i],true),action);
    }
}
void ConfirmBackend() {
    if(!pendingBackend) return;
    if(backendSwitch.Begin(Dvar_GetInt("gpad_backend"),pendingBackend->current.integer,Sys_Milliseconds())) backendNoticeUntil=0;
}
void CancelBackend() {
    backendSwitch.pending=false;
    if(pendingBackend) Dvar_SetInt(pendingBackend,Dvar_GetInt("gpad_backend"));
    backendNoticeUntil=0;
}
int RequestedBackend() { return backendSwitch.Effective(Dvar_GetInt("gpad_backend")); }
void ObserveBackend(bool present) {
    const auto result=backendSwitch.Observe(Dvar_GetInt("gpad_backend"),present,Sys_Milliseconds());
    if(result==controller::BackendSwitch::Result::None) return;
    if(result==controller::BackendSwitch::Result::Commit) {
        Dvar_SetIntByName("gpad_backend",backendSwitch.target);
        std::snprintf(backendStatus,sizeof(backendStatus),"Input backend applied");
    } else std::snprintf(backendStatus,sizeof(backendStatus),"Previous backend restored");
    if(pendingBackend) Dvar_SetInt(pendingBackend,Dvar_GetInt("gpad_backend"));
    backendNoticeUntil=Sys_Milliseconds()+8000;
    Com_Printf(CON_CHANNEL_UI,"%s\n",backendStatus);
}
void Open() {
    if(!context) return;
    if(!Key_IsCatcherActive(0,KEYCATCH_UI)) UI_SetActiveMenu(0,CL_GetLocalClientConnectionState(0)==CA_ACTIVE?UIMENU_INGAME:UIMENU_MAIN);
    Menus_OpenByName(context,"live_controller");
}
void Back() {
    if(!context) return;
    auto* menu=Menu_GetFocused(context);
    if(!menu) return;
    for(auto* p:{&general,&response,&sticks,&timing,&bindings}) {
        if(menu!=&p->menu) continue;
        if(p->native) {
            itemDef_s* category=nullptr;
            int categoryIndex=-1;
            bool inSidebar=false;
            for(int i=0;i<menu->itemCount;++i) {
                auto* item=menu->items[i];
                const char* name=item->window.name;
                if(!name) continue;
                if(!std::strcmp(name,menu->window.name)) { category=item;categoryIndex=i; }
                if((item->window.dynamicFlags[0]&2)&&
                    (!std::strncmp(name,"live_controller",15)||!std::strcmp(name,"live_back"))) inSidebar=true;
            }
            if(!inSidebar&&category&&Item_SetFocus(context,category,category->window.rect.x,category->window.rect.y)) {
                menu->cursorItem[context->localClientNum]=categoryIndex;
                return;
            }
        }
        Menus_CloseByName(context,menu->window.name);
        return;
    }
}
bool IsOpen() {
    const char* menu=UI_GetTopActiveMenuName(0);
    return context&&menu&&!std::strncmp(menu,"live_controller",15);
}
void Dump() {
    if(!context) { Com_Printf(CON_CHANNEL_UI,"Controller UI is not initialized.\n"); return; }
    const char* filter=Cmd_Argc()>1?Cmd_Argv(1):nullptr;
    int menus=0,items=0;
    Com_Printf(CON_CHANNEL_UI,"Controller menu dump: %d loaded, %d open; filter=%s\n",context->menuCount,context->openMenuCount,filter?filter:"open menus");
    for(int n=0;n<context->menuCount && menus<32 && items<256;++n) {
        const auto* menu=context->Menus[n];
        bool open=false;
        for(int i=0;i<context->openMenuCount;++i) open|=context->menuStack[i]==menu;
        if(filter ? !Contains(menu->window.name,filter) : !open) continue;
        ++menus;
        Com_Printf(CON_CHANNEL_UI,"menu[%d] %s open=%d flags=%x fullscreen=%d rect=%.0f %.0f %.0f %.0f items=%d\n",n,menu->window.name?menu->window.name:"",open,menu->window.dynamicFlags[0],menu->fullScreen,menu->window.rect.x,menu->window.rect.y,menu->window.rect.w,menu->window.rect.h,menu->itemCount);
        for(int i=0;i<menu->itemCount && items<256;++i,++items) {
            const auto* item=menu->items[i];
            Com_Printf(CON_CHANNEL_UI,"  [%d] %.64s type=%d flags=%x rect=%.0f %.0f %.0f %.0f text=%.120s action=%.240s\n",i,item->window.name?item->window.name:"",item->type,item->window.dynamicFlags[0],item->window.rect.x,item->window.rect.y,item->window.rect.w,item->window.rect.h,item->text?item->text:"",item->action?item->action:"");
            Com_Printf(CON_CHANNEL_UI,"       static=%x dvar=%.64s focus=%.180s\n",item->window.staticFlags,item->dvar?item->dvar:"",item->onFocus?item->onFocus:"");
        }
    }
    Com_Printf(CON_CHANNEL_UI,"Controller menu dump end: %d menus, %d items (limit 32/256).\n",menus,items);
}
bool HandlesKey(int key) {
    if(!context) return false;
    const auto* menu=Menu_GetFocused(context);
    if(!menu) return false;
    auto handles=[key](const ItemKeyHandler* entry) {
        for(;entry;entry=entry->next) if(entry->key==key) return true;
        return false;
    };
    if(handles(menu->onKey)) return true;
    for(int i=0;i<menu->itemCount;++i) {
        const auto* item=menu->items[i];
        if((item->window.dynamicFlags[0]&6)==6 && handles(item->onKey)) return true;
    }
    return false;
}
}
