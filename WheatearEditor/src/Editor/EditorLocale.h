#pragma once

#include <imgui/imgui.h>

namespace Wheatear::EditorLocale {

    enum class Language
    {
        English = 0,
        ChineseSimplified
    };

    inline Language& ActiveLanguage()
    {
        static Language language = Language::English;
        return language;
    }

    inline Language GetLanguage()
    {
        return ActiveLanguage();
    }

    inline void SetLanguage(Language language)
    {
        ActiveLanguage() = language;
    }

    inline bool IsChinese()
    {
        return GetLanguage() == Language::ChineseSimplified;
    }

    inline const char* Text(const char* english, const char* chinese)
    {
        return IsChinese() && chinese ? chinese : english;
    }

    inline bool DrawLanguageMenu()
    {
        bool changed = false;
        if (ImGui::MenuItem("English", nullptr, GetLanguage() == Language::English))
        {
            SetLanguage(Language::English);
            changed = true;
        }
        if (ImGui::MenuItem("中文", nullptr, GetLanguage() == Language::ChineseSimplified))
        {
            SetLanguage(Language::ChineseSimplified);
            changed = true;
        }
        return changed;
    }

} // namespace Wheatear::EditorLocale
