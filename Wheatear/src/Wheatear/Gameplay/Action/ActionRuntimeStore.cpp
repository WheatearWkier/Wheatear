#include "wtpch.h"
#include "ActionRuntimeStore.h"

#include <algorithm>
#include <unordered_map>

namespace Wheatear::WAO {

    namespace {

        using RuntimeMap = std::unordered_map<UUID, std::shared_ptr<ActionRuntime>>;

        std::unordered_map<Scene*, RuntimeMap>& Runtimes()
        {
            static std::unordered_map<Scene*, RuntimeMap> runtimes;
            return runtimes;
        }

    } // namespace

    std::shared_ptr<ActionRuntime> ActionRuntimeStore::Find(Scene* scene, UUID actor)
    {
        if (!scene || actor == 0)
            return {};

        const auto sceneIt = Runtimes().find(scene);
        if (sceneIt == Runtimes().end())
            return {};

        const auto actorIt = sceneIt->second.find(actor);
        return actorIt == sceneIt->second.end() ? std::shared_ptr<ActionRuntime>{}
            : actorIt->second;
    }

    std::shared_ptr<ActionRuntime> ActionRuntimeStore::Get(Scene* scene, UUID actor)
    {
        if (!scene || actor == 0)
            return {};

        auto& runtime = Runtimes()[scene][actor];
        if (!runtime)
            runtime = std::make_shared<ActionRuntime>();
        return runtime;
    }

    ActionRuntime* ActionRuntimeStore::Resolve(Scene* scene, UUID actor)
    {
        const std::shared_ptr<ActionRuntime> runtime = Get(scene, actor);
        return runtime.get();
    }

    void ActionRuntimeStore::Update(Scene* scene, float deltaSeconds)
    {
        if (!scene)
            return;

        const float dt = std::max(0.0f, deltaSeconds);
        const auto sceneIt = Runtimes().find(scene);
        if (sceneIt == Runtimes().end())
            return;

        for (const auto& [actor, runtime] : sceneIt->second)
        {
            if (!runtime)
                continue;

            for (auto it = runtime->Cooldowns.begin();
                it != runtime->Cooldowns.end();)
            {
                it->second = std::max(0.0f, it->second - dt);
                if (it->second <= 0.0f)
                    it = runtime->Cooldowns.erase(it);
                else
                    ++it;
            }

            for (auto it = runtime->States.begin();
                it != runtime->States.end();)
            {
                RuntimeState& state = *it;
                if (state.Infinite)
                {
                    ++it;
                    continue;
                }

                state.RemainingSeconds = std::max(
                    0.0f,
                    state.RemainingSeconds - dt);

                // Seconds-based states expire here; turn-based states are
                // advanced by the owning module via TickTurnStates.
                if (state.RemainingSeconds <= 0.0f && state.RemainingTurns <= 0)
                    it = runtime->States.erase(it);
                else
                    ++it;
            }
        }
    }

    void ActionRuntimeStore::ClearScene(Scene* scene)
    {
        if (scene)
            Runtimes().erase(scene);
    }

    void ActionRuntimeStore::Clear()
    {
        Runtimes().clear();
    }

} // namespace Wheatear::WAO
