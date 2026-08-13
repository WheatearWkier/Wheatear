#include "wtpch.h"
#include "StateRegistry.h"

#include <algorithm>
#include <sstream>

namespace Wheatear::WAO {

    namespace {

        const std::vector<StateDefinition>& BuiltInStates()
        {
            static const std::vector<StateDefinition> states = {
                { StateIds::Guard, "防御", false, 0.55f },
                { StateIds::Regeneration, "再生", false, 8.0f },
                { StateIds::Burn, "燃烧", true, 6.0f },
                { StateIds::DefenseDown, "破甲", true, 0.22f },
                { StateIds::Stun, "眩晕", true, 1.0f }
            };
            return states;
        }

    } // namespace

    const StateDefinition* FindStateDefinition(const std::string& id)
    {
        const auto& states = BuiltInStates();
        const auto it = std::find_if(states.begin(),
            states.end(),
            [&](const StateDefinition& state)
            {
                return state.Id == id;
            });
        return it == states.end() ? nullptr : &(*it);
    }

    std::vector<StateDefinition> AllStateDefinitions()
    {
        return BuiltInStates();
    }

    RuntimeState MakeState(const std::string& id, int turns, float power, UUID source)
    {
        RuntimeState state;
        state.Id = id;
        state.RemainingTurns = std::max(0, turns);
        state.Power = power;
        state.Source = source;

        if (const StateDefinition* definition = FindStateDefinition(id))
        {
            state.DisplayName = definition->DisplayName;
            state.Harmful = definition->Harmful;
            if (state.Power <= 0.0f)
                state.Power = definition->DefaultPower;
        }
        else
        {
            state.DisplayName = id;
        }

        return state;
    }

    bool HasState(const std::vector<RuntimeState>& states, const std::string& id)
    {
        return std::any_of(states.begin(),
            states.end(),
            [&](const RuntimeState& state)
            {
                return state.Id == id && state.RemainingTurns > 0;
            });
    }

    void ApplyState(std::vector<RuntimeState>& states, const RuntimeState& state)
    {
        if (state.Id.empty())
            return;
        if (state.RemainingTurns <= 0 && state.RemainingSeconds <= 0.0f)
            return;

        for (RuntimeState& current : states)
        {
            if (current.Id != state.Id)
                continue;

            current.RemainingTurns = std::max(current.RemainingTurns, state.RemainingTurns);
            current.RemainingSeconds = std::max(current.RemainingSeconds, state.RemainingSeconds);
            current.Power = std::max(current.Power, state.Power);
            current.StackCount = std::max(current.StackCount, state.StackCount);
            current.Source = state.Source ? state.Source : current.Source;
            return;
        }

        states.push_back(state);
    }

    void RemoveState(std::vector<RuntimeState>& states, const std::string& id)
    {
        states.erase(
            std::remove_if(states.begin(),
                states.end(),
                [&](const RuntimeState& state)
                {
                    return state.Id == id;
                }),
            states.end());
    }

    std::string FormatStates(const std::vector<RuntimeState>& states)
    {
        std::ostringstream stream;
        bool first = true;
        for (const RuntimeState& state : states)
        {
            if (state.RemainingTurns <= 0 && state.RemainingSeconds <= 0.0f)
                continue;
            if (!first)
                stream << " ";
            stream << (state.DisplayName.empty() ? state.Id : state.DisplayName);
            if (state.RemainingTurns > 0)
                stream << state.RemainingTurns;
            first = false;
        }
        return stream.str();
    }

    float CalculateDefenseMultiplier(const std::vector<RuntimeState>& states)
    {
        float multiplier = 1.0f;
        for (const RuntimeState& state : states)
        {
            if (state.Id == StateIds::DefenseDown && state.RemainingTurns > 0)
                multiplier *= std::max(0.15f, 1.0f - std::max(0.0f, state.Power));
        }
        return multiplier;
    }

    float CalculateDamageTakenMultiplier(const std::vector<RuntimeState>& states,
        bool guarding,
        float guardingMultiplier)
    {
        float multiplier = guarding ? guardingMultiplier : 1.0f;
        for (const RuntimeState& state : states)
        {
            if (state.Id == StateIds::Guard && state.RemainingTurns > 0)
            {
                multiplier *= std::clamp(
                    state.Power > 0.0f ? state.Power : guardingMultiplier,
                    0.20f,
                    1.0f);
            }
        }
        return multiplier;
    }

    StateTickResult TickTurnStates(std::vector<RuntimeState>& states,
        float& health,
        float maxHealth)
    {
        StateTickResult result;

        for (RuntimeState& state : states)
        {
            if (state.RemainingTurns <= 0)
                continue;

            if (state.Id == StateIds::Regeneration)
            {
                const float before = health;
                health = std::min(maxHealth, health + std::max(0.0f, state.Power));
                result.HealthDelta += health - before;
                result.Events.push_back(StateTickEvent::Heal);
            }
            else if (state.Id == StateIds::Burn)
            {
                const float before = health;
                health = std::max(0.0f, health - std::max(0.0f, state.Power));
                result.HealthDelta += health - before;
                result.Events.push_back(StateTickEvent::Damage);
            }

            --state.RemainingTurns;
        }

        states.erase(
            std::remove_if(states.begin(),
                states.end(),
                [](const RuntimeState& state)
                {
                    return state.RemainingTurns <= 0 && state.RemainingSeconds <= 0.0f;
                }),
            states.end());

        result.Killed = health <= 0.0f;
        return result;
    }

} // namespace Wheatear::WAO
