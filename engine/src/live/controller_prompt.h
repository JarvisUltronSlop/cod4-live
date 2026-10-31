#pragma once
#include <initializer_list>
#include <span>
#include <string_view>

namespace live::controller {
struct PromptBinding { int key; const char* command; };

// A connected controller is the only valid gameplay input. Menu last-input
// switching stays separate, so typing in chat cannot change gameplay hints.
constexpr bool UseControllerPrompts(bool inUse, bool connected, bool liveGameplay) {
    return inUse || (connected && liveGameplay);
}

namespace prompt_detail {
constexpr bool PhysicalButton(int key) {
    return (key >= 1 && key <= 6) || (key >= 14 && key <= 23);
}
inline std::string_view Trim(std::string_view text) {
    const auto space=[](char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; };
    while (!text.empty() && space(text.front())) text.remove_prefix(1);
    while (!text.empty() && space(text.back())) text.remove_suffix(1);
    return text;
}
inline bool Equal(std::string_view a, std::string_view b) {
    a=Trim(a); b=Trim(b);
    if (a.size()!=b.size()) return false;
    const auto lower=[](char c) { return c>='A' && c<='Z' ? c+('a'-'A') : c; };
    for (std::size_t i=0;i<a.size();++i) if (lower(a[i])!=lower(b[i])) return false;
    return true;
}
inline bool Ads(std::string_view command) {
    return Equal(command,"+speed") || Equal(command,"+speed_throw") ||
        Equal(command,"+toggleads_throw") || Equal(command,"toggleads");
}
inline bool MergedAction(std::string_view query, std::string_view bound) {
    // IW4x Engine/Bind.cpp maps use/reload and scoreboard prompt aliases.
    // CoD4 also has merged sprint/breath and stance commands. These matches
    // describe actions the bound command actually performs; bindings are not
    // rewritten and a reload-only custom binding is never presented as Use.
    if (Equal(query,"+activate") || Equal(query,"+reload")) return Equal(bound,"+usereload");
    if (Equal(query,"+holdbreath"))
        return Equal(bound,"+holdbreath") || Equal(bound,"+breath_sprint") || Equal(bound,"+melee_breath");
    if (Equal(query,"+sprint")) return Equal(bound,"+breath_sprint");
    if (Equal(query,"+melee")) return Equal(bound,"+melee_breath");
    if (Ads(query)) return Ads(bound);
    if (Equal(query,"+gostand")) return Equal(bound,"+moveup");
    if (Equal(query,"+moveup")) return Equal(bound,"+gostand");
    if (Equal(query,"gocrouch") || Equal(query,"goprone") || Equal(query,"+prone") ||
        Equal(query,"+movedown") || Equal(query,"raisestance") || Equal(query,"lowerstance") ||
        Equal(query,"togglecrouch") || Equal(query,"toggleprone")) return Equal(bound,"+stance");
    if (Equal(query,"togglescores")) return Equal(bound,"+scores");
    if (Equal(query,"+scores")) return Equal(bound,"togglescores");
    return false;
}
}

// Resolve against current physical bindings rather than a preset's default
// button. Exact custom bindings take priority over merged-action fallbacks.
inline int ResolveCommandKeys(std::string_view command, std::span<const PromptBinding> bindings, int* keys) {
    keys[0]=keys[1]=-1;
    if (prompt_detail::Trim(command).empty()) return 0;
    for (bool fallback : {false,true}) {
        int count=0;
        for (const auto& binding:bindings) {
            if (!prompt_detail::PhysicalButton(binding.key) || !binding.command || !*binding.command) continue;
            const bool matches=fallback ? prompt_detail::MergedAction(command,binding.command) :
                prompt_detail::Equal(command,binding.command);
            if (!matches || (count && keys[0]==binding.key)) continue;
            keys[count++]=binding.key;
            if (count==2) break;
        }
        if (count) return count;
    }
    return 0;
}
}
