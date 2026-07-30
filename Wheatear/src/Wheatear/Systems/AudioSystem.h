#pragma once

#include "ISystem.h"

namespace Wheatear {

    /// 
    class AudioSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
    };

} // namespace Wheatear
