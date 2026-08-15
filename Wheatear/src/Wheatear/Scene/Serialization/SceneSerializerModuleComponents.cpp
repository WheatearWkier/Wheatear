#include "wtpch.h"
#include "SceneSerializerComponentGroups.h"
#include "SceneSerializerComponentSupport.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {

    // Per-module serializer blocks live in SceneSerializerModuleComponents_{
    // Arcade, SideCombat, Tactical, Turn}.cpp; this file keeps the Visual
    // Novel component and the module aggregate entry points.

    template<> struct ComponentSerializer<VisualNovelComponent> {
        static constexpr const char* Key = "VisualNovelComponent";
        static void Serialize(YAML::Emitter& o, const VisualNovelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ScriptPath" << YAML::Value << c.ScriptPath;
            o << YAML::Key << "CharactersPerSecond" << YAML::Value << c.CharactersPerSecond;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "RestartOnFinish" << YAML::Value << c.RestartOnFinish;
            o << YAML::Key << "SpeakerTextEntityName" << YAML::Value << c.SpeakerTextEntityName;
            o << YAML::Key << "BodyTextEntityName" << YAML::Value << c.BodyTextEntityName;
            o << YAML::Key << "AdvanceHintEntityName" << YAML::Value << c.AdvanceHintEntityName;
            o << YAML::Key << "BackgroundEntityName" << YAML::Value << c.BackgroundEntityName;
            o << YAML::Key << "FloorEntityName" << YAML::Value << c.FloorEntityName;
            o << YAML::Key << "CharacterEntityPrefix" << YAML::Value << c.CharacterEntityPrefix;
            o << YAML::Key << "ChoiceEntityPrefix" << YAML::Value << c.ChoiceEntityPrefix;
            o << YAML::Key << "MaxVisibleChoices" << YAML::Value << c.MaxVisibleChoices;
            o << YAML::Key << "AutoPlayOnStart" << YAML::Value << c.AutoPlayOnStart;
            o << YAML::Key << "AutoPlayDelay" << YAML::Value << c.AutoPlayDelay;
            o << YAML::Key << "HistoryTextEntityName" << YAML::Value << c.HistoryTextEntityName;
            o << YAML::Key << "AutoPlayIndicatorEntityName" << YAML::Value << c.AutoPlayIndicatorEntityName;
            o << YAML::Key << "CommandBarEntityName" << YAML::Value << c.CommandBarEntityName;
            o << YAML::Key << "CommandTooltipEntityName" << YAML::Value << c.CommandTooltipEntityName;
            o << YAML::Key << "CommandTooltipFollowMouse" << YAML::Value << c.CommandTooltipFollowMouse;
            o << YAML::Key << "CommandTooltipMouseOffset" << YAML::Value << c.CommandTooltipMouseOffset;
            o << YAML::Key << "HistoryPanelEntityName" << YAML::Value << c.HistoryPanelEntityName;
            o << YAML::Key << "HistoryScrollEntityName" << YAML::Value << c.HistoryScrollEntityName;
            o << YAML::Key << "SettingsPanelEntityName" << YAML::Value << c.SettingsPanelEntityName;
            o << YAML::Key << "SettingsTextEntityName" << YAML::Value << c.SettingsTextEntityName;
            o << YAML::Key << "SaveLoadPanelEntityName" << YAML::Value << c.SaveLoadPanelEntityName;
            o << YAML::Key << "SaveLoadTextEntityName" << YAML::Value << c.SaveLoadTextEntityName;
            o << YAML::Key << "SystemMessageEntityName" << YAML::Value << c.SystemMessageEntityName;
            o << YAML::Key << "MusicNoticePanelEntityName" << YAML::Value << c.MusicNoticePanelEntityName;
            o << YAML::Key << "MusicNoticeTextEntityName" << YAML::Value << c.MusicNoticeTextEntityName;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, VisualNovelComponent& c) {
            c.ScriptPath = n["ScriptPath"].as<std::string>(c.ScriptPath);
            c.CharactersPerSecond = n["CharactersPerSecond"].as<float>(c.CharactersPerSecond);
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.RestartOnFinish = n["RestartOnFinish"].as<bool>(c.RestartOnFinish);
            c.SpeakerTextEntityName = n["SpeakerTextEntityName"].as<std::string>(c.SpeakerTextEntityName);
            c.BodyTextEntityName = n["BodyTextEntityName"].as<std::string>(c.BodyTextEntityName);
            c.AdvanceHintEntityName = n["AdvanceHintEntityName"].as<std::string>(c.AdvanceHintEntityName);
            c.BackgroundEntityName = n["BackgroundEntityName"].as<std::string>(c.BackgroundEntityName);
            c.FloorEntityName = n["FloorEntityName"].as<std::string>(c.FloorEntityName);
            c.CharacterEntityPrefix = n["CharacterEntityPrefix"].as<std::string>(c.CharacterEntityPrefix);
            c.ChoiceEntityPrefix = n["ChoiceEntityPrefix"].as<std::string>(c.ChoiceEntityPrefix);
            c.MaxVisibleChoices = n["MaxVisibleChoices"].as<uint32_t>(c.MaxVisibleChoices);
            c.AutoPlayOnStart = n["AutoPlayOnStart"].as<bool>(c.AutoPlayOnStart);
            c.AutoPlayDelay = n["AutoPlayDelay"].as<float>(c.AutoPlayDelay);
            c.HistoryTextEntityName = n["HistoryTextEntityName"].as<std::string>(c.HistoryTextEntityName);
            c.AutoPlayIndicatorEntityName = n["AutoPlayIndicatorEntityName"].as<std::string>(c.AutoPlayIndicatorEntityName);
            c.CommandBarEntityName = n["CommandBarEntityName"].as<std::string>(c.CommandBarEntityName);
            c.CommandTooltipEntityName = n["CommandTooltipEntityName"].as<std::string>(c.CommandTooltipEntityName);
            c.CommandTooltipFollowMouse = n["CommandTooltipFollowMouse"].as<bool>(c.CommandTooltipFollowMouse);
            c.CommandTooltipMouseOffset = n["CommandTooltipMouseOffset"].as<glm::vec2>(c.CommandTooltipMouseOffset);
            c.HistoryPanelEntityName = n["HistoryPanelEntityName"].as<std::string>(c.HistoryPanelEntityName);
            c.HistoryScrollEntityName = n["HistoryScrollEntityName"].as<std::string>(c.HistoryScrollEntityName);
            c.SettingsPanelEntityName = n["SettingsPanelEntityName"].as<std::string>(c.SettingsPanelEntityName);
            c.SettingsTextEntityName = n["SettingsTextEntityName"].as<std::string>(c.SettingsTextEntityName);
            c.SaveLoadPanelEntityName = n["SaveLoadPanelEntityName"].as<std::string>(c.SaveLoadPanelEntityName);
            c.SaveLoadTextEntityName = n["SaveLoadTextEntityName"].as<std::string>(c.SaveLoadTextEntityName);
            c.SystemMessageEntityName = n["SystemMessageEntityName"].as<std::string>(c.SystemMessageEntityName);
            c.MusicNoticePanelEntityName = n["MusicNoticePanelEntityName"].as<std::string>(c.MusicNoticePanelEntityName);
            c.MusicNoticeTextEntityName = n["MusicNoticeTextEntityName"].as<std::string>(c.MusicNoticeTextEntityName);
        }
    };

    using VNModuleSceneComponents = ComponentGroup<VisualNovelComponent>;

    void SerializeVNModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(VNModuleSceneComponents{}, out, entity);
    }

    void DeserializeVNModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(VNModuleSceneComponents{}, node, entity);
    }

    void SerializeModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeVNModuleSceneComponents(out, entity);
        SerializeArcadeModuleSceneComponents(out, entity);
        SerializeSideCombatModuleSceneComponents(out, entity);
        SerializeTacticalCombatModuleSceneComponents(out, entity);
        SerializeTurnCombatModuleSceneComponents(out, entity);
    }

    void DeserializeModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeVNModuleSceneComponents(node, entity);
        DeserializeArcadeModuleSceneComponents(node, entity);
        DeserializeSideCombatModuleSceneComponents(node, entity);
        DeserializeTacticalCombatModuleSceneComponents(node, entity);
        DeserializeTurnCombatModuleSceneComponents(node, entity);
    }

} // namespace Wheatear
