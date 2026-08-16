#include "wtpch.h"
#include "SceneSerializerComponentGroups.h"
#include "SceneSerializerComponentSupport.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {


    template<> struct ComponentSerializer<TacticalCombatLevelComponent> {
        static constexpr const char* Key = "TacticalCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const TacticalCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "LevelId" << YAML::Value << c.LevelId;
            o << YAML::Key << "GridWidth" << YAML::Value << c.GridWidth;
            o << YAML::Key << "GridHeight" << YAML::Value << c.GridHeight;
            o << YAML::Key << "BoardOrigin" << YAML::Value << c.BoardOrigin;
            o << YAML::Key << "CellSize" << YAML::Value << c.CellSize;
            o << YAML::Key << "CellEntityPrefix" << YAML::Value << c.CellEntityPrefix;
            o << YAML::Key << "UnitEntityPrefix" << YAML::Value << c.UnitEntityPrefix;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "PhaseTextEntityName" << YAML::Value << c.PhaseTextEntityName;
            o << YAML::Key << "DetailTextEntityName" << YAML::Value << c.DetailTextEntityName;
            o << YAML::Key << "CommandPanelEntityName" << YAML::Value << c.CommandPanelEntityName;
            o << YAML::Key << "ActionEffectEntityName" << YAML::Value << c.ActionEffectEntityName;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "TuningPath" << YAML::Value << YAML::DoubleQuoted << c.TuningPath;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "IntroDuration" << YAML::Value << c.IntroDuration;
            o << YAML::Key << "ActionDuration" << YAML::Value << c.ActionDuration;
            o << YAML::Key << "EnemyStepDuration" << YAML::Value << c.EnemyStepDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "TileNormalColor" << YAML::Value << c.TileNormalColor;
            o << YAML::Key << "TileMoveColor" << YAML::Value << c.TileMoveColor;
            o << YAML::Key << "TileAttackColor" << YAML::Value << c.TileAttackColor;
            o << YAML::Key << "TileSelectedColor" << YAML::Value << c.TileSelectedColor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TacticalCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.LevelId = n["LevelId"].as<std::string>(c.LevelId);
            c.GridWidth = n["GridWidth"].as<int>(c.GridWidth);
            c.GridHeight = n["GridHeight"].as<int>(c.GridHeight);
            c.BoardOrigin = n["BoardOrigin"].as<glm::vec2>(c.BoardOrigin);
            c.CellSize = n["CellSize"].as<glm::vec2>(c.CellSize);
            c.CellEntityPrefix = n["CellEntityPrefix"].as<std::string>(c.CellEntityPrefix);
            c.UnitEntityPrefix = n["UnitEntityPrefix"].as<std::string>(c.UnitEntityPrefix);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.PhaseTextEntityName = n["PhaseTextEntityName"].as<std::string>(c.PhaseTextEntityName);
            c.DetailTextEntityName = n["DetailTextEntityName"].as<std::string>(c.DetailTextEntityName);
            c.CommandPanelEntityName = n["CommandPanelEntityName"].as<std::string>(c.CommandPanelEntityName);
            c.ActionEffectEntityName = n["ActionEffectEntityName"].as<std::string>(c.ActionEffectEntityName);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.TuningPath = n["TuningPath"].as<std::string>(c.TuningPath);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.IntroDuration = n["IntroDuration"].as<float>(c.IntroDuration);
            c.ActionDuration = n["ActionDuration"].as<float>(c.ActionDuration);
            c.EnemyStepDuration = n["EnemyStepDuration"].as<float>(c.EnemyStepDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.TileNormalColor = n["TileNormalColor"].as<glm::vec4>(c.TileNormalColor);
            c.TileMoveColor = n["TileMoveColor"].as<glm::vec4>(c.TileMoveColor);
            c.TileAttackColor = n["TileAttackColor"].as<glm::vec4>(c.TileAttackColor);
            c.TileSelectedColor = n["TileSelectedColor"].as<glm::vec4>(c.TileSelectedColor);
        }
    };

    template<> struct ComponentSerializer<TacticalUnitComponent> {
        static constexpr const char* Key = "TacticalUnitComponent";
        static void Serialize(YAML::Emitter& o, const TacticalUnitComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "Slot" << YAML::Value << c.Slot;
            o << YAML::Key << "GridX" << YAML::Value << c.GridX;
            o << YAML::Key << "GridY" << YAML::Value << c.GridY;
            o << YAML::Key << "DisplayName" << YAML::Value << c.DisplayName;
            o << YAML::Key << "ClassName" << YAML::Value << c.ClassName;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "Attack" << YAML::Value << c.Attack;
            o << YAML::Key << "Magic" << YAML::Value << c.Magic;
            o << YAML::Key << "Defense" << YAML::Value << c.Defense;
            o << YAML::Key << "MoveRange" << YAML::Value << c.MoveRange;
            o << YAML::Key << "AttackRange" << YAML::Value << c.AttackRange;
            o << YAML::Key << "Controllable" << YAML::Value << c.Controllable;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::Key << "BasicSkillId" << YAML::Value << c.BasicSkillId;
            o << YAML::Key << "Skill1Id" << YAML::Value << c.Skill1Id;
            o << YAML::Key << "Skill2Id" << YAML::Value << c.Skill2Id;
            o << YAML::Key << "HealthBarEntityName" << YAML::Value << c.HealthBarEntityName;
            o << YAML::Key << "StatusTextEntityName" << YAML::Value << c.StatusTextEntityName;
            o << YAML::Key << "MarkerEntityName" << YAML::Value << c.MarkerEntityName;
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
        static void Deserialize(const YAML::Node& n, TacticalUnitComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.Slot = n["Slot"].as<int>(c.Slot);
            c.GridX = n["GridX"].as<int>(c.GridX);
            c.GridY = n["GridY"].as<int>(c.GridY);
            c.DisplayName = n["DisplayName"].as<std::string>(c.DisplayName);
            c.ClassName = n["ClassName"].as<std::string>(c.ClassName);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.Attack = n["Attack"].as<float>(c.Attack);
            c.Magic = n["Magic"].as<float>(c.Magic);
            c.Defense = n["Defense"].as<float>(c.Defense);
            c.MoveRange = n["MoveRange"].as<int>(c.MoveRange);
            c.AttackRange = n["AttackRange"].as<int>(c.AttackRange);
            c.Controllable = n["Controllable"].as<bool>(c.Controllable);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
            c.BasicSkillId = n["BasicSkillId"].as<std::string>(c.BasicSkillId);
            c.Skill1Id = n["Skill1Id"].as<std::string>(c.Skill1Id);
            c.Skill2Id = n["Skill2Id"].as<std::string>(c.Skill2Id);
            c.HealthBarEntityName = n["HealthBarEntityName"].as<std::string>(c.HealthBarEntityName);
            c.StatusTextEntityName = n["StatusTextEntityName"].as<std::string>(c.StatusTextEntityName);
            c.MarkerEntityName = n["MarkerEntityName"].as<std::string>(c.MarkerEntityName);
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


    using TacticalCombatModuleSceneComponents = ComponentGroup
    <
        TacticalCombatLevelComponent,
        TacticalUnitComponent
    >;

    void SerializeTacticalCombatModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(TacticalCombatModuleSceneComponents{}, out, entity);
    }

    void DeserializeTacticalCombatModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(TacticalCombatModuleSceneComponents{}, node, entity);
    }

} // namespace Wheatear
