#pragma once

// Shared file-internal helpers for the WAO action editor, extracted from
// WAOActionEditorPanel.cpp so per-tab translation units can be split off.
// Inline so each TU compiles independently; callers use
// `using namespace WAOActionEditorInternal;`.

#include "Editor/EditorWidgets.h"
#include "Editor/EditorContentPickers.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <yaml-cpp/yaml.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::WAOActionEditorInternal {


        inline const char* EffectTypeName(WAO::EffectType type)
        {
            switch (type)
            {
            case WAO::EffectType::Damage: return "Damage";
            case WAO::EffectType::Heal: return "Heal";
            case WAO::EffectType::ModifyAttribute: return "Modify Attribute";
            case WAO::EffectType::AddState: return "Add State";
            case WAO::EffectType::RemoveState: return "Remove State";
            case WAO::EffectType::StartCooldown: return "Start Cooldown";
            case WAO::EffectType::ConsumeResource: return "Consume Resource";
            case WAO::EffectType::Launch: return "Launch";
            case WAO::EffectType::HitStun: return "Hit Stun";
            case WAO::EffectType::EmitSignal: return "Emit Signal";
            case WAO::EffectType::None:
            default: return "None";
            }
        }

        inline const char* EffectTypeYamlName(WAO::EffectType type)
        {
            switch (type)
            {
            case WAO::EffectType::Damage: return "damage";
            case WAO::EffectType::Heal: return "heal";
            case WAO::EffectType::ModifyAttribute: return "modify_attribute";
            case WAO::EffectType::AddState: return "add_state";
            case WAO::EffectType::RemoveState: return "remove_state";
            case WAO::EffectType::StartCooldown: return "start_cooldown";
            case WAO::EffectType::ConsumeResource: return "consume_resource";
            case WAO::EffectType::Launch: return "launch";
            case WAO::EffectType::HitStun: return "hit_stun";
            case WAO::EffectType::EmitSignal: return "emit_signal";
            case WAO::EffectType::None:
            default: return "none";
            }
        }

        inline const char* DurationPolicyName(WAO::EffectDurationPolicy policy)
        {
            switch (policy)
            {
            case WAO::EffectDurationPolicy::Seconds: return "Seconds";
            case WAO::EffectDurationPolicy::Turns: return "Turns";
            case WAO::EffectDurationPolicy::Infinite: return "Infinite";
            case WAO::EffectDurationPolicy::Instant:
            default: return "Instant";
            }
        }

        inline const char* DurationPolicyYamlName(WAO::EffectDurationPolicy policy)
        {
            switch (policy)
            {
            case WAO::EffectDurationPolicy::Seconds: return "seconds";
            case WAO::EffectDurationPolicy::Turns: return "turns";
            case WAO::EffectDurationPolicy::Infinite: return "infinite";
            case WAO::EffectDurationPolicy::Instant:
            default: return "instant";
            }
        }

        inline const std::vector<WAO::EffectType>& EditableEffectTypes()
        {
            static const std::vector<WAO::EffectType> types = {
                WAO::EffectType::Damage,
                WAO::EffectType::Heal,
                WAO::EffectType::ModifyAttribute,
                WAO::EffectType::AddState,
                WAO::EffectType::RemoveState,
                WAO::EffectType::StartCooldown,
                WAO::EffectType::ConsumeResource,
                WAO::EffectType::Launch,
                WAO::EffectType::HitStun,
                WAO::EffectType::EmitSignal
            };
            return types;
        }

        inline const std::vector<WAO::EffectDurationPolicy>& EditableDurationPolicies()
        {
            static const std::vector<WAO::EffectDurationPolicy> policies = {
                WAO::EffectDurationPolicy::Instant,
                WAO::EffectDurationPolicy::Seconds,
                WAO::EffectDurationPolicy::Turns,
                WAO::EffectDurationPolicy::Infinite
            };
            return policies;
        }

        inline const std::vector<std::string>& CommonParamKeys()
        {
            static const std::vector<std::string> keys = {
                "targetRule",
                "category",
                "magic",
                "physical",
                "defensePierce",
                "comboStep",
                "airborne",
                "guardBreak",
                "range",
                "radius",
                "knockback",
                "hitStop",
                "cameraShake",
                "sideSkillId",
                "turnSkillId",
                "tacticalCommandId"
            };
            return keys;
        }

        inline std::vector<std::string> CommonParamValues(const std::string& key)
        {
            if (key == "targetRule")
                return { "Self", "EnemySingle", "EnemyAll", "AllySingle", "AllyAll", "Point", "Area" };
            if (key == "category")
                return { "Basic", "Skill", "Ultimate", "Item", "Reaction", "System" };
            if (key == "magic" || key == "physical" || key == "airborne" || key == "guardBreak")
                return { "true", "false" };
            if (key == "comboStep")
                return { "1", "2", "3" };
            return {};
        }

        inline bool DrawStringChoice(const char* buttonLabel,
            std::string& value,
            const std::vector<std::string>& choices)
        {
            if (choices.empty())
                return false;

            bool changed = false;
            const std::string popupId = std::string("##choice_popup_") + (buttonLabel ? buttonLabel : "value");
            if (ImGui::SmallButton(buttonLabel))
                ImGui::OpenPopup(popupId.c_str());
            if (ImGui::BeginPopup(popupId.c_str()))
            {
                for (const std::string& choice : choices)
                {
                    const bool selected = value == choice;
                    if (ImGui::Selectable(choice.c_str(), selected))
                    {
                        value = choice;
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndPopup();
            }
            return changed;
        }

        inline WAO::EffectSpec MakeEffectTemplate(WAO::EffectType type)
        {
            WAO::EffectSpec effect;
            effect.Type = type;
            switch (type)
            {
            case WAO::EffectType::Damage:
                effect.AttributeId = "hp";
                effect.Value = -10.0f;
                break;
            case WAO::EffectType::Heal:
                effect.AttributeId = "hp";
                effect.Value = 10.0f;
                break;
            case WAO::EffectType::ModifyAttribute:
                effect.AttributeId = "atk";
                effect.Value = 1.0f;
                effect.DurationPolicy = WAO::EffectDurationPolicy::Turns;
                effect.Turns = 1;
                break;
            case WAO::EffectType::AddState:
                effect.StateId = WAO::StateIds::Stun;
                effect.DurationPolicy = WAO::EffectDurationPolicy::Turns;
                effect.Turns = 1;
                break;
            case WAO::EffectType::RemoveState:
                effect.StateId = WAO::StateIds::Stun;
                break;
            case WAO::EffectType::StartCooldown:
                effect.Seconds = 1.0f;
                effect.DurationPolicy = WAO::EffectDurationPolicy::Seconds;
                break;
            case WAO::EffectType::ConsumeResource:
                effect.AttributeId = "mana";
                effect.Value = 1.0f;
                break;
            case WAO::EffectType::Launch:
                effect.Value = 1.0f;
                break;
            case WAO::EffectType::HitStun:
                effect.Seconds = 0.20f;
                effect.DurationPolicy = WAO::EffectDurationPolicy::Seconds;
                break;
            case WAO::EffectType::EmitSignal:
                effect.SignalId = "action_hit";
                break;
            case WAO::EffectType::None:
            default:
                break;
            }
            return effect;
        }

        // Known attribute ids: the runner's hardcoded Health key (ActionRunner.cpp
        // uses the literal "Health" for Damage/Heal), the editor template seeds
        // (hp/atk/mana), and every attribute referenced by any recipe's effects.
        // Cached on ActionDatabase::Revision so the set refreshes on recipe reload.
        inline const std::unordered_map<std::string, float>& KnownAttributes()
        {
            static std::unordered_map<std::string, float> attributes;
            static uint64_t lastRevision = 0;
            const uint64_t revision = WAO::ActionDatabase::Revision();
            if (revision != lastRevision)
            {
                lastRevision = revision;
                attributes.clear();
                attributes["Health"] = 0.0f;
                attributes["hp"] = 0.0f;
                attributes["atk"] = 0.0f;
                attributes["mana"] = 0.0f;
                for (const WAO::ActionRecipe& recipe : WAO::ActionDatabase::All())
                {
                    for (const WAO::EffectSpec& effect : recipe.Effects)
                    {
                        if (!effect.AttributeId.empty())
                            attributes.emplace(effect.AttributeId, 0.0f);
                    }
                }
            }
            return attributes;
        }

        inline bool IsKnownAttribute(const std::string& id)
        {
            return !id.empty() && KnownAttributes().count(id) > 0;
        }

        inline std::string ActionModuleKey(const WAO::ActionRecipe& recipe)
        {
            const size_t split = recipe.Id.find('.');
            if (split != std::string::npos && split > 0)
                return recipe.Id.substr(0, split);
            return "misc";
        }

        struct ActionSetDefinition
        {
            std::string Key;
            std::string Label;
            std::string Path;
        };

        inline std::string ReadYamlString(const YAML::Node& node, const char* key, const std::string& fallback = {})
        {
            const YAML::Node value = node[key];
            return value ? value.as<std::string>(fallback) : fallback;
        }

        inline std::vector<ActionSetDefinition> LoadActionSets()
        {
            std::vector<ActionSetDefinition> sets;
            const std::filesystem::path path = AssetPath::Resolve(
                AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
            if (!std::filesystem::is_regular_file(path))
                return {};

            try
            {
                const YAML::Node root = YAML::LoadFile(path.string());
                const YAML::Node yamlSets = root["sets"];
                if (!yamlSets || !yamlSets.IsSequence())
                    return {};

                for (const YAML::Node& item : yamlSets)
                {
                    ActionSetDefinition set;
                    set.Key = ReadYamlString(item, "key");
                    set.Label = ReadYamlString(item, "label", set.Key);
                    set.Path = ReadYamlString(item, "path");
                    if (!set.Key.empty() && !set.Path.empty())
                        sets.push_back(std::move(set));
                }
            }
            catch (const YAML::Exception&)
            {
                return {};
            }

            return sets;
        }

        inline std::filesystem::path ActionSetsManifestPath()
        {
            return AssetPath::Resolve(
                AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
        }

        inline std::vector<ActionSetDefinition>& MutableActionSets()
        {
            static std::vector<ActionSetDefinition> sets = LoadActionSets();
            return sets;
        }

        inline const std::vector<ActionSetDefinition>& ActionSets()
        {
            return MutableActionSets();
        }

        inline YAML::Node ActionSetsToYaml(const std::vector<ActionSetDefinition>& sets)
        {
            YAML::Node root(YAML::NodeType::Map);
            root["version"] = 1;
            YAML::Node yamlSets(YAML::NodeType::Sequence);
            for (const ActionSetDefinition& set : sets)
            {
                YAML::Node item(YAML::NodeType::Map);
                item["key"] = set.Key;
                item["label"] = set.Label.empty() ? set.Key : set.Label;
                item["path"] = set.Path;
                yamlSets.push_back(item);
            }
            root["sets"] = yamlSets;
            return root;
        }

        inline void ReloadActionSetDefinitions()
        {
            MutableActionSets() = LoadActionSets();
        }

        inline std::string ActionModuleLabel(const std::string& key)
        {
            for (const ActionSetDefinition& set : ActionSets())
            {
                if (set.Key == key)
                    return set.Label;
            }
            return "Misc";
        }

        inline const ActionSetDefinition* FindActionSetByKey(const std::string& key)
        {
            for (const ActionSetDefinition& set : ActionSets())
            {
                if (set.Key == key)
                    return &set;
            }
            return nullptr;
        }

        inline std::string FirstActionSetKey()
        {
            return ActionSets().empty() ? std::string{} : ActionSets().front().Key;
        }

        inline std::string RecipeSourcePath(const std::string& actionId)
        {
            const size_t split = actionId.find('.');
            if (split == std::string::npos)
                return {};

            const std::string key = actionId.substr(0, split);
            for (const ActionSetDefinition& set : ActionSets())
            {
                if (set.Key == key)
                    return set.Path;
            }
            return {};
        }

        inline std::string TuningSourcePath(const std::string& actionId)
        {
            if (actionId.rfind("side.", 0) == 0)
                return "side.tuning";
            return {};
        }

        inline static std::string NormalizeAssetReference(std::string value)
        {
            std::replace(value.begin(), value.end(), '\\', '/');
            return value;
        }

        inline std::string AuthoringPath(const std::string& value)
        {
            if (value.empty())
                return {};

            const std::string normalized = NormalizeAssetReference(value);
            for (const auto& [alias, path] : AssetAliasRegistry::All())
            {
                if (NormalizeAssetReference(path) == normalized)
                    return alias;
            }
            return normalized;
        }

        inline bool MatchesFilter(const WAO::ActionRecipe& recipe, const char* filter)
        {
            if (!filter || filter[0] == '\0')
                return true;

            auto contains = [filter](const std::string& value)
            {
                return value.find(filter) != std::string::npos;
            };

            if (contains(recipe.Id) || contains(recipe.DisplayName) || contains(recipe.Description))
                return true;

            for (const std::string& tag : recipe.Tags)
            {
                if (contains(tag))
                    return true;
            }
            return false;
        }

        // Token-aware project-wide replacement for a dotted action id. A match
        // requires word boundaries on both sides so "side.basic1" never matches
        // inside "side.basic10" or "side.basic1_combo". Only the dotted canonical
        // form is rewritten; the colon command form ("side:launcher") is left for
        // manual review because SideCombatSystem.cpp hardcodes it in an if/else chain.
        inline bool IsIdBoundaryChar(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return !(std::isalnum(u) || c == '.');
        }

        inline bool ReplaceDottedIdInFile(const std::filesystem::path& path,
            const std::string& oldId,
            const std::string& newId,
            size_t& occurrences)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;

            std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            input.close();

            size_t replaced = 0;
            std::string output;
            output.reserve(text.size());
            size_t pos = 0;
            while (pos < text.size())
            {
                const size_t hit = text.find(oldId, pos);
                if (hit == std::string::npos)
                {
                    output.append(text, pos, std::string::npos);
                    break;
                }
                const size_t end = hit + oldId.size();
                const bool leftOk = (hit == 0) || IsIdBoundaryChar(text[hit - 1]);
                const bool rightOk = (end >= text.size()) || IsIdBoundaryChar(text[end]);
                if (leftOk && rightOk)
                {
                    output.append(text, pos, hit - pos);
                    output += newId;
                    pos = end;
                    ++replaced;
                }
                else
                {
                    output.append(text, pos, end - pos);
                    pos = end;
                }
            }

            if (replaced == 0)
                return false;

            if (!EditorWidgets::WriteFileText(path, output))
                return false;

            occurrences += replaced;
            return true;
        }

        inline bool IsReferenceTextFile(const std::filesystem::path& path)
        {
            const std::string ext = path.extension().string();
            static const std::set<std::string> textExts = {
                ".yaml", ".yml", ".json", ".wt", ".wts", ".vn", ".txt", ".md",
                ".lua", ".cs", ".ini", ".cfg", ".h", ".hpp", ".cpp", ".cc", ".inl"
            };
            return textExts.count(ext) > 0;
        }

        inline bool ShouldSkipReferencePath(const std::filesystem::path& path)
        {
            const std::string relative = path.generic_string();
            if (relative.find(".git") != std::string::npos)
                return true;
            if (relative.find(".vs/") != std::string::npos)
                return true;
            if (relative.find("/bin/") != std::string::npos || relative.find("/bin-int/") != std::string::npos)
                return true;
            if (relative.find("/vendor/") != std::string::npos)
                return true;
            if (relative.find("assets/.wheatear/archive") != std::string::npos)
                return true;
            return false;
        }

        inline size_t ReplaceDottedIdInProject(const std::string& oldId,
            const std::string& newId,
            const std::filesystem::path& excludedPath = {})
        {
            const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
            const std::filesystem::path workspaceRoot = projectRoot.parent_path();
            if (workspaceRoot.empty() || !std::filesystem::exists(workspaceRoot))
                return 0;

            size_t total = 0;
            try
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(workspaceRoot))
                {
                    if (!entry.is_regular_file())
                        continue;
                    const std::filesystem::path path = entry.path();
                    if (!excludedPath.empty() && path.lexically_normal() == excludedPath.lexically_normal())
                        continue;
                    if (ShouldSkipReferencePath(path))
                        continue;
                    if (!IsReferenceTextFile(path))
                        continue;
                    ReplaceDottedIdInFile(path, oldId, newId, total);
                }
            }
            catch (...)
            {
            }
            return total;
        }

        inline std::vector<WAO::ActionRecipe> SortedActions()
        {
            return WAO::RecipesWithPrefix("");
        }

        inline bool RecipeIdExists(const std::string& actionId)
        {
            const auto actions = SortedActions();
            return std::find_if(actions.begin(), actions.end(), [&](const WAO::ActionRecipe& recipe)
            {
                return recipe.Id == actionId;
            }) != actions.end();
        }

        inline std::string ActionIdSuffix(const std::string& actionId)
        {
            const size_t split = actionId.find('.');
            if (split == std::string::npos || split + 1 >= actionId.size())
                return actionId.empty() ? "new_action" : actionId;
            return actionId.substr(split + 1);
        }

        inline std::string MakeUniqueActionId(const std::string& setKey, const std::string& baseSuffix)
        {
            const std::string prefix = setKey.empty() ? "misc" : setKey;
            std::string suffix = baseSuffix.empty() ? "new_action" : baseSuffix;
            std::replace(suffix.begin(), suffix.end(), ' ', '_');

            const std::string base = prefix + "." + suffix;
            if (!RecipeIdExists(base))
                return base;

            for (int i = 2; i < 10000; ++i)
            {
                const std::string candidate = base + "_" + std::to_string(i);
                if (!RecipeIdExists(candidate))
                    return candidate;
            }
            return base + "_copy";
        }

        inline const WAO::ActionRecipe* FindSelectedRecipe(const std::string& id)
        {
            return id.empty() ? nullptr : WAO::FindRecipeOrWarn(id, "WAOActionEditorPanel");
        }

        inline std::string JoinResourceCost(const std::unordered_map<std::string, float>& values)
        {
            std::vector<std::pair<std::string, float>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << sorted[i].first << "=" << sorted[i].second;
            }
            return stream.str();
        }

        inline std::unordered_map<std::string, float> ParseResourceCost(const std::string& text)
        {
            std::unordered_map<std::string, float> costs;
            std::stringstream stream(text);
            std::string item;
            while (std::getline(stream, item, ','))
            {
                const size_t split = item.find('=');
                if (split == std::string::npos)
                    continue;

                std::string key = item.substr(0, split);
                std::string value = item.substr(split + 1);
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                if (key.empty())
                    continue;

                try
                {
                    const float cost = std::stof(value);
                    if (cost > 0.0f)
                        costs[key] = cost;
                }
                catch (...)
                {
                }
            }
            return costs;
        }

        inline std::string JoinParams(const std::unordered_map<std::string, std::string>& values)
        {
            std::vector<std::pair<std::string, std::string>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << sorted[i].first << "=" << sorted[i].second;
            }
            return stream.str();
        }

        inline std::unordered_map<std::string, std::string> ParseParams(const std::string& text)
        {
            std::unordered_map<std::string, std::string> params;
            std::stringstream stream(text);
            std::string item;
            while (std::getline(stream, item, ','))
            {
                const size_t split = item.find('=');
                if (split == std::string::npos)
                    continue;

                std::string key = item.substr(0, split);
                std::string value = item.substr(split + 1);
                const auto trim = [](std::string& textValue)
                {
                    const char* whitespace = " \t\r\n";
                    const size_t begin = textValue.find_first_not_of(whitespace);
                    if (begin == std::string::npos)
                    {
                        textValue.clear();
                        return;
                    }
                    const size_t end = textValue.find_last_not_of(whitespace);
                    textValue = textValue.substr(begin, end - begin + 1);
                };
                trim(key);
                trim(value);
                if (!key.empty())
                    params[key] = value;
            }
            return params;
        }

        inline void WriteStringSequence(YAML::Node node, const char* key, const std::vector<std::string>& values)
        {
            YAML::Node sequence(YAML::NodeType::Sequence);
            for (const std::string& value : values)
            {
                if (!value.empty())
                    sequence.push_back(value);
            }
            node[key] = sequence;
        }

        inline void WriteResourceCost(YAML::Node node, const std::unordered_map<std::string, float>& values)
        {
            YAML::Node map(YAML::NodeType::Map);
            std::vector<std::pair<std::string, float>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& [id, cost] : sorted)
            {
                if (!id.empty() && cost > 0.0f)
                    map[id] = cost;
            }
            node["resourceCost"] = map;
        }

        inline void WriteParams(YAML::Node node, const std::unordered_map<std::string, std::string>& values)
        {
            YAML::Node map(YAML::NodeType::Map);
            std::vector<std::pair<std::string, std::string>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& [id, value] : sorted)
            {
                if (!id.empty())
                    map[id] = value;
            }
            node["params"] = map;
        }

        inline void WriteEffects(YAML::Node node, const std::vector<WAO::EffectSpec>& effects)
        {
            YAML::Node sequence(YAML::NodeType::Sequence);
            for (const WAO::EffectSpec& effect : effects)
            {
                YAML::Node item(YAML::NodeType::Map);
                item["type"] = EffectTypeYamlName(effect.Type);
                if (!effect.AttributeId.empty())
                    item["attribute"] = effect.AttributeId;
                if (!effect.StateId.empty())
                    item["state"] = effect.StateId;
                if (!effect.SignalId.empty())
                    item["signal"] = effect.SignalId;
                if (effect.Value != 0.0f)
                    item["value"] = effect.Value;
                if (effect.Turns > 0)
                    item["turns"] = effect.Turns;
                if (effect.Seconds > 0.0f)
                    item["seconds"] = effect.Seconds;
                item["durationPolicy"] = DurationPolicyYamlName(effect.DurationPolicy);
                sequence.push_back(item);
            }
            node["effects"] = sequence;
        }

        inline bool WriteEditableRecipeFields(YAML::Node node, const WAO::ActionRecipe& recipe)
        {
            if (!node)
                return false;

            node["id"] = recipe.Id;
            node["displayName"] = recipe.DisplayName;
            node["description"] = recipe.Description;
            node["icon"] = AuthoringPath(recipe.IconPath);
            node["animation"] = recipe.AnimationId;
            node["sound"] = AuthoringPath(recipe.SoundPath);
            node["effect"] = AuthoringPath(recipe.EffectPath);
            node["cooldown"] = recipe.Cooldown;
            node["duration"] = recipe.Duration;
            node["startup"] = recipe.Startup;
            node["hitTime"] = recipe.HitTime;
            node["recovery"] = recipe.Recovery;
            node["cancelStart"] = recipe.CancelStart;
            node["cancelEnd"] = recipe.CancelEnd;
            node["movementScale"] = recipe.MovementScale;
            WriteStringSequence(node, "tags", recipe.Tags);
            WriteStringSequence(node, "requiredStates", recipe.RequiredStates);
            WriteStringSequence(node, "blockedStates", recipe.BlockedStates);
            WriteStringSequence(node, "requiredTags", recipe.RequiredTags);
            WriteStringSequence(node, "blockedTags", recipe.BlockedTags);
            WriteStringSequence(node, "signals", recipe.Signals);
            WriteResourceCost(node, recipe.ResourceCost);
            WriteParams(node, recipe.Params);
            WriteEffects(node, recipe.Effects);
            return true;
        }

        inline YAML::Node RecipeToYaml(const WAO::ActionRecipe& recipe)
        {
            YAML::Node node(YAML::NodeType::Map);
            WriteEditableRecipeFields(node, recipe);
            return node;
        }

        inline void LabelValue(const char* label, const std::string& value)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(150.0f);
            ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
        }

        inline void LabelValue(const char* label, float value)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(150.0f);
            ImGui::Text("%.3f", value);
        }

        inline std::string FormatDuration(const WAO::EffectSpec& effect)
        {
            if (effect.Turns > 0)
                return std::to_string(effect.Turns) + " turn(s)";
            if (effect.Seconds > 0.0f)
            {
                std::ostringstream out;
                out << effect.Seconds << " sec";
                return out.str();
            }
            return "instant";
        }

        inline std::string EffectTargetText(const WAO::EffectSpec& effect)
        {
            if (!effect.AttributeId.empty())
                return effect.AttributeId;
            if (!effect.StateId.empty())
                return effect.StateId;
            if (!effect.SignalId.empty())
                return effect.SignalId;
            return "-";
        }

        inline void SectionHeader(const char* label)
        {
            EditorWidgets::SectionHeader(label);
        }


} // namespace Wheatear::WAOActionEditorInternal
