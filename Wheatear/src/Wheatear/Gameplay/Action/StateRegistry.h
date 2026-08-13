#pragma once

#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear::WAO {

    namespace StateIds {
        inline constexpr const char* Guard = "guard";
        inline constexpr const char* Regeneration = "regen";
        inline constexpr const char* Burn = "burn";
        inline constexpr const char* DefenseDown = "def_down";
        inline constexpr const char* Stun = "stun";
    }

    enum class StateTickEvent
    {
        None = 0,
        Heal,
        Damage
    };

    struct StateDefinition
    {
        const char* Id = "";
        const char* DisplayName = "";
        bool Harmful = false;
        float DefaultPower = 0.0f;
    };

    struct StateTickResult
    {
        float HealthDelta = 0.0f;
        bool Killed = false;
        std::vector<StateTickEvent> Events;
    };

    WHEATEAR_API const StateDefinition* FindStateDefinition(const std::string& id);
    // Snapshot of all registered state ids; used by the editor to drive state
    // pickers and referential validation without exposing the static vector.
    WHEATEAR_API std::vector<StateDefinition> AllStateDefinitions();
    WHEATEAR_API RuntimeState MakeState(const std::string& id,
        int turns,
        float power,
        UUID source = 0);

    WHEATEAR_API bool HasState(const std::vector<RuntimeState>& states, const std::string& id);
    WHEATEAR_API void ApplyState(std::vector<RuntimeState>& states, const RuntimeState& state);
    WHEATEAR_API void RemoveState(std::vector<RuntimeState>& states, const std::string& id);
    WHEATEAR_API std::string FormatStates(const std::vector<RuntimeState>& states);

    WHEATEAR_API float CalculateDefenseMultiplier(const std::vector<RuntimeState>& states);
    WHEATEAR_API float CalculateDamageTakenMultiplier(const std::vector<RuntimeState>& states,
        bool guarding,
        float guardingMultiplier);

    WHEATEAR_API StateTickResult TickTurnStates(std::vector<RuntimeState>& states,
        float& health,
        float maxHealth);

} // namespace Wheatear::WAO
