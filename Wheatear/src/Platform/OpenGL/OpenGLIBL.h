#pragma once
// Platform/OpenGL/OpenGLIBL.h

#include "Wheatear/Renderer/IBLPrecompute.h"
#include <cstdint>
#include <string>

namespace Wheatear {

    class OpenGLIBL
    {
    public:
        static Ref<IBLResult> Compute(const std::string& hdrPath);
        static Ref<IBLResult> ComputeOrLoad(const std::string& hdrPath);
        static void           SaveToCache(const Ref<IBLResult>& ibl,
            const std::string& cachePath);
        static Ref<IBLResult> LoadFromCache(const std::string& cachePath);
        static uint32_t       GetBrdfLUT();

    private:
        static constexpr int k_CubemapSize = 512;
        static constexpr int k_IrradianceSize = 32;
        static constexpr int k_PrefilterSize = 128;
        static constexpr int k_PrefilterMips = 5;
        static constexpr int k_BrdfLUTSize = 512;

        static uint32_t    LoadEquirect(const std::string& path);
        static uint32_t    CreateCubemap(int size, uint32_t internalFormat,
            bool generateMips = false);
        static uint32_t    CreateBrdfLUT();
        static std::string GetCachePath(const std::string& hdrPath);


        static uint32_t s_BrdfLUT;
    };

} // namespace Wheatear
