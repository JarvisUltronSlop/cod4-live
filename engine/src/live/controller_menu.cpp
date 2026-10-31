#include "controller_menu.h"
#include "controller.h"
#include "controller_glyph.h"
#include "bot_menu.h"
#include "input_policy.h"
#include <universal/q_shared.h>
#include <client_mp/client_mp.h>
#include <client/client.h>
#include <ui/ui_shared.h>
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
    std::array<itemDef_s, 64> rows{};
    std::array<itemDef_s*, 64> pointers{};
    std::array<multiDef_s, 16> choices{};
    std::array<editFieldDef_s, 16> sliders{};
    std::array<std::array<char, 64>, 16> values{};
    std::array<const char*, 16> valueDvars{};
    std::string escape, focus;
    int choiceCount{}, sliderCount{}, valueCount{};
};
Page general, response, sticks, timing, bindings;
UiContext* context{};
int inspected{};
char footer[256]{}, deviceStatus[128]{};
std::array<std::array<char, 160>, 16> bindingText{};
struct Extension {
    menuDef_t* menu{};
    itemDef_s** original{};
    int originalCount{};
    std::vector<itemDef_s*> pointers;
    std::vector<std::unique_ptr<itemDef_s>> items;
};
std::vector<Extension> extensions;
constexpr const char* layoutValues[]{"buttons_default", "buttons_tactical", "buttons_lefty", "buttons_nomad",
    "buttons_default_alt", "buttons_tactical_alt", "buttons_lefty_alt", "buttons_nomad_alt"};
