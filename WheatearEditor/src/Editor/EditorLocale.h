#pragma once

#include "Wheatear/Config/UserSettings.h"

#ifdef WT_PLATFORM_WINDOWS
#include <windows.h>
#endif

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

    // Follow the OS UI language (used when the user has not pinned a choice).
    inline Language DetectSystemLanguage()
    {
#ifdef WT_PLATFORM_WINDOWS
        const LANGID langId = GetUserDefaultUILanguage();
        if (PRIMARYLANGID(langId) == LANG_CHINESE)
            return Language::ChineseSimplified;
#endif
        return Language::English;
    }

    // Apply the persisted editor language (-1 = auto -> OS detection).
    inline void ApplyFromSettings()
    {
        const int persisted = UserSettings::Get().EditorLanguage;
        if (persisted == 1)
            SetLanguage(Language::ChineseSimplified);
        else if (persisted == 0)
            SetLanguage(Language::English);
        else
            SetLanguage(DetectSystemLanguage());
    }

    // Persist a manual choice; Auto is not re-persisted (stays -1 in settings).
    inline void PersistLanguage(Language language)
    {
        auto& settings = UserSettings::Get();
        settings.EditorLanguage = (language == Language::ChineseSimplified) ? 1 : 0;
        UserSettings::Save();
    }

    inline bool DrawLanguageMenu()
    {
        bool changed = false;
        if (ImGui::MenuItem("English", nullptr, GetLanguage() == Language::English))
        {
            SetLanguage(Language::English);
            PersistLanguage(Language::English);
            changed = true;
        }
        if (ImGui::MenuItem("中文", nullptr, GetLanguage() == Language::ChineseSimplified))
        {
            SetLanguage(Language::ChineseSimplified);
            PersistLanguage(Language::ChineseSimplified);
            changed = true;
        }
        return changed;
    }

} // namespace Wheatear::EditorLocale
