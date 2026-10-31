#include "bot_menu.h"
#include "bot_warfare.h"
#include "controller.h"
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <client_mp/client_mp.h>
#include <ui/ui_shared.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Real CoD4 menu widgets: the engine owns drawing, focus, key dispatch, mouse
// selection and menu stacking. The only dynamic content is the allowed count.
namespace live::bots_ui {
namespace {
menuDef_t menu{};
itemDef_s rows[10]{};
itemDef_s* pointers[10]{};
char countLabels[64][4]{};
const char* countChoices[65]{};
const char* difficulties[]{"Random", "Recruit", "Easy", "Regular", "Hardened", "Veteran", "Expert", "Maximum", nullptr};
const dvar_s* countDomain = nullptr;
const dvar_s* enabled = nullptr;
int previousCap = -1;
bool built = false;
char capacityText[96]{};
char statusText[128]{};
char helpText[256]{};
enum Row { Title, Intro, CountLabel, Count, DifficultyLabel, Difficulty, Capacity, Status, Help, Back };

void Color(float (&color)[4], float r, float g, float b, float a = 1.0f) {
    color[0] = r; color[1] = g; color[2] = b; color[3] = a;
}
void Item(Row index, const char* name, const char* text, float x, float y, float width, bool interactive = false) {
    auto& item = rows[index];
    item = {};
    item.parent = &menu;
    item.window.name = name;
    item.window.rect = {x, y, width, 28.0f, 0, 0};
    item.window.rectClient = item.window.rect;
    item.window.dynamicFlags[0] = 4 | 0x10000;
    item.window.staticFlags = interactive ? 0 : 0x100000;
    Color(item.window.foreColor, 0.84f, 0.85f, 0.78f);
    Color(item.window.outlineColor, 0, 0, 0, 0.75f);
    item.fontEnum = 4;
    item.textscale = 0.36f;
    item.textStyle = 3;
    item.textaligny = 20.0f;
    item.text = text;
    item.imageTrack = 3;
    item.type = interactive ? 1 : 0;
    item.action = "";
    pointers[index] = &item;
}
void Choice(Row index, const char* dvar, const char* domain) {
    auto& item = rows[index];
    item.type = item.dataType = 13;
    item.dvar = dvar;
    item.typeData.enumDvarName = domain;
    item.dvarTest = "ui_live_botControls";
    item.enableDvar = "1";
    item.dvarFlags = 1;
}
int CapacityLimit() {
    const bool hosting = com_sv_running && com_sv_running->current.enabled;
    const auto read = [hosting](const dvar_s* capacity) {
        if (!capacity) return 0;
        if (capacity->type == DVAR_TYPE_INT || capacity->type == DVAR_TYPE_ENUM)
            return hosting ? capacity->current.integer : capacity->latched.integer;
        if (capacity->type == DVAR_TYPE_STRING) return std::atoi(capacity->current.string);
        return 0;
    };
    int value = !hosting ? read(Dvar_FindVar("ui_maxclients")) : 0;
    if (value < 1 || value > 64) value = read(Dvar_FindVar("sv_maxclients"));
    if (value < 1) value = 32;
    return std::clamp(value, 1, 64) - 1;
}
}

bool CanConfigure() {
    return (com_sv_running && com_sv_running->current.enabled) ||
        clientUIActives[0].connectionState == CA_DISCONNECTED;
}

void Initialize() {
    live::bots::Initialize();
    for (int i = 0; i < 64; ++i) {
        std::snprintf(countLabels[i], sizeof(countLabels[i]), "%d", i);
        countChoices[i] = countLabels[i];
    }
    countChoices[64] = nullptr;
    countDomain = Dvar_RegisterEnum("ui_live_botCountOptions", countChoices, 0, DVAR_NOFLAG, "Bot count menu choices");
    Dvar_RegisterEnum("ui_live_botDifficultyOptions", difficulties, 0, DVAR_NOFLAG, "Bot difficulty menu choices");
    enabled = Dvar_RegisterBool("ui_live_botControls", true, DVAR_ROM, "Local bot configuration available");
    previousCap = -1;
    menu = {};
    menu.window.name = "live_bot_settings";
    menu.window.rect = {0, 0, 640, 480, 0, 0};
    menu.window.rectClient = menu.window.rect;
    menu.window.style = 1;
    Color(menu.window.foreColor, 1, 1, 1);
    Color(menu.window.backColor, 0.06f, 0.075f, 0.065f, 0.96f);
    Color(menu.focusColor, 0.94f, 0.87f, 0.50f);
    Color(menu.disableColor, 0.4f, 0.4f, 0.4f);
    menu.fullScreen = 1;
    menu.imageTrack = 3;
    menu.fadeClamp = 1;
    menu.fadeAmount = 0.1f;
    menu.fadeInAmount = 0.1f;
    menu.fadeCycle = 1;
    menu.items = pointers;
    menu.itemCount = 10;
    menu.cursorItem[0] = -1;
    menu.onOpen = "setfocus bot_count ;";
    menu.onESC = "close live_bot_settings ;";

    Item(Title, "bot_title", "BOT WARFARE", 48, 40, 544);
    rows[Title].textscale = 0.60f;
    Color(rows[Title].window.foreColor, 0.94f, 0.87f, 0.50f);
    Item(Intro, "bot_intro", "Configure AI players for your hosted match.", 48, 85, 544);
    Item(CountLabel, "bot_count_label", "NUMBER OF BOTS", 64, 150, 300);
    Item(Count, "bot_count", "", 390, 150, 180, true);
    Choice(Count, "live_botCount", "ui_live_botCountOptions");
    Item(DifficultyLabel, "bot_difficulty_label", "DIFFICULTY", 64, 195, 300);
    Item(Difficulty, "bot_difficulty", "", 390, 195, 180, true);
    Choice(Difficulty, "live_botDifficulty", "ui_live_botDifficultyOptions");
    Item(Capacity, "bot_capacity", capacityText, 64, 248, 520);
    Item(Status, "bot_status", statusText, 64, 279, 520);
    Item(Help, "bot_help", helpText, 64, 345, 520);
    rows[Help].textscale = 0.31f;
    Item(Back, "bot_back", "BACK", 48, 405, 130, true);
    rows[Back].action = "close live_bot_settings ;";
    built = true;
    Refresh();
}

menuDef_t* Menu() { return built ? &menu : nullptr; }

void Refresh() {
    if (!built) return;
    if (controller::InUse())
        std::snprintf(helpText, sizeof(helpText), "%s Select    %s Back    %s %s Adjust",
            controller::KeyName(1, true), controller::KeyName(2, true),
            controller::KeyName(22, true), controller::KeyName(23, true));
    else
        std::snprintf(helpText, sizeof(helpText), "Enter: Select    Escape: Back    Left / Right: Adjust");
    const int maximum = CapacityLimit();
    if (previousCap != maximum) {
        for (int i = 0; i <= maximum; ++i) countChoices[i] = countLabels[i];
        countChoices[maximum + 1] = nullptr;
        Dvar_UpdateEnumDomain(const_cast<dvar_s*>(countDomain), countChoices);
        previousCap = maximum;
    }
    const bool allowed = CanConfigure();
    Dvar_SetBool(enabled, allowed);
    if (allowed) {
        const int count = Dvar_GetInt("live_botCount");
        if (count < 0 || count > maximum) Dvar_SetIntByName("live_botCount", std::clamp(count, 0, maximum));
    } else {
        rows[Count].window.dynamicFlags[0] &= ~2;
        rows[Difficulty].window.dynamicFlags[0] &= ~2;
    }
    std::snprintf(capacityText, sizeof(capacityText), "0 disables bots. Maximum: %d (one slot reserved for a player).", maximum);
    std::snprintf(statusText, sizeof(statusText), "%s", !allowed ? "Only the host can configure bots." :
        com_sv_running && com_sv_running->current.enabled ? "Changes apply to this hosted match." : "These settings apply when you start a hosted match.");
}
}
