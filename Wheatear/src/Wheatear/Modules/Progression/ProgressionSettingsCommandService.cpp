#include "wtpch.h"
#include "ProgressionSettingsCommandService.h"

#include "Wheatear/Config/UserSettings.h"

#include <algorithm>
#include <sstream>

namespace Wheatear::ProgressionSettingsCommandService {

    namespace {

        static float ParseFloat(const std::string& value, float fallback)
        {
            try
            {
                size_t consumed = 0;
                const float parsed = std::stof(value, &consumed);
                return consumed > 0 ? parsed : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        static void SetResult(GameProgress::CommandResult& result, GameProgress::State& state, const std::string& message, bool changed = true)
        {
            state.LastResultMessage = message;
            result.Handled = true;
            result.Success = true;
            result.Changed = changed;
        }

    } // namespace

    bool IsSettingsCommand(const std::string& action)
    {
        return action == "text_speed_up"
            || action == "text_speed_down"
            || action == "master_volume_up"
            || action == "master_volume_down"
            || action == "bgm_volume_up"
            || action == "bgm_volume_down"
            || action == "sfx_volume_up"
            || action == "sfx_volume_down"
            || action == "toggle_screen_shake"
            || action == "toggle_fullscreen"
            || action.rfind("set_text_speed:", 0) == 0
            || action.rfind("set_master_volume:", 0) == 0
            || action.rfind("set_bgm_volume:", 0) == 0
            || action.rfind("set_sfx_volume:", 0) == 0;
    }

    void ApplyToRuntime()
    {
        UserSettings::Save();
        UserSettings::ApplyToRuntime();
    }

    GameProgress::CommandResult Execute(const std::string& action, GameProgress::State& state)
    {
        GameProgress::CommandResult result;
        if (!IsSettingsCommand(action))
            return result;

        auto& settings = UserSettings::Get();
        if (action.rfind("set_text_speed:", 0) == 0)
        {
            settings.TextSpeed = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.TextSpeed)) + 0.5f), 12, 180);
            SetResult(result, state, "文字速度设置为 " + std::to_string(settings.TextSpeed) + " 字/秒。");
        }
        else if (action.rfind("set_master_volume:", 0) == 0)
        {
            settings.MasterVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(18), static_cast<float>(settings.MasterVolume)) + 0.5f), 0, 100);
            SetResult(result, state, "主音量设置为 " + std::to_string(settings.MasterVolume) + "%。");
        }
        else if (action.rfind("set_bgm_volume:", 0) == 0)
        {
            settings.BGMVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.BGMVolume)) + 0.5f), 0, 100);
            SetResult(result, state, "BGM 音量设置为 " + std::to_string(settings.BGMVolume) + "%。");
        }
        else if (action.rfind("set_sfx_volume:", 0) == 0)
        {
            settings.SFXVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.SFXVolume)) + 0.5f), 0, 100);
            SetResult(result, state, "音效音量设置为 " + std::to_string(settings.SFXVolume) + "%。");
        }
        else if (action == "text_speed_up")
        {
            settings.TextSpeed = std::min(180, settings.TextSpeed + 6);
            SetResult(result, state, "文字速度提高到 " + std::to_string(settings.TextSpeed) + " 字/秒。");
        }
        else if (action == "text_speed_down")
        {
            settings.TextSpeed = std::max(12, settings.TextSpeed - 6);
            SetResult(result, state, "文字速度降低到 " + std::to_string(settings.TextSpeed) + " 字/秒。");
        }
        else if (action == "master_volume_up")
        {
            settings.MasterVolume = std::min(100, settings.MasterVolume + 5);
            SetResult(result, state, "主音量 " + std::to_string(settings.MasterVolume) + "%。");
        }
        else if (action == "master_volume_down")
        {
            settings.MasterVolume = std::max(0, settings.MasterVolume - 5);
            SetResult(result, state, "主音量 " + std::to_string(settings.MasterVolume) + "%。");
        }
        else if (action == "bgm_volume_up")
        {
            settings.BGMVolume = std::min(100, settings.BGMVolume + 5);
            SetResult(result, state, "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。");
        }
        else if (action == "bgm_volume_down")
        {
            settings.BGMVolume = std::max(0, settings.BGMVolume - 5);
            SetResult(result, state, "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。");
        }
        else if (action == "sfx_volume_up")
        {
            settings.SFXVolume = std::min(100, settings.SFXVolume + 5);
            SetResult(result, state, "音效音量 " + std::to_string(settings.SFXVolume) + "%。");
        }
        else if (action == "sfx_volume_down")
        {
            settings.SFXVolume = std::max(0, settings.SFXVolume - 5);
            SetResult(result, state, "音效音量 " + std::to_string(settings.SFXVolume) + "%。");
        }
        else if (action == "toggle_screen_shake")
        {
            settings.ScreenShake = !settings.ScreenShake;
            SetResult(result, state, std::string("屏幕震动已") + (settings.ScreenShake ? "开启。" : "关闭。"));
        }
        else if (action == "toggle_fullscreen")
        {
            settings.Fullscreen = !settings.Fullscreen;
            SetResult(result, state, std::string("全屏偏好已") + (settings.Fullscreen ? "开启。" : "关闭。") + "已应用到当前窗口。");
        }

        if (result.Success && result.Changed)
            ApplyToRuntime();

        result.Message = state.LastResultMessage;
        return result;
    }

    std::string BuildStatusText()
    {
        const auto& settings = UserSettings::Get();
        std::ostringstream stream;
        stream << "文字速度: " << settings.TextSpeed << " 字/秒\n";
        stream << "主音量: " << settings.MasterVolume << "%\n";
        stream << "BGM 音量: " << settings.BGMVolume << "%\n";
        stream << "音效音量: " << settings.SFXVolume << "%\n";
        stream << "全屏偏好: " << (settings.Fullscreen ? "开" : "关") << "\n";
        stream << "屏幕震动: " << (settings.ScreenShake ? "开" : "关") << "\n\n";
        stream << "音量设置已接入 VN BGM、战斗 BGM 和战斗音效。";
        return stream.str();
    }

} // namespace Wheatear::ProgressionSettingsCommandService
