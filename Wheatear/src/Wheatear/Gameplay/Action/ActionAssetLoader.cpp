#include "wtpch.h"
#include "ActionAssetLoader.h"

#include "ActionDatabase.h"
#include "ActionTypes.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <system_error>
#include <vector>

namespace Wheatear::WAO {

    namespace {

        std::string Lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool IsYamlFile(const std::filesystem::path& path)
        {
            const std::string extension = Lower(path.extension().generic_string());
            return extension == ".yaml" || extension == ".yml";
        }

        YAML::Node FirstNode(const YAML::Node& node, std::initializer_list<const char*> keys)
        {
            for (const char* key : keys)
            {
                YAML::Node value = node[key];
                if (value)
                    return value;
            }
            return {};
        }

        std::string ReadString(const YAML::Node& node,
            std::initializer_list<const char*> keys,
            const std::string& fallback = {})
        {
            YAML::Node value = FirstNode(node, keys);
            return value ? value.as<std::string>(fallback) : fallback;
        }

        float ReadFloat(const YAML::Node& node,
            std::initializer_list<const char*> keys,
            float fallback = 0.0f)
        {
            YAML::Node value = FirstNode(node, keys);
            return value ? value.as<float>(fallback) : fallback;
        }

        int ReadInt(const YAML::Node& node,
            std::initializer_list<const char*> keys,
            int fallback = 0)
        {
            YAML::Node value = FirstNode(node, keys);
            return value ? value.as<int>(fallback) : fallback;
        }

        std::vector<std::string> ReadStringList(const YAML::Node& node)
        {
            std::vector<std::string> values;
            if (!node)
                return values;

            if (node.IsScalar())
            {
                values.push_back(node.as<std::string>());
                return values;
            }

            if (!node.IsSequence())
                return values;

            for (const YAML::Node& item : node)
            {
                if (item)
                    values.push_back(item.as<std::string>());
            }
            return values;
        }

        EffectType ParseEffectType(const std::string& text)
        {
            const std::string value = Lower(text);
            if (value == "damage") return EffectType::Damage;
            if (value == "heal") return EffectType::Heal;
            if (value == "modifyattribute" || value == "modify_attribute") return EffectType::ModifyAttribute;
            if (value == "addstate" || value == "add_state") return EffectType::AddState;
            if (value == "removestate" || value == "remove_state") return EffectType::RemoveState;
            if (value == "startcooldown" || value == "start_cooldown") return EffectType::StartCooldown;
            if (value == "consumeresource" || value == "consume_resource") return EffectType::ConsumeResource;
            if (value == "launch") return EffectType::Launch;
            if (value == "hitstun" || value == "hit_stun") return EffectType::HitStun;
            if (value == "emitsignal" || value == "emit_signal" || value == "signal") return EffectType::EmitSignal;
            return EffectType::None;
        }

        EffectDurationPolicy ParseDurationPolicy(const std::string& text)
        {
            const std::string value = Lower(text);
            if (value == "seconds") return EffectDurationPolicy::Seconds;
            if (value == "turns") return EffectDurationPolicy::Turns;
            if (value == "infinite") return EffectDurationPolicy::Infinite;
            return EffectDurationPolicy::Instant;
        }

        void ReadResourceCost(const YAML::Node& node, ActionRecipe* recipe)
        {
            if (!node || !node.IsMap() || !recipe)
                return;

            for (const auto& entry : node)
            {
                const std::string id = entry.first.as<std::string>();
                const float value = entry.second.as<float>(0.0f);
                if (!id.empty() && value > 0.0f)
                    recipe->ResourceCost[id] = value;
            }
        }

        void ReadParams(const YAML::Node& node, ActionRecipe* recipe)
        {
            if (!node || !node.IsMap() || !recipe)
                return;

            for (const auto& entry : node)
            {
                if (!entry.first.IsScalar() || !entry.second.IsScalar())
                    continue;

                const std::string id = entry.first.as<std::string>();
                const std::string value = entry.second.as<std::string>("");
                if (!id.empty())
                    recipe->Params[id] = value;
            }
        }

        EffectSpec ReadEffect(const YAML::Node& node)
        {
            EffectSpec effect;
            effect.Type = ParseEffectType(ReadString(node, { "type", "Type" }));
            effect.AttributeId = ReadString(node, { "attribute", "attributeId", "AttributeId" });
            effect.StateId = ReadString(node, { "state", "stateId", "StateId" });
            effect.SignalId = ReadString(node, { "signal", "signalId", "SignalId" });
            effect.Value = ReadFloat(node, { "value", "Value" });
            effect.Turns = ReadInt(node, { "turns", "Turns" });
            effect.Seconds = ReadFloat(node, { "seconds", "Seconds" });
            effect.DurationPolicy = ParseDurationPolicy(ReadString(node, { "durationPolicy", "DurationPolicy" }));
            return effect;
        }

