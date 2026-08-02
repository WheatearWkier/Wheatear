#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    struct UserSettingsData
    {
        int TextSpeed = 48;
        int MasterVolume = 60;
        int BGMVolume = 55;
        int SFXVolume = 55;
        bool Fullscreen = false;
        bool ScreenShake = true;
        std::unordered_map<std::string, std::vector<int>> KeyBindings;
    };

    class WHEATEAR_API UserSettings
    {
    public:
        static UserSettingsData Defaults();
        static UserSettingsData& Get();
        static void Set(const UserSettingsData& settings);

        static void Load();
        static bool Save();
        static void ResetToDefaults();
        static void ApplyToRuntime();

        static std::filesystem::path SettingsPath();
    };

} // namespace Wheatear
