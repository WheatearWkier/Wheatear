#include "wtpch.h"
#include "BehaviorSystem.h"

#include "BehaviorVM.h"
#include "Wheatear/Gameplay/Action/ActionRuntimeStore.h"

namespace Wheatear::WAO {

    void BehaviorSystem::OnRuntimeStop(Scene* scene)
    {
        BehaviorVM::StopScene(scene);
        ActionRuntimeStore::ClearScene(scene);
    }

    void BehaviorSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        ActionRuntimeStore::Update(scene, ts.GetSeconds());
        BehaviorVM::Update(scene, ts.GetSeconds());
    }

} // namespace Wheatear::WAO
