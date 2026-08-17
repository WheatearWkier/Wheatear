#include "wepch.h"
#include "CommandBuilder.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Wheatear::EditorCommandBuilder {

    const char* CommandKindLabel(CommandKind kind)
    {
        switch (kind)
        {
        case CommandKind::None: return EditorLocale::Text("None", "无");
        case CommandKind::Scene: return EditorLocale::Text("Scene Load", "加载场景");
        case CommandKind::Event: return EditorLocale::Text("Event", "事件");
        case CommandKind::NewGame: return EditorLocale::Text("New Game", "新游戏");
        case CommandKind::LoadGame: return EditorLocale::Text("Load Game", "读取存档");
        case CommandKind::GameSaveOpenSaveMenu: return EditorLocale::Text("Open Save Menu", "打开存档菜单");
        case CommandKind::GameSaveOpenLoadMenu: return EditorLocale::Text("Open Load Menu", "打开读档菜单");
        case CommandKind::GameSaveSlotSave: return EditorLocale::Text("Save Slot", "保存槽位");
        case CommandKind::GameSaveLoadSlot: return EditorLocale::Text("Load Slot", "读取槽位");
        case CommandKind::GameSaveClose: return EditorLocale::Text("Close Save UI", "关闭存档界面");
        case CommandKind::GameSaveConfirmOverwrite: return EditorLocale::Text("Confirm Overwrite", "确认覆盖");
        case CommandKind::GameSaveCancelOverwrite: return EditorLocale::Text("Cancel Overwrite", "取消覆盖");
        case CommandKind::GameSavePushAllowAll: return EditorLocale::Text("Push Allow Save/Load", "临时允许存读");
        case CommandKind::GameSavePushBlockAll: return EditorLocale::Text("Push Block Save/Load", "临时禁止存读");
        case CommandKind::GameSavePopPolicy: return EditorLocale::Text("Restore Save Policy", "恢复存档策略");
        case CommandKind::GameSaveClearPolicy: return EditorLocale::Text("Clear Save Policy", "清空临时存档策略");
        case CommandKind::ProgressionSetFlag: return EditorLocale::Text("Set Story Flag", "设置剧情标记");
        case CommandKind::ProgressionClearFlag: return EditorLocale::Text("Clear Story Flag", "清除剧情标记");
        case CommandKind::ProgressionSetActiveDungeon: return EditorLocale::Text("Set Active Dungeon", "设置当前地牢");
        case CommandKind::ProgressionClearActiveDungeon: return EditorLocale::Text("Clear Active Dungeon", "清除当前地牢");
        case CommandKind::ProgressionSetChapter: return EditorLocale::Text("Set Chapter", "设置章节");
        case CommandKind::UiPager: return EditorLocale::Text("UI Pager", "UI 分页器");
        case CommandKind::Anim: return EditorLocale::Text("Animation", "动画");
        case CommandKind::VnAuto: return EditorLocale::Text("VN: Toggle Auto", "VN: 自动播放开关");
        case CommandKind::VnHistory: return EditorLocale::Text("VN: Toggle History", "VN: 历史记录开关");
        case CommandKind::VnSettings: return EditorLocale::Text("VN: Toggle Settings", "VN: 设置开关");
        case CommandKind::VnClose: return EditorLocale::Text("VN: Close Panels", "VN: 关闭面板");
        case CommandKind::VnHide: return EditorLocale::Text("VN: Hide Dialogue", "VN: 隐藏对白");
        case CommandKind::VnSaveMenu: return EditorLocale::Text("VN: Open Save Menu", "VN: 打开存档菜单");
        case CommandKind::VnLoadMenu: return EditorLocale::Text("VN: Open Load Menu", "VN: 打开读档菜单");
        case CommandKind::VnConfirmOverwrite: return EditorLocale::Text("VN: Confirm Overwrite", "VN: 确认覆盖");
        case CommandKind::VnCancelOverwrite: return EditorLocale::Text("VN: Cancel Overwrite", "VN: 取消覆盖");
        case CommandKind::VnTextSpeedUp: return EditorLocale::Text("VN: Text Speed +", "VN: 文字速度 +");
        case CommandKind::VnTextSpeedDown: return EditorLocale::Text("VN: Text Speed -", "VN: 文字速度 -");
        case CommandKind::VnAutoDelayUp: return EditorLocale::Text("VN: Auto Delay +", "VN: 自动延迟 +");
        case CommandKind::VnAutoDelayDown: return EditorLocale::Text("VN: Auto Delay -", "VN: 自动延迟 -");
        case CommandKind::VnAdvance: return EditorLocale::Text("VN: Advance", "VN: 推进剧情");
        case CommandKind::VnSkip: return EditorLocale::Text("VN: Toggle Skip", "VN: 快进开关");
        case CommandKind::TurnMenu: return EditorLocale::Text("Turn: Menu Page", "回合: 菜单页");
        case CommandKind::TurnCancel: return EditorLocale::Text("Turn: Cancel", "回合: 取消");
        case CommandKind::TurnWait: return EditorLocale::Text("Turn: Wait", "回合: 待机");
        case CommandKind::TurnGuard: return EditorLocale::Text("Turn: Guard", "回合: 防御");
        case CommandKind::TurnItem: return EditorLocale::Text("Turn: Use Item", "回合: 使用道具");
        case CommandKind::TurnSkill: return EditorLocale::Text("Turn: Use Skill", "回合: 使用技能");
        case CommandKind::TurnTarget: return EditorLocale::Text("Turn: Select Target", "回合: 选择目标");
        case CommandKind::TacticCell: return EditorLocale::Text("Tactic: Select Cell", "战棋: 选择格子");
        case CommandKind::TacticMenu: return EditorLocale::Text("Tactic: Menu Page", "战棋: 菜单页");
        case CommandKind::TacticCancel: return EditorLocale::Text("Tactic: Cancel", "战棋: 取消");
        case CommandKind::TacticSkill: return EditorLocale::Text("Tactic: Use Skill", "战棋: 使用技能");
        case CommandKind::SideItem1: return EditorLocale::Text("Side: Item Slot 1", "横版: 道具槽 1");
        case CommandKind::SideItem2: return EditorLocale::Text("Side: Item Slot 2", "横版: 道具槽 2");
        case CommandKind::SideItem3: return EditorLocale::Text("Side: Item Slot 3", "横版: 道具槽 3");
        case CommandKind::SideBasic: return EditorLocale::Text("Side: Basic Attack", "横版: 普通攻击");
        case CommandKind::SideLauncher: return EditorLocale::Text("Side: Launcher", "横版: 挑空");
        case CommandKind::SideMagic: return EditorLocale::Text("Side: Magic Bolt", "横版: 魔法弹");
        case CommandKind::SideSupport: return EditorLocale::Text("Side: Support", "横版: 支援");
        case CommandKind::SideDash: return EditorLocale::Text("Side: Dash", "横版: 冲刺");
        case CommandKind::SideBreakLimit: return EditorLocale::Text("Side: Break Limit", "横版: 破限");
        case CommandKind::ProgUpgradeTravelerArmor: return EditorLocale::Text("Progression: Upgrade Armor", "成长: 升级护甲");
        case CommandKind::ProgUpgradeItem: return EditorLocale::Text("Progression: Upgrade Item", "成长: 升级装备");
        case CommandKind::ProgLearnSelectedSkill: return EditorLocale::Text("Progression: Learn Skill", "成长: 学习技能");
        case CommandKind::ProgSelectSkillNode: return EditorLocale::Text("Progression: Select Skill Node", "成长: 选择技能节点");
        case CommandKind::ProgEquipmentPageSlider: return EditorLocale::Text("Progression: Equipment Page Slider", "成长: 装备页滑条");
        case CommandKind::ProgEquipmentPage1: return EditorLocale::Text("Progression: Equipment Page 1", "成长: 装备页 1");
        case CommandKind::ProgEquipmentPage2: return EditorLocale::Text("Progression: Equipment Page 2", "成长: 装备页 2");
        case CommandKind::ProgSelectEquipmentSlot: return EditorLocale::Text("Progression: Select Equip Slot", "成长: 选择装备槽");
        case CommandKind::ProgToggleSelectedEquipment: return EditorLocale::Text("Progression: Toggle Equip", "成长: 穿脱装备");
        case CommandKind::ProgSelectEquipment: return EditorLocale::Text("Progression: Select Equipment", "成长: 选择装备");
        case CommandKind::ProgReset: return EditorLocale::Text("Progression: Reset (New Game)", "成长: 重置（新游戏）");
        case CommandKind::ProgSelectSupport: return EditorLocale::Text("Progression: Select Support", "成长: 配置支援");
        case CommandKind::ProgTextSpeedUp: return EditorLocale::Text("Settings: Text Speed +", "设置: 文字速度 +");
        case CommandKind::ProgTextSpeedDown: return EditorLocale::Text("Settings: Text Speed -", "设置: 文字速度 -");
        case CommandKind::ProgMasterVolumeUp: return EditorLocale::Text("Settings: Master Volume +", "设置: 主音量 +");
        case CommandKind::ProgMasterVolumeDown: return EditorLocale::Text("Settings: Master Volume -", "设置: 主音量 -");
        case CommandKind::ProgBgmVolumeUp: return EditorLocale::Text("Settings: BGM Volume +", "设置: BGM 音量 +");
        case CommandKind::ProgBgmVolumeDown: return EditorLocale::Text("Settings: BGM Volume -", "设置: BGM 音量 -");
        case CommandKind::ProgSfxVolumeUp: return EditorLocale::Text("Settings: SFX Volume +", "设置: 音效音量 +");
        case CommandKind::ProgSfxVolumeDown: return EditorLocale::Text("Settings: SFX Volume -", "设置: 音效音量 -");
        case CommandKind::ProgToggleScreenShake: return EditorLocale::Text("Settings: Toggle Screen Shake", "设置: 震屏开关");
        case CommandKind::ProgToggleFullscreen: return EditorLocale::Text("Settings: Toggle Fullscreen", "设置: 全屏开关");
        case CommandKind::ProgSetTextSpeed: return EditorLocale::Text("Settings: Set Text Speed", "设置: 设定文字速度");
        case CommandKind::ProgSetMasterVolume: return EditorLocale::Text("Settings: Set Master Volume", "设置: 设定主音量");
        case CommandKind::ProgSetBgmVolume: return EditorLocale::Text("Settings: Set BGM Volume", "设置: 设定 BGM 音量");
        case CommandKind::ProgSetSfxVolume: return EditorLocale::Text("Settings: Set SFX Volume", "设置: 设定音效音量");
        case CommandKind::Quit: return EditorLocale::Text("Quit", "退出");
        case CommandKind::Raw: return EditorLocale::Text("Raw Command", "原始命令");
        }
        return EditorLocale::Text("Raw Command", "原始命令");
    }

    std::string FormatSeek(float seconds)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << std::max(0.0f, seconds);
        return stream.str();
    }

    std::string FormatFloat(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(1) << value;
        return stream.str();
    }

    std::string BuildCommand(const CommandSpec& spec)
    {
        switch (spec.Kind)
        {
        case CommandKind::None: return {};
        case CommandKind::Scene: return "scene:" + spec.Primary;
        case CommandKind::Event:
            return spec.Target.empty()
                ? "event:" + spec.Primary
                : "event:" + spec.Target + ":" + spec.Primary;
        case CommandKind::NewGame: return "newgame:" + spec.Primary;
        case CommandKind::LoadGame: return "loadgame:" + spec.Primary + ":" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveOpenSaveMenu: return "gamesave:open_save_menu";
        case CommandKind::GameSaveOpenLoadMenu: return "gamesave:open_load_menu";
        case CommandKind::GameSaveSlotSave: return "gamesave:slot_save_" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveLoadSlot: return "gamesave:load_" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveClose: return "gamesave:close";
        case CommandKind::GameSaveConfirmOverwrite: return "gamesave:confirm_overwrite";
        case CommandKind::GameSaveCancelOverwrite: return "gamesave:cancel_overwrite";
        case CommandKind::GameSavePushAllowAll: return "gamesave:push_policy:save=1:load=1";
        case CommandKind::GameSavePushBlockAll: return "gamesave:push_policy:save=0:load=0";
        case CommandKind::GameSavePopPolicy: return "gamesave:pop_policy";
        case CommandKind::GameSaveClearPolicy: return "gamesave:clear_policy";
        case CommandKind::ProgressionSetFlag: return "progression:set_flag:" + spec.Primary;
        case CommandKind::ProgressionClearFlag: return "progression:clear_flag:" + spec.Primary;
        case CommandKind::ProgressionSetActiveDungeon: return "progression:set_active_dungeon:" + spec.Primary;
        case CommandKind::ProgressionClearActiveDungeon: return "progression:clear_active_dungeon";
        case CommandKind::ProgressionSetChapter: return "progression:set_chapter:" + std::to_string(std::max(1, spec.Number));
        case CommandKind::UiPager:
        {
            // Grammar: ui:pager:@UUID:action[:page]
            std::string action = spec.Secondary.empty() ? "next" : spec.Secondary;
            std::string result = "ui:pager:" + spec.Primary + ":" + action;
            if (action == "page" || action == "set")
                result += ":" + std::to_string(std::max(1, spec.Number));
            return result;
        }
        case CommandKind::Anim:
        {
            // Grammar: anim:action:@UUID[:arg]  - action FIRST, selector SECOND (opposite of ui:pager)
            std::string action = spec.Secondary.empty() ? "play" : spec.Secondary;
            if (action == "seek")
                return "anim:seek:" + spec.Primary + ":" + FormatSeek(spec.FloatValue);
            if (action == "play")
            {
                if (spec.Raw.empty())
                    return "anim:play:" + spec.Primary;
                return "anim:play:" + spec.Primary + ":" + spec.Raw;
            }
            return "anim:" + action + ":" + spec.Primary;
        }
                // ---- vn: family ----
        case CommandKind::VnAuto: return "vn:auto";
        case CommandKind::VnHistory: return "vn:history";
        case CommandKind::VnSettings: return "vn:settings";
        case CommandKind::VnClose: return "vn:close";
        case CommandKind::VnHide: return "vn:hide";
        case CommandKind::VnSaveMenu: return "vn:savemenu";
        case CommandKind::VnLoadMenu: return "vn:loadmenu";
        case CommandKind::VnConfirmOverwrite: return "vn:confirm_overwrite";
        case CommandKind::VnCancelOverwrite: return "vn:cancel_overwrite";
        case CommandKind::VnTextSpeedUp: return "vn:textspeed+";
        case CommandKind::VnTextSpeedDown: return "vn:textspeed-";
        case CommandKind::VnAutoDelayUp: return "vn:autodelay+";
        case CommandKind::VnAutoDelayDown: return "vn:autodelay-";
        case CommandKind::VnAdvance: return "vn:advance";
        case CommandKind::VnSkip: return "vn:skip";

        // ---- turn: family ----
        case CommandKind::TurnMenu: return "turn:menu:" + spec.Primary;
        case CommandKind::TurnCancel: return "turn:cancel";
        case CommandKind::TurnWait: return "turn:wait";
        case CommandKind::TurnGuard: return "turn:guard";
        case CommandKind::TurnItem: return "turn:item:" + spec.Primary;
        case CommandKind::TurnSkill: return "turn:skill:" + spec.Primary;
        case CommandKind::TurnTarget: return "turn:target:" + spec.Primary;

        // ---- tactic: family ----
        case CommandKind::TacticCell:
            return "tactic:cell:" + std::to_string(std::max(0, spec.Number))
                + ":" + std::to_string(std::max(0, spec.Number2));
        case CommandKind::TacticMenu: return "tactic:menu:" + spec.Primary;
        case CommandKind::TacticCancel: return "tactic:cancel";
        case CommandKind::TacticSkill: return "tactic:skill:" + spec.Primary;

        // ---- side: family ----
        case CommandKind::SideItem1: return "side:item:1";
        case CommandKind::SideItem2: return "side:item:2";
        case CommandKind::SideItem3: return "side:item:3";
        case CommandKind::SideBasic: return "side:basic";
        case CommandKind::SideLauncher: return "side:launcher";
        case CommandKind::SideMagic: return "side:magic";
        case CommandKind::SideSupport: return "side:support";
        case CommandKind::SideDash: return "side:dash";
        case CommandKind::SideBreakLimit: return "side:break_limit";

        // ---- progression: long tail ----
        case CommandKind::ProgUpgradeTravelerArmor: return "progression:upgrade_traveler_armor";
        case CommandKind::ProgUpgradeItem: return "progression:upgrade_item:" + spec.Primary;
        case CommandKind::ProgLearnSelectedSkill: return "progression:learn_selected_skill";
        case CommandKind::ProgSelectSkillNode: return "progression:select_skill_node:" + spec.Primary;
        case CommandKind::ProgEquipmentPageSlider:
            return "progression:equipment_page_slider:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgEquipmentPage1: return "progression:equipment_page_1";
        case CommandKind::ProgEquipmentPage2: return "progression:equipment_page_2";
        case CommandKind::ProgSelectEquipmentSlot: return "progression:select_equipment_slot:" + spec.Primary;
        case CommandKind::ProgToggleSelectedEquipment: return "progression:toggle_selected_equipment";
        case CommandKind::ProgSelectEquipment: return "progression:select_equipment_" + spec.Primary;
        case CommandKind::ProgReset: return "progression:reset";
        case CommandKind::ProgSelectSupport: return "progression:select_support:" + spec.Primary;
        case CommandKind::ProgTextSpeedUp: return "progression:text_speed_up";
        case CommandKind::ProgTextSpeedDown: return "progression:text_speed_down";
        case CommandKind::ProgMasterVolumeUp: return "progression:master_volume_up";
        case CommandKind::ProgMasterVolumeDown: return "progression:master_volume_down";
        case CommandKind::ProgBgmVolumeUp: return "progression:bgm_volume_up";
        case CommandKind::ProgBgmVolumeDown: return "progression:bgm_volume_down";
        case CommandKind::ProgSfxVolumeUp: return "progression:sfx_volume_up";
        case CommandKind::ProgSfxVolumeDown: return "progression:sfx_volume_down";
        case CommandKind::ProgToggleScreenShake: return "progression:toggle_screen_shake";
        case CommandKind::ProgToggleFullscreen: return "progression:toggle_fullscreen";
        case CommandKind::ProgSetTextSpeed: return "progression:set_text_speed:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetMasterVolume: return "progression:set_master_volume:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetBgmVolume: return "progression:set_bgm_volume:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetSfxVolume: return "progression:set_sfx_volume:" + FormatFloat(spec.FloatValue);

        case CommandKind::Quit: return "quit";
        case CommandKind::Raw: return spec.Raw;
        }
        return spec.Raw;
    }

    CommandSpec ParseCommand(const std::string& command)
    {
        CommandSpec spec;
        spec.Raw = command;

        // Local split-on-':' mirroring CommandBus::SplitCommand (runtime, CommandBus.cpp).
        auto splitColon = [](const std::string& value)
        {
            std::vector<std::string> parts;
            std::string current;
            for (char c : value)
            {
                if (c == ':')
                {
                    parts.push_back(current);
                    current.clear();
                }
                else
                    current.push_back(c);
            }
            parts.push_back(current);
            return parts;
        };

        if (command.empty())
            return spec;

        if (command == "quit")
        {
            spec.Kind = CommandKind::Quit;
            return spec;
        }

        if (StartsWith(command, "scene:"))
        {
            spec.Kind = CommandKind::Scene;
            spec.Primary = PayloadAfter(command, "scene:");
            return spec;
        }

        if (StartsWith(command, "event:"))
        {
            spec.Kind = CommandKind::Event;
            spec.Primary = PayloadAfter(command, "event:");
            // Grammar: event:@UUID:name -> direct call to one entity; the
            // selector is kept in Target so the builder UI can edit both.
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 3 && parts[1].size() > 1 && parts[1].front() == '@')
            {
                spec.Target = parts[1];
                spec.Primary = parts[2];
            }
            return spec;
        }

        if (StartsWith(command, "newgame:"))
        {
            spec.Kind = CommandKind::NewGame;
            spec.Primary = PayloadAfter(command, "newgame:");
            return spec;
        }

        if (StartsWith(command, "loadgame:"))
        {
            spec.Kind = CommandKind::LoadGame;
            spec.Primary = PayloadAfter(command, "loadgame:");
            const size_t slotSeparator = spec.Primary.rfind(':');
            if (slotSeparator != std::string::npos)
            {
                int parsedSlot = 1;
                if (TryParsePositiveInt(spec.Primary.substr(slotSeparator + 1), parsedSlot))
                {
                    spec.Number = parsedSlot;
                    spec.Primary = spec.Primary.substr(0, slotSeparator);
                }
            }
            return spec;
        }

        if (command == "gamesave:open_save_menu")
        {
            spec.Kind = CommandKind::GameSaveOpenSaveMenu;
            return spec;
        }

        if (command == "gamesave:open_load_menu")
        {
            spec.Kind = CommandKind::GameSaveOpenLoadMenu;
            return spec;
        }

        if (command == "gamesave:close")
        {
            spec.Kind = CommandKind::GameSaveClose;
            return spec;
        }

        if (command == "gamesave:confirm_overwrite")
        {
            spec.Kind = CommandKind::GameSaveConfirmOverwrite;
            return spec;
        }

        if (command == "gamesave:cancel_overwrite")
        {
            spec.Kind = CommandKind::GameSaveCancelOverwrite;
            return spec;
        }

        if (command == "gamesave:push_policy:save=1:load=1")
        {
            spec.Kind = CommandKind::GameSavePushAllowAll;
            return spec;
        }

        if (command == "gamesave:push_policy:save=0:load=0")
        {
            spec.Kind = CommandKind::GameSavePushBlockAll;
            return spec;
        }

        if (command == "gamesave:pop_policy")
        {
            spec.Kind = CommandKind::GameSavePopPolicy;
            return spec;
        }

        if (command == "gamesave:clear_policy")
        {
            spec.Kind = CommandKind::GameSaveClearPolicy;
            return spec;
        }

        if (StartsWith(command, "gamesave:slot_save_"))
        {
            spec.Kind = CommandKind::GameSaveSlotSave;
            TryParsePositiveInt(PayloadAfter(command, "gamesave:slot_save_"), spec.Number);
            return spec;
        }

        if (StartsWith(command, "gamesave:load_"))
        {
            spec.Kind = CommandKind::GameSaveLoadSlot;
            TryParsePositiveInt(PayloadAfter(command, "gamesave:load_"), spec.Number);
            return spec;
        }

        if (StartsWith(command, "progression:set_flag:"))
        {
            spec.Kind = CommandKind::ProgressionSetFlag;
            spec.Primary = PayloadAfter(command, "progression:set_flag:");
            return spec;
        }

        if (StartsWith(command, "progression:clear_flag:"))
        {
            spec.Kind = CommandKind::ProgressionClearFlag;
            spec.Primary = PayloadAfter(command, "progression:clear_flag:");
            return spec;
        }

        if (StartsWith(command, "progression:set_active_dungeon:"))
        {
            spec.Kind = CommandKind::ProgressionSetActiveDungeon;
            spec.Primary = PayloadAfter(command, "progression:set_active_dungeon:");
            return spec;
        }

        if (command == "progression:clear_active_dungeon")
        {
            spec.Kind = CommandKind::ProgressionClearActiveDungeon;
            return spec;
        }

        if (StartsWith(command, "progression:set_chapter:"))
        {
            spec.Kind = CommandKind::ProgressionSetChapter;
            TryParsePositiveInt(PayloadAfter(command, "progression:set_chapter:"), spec.Number);
            return spec;
        }

        // Grammar: ui:pager:@UUID:action[:page]  (selector at parts[2], action at parts[3])
        if (StartsWith(command, "ui:pager:"))
        {
            spec.Kind = CommandKind::UiPager;
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 3) spec.Primary = parts[2];        // @UUID
            if (parts.size() >= 4) spec.Secondary = parts[3];     // action
            if (parts.size() >= 5 && (parts[3] == "page" || parts[3] == "set"))
                TryParsePositiveInt(parts[4], spec.Number);
            return spec;
        }

        // Grammar: anim:action:@UUID[:arg]  (action at parts[1], selector at parts[2])
        if (StartsWith(command, "anim:"))
        {
            spec.Kind = CommandKind::Anim;
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2) spec.Secondary = parts[1];     // action
            if (parts.size() >= 3) spec.Primary = parts[2];       // @UUID
            if (parts.size() >= 4)
            {
                if (spec.Secondary == "play")
                    spec.Raw = parts[3];                          // clip name
                else if (spec.Secondary == "seek")
                {
                    try { spec.FloatValue = std::stof(parts[3]); }
                    catch (...) { spec.FloatValue = 0.0f; }
                }
            }
            return spec;
        }

        // ---- vn: family ----
        if (StartsWith(command, "vn:"))
        {
            const std::string action = command.substr(3);
            if (action == "auto") spec.Kind = CommandKind::VnAuto;
            else if (action == "history") spec.Kind = CommandKind::VnHistory;
            else if (action == "settings") spec.Kind = CommandKind::VnSettings;
            else if (action == "close") spec.Kind = CommandKind::VnClose;
            else if (action == "hide") spec.Kind = CommandKind::VnHide;
            else if (action == "savemenu") spec.Kind = CommandKind::VnSaveMenu;
            else if (action == "loadmenu") spec.Kind = CommandKind::VnLoadMenu;
            else if (action == "confirm_overwrite") spec.Kind = CommandKind::VnConfirmOverwrite;
            else if (action == "cancel_overwrite") spec.Kind = CommandKind::VnCancelOverwrite;
            else if (action == "textspeed+" || action == "speed+") spec.Kind = CommandKind::VnTextSpeedUp;
            else if (action == "textspeed-" || action == "speed-") spec.Kind = CommandKind::VnTextSpeedDown;
            else if (action == "autodelay+") spec.Kind = CommandKind::VnAutoDelayUp;
            else if (action == "autodelay-") spec.Kind = CommandKind::VnAutoDelayDown;
            else if (action == "advance") spec.Kind = CommandKind::VnAdvance;
            else if (action == "skip") spec.Kind = CommandKind::VnSkip;
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- turn: family ----
        if (StartsWith(command, "turn:"))
        {
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2)
            {
                const std::string action = parts[1];
                if (action == "menu" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnMenu;
                    spec.Primary = parts[2];
                }
                else if (action == "cancel") spec.Kind = CommandKind::TurnCancel;
                else if (action == "wait") spec.Kind = CommandKind::TurnWait;
                else if (action == "guard") spec.Kind = CommandKind::TurnGuard;
                else if (action == "item" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnItem;
                    spec.Primary = parts[2];
                }
                else if (action == "skill" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnSkill;
                    spec.Primary = parts[2];
                }
                else if (action == "target" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnTarget;
                    spec.Primary = parts[2];
                }
                else spec.Kind = CommandKind::Raw;
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- tactic: family ----
        if (StartsWith(command, "tactic:"))
        {
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2)
            {
                const std::string action = parts[1];
                if (action == "cell" && parts.size() >= 4)
                {
                    spec.Kind = CommandKind::TacticCell;
                    try { spec.Number = std::stoi(parts[2]); } catch (...) { spec.Number = 0; }
                    try { spec.Number2 = std::stoi(parts[3]); } catch (...) { spec.Number2 = 0; }
                }
                else if (action == "menu" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TacticMenu;
                    spec.Primary = parts[2];
                }
                else if (action == "cancel") spec.Kind = CommandKind::TacticCancel;
                else if (action == "skill" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TacticSkill;
                    spec.Primary = parts[2];
                }
                else spec.Kind = CommandKind::Raw;
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- side: family ----
        if (StartsWith(command, "side:"))
        {
            if (command == "side:item:1") spec.Kind = CommandKind::SideItem1;
            else if (command == "side:item:2") spec.Kind = CommandKind::SideItem2;
            else if (command == "side:item:3") spec.Kind = CommandKind::SideItem3;
            else if (command == "side:basic") spec.Kind = CommandKind::SideBasic;
            else if (command == "side:launcher") spec.Kind = CommandKind::SideLauncher;
            else if (command == "side:magic") spec.Kind = CommandKind::SideMagic;
            else if (command == "side:support") spec.Kind = CommandKind::SideSupport;
            else if (command == "side:dash") spec.Kind = CommandKind::SideDash;
            else if (command == "side:break_limit") spec.Kind = CommandKind::SideBreakLimit;
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- progression: long tail ----
        if (StartsWith(command, "progression:"))
        {
            const std::string action = command.substr(12);
            if (action == "upgrade_traveler_armor") spec.Kind = CommandKind::ProgUpgradeTravelerArmor;
            else if (StartsWith(action, "upgrade_item:"))
            {
                spec.Kind = CommandKind::ProgUpgradeItem;
                spec.Primary = action.substr(13);
            }
            else if (action == "learn_selected_skill") spec.Kind = CommandKind::ProgLearnSelectedSkill;
            else if (StartsWith(action, "select_skill_node:"))
            {
                spec.Kind = CommandKind::ProgSelectSkillNode;
                spec.Primary = action.substr(18);
            }
            else if (StartsWith(action, "equipment_page_slider:"))
            {
                spec.Kind = CommandKind::ProgEquipmentPageSlider;
                try { spec.FloatValue = std::stof(action.substr(22)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (action == "equipment_page_1") spec.Kind = CommandKind::ProgEquipmentPage1;
            else if (action == "equipment_page_2") spec.Kind = CommandKind::ProgEquipmentPage2;
            else if (StartsWith(action, "select_equipment_slot:"))
            {
                spec.Kind = CommandKind::ProgSelectEquipmentSlot;
                spec.Primary = action.substr(22);
            }
            else if (action == "toggle_selected_equipment") spec.Kind = CommandKind::ProgToggleSelectedEquipment;
            else if (StartsWith(action, "select_equipment_"))
            {
                spec.Kind = CommandKind::ProgSelectEquipment;
                spec.Primary = action.substr(17);
            }
            else if (action == "reset") spec.Kind = CommandKind::ProgReset;
            else if (StartsWith(action, "select_support:"))
            {
                spec.Kind = CommandKind::ProgSelectSupport;
                spec.Primary = action.substr(15);
            }
            // Legacy select_support_<name> commands; map onto the table-driven
            // form so old scene buttons still render and work.
            else if (action == "select_support_mentor")
            {
                spec.Kind = CommandKind::ProgSelectSupport;
                spec.Primary = "mentor";
            }
            else if (action == "select_support_white_mage")
            {
                spec.Kind = CommandKind::ProgSelectSupport;
                spec.Primary = "white_mage";
            }
            else if (action == "select_support_guard")
            {
                spec.Kind = CommandKind::ProgSelectSupport;
                spec.Primary = "shield_guard";
            }
            else if (action == "select_support_black_mage")
            {
                spec.Kind = CommandKind::ProgSelectSupport;
                spec.Primary = "black_mage";
            }
            else if (action == "text_speed_up") spec.Kind = CommandKind::ProgTextSpeedUp;
            else if (action == "text_speed_down") spec.Kind = CommandKind::ProgTextSpeedDown;
            else if (action == "master_volume_up") spec.Kind = CommandKind::ProgMasterVolumeUp;
            else if (action == "master_volume_down") spec.Kind = CommandKind::ProgMasterVolumeDown;
            else if (action == "bgm_volume_up") spec.Kind = CommandKind::ProgBgmVolumeUp;
            else if (action == "bgm_volume_down") spec.Kind = CommandKind::ProgBgmVolumeDown;
            else if (action == "sfx_volume_up") spec.Kind = CommandKind::ProgSfxVolumeUp;
            else if (action == "sfx_volume_down") spec.Kind = CommandKind::ProgSfxVolumeDown;
            else if (action == "toggle_screen_shake") spec.Kind = CommandKind::ProgToggleScreenShake;
            else if (action == "toggle_fullscreen") spec.Kind = CommandKind::ProgToggleFullscreen;
            else if (StartsWith(action, "set_text_speed:"))
            {
                spec.Kind = CommandKind::ProgSetTextSpeed;
                try { spec.FloatValue = std::stof(action.substr(15)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (StartsWith(action, "set_master_volume:"))
            {
                spec.Kind = CommandKind::ProgSetMasterVolume;
                try { spec.FloatValue = std::stof(action.substr(18)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (StartsWith(action, "set_bgm_volume:") || StartsWith(action, "set_sfx_volume:"))
            {
                spec.Kind = StartsWith(action, "set_bgm_volume:")
                    ? CommandKind::ProgSetBgmVolume : CommandKind::ProgSetSfxVolume;
                try { spec.FloatValue = std::stof(action.substr(15)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        spec.Kind = CommandKind::Raw;
        return spec;
    }

    void SeedCommandKind(CommandSpec& spec, CommandKind kind, const std::string& currentCommand)
    {
        spec.Kind = kind;
        spec.Raw = currentCommand;
        spec.Number = std::max(1, spec.Number);

        if (kind == CommandKind::Scene || kind == CommandKind::NewGame || kind == CommandKind::LoadGame)
        {
            if (spec.Primary.empty() && !SceneChoices().empty())
                spec.Primary = SceneChoices().front();
        }
        else if (kind == CommandKind::Event)
        {
            if (spec.Primary.empty() && !EventChoices().empty())
                spec.Primary = EventChoices().front();
        }
        else
        {
            spec.Target.clear();
            // Defaults for commands that carry arguments.
            if (kind == CommandKind::TurnMenu)
            {
                if (spec.Primary.empty()) spec.Primary = "root";
            }
            else if (kind == CommandKind::TurnItem || kind == CommandKind::TurnSkill)
            {
                if (spec.Primary.empty()) spec.Primary = "basic";
            }
            else if (kind == CommandKind::TacticCell)
            {
                spec.Number = std::max(0, spec.Number);
                spec.Number2 = std::max(0, spec.Number2);
            }
            else if (kind == CommandKind::TacticMenu)
            {
                if (spec.Primary.empty()) spec.Primary = "move";
            }
            else if (kind == CommandKind::TacticSkill)
            {
                if (spec.Primary.empty()) spec.Primary = "basic";
            }
            else if (kind == CommandKind::ProgSelectSkillNode)
            {
                if (spec.Primary.empty()) spec.Primary = "magic_sword_core";
            }
            else if (kind == CommandKind::ProgSelectEquipmentSlot)
            {
                if (spec.Primary.empty()) spec.Primary = "weapon";
            }
            else if (kind == CommandKind::ProgSetTextSpeed)
                spec.FloatValue = std::clamp(spec.FloatValue, 12.0f, 180.0f);
            else if (kind == CommandKind::ProgSetMasterVolume
                || kind == CommandKind::ProgSetBgmVolume
                || kind == CommandKind::ProgSetSfxVolume)
                spec.FloatValue = std::clamp(spec.FloatValue, 0.0f, 100.0f);
        }
    }

    bool DrawOptionPicker(
        const char* label,
        std::string& value,
        const std::vector<std::string>& options,
        size_t capacity)
    {
        bool changed = EditorWidgets::InputString(label, value, capacity);

        const std::string comboLabel = std::string("Pick ") + label;
        if (ImGui::BeginCombo(comboLabel.c_str(), value.empty() ? "(select)" : value.c_str()))
        {
            for (size_t i = 0; i < options.size(); ++i)
            {
                const bool selected = options[i] == value;
                const std::string itemLabel = EditorWidgets::LabelWithId(options[i], std::string(label) + ":" + std::to_string(i));
                if (ImGui::Selectable(itemLabel.c_str(), selected))
                {
                    value = options[i];
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh Assets"))
            RefreshAssetChoices();

        return changed;
    }

    bool DrawCommandBuilder(const char* label, std::string& command, size_t rawCapacity)
    {
        bool changed = false;
        CommandSpec spec = ParseCommand(command);
        const char* displayLabel = label ? label : "Command";

        ImGui::PushID(displayLabel);
        ImGui::TextDisabled("%s", displayLabel);

        static const CommandKind kinds[] = {
            CommandKind::None,
            CommandKind::Scene,
            CommandKind::Event,
            CommandKind::NewGame,
            CommandKind::LoadGame,
            CommandKind::GameSaveOpenSaveMenu,
            CommandKind::GameSaveOpenLoadMenu,
            CommandKind::GameSaveSlotSave,
            CommandKind::GameSaveLoadSlot,
            CommandKind::GameSaveClose,
            CommandKind::GameSaveConfirmOverwrite,
            CommandKind::GameSaveCancelOverwrite,
            CommandKind::GameSavePushAllowAll,
            CommandKind::GameSavePushBlockAll,
            CommandKind::GameSavePopPolicy,
            CommandKind::GameSaveClearPolicy,
            CommandKind::ProgressionSetFlag,
            CommandKind::ProgressionClearFlag,
            CommandKind::ProgressionSetActiveDungeon,
            CommandKind::ProgressionClearActiveDungeon,
            CommandKind::ProgressionSetChapter,
            CommandKind::UiPager,
            CommandKind::Anim,
            CommandKind::VnAuto, CommandKind::VnHistory, CommandKind::VnSettings,
            CommandKind::VnClose, CommandKind::VnHide,
            CommandKind::VnSaveMenu, CommandKind::VnLoadMenu,
            CommandKind::VnConfirmOverwrite, CommandKind::VnCancelOverwrite,
            CommandKind::VnTextSpeedUp, CommandKind::VnTextSpeedDown,
            CommandKind::VnAutoDelayUp, CommandKind::VnAutoDelayDown,
            CommandKind::VnAdvance, CommandKind::VnSkip,
            CommandKind::TurnMenu, CommandKind::TurnCancel, CommandKind::TurnWait,
            CommandKind::TurnGuard, CommandKind::TurnItem, CommandKind::TurnSkill,
            CommandKind::TurnTarget,
            CommandKind::TacticCell, CommandKind::TacticMenu, CommandKind::TacticCancel,
            CommandKind::TacticSkill,
            CommandKind::SideItem1, CommandKind::SideItem2, CommandKind::SideItem3,
            CommandKind::SideBasic, CommandKind::SideLauncher, CommandKind::SideMagic,
            CommandKind::SideSupport, CommandKind::SideDash, CommandKind::SideBreakLimit,
            CommandKind::ProgUpgradeTravelerArmor, CommandKind::ProgLearnSelectedSkill,
            CommandKind::ProgSelectSkillNode,
            CommandKind::ProgEquipmentPageSlider, CommandKind::ProgEquipmentPage1,
            CommandKind::ProgEquipmentPage2,
            CommandKind::ProgSelectEquipmentSlot, CommandKind::ProgToggleSelectedEquipment,
            CommandKind::ProgSelectEquipment, CommandKind::ProgReset,
            CommandKind::ProgUpgradeItem, CommandKind::ProgSelectSupport,
            CommandKind::ProgTextSpeedUp, CommandKind::ProgTextSpeedDown,
            CommandKind::ProgMasterVolumeUp, CommandKind::ProgMasterVolumeDown,
            CommandKind::ProgBgmVolumeUp, CommandKind::ProgBgmVolumeDown,
            CommandKind::ProgSfxVolumeUp, CommandKind::ProgSfxVolumeDown,
            CommandKind::ProgToggleScreenShake, CommandKind::ProgToggleFullscreen,
            CommandKind::ProgSetTextSpeed, CommandKind::ProgSetMasterVolume,
            CommandKind::ProgSetBgmVolume, CommandKind::ProgSetSfxVolume,
            CommandKind::Quit,
            CommandKind::Raw
        };

        // Family markers so the combo groups related commands; returns the label
        // to draw as a separator header, or nullptr for regular items.
        auto groupHeader = [](CommandKind kind) -> const char*
        {
            if (kind == CommandKind::GameSaveOpenSaveMenu) return EditorLocale::Text("Game Save", "存档系统");
            if (kind == CommandKind::ProgressionSetFlag) return EditorLocale::Text("Progression (core)", "成长（核心）");
            if (kind == CommandKind::UiPager) return EditorLocale::Text("UI / Animation", "UI / 动画");
            if (kind == CommandKind::VnAuto) return EditorLocale::Text("Visual Novel", "视觉小说");
            if (kind == CommandKind::TurnMenu) return EditorLocale::Text("Turn Combat", "回合制战斗");
            if (kind == CommandKind::TacticCell) return EditorLocale::Text("Tactical Combat", "战棋战斗");
            if (kind == CommandKind::SideItem1) return EditorLocale::Text("Side Combat", "横版战斗");
            if (kind == CommandKind::ProgUpgradeTravelerArmor) return EditorLocale::Text("Progression (actions)", "成长（操作）");
            if (kind == CommandKind::ProgTextSpeedUp) return EditorLocale::Text("Settings", "设置");
            if (kind == CommandKind::Quit) return EditorLocale::Text("System", "系统");
            return nullptr;
        };

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Type", CommandKindLabel(spec.Kind)))
        {
            for (CommandKind kind : kinds)
            {
                if (const char* header = groupHeader(kind))
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("%s", header);
                }
                const bool selected = kind == spec.Kind;
                if (ImGui::Selectable(CommandKindLabel(kind), selected))
                {
                    if (kind != spec.Kind)
                    {
                        SeedCommandKind(spec, kind, command);
                        command = BuildCommand(spec);
                        changed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        switch (spec.Kind)
        {
        case CommandKind::Scene:
        case CommandKind::NewGame:
        case CommandKind::LoadGame:
            if (DrawOptionPicker("Scene", spec.Primary, SceneChoices(), 512))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Kind == CommandKind::LoadGame)
            {
                int slot = std::max(1, spec.Number);
                if (ImGui::DragInt("Slot", &slot, 1.0f, 1, 20))
                {
                    spec.Number = std::max(1, slot);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        case CommandKind::Event:
            if (DrawOptionPicker("Event Name", spec.Primary, EventChoices(), 256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            if (EditorWidgets::InputString(
                    "Target (@UUID, empty = broadcast)", spec.Target, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip(
                "Leave empty to broadcast to every EventScriptComponent. "
                "To target a single entity, type its @UUID selector here; "
                "the command becomes event:@UUID:name.");
            break;
        case CommandKind::GameSaveSlotSave:
        case CommandKind::GameSaveLoadSlot:
        {
            int slot = std::max(1, spec.Number);
            if (ImGui::DragInt("Slot", &slot, 1.0f, 1, 20))
            {
                spec.Number = std::max(1, slot);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::ProgressionSetFlag:
        case CommandKind::ProgressionClearFlag:
            if (EditorContentPickers::DrawStoryFlagField("Flag", spec.Primary, 256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgressionSetActiveDungeon:
            if (EditorContentPickers::DrawProgressionIdField("Dungeon",
                spec.Primary,
                EditorContentPickers::ProgressionIdKind::Dungeon,
                256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgressionSetChapter:
        {
            int chapter = std::max(1, spec.Number);
            if (ImGui::DragInt("Chapter", &chapter, 1.0f, 1, 99))
            {
                spec.Number = std::max(1, chapter);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::UiPager:
        {
            // Grammar: ui:pager:@UUID:action[:page]
            if (EditorWidgets::InputString("Pager Target", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Target is the pager entity UUID, e.g. @123456789.");
            static const char* pagerActions[] = { "next", "prev", "first", "last", "page", "set" };
            int actionIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(pagerActions); ++i)
                if (spec.Secondary == pagerActions[i]) { actionIndex = i; break; }
            if (ImGui::Combo("Action", &actionIndex, pagerActions, IM_ARRAYSIZE(pagerActions)))
            {
                spec.Secondary = pagerActions[actionIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Secondary == "page" || spec.Secondary == "set")
            {
                int page = std::max(1, spec.Number);
                if (ImGui::DragInt("Page", &page, 1.0f, 1, 999))
                {
                    spec.Number = std::max(1, page);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        }
        case CommandKind::Anim:
        {
            // Grammar: anim:action:@UUID[:arg]
            if (EditorWidgets::InputString("Animation Target", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Target is the animator entity UUID, e.g. @123456789.");
            static const char* animActions[] = { "play", "restart", "pause", "resume", "stop", "seek" };
            int actionIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(animActions); ++i)
                if (spec.Secondary == animActions[i]) { actionIndex = i; break; }
            if (ImGui::Combo("Action", &actionIndex, animActions, IM_ARRAYSIZE(animActions)))
            {
                spec.Secondary = animActions[actionIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Secondary == "play")
            {
                if (EditorWidgets::InputString("Clip Name", spec.Raw, 128))
                {
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            else if (spec.Secondary == "seek")
            {
                float seek = std::max(0.0f, spec.FloatValue);
                if (ImGui::DragFloat("Seek Time", &seek, 0.01f, 0.0f, 120.0f, "%.2f"))
                {
                    spec.FloatValue = std::max(0.0f, seek);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        }
        case CommandKind::TurnMenu:
            if (EditorWidgets::InputString("Menu Page", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Menu page key, e.g. root / skills / items.");
            break;
        case CommandKind::TurnItem:
        case CommandKind::TurnSkill:
        {
            static const char* turnSlots[] = { "basic", "slot0", "slot1", "slot2", "slot3", "item0", "potion" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(turnSlots); ++i)
                if (spec.Primary == turnSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Slot", &slotIndex, turnSlots, IM_ARRAYSIZE(turnSlots)))
            {
                spec.Primary = turnSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (EditorWidgets::InputString("Skill Id (optional)", spec.Raw, 128))
            {
                spec.Primary = spec.Raw.empty() ? spec.Primary : spec.Raw;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Choose a slot, or type a raw skill id (e.g. turn.aether_edge) for custom skills.");
            break;
        }
        case CommandKind::TurnTarget:
            if (EditorWidgets::InputString("Target Name", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Combatant entity name or its Target button name.");
            break;
        case CommandKind::TacticCell:
        {
            int row = std::max(0, spec.Number);
            int col = std::max(0, spec.Number2);
            bool cellChanged = false;
            if (ImGui::DragInt("Row", &row, 1.0f, 0, 99)) cellChanged = true;
            if (ImGui::DragInt("Column", &col, 1.0f, 0, 99)) cellChanged = true;
            if (cellChanged)
            {
                spec.Number = std::max(0, row);
                spec.Number2 = std::max(0, col);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::TacticMenu:
        {
            static const char* tacticPages[] = { "move", "attack", "skills", "items" };
            int pageIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(tacticPages); ++i)
                if (spec.Primary == tacticPages[i]) { pageIndex = i; break; }
            if (ImGui::Combo("Menu Page", &pageIndex, tacticPages, IM_ARRAYSIZE(tacticPages)))
            {
                spec.Primary = tacticPages[pageIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::TacticSkill:
        {
            static const char* tacticSlots[] = { "cancel", "wait", "guard", "basic", "slot0", "slot1", "slot2", "item0", "potion" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(tacticSlots); ++i)
                if (spec.Primary == tacticSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Slot", &slotIndex, tacticSlots, IM_ARRAYSIZE(tacticSlots)))
            {
                spec.Primary = tacticSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (EditorWidgets::InputString("Skill Id (optional)", spec.Raw, 128))
            {
                spec.Primary = spec.Raw.empty() ? spec.Primary : spec.Raw;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Choose a slot, or type a raw skill id for custom skills.");
            break;
        }
        case CommandKind::ProgSelectSkillNode:
            if (EditorWidgets::InputString("Skill Node Id", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgEquipmentPageSlider:
        {
            float value = spec.FloatValue;
            if (ImGui::DragFloat("Slider Value", &value, 0.05f, 0.0f, 3.0f, "%.1f"))
            {
                spec.FloatValue = value;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip(">= 1.5 switches to page 2, else page 1.");
            break;
        }
        case CommandKind::ProgSelectEquipmentSlot:
        {
            static const char* equipSlots[] = { "weapon", "armor", "ring", "charm", "boots", "special" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(equipSlots); ++i)
                if (spec.Primary == equipSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Equipment Slot", &slotIndex, equipSlots, IM_ARRAYSIZE(equipSlots)))
            {
                spec.Primary = equipSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::ProgSelectEquipment:
            if (EditorWidgets::InputString("Equipment Id", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgSetTextSpeed:
        case CommandKind::ProgSetMasterVolume:
        case CommandKind::ProgSetBgmVolume:
        case CommandKind::ProgSetSfxVolume:
        {
            float value = spec.FloatValue;
            const float maxValue = (spec.Kind == CommandKind::ProgSetTextSpeed) ? 180.0f : 100.0f;
            if (ImGui::DragFloat("Value", &value, 1.0f, 0.0f, maxValue, "%.1f"))
            {
                spec.FloatValue = std::clamp(value, 0.0f, maxValue);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::Raw:
            if (EditorWidgets::InputString("Raw", spec.Raw, rawCapacity))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        default:
            break;
        }

        ImGui::TextDisabled("Command: %s", command.empty() ? "(none)" : command.c_str());
        ImGui::PopID();
        return changed;
    }

} // namespace Wheatear::EditorCommandBuilder