        bool ReadRecipe(const YAML::Node& node, ActionRecipe* recipe)
        {
            if (!node || !recipe)
                return false;

            ActionRecipe parsed;
            parsed.Id = ReadString(node, { "id", "Id" });
            if (parsed.Id.empty())
                return false;

            parsed.DisplayName = ReadString(node, { "displayName", "DisplayName", "name", "Name" }, parsed.Id);
            parsed.Description = ReadString(node, { "description", "Description" });
            parsed.IconPath = AssetAliasRegistry::Resolve(ReadString(node, { "icon", "iconPath", "IconPath" }));
            parsed.AnimationId = ReadString(node, { "animation", "animationId", "AnimationId" });
            parsed.SoundPath = AssetAliasRegistry::Resolve(ReadString(node, { "sound", "soundPath", "SoundPath" }));
            parsed.EffectPath = AssetAliasRegistry::Resolve(ReadString(node, { "effect", "effectPath", "EffectPath" }));
            parsed.Cooldown = ReadFloat(node, { "cooldown", "Cooldown" });
            parsed.Duration = ReadFloat(node, { "duration", "Duration" });
            parsed.Startup = ReadFloat(node, { "startup", "Startup" });
            parsed.Recovery = ReadFloat(node, { "recovery", "Recovery" });
            parsed.HitTime = ReadFloat(node, { "hitTime", "HitTime" });
            parsed.CancelStart = ReadFloat(node, { "cancelStart", "CancelStart" });
            parsed.CancelEnd = ReadFloat(node, { "cancelEnd", "CancelEnd" });
            parsed.MovementScale = ReadFloat(node, { "movementScale", "MovementScale" }, 1.0f);
            parsed.Tags = ReadStringList(FirstNode(node, { "tags", "Tags" }));
            parsed.RequiredStates = ReadStringList(FirstNode(node, { "requiredStates", "RequiredStates" }));
            parsed.BlockedStates = ReadStringList(FirstNode(node, { "blockedStates", "BlockedStates" }));
            parsed.RequiredTags = ReadStringList(FirstNode(node, { "requiredTags", "RequiredTags" }));
            parsed.BlockedTags = ReadStringList(FirstNode(node, { "blockedTags", "BlockedTags" }));
            parsed.Signals = ReadStringList(FirstNode(node, { "signals", "Signals" }));

            ReadResourceCost(FirstNode(node, { "resourceCost", "ResourceCost", "resources", "Resources" }), &parsed);
            ReadParams(FirstNode(node, { "params", "Params" }), &parsed);

            YAML::Node effects = FirstNode(node, { "effects", "Effects" });
            if (effects && effects.IsSequence())
            {
                for (const YAML::Node& effect : effects)
                    parsed.Effects.push_back(ReadEffect(effect));
            }

            *recipe = parsed;
            return true;
        }

        size_t LoadNode(const YAML::Node& root)
        {
            size_t loaded = 0;
            YAML::Node actions = root["actions"];
            if (actions && actions.IsSequence())
            {
                for (const YAML::Node& node : actions)
                {
                    ActionRecipe recipe;
                    if (ReadRecipe(node, &recipe))
                    {
                        ActionDatabase::Register(recipe);
                        ++loaded;
                    }
                }
                return loaded;
            }

            ActionRecipe recipe;
            if (ReadRecipe(root, &recipe))
            {
                ActionDatabase::Register(recipe);
                return 1;
            }

            return 0;
        }

    } // namespace

    size_t ActionAssetLoader::LoadFile(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_regular_file(resolved))
            return 0;

        try
        {
            return LoadNode(YAML::LoadFile(resolved.string()));
        }
        catch (const YAML::Exception& exception)
        {
            WT_CORE_WARN("ActionAssetLoader: failed to load '{}': {}", resolved.string(), exception.what());
            return 0;
        }
    }

    size_t ActionAssetLoader::LoadDirectory(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_directory(resolved))
            return 0;

        std::vector<std::filesystem::path> files;
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(resolved, error))
        {
            if (error || !entry.is_regular_file() || !IsYamlFile(entry.path()))
                continue;
            files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end());

        size_t loaded = 0;
        for (const std::filesystem::path& file : files)
            loaded += LoadFile(file);

        if (loaded > 0)
            WT_CORE_INFO("ActionAssetLoader: loaded {} action recipe(s) from '{}'", loaded, resolved.string());
        return loaded;
    }

    size_t ActionAssetLoader::LoadManifest(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_regular_file(resolved))
            return 0;

        try
        {
            const YAML::Node root = YAML::LoadFile(resolved.string());
            const YAML::Node sets = root["sets"];
            if (!sets || !sets.IsSequence())
                return 0;

            size_t loaded = 0;
            for (const YAML::Node& set : sets)
            {
                const std::string source = ReadString(set, { "path", "file", "directory" });
                if (source.empty())
                    continue;

                const std::filesystem::path sourcePath = AssetPath::ResolveRuntimeData(source);
                if (std::filesystem::is_directory(sourcePath))
                    loaded += LoadDirectory(source);
                else
                    loaded += LoadFile(source);
            }

            if (loaded > 0)
                WT_CORE_INFO("ActionAssetLoader: loaded {} action recipe(s) from manifest '{}'", loaded, resolved.string());
            return loaded;
        }
        catch (const YAML::Exception& exception)
        {
            WT_CORE_WARN("ActionAssetLoader: failed to load manifest '{}': {}", resolved.string(), exception.what());
            return 0;
        }
    }

    size_t ActionAssetLoader::ReloadDirectory(const std::filesystem::path& path)
    {
        ActionDatabase::Clear();
        return LoadDirectory(path);
    }

    size_t ActionAssetLoader::ReloadManifest(const std::filesystem::path& path)
    {
        ActionDatabase::Clear();
        return LoadManifest(path);
    }

} // namespace Wheatear::WAO
