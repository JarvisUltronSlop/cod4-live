#include <live/controller_policy.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>

// Rendering, dvar storage, scripts and unrelated widget types are fixtures.
// Native focus eligibility, menu key dispatch, enum lookup and writes are the
// production functions extracted at build time, rather than a copied model.
struct rectDef_s { float x{}, y{}, w{640}, h{480}; int horzAlign{}, vertAlign{}; };
struct windowDef_t { int dynamicFlags[1]{6}; int staticFlags{}; rectDef_s rect; int ownerDraw{}, ownerDrawFlags{}; };
struct menuDef_t;
struct ItemKeyHandler { int key; const char* action; ItemKeyHandler* next; };
struct listBoxDef_s { int startPos[1]{}, endPos[1]{}, mousePos{}, notselectable{}; const char* doubleClick{}; };
struct editFieldDef_s { float minVal{}, maxVal{1}; };
struct multiDef_s { int count{}, strDef{}; const char* dvarStr[32]{}; float dvarValue[32]{}; };
struct itemDef_s {
    windowDef_t window; menuDef_t* parent{}; int type{13}, dvarFlags{}, cursorPos[1]{};
    const char* dvar{"live_botCount"}; const char* onFocus{};
    float special{};
    ItemKeyHandler* onKey{};
    union { const char* enumDvarName; listBoxDef_s* listBox; editFieldDef_s* editField; multiDef_s* multi; } typeData{"ui_live_botCountOptions"};
};
namespace live::host_ui { void SettingChanged(const itemDef_s*) {} }
struct menuDef_t { windowDef_t window; const char* allowedBinding{}; int fullScreen{1}, itemCount{1}; itemDef_s** items{}; const char* onESC{}; };
struct UiContext { int localClientNum{}, realTime{}; struct { float x{}, y{}; } cursor; bool isCursorVisible{}; };
struct dvar_s { int type{6}; union { struct { int stringCount; const char** strings; } enumeration; struct { int min, max; } integer; } domain{{64, nullptr}}; };
static dvar_s domain;
static float value;
static int writes, actions, nextFocus, previousFocus, scripts, lastListBoxClickTime, selectedMap, feederCount=12;
static bool controllerNavigation=true;
static int ownerDrawKey{};
namespace live::controller { bool NavigatingMenu() { return controllerNavigation; } }
static bool enabled = true;
static int g_waitingForKey, g_editingField, g_debugMode, inHandleKey;
static itemDef_s *g_bindItem, *g_editItem;
static void *itemCapture, *captureFunc, *captureData;
constexpr int alwaysfails=0;

