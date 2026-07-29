#pragma once

#include "Entity.h"

#include <yaml-cpp/yaml.h>

namespace Wheatear {

    void SerializeCoreSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeCoreSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeScriptingSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeScriptingSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeModuleSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeUISceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeUISceneComponents(const YAML::Node& node, Entity entity);

    void SerializeAnimationSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeAnimationSceneComponents(const YAML::Node& node, Entity entity);

} // namespace Wheatear
