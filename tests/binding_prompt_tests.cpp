#include <live/controller_prompt.h>
#include <live/controller_prompt_text.h>
#include <live/controller_glyph.h>
#include <live/iw4x/Mapping/Binding.hpp>
#include <live/iw4x/Mapping/Glyph.hpp>
#include <ui/keycodes.h>
#include <cstdarg>

// Only engine state, binding storage and localization are fixtures. Lookup,
// alias resolution, display-family selection, glyph encoding and UI formatting
// below use production code, including the actual eight vendored layouts.
static void Require(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static std::array<std::string,256> bindings;
const char* Key_GetBinding(int client,unsigned key) {
    Require(client==0 && key<bindings.size(),"binding query bounds");
    return bindings[key].empty()?nullptr:bindings[key].c_str();
}
void MyAssertHandler(const char*,int,int,const char*,...) { Require(false,"production assertion"); }
int I_stricmp(const char* a,const char* b) { return _stricmp(a,b); }
void I_strncpyz(char* output,const char* input,int size) {
    Require(size>0,"copy size"); std::snprintf(output,size,"%s",input);
}
void Com_sprintf(char* output,unsigned size,const char* format,...) {
    va_list args; va_start(args,format); std::vsnprintf(output,size,format,args); va_end(args);
}
const char* SEH_StringEd_GetString(const char* key) {
    if(!std::strcmp(key,"KEY_SPACE"))return "Space";
    if(!std::strcmp(key,"KEY_MOUSE1"))return "Mouse 1";
    return nullptr;
}
char* UI_SafeTranslateString(const char* key) {
    if(!std::strcmp(key,"KEY_OR"))return const_cast<char*>("or");
    if(!std::strcmp(key,"KEY_UNBOUND"))return const_cast<char*>("Unbound");
    return const_cast<char*>(key);
}
int SEH_GetCurrentLanguage() { return 0; }
struct keyname_t { const char* name; int keynum; };
static keyname_t keynames[]{{"SPACE",K_SPACE},{"MOUSE1",K_MOUSE1},{nullptr,0}};
static keyname_t keynames_localized[160]{{"KEY_SPACE",K_SPACE},{"KEY_MOUSE1",K_MOUSE1}};
static char tinystr[5];
#define qmemcpy std::memcpy
constexpr int CA_ACTIVE=9, KEYCATCH_UI=16, UIMENU_SCOREBOARD=7;
static int connectionState=CA_ACTIVE,catchers=0,activeMenu=0;
struct Connection { bool demoplaying{}; };
static Connection connection;
int CL_GetLocalClientConnectionState(int) { return connectionState; }
Connection* CL_GetLocalClientConnection(int) { return &connection; }
bool Key_IsCatcherActive(int,int mask) { return (catchers&mask)!=0; }
int UI_GetActiveMenu(int) { return activeMenu; }
namespace live::controller {
namespace mapping=Controller::mapping;
using mapping::engine_key;
static bool ready=true,connected=true,inUse=true;
static Controller::family activeFamily=Controller::family::xbox;
struct Style { struct { int integer{}; } current; };
static Style styleValue;
static Style* style=&styleValue;
bool InUse() { return ready&&inUse; }
}
#include "generated_binding_prompts.inc"

static void Layout(const char* name) {
    bindings.fill("");
    Controller::mapping::binding_table table;
    Controller::mapping::apply_button_layout(table,name);
    table.for_each([](auto key,const std::string& command) { bindings[static_cast<int>(key)]=command; });
    bindings['F']="+activate"; bindings['R']="+reload"; bindings[K_SPACE]="+gostand";
    bindings[K_MOUSE1]="+attack";
}
static void ExpectKey(const char* command,int expected) {
    int keys[2]{999,999};
    Require(Key_GetCommandAssignmentInternal(0,command,keys)==1 && keys[0]==expected && keys[1]==-1,command);
}
static std::string Display(const char* command,int expectedCount=1,bool single=false) {
    struct Guarded { char before[8]; char text[256]; char after[8]; } output;
    std::memset(&output,0x6A,sizeof(output));
    Require(GetKeyBindingLocalizedString(0,command,output.text,single)==expectedCount,"display binding count");
    for(char c:output.before)Require(c==0x6A,"display underflow");
    for(char c:output.after)Require(c==0x6A,"display overflow");
    Require(std::memchr(output.text,0,sizeof(output.text))!=nullptr,"display terminated");
    return output.text;
}
static void ExpectGlyph(const char* command,const char* material) {
    const auto text=Display(command);
    Require(text.size()>5 && text[0]=='^' && text[1]==3,"native named glyph encoding");
    Require(live::controller_glyph::Named(text.c_str()+1),"renderer accepts encoded prompt");
    Require(text.substr(5)==material,command);
    Require(live::controller_glyph::Bytes(text.c_str()+1)+1==text.size(),"glyph string length");
}
int main() {
    using namespace live::controller;
    struct Profile { const char* name; int fire,ads,frag,special,melee,sprint,stance; };
    const Profile profiles[]{
        {"buttons_default",19,18,6,5,17,16,2}, {"buttons_tactical",19,18,6,5,2,16,17},
        {"buttons_lefty",18,19,5,6,16,17,2}, {"buttons_nomad",19,18,6,5,2,16,17},
        {"buttons_default_alt",6,5,19,18,17,16,2}, {"buttons_tactical_alt",6,5,19,18,2,16,17},
        {"buttons_lefty_alt",5,6,18,19,16,17,2}, {"buttons_nomad_alt",6,5,19,18,2,16,17}};
    for(const auto& profile:profiles) {
        Layout(profile.name);
        ExpectKey("+activate",3); ExpectKey("+reload",3); ExpectKey("+usereload",3);
        ExpectKey("+attack",profile.fire); ExpectKey("+frag",profile.frag); ExpectKey("+smoke",profile.special);
        for(auto command:{"+speed","+speed_throw","+toggleads_throw","toggleads"}) ExpectKey(command,profile.ads);
        ExpectKey("+melee",profile.melee); ExpectKey("+sprint",profile.sprint);
        ExpectKey("+holdbreath",profile.sprint);
        int unmatched[2]{};
        Require(Key_GetCommandAssignmentInternal(0,"+melee_breath",unmatched)==0,
            "combined melee/breath query must not select a sprint/breath button");
        for(auto command:{"gocrouch","goprone","+prone","+movedown","raisestance","lowerstance","togglecrouch","toggleprone","+stance"}) ExpectKey(command,profile.stance);
        ExpectKey("+gostand",1); ExpectKey("+moveup",1); ExpectKey("weapnext",4);
        ExpectKey("togglescores",15); ExpectKey("+scores",15); ExpectKey("togglemenu",14);
        for(int i=1;i<=4;++i) ExpectKey(("+actionslot "+std::to_string(i)).c_str(),19+i);
        for(auto family:{Controller::family::xbox,Controller::family::dualsense}) {
            activeFamily=family;
            for(const auto& binding:bindings) if(!binding.empty()) {
                const auto text=Display(binding.c_str());
                Require(text.find("KEY_UNBOUND")==std::string::npos && text.starts_with("^\x03"),"all mapped profile actions display controller glyphs");
            }
        }
    }
    Layout("buttons_default"); activeFamily=Controller::family::xbox;
    ExpectGlyph("+activate","button_x"); ExpectGlyph("+gostand","button_a");
    activeFamily=Controller::family::dualsense;
    ExpectGlyph("+activate","button_ps3_square"); ExpectGlyph("+gostand","button_ps3_cross");
    styleValue.current.integer=2; ExpectGlyph("+activate","button_x");
    styleValue.current.integer=1; activeFamily=Controller::family::xbox; ExpectGlyph("+activate","button_ps3_square");
    styleValue.current.integer=0;
    Require(!std::strcmp(KeyName(3,false),"BUTTON_X"),"config key names are text");
    Require(!std::strcmp(KeyName(28,true),"APAD_UP"),"controller key without image has text fallback");
    Require(KeyName('F',true)==nullptr,"ordinary keyboard names reach stock formatter");
    Require(KeyName(24,false)==nullptr && KeyName(25,false)==nullptr &&
        KeyName(24,true)==nullptr && KeyName(25,true)==nullptr,"reserved key holes never assert or alias a controller button");
    bindings[6]=" \t+AcTiVaTe\r\n"; ExpectKey(" +ACTIVATE ",6);
    ExpectGlyph("+activate","button_rshldr");
    bindings[6]="+reload"; ExpectKey("+reload",6); ExpectKey("+activate",3);
    bindings[3]="+reload"; bindings[6]="";
    Require(Display("+activate",0)=="Unbound","reload-only custom binding must not masquerade as Use or fall back to keyboard");
    bindings[3]="+holdbreath";
    Require(Display("+melee_breath",0)=="Unbound","holdbreath-only binding cannot perform spectator melee action");
    bindings[3]="+melee_breath";
    bindings[16]="";
    ExpectKey("+melee_breath",3); ExpectKey("+melee",17); ExpectKey("+holdbreath",3);
    bindings[17]=""; ExpectKey("+melee",3);
    bindings[3]="+activate"; bindings[6]="+activate"; bindings[19]="+activate";
    int keys[4]{123,0,0,456};
    Require(Key_GetCommandAssignmentInternal(0,"+activate",keys+1)==2 && keys[0]==123 && keys[1]==3 && keys[2]==6 && keys[3]==456,"two-key bounded output");
    const auto multiple=Display("+activate",2);
    Require(multiple.find(" or ")!=multiple.npos,"two controller bindings retain localized conjunction");
    Require(Display("+activate",1,true).find(" or ")==std::string::npos,"single binding formatting returns first glyph");
    const PromptBinding candidates[]{{'F',"+activate"},{28,"+activate"},{224,"+activate"},{3,nullptr},{6,""}};
    Require(ResolveCommandKeys("+activate",candidates,keys)==0 && keys[0]==-1 && keys[1]==-1,"only current physical buttons qualify");
    const PromptBinding malformed[]{{3,"+usereload; say test"},{6,"+reload_extra"}};
    Require(ResolveCommandKeys("+reload",malformed,keys)==0,"aliases never match command substrings or scripts");
    Require(ResolveCommandKeys(" \t\n",malformed,keys)==0,"blank query is unbound");
    Require(CommandKeys(nullptr,keys)==0,"null bridge query is unbound");
    const PromptBinding duplicate[]{{3,"+activate"},{3,"+activate"},{6,"+activate"}};
    Require(ResolveCommandKeys("+activate",duplicate,keys)==2 && keys[0]==3 && keys[1]==6,"duplicate physical keys cannot repeat a prompt");
    Layout("buttons_default"); inUse=false;
    Require(PromptsActive(),"connected live game keeps controller prompts after keyboard/chat input");
    ExpectGlyph("+activate","button_x");
    catchers=KEYCATCH_UI; activeMenu=UIMENU_SCOREBOARD;
    Require(PromptsActive(),"scoreboard keeps gameplay prompt family");
    activeMenu=0; Require(!PromptsActive(),"actual menu follows keyboard last input");
    Require(Display("+activate")=="F" && Display("+reload")=="R","menu keyboard fallback excludes earlier controller key codes");
    Require(Display("+gostand")=="Space","keyboard labels retain localization");
    Require(Display("+attack")=="Mouse 1","mouse labels retain localization");
    inUse=true; ExpectGlyph("+activate","button_x");
    inUse=false; catchers=0; connection.demoplaying=true;
    Require(!PromptsActive(),"demo playback does not force connected gameplay family");
    connection.demoplaying=false; connected=false;
    Require(!PromptsActive(),"disconnected keyboard-last input does not force controller prompts");
    connected=true; ready=false; Require(!PromptsActive(),"uninitialized controller does not force prompts");
    ready=true; inUse=true; Layout("buttons_default");
    for(auto family:{Controller::family::xbox,Controller::family::dualsense}) {
        activeFamily=family;
        const std::string back=KeyName(2,true),confirm=KeyName(1,true),start=KeyName(14,true),fire=KeyName(19,true);
        Require(IsStockMenuPrompt("platform_eog_press_esc") && IsStockMenuPrompt("PLATFORM_PRESS_TO_SET_AIRSTRIKE") &&
            IsStockMenuPrompt("quickmessage_esc_exit"),"stock prompt keys are case insensitive");
        Require(!IsStockMenuPrompt("PLATFORM_EOG_PRESS_ESC_EXTRA"),"stock adaptation requires exact key");
        auto adapted=AdaptStockMenuPrompt("platform_eog_press_esc","Press ^1EsCaPe^7 to leave.",back,confirm);
        Require(adapted=="Press ^1"+back+"^7 to leave.","EOG replaces only action token and preserves wording/colors");
        Require(adapted.find(start)==adapted.npos,"EOG back prompt never substitutes Start");
        Require(AdaptStockMenuPrompt("PLATFORM_EOG_PRESS_ESC","Press [ESC]",back,confirm)=="Press ["+back+"]","EOG bracketed key keeps punctuation");
        Require(AdaptStockMenuPrompt("QUICKMESSAGE_ESC_EXIT","ESC: Exit",back,confirm)==back+": Exit","quick-message exit uses actual menu Back");
        Require(AdaptStockMenuPrompt("PLATFORM_EOG_PRESS_ESC","Appuyez sur eChAp pour sortir.",back,confirm,"ECHAP")=="Appuyez sur "+back+" pour sortir.",
            "localized Escape token matches case-insensitively");
        Require(AdaptStockMenuPrompt("QUICKMESSAGE_ESC_EXIT","[TOUCHE ECHAP]: Quitter",back,confirm,"touche echap")=="["+back+"]: Quitter",
            "localized Escape names may span multiple words");
        Require(AdaptStockMenuPrompt("QUICKMESSAGE_ESC_EXIT","TOUCHE ECHAPPEE",back,confirm,"TOUCHE ECHAP")==back+" TOUCHE ECHAPPEE",
            "localized Escape phrase must end at a word boundary");
        Require(AdaptStockMenuPrompt("QUICKMESSAGE_ESC_EXIT","PRECHAP",back,confirm,"ECHAP")==back+" PRECHAP",
            "localized Escape phrase must start at a word boundary");
        adapted=AdaptStockMenuPrompt("PLATFORM_PRESS_TO_SET_AIRSTRIKE","Press fIrE to set airstrike.",back,confirm);
        Require(adapted=="Press "+confirm+" to set airstrike." && adapted.find(fire)==adapted.npos,
            "airstrike location confirms with context A/Cross rather than gameplay Fire");
        Require(AdaptStockMenuPrompt("PLATFORM_EOG_PRESS_ESC","Escapee and ESC_key remain unchanged.",back,confirm)==back+" Escapee and ESC_key remain unchanged.",
            "EOG alias never replaces a substring inside another word");
        Require(AdaptStockMenuPrompt("PLATFORM_PRESS_TO_SET_AIRSTRIKE","Firestorm and wildfire remain unchanged.",back,confirm)==confirm+" Firestorm and wildfire remain unchanged.",
            "airstrike alias never replaces a substring inside another word");
        const std::string localized="Appuyez sur \xC3\x89" "chap pour revenir.";
        Require(AdaptStockMenuPrompt("PLATFORM_EOG_PRESS_ESC",localized,back,confirm)==back+" "+localized,
            "unknown localized token retains complete UTF-8 wording after glyph prefix");
        Require(AdaptStockMenuPrompt("MENU_OTHER","Press ESC",back,confirm).empty(),"unrelated localized strings stay on original path");
        Require(AdaptStockMenuPrompt("PLATFORM_EOG_PRESS_ESC","Press ESC","",confirm).empty(),"missing back binding leaves original text path");
        Require(AdaptStockMenuPrompt("PLATFORM_PRESS_TO_SET_AIRSTRIKE","Fire",back,"").empty(),"missing confirm binding leaves original text path");
        Require(PrefixPromptBinding("Reload",Display("+reload"))==std::string(KeyName(3,true))+" Reload",
            "reload composition uses actual current merged use/reload binding");
        Require(PrefixPromptBinding("Reload","")=="Reload","empty reload binding preserves message");
    }
    bindings[6]="+reload";
    Require(PrefixPromptBinding("Reload",Display("+reload"))==std::string(KeyName(6,true))+" Reload",
        "reload prompt follows explicit reassignment rather than default Square/X");
    std::puts("Production gameplay binding aliases, eight layouts, glyph formatting, localized context prompts and keyboard fallback passed");
}
