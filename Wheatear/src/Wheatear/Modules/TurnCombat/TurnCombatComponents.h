#pragma once

#include "Wheatear/Core/UUID.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"
#include "Wheatear/Modules/Common/GameplayVisualService.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace Wheatear {

    enum class TurnCombatTeam
    {
        Neutral = 0,
        Player = 1,
        Enemy = 2
    };

    enum class TurnCombatPhase
    {
        Intro = 0,
        AwaitCommand = 1,
        AwaitTarget = 2,
        Acting = 3,
        Victory = 4,
        Defeat = 5
    };

    enum class TurnTargetRule
    {
        EnemySingle = 0,
        AllySingle = 1,
        Self = 2,
        EnemyAll = 3,
        AllyAll = 4
    };

    struct TurnCombatLevelComponent
    {
        bool PlayOnStart = true;
        std::string LevelId = "CH01_TURN_MagicSwordTrial";

        std::string FadeEntityName = "TC_Fade";
        std::string MessageTextEntityName = "TC_MessageText";
        std::string ActiveActorTextEntityName = "TC_ActiveActorText";
        std::string TurnOrderTextEntityName = "TC_TurnOrderText";
        std::string SkillDetailTextEntityName = "TC_SkillDetailText";
        std::string CommandPanelEntityName = "TC_CommandPanel";
        std::string TargetHintTextEntityName = "TC_TargetHintText";
        std::string ActionFlashEntityName = "TC_ActionFlash";
        std::string ActionEffectEntityName = "TC_ActionEffect";

        std::string VictorySceneCommand = "event:turn_combat_victory";
        std::string DefeatSceneCommand = "event:turn_combat_retry";

        float StartFadeDuration = 0.55f;
        float IntroDuration = 0.80f;
        float ActionDuration = 0.72f;
        float VictoryReturnDelay = 1.80f;
        float DefeatReturnDelay = 1.40f;

        float RuntimeElapsed = 0.0f;
        float RuntimeFadeAlpha = 1.0f;
        TurnCombatPhase RuntimePhase = TurnCombatPhase::Intro;
        int RuntimeRound = 1;
        int RuntimeTurnIndex = 0;
        std::vector<UUID> RuntimeTurnQueue;
        UUID RuntimeActiveActor = 0;
        std::string RuntimeCommandMenuPage = "root";
        std::string RuntimeSelectedSkillId;
        UUID RuntimeActionActor = 0;
        UUID RuntimeActionTarget = 0;
        std::string RuntimeActionSkillId;
        std::string RuntimeMessage;
        std::string RuntimeRequestedCommand;
        float RuntimeIntroTimer = 0.0f;
        float RuntimeActionTimer = 0.0f;
        float RuntimeResultTimer = 0.0f;
        bool RuntimeInitialized = false;
        bool RuntimeActionApplied = false;
        bool RuntimeResultCommandIssued = false;

        TurnCombatLevelComponent() = default;
        TurnCombatLevelComponent(const TurnCombatLevelComponent&) = default;
    };

    struct TurnCombatantComponent
    {
        int Team = (int)TurnCombatTeam::Neutral;
        int Slot = 0;
        std::string DisplayName = "Combatant";
        std::string RoleName = "";

        float MaxHealth = 100.0f;
        float Health = 100.0f;
        float MaxMana = 40.0f;
        float Mana = 40.0f;
        float Attack = 20.0f;
        float Magic = 16.0f;
        float Defense = 8.0f;
        float Speed = 10.0f;
        bool Controllable = false;
        bool Invulnerable = false;

        std::string BasicSkillId = "slash";
        std::string Skill1Id = "";
        std::string Skill2Id = "";
        std::string Skill3Id = "";

        std::string HealthBarEntityName;
        std::string ManaBarEntityName;
        std::string StatusTextEntityName;
        std::string TargetButtonEntityName;
        std::string TargetMarkerEntityName;

        std::string IdleFramePattern;
        std::string AttackFramePattern;
        std::string HitFramePattern;
        std::string DownFramePattern;
        GameplayVisualService::TextureAtlasFrameSpec IdleFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec AttackFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec HitFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec DownFrameAtlas;
        int IdleFrameCount = 1;
        int AttackFrameCount = 1;
        int HitFrameCount = 1;
        int DownFrameCount = 1;
        float AnimationFrameRate = 8.0f;

        bool RuntimeAlive = true;
        bool RuntimeGuarding = false;
        bool RuntimeSelectedTarget = false;
        float RuntimeHitFlashTimer = 0.0f;
        std::vector<WAO::RuntimeState> RuntimeStatusEffects;
        glm::vec3 RuntimeBaseTranslation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 RuntimeBaseScale = { 1.0f, 1.0f, 1.0f };
        bool RuntimeVisualCached = false;
        std::string RuntimeAnimationClip;
        float RuntimeAnimationTimer = 0.0f;

        TurnCombatantComponent() = default;
        TurnCombatantComponent(const TurnCombatantComponent&) = default;
    };

} // namespace Wheatear
