#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ProgressionSaveLoadPageService {

    void EnsureLayout(Scene* scene, bool saveMode, int pendingOverwriteSlot, const std::string& saveDirectory);

} // namespace Wheatear::ProgressionSaveLoadPageService
