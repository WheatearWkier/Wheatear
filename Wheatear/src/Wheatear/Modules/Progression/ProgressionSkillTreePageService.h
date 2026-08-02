#pragma once

#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ProgressionSkillTreePageService {

    WHEATEAR_API void ResetCache();
    WHEATEAR_API bool SyncView(Scene* scene);
    WHEATEAR_API void UpdateDrag(Scene* scene);
    WHEATEAR_API void UpdateLegacyCanvas(Scene* scene);

} // namespace Wheatear::ProgressionSkillTreePageService