constexpr const char* layoutNames[]{"Default", "Tactical", "Lefty", "Nomad", "Default flipped", "Tactical flipped", "Lefty flipped", "Nomad flipped"};
constexpr const char* stickValues[]{"thumbstick_default", "thumbstick_southpaw", "thumbstick_legacy", "thumbstick_legacysouthpaw"};
constexpr const char* stickNames[]{"Default", "Southpaw", "Legacy", "Legacy southpaw"};
void Color(float (&c)[4], float r, float g, float b, float a = 1) { c[0]=r; c[1]=g; c[2]=b; c[3]=a; }
itemDef_s& Item(Page& p, const char* name, const char* text, float x, float y, float width, bool interactive=false) {
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
void Init(Page& p,const char* name,const char* title,const char* first) {
    p=Page{}; p.menu.window.name=name;
    p.menu.window.rect=p.menu.window.rectClient={0,0,640,480,0,0};
    p.menu.window.style=1;
    Color(p.menu.window.foreColor,1,1,1); Color(p.menu.window.backColor,.045f,.06f,.05f,.98f);
    Color(p.menu.focusColor,.94f,.87f,.50f); Color(p.menu.disableColor,.4f,.4f,.4f);
    p.menu.fullScreen=1; p.menu.imageTrack=3; p.menu.fadeClamp=1;
    p.menu.fadeAmount=p.menu.fadeInAmount=.1f; p.menu.fadeCycle=1;
    p.menu.items=p.pointers.data(); p.menu.cursorItem[0]=-1;
    p.escape="close "+std::string(name)+" ;"; p.menu.onESC=p.escape.c_str();
    p.focus="setfocus "+std::string(first)+" ;"; p.menu.onOpen=p.focus.c_str();
    auto& heading=Item(p,"live_heading",title,48,30,544); heading.textscale=.56f;
    Color(heading.window.foreColor,.94f,.87f,.5f);
    Item(p,"live_status",deviceStatus,48,66,544).textscale=.25f;
    Item(p,"live_footer",footer,48,445,544).textscale=.26f;
    auto& back=Item(p,"live_back","BACK",48,407,120,true); back.action=p.escape.c_str();
}
itemDef_s& Label(Page& p,const char* name,const char* label,int row) {
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
    slider.minVal=minimum; slider.maxVal=maximum; slider.defVal=def;
    const int value=p.valueCount++; p.valueDvars[value]=dvar;
    Item(p,name,p.values[value].data(),470,100+row*27.f,120);
}
void Link(Page& p,const char* name,const char* label,int row,const char* action) {
    Item(p,name,label,64,100+row*27.f,520,true).action=action;
}
void Append(UiContext* dc,menuDef_t* menu) {
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
    Extension extension; extension.menu=menu; extension.original=menu->items; extension.originalCount=menu->itemCount;
    extension.pointers.assign(menu->items,menu->items+menu->itemCount);
    // Leave retail item geometry untouched. Add adjacent native rows in the
    // unused right column; this also survives localized text and mod menus.
    auto add=[&](const char* name,const char* text,const char* action,float y,bool bot) {
        auto item=std::make_unique<itemDef_s>(); auto& i=*item;
        i.parent=menu; i.window.name=name;
        i.window.rect=i.window.rectClient={380-menu->window.rect.x,y-menu->window.rect.y,220,26,0,0};
        i.window.dynamicFlags[0]=4|0x10000; Color(i.window.foreColor,.88f,.89f,.81f);
        Color(i.window.outlineColor,0,0,0,.8f); i.type=1; i.fontEnum=4; i.textscale=.34f;
        i.textStyle=3; i.textaligny=20; i.text=text; i.action=action; i.imageTrack=3;
        if(bot) { i.dvarTest="ui_live_botControls"; i.enableDvar="1"; i.dvarFlags=1; }
        extension.pointers.push_back(item.get()); extension.items.push_back(std::move(item));
    };
    if(controller) add("live_controller_entry","CONTROLLER","open live_controller ;",355,false);
    if(bots) add("live_bots_entry","BOT WARFARE","open live_bot_settings ;",385,true);
    add("live_controller_help",footer,"",453,false);
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
        const bool main=!I_stricmp(n,"main");
        options |= !I_stricmp(n,"options")||!I_stricmp(n,"options_menu")||!I_stricmp(n,"options_multi")||!I_stricmp(n,"setup_menu2");
        host |= Contains(n,"create_server")||Contains(n,"createserver")||Contains(n,"startserver");
        // Every native menu receives the active-device footer. Main/options,
        // host and pause screens additionally receive their new selectable rows.
        Extend(m,main||options||pause,main||pause||host);
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
void Initialize(UiContext* dc) {
    context=dc;
    controller_glyph::Initialize();
    Init(general,"live_controller","CONTROLLER","controller_backend");
    constexpr const char* modes[]{"Native Xbox / PlayStation","Steam Input (XInput)"}; constexpr float binary[]{0,1};
    Floats(general,"controller_backend","INPUT BACKEND",0,"gpad_backend",modes,binary,2);
    Strings(general,"controller_buttons","BUTTON LAYOUT",1,"gpad_buttonConfig",layoutNames,layoutValues,8);
    Strings(general,"controller_sticks","STICK LAYOUT",2,"gpad_sticksConfig",stickNames,stickValues,4);
    constexpr const char* prompts[]{"Automatic","PlayStation","Xbox"}; constexpr float styles[]{0,1,2};
    Floats(general,"controller_prompts","BUTTON PROMPTS",3,"gpad_style",prompts,styles,3);
    Toggle(general,"controller_rumble","VIBRATION",4,"gpad_rumble");
    Slider(general,"controller_fov","FIELD OF VIEW SCALE",5,"cg_fovScale",1,MaxFovScale,1);
    Link(general,"controller_response","LOOK / SENSITIVITY",6,"open live_controller_response ;");
    Link(general,"controller_deadzone","STICKS / TRIGGERS",7,"open live_controller_sticks ;");
    Link(general,"controller_timing","MENU SCROLL",8,"open live_controller_timing ;");
    Link(general,"controller_mapping","VIEW BUTTON MAPPING",9,"open live_controller_bindings ;");
    Init(response,"live_controller_response","LOOK / SENSITIVITY","controller_preset");
    constexpr const char* presets[]{"1 (Low)","2 (Medium)","3","4 (High)","5","6","7 (Very high)","8","9","10 (Insane)"};
    constexpr float multipliers[]{.6f,1,1.4f,1.8f,2,2.2f,2.6f,3,3.5f,4};
    Floats(response,"controller_preset","IW4X SENSITIVITY PRESET",0,"input_viewSensitivity",presets,multipliers,10);
    Slider(response,"controller_sensitivity","CUSTOM LOOK SENSITIVITY",1,"input_viewSensitivity",.05f,5,1);
    Slider(response,"controller_ads","ADS SENSITIVITY",2,"gpad_adsSensitivity",.25f,2,1);
    Toggle(response,"controller_invert","INVERT LOOK",3,"input_invertPitch");
    Item(response,"controller_fixed","Aim assistance and response tuning are fixed for everyone.",64,270,520).textscale=.27f;
    Init(sticks,"live_controller_sticks","STICKS / TRIGGERS","controller_inner");
    Slider(sticks,"controller_inner","INNER STICK DEADZONE",0,"gpad_stick_deadzone_min",0,.9f,.2f);
    Slider(sticks,"controller_outer","OUTER STICK DEADZONE",1,"gpad_stick_deadzone_max",0,.5f,.01f);
    Slider(sticks,"controller_anti","STICK ANTI-DEADZONE",2,"gpad_stick_anti_deadzone",0,.9f,0);
    Slider(sticks,"controller_press","MENU STICK THRESHOLD",3,"gpad_stick_pressed",.05f,1,.4f);
    Slider(sticks,"controller_hysteresis","MENU STICK HYSTERESIS",4,"gpad_stick_pressed_hysteresis",0,.5f,.1f);
    Slider(sticks,"controller_trigger","TRIGGER THRESHOLD",5,"gpad_button_deadzone",0,.9f,.13f);
    Slider(sticks,"controller_trigger_hysteresis","TRIGGER HYSTERESIS",6,"gpad_button_deadzone_hysteresis",0,.5f,.05f);
    Init(timing,"live_controller_timing","MENU SCROLL","controller_repeat_first");
    Slider(timing,"controller_repeat_first","FIRST REPEAT DELAY (MS)",0,"gpad_menu_scroll_delay_first",0,1000,420);
    Slider(timing,"controller_repeat_rest","REPEAT DELAY (MS)",1,"gpad_menu_scroll_delay_rest",0,1000,210);
    Slider(timing,"controller_repeat_min","FASTEST REPEAT (MS)",2,"gpad_menu_scroll_delay_min",0,1000,50);
    Slider(timing,"controller_repeat_accel","ACCELERATION TIME (MS)",3,"gpad_menu_scroll_accel_time",0,5000,1500);
    Init(bindings,"live_controller_bindings","BUTTON MAPPING","live_back");
    for(int i=0;i<16;++i) Item(bindings,"controller_binding",bindingText[i].data(),48+(i/8)*290.f,107+(i%8)*34.f,285).textscale=.30f;
    for(auto* p:{&general,&response,&sticks,&timing,&bindings}) Append(dc,&p->menu);
    bots_ui::Initialize(); Append(dc,bots_ui::Menu());
    Refresh(dc);
}
void Refresh(UiContext* dc) {
    if(!context) return;
    bots_ui::Refresh(); Inspect(dc);
    std::snprintf(deviceStatus,sizeof(deviceStatus),"%s  |  %s",Dvar_GetBool("gpad_present")?"Controller connected":"Connect a controller to play",
        Dvar_GetInt("gpad_backend")?"Steam Input: use the Gamepad template":"Native controller support");
    if(controller::InUse()) std::snprintf(footer,sizeof(footer),"%s Select    %s Back    %s %s Adjust",controller::KeyName(1,true),controller::KeyName(2,true),controller::KeyName(22,true),controller::KeyName(23,true));
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
    for(int i=0;i<16;++i) std::snprintf(bindingText[i].data(),bindingText[i].size(),"%s  %s",controller::KeyName(keys[i],true),ActionName(Key_GetBinding(0,keys[i])));
}
void Open() {
    if(!context) return;
    if(!Key_IsCatcherActive(0,KEYCATCH_UI)) UI_SetActiveMenu(0,CL_GetLocalClientConnectionState(0)==CA_ACTIVE?UIMENU_INGAME:UIMENU_MAIN);
    Menus_OpenByName(context,"live_controller");
}
bool IsOpen() {
    const char* menu=UI_GetTopActiveMenuName(0);
    return context&&menu&&!std::strncmp(menu,"live_controller",15);
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
