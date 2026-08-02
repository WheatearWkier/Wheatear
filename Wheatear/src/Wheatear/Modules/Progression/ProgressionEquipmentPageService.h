#pragma once

#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ProgressionEquipmentPageService {

    WHEATEAR_API int SyncPager(Scene* scene);
    WHEATEAR_API void EnsureLayout(Scene* scene);
    WHEATEAR_API void UpdateItems(Scene* scene);

} // namespace Wheatear::ProgressionEquipmentPageService
