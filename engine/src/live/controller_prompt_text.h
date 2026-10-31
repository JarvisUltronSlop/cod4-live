#pragma once
#include <string>
#include <string_view>

namespace live::controller {
inline bool PromptTextEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    const auto lower=[](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    for (std::size_t i=0; i<a.size(); ++i) if (lower(a[i]) != lower(b[i])) return false;
    return true;
}

inline std::string PrefixPromptBinding(std::string_view localized, std::string_view binding) {
    if (binding.empty()) return std::string(localized);
    return std::string(binding) + " " + std::string(localized);
}

inline bool IsStockMenuPrompt(std::string_view key) {
    return PromptTextEqual(key, "PLATFORM_EOG_PRESS_ESC") ||
        PromptTextEqual(key, "QUICKMESSAGE_ESC_EXIT") ||
        PromptTextEqual(key, "PLATFORM_PRESS_TO_SET_AIRSTRIKE");
}

// Stock PC menu messages bypass the binding-expression and HUD-directive
// paths. Preserve their localized wording and replace only the standalone
// action token. These are context controls (menu Back/location Confirm), not
// configurable gameplay bindings. Unknown translations retain their wording
// with the actual context button prepended.
inline std::string AdaptStockMenuPrompt(std::string_view key, std::string_view localized,
    std::string_view menuBack, std::string_view locationConfirm,
    std::string_view localizedEscape={}) {
    const bool menuEscape=PromptTextEqual(key, "PLATFORM_EOG_PRESS_ESC") ||
        PromptTextEqual(key, "QUICKMESSAGE_ESC_EXIT");
    const bool airstrike=PromptTextEqual(key, "PLATFORM_PRESS_TO_SET_AIRSTRIKE");
    if (!menuEscape && !airstrike) return {};
    const auto binding=menuEscape ? menuBack : locationConfirm;
    if (binding.empty()) return {};
    const auto word=[](char c) {
        const auto u=static_cast<unsigned char>(c);
        return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
            (u >= '0' && u <= '9') || u == '_' || u >= 128;
    };
    for (std::size_t begin=0; begin<localized.size();) {
        // Native text color escapes do not belong to the following word.
        if (localized[begin]=='^' && begin+1<localized.size() &&
            localized[begin+1]>='0' && localized[begin+1]<='9') { begin+=2; continue; }
        if (!word(localized[begin])) { ++begin; continue; }
        if (menuEscape && !localizedEscape.empty() &&
            localizedEscape.size()<=localized.size()-begin &&
            PromptTextEqual(localized.substr(begin,localizedEscape.size()),localizedEscape)) {
            const auto end=begin+localizedEscape.size();
            if (end==localized.size() || !word(localized[end]))
                return std::string(localized.substr(0,begin)) + std::string(binding) +
                    std::string(localized.substr(end));
        }
        std::size_t end=begin+1;
        while (end<localized.size() && word(localized[end])) ++end;
        const auto token=localized.substr(begin,end-begin);
        if ((menuEscape && (PromptTextEqual(token,"ESC") || PromptTextEqual(token,"Escape"))) ||
            (airstrike && PromptTextEqual(token,"Fire"))) {
            return std::string(localized.substr(0,begin)) + std::string(binding) +
                std::string(localized.substr(end));
        }
        begin=end;
    }
    return PrefixPromptBinding(localized,binding);
}
}
