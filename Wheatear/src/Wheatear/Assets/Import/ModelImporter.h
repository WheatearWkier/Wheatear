#pragma once

#include "Wheatear/Renderer/Model.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear::ModelImporter {

    enum class MaterialChannelMode
    {
        Separate = 0,
        OcclusionRoughnessMetallic,
        MetallicSmoothness
    };

    struct ImportSettings
    {
        float Scale = 1.0f;
        bool ConvertToYUp = true;
        bool FlipNormalGreen = false;
        MaterialChannelMode ChannelMode = MaterialChannelMode::Separate;
        std::string TextureSearchPath;
    };

    // Loads FBX (ufbx), glTF/GLB (cgltf) or OBJ into material-split ModelParts.
    // Returns false and fills `error` on failure; never silently substitutes a
    // cube.
    bool Load(const std::filesystem::path& path,
        std::vector<ModelPart>& outParts,
        std::string* error = nullptr,
        const ImportSettings& settings = {},
        std::vector<ModelBone>* outBones = nullptr,
        std::vector<ModelAnimation>* outAnimations = nullptr);

    // Imports a source model and writes a cooked .wtmodel next to `cookedPath`.
    bool Cook(const std::filesystem::path& sourcePath,
        const std::filesystem::path& cookedPath,
        const ImportSettings& settings,
        std::string* error = nullptr);

} // namespace Wheatear::ModelImporter
