#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {
    class Scene;
}

namespace Wheatear::SideCombatHudPreset {

    WHEATEAR_API const char* DefaultPath();

    WHEATEAR_API bool Apply(SideCombatLevelComponent& level,
        const std::string& sourcePath = {});

    WHEATEAR_API int CaptureSceneLayout(SideCombatLevelComponent& level,
        Scene* scene);

    WHEATEAR_API bool Save(const SideCombatLevelComponent& level,
        const std::string& sourcePath = {},
        std::string* status = nullptr);

} // namespace Wheatear::SideCombatHudPreset
