#pragma once

#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ProgressionSaveLoadPageService {

    void EnsureLayout(Scene* scene, bool saveMode, int pendingOverwriteSlot);

} // namespace Wheatear::ProgressionSaveLoadPageService