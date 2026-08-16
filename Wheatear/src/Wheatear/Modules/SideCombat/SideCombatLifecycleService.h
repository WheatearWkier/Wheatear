#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatLifecycleService {

    // Generates enemy entities from SideCombatLevelComponent::WaveSpawns
    // (data-driven wave table). No-op when the table is empty; when non-empty
    // the scene's statically placed enemies (except the boss) are removed and
    // the wave composition comes entirely from the table.
    WHEATEAR_API void SpawnWavesFromTable(Scene* scene, SideCombatLevelComponent& level);

    WHEATEAR_API void ResetLevelRuntime(Scene* scene, SideCombatLevelComponent& level);
    WHEATEAR_API void ResetCombatants(Scene* scene, SideCombatLevelComponent& level);

} // namespace Wheatear::SideCombatLifecycleService
