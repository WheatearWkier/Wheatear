#pragma once

// Aggregate header for scene components.
//
// New components belong in a themed header under Scene/Components/ (see
// CoreComponents.h, UIComponents.h, ...). Add an include here ONLY when the
// component should be visible to every consumer of this aggregate; prefer
// including the themed header directly from code that uses one component
// family, so unrelated translation units do not recompile.
//
// Module components (VisualNovel/SideCombat/TacticalCombat/TurnCombat/
// ArcadeCombat) live in their own module headers and are intentionally NOT
// included here — include the module header or
// Wheatear/Modules/GameplayModuleComponents.h when you need them.

#include "Wheatear/Scene/Components/CoreComponents.h"
#include "Wheatear/Scene/Components/RenderingComponents.h"
#include "Wheatear/Scene/Components/CameraComponents.h"
#include "Wheatear/Scene/Components/ScriptComponents.h"
#include "Wheatear/Scene/Components/Physics2DComponents.h"
#include "Wheatear/Scene/Components/AnimationComponents.h"
#include "Wheatear/Scene/Components/UIComponents.h"
#include "Wheatear/Scene/Components/AudioComponents.h"

#include "Entity.inl"
