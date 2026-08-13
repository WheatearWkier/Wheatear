#include "wtpch.h"
#include "SceneSerializerComponentGroups.h"
#include "SceneSerializerComponentSupport.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {

    template<> struct ComponentSerializer<TurnCombatLevelComponent> {
        static constexpr const char* Key = "TurnCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const TurnCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "LevelId" << YAML::Value << c.LevelId;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "ActiveActorTextEntityName" << YAML::Value << c.ActiveActorTextEntityName;
            o << YAML::Key << "TurnOrderTextEntityName" << YAML::Value << c.TurnOrderTextEntityName;
            o << YAML::Key << "SkillDetailTextEntityName" << YAML::Value << c.SkillDetailTextEntityName;
            o << YAML::Key << "CommandPanelEntityName" << YAML::Value << c.CommandPanelEntityName;
            o << YAML::Key << "TargetHintTextEntityName" << YAML::Value << c.TargetHintTextEntityName;
            o << YAML::Key << "ActionFlashEntityName" << YAML::Value << c.ActionFlashEntityName;
            o << YAML::Key << "ActionEffectEntityName" << YAML::Value << c.ActionEffectEntityName;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "IntroDuration" << YAML::Value << c.IntroDuration;
            o << YAML::Key << "ActionDuration" << YAML::Value << c.ActionDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TurnCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.LevelId = n["LevelId"].as<std::string>(c.LevelId);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.ActiveActorTextEntityName = n["ActiveActorTextEntityName"].as<std::string>(c.ActiveActorTextEntityName);
            c.TurnOrderTextEntityName = n["TurnOrderTextEntityName"].as<std::string>(c.TurnOrderTextEntityName);
            c.SkillDetailTextEntityName = n["SkillDetailTextEntityName"].as<std::string>(c.SkillDetailTextEntityName);
            c.CommandPanelEntityName = n["CommandPanelEntityName"].as<std::string>(c.CommandPanelEntityName);
            c.TargetHintTextEntityName = n["TargetHintTextEntityName"].as<std::string>(c.TargetHintTextEntityName);
            c.ActionFlashEntityName = n["ActionFlashEntityName"].as<std::string>(c.ActionFlashEntityName);
            c.ActionEffectEntityName = n["ActionEffectEntityName"].as<std::string>(c.ActionEffectEntityName);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.IntroDuration = n["IntroDuration"].as<float>(c.IntroDuration);
            c.ActionDuration = n["ActionDuration"].as<float>(c.ActionDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
        }
    };

    template<> struct ComponentSerializer<TurnCombatantComponent> {
        static constexpr const char* Key = "TurnCombatantComponent";
        static void Serialize(YAML::Emitter& o, const TurnCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "Slot" << YAML::Value << c.Slot;
            o << YAML::Key << "DisplayName" << YAML::Value << c.DisplayName;
            o << YAML::Key << "RoleName" << YAML::Value << c.RoleName;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "MaxMana" << YAML::Value << c.MaxMana;
            o << YAML::Key << "Mana" << YAML::Value << c.Mana;
            o << YAML::Key << "Attack" << YAML::Value << c.Attack;
            o << YAML::Key << "Magic" << YAML::Value << c.Magic;
            o << YAML::Key << "Defense" << YAML::Value << c.Defense;
            o << YAML::Key << "Speed" << YAML::Value << c.Speed;
            o << YAML::Key << "Controllable" << YAML::Value << c.Controllable;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::Key << "BasicSkillId" << YAML::Value << c.BasicSkillId;
            o << YAML::Key << "Skill1Id" << YAML::Value << c.Skill1Id;
            o << YAML::Key << "Skill2Id" << YAML::Value << c.Skill2Id;
            o << YAML::Key << "Skill3Id" << YAML::Value << c.Skill3Id;
            o << YAML::Key << "HealthBarEntityName" << YAML::Value << c.HealthBarEntityName;
            o << YAML::Key << "ManaBarEntityName" << YAML::Value << c.ManaBarEntityName;
            o << YAML::Key << "StatusTextEntityName" << YAML::Value << c.StatusTextEntityName;
            o << YAML::Key << "TargetButtonEntityName" << YAML::Value << c.TargetButtonEntityName;
            o << YAML::Key << "TargetMarkerEntityName" << YAML::Value << c.TargetMarkerEntityName;
            o << YAML::Key << "IdleFramePattern" << YAML::Value << c.IdleFramePattern;
            o << YAML::Key << "AttackFramePattern" << YAML::Value << c.AttackFramePattern;
            o << YAML::Key << "HitFramePattern" << YAML::Value << c.HitFramePattern;
            o << YAML::Key << "DownFramePattern" << YAML::Value << c.DownFramePattern;
            SerializeAtlasFrame(o, "IdleFrame", c.IdleFrameAtlas);
            SerializeAtlasFrame(o, "AttackFrame", c.AttackFrameAtlas);
            SerializeAtlasFrame(o, "HitFrame", c.HitFrameAtlas);
            SerializeAtlasFrame(o, "DownFrame", c.DownFrameAtlas);
            o << YAML::Key << "IdleFrameCount" << YAML::Value << c.IdleFrameCount;
            o << YAML::Key << "AttackFrameCount" << YAML::Value << c.AttackFrameCount;
            o << YAML::Key << "HitFrameCount" << YAML::Value << c.HitFrameCount;
            o << YAML::Key << "DownFrameCount" << YAML::Value << c.DownFrameCount;
            o << YAML::Key << "AnimationFrameRate" << YAML::Value << c.AnimationFrameRate;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TurnCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.Slot = n["Slot"].as<int>(c.Slot);
            c.DisplayName = n["DisplayName"].as<std::string>(c.DisplayName);
            c.RoleName = n["RoleName"].as<std::string>(c.RoleName);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.MaxMana = n["MaxMana"].as<float>(c.MaxMana);
            c.Mana = n["Mana"].as<float>(c.Mana);
            c.Attack = n["Attack"].as<float>(c.Attack);
            c.Magic = n["Magic"].as<float>(c.Magic);
            c.Defense = n["Defense"].as<float>(c.Defense);
            c.Speed = n["Speed"].as<float>(c.Speed);
            c.Controllable = n["Controllable"].as<bool>(c.Controllable);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
            c.BasicSkillId = n["BasicSkillId"].as<std::string>(c.BasicSkillId);
            c.Skill1Id = n["Skill1Id"].as<std::string>(c.Skill1Id);
            c.Skill2Id = n["Skill2Id"].as<std::string>(c.Skill2Id);
            c.Skill3Id = n["Skill3Id"].as<std::string>(c.Skill3Id);
            c.HealthBarEntityName = n["HealthBarEntityName"].as<std::string>(c.HealthBarEntityName);
            c.ManaBarEntityName = n["ManaBarEntityName"].as<std::string>(c.ManaBarEntityName);
            c.StatusTextEntityName = n["StatusTextEntityName"].as<std::string>(c.StatusTextEntityName);
            c.TargetButtonEntityName = n["TargetButtonEntityName"].as<std::string>(c.TargetButtonEntityName);
            c.TargetMarkerEntityName = n["TargetMarkerEntityName"].as<std::string>(c.TargetMarkerEntityName);
            c.IdleFramePattern = n["IdleFramePattern"].as<std::string>(c.IdleFramePattern);
            c.AttackFramePattern = n["AttackFramePattern"].as<std::string>(c.AttackFramePattern);
            c.HitFramePattern = n["HitFramePattern"].as<std::string>(c.HitFramePattern);
            c.DownFramePattern = n["DownFramePattern"].as<std::string>(c.DownFramePattern);
            DeserializeAtlasFrame(n, "IdleFrame", c.IdleFrameAtlas);
            DeserializeAtlasFrame(n, "AttackFrame", c.AttackFrameAtlas);
            DeserializeAtlasFrame(n, "HitFrame", c.HitFrameAtlas);
            DeserializeAtlasFrame(n, "DownFrame", c.DownFrameAtlas);
            c.IdleFrameCount = n["IdleFrameCount"].as<int>(c.IdleFrameCount);
            c.AttackFrameCount = n["AttackFrameCount"].as<int>(c.AttackFrameCount);
            c.HitFrameCount = n["HitFrameCount"].as<int>(c.HitFrameCount);
            c.DownFrameCount = n["DownFrameCount"].as<int>(c.DownFrameCount);
            c.AnimationFrameRate = n["AnimationFrameRate"].as<float>(c.AnimationFrameRate);
        }
    };

    using TurnCombatModuleSceneComponents = ComponentGroup
    <
        TurnCombatLevelComponent,
        TurnCombatantComponent
    >;

    void SerializeTurnCombatModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(TurnCombatModuleSceneComponents{}, out, entity);
    }

    void DeserializeTurnCombatModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(TurnCombatModuleSceneComponents{}, node, entity);
    }

} // namespace Wheatear