static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void MyAssertHandler(const char*, int, int, const char*, ...) { Require(false, "production assertion"); }
int I_stricmp(const char* a, const char* b) { return std::strcmp(a, b); }
char* va(const char* format, ...) { static char text[64]; va_list args; va_start(args, format); std::vsnprintf(text, sizeof(text), format, args); va_end(args); return text; }
const dvar_s* Dvar_FindVar(const char* name) { Require(!std::strcmp(name, "ui_live_botCountOptions"), "enum domain lookup"); return &domain; }
const char* Dvar_GetVariantString(const char* name) { Require(!std::strcmp(name, "live_botCount"), "target dvar lookup"); return va("%g", value); }
void Dvar_SetFromStringByName(const char* name, const char* text) { Require(!std::strcmp(name, "live_botCount"), "target dvar write"); value = std::atof(text); ++writes; }
void I_strncpyz(char* out, const char* in, int count) { std::snprintf(out,count,"%s",in); }
int Dvar_GetInt(const char*) { return 0; }
bool Window_IsVisible(int, const windowDef_t* w) { return (w->dynamicFlags[0] & 4) != 0; }
bool Item_ContainsMouse(UiContext* dc, itemDef_s*) { return dc->isCursorVisible; }
int Item_IsVisible(int client, itemDef_s* item) { return Window_IsVisible(client, &item->window); }
bool Item_EnableShowViaDvar(const itemDef_s*, int) { return enabled; }
bool Rect_ContainsPoint(int, const rectDef_s*, float, float) { return true; }
void Window_AddDynamicFlags(int, windowDef_t* window, int flags) { window->dynamicFlags[0] |= flags; }
menuDef_t* Menu_GetFocused(UiContext*) { return nullptr; }
itemDef_s* Menu_ClearFocus(UiContext*, menuDef_t* menu) { for (int i=0; i<menu->itemCount; ++i) menu->items[i]->window.dynamicFlags[0] &= ~2; return nullptr; }
void Menu_SetCursorItem(int, menuDef_t*, int) {}
const rectDef_s* Item_GetTextRect(int, itemDef_s* item) { return &item->window.rect; }
const rectDef_s* Item_CorrectedTextRect(int, itemDef_s* item) { return &item->window.rect; }
bool Item_IsTextField(itemDef_s*) { return false; }
void Item_RunScript(UiContext*, itemDef_s*, char*) { ++scripts; }
void Item_Action(UiContext*, itemDef_s*) { ++actions; }
void Item_StartCapture(UiContext*, itemDef_s*, int) {}
listBoxDef_s* Item_GetListBoxDef(itemDef_s* item) { return item->typeData.listBox; }
editFieldDef_s* Item_GetEditFieldDef(itemDef_s* item) { return item->typeData.editField; }
multiDef_s* Item_GetMultiDef(itemDef_s* item) { return item->typeData.multi; }
int UI_FeederCount(int, float) { return feederCount; }
void UI_FeederSelection(int, float, int index) { selectedMap=index; }
void UI_OverrideCursorPos(int, itemDef_s* item) { item->cursorPos[0]=selectedMap; }
int Item_ListBox_MaxScroll(int, itemDef_s*) { return std::max(0,feederCount-3); }
int Item_ListBox_Viewmax(itemDef_s*) { return 3; }
bool ListBox_HasValidCursorPos(int, itemDef_s* item) { return item->cursorPos[0]>=0 && item->cursorPos[0]<feederCount; }
int CL_GetLocalClientActiveCount() { return 1; }
void Script_FeederTop(UiContext*, itemDef_s*) {}
void Script_FeederBottom(UiContext*, itemDef_s*) {}
void Scroll_Slider_SetThumbPos(UiContext*, itemDef_s*) {}
int UI_OwnerDrawHandleKey(int, int, float*, int key) { ownerDrawKey=key; return key==K_MOUSE1||key==K_MOUSE2; }
int Item_Bind_HandleKey(UiContext*, itemDef_s*, int, int) { return 0; }
bool Item_TextField_HandleKey(UiContext*, itemDef_s*, int) { return true; }
void Item_TextField_BeginEdit(int, itemDef_s*) {}
void Display_MouseMove(UiContext*) {}
const char* Key_GetBinding(int, int) { return nullptr; }
int CL_ControllerIndexFromClientNum(int) { return 0; }
void Cbuf_ExecuteBuffer(int, int, char*) {}
void Cbuf_AddText(int, const char*) {}
void Key_SetOverstrikeMode(int, int) {}
int Menu_CheckOnKey(UiContext*, menuDef_t*, int) { return 0; }
void Menu_SetNextCursorItem(UiContext*, menuDef_t*) { ++nextFocus; }
void Menu_SetPrevCursorItem(UiContext*, menuDef_t*) { ++previousFocus; }
void Menus_HandleOOBClick(UiContext*, menuDef_t*, int, int) {}
#include "generated_native_enum.inc"

