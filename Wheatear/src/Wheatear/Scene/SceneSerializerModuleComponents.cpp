#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

#include "Wheatear/Modules/GameplayModuleComponents.h"

namespace Wheatear {

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
            o << YAML::Key << "HistoryPanelEntityName" << YAML::Value << c.HistoryPanelEntityName;
            o << YAML::Key << "SettingsPanelEntityName" << YAML::Value << c.SettingsPanelEntityName;
            o << YAML::Key << "SettingsTextEntityName" << YAML::Value << c.SettingsTextEntityName;
            o << YAML::Key << "SaveLoadPanelEntityName" << YAML::Value << c.SaveLoadPanelEntityName;
            o << YAML::Key << "SaveLoadTextEntityName" << YAML::Value << c.SaveLoadTextEntityName;
            o << YAML::Key << "SystemMessageEntityName" << YAML::Value << c.SystemMessageEntityName;
            o << YAML::Key << "MusicNoticePanelEntityName" << YAML::Value << c.MusicNoticePanelEntityName;
            o << YAML::Key << "MusicNoticeTextEntityName" << YAML::Value << c.MusicNoticeTextEntityName;
            o << YAML::Key << "SaveDirectory" << YAML::Value << c.SaveDirectory;
            o << YAML::Key << "AutoLoadSlot" << YAML::Value << c.AutoLoadSlot;
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
            c.HistoryPanelEntityName = n["HistoryPanelEntityName"].as<std::string>(c.HistoryPanelEntityName);
            c.SettingsPanelEntityName = n["SettingsPanelEntityName"].as<std::string>(c.SettingsPanelEntityName);
            c.SettingsTextEntityName = n["SettingsTextEntityName"].as<std::string>(c.SettingsTextEntityName);
            c.SaveLoadPanelEntityName = n["SaveLoadPanelEntityName"].as<std::string>(c.SaveLoadPanelEntityName);
            c.SaveLoadTextEntityName = n["SaveLoadTextEntityName"].as<std::string>(c.SaveLoadTextEntityName);
            c.SystemMessageEntityName = n["SystemMessageEntityName"].as<std::string>(c.SystemMessageEntityName);
            c.MusicNoticePanelEntityName = n["MusicNoticePanelEntityName"].as<std::string>(c.MusicNoticePanelEntityName);
            c.MusicNoticeTextEntityName = n["MusicNoticeTextEntityName"].as<std::string>(c.MusicNoticeTextEntityName);
            c.SaveDirectory = n["SaveDirectory"].as<std::string>(c.SaveDirectory);
            c.AutoLoadSlot = n["AutoLoadSlot"].as<int>(c.AutoLoadSlot);
        }
    };
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

    template<> struct ComponentSerializer<SideCombatLevelComponent> {
        static constexpr const char* Key = "SideCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "LevelId" << YAML::Value << c.LevelId;
            o << YAML::Key << "TuningPath" << YAML::Value << c.TuningPath;
            o << YAML::Key << "ArenaMin" << YAML::Value << c.ArenaMin;
            o << YAML::Key << "ArenaMax" << YAML::Value << c.ArenaMax;
            o << YAML::Key << "GroundY" << YAML::Value << c.GroundY;
            o << YAML::Key << "LaneMinY" << YAML::Value << c.LaneMinY;
            o << YAML::Key << "LaneMaxY" << YAML::Value << c.LaneMaxY;
            o << YAML::Key << "PlayerEntityName" << YAML::Value << c.PlayerEntityName;
            o << YAML::Key << "BossEntityName" << YAML::Value << c.BossEntityName;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "ComboTextEntityName" << YAML::Value << c.ComboTextEntityName;
            o << YAML::Key << "SkillTextEntityName" << YAML::Value << c.SkillTextEntityName;
            o << YAML::Key << "RewardTextEntityName" << YAML::Value << c.RewardTextEntityName;
            o << YAML::Key << "PlayerHealthBarEntityName" << YAML::Value << c.PlayerHealthBarEntityName;
            o << YAML::Key << "PlayerHealthTextEntityName" << YAML::Value << c.PlayerHealthTextEntityName;
            o << YAML::Key << "BossHealthBarEntityName" << YAML::Value << c.BossHealthBarEntityName;
            o << YAML::Key << "BossHealthTextEntityName" << YAML::Value << c.BossHealthTextEntityName;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "ResultSceneFadeDuration" << YAML::Value << c.ResultSceneFadeDuration;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "ComboDropDelay" << YAML::Value << c.ComboDropDelay;
            o << YAML::Key << "FirstClearRewardText" << YAML::Value << YAML::DoubleQuoted << c.FirstClearRewardText;
            o << YAML::Key << "WaveModeEnabled" << YAML::Value << c.WaveModeEnabled;
            o << YAML::Key << "WaveCount" << YAML::Value << c.WaveCount;
            o << YAML::Key << "Wave1RightWall" << YAML::Value << c.Wave1RightWall;
            o << YAML::Key << "Wave2RightWall" << YAML::Value << c.Wave2RightWall;
            o << YAML::Key << "Wave3RightWall" << YAML::Value << c.Wave3RightWall;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.LevelId = n["LevelId"].as<std::string>(c.LevelId);
            c.TuningPath = n["TuningPath"].as<std::string>(c.TuningPath);
            c.ArenaMin = n["ArenaMin"].as<glm::vec2>(c.ArenaMin);
            c.ArenaMax = n["ArenaMax"].as<glm::vec2>(c.ArenaMax);
            c.GroundY = n["GroundY"].as<float>(c.GroundY);
            c.LaneMinY = n["LaneMinY"].as<float>(c.LaneMinY);
            c.LaneMaxY = n["LaneMaxY"].as<float>(c.LaneMaxY);
            c.PlayerEntityName = n["PlayerEntityName"].as<std::string>(c.PlayerEntityName);
            c.BossEntityName = n["BossEntityName"].as<std::string>(c.BossEntityName);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.ComboTextEntityName = n["ComboTextEntityName"].as<std::string>(c.ComboTextEntityName);
            c.SkillTextEntityName = n["SkillTextEntityName"].as<std::string>(c.SkillTextEntityName);
            c.RewardTextEntityName = n["RewardTextEntityName"].as<std::string>(c.RewardTextEntityName);
            c.PlayerHealthBarEntityName = n["PlayerHealthBarEntityName"].as<std::string>(c.PlayerHealthBarEntityName);
            c.PlayerHealthTextEntityName = n["PlayerHealthTextEntityName"].as<std::string>(c.PlayerHealthTextEntityName);
            c.BossHealthBarEntityName = n["BossHealthBarEntityName"].as<std::string>(c.BossHealthBarEntityName);
            c.BossHealthTextEntityName = n["BossHealthTextEntityName"].as<std::string>(c.BossHealthTextEntityName);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.ResultSceneFadeDuration = n["ResultSceneFadeDuration"].as<float>(c.ResultSceneFadeDuration);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.ComboDropDelay = n["ComboDropDelay"].as<float>(c.ComboDropDelay);
            c.FirstClearRewardText = n["FirstClearRewardText"].as<std::string>(c.FirstClearRewardText);
            c.WaveModeEnabled = n["WaveModeEnabled"].as<bool>(c.WaveModeEnabled);
            c.WaveCount = n["WaveCount"].as<int>(c.WaveCount);
            c.Wave1RightWall = n["Wave1RightWall"].as<float>(c.Wave1RightWall);
            c.Wave2RightWall = n["Wave2RightWall"].as<float>(c.Wave2RightWall);
            c.Wave3RightWall = n["Wave3RightWall"].as<float>(c.Wave3RightWall);
        }
    };

    template<> struct ComponentSerializer<SideCombatantComponent> {
        static constexpr const char* Key = "SideCombatantComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "Attack" << YAML::Value << c.Attack;
            o << YAML::Key << "Defense" << YAML::Value << c.Defense;
            o << YAML::Key << "MoveSpeed" << YAML::Value << c.MoveSpeed;
            o << YAML::Key << "CollisionSize" << YAML::Value << c.CollisionSize;
            o << YAML::Key << "CollisionHeight" << YAML::Value << c.CollisionHeight;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::Key << "KnockbackResistance" << YAML::Value << c.KnockbackResistance;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.Attack = n["Attack"].as<float>(c.Attack);
            c.Defense = n["Defense"].as<float>(c.Defense);
            c.MoveSpeed = n["MoveSpeed"].as<float>(c.MoveSpeed);
            c.CollisionSize = n["CollisionSize"].as<glm::vec2>(c.CollisionSize);
            c.CollisionHeight = n["CollisionHeight"].as<float>(c.CollisionHeight);
            c.GravityScale = n["GravityScale"].as<float>(c.GravityScale);
            c.KnockbackResistance = n["KnockbackResistance"].as<float>(c.KnockbackResistance);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
        }
    };

    template<> struct ComponentSerializer<SidePlayerControllerComponent> {
        static constexpr const char* Key = "SidePlayerControllerComponent";
        static void Serialize(YAML::Emitter& o, const SidePlayerControllerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MaxJumps" << YAML::Value << c.MaxJumps;
            o << YAML::Key << "JumpImpulse" << YAML::Value << c.JumpImpulse;
            o << YAML::Key << "Gravity" << YAML::Value << c.Gravity;
            o << YAML::Key << "AirControl" << YAML::Value << c.AirControl;
            o << YAML::Key << "JumpBufferTime" << YAML::Value << c.JumpBufferTime;
            o << YAML::Key << "CoyoteTime" << YAML::Value << c.CoyoteTime;
            o << YAML::Key << "LaneSpeedScale" << YAML::Value << c.LaneSpeedScale;
            o << YAML::Key << "LaneAcceleration" << YAML::Value << c.LaneAcceleration;
            o << YAML::Key << "GroundFriction" << YAML::Value << c.GroundFriction;
            o << YAML::Key << "BasicCooldown" << YAML::Value << c.BasicCooldown;
            o << YAML::Key << "LauncherCooldown" << YAML::Value << c.LauncherCooldown;
            o << YAML::Key << "MagicBoltCooldown" << YAML::Value << c.MagicBoltCooldown;
            o << YAML::Key << "AllySupportCooldown" << YAML::Value << c.AllySupportCooldown;
            o << YAML::Key << "DashCooldown" << YAML::Value << c.DashCooldown;
            o << YAML::Key << "DashManaCost" << YAML::Value << c.DashManaCost;
            o << YAML::Key << "DashSpeed" << YAML::Value << c.DashSpeed;
            o << YAML::Key << "DashInvulnerableTime" << YAML::Value << c.DashInvulnerableTime;
            o << YAML::Key << "MaxMana" << YAML::Value << c.MaxMana;
            o << YAML::Key << "LauncherManaCost" << YAML::Value << c.LauncherManaCost;
            o << YAML::Key << "MagicBoltManaCost" << YAML::Value << c.MagicBoltManaCost;
            o << YAML::Key << "AllySupportManaCost" << YAML::Value << c.AllySupportManaCost;
            o << YAML::Key << "HealItemAmount" << YAML::Value << c.HealItemAmount;
            o << YAML::Key << "ManaItemAmount" << YAML::Value << c.ManaItemAmount;
            o << YAML::Key << "AttackBuffMultiplier" << YAML::Value << c.AttackBuffMultiplier;
            o << YAML::Key << "AttackBuffDuration" << YAML::Value << c.AttackBuffDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePlayerControllerComponent& c) {
            c.MaxJumps = n["MaxJumps"].as<int>(c.MaxJumps);
            c.JumpImpulse = n["JumpImpulse"].as<float>(c.JumpImpulse);
            c.Gravity = n["Gravity"].as<float>(c.Gravity);
            c.AirControl = n["AirControl"].as<float>(c.AirControl);
            c.JumpBufferTime = n["JumpBufferTime"].as<float>(c.JumpBufferTime);
            c.CoyoteTime = n["CoyoteTime"].as<float>(c.CoyoteTime);
            c.LaneSpeedScale = n["LaneSpeedScale"].as<float>(c.LaneSpeedScale);
            c.LaneAcceleration = n["LaneAcceleration"].as<float>(c.LaneAcceleration);
            c.GroundFriction = n["GroundFriction"].as<float>(c.GroundFriction);
            c.BasicCooldown = n["BasicCooldown"].as<float>(c.BasicCooldown);
            c.LauncherCooldown = n["LauncherCooldown"].as<float>(c.LauncherCooldown);
            c.MagicBoltCooldown = n["MagicBoltCooldown"].as<float>(c.MagicBoltCooldown);
            c.AllySupportCooldown = n["AllySupportCooldown"].as<float>(c.AllySupportCooldown);
            c.DashCooldown = n["DashCooldown"].as<float>(c.DashCooldown);
            c.DashManaCost = n["DashManaCost"].as<float>(c.DashManaCost);
            c.DashSpeed = n["DashSpeed"].as<float>(c.DashSpeed);
            c.DashInvulnerableTime = n["DashInvulnerableTime"].as<float>(c.DashInvulnerableTime);
            c.MaxMana = n["MaxMana"].as<float>(c.MaxMana);
            c.LauncherManaCost = n["LauncherManaCost"].as<float>(c.LauncherManaCost);
            c.MagicBoltManaCost = n["MagicBoltManaCost"].as<float>(c.MagicBoltManaCost);
            c.AllySupportManaCost = n["AllySupportManaCost"].as<float>(c.AllySupportManaCost);
            c.HealItemAmount = n["HealItemAmount"].as<float>(c.HealItemAmount);
            c.ManaItemAmount = n["ManaItemAmount"].as<float>(c.ManaItemAmount);
            c.AttackBuffMultiplier = n["AttackBuffMultiplier"].as<float>(c.AttackBuffMultiplier);
            c.AttackBuffDuration = n["AttackBuffDuration"].as<float>(c.AttackBuffDuration);
        }
    };

    template<> struct ComponentSerializer<SideEnemyAIComponent> {
        static constexpr const char* Key = "SideEnemyAIComponent";
        static void Serialize(YAML::Emitter& o, const SideEnemyAIComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Kind" << YAML::Value << (int)c.Kind;
            o << YAML::Key << "AggroRange" << YAML::Value << c.AggroRange;
            o << YAML::Key << "AttackRange" << YAML::Value << c.AttackRange;
            o << YAML::Key << "PreferredRange" << YAML::Value << c.PreferredRange;
            o << YAML::Key << "AttackInterval" << YAML::Value << c.AttackInterval;
            o << YAML::Key << "PatrolMinX" << YAML::Value << c.PatrolMinX;
            o << YAML::Key << "PatrolMaxX" << YAML::Value << c.PatrolMaxX;
            o << YAML::Key << "LaneTolerance" << YAML::Value << c.LaneTolerance;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideEnemyAIComponent& c) {
            c.Kind = (SideEnemyKind)n["Kind"].as<int>((int)c.Kind);
            c.AggroRange = n["AggroRange"].as<float>(c.AggroRange);
            c.AttackRange = n["AttackRange"].as<float>(c.AttackRange);
            c.PreferredRange = n["PreferredRange"].as<float>(c.PreferredRange);
            c.AttackInterval = n["AttackInterval"].as<float>(c.AttackInterval);
            c.PatrolMinX = n["PatrolMinX"].as<float>(c.PatrolMinX);
            c.PatrolMaxX = n["PatrolMaxX"].as<float>(c.PatrolMaxX);
            c.LaneTolerance = n["LaneTolerance"].as<float>(c.LaneTolerance);
        }
    };

    template<> struct ComponentSerializer<SideHitboxComponent> {
        static constexpr const char* Key = "SideHitboxComponent";
        static void Serialize(YAML::Emitter& o, const SideHitboxComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "AttackKind" << YAML::Value << (int)c.AttackKind;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Velocity" << YAML::Value << c.Velocity;
            o << YAML::Key << "LaunchVelocity" << YAML::Value << c.LaunchVelocity;
            o << YAML::Key << "AirHeight" << YAML::Value << c.AirHeight;
            o << YAML::Key << "AirRange" << YAML::Value << c.AirRange;
            o << YAML::Key << "Damage" << YAML::Value << c.Damage;
            o << YAML::Key << "Lifetime" << YAML::Value << c.Lifetime;
            o << YAML::Key << "HitStun" << YAML::Value << c.HitStun;
            o << YAML::Key << "AttackerAirImpulse" << YAML::Value << c.AttackerAirImpulse;
            o << YAML::Key << "AttackerAirFallStep" << YAML::Value << c.AttackerAirFallStep;
            o << YAML::Key << "TargetAirFallStep" << YAML::Value << c.TargetAirFallStep;
            o << YAML::Key << "ProtectionGain" << YAML::Value << c.ProtectionGain;
            o << YAML::Key << "DestroyOnHit" << YAML::Value << c.DestroyOnHit;
            o << YAML::Key << "TextureFramePattern" << YAML::Value << c.TextureFramePattern;
            o << YAML::Key << "TextureFrameCount" << YAML::Value << c.TextureFrameCount;
            o << YAML::Key << "TextureFrameRate" << YAML::Value << c.TextureFrameRate;
            o << YAML::Key << "HitSound" << YAML::Value << c.HitSound;
            o << YAML::Key << "HitSoundVolume" << YAML::Value << c.HitSoundVolume;
            o << YAML::Key << "HitPause" << YAML::Value << c.HitPause;
            o << YAML::Key << "CameraShake" << YAML::Value << c.CameraShake;
            o << YAML::Key << "CameraShakeDuration" << YAML::Value << c.CameraShakeDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideHitboxComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.AttackKind = (SideAttackKind)n["AttackKind"].as<int>((int)c.AttackKind);
            c.Size = n["Size"].as<glm::vec2>(c.Size);
            c.Velocity = n["Velocity"].as<glm::vec2>(c.Velocity);
            c.LaunchVelocity = n["LaunchVelocity"].as<glm::vec2>(c.LaunchVelocity);
            c.AirHeight = n["AirHeight"].as<float>(c.AirHeight);
            c.AirRange = n["AirRange"].as<float>(c.AirRange);
            c.Damage = n["Damage"].as<float>(c.Damage);
            c.Lifetime = n["Lifetime"].as<float>(c.Lifetime);
            c.HitStun = n["HitStun"].as<float>(c.HitStun);
            c.AttackerAirImpulse = n["AttackerAirImpulse"].as<float>(c.AttackerAirImpulse);
            c.AttackerAirFallStep = n["AttackerAirFallStep"].as<float>(c.AttackerAirFallStep);
            c.TargetAirFallStep = n["TargetAirFallStep"].as<float>(c.TargetAirFallStep);
            c.ProtectionGain = n["ProtectionGain"].as<float>(c.ProtectionGain);
            c.DestroyOnHit = n["DestroyOnHit"].as<bool>(c.DestroyOnHit);
            c.TextureFramePattern = n["TextureFramePattern"].as<std::string>(c.TextureFramePattern);
            c.TextureFrameCount = n["TextureFrameCount"].as<int>(c.TextureFrameCount);
            c.TextureFrameRate = n["TextureFrameRate"].as<float>(c.TextureFrameRate);
            c.HitSound = n["HitSound"].as<std::string>(c.HitSound);
            c.HitSoundVolume = n["HitSoundVolume"].as<float>(c.HitSoundVolume);
            c.HitPause = n["HitPause"].as<float>(c.HitPause);
            c.CameraShake = n["CameraShake"].as<float>(c.CameraShake);
            c.CameraShakeDuration = n["CameraShakeDuration"].as<float>(c.CameraShakeDuration);
        }
    };

    template<> struct ComponentSerializer<SidePickupComponent> {
        static constexpr const char* Key = "SidePickupComponent";
        static void Serialize(YAML::Emitter& o, const SidePickupComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ItemId" << YAML::Value << c.ItemId;
            o << YAML::Key << "DisplayName" << YAML::Value << c.DisplayName;
            o << YAML::Key << "Amount" << YAML::Value << c.Amount;
            o << YAML::Key << "PickupRadius" << YAML::Value << c.PickupRadius;
            o << YAML::Key << "AttractRadius" << YAML::Value << c.AttractRadius;
            o << YAML::Key << "AttractSpeed" << YAML::Value << c.AttractSpeed;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePickupComponent& c) {
            c.ItemId = n["ItemId"].as<std::string>(c.ItemId);
            c.DisplayName = n["DisplayName"].as<std::string>(c.DisplayName);
            c.Amount = n["Amount"].as<int>(c.Amount);
            c.PickupRadius = n["PickupRadius"].as<float>(c.PickupRadius);
            c.AttractRadius = n["AttractRadius"].as<float>(c.AttractRadius);
            c.AttractSpeed = n["AttractSpeed"].as<float>(c.AttractSpeed);
        }
    };

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
            c.IdleFrameCount = n["IdleFrameCount"].as<int>(c.IdleFrameCount);
            c.AttackFrameCount = n["AttackFrameCount"].as<int>(c.AttackFrameCount);
            c.HitFrameCount = n["HitFrameCount"].as<int>(c.HitFrameCount);
            c.DownFrameCount = n["DownFrameCount"].as<int>(c.DownFrameCount);
            c.AnimationFrameRate = n["AnimationFrameRate"].as<float>(c.AnimationFrameRate);
        }
    };

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
            c.IdleFrameCount = n["IdleFrameCount"].as<int>(c.IdleFrameCount);
            c.AttackFrameCount = n["AttackFrameCount"].as<int>(c.AttackFrameCount);
            c.HitFrameCount = n["HitFrameCount"].as<int>(c.HitFrameCount);
            c.DownFrameCount = n["DownFrameCount"].as<int>(c.DownFrameCount);
            c.AnimationFrameRate = n["AnimationFrameRate"].as<float>(c.AnimationFrameRate);
        }
    };

    void SerializeModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(GameplayModuleSceneComponents{}, out, entity);
    }

    void DeserializeModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(GameplayModuleSceneComponents{}, node, entity);
    }

} // namespace Wheatear
