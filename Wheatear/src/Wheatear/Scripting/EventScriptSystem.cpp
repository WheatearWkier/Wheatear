#include "wtpch.h"
#include "EventScriptSystem.h"

#include "EventScript.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace Wheatear {

    namespace {

        struct CachedEventScript
        {
            EventScript Script;
            std::filesystem::file_time_type LastWriteTime{};
            bool Loaded = false;
        };

        static std::unordered_map<std::string, CachedEventScript>& ScriptCache()
        {
            static std::unordered_map<std::string, CachedEventScript> cache;
            return cache;
        }

        using Wheatear::StringUtils::ToLower;

        static std::vector<std::string> SplitWords(const std::string& value)
        {
            std::istringstream stream(value);
            std::vector<std::string> words;
            std::string word;
            while (stream >> word)
                words.push_back(word);
            return words;
        }

        static bool CompareInt(int left, const std::string& op, int right)
        {
            if (op == "==" || op == "=") return left == right;
            if (op == "!=") return left != right;
            if (op == ">") return left > right;
            if (op == ">=") return left >= right;
            if (op == "<") return left < right;
            if (op == "<=") return left <= right;
            return false;
        }

        static int ParseInt(const std::string& value, int fallback = 0)
        {
            try
            {
                return std::stoi(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static const EventScript* LoadScript(const std::string& scriptPath)
        {
            if (scriptPath.empty())
                return nullptr;

            const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(scriptPath);
            std::error_code error;
            const auto writeTime = std::filesystem::exists(resolved, error)
                ? std::filesystem::last_write_time(resolved, error)
                : std::filesystem::file_time_type{};

            auto& cache = ScriptCache()[scriptPath];
            if (!cache.Loaded || cache.LastWriteTime != writeTime)
            {
                cache.Script = EventScript::FromFile(resolved);
                cache.LastWriteTime = writeTime;
                cache.Loaded = true;
                WT_CORE_INFO("EventScriptSystem: reloaded '{}' ({} events)",
                    scriptPath, cache.Script.GetEvents().size());
            }

            return &cache.Script;
        }

        static bool EvaluateCondition(const std::string& expression)
        {
            std::vector<std::string> words = SplitWords(expression);
            if (words.empty())
                return false;

            bool negate = false;
            if (ToLower(words.front()) == "not")
            {
                negate = true;
                words.erase(words.begin());
            }

            if (words.empty())
                return false;

            const auto& state = GameProgress::GetState();
            const std::string kind = ToLower(words[0]);
            bool value = false;

            if (kind == "always")
            {
                value = true;
            }
            else if (kind == "never")
            {
                value = false;
            }
            else if (kind == "flag" && words.size() >= 2)
            {
                value = state.StoryFlags.count(words[1]) > 0;
            }
            else if (kind == "skill" && words.size() >= 2)
            {
                value = GameProgress::IsSkillUnlocked(words[1]);
            }
            else if ((kind == "dungeon" || kind == "dungeon_unlocked") && words.size() >= 2)
            {
                value = GameProgress::IsDungeonUnlocked(words[1]);
            }
            else if ((kind == "completed" || kind == "dungeon_completed") && words.size() >= 2)
            {
                value = state.CompletedDungeons.count(words[1]) > 0;
            }
            else if ((kind == "last_dungeon" || kind == "last_result") && words.size() >= 2)
            {
                value = state.LastDungeonResult.Valid
                    && state.LastDungeonResult.DungeonId == words[1];
            }
            else if (kind == "equipment" && words.size() >= 2)
            {
                value = GameProgress::IsEquipmentOwned(words[1]);
            }
            else if (kind == "equipped" && words.size() >= 2)
            {
                value = GameProgress::IsEquipmentEquipped(words[1]);
            }
            else if (kind == "chapter" && words.size() >= 3)
            {
                value = CompareInt(state.CurrentChapter, words[1], ParseInt(words[2]));
            }
            else if (kind == "material" && words.size() >= 2)
            {
                const int amount = GameProgress::GetMaterialAmount(words[1]);
                value = words.size() >= 4
                    ? CompareInt(amount, words[2], ParseInt(words[3]))
                    : amount > 0;
            }

            return negate ? !value : value;
        }

        static size_t FindMatchingEndIf(const std::vector<EventScriptInstruction>& instructions, size_t ifIndex)
        {
            int depth = 0;
            for (size_t i = ifIndex + 1; i < instructions.size(); ++i)
            {
                if (instructions[i].Type == EventScriptInstructionType::If)
                {
                    ++depth;
                }
                else if (instructions[i].Type == EventScriptInstructionType::EndIf)
                {
                    if (depth == 0)
                        return i + 1;
                    --depth;
                }
            }
            return instructions.size();
        }

        static bool MatchesEventTarget(entt::registry& registry,
            entt::entity entity,
            const EventCommandRequest& request)
        {
            if (static_cast<uint64_t>(request.TargetEntity) != 0)
            {
                return registry.all_of<IDComponent>(entity)
                    && registry.get<IDComponent>(entity).ID == request.TargetEntity;
            }

            return true;
        }

    } // namespace

    void EventScriptSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<EventScriptComponent>())
        {
            auto& script = registry.get<EventScriptComponent>(entity);
            script.RuntimeActive = false;
            script.RuntimeCompleted = false;
            script.RuntimeStarted = false;
            script.RuntimeEventName.clear();
            script.RuntimeInstructionIndex = 0;
            script.RuntimeWaitRemaining = 0.0f;

            if (script.Enabled && script.RunOnStart)
                StartEvent(scene, entity, script.StartEvent);
        }
    }

    void EventScriptSystem::OnRuntimeStop(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<EventScriptComponent>())
        {
            auto& script = registry.get<EventScriptComponent>(entity);
            script.RuntimeActive = false;
            script.RuntimeEventName.clear();
            script.RuntimeInstructionIndex = 0;
            script.RuntimeWaitRemaining = 0.0f;
        }
    }

    void EventScriptSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (const EventCommandRequest& request : CommandBus::DrainEventCommands(scene))
        {
            for (auto entity : registry.view<EventScriptComponent>())
            {
                if (!MatchesEventTarget(registry, entity, request))
                    continue;

                StartEvent(scene, entity, request.EventName);
            }
        }

        const float deltaSeconds = ts.GetSeconds();
        for (auto entity : registry.view<EventScriptComponent>())
            UpdateScript(scene, entity, deltaSeconds);
    }

    void EventScriptSystem::StartEvent(Scene* scene, entt::entity entity, const std::string& eventName)
    {
        if (!scene || eventName.empty())
            return;

        auto& registry = scene->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<EventScriptComponent>(entity))
            return;

        auto& component = registry.get<EventScriptComponent>(entity);
        if (!component.Enabled || (component.RunOnce && component.RuntimeCompleted))
            return;

        const EventScript* script = LoadScript(component.ScriptPath);
        const EventScriptBlock* block = script ? script->FindEvent(eventName) : nullptr;
        if (!block)
        {
            WT_CORE_WARN("EventScriptSystem: event '{}' not found in '{}'", eventName, component.ScriptPath);
            return;
        }

        component.RuntimeActive = true;
        component.RuntimeStarted = true;
        component.RuntimeEventName = eventName;
        component.RuntimeInstructionIndex = 0;
        component.RuntimeWaitRemaining = 0.0f;
    }

    void EventScriptSystem::UpdateScript(Scene* scene, entt::entity entity, float deltaSeconds)
    {
        auto& registry = scene->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<EventScriptComponent>(entity))
            return;

        auto& component = registry.get<EventScriptComponent>(entity);
        if (!component.Enabled || !component.RuntimeActive)
            return;

        if (component.RuntimeWaitRemaining > 0.0f)
        {
            component.RuntimeWaitRemaining = std::max(0.0f, component.RuntimeWaitRemaining - deltaSeconds);
            if (component.RuntimeWaitRemaining > 0.0f)
                return;
        }

        const EventScript* script = LoadScript(component.ScriptPath);
        const EventScriptBlock* block = script ? script->FindEvent(component.RuntimeEventName) : nullptr;
        if (!block)
        {
            WT_CORE_WARN("EventScriptSystem: running event '{}' no longer exists in '{}' after reload; stopped.", component.RuntimeEventName, component.ScriptPath);
            component.RuntimeActive = false;
            component.RuntimeEventName.clear();
            return;
        }

        const auto& instructions = block->Instructions;
        while (component.RuntimeInstructionIndex < instructions.size())
        {
            const EventScriptInstruction& instruction = instructions[component.RuntimeInstructionIndex];
            switch (instruction.Type)
            {
            case EventScriptInstructionType::Command:
                CommandBus::Execute(scene, instruction.Text);
                ++component.RuntimeInstructionIndex;
                break;
            case EventScriptInstructionType::Wait:
                component.RuntimeWaitRemaining = instruction.Seconds;
                ++component.RuntimeInstructionIndex;
                return;
            case EventScriptInstructionType::If:
                if (EvaluateCondition(instruction.Text))
                    ++component.RuntimeInstructionIndex;
                else
                    component.RuntimeInstructionIndex = FindMatchingEndIf(instructions, component.RuntimeInstructionIndex);
                break;
            case EventScriptInstructionType::EndIf:
                ++component.RuntimeInstructionIndex;
                break;
            }
        }

        component.RuntimeActive = false;
        component.RuntimeCompleted = true;
    }

} // namespace Wheatear