int main() {
    UiContext dc; itemDef_s item; itemDef_s* items[]{&item}; menuDef_t menu;
    menu.items=items; item.parent=&menu;
    auto reset=[&](int count, int current=0) {
        domain.domain.enumeration.stringCount=count; value=current; writes=actions=nextFocus=previousFocus=0;
        item.window.dynamicFlags[0]=6; item.dvarFlags=0; enabled=true;
    };
    auto press=[&](int key) { Menu_HandleKey(&dc, &menu, key, true); Menu_HandleKey(&dc, &menu, key, false); };
    reset(64); press(K_RIGHTARROW);
    Require(value==1 && writes==1 && actions==1 && nextFocus==0, "native right adjusts focused enum before navigation");
    press(K_LEFTARROW); Require(value==0 && writes==2 && actions==2 && previousFocus==0, "native left reverses right");
    press(K_LEFTARROW); Require(value==63, "left wraps zero to maximum bot count");
    press(K_RIGHTARROW); Require(value==0, "right wraps maximum to zero");
    press(live::controller::MenuKeyForButton(1)); Require(value==1, "production Cross/A mapping advances the native enum");
    reset(64); for(int i=0;i<7;++i) press(K_RIGHTARROW);
    Require(value==7 && writes==7 && nextFocus==0, "seven repeated directional presses select seven bots");
    reset(8,7); press(K_RIGHTARROW); Require(value==0, "difficulty wraps its eight choices");
    press(K_LEFTARROW); Require(value==7, "difficulty reverses wrap");
    reset(1); press(K_RIGHTARROW); press(K_LEFTARROW); press(live::controller::MenuKeyForButton(1));
    Require(value==0 && writes==0 && nextFocus==0 && previousFocus==0, "single-choice settings retain focus on adjust");
    domain.domain.enumeration.stringCount=64; press(K_RIGHTARROW);
    Require(value==1 && writes==1, "updated host capacity immediately exposes further native enum choices");
    reset(64); item.window.dynamicFlags[0]=4;
    press(K_RIGHTARROW); press(K_LEFTARROW); press(K_ENTER);
    Require(value==0 && writes==0, "unfocused enum never changes");
    item.dvarFlags=1; enabled=false;
    Require(Item_SetFocus(&dc,&item,0,0)==0, "disabled enum cannot gain native focus");
    press(K_RIGHTARROW); Require(writes==0, "disabled unfocused enum is unchanged");
    enabled=true; Require(Item_SetFocus(&dc,&item,0,0)==1, "enabled enum gains native focus");
    press(K_RIGHTARROW); Require(value==1, "newly focused enabled enum adjusts");
    reset(64); item.window.dynamicFlags[0]=2; press(K_RIGHTARROW);
    Require(writes==0, "hidden focused flag cannot change enum");
    reset(64); Menu_HandleKey(&dc,&menu,K_RIGHTARROW,false); Require(writes==0, "key release never applies second change");
    press(22); press(23); Require(writes==0, "raw gamepad directions require controller-to-arrow translation");
    item.dvarFlags=1; enabled=false; press(K_RIGHTARROW);
    Require(writes==0, "a focused setting that becomes disabled cannot change");
    reset(64); item.type=10; editFieldDef_s slider; item.typeData.editField=&slider;
    press(K_RIGHTARROW); Require(std::fabs(value-.05f)<.0001f && nextFocus==0, "graphics slider adjusts without visible mouse");
    value=1; press(K_RIGHTARROW); Require(value==1 && nextFocus==0, "slider endpoint retains focus");
    press(6); press(5); Require(nextFocus==1 && previousFocus==1, "shoulders leave a slider without changing its value");
    g_editingField=1;g_editItem=&item;press(6);
    Require(!g_editingField&&!g_editItem&&nextFocus==2,"shoulder exits text editing before moving focus");
    reset(64);item.type=8;press(K_RIGHTARROW);
    Require(ownerDrawKey==K_MOUSE1&&nextFocus==0,"game mode accepts Right as forward adjustment");
    press(K_LEFTARROW);Require(ownerDrawKey==K_MOUSE2&&previousFocus==0,"game mode accepts Left as reverse adjustment");
    reset(64); item.type=12; multiDef_s multi; multi.count=3; multi.dvarValue[0]=0; multi.dvarValue[1]=2; multi.dvarValue[2]=4; item.typeData.multi=&multi;
    press(K_RIGHTARROW); Require(value==2 && nextFocus==0, "graphics multi-choice selects next supported value");
    press(K_LEFTARROW); Require(value==0, "graphics multi-choice reverses");
    item.type=11; press(K_ENTER); Require(value==1,"graphics toggle accepts Cross/Enter");
    reset(64); item.type=6; listBoxDef_s list; list.doubleClick="join selected"; item.typeData.listBox=&list;
    dc.isCursorVisible=false; selectedMap=0; scripts=0;
    for(int i=0;i<7;++i) press(K_DOWNARROW);
    Require(selectedMap==7 && list.startPos[0]==5 && nextFocus==0, "controller scrolls map list past the visible page without mouse hover");
    press(K_UPARROW); Require(selectedMap==6,"controller reverses map selection");
    const int previousScripts=scripts; press(K_ENTER);
    Require(scripts==previousScripts+1 && selectedMap==6,"Cross activates the selected row once");
    press(6); press(5); Require(nextFocus==1 && previousFocus==1,"shoulders escape a focused list");
    press(K_LEFTARROW); Require(previousFocus==2,"left exits a vertical list to surrounding controls");
    const int before=selectedMap; press(K_MOUSE1); Require(selectedMap==before,"off-list mouse click cannot select a map");
    item.dvarFlags=1; enabled=false; press(K_DOWNARROW); Require(selectedMap==before,"disabled list rejects selection");
    enabled=true; item.window.dynamicFlags[0]=2; press(K_DOWNARROW); Require(selectedMap==before,"hidden list rejects selection");
    item.window.dynamicFlags[0]=6; feederCount=0; scripts=0; press(K_ENTER);
    Require(scripts==0,"empty list cannot activate a missing row");
    std::puts("Native controller focus, graphics widgets, list navigation and boundary checks passed");
}
