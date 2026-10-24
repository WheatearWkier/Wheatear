#pragma once

#include "ActionRunner.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

#include <memory>

namespace Wheatear {

    class Scene;

    namespace WAO {

        // Owns the per-scene, per-actor action state used by both immediate
        // actions and suspended behavior instances.
        class WHEATEAR_API ActionRuntimeStore
        {
        public:
            static std::shared_ptr<ActionRuntime> Find(Scene* scene, UUID actor);
            static std::shared_ptr<ActionRuntime> Get(Scene* scene, UUID actor);
            static ActionRuntime* Resolve(Scene* scene, UUID actor);
            static void Update(Scene* scene, float deltaSeconds);

            static void ClearScene(Scene* scene);
            static void Clear();
        };

    } // namespace WAO

} // namespace Wheatear
