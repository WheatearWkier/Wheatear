#include "wtpch.h"
#include "UserSettings.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/Window.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Wheatear {

    namespace {

        UserSettingsData& Storage()
        {
            static UserSettingsData settings = UserSettings::Defaults();
            return settings;
        }

        bool& Loaded()
        {
            static bool loaded = false;
            return loaded;
        }

        static int ParseInt(const std::string& value, int fallback)
        {
            try
            {
                return std::stoi(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static bool ParseBool(const std::string& value, bool fallback)
        {
            if (value == "1" || value == "true" || value == "True")
                return true;
            if (value == "0" || value == "false" || value == "False")
                return false;
            return fallback;
        }

        static std::vector<int> ParseKeyList(const std::string& value)
        {
            std::vector<int> keys;
            std::stringstream stream(value);
            std::string item;
            while (std::getline(stream, item, ','))
            {
                const int key = ParseInt(item, -1);
                if (key >= -1)
                    keys.push_back(key);
            }
            return keys;
        }

        static std::string JoinKeyList(const std::vector<int>& keys)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < keys.size(); ++i)
            {
                if (i > 0)
                    stream << ",";
                stream << keys[i];
            }
            return stream.str();
        }

        static void AddBinding(UserSettingsData& settings, const char* actionId, std::initializer_list<int> keys)
        {
            settings.KeyBindings[actionId] = std::vector<int>(keys.begin(), keys.end());
        }

        static void MergeMissingDefaults(UserSettingsData& settings)
        {
            const UserSettingsData defaults = UserSettings::Defaults();
            for (const auto& [actionId, keys] : defaults.KeyBindings)
            {
                auto it = settings.KeyBindings.find(actionId);
                if (it == settings.KeyBindings.end())
                {
                    settings.KeyBindings[actionId] = keys;
                    continue;
                }

                // Older settings files dropped the -1 mouse sentinel during
                // parse/save. Reinsert default mouse bindings in memory so
                // mouse/touch-adjacent actions keep working for existing saves.
                for (int key : keys)
                {
                    if (key >= 0)
                        continue;
                    if (std::find(it->second.begin(), it->second.end(), key) == it->second.end())
                        it->second.push_back(key);
                }
            }
        }

    } // namespace

    UserSettingsData UserSettings::Defaults()
    {
        UserSettingsData settings;
        settings.TextSpeed = 48;
        settings.MasterVolume = 60;
        settings.BGMVolume = 55;
        settings.SFXVolume = 55;
        settings.Fullscreen = false;
        settings.ScreenShake = true;
        settings.EditorLanguage = -1;

        AddBinding(settings, "move.left", { WT_KEY_A, WT_KEY_LEFT });
        AddBinding(settings, "move.right", { WT_KEY_D, WT_KEY_RIGHT });
        AddBinding(settings, "move.up", { WT_KEY_W, WT_KEY_UP });
        AddBinding(settings, "move.down", { WT_KEY_S, WT_KEY_DOWN });
        AddBinding(settings, "game.pause", { WT_KEY_P, WT_KEY_ESCAPE });
        AddBinding(settings, "game.confirm", { WT_KEY_SPACE, WT_KEY_ENTER, WT_KEY_RIGHT });

        AddBinding(settings, "side.jump", { WT_KEY_K, WT_KEY_SPACE });
        AddBinding(settings, "side.basic", { WT_KEY_J, -1 });   // -1 = mouse left
        AddBinding(settings, "side.magic", { WT_KEY_U });
        AddBinding(settings, "side.support", { WT_KEY_H });
        AddBinding(settings, "side.dash", { WT_KEY_I });
        AddBinding(settings, "side.break_limit", { WT_KEY_L });
        AddBinding(settings, "side.item1", { WT_KEY_1 });
        AddBinding(settings, "side.item2", { WT_KEY_2 });
        AddBinding(settings, "side.item3", { WT_KEY_3 });

        AddBinding(settings, "arcade.attack", { WT_KEY_J, WT_KEY_SPACE, -1 });   // -1 = mouse left
        AddBinding(settings, "arcade.weapon1", { WT_KEY_1 });
        AddBinding(settings, "arcade.weapon2", { WT_KEY_2 });
        AddBinding(settings, "arcade.weapon3", { WT_KEY_3 });

        AddBinding(settings, "vn.advance", { WT_KEY_SPACE, WT_KEY_ENTER, WT_KEY_RIGHT });
        AddBinding(settings, "vn.auto", { WT_KEY_A });
        AddBinding(settings, "vn.history", { WT_KEY_H });
        AddBinding(settings, "vn.save", { WT_KEY_F5 });
        AddBinding(settings, "vn.load", { WT_KEY_F9 });

        return settings;
    }

    UserSettingsData& UserSettings::Get()
    {
        if (!Loaded())
            Load();
        return Storage();
    }

    void UserSettings::Set(const UserSettingsData& settings)
    {
        Storage() = settings;
        MergeMissingDefaults(Storage());
        Loaded() = true;
    }

    std::filesystem::path UserSettings::SettingsPath()
    {
        // Settings are runtime-generated data: write under the writable root
        // (project in the editor, next to the executable when packaged) so key
        // bindings survive cache re-extraction and repacks, matching where
        // GameProgress writes its saves.
        return AssetPath::GetWritableRoot() / "assets" / "saves"
            / "user_settings.wtsettings";
    }

    void UserSettings::Load()
    {
        UserSettingsData loaded = Defaults();
        const std::filesystem::path path = SettingsPath();
        std::ifstream input(path, std::ios::binary);
        if (input.is_open())
        {
            std::string line;
            while (std::getline(input, line))
            {
                const size_t split = line.find('=');
                if (split == std::string::npos)
                    continue;

                const std::string key = line.substr(0, split);
                const std::string value = line.substr(split + 1);

                if (key == "textSpeed") loaded.TextSpeed = std::clamp(ParseInt(value, loaded.TextSpeed), 12, 180);
                else if (key == "masterVolume") loaded.MasterVolume = std::clamp(ParseInt(value, loaded.MasterVolume), 0, 100);
                else if (key == "bgmVolume") loaded.BGMVolume = std::clamp(ParseInt(value, loaded.BGMVolume), 0, 100);
                else if (key == "sfxVolume") loaded.SFXVolume = std::clamp(ParseInt(value, loaded.SFXVolume), 0, 100);
                else if (key == "fullscreen") loaded.Fullscreen = ParseBool(value, loaded.Fullscreen);
                else if (key == "screenShake") loaded.ScreenShake = ParseBool(value, loaded.ScreenShake);
                else if (key == "editorLanguage") loaded.EditorLanguage = ParseInt(value, loaded.EditorLanguage);
                else if (key.rfind("key.", 0) == 0)
                {
                    std::vector<int> keys = ParseKeyList(value);
                    if (!keys.empty())
                        loaded.KeyBindings[key.substr(4)] = keys;
                }
            }
        }

        Set(loaded);
    }

    bool UserSettings::Save()
    {
        const std::filesystem::path path = SettingsPath();
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            WT_CORE_WARN("UserSettings: failed to write '{}'", path.string());
            return false;
        }

        const UserSettingsData& settings = Storage();
        output << "schema=wheatear.user_settings.v1\n";
        output << "textSpeed=" << settings.TextSpeed << "\n";
        output << "masterVolume=" << settings.MasterVolume << "\n";
        output << "bgmVolume=" << settings.BGMVolume << "\n";
        output << "sfxVolume=" << settings.SFXVolume << "\n";
        output << "fullscreen=" << (settings.Fullscreen ? 1 : 0) << "\n";
        output << "screenShake=" << (settings.ScreenShake ? 1 : 0) << "\n";
        output << "editorLanguage=" << settings.EditorLanguage << "\n";

        std::vector<std::string> actionIds;
        actionIds.reserve(settings.KeyBindings.size());
        for (const auto& [actionId, _] : settings.KeyBindings)
            actionIds.push_back(actionId);
        std::sort(actionIds.begin(), actionIds.end());

        for (const std::string& actionId : actionIds)
        {
            const auto it = settings.KeyBindings.find(actionId);
            if (it != settings.KeyBindings.end() && !it->second.empty())
                output << "key." << actionId << "=" << JoinKeyList(it->second) << "\n";
        }

        return true;
    }

    void UserSettings::ResetToDefaults()
    {
        Set(Defaults());
        Save();
        ApplyToRuntime();
    }

    void UserSettings::ApplyToRuntime()
    {
        if (!Application::Exists())
            return;

        Application::Get().GetWindow().SetFullscreen(Get().Fullscreen);
    }

} // namespace Wheatear
