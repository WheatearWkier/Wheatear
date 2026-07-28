#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Systems/ISystem.h"

#include <functional>
#include <string>

namespace Wheatear {

    class WHEATEAR_API SceneSystemRegistry
    {
    public:
        using SystemFactory = std::function<Scope<ISystem>()>;
        using SystemVisitor = std::function<void(const std::string& name, const SystemFactory& factory)>;

        static void RegisterRuntimeSystem(const std::string& name, SystemFactory factory);
        static void ClearRuntimeSystems();
        static void ForEachRuntimeSystem(const SystemVisitor& visitor);
    };

} // namespace Wheatear
