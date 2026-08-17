#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Wheatear::SystemBindings {

    enum class BindingKind
    {
        Exact = 0,
        Indexed,
        IndexedParts
    };

    struct BindingRecord
    {
        std::string_view Id;
        std::string_view Owner;
        std::string_view Context;
        std::string_view Description;
        BindingKind Kind = BindingKind::Exact;
        std::string_view Name;
        std::string_view Prefix;
        int FirstIndex = 0;
        int LastIndex = 0;
        std::array<std::string_view, 4> Parts{};
        size_t PartCount = 0;
        bool Required = true;
    };

    inline BindingRecord Exact(std::string_view id,
        std::string_view owner,
        std::string_view context,
        std::string_view name,
        std::string_view description,
        bool required = true)
    {
        BindingRecord record;
        record.Id = id;
        record.Owner = owner;
        record.Context = context;
        record.Description = description;
        record.Kind = BindingKind::Exact;
        record.Name = name;
        record.Required = required;
        return record;
    }

    inline BindingRecord Indexed(std::string_view id,
        std::string_view owner,
        std::string_view context,
        std::string_view prefix,
        int firstIndex,
        int lastIndex,
        std::string_view description,
        bool required = true)
    {
        BindingRecord record;
        record.Id = id;
        record.Owner = owner;
        record.Context = context;
        record.Description = description;
        record.Kind = BindingKind::Indexed;
        record.Prefix = prefix;
        record.FirstIndex = firstIndex;
        record.LastIndex = lastIndex;
        record.Required = required;
        return record;
    }

    inline BindingRecord IndexedParts(std::string_view id,
        std::string_view owner,
        std::string_view context,
        std::string_view prefix,
        int firstIndex,
        int lastIndex,
        const std::array<std::string_view, 4>& parts,
        size_t partCount,
        std::string_view description,
        bool required = true)
    {
        BindingRecord record;
        record.Id = id;
        record.Owner = owner;
        record.Context = context;
        record.Description = description;
        record.Kind = BindingKind::IndexedParts;
        record.Prefix = prefix;
        record.FirstIndex = firstIndex;
        record.LastIndex = lastIndex;
        record.Parts = parts;
        record.PartCount = partCount;
        record.Required = required;
        return record;
    }

    inline std::string IndexedName(std::string_view prefix, int index)
    {
        return std::string(prefix) + std::to_string(index);
    }

    inline std::string IndexedPartName(std::string_view prefix, int index, std::string_view part)
    {
        return IndexedName(prefix, index) + std::string(part);
    }

    namespace VisualNovel {
        inline constexpr const char* RootPrefix = "VN_";
        inline constexpr const char* CommandPrefix = "VN_Command";
        inline constexpr const char* HistoryPrefix = "VN_History";
        inline constexpr const char* SettingsPrefix = "VN_Settings";
        inline constexpr const char* SaveLoadPrefix = "VN_SaveLoad";

        inline constexpr const char* SpeakerText = "VN_SpeakerText";
        inline constexpr const char* BodyText = "VN_BodyText";
        inline constexpr const char* AdvanceHint = "VN_AdvanceHint";
        inline constexpr const char* Background = "VN_Background";
        inline constexpr const char* Floor = "VN_Floor";
        inline constexpr const char* CharacterPrefix = "VN_Character_";
        inline constexpr const char* ChoicePrefix = "VN_Choice_";
        inline constexpr const char* HistoryText = "VN_HistoryText";
        inline constexpr const char* AutoPlayIndicator = "VN_AutoPlayIndicator";
        inline constexpr const char* CommandBar = "VN_CommandBar";
        inline constexpr const char* CommandTooltip = "VN_CommandTooltip";
        inline constexpr const char* HistoryPanel = "VN_HistoryPanel";
        inline constexpr const char* HistoryScroll = "VN_HistoryScroll";
        inline constexpr const char* SettingsPanel = "VN_SettingsPanel";
        inline constexpr const char* SettingsText = "VN_SettingsText";
        inline constexpr const char* SaveLoadPanel = "VN_SaveLoadPanel";
        inline constexpr const char* SaveLoadText = "VN_SaveLoadText";
        inline constexpr const char* SystemMessage = "VN_SystemMessage";
        inline constexpr const char* MusicNoticePanel = "VN_MusicNoticePanel";
        inline constexpr const char* MusicNoticePrefix = "VN_MusicNotice";
        inline constexpr const char* DialoguePanel = "VN_DialoguePanel";

        inline constexpr const char* CommandSave = "VN_Command_Save";
        inline constexpr const char* CommandLoad = "VN_Command_Load";
        inline constexpr const char* CommandQuickSave = "VN_Command_QuickSave";
        inline constexpr const char* CommandQuickLoad = "VN_Command_QuickLoad";
        inline constexpr const char* CommandSettings = "VN_Command_Settings";
        inline constexpr const char* CommandHistory = "VN_Command_History";
        inline constexpr const char* CommandAuto = "VN_Command_Auto";
        inline constexpr const char* CommandSkip = "VN_Command_Skip";
        inline constexpr const char* CommandHide = "VN_Command_Hide";

        inline constexpr const char* SettingsMasterVolumeLabel = "VN_Settings_MasterVolumeLabel";
        inline constexpr const char* SettingsMasterVolumeSlider = "VN_Settings_MasterVolumeSlider";
        inline constexpr const char* SettingsMasterVolumeDown = "VN_Settings_MasterVolumeDown";
        inline constexpr const char* SettingsMasterVolumeUp = "VN_Settings_MasterVolumeUp";
        inline constexpr const char* SettingsBGMVolumeLabel = "VN_Settings_BGMVolumeLabel";
        inline constexpr const char* SettingsBGMVolumeSlider = "VN_Settings_BGMVolumeSlider";
        inline constexpr const char* SettingsBGMVolumeDown = "VN_Settings_BGMVolumeDown";
        inline constexpr const char* SettingsBGMVolumeUp = "VN_Settings_BGMVolumeUp";
        inline constexpr const char* SettingsSFXVolumeLabel = "VN_Settings_SFXVolumeLabel";
        inline constexpr const char* SettingsSFXVolumeSlider = "VN_Settings_SFXVolumeSlider";
        inline constexpr const char* SettingsSFXVolumeDown = "VN_Settings_SFXVolumeDown";
        inline constexpr const char* SettingsSFXVolumeUp = "VN_Settings_SFXVolumeUp";

        inline constexpr const char* SaveLoadSlotScroll = "VN_SaveLoadSlotScroll";
        inline constexpr const char* SaveLoadSaveSlot1 = "VN_SaveLoad_SaveSlot1";
        inline constexpr const char* SaveLoadLoadSlot1 = "VN_SaveLoad_LoadSlot1";
        inline constexpr const char* SaveLoadTitle = "VN_SaveLoadTitle";
        inline constexpr const char* SaveLoadSlotPrefix = "VN_SaveLoad_Slot_";
        inline constexpr const char* SaveLoadConfirmPanel = "VN_SaveLoadConfirmPanel";
        inline constexpr const char* SaveLoadConfirmText = "VN_SaveLoadConfirmText";
        inline constexpr const char* SaveLoadConfirmYes = "VN_SaveLoadConfirmYes";
        inline constexpr const char* SaveLoadConfirmNo = "VN_SaveLoadConfirmNo";
    } // namespace VisualNovel

    namespace Arcade {
        inline constexpr const char* JoystickBase = "AR_JoystickBase";
        inline constexpr const char* JoystickThumb = "AR_JoystickThumb";
        inline constexpr const char* AttackButton = "AR_Attack";
        inline constexpr const char* WeaponPrefix = "AR_Weapon_";

        inline constexpr const char* Player = "Battle_Player";
        inline constexpr const char* Boss = "Battle_Boss";
        inline constexpr const char* Fade = "Battle_Fade";
        inline constexpr const char* PausePanel = "Battle_PausePanel";
        inline constexpr const char* MessageText = "Battle_Message";
        inline constexpr const char* WeaponText = "Battle_WeaponText";
        inline constexpr const char* PlayerHealthBar = "Battle_PlayerHealth";
        inline constexpr const char* PlayerHealthText = "Battle_PlayerHealthText";
        inline constexpr const char* BossHealthBar = "Battle_BossHealth";
        inline constexpr const char* BossHealthText = "Battle_BossHealthText";
    } // namespace Arcade

    namespace Turn {
        inline constexpr const char* CommandPrefix = "TC_Command_";
        inline constexpr const char* CommandRootSuffix = "_Root";
        inline constexpr const char* CommandIconSuffix = "_Icon";
        inline constexpr const char* CommandTextSuffix = "_Text";

        inline constexpr const char* Fade = "TC_Fade";
        inline constexpr const char* MessageText = "TC_MessageText";
        inline constexpr const char* ActiveActorText = "TC_ActiveActorText";
        inline constexpr const char* TurnOrderText = "TC_TurnOrderText";
        inline constexpr const char* SkillDetailText = "TC_SkillDetailText";
        inline constexpr const char* CommandPanel = "TC_CommandPanel";
        inline constexpr const char* TargetHintText = "TC_TargetHintText";
        inline constexpr const char* ActionFlash = "TC_ActionFlash";
        inline constexpr const char* ActionEffect = "TC_ActionEffect";
    } // namespace Turn

    namespace Tactical {
        inline constexpr const char* CommandPrefix = "TK_Command_";
        inline constexpr const char* CommandRootSuffix = "_Root";
        inline constexpr const char* CommandIconSuffix = "_Icon";
        inline constexpr const char* CommandTextSuffix = "_Text";
        inline constexpr const char* CancelButton = "TK_CancelButton";
        inline constexpr const char* CancelText = "TK_CancelText";

        inline constexpr const char* CellPrefix = "TK_Cell_";
        inline constexpr const char* UnitPrefix = "TK_Unit_";
        inline constexpr const char* Fade = "TK_Fade";
        inline constexpr const char* MessageText = "TK_MessageText";
        inline constexpr const char* PhaseText = "TK_PhaseText";
        inline constexpr const char* DetailText = "TK_DetailText";
        inline constexpr const char* CommandPanel = "TK_CommandPanel";
        inline constexpr const char* ActionEffect = "TK_ActionEffect";
    } // namespace Tactical

    namespace Side {
        inline constexpr const char* SkillPrefix = "SC_Skill";
        inline constexpr const char* ItemSlotPrefix = "SC_ItemSlot_";
        inline constexpr const char* Wave1Prefix = "SC_Wave1_";
        inline constexpr const char* Wave2Prefix = "SC_Wave2_";
        inline constexpr const char* EnemyWavePrefix = "SC_Enemy_W";
    } // namespace Side

    namespace Progression {
        inline constexpr const char* SaveLoadSlotScroll = "SaveLoad_SlotScroll";
        inline constexpr const char* SaveLoadConfirmPanel = "SaveLoad_ConfirmPanel";
        inline constexpr const char* SaveLoadConfirmText = "SaveLoad_ConfirmText";
        inline constexpr const char* SaveLoadConfirmYes = "SaveLoad_ConfirmYes";
        inline constexpr const char* SaveLoadConfirmNo = "SaveLoad_ConfirmNo";
        inline constexpr const char* SaveLoadMainPanel = "SaveLoad_MainPanel";
        inline constexpr const char* SaveLoadIcon = "SaveLoad_Icon";
        inline constexpr const char* SaveLoadTitle = "SaveLoad_Title";
        inline constexpr const char* SaveLoadClose = "SaveLoad_Close";
        inline constexpr const char* SaveLoadSlotPrefix = "SaveLoad_Slot_";

        inline constexpr const char* HubStatus = "Hub_Status";
        inline constexpr const char* HubSubtitle = "Hub_Subtitle";
        inline constexpr const char* HubDungeonButton = "Hub_Button_Dungeon";
        inline constexpr const char* HubSkillButton = "Hub_Button_Skill";
        inline constexpr const char* HubEquipmentButton = "Hub_Button_Equip";

        inline constexpr const char* ResultTitle = "Result_Title";
        inline constexpr const char* ResultStats = "Result_Stats";
        inline constexpr const char* ResultRewards = "Result_Rewards";
        inline constexpr const char* ResultEXPBar = "Result_EXPBar";
        inline constexpr const char* ResultDropPrefix = "Result_Drop_";
        inline constexpr const char* ResultDropCoreFrame = "Result_Drop_Core_Frame";
        inline constexpr const char* ResultDropTooltipPanel = "Result_DropTooltipPanel";
        inline constexpr const char* ResultDropTooltipText = "Result_DropTooltipText";

        inline constexpr const char* SkillTreeView = "SkillTree_View";
        inline constexpr const char* SkillTreeSubtitle = "SkillTree_Subtitle";
        inline constexpr const char* SkillTreeStatus = "SkillTree_Status";
        inline constexpr const char* SkillTreeDetails = "SkillTree_Details";
        inline constexpr const char* SkillTreeMaterials = "SkillTree_Materials";
        inline constexpr const char* SkillTreeLearnButton = "SkillTree_Button_LearnSelectedSkill";
        inline constexpr const char* SkillTreeMagicSwordBar = "SkillTree_MagicSwordBar";

        inline constexpr const char* EquipmentStatus = "Equipment_Status";
        inline constexpr const char* EquipmentSubtitle = "Equipment_Subtitle";
        inline constexpr const char* EquipmentDetails = "Equipment_Details";
        inline constexpr const char* EquipmentDetailsScroll = "Equipment_DetailsScroll";
        inline constexpr const char* EquipmentMaterialsScroll = "Equipment_MaterialsScroll";
        inline constexpr const char* EquipmentPageText = "Equipment_PageText";
        inline constexpr const char* EquipmentMaterials = "Equipment_Materials";
        inline constexpr const char* EquipmentUpgradeArmorButton = "Equipment_Button_UpgradeArmor";
        inline constexpr const char* EquipmentArmorBar = "Equipment_ArmorBar";
        inline constexpr const char* EquipmentButtonPagePrev = "Equipment_Button_PagePrev";
        inline constexpr const char* EquipmentButtonPageNext = "Equipment_Button_PageNext";
        inline constexpr const char* EquipmentPageSlider = "Equipment_PageSlider";
        inline constexpr const char* EquipmentToggleButton = "Equipment_Button_Toggle";
        inline constexpr const char* EquipmentTooltipPanel = "Equipment_TooltipPanel";
        inline constexpr const char* EquipmentTooltipText = "Equipment_TooltipText";
        inline constexpr const char* EquipmentSlotArmor = "Equipment_SlotArmor";
        inline constexpr const char* EquipmentSlotArmorButton = "Equipment_SlotArmor_Button";
        inline constexpr const char* EquipmentSlotRing = "Equipment_SlotRing";
        inline constexpr const char* EquipmentSlotRingButton = "Equipment_SlotRing_Button";
        inline constexpr const char* EquipmentSlotCharm = "Equipment_SlotCharm";
        inline constexpr const char* EquipmentSlotCharmButton = "Equipment_SlotCharm_Button";
        inline constexpr const char* EquipmentSlotBoots = "Equipment_SlotBoots";
        inline constexpr const char* EquipmentSlotBootsButton = "Equipment_SlotBoots_Button";
        inline constexpr const char* EquipmentSlotWeapon = "Equipment_SlotWeapon";
        inline constexpr const char* EquipmentSlotWeaponButton = "Equipment_SlotWeapon_Button";
        inline constexpr const char* EquipmentSlotSpecial = "Equipment_SlotSpecial";
        inline constexpr const char* EquipmentSlotSpecialButton = "Equipment_SlotSpecial_Button";

        inline constexpr const char* DungeonStatus = "Dungeon_Status";
        inline constexpr const char* DungeonSubtitle = "Dungeon_Subtitle";
        inline constexpr const char* DungeonRewards = "Dungeon_Rewards";
        inline constexpr const char* RelationshipStatus = "Relationship_Status";
        inline constexpr const char* RelationshipSubtitle = "Relationship_Subtitle";
        inline constexpr const char* SupportStatus = "Support_Status";
        inline constexpr const char* SupportSubtitle = "Support_Subtitle";

        inline constexpr const char* SettingsStatus = "Settings_Status";
        inline constexpr const char* SettingsSubtitle = "Settings_Subtitle";
        inline constexpr const char* SettingsControlPanel = "Settings_ControlPanel";
        inline constexpr const char* SettingsMasterVolumeLabel = "Settings_MasterVolumeLabel";
        inline constexpr const char* SettingsBGMVolumeLabel = "Settings_BGMVolumeLabel";
        inline constexpr const char* SettingsSFXVolumeLabel = "Settings_SFXVolumeLabel";
        inline constexpr const char* SettingsTextSpeedSlider = "Settings_TextSpeedSlider";
        inline constexpr const char* SettingsMasterVolumeSlider = "Settings_MasterVolumeSlider";
        inline constexpr const char* SettingsBGMVolumeSlider = "Settings_BGMVolumeSlider";
        inline constexpr const char* SettingsSFXVolumeSlider = "Settings_SFXVolumeSlider";
        inline constexpr const char* SettingsVolumeDownButton = "Settings_Button_VolumeDown";
        inline constexpr const char* SettingsVolumeUpButton = "Settings_Button_VolumeUp";
        inline constexpr const char* SettingsBGMDownButton = "Settings_Button_BGMDown";
        inline constexpr const char* SettingsBGMUpButton = "Settings_Button_BGMUp";
        inline constexpr const char* SettingsSFXDownButton = "Settings_Button_SFXDown";
        inline constexpr const char* SettingsSFXUpButton = "Settings_Button_SFXUp";
    } // namespace Progression

    inline const std::vector<BindingRecord>& AllBindings()
    {
        static const std::vector<BindingRecord> records = {
            Exact("vn.command.save", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandSave, "Save command button"),
            Exact("vn.command.load", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandLoad, "Load command button"),
            Exact("vn.command.quick_save", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandQuickSave, "Quick-save command button"),
            Exact("vn.command.quick_load", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandQuickLoad, "Quick-load command button"),
            Exact("vn.command.settings", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandSettings, "Settings command button"),
            Exact("vn.command.history", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandHistory, "History command button"),
            Exact("vn.command.auto", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandAuto, "Auto-play command button"),
            Exact("vn.command.skip", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandSkip, "Skip command button"),
            Exact("vn.command.hide", "VisualNovel", VisualNovel::CommandBar, VisualNovel::CommandHide, "Hidden dialogue toggle", false),

            Exact("vn.settings.master_label", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsMasterVolumeLabel, "Master volume label"),
            Exact("vn.settings.master_slider", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsMasterVolumeSlider, "Master volume slider"),
            Exact("vn.settings.master_down", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsMasterVolumeDown, "Master volume decrement"),
            Exact("vn.settings.master_up", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsMasterVolumeUp, "Master volume increment"),
            Exact("vn.settings.bgm_label", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsBGMVolumeLabel, "BGM volume label"),
            Exact("vn.settings.bgm_slider", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsBGMVolumeSlider, "BGM volume slider"),
            Exact("vn.settings.bgm_down", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsBGMVolumeDown, "BGM volume decrement"),
            Exact("vn.settings.bgm_up", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsBGMVolumeUp, "BGM volume increment"),
            Exact("vn.settings.sfx_label", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsSFXVolumeLabel, "SFX volume label"),
            Exact("vn.settings.sfx_slider", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsSFXVolumeSlider, "SFX volume slider"),
            Exact("vn.settings.sfx_down", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsSFXVolumeDown, "SFX volume decrement"),
            Exact("vn.settings.sfx_up", "VisualNovel", VisualNovel::SettingsPanel, VisualNovel::SettingsSFXVolumeUp, "SFX volume increment"),

            Exact("vn.saveload.scroll", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadSlotScroll, "VN save/load slot scroll"),
            Exact("vn.saveload.title", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadTitle, "VN save/load title"),
            Exact("vn.saveload.confirm_panel", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadConfirmPanel, "VN overwrite confirmation panel"),
            Exact("vn.saveload.confirm_text", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadConfirmText, "VN overwrite confirmation text"),
            Exact("vn.saveload.confirm_yes", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadConfirmYes, "VN overwrite confirm button"),
            Exact("vn.saveload.confirm_no", "VisualNovel", VisualNovel::SaveLoadPanel, VisualNovel::SaveLoadConfirmNo, "VN overwrite cancel button"),
            Indexed("vn.saveload.slot", "VisualNovel", VisualNovel::SaveLoadSlotScroll, VisualNovel::SaveLoadSlotPrefix, 1, 20, "VN save/load slot button"),

            Exact("arcade.joystick_base", "ArcadeCombat", "ArcadeCombatLevelComponent", Arcade::JoystickBase, "Virtual joystick base"),
            Exact("arcade.joystick_thumb", "ArcadeCombat", "ArcadeCombatLevelComponent", Arcade::JoystickThumb, "Virtual joystick thumb"),
            Exact("arcade.attack", "ArcadeCombat", "ArcadeCombatLevelComponent", Arcade::AttackButton, "Attack touch button"),
            Indexed("arcade.weapon", "ArcadeCombat", "ArcadeCombatLevelComponent", Arcade::WeaponPrefix, 1, 3, "Weapon touch button"),

            IndexedParts("turn.command", "TurnCombat", "TurnCombatLevelComponent", Turn::CommandPrefix, 1, 5,
                { Turn::CommandRootSuffix, Turn::CommandIconSuffix, Turn::CommandTextSuffix, {} }, 3,
                "Turn command slot widgets"),

            IndexedParts("tactical.command", "TacticalCombat", "TacticalCombatLevelComponent", Tactical::CommandPrefix, 1, 4,
                { Tactical::CommandRootSuffix, Tactical::CommandIconSuffix, Tactical::CommandTextSuffix, {} }, 3,
                "Tactical command slot widgets"),
            Exact("tactical.cancel_button", "TacticalCombat", "TacticalCombatLevelComponent", Tactical::CancelButton, "Tactical cancel button"),
            Exact("tactical.cancel_text", "TacticalCombat", "TacticalCombatLevelComponent", Tactical::CancelText, "Tactical cancel label"),

            Exact("hub.subtitle", "Progression", Progression::HubStatus, Progression::HubSubtitle, "Hub subtitle"),
            Exact("hub.status", "Progression", Progression::HubStatus, Progression::HubStatus, "Hub status"),
            Exact("hub.dungeon_button", "Progression", Progression::HubStatus, Progression::HubDungeonButton, "Dungeon navigation button"),
            Exact("hub.skill_button", "Progression", Progression::HubStatus, Progression::HubSkillButton, "Skill tree navigation button"),
            Exact("hub.equipment_button", "Progression", Progression::HubStatus, Progression::HubEquipmentButton, "Equipment navigation button"),

            Exact("result.title", "Progression", Progression::ResultStats, Progression::ResultTitle, "Result title"),
            Exact("result.stats", "Progression", Progression::ResultStats, Progression::ResultStats, "Result stats"),
            Exact("result.rewards", "Progression", Progression::ResultStats, Progression::ResultRewards, "Result rewards text"),
            Exact("result.exp", "Progression", Progression::ResultStats, Progression::ResultEXPBar, "Result experience bar"),
            Exact("result.drop_core_frame", "Progression", Progression::ResultStats, Progression::ResultDropCoreFrame, "Result drop layout sentinel", false),
            Exact("result.drop_tooltip_panel", "Progression", Progression::ResultStats, Progression::ResultDropTooltipPanel, "Result drop tooltip panel", false),
            Exact("result.drop_tooltip_text", "Progression", Progression::ResultStats, Progression::ResultDropTooltipText, "Result drop tooltip text", false),

            Exact("skill_tree.view", "Progression", Progression::SkillTreeView, Progression::SkillTreeView, "Skill tree view"),
            Exact("skill_tree.subtitle", "Progression", Progression::SkillTreeView, Progression::SkillTreeSubtitle, "Skill tree subtitle"),
            Exact("skill_tree.status", "Progression", Progression::SkillTreeView, Progression::SkillTreeStatus, "Skill tree status", false),
            Exact("skill_tree.details", "Progression", Progression::SkillTreeView, Progression::SkillTreeDetails, "Skill tree details"),
            Exact("skill_tree.materials", "Progression", Progression::SkillTreeView, Progression::SkillTreeMaterials, "Skill tree materials"),
            Exact("skill_tree.learn", "Progression", Progression::SkillTreeView, Progression::SkillTreeLearnButton, "Skill tree learn button"),
            Exact("skill_tree.magic_sword", "Progression", Progression::SkillTreeView, Progression::SkillTreeMagicSwordBar, "Magic sword progress bar"),

            Exact("equipment.subtitle", "Progression", Progression::EquipmentStatus, Progression::EquipmentSubtitle, "Equipment subtitle"),
            Exact("equipment.status", "Progression", Progression::EquipmentStatus, Progression::EquipmentStatus, "Equipment status"),
            Exact("equipment.details", "Progression", Progression::EquipmentStatus, Progression::EquipmentDetails, "Equipment details"),
            Exact("equipment.page_text", "Progression", Progression::EquipmentStatus, Progression::EquipmentPageText, "Equipment page text"),
            Exact("equipment.materials", "Progression", Progression::EquipmentStatus, Progression::EquipmentMaterials, "Equipment materials"),
            Exact("equipment.upgrade", "Progression", Progression::EquipmentStatus, Progression::EquipmentUpgradeArmorButton, "Equipment upgrade button"),
            Exact("equipment.armor_bar", "Progression", Progression::EquipmentStatus, Progression::EquipmentArmorBar, "Equipment armor bar"),
            Exact("equipment.page_prev", "Progression", Progression::EquipmentStatus, Progression::EquipmentButtonPagePrev, "Equipment previous page button", false),
            Exact("equipment.page_next", "Progression", Progression::EquipmentStatus, Progression::EquipmentButtonPageNext, "Equipment next page button", false),
            Exact("equipment.page_slider", "Progression", Progression::EquipmentStatus, Progression::EquipmentPageSlider, "Equipment page slider", false),
            Exact("equipment.tooltip_panel", "Progression", Progression::EquipmentStatus, Progression::EquipmentTooltipPanel, "Equipment tooltip panel", false),
            Exact("equipment.tooltip_text", "Progression", Progression::EquipmentStatus, Progression::EquipmentTooltipText, "Equipment tooltip text", false),

            Exact("dungeon.subtitle", "Progression", Progression::DungeonStatus, Progression::DungeonSubtitle, "Dungeon select subtitle"),
            Exact("dungeon.status", "Progression", Progression::DungeonStatus, Progression::DungeonStatus, "Dungeon select status"),
            Exact("dungeon.rewards", "Progression", Progression::DungeonStatus, Progression::DungeonRewards, "Dungeon select rewards"),
            Exact("relationship.subtitle", "Progression", Progression::RelationshipStatus, Progression::RelationshipSubtitle, "Relationship subtitle"),
            Exact("relationship.status", "Progression", Progression::RelationshipStatus, Progression::RelationshipStatus, "Relationship status"),
            Exact("support.subtitle", "Progression", Progression::SupportStatus, Progression::SupportSubtitle, "Support subtitle"),
            Exact("support.status", "Progression", Progression::SupportStatus, Progression::SupportStatus, "Support status"),

            Exact("settings.subtitle", "Progression", Progression::SettingsStatus, Progression::SettingsSubtitle, "Settings subtitle"),
            Exact("settings.status", "Progression", Progression::SettingsStatus, Progression::SettingsStatus, "Settings status"),
            Exact("settings.control_panel", "Progression", Progression::SettingsStatus, Progression::SettingsControlPanel, "Settings controls panel"),
            Exact("settings.master_label", "Progression", Progression::SettingsControlPanel, Progression::SettingsMasterVolumeLabel, "Master volume label"),
            Exact("settings.bgm_label", "Progression", Progression::SettingsControlPanel, Progression::SettingsBGMVolumeLabel, "BGM volume label"),
            Exact("settings.sfx_label", "Progression", Progression::SettingsControlPanel, Progression::SettingsSFXVolumeLabel, "SFX volume label"),
            Exact("settings.text_speed_slider", "Progression", Progression::SettingsControlPanel, Progression::SettingsTextSpeedSlider, "Text speed slider"),
            Exact("settings.master_slider", "Progression", Progression::SettingsControlPanel, Progression::SettingsMasterVolumeSlider, "Master volume slider"),
            Exact("settings.bgm_slider", "Progression", Progression::SettingsControlPanel, Progression::SettingsBGMVolumeSlider, "BGM volume slider"),
            Exact("settings.sfx_slider", "Progression", Progression::SettingsControlPanel, Progression::SettingsSFXVolumeSlider, "SFX volume slider"),
            Exact("settings.master_down", "Progression", Progression::SettingsControlPanel, Progression::SettingsVolumeDownButton, "Master volume down"),
            Exact("settings.master_up", "Progression", Progression::SettingsControlPanel, Progression::SettingsVolumeUpButton, "Master volume up"),
            Exact("settings.bgm_down", "Progression", Progression::SettingsControlPanel, Progression::SettingsBGMDownButton, "BGM volume down"),
            Exact("settings.bgm_up", "Progression", Progression::SettingsControlPanel, Progression::SettingsBGMUpButton, "BGM volume up"),
            Exact("settings.sfx_down", "Progression", Progression::SettingsControlPanel, Progression::SettingsSFXDownButton, "SFX volume down"),
            Exact("settings.sfx_up", "Progression", Progression::SettingsControlPanel, Progression::SettingsSFXUpButton, "SFX volume up"),

            Exact("saveload.main_panel", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadMainPanel, "Save/load main panel"),
            Exact("saveload.icon", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadIcon, "Save/load icon"),
            Exact("saveload.title", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadTitle, "Save/load title"),
            Exact("saveload.close", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadClose, "Save/load close button"),
            Exact("saveload.slot_scroll", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadSlotScroll, "Save/load slot scroll"),
            Exact("saveload.confirm_panel", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadConfirmPanel, "Save/load confirm panel"),
            Exact("saveload.confirm_text", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadConfirmText, "Save/load confirm text"),
            Exact("saveload.confirm_yes", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadConfirmYes, "Save/load confirm yes"),
            Exact("saveload.confirm_no", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadConfirmNo, "Save/load confirm no"),
            Indexed("saveload.slot", "Progression", Progression::SaveLoadSlotScroll, Progression::SaveLoadSlotPrefix, 1, 20, "Save/load slot buttons")
        };
        return records;
    }

    inline std::vector<std::string> EvaluateNames(const BindingRecord& record)
    {
        std::vector<std::string> names;
        switch (record.Kind)
        {
        case BindingKind::Exact:
            if (!record.Name.empty())
                names.emplace_back(record.Name);
            break;
        case BindingKind::Indexed:
            if (record.FirstIndex <= record.LastIndex)
            {
                names.reserve(static_cast<size_t>(record.LastIndex - record.FirstIndex + 1));
                for (int index = record.FirstIndex; index <= record.LastIndex; ++index)
                    names.push_back(IndexedName(record.Prefix, index));
            }
            break;
        case BindingKind::IndexedParts:
            if (record.FirstIndex <= record.LastIndex)
            {
                names.reserve(static_cast<size_t>(record.LastIndex - record.FirstIndex + 1) * record.PartCount);
                for (int index = record.FirstIndex; index <= record.LastIndex; ++index)
                {
                    for (size_t partIndex = 0; partIndex < record.PartCount; ++partIndex)
                        names.push_back(IndexedPartName(record.Prefix, index, record.Parts[partIndex]));
                }
            }
            break;
        }
        return names;
    }

} // namespace Wheatear::SystemBindings
