#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace Wheatear::WAO {

    enum class EffectType
    {
        None = 0,
        Damage,
        Heal,
        ModifyAttribute,
        AddState,
        RemoveState,
        StartCooldown,
        ConsumeResource,
        Launch,
        HitStun,
        EmitSignal
    };

    enum class EffectDurationPolicy
    {
        Instant = 0,
        Seconds,
        Turns,
        Infinite
    };

    struct ActionIntent
    {
        UUID Actor = 0;
        std::string ActionId;
        UUID ExplicitTarget = 0;
        glm::vec2 WorldPoint = { 0.0f, 0.0f };
        std::string InputId;
        std::string Source;
    };

    struct AttributeStore
    {
        std::unordered_map<std::string, float> Values;

        float Get(const std::string& id, float fallback = 0.0f) const;
        void Set(const std::string& id, float value);
        float Modify(const std::string& id, float delta, float fallback = 0.0f);
    };

    struct RuntimeState
    {
        std::string Id;
        std::string DisplayName;
        int RemainingTurns = 0;
        float RemainingSeconds = 0.0f;
        float Power = 0.0f;
        int StackCount = 1;
        bool Harmful = false;
        UUID Source = 0;
    };

    struct EffectSpec
    {
        EffectType Type = EffectType::None;
        UUID Source = 0;
        UUID Target = 0;
        std::string AttributeId;
        std::string StateId;
        std::string SignalId;
        float Value = 0.0f;
        int Turns = 0;
        float Seconds = 0.0f;
        EffectDurationPolicy DurationPolicy = EffectDurationPolicy::Instant;
    };

    struct ActionRecipe
    {
        std::string Id;
        std::string DisplayName;
        std::string Description;
        std::string IconPath;
        std::string AnimationId;
        std::string SoundPath;
        std::string EffectPath;
        std::vector<std::string> Tags;
        float Cooldown = 0.0f;
        float Duration = 0.0f;
        float Startup = 0.0f;
        float Recovery = 0.0f;
        float HitTime = 0.0f;
        float CancelStart = 0.0f;
        float CancelEnd = 0.0f;
        float MovementScale = 1.0f;
        std::unordered_map<std::string, float> ResourceCost;
        std::vector<std::string> RequiredStates;
        std::vector<std::string> BlockedStates;
        std::vector<std::string> RequiredTags;
        std::vector<std::string> BlockedTags;
        std::vector<EffectSpec> Effects;
        std::vector<std::string> Signals;
    };

    struct EffectBundle
    {
        std::vector<EffectSpec> Effects;

        bool Empty() const;
        void Add(const EffectSpec& effect);
    };

    struct EffectLedgerEntry
    {
        std::string ActionId;
        EffectType Type = EffectType::None;
        UUID Source = 0;
        UUID Target = 0;
        std::string Detail;
        float Value = 0.0f;
        bool Applied = false;
    };

    class WHEATEAR_API EffectLedger
    {
    public:
        void BeginAction(const ActionIntent& intent);
        void Record(const EffectLedgerEntry& entry);
        void Clear();

        const ActionIntent& CurrentIntent() const { return m_CurrentIntent; }
        const std::vector<EffectLedgerEntry>& Entries() const { return m_Entries; }

    private:
        ActionIntent m_CurrentIntent;
        std::vector<EffectLedgerEntry> m_Entries;
    };

} // namespace Wheatear::WAO
