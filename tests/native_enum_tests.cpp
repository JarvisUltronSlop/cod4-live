#include <live/controller_policy.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Rendering, dvar storage, scripts and unrelated widget types are fixtures.
// Native focus eligibility, menu key dispatch, enum lookup and writes are the
// production functions extracted at build time, rather than a copied model.
struct rectDef_s { float x{}, y{}, w{640}, h{480}; int horzAlign{}, vertAlign{}; };
struct windowDef_t { int dynamicFlags[1]{6}; int staticFlags{}; rectDef_s rect; int ownerDraw{}, ownerDrawFlags{}; };
struct menuDef_t;
struct itemDef_s {
    windowDef_t window; menuDef_t* parent{}; int type{13}, dvarFlags{}, cursorPos[1]{};
    const char* dvar{"live_botCount"}; const char* onFocus{};
    float special{};
    union { const char* enumDvarName; void* listBox; } typeData{"ui_live_botCountOptions"};
};
struct menuDef_t { windowDef_t window; const char* allowedBinding{}; int fullScreen{1}, itemCount{1}; itemDef_s** items{}; const char* onESC{}; };
struct UiContext { int localClientNum{}; struct { float x{}, y{}; } cursor; bool isCursorVisible{}; };
struct dvar_s { int type{6}; union { struct { int stringCount; const char** strings; } enumeration; struct { int min, max; } integer; } domain{{64, nullptr}}; };
static dvar_s domain;
static int value, writes, actions, nextFocus, previousFocus;
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
const char* Dvar_GetVariantString(const char* name) { Require(!std::strcmp(name, "live_botCount"), "target dvar lookup"); return va("%d", value); }
void Dvar_SetFromStringByName(const char* name, const char* text) { Require(!std::strcmp(name, "live_botCount"), "target dvar write"); value = std::atoi(text); ++writes; }
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
void Item_RunScript(UiContext*, itemDef_s*, char*) {}
void Item_Action(UiContext*, itemDef_s*) { ++actions; }
void Item_StartCapture(UiContext*, itemDef_s*, int) {}
int Item_ListBox_HandleKey(UiContext*, itemDef_s*, int, int, int) { return 0; }
int UI_OwnerDrawHandleKey(int, int, float*, int) { return 0; }
int Item_Slider_HandleKey(UiContext*, itemDef_s*, int, int) { return 0; }
int Item_YesNo_HandleKey(UiContext*, itemDef_s*, int) { return 0; }
int Item_Multi_HandleKey(UiContext*, itemDef_s*, int) { return 0; }
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
    Require(value==0 && writes==0 && nextFocus==1 && previousFocus==1, "one-player capacity has only zero bots and arrows fall through to navigation");
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
    std::puts("Native menu focus, enum key routing and capacity boundary checks passed");
}
