#include "wtpch.h"
#include "SceneSerializerComponentGroups.h"
#include "SceneSerializerComponentSupport.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {

    template<> struct ComponentSerializer<ArcadeCombatLevelComponent> {
        static constexpr const char* Key = "ArcadeCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "ArenaMin" << YAML::Value << c.ArenaMin;
            o << YAML::Key << "ArenaMax" << YAML::Value << c.ArenaMax;
            o << YAML::Key << "PlayerEntityName" << YAML::Value << c.PlayerEntityName;
            o << YAML::Key << "BossEntityName" << YAML::Value << c.BossEntityName;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "PausePanelEntityName" << YAML::Value << c.PausePanelEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "WeaponTextEntityName" << YAML::Value << c.WeaponTextEntityName;
            o << YAML::Key << "PlayerHealthBarEntityName" << YAML::Value << c.PlayerHealthBarEntityName;
            o << YAML::Key << "PlayerHealthTextEntityName" << YAML::Value << c.PlayerHealthTextEntityName;
            o << YAML::Key << "BossHealthBarEntityName" << YAML::Value << c.BossHealthBarEntityName;
            o << YAML::Key << "BossHealthTextEntityName" << YAML::Value << c.BossHealthTextEntityName;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "ResultSceneFadeDuration" << YAML::Value << c.ResultSceneFadeDuration;
            o << YAML::Key << "BossDefeatFadeDuration" << YAML::Value << c.BossDefeatFadeDuration;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "TuningPath" << YAML::Value << YAML::DoubleQuoted << c.TuningPath;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.ArenaMin = n["ArenaMin"].as<glm::vec2>(c.ArenaMin);
            c.ArenaMax = n["ArenaMax"].as<glm::vec2>(c.ArenaMax);
            c.PlayerEntityName = n["PlayerEntityName"].as<std::string>(c.PlayerEntityName);
            c.BossEntityName = n["BossEntityName"].as<std::string>(c.BossEntityName);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.PausePanelEntityName = n["PausePanelEntityName"].as<std::string>(c.PausePanelEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.WeaponTextEntityName = n["WeaponTextEntityName"].as<std::string>(c.WeaponTextEntityName);
            c.PlayerHealthBarEntityName = n["PlayerHealthBarEntityName"].as<std::string>(c.PlayerHealthBarEntityName);
            c.PlayerHealthTextEntityName = n["PlayerHealthTextEntityName"].as<std::string>(c.PlayerHealthTextEntityName);
            c.BossHealthBarEntityName = n["BossHealthBarEntityName"].as<std::string>(c.BossHealthBarEntityName);
            c.BossHealthTextEntityName = n["BossHealthTextEntityName"].as<std::string>(c.BossHealthTextEntityName);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.ResultSceneFadeDuration = n["ResultSceneFadeDuration"].as<float>(c.ResultSceneFadeDuration);
            c.BossDefeatFadeDuration = n["BossDefeatFadeDuration"].as<float>(c.BossDefeatFadeDuration);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.TuningPath = n["TuningPath"].as<std::string>(c.TuningPath);
        }
    };

    template<> struct ComponentSerializer<ArcadeCombatantComponent> {
        static constexpr const char* Key = "ArcadeCombatantComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "MoveSpeed" << YAML::Value << c.MoveSpeed;
            o << YAML::Key << "CollisionRadius" << YAML::Value << c.CollisionRadius;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.MoveSpeed = n["MoveSpeed"].as<float>(c.MoveSpeed);
            c.CollisionRadius = n["CollisionRadius"].as<float>(c.CollisionRadius);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
        }
    };

    template<> struct ComponentSerializer<ArcadePlayerControllerComponent> {
        static constexpr const char* Key = "ArcadePlayerControllerComponent";
        static void Serialize(YAML::Emitter& o, const ArcadePlayerControllerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "CurrentWeapon" << YAML::Value << (int)c.CurrentWeapon;
            o << YAML::Key << "AutoAim" << YAML::Value << c.AutoAim;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadePlayerControllerComponent& c) {
            c.CurrentWeapon = (ArcadeWeaponType)n["CurrentWeapon"].as<int>((int)c.CurrentWeapon);
            c.AutoAim = n["AutoAim"].as<bool>(c.AutoAim);
        }
    };

    template<> struct ComponentSerializer<ArcadeBossComponent> {
        static constexpr const char* Key = "ArcadeBossComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeBossComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Active" << YAML::Value << c.Active;
            o << YAML::Key << "IntroStartPosition" << YAML::Value << c.IntroStartPosition;
            o << YAML::Key << "FightPosition" << YAML::Value << c.FightPosition;
            o << YAML::Key << "IntroDuration" << YAML::Value << c.IntroDuration;
            o << YAML::Key << "ShootInterval" << YAML::Value << c.ShootInterval;
            o << YAML::Key << "JumpInterval" << YAML::Value << c.JumpInterval;
            o << YAML::Key << "JumpDuration" << YAML::Value << c.JumpDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeBossComponent& c) {
            c.Active = n["Active"].as<bool>(c.Active);
            c.IntroStartPosition = n["IntroStartPosition"].as<glm::vec3>(c.IntroStartPosition);
            c.FightPosition = n["FightPosition"].as<glm::vec3>(c.FightPosition);
            c.IntroDuration = n["IntroDuration"].as<float>(c.IntroDuration);
            c.ShootInterval = n["ShootInterval"].as<float>(c.ShootInterval);
            c.JumpInterval = n["JumpInterval"].as<float>(c.JumpInterval);
            c.JumpDuration = n["JumpDuration"].as<float>(c.JumpDuration);
        }
    };

    template<> struct ComponentSerializer<ArcadeProjectileComponent> {
        static constexpr const char* Key = "ArcadeProjectileComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeProjectileComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Velocity" << YAML::Value << c.Velocity;
            o << YAML::Key << "Damage" << YAML::Value << c.Damage;
            o << YAML::Key << "Lifetime" << YAML::Value << c.Lifetime;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "Heavy" << YAML::Value << c.Heavy;
            o << YAML::Key << "Melee" << YAML::Value << c.Melee;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeProjectileComponent& c) {
            c.Velocity = n["Velocity"].as<glm::vec2>(c.Velocity);
            c.Damage = n["Damage"].as<float>(c.Damage);
            c.Lifetime = n["Lifetime"].as<float>(c.Lifetime);
            c.Radius = n["Radius"].as<float>(c.Radius);
            c.Team = n["Team"].as<int>(c.Team);
            c.Heavy = n["Heavy"].as<bool>(c.Heavy);
            c.Melee = n["Melee"].as<bool>(c.Melee);
        }
    };

    template<> struct ComponentSerializer<ArcadeCoverComponent> {
        static constexpr const char* Key = "ArcadeCoverComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCoverComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "BlocksProjectiles" << YAML::Value << c.BlocksProjectiles;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCoverComponent& c) {
            c.Radius = n["Radius"].as<float>(c.Radius);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.BlocksProjectiles = n["BlocksProjectiles"].as<bool>(c.BlocksProjectiles);
        }
    };

    template<> struct ComponentSerializer<ArcadeTriggerComponent> {
        static constexpr const char* Key = "ArcadeTriggerComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeTriggerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Type" << YAML::Value << (int)c.Type;
            o << YAML::Key << "TriggerName" << YAML::Value << c.TriggerName;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeTriggerComponent& c) {
            c.Type = (ArcadeTriggerType)n["Type"].as<int>((int)c.Type);
            c.TriggerName = n["TriggerName"].as<std::string>(c.TriggerName);
            c.Radius = n["Radius"].as<float>(c.Radius);
        }
    };



    using ArcadeModuleSceneComponents = ComponentGroup
    <
        ArcadeCombatLevelComponent,
        ArcadeCombatantComponent,
        ArcadePlayerControllerComponent,
        ArcadeBossComponent,
        ArcadeProjectileComponent,
        ArcadeCoverComponent,
        ArcadeTriggerComponent
    >;

    void SerializeArcadeModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(ArcadeModuleSceneComponents{}, out, entity);
    }

    void DeserializeArcadeModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(ArcadeModuleSceneComponents{}, node, entity);
    }

} // namespace Wheatear
