#pragma once

#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"

#include <yaml-cpp/yaml.h>

namespace Wheatear {

    // Shared by the per-module serializer TUs (atlas frame layout round-trip).
    inline void SerializeAtlasFrame(YAML::Emitter& o,
        const char* prefix,
        const GameplayVisualService::TextureAtlasFrameSpec& atlas)
    {
        const std::string keyPrefix = prefix ? prefix : "";
        o << YAML::Key << keyPrefix + "Sheet" << YAML::Value << atlas.SheetPath;
        o << YAML::Key << keyPrefix + "CellWidth" << YAML::Value << atlas.CellWidth;
        o << YAML::Key << keyPrefix + "CellHeight" << YAML::Value << atlas.CellHeight;
        o << YAML::Key << keyPrefix + "Columns" << YAML::Value << atlas.Columns;
        o << YAML::Key << keyPrefix + "StartFrame" << YAML::Value << atlas.StartFrame;
    }

    inline void DeserializeAtlasFrame(const YAML::Node& n,
        const char* prefix,
        GameplayVisualService::TextureAtlasFrameSpec& atlas)
    {
        const std::string keyPrefix = prefix ? prefix : "";
        atlas.SheetPath = n[keyPrefix + "Sheet"].as<std::string>(atlas.SheetPath);
        atlas.CellWidth = n[keyPrefix + "CellWidth"].as<int>(atlas.CellWidth);
        atlas.CellHeight = n[keyPrefix + "CellHeight"].as<int>(atlas.CellHeight);
        atlas.Columns = n[keyPrefix + "Columns"].as<int>(atlas.Columns);
        atlas.StartFrame = n[keyPrefix + "StartFrame"].as<int>(atlas.StartFrame);
    }

    void SerializeCoreSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeCoreSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeScriptingSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeScriptingSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeModuleSceneComponents(const YAML::Node& node, Entity entity);

    // Per-module serializer entry points (SceneSerializerModuleComponents_*.cpp).
    void SerializeVNModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeVNModuleSceneComponents(const YAML::Node& node, Entity entity);
    void SerializeArcadeModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeArcadeModuleSceneComponents(const YAML::Node& node, Entity entity);
    void SerializeSideCombatModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeSideCombatModuleSceneComponents(const YAML::Node& node, Entity entity);
    void SerializeTacticalCombatModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeTacticalCombatModuleSceneComponents(const YAML::Node& node, Entity entity);
    void SerializeTurnCombatModuleSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeTurnCombatModuleSceneComponents(const YAML::Node& node, Entity entity);

    void SerializeUISceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeUISceneComponents(const YAML::Node& node, Entity entity);

    void SerializeAnimationSceneComponents(YAML::Emitter& out, Entity entity);
    void DeserializeAnimationSceneComponents(const YAML::Node& node, Entity entity);

} // namespace Wheatear
