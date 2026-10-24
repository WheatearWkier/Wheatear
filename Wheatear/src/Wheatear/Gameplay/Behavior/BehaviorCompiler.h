#pragma once

#include "BehaviorTypes.h"
#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>

namespace YAML {
    class Node;
}

namespace Wheatear::WAO {

    class WHEATEAR_API BehaviorCompiler
    {
    public:
        static bool Compile(const YAML::Node& root,
            const std::filesystem::path& sourcePath,
            BehaviorProgram* program,
            std::string* error = nullptr);
    };

} // namespace Wheatear::WAO
