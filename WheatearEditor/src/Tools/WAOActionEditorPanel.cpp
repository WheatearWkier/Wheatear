#include "wepch.h"
#include "WAOActionEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

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

namespace Wheatear {

    namespace {

        const char* EffectTypeName(WAO::EffectType type)
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

        const char* EffectTypeYamlName(WAO::EffectType type)
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

        const char* DurationPolicyName(WAO::EffectDurationPolicy policy)
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

        const char* DurationPolicyYamlName(WAO::EffectDurationPolicy policy)
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

        const std::vector<WAO::EffectType>& EditableEffectTypes()
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

        const std::vector<WAO::EffectDurationPolicy>& EditableDurationPolicies()
        {
            static const std::vector<WAO::EffectDurationPolicy> policies = {
                WAO::EffectDurationPolicy::Instant,
                WAO::EffectDurationPolicy::Seconds,
                WAO::EffectDurationPolicy::Turns,
                WAO::EffectDurationPolicy::Infinite
            };
            return policies;
        }

        const std::vector<std::string>& CommonParamKeys()
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

        std::vector<std::string> CommonParamValues(const std::string& key)
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

        bool DrawStringChoice(const char* buttonLabel,
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

        WAO::EffectSpec MakeEffectTemplate(WAO::EffectType type)
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
        const std::unordered_map<std::string, float>& KnownAttributes()
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

        bool IsKnownAttribute(const std::string& id)
        {
            return !id.empty() && KnownAttributes().count(id) > 0;
        }

        std::string ActionModuleKey(const WAO::ActionRecipe& recipe)
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

        std::string ReadYamlString(const YAML::Node& node, const char* key, const std::string& fallback = {})
        {
            const YAML::Node value = node[key];
            return value ? value.as<std::string>(fallback) : fallback;
        }

        std::vector<ActionSetDefinition> LoadActionSets()
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

        std::filesystem::path ActionSetsManifestPath()
        {
            return AssetPath::Resolve(
                AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
        }

        std::vector<ActionSetDefinition>& MutableActionSets()
        {
            static std::vector<ActionSetDefinition> sets = LoadActionSets();
            return sets;
        }

        const std::vector<ActionSetDefinition>& ActionSets()
        {
            return MutableActionSets();
        }

        YAML::Node ActionSetsToYaml(const std::vector<ActionSetDefinition>& sets)
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

        void ReloadActionSetDefinitions()
        {
            MutableActionSets() = LoadActionSets();
        }

        std::string ActionModuleLabel(const std::string& key)
        {
            for (const ActionSetDefinition& set : ActionSets())
            {
                if (set.Key == key)
                    return set.Label;
            }
            return "Misc";
        }

        const ActionSetDefinition* FindActionSetByKey(const std::string& key)
        {
            for (const ActionSetDefinition& set : ActionSets())
            {
                if (set.Key == key)
                    return &set;
            }
            return nullptr;
        }

        std::string FirstActionSetKey()
        {
            return ActionSets().empty() ? std::string{} : ActionSets().front().Key;
        }

        std::string RecipeSourcePath(const std::string& actionId)
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

        std::string TuningSourcePath(const std::string& actionId)
        {
            if (actionId.rfind("side.", 0) == 0)
                return "side.tuning";
            return {};
        }

        static std::string NormalizeAssetReference(std::string value)
        {
            std::replace(value.begin(), value.end(), '\\', '/');
            return value;
        }

        std::string AuthoringPath(const std::string& value)
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

        bool MatchesFilter(const WAO::ActionRecipe& recipe, const char* filter)
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
        bool IsIdBoundaryChar(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return !(std::isalnum(u) || c == '.');
        }

        bool ReplaceDottedIdInFile(const std::filesystem::path& path,
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

        bool IsReferenceTextFile(const std::filesystem::path& path)
        {
            const std::string ext = path.extension().string();
            static const std::set<std::string> textExts = {
                ".yaml", ".yml", ".json", ".wt", ".wts", ".vn", ".txt", ".md",
                ".lua", ".cs", ".ini", ".cfg", ".h", ".hpp", ".cpp", ".cc", ".inl"
            };
            return textExts.count(ext) > 0;
        }

        bool ShouldSkipReferencePath(const std::filesystem::path& path)
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

        size_t ReplaceDottedIdInProject(const std::string& oldId,
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

        std::vector<WAO::ActionRecipe> SortedActions()
        {
            return WAO::RecipesWithPrefix("");
        }

        bool RecipeIdExists(const std::string& actionId)
        {
            const auto actions = SortedActions();
            return std::find_if(actions.begin(), actions.end(), [&](const WAO::ActionRecipe& recipe)
            {
                return recipe.Id == actionId;
            }) != actions.end();
        }

        std::string ActionIdSuffix(const std::string& actionId)
        {
            const size_t split = actionId.find('.');
            if (split == std::string::npos || split + 1 >= actionId.size())
                return actionId.empty() ? "new_action" : actionId;
            return actionId.substr(split + 1);
        }

        std::string MakeUniqueActionId(const std::string& setKey, const std::string& baseSuffix)
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

        const WAO::ActionRecipe* FindSelectedRecipe(const std::string& id)
        {
            return id.empty() ? nullptr : WAO::FindRecipeOrWarn(id, "WAOActionEditorPanel");
        }

        std::string JoinResourceCost(const std::unordered_map<std::string, float>& values)
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

        std::unordered_map<std::string, float> ParseResourceCost(const std::string& text)
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

        std::string JoinParams(const std::unordered_map<std::string, std::string>& values)
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

        std::unordered_map<std::string, std::string> ParseParams(const std::string& text)
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

        void WriteStringSequence(YAML::Node node, const char* key, const std::vector<std::string>& values)
        {
            YAML::Node sequence(YAML::NodeType::Sequence);
            for (const std::string& value : values)
            {
                if (!value.empty())
                    sequence.push_back(value);
            }
            node[key] = sequence;
        }

        void WriteResourceCost(YAML::Node node, const std::unordered_map<std::string, float>& values)
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

        void WriteParams(YAML::Node node, const std::unordered_map<std::string, std::string>& values)
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

        void WriteEffects(YAML::Node node, const std::vector<WAO::EffectSpec>& effects)
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

        bool WriteEditableRecipeFields(YAML::Node node, const WAO::ActionRecipe& recipe)
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

        YAML::Node RecipeToYaml(const WAO::ActionRecipe& recipe)
        {
            YAML::Node node(YAML::NodeType::Map);
            WriteEditableRecipeFields(node, recipe);
            return node;
        }

        void LabelValue(const char* label, const std::string& value)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(150.0f);
            ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
        }

        void LabelValue(const char* label, float value)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(150.0f);
            ImGui::Text("%.3f", value);
        }

        std::string FormatDuration(const WAO::EffectSpec& effect)
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

        std::string EffectTargetText(const WAO::EffectSpec& effect)
        {
            if (!effect.AttributeId.empty())
                return effect.AttributeId;
            if (!effect.StateId.empty())
                return effect.StateId;
            if (!effect.SignalId.empty())
                return effect.SignalId;
            return "-";
        }

        void SectionHeader(const char* label)
        {
            EditorWidgets::SectionHeader(label);
        }

    } // namespace

    void WAOActionEditorPanel::Open(const EditorToolContext&)
    {
        m_Open = true;
        if (m_NewActionSetKey.empty())
            m_NewActionSetKey = FirstActionSetKey();
        if (m_SelectedActionId.empty())
        {
            const auto actions = SortedActions();
            if (!actions.empty())
                m_SelectedActionId = actions.front().Id;
        }
    }

    void WAOActionEditorPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (m_NewActionSetKey.empty())
            m_NewActionSetKey = FirstActionSetKey();

        if (!EditorFloatingWindow::Begin("WAO Action Editor", &m_Open, 0, { 1220.0f, 760.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }

        const auto actions = SortedActions();
        EditorWidgets::PanelHeader(
            EditorLocale::Text("WAO Action Editor", "WAO 动作编辑器"),
            EditorLocale::Text("Author and inspect reusable gameplay action recipes, effects, validation, and runtime ledger entries.", "编辑可复用 Gameplay 动作、效果、校验和运行时记录。"));
        EditorWidgets::StatusBadge((std::to_string(actions.size()) + " recipes").c_str(), EditorWidgets::StatusKind::Info);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Clear Ledger", "清空记录")))
        {
            WAO::ActionDebugHistory::Clear();
            m_SelectedRecordSequence = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Reload YAML", "重载 YAML")))
            ReloadActionSources();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::BeginCombo("Action Set", m_NewActionSetKey.empty() ? "(none)" : ActionModuleLabel(m_NewActionSetKey).c_str()))
        {
            for (const ActionSetDefinition& set : ActionSets())
            {
                const bool selected = set.Key == m_NewActionSetKey;
                const std::string label = EditorWidgets::LabelWithId(set.Label, "wao_action_set:" + set.Key);
                if (ImGui::Selectable(label.c_str(), selected))
                    m_NewActionSetKey = set.Key;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("New Action", "新建动作")))
            CreateRecipeInSet(m_NewActionSetKey.empty() ? FirstActionSetKey() : m_NewActionSetKey);
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedActionId.empty());
        if (ImGui::Button(EditorLocale::Text("Duplicate", "复制")))
            DuplicateSelectedRecipe();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Delete", "删除")))
            DeleteSelectedRecipe();
        ImGui::EndDisabled();
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton("WAO Action Editor");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_EditDirty,
            true,
            m_SelectedActionId.empty() ? std::string{} : RecipeSourcePath(m_SelectedActionId),
            m_SaveStatus.empty() ? "Runtime recipes update as gameplay starts actions." : m_SaveStatus
        });

        ImGui::Separator();
        const float leftWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.30f);
        ImGui::BeginChild("##wao_action_list", ImVec2(leftWidth, 0.0f), true);
        DrawActionList();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##wao_action_detail", ImVec2(0.0f, 0.0f), true);
        DrawActionDetails();
        ImGui::EndChild();

        EditorFloatingWindow::End();
    }

    void WAOActionEditorPanel::DrawActionList()
    {
        EditorWidgets::SectionHeader("Recipes", "Filter by id, display name, description, or tag.");
        ImGui::SetNextItemWidth(-1.0f);
        EditorWidgets::SearchBar("##WAOFilter", m_Filter, sizeof(m_Filter), "Filter id / tag / text");
        ImGui::Checkbox("Group by module", &m_GroupByModule);
        ImGui::Separator();

        const std::vector<WAO::ActionRecipe> actions = SortedActions();
        size_t visibleRecipeIndex = 0;
        auto drawRecipe = [this, &visibleRecipeIndex](const WAO::ActionRecipe& recipe)
        {
            if (!MatchesFilter(recipe, m_Filter))
                return;

            const bool selected = recipe.Id == m_SelectedActionId;
            const std::string label = EditorWidgets::LabelWithId(
                recipe.Id.empty() ? "(unnamed)" : recipe.Id,
                "wao_recipe:" + std::to_string(visibleRecipeIndex++));
            if (ImGui::Selectable(label.c_str(), selected))
                m_SelectedActionId = recipe.Id;

            if (!recipe.DisplayName.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", recipe.DisplayName.c_str());
            }
        };

        if (!m_GroupByModule)
        {
            for (const WAO::ActionRecipe& recipe : actions)
                drawRecipe(recipe);
            return;
        }

        std::map<std::string, std::vector<WAO::ActionRecipe>> byModule;
        for (const WAO::ActionRecipe& recipe : actions)
        {
            if (MatchesFilter(recipe, m_Filter))
                byModule[ActionModuleKey(recipe)].push_back(recipe);
        }

        for (const auto& [module, recipes] : byModule)
        {
            const std::string header = ActionModuleLabel(module) + " (" + std::to_string(recipes.size()) + ")";
            if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                continue;

            ImGui::Indent();
            for (const WAO::ActionRecipe& recipe : recipes)
                drawRecipe(recipe);
            ImGui::Unindent();
        }
    }

    void WAOActionEditorPanel::DrawActionDetails()
    {
        const bool hasRecipe = FindSelectedRecipe(m_SelectedActionId) != nullptr;

        if (ImGui::BeginTabBar("##WAOActionTabs"))
        {
            if (hasRecipe && ImGui::BeginTabItem("Recipe"))
            {
                DrawRecipeOverview();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem("Effects"))
            {
                DrawEffectsTable();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem("Validation"))
            {
                DrawValidationPanel();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem("Preview"))
            {
                DrawPreviewPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Action Sets"))
            {
                DrawActionSetsPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ledger"))
            {
                DrawDebugLedger();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (!hasRecipe)
            EditorWidgets::EmptyState("Select an action recipe.", "Choose a recipe on the left, or use Action Sets to manage YAML recipe files.");
    }

    void WAOActionEditorPanel::DrawRecipeOverview()
    {
        const WAO::ActionRecipe* recipe = FindSelectedRecipe(m_SelectedActionId);
        if (!recipe)
            return;

        if (m_EditMode && m_EditingActionId != recipe->Id)
        {
            m_EditMode = false;
            m_EditDirty = false;
            m_EditingActionId.clear();
        }

        if (m_EditMode)
        {
            DrawRecipeEditor();
            return;
        }

        if (ImGui::Button("Edit Recipe"))
            BeginEdit(*recipe);
        ImGui::SameLine();
        ImGui::TextDisabled("Edits common authoring fields and saves back to YAML.");
        if (!m_SaveStatus.empty())
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", m_SaveStatus.c_str());
        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Id");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", recipe->Id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Rename..."))
        {
            m_RenameNewId = recipe->Id;
            m_RenameStatus.clear();
            m_RenameOpen = true;
        }
        EditorWidgets::HelpTooltip("Renames this action id and updates dotted references project-wide. Colon command forms (side:xxx) are left for manual review.");
        LabelValue("Name", recipe->DisplayName);
        LabelValue("Description", recipe->Description);
        LabelValue("Module", ActionModuleLabel(ActionModuleKey(*recipe)));

        SectionHeader("Authoring");
        EditorWidgets::DrawLabeledPathTools("Recipe YAML", RecipeSourcePath(recipe->Id));
        EditorWidgets::DrawLabeledPathTools("Tuning", TuningSourcePath(recipe->Id));
        EditorWidgets::DrawLabeledPathTools("Icon", recipe->IconPath);
        LabelValue("Animation", recipe->AnimationId);
        EditorWidgets::DrawLabeledPathTools("SFX", recipe->SoundPath);
        EditorWidgets::DrawLabeledPathTools("VFX", recipe->EffectPath);

        ImGui::Separator();
        LabelValue("Cooldown", recipe->Cooldown);
        LabelValue("Duration", recipe->Duration);
        LabelValue("Startup", recipe->Startup);
        LabelValue("Hit Time", recipe->HitTime);
        LabelValue("Recovery", recipe->Recovery);
        LabelValue("Cancel Start", recipe->CancelStart);
        LabelValue("Cancel End", recipe->CancelEnd);
        LabelValue("Move Scale", recipe->MovementScale);

        SectionHeader("Resources");
        if (recipe->ResourceCost.empty())
            ImGui::TextDisabled("No resource cost.");
        else
        {
            for (const auto& [id, cost] : recipe->ResourceCost)
                LabelValue(id.c_str(), cost);
        }

        SectionHeader("Params");
        if (recipe->Params.empty())
            ImGui::TextDisabled("No recipe params.");
        else
        {
            for (const auto& [id, value] : recipe->Params)
                LabelValue(id.c_str(), value);
        }

        SectionHeader("Tags");
        if (recipe->Tags.empty())
            ImGui::TextDisabled("No tags.");
        else
        {
            for (const std::string& tag : recipe->Tags)
            {
                ImGui::BulletText("%s", tag.c_str());
            }
        }

        SectionHeader("Signals");
        if (recipe->Signals.empty())
            ImGui::TextDisabled("No signals.");
        else
        {
            for (const std::string& signal : recipe->Signals)
                ImGui::BulletText("%s", signal.c_str());
        }

        DrawRenameDialog(*recipe);
    }

    void WAOActionEditorPanel::DrawRenameDialog(const WAO::ActionRecipe& recipe)
    {
        if (!m_RenameOpen)
            return;

        ImGui::OpenPopup("Rename Action Id");
        ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Rename Action Id", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Renaming '%s' to a new dotted id will rewrite the recipe YAML "
                "and update project-wide dotted references. Colon command forms (e.g. "
                "side:launcher in scenes / SideCombatSystem.cpp) are NOT auto-rewritten - "
                "review those manually.", recipe.Id.c_str());

            ImGui::Separator();
            ImGui::TextDisabled("New Id");
            if (EditorWidgets::InputString("##RenameNewId", m_RenameNewId, 128))
                m_RenameStatus.clear();

            if (!m_RenameStatus.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "%s", m_RenameStatus.c_str());

            const bool newIdValid = !m_RenameNewId.empty()
                && m_RenameNewId != recipe.Id
                && !RecipeIdExists(m_RenameNewId)
                && m_RenameNewId.find(' ') == std::string::npos;

            ImGui::Separator();
            const bool canConfirm = newIdValid;
            if (!canConfirm)
                ImGui::BeginDisabled();
            if (ImGui::Button("Rename"))
            {
                if (PerformRename(recipe.Id, m_RenameNewId))
                {
                    m_RenameOpen = false;
                    m_RenameNewId.clear();
                    m_RenameStatus.clear();
                }
                // On failure keep the modal open; m_RenameStatus shows the reason.
            }
            if (!canConfirm)
                ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_RenameOpen = false;
                m_RenameNewId.clear();
                m_RenameStatus.clear();
            }
            ImGui::EndPopup();
        }
    }

    bool WAOActionEditorPanel::PerformRename(const std::string& oldId, const std::string& newId)
    {
        const std::string relativePath = RecipeSourcePath(oldId);
        if (relativePath.empty())
        {
            m_RenameStatus = "No YAML source mapping for " + oldId;
            return false;
        }
        if (RecipeIdExists(newId))
        {
            m_RenameStatus = "Id already exists: " + newId;
            return false;
        }
        // Guard: oldId must not be a prefix of another recipe id, otherwise the
        // token-aware replacement would still rewrite the shared prefix.
        for (const WAO::ActionRecipe& other : WAO::ActionDatabase::All())
        {
            if (other.Id != oldId && other.Id.size() > oldId.size()
                && other.Id.compare(0, oldId.size(), oldId) == 0)
            {
                m_RenameStatus = "Cannot rename: '" + oldId + "' is a prefix of '" + other.Id + "'.";
                return false;
            }
        }

        const std::filesystem::path path = EditorWidgets::ResolveProjectAsset(relativePath);
        if (path.empty() || !std::filesystem::is_regular_file(path))
        {
            m_RenameStatus = "Missing YAML source: " + relativePath;
            return false;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(path.string());
            bool found = false;
            YAML::Node actions = root["actions"];
            if (actions && actions.IsSequence())
            {
                for (YAML::Node action : actions)
                {
                    if (action && action["id"] && action["id"].as<std::string>() == oldId)
                    {
                        action["id"] = newId;
                        found = true;
                        break;
                    }
                }
            }
            else if (root["id"] && root["id"].as<std::string>() == oldId)
            {
                root["id"] = newId;
                found = true;
            }

            if (!found)
            {
                m_RenameStatus = "Recipe not found in " + relativePath;
                return false;
            }

            if (!EditorWidgets::WriteFileText(path, std::string(YAML::Dump(root))))
            {
                m_RenameStatus = "Failed to write " + path.string();
                return false;
            }
        }
        catch (const YAML::Exception& exception)
        {
            m_RenameStatus = std::string("YAML error: ") + exception.what();
            return false;
        }
        catch (const std::exception& exception)
        {
            m_RenameStatus = std::string("Rename failed: ") + exception.what();
            return false;
        }

        // Token-aware project-wide dotted replacement, skipping the recipe file
        // we just rewrote so its own id field is not double-processed.
        const size_t replaced = ReplaceDottedIdInProject(oldId, newId, path);

        ReloadActionSources();
        m_SelectedActionId = newId;
        m_EditMode = false;
        m_EditDirty = false;
        m_EditingActionId.clear();
        m_SaveStatus = "Renamed " + oldId + " -> " + newId + " (" + std::to_string(replaced) + " reference(s) updated).";
        return true;
    }

    void WAOActionEditorPanel::BeginEdit(const WAO::ActionRecipe& recipe)
    {
        m_EditRecipe = recipe;
        m_EditingActionId = recipe.Id;
        m_EditMode = true;
        m_EditDirty = false;
        m_SelectedEffectIndex = recipe.Effects.empty() ? -1 : 0;
        m_SaveStatus.clear();
    }

    void WAOActionEditorPanel::CreateRecipeInSet(const std::string& setKey, const WAO::ActionRecipe* sourceRecipe)
    {
        const ActionSetDefinition* set = FindActionSetByKey(setKey);
        if (!set)
        {
            m_SaveStatus = "Choose an action set before creating a recipe.";
            return;
        }

        WAO::ActionRecipe recipe = sourceRecipe ? *sourceRecipe : WAO::ActionRecipe{};
        recipe.Id = MakeUniqueActionId(set->Key, sourceRecipe ? ActionIdSuffix(sourceRecipe->Id) + "_copy" : "new_action");

        if (sourceRecipe)
        {
            recipe.DisplayName = sourceRecipe->DisplayName.empty()
                ? recipe.Id
                : sourceRecipe->DisplayName + " Copy";
        }
        else
        {
            recipe.DisplayName = "New Action";
            recipe.Description = "New WAO action recipe.";
            recipe.MovementScale = 1.0f;
            recipe.Tags = { set->Key };
        }

        const std::filesystem::path path = EditorWidgets::ResolveProjectAsset(set->Path);
        if (path.empty())
        {
            m_SaveStatus = "Missing YAML source path for action set " + set->Key;
            return;
        }

        try
        {
            YAML::Node root(YAML::NodeType::Map);
            if (std::filesystem::is_regular_file(path))
                root = YAML::LoadFile(path.string());

            YAML::Node actions = root["actions"];
            if (!actions || !actions.IsSequence())
            {
                YAML::Node sequence(YAML::NodeType::Sequence);
                if (root["id"])
                    sequence.push_back(YAML::Clone(root));

                root = YAML::Node(YAML::NodeType::Map);
                root["actions"] = sequence;
                actions = root["actions"];
            }

            actions.push_back(RecipeToYaml(recipe));

            if (!path.parent_path().empty())
                std::filesystem::create_directories(path.parent_path());

            if (!EditorWidgets::WriteFileText(path, std::string(YAML::Dump(root))))
            {
                m_SaveStatus = "Failed to write " + path.string();
                return;
            }

            ReloadActionSources();
            m_SelectedActionId = recipe.Id;
            BeginEdit(recipe);
            m_EditDirty = false;
            m_NewActionSetKey = set->Key;
            m_SaveStatus = "Created " + recipe.Id + ".";
        }
        catch (const YAML::Exception& exception)
        {
            m_SaveStatus = std::string("YAML error: ") + exception.what();
        }
        catch (const std::exception& exception)
        {
            m_SaveStatus = std::string("Create failed: ") + exception.what();
        }
    }

    void WAOActionEditorPanel::DuplicateSelectedRecipe()
    {
        const WAO::ActionRecipe* recipe = FindSelectedRecipe(m_SelectedActionId);
        if (!recipe)
        {
            m_SaveStatus = "Select a recipe to duplicate.";
            return;
        }

        const std::string targetSet = m_NewActionSetKey.empty()
            ? ActionModuleKey(*recipe)
            : m_NewActionSetKey;
        CreateRecipeInSet(targetSet, recipe);
    }

    bool WAOActionEditorPanel::DeleteSelectedRecipe()
    {
        if (m_SelectedActionId.empty())
        {
            m_SaveStatus = "Select a recipe to delete.";
            return false;
        }

        const std::string deletedId = m_SelectedActionId;
        const std::string relativePath = RecipeSourcePath(deletedId);
        if (relativePath.empty())
        {
            m_SaveStatus = "No YAML source mapping for " + deletedId;
            return false;
        }

        const std::filesystem::path path = EditorWidgets::ResolveProjectAsset(relativePath);
        if (path.empty() || !std::filesystem::is_regular_file(path))
        {
            m_SaveStatus = "Missing YAML source: " + relativePath;
            return false;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(path.string());
            bool removed = false;

            YAML::Node actions = root["actions"];
            if (actions && actions.IsSequence())
            {
                for (size_t i = 0; i < actions.size(); ++i)
                {
                    YAML::Node action = actions[i];
                    if (action && action["id"] && action["id"].as<std::string>() == deletedId)
                    {
                        actions.remove(i);
                        removed = true;
                        break;
                    }
                }
            }
            else if (root["id"] && root["id"].as<std::string>() == deletedId)
            {
                root = YAML::Node(YAML::NodeType::Map);
                root["actions"] = YAML::Node(YAML::NodeType::Sequence);
                removed = true;
            }

            if (!removed)
            {
                m_SaveStatus = "Recipe was not found in " + relativePath;
                return false;
            }

            if (!EditorWidgets::WriteFileText(path, std::string(YAML::Dump(root))))
            {
                m_SaveStatus = "Failed to write " + path.string();
                return false;
            }

            ReloadActionSources();
            const auto actionsAfterReload = SortedActions();
            m_SelectedActionId = actionsAfterReload.empty() ? std::string{} : actionsAfterReload.front().Id;
            if (m_EditingActionId == deletedId)
            {
                m_EditMode = false;
                m_EditDirty = false;
                m_EditingActionId.clear();
            }
            m_SaveStatus = "Deleted " + deletedId + ".";
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            m_SaveStatus = std::string("YAML error: ") + exception.what();
            return false;
        }
        catch (const std::exception& exception)
        {
            m_SaveStatus = std::string("Delete failed: ") + exception.what();
            return false;
        }
    }

    void WAOActionEditorPanel::DrawRecipeEditor()
    {
        if (m_EditingActionId.empty())
        {
            m_EditMode = false;
            return;
        }

        ImGui::TextDisabled("Editing");
        ImGui::SameLine(150.0f);
        ImGui::TextUnformatted(m_EditingActionId.c_str());

        bool changed = false;
        changed |= EditorWidgets::InputString("Name", m_EditRecipe.DisplayName);
        changed |= EditorWidgets::InputMultilineString("Description", m_EditRecipe.Description, ImVec2(0.0f, 76.0f), 1536);
        changed |= EditorWidgets::DrawAssetReferenceField("Icon",
            m_EditRecipe.IconPath,
            EditorWidgets::AssetReferenceKind::Texture);
        changed |= EditorWidgets::InputString("Animation Id", m_EditRecipe.AnimationId);
        changed |= EditorWidgets::DrawAssetReferenceField("SFX",
            m_EditRecipe.SoundPath,
            EditorWidgets::AssetReferenceKind::Audio);
        changed |= EditorWidgets::DrawAssetReferenceField("VFX",
            m_EditRecipe.EffectPath,
            EditorWidgets::AssetReferenceKind::Any);

        SectionHeader("Timing");
        changed |= ImGui::DragFloat("Cooldown", &m_EditRecipe.Cooldown, 0.01f, 0.0f, 60.0f, "%.3f");
        changed |= ImGui::DragFloat("Duration", &m_EditRecipe.Duration, 0.01f, 0.0f, 60.0f, "%.3f");
        changed |= ImGui::DragFloat("Startup", &m_EditRecipe.Startup, 0.005f, 0.0f, 10.0f, "%.3f");
        changed |= ImGui::DragFloat("Hit Time", &m_EditRecipe.HitTime, 0.005f, 0.0f, 10.0f, "%.3f");
        changed |= ImGui::DragFloat("Recovery", &m_EditRecipe.Recovery, 0.005f, 0.0f, 10.0f, "%.3f");
        changed |= ImGui::DragFloat("Cancel Start", &m_EditRecipe.CancelStart, 0.005f, 0.0f, 10.0f, "%.3f");
        changed |= ImGui::DragFloat("Cancel End", &m_EditRecipe.CancelEnd, 0.005f, 0.0f, 10.0f, "%.3f");
        changed |= ImGui::DragFloat("Move Scale", &m_EditRecipe.MovementScale, 0.01f, -4.0f, 4.0f, "%.3f");

        SectionHeader("Lists");
        std::string tags = EditorWidgets::JoinList(m_EditRecipe.Tags);
        if (EditorWidgets::InputString("Tags", tags))
        {
            m_EditRecipe.Tags = EditorWidgets::SplitList(tags);
            changed = true;
        }

        std::string signals = EditorWidgets::JoinList(m_EditRecipe.Signals);
        if (EditorWidgets::InputString("Signals", signals))
        {
            m_EditRecipe.Signals = EditorWidgets::SplitList(signals);
            changed = true;
        }

        std::string requiredStates = EditorWidgets::JoinList(m_EditRecipe.RequiredStates);
        if (EditorWidgets::InputString("Required States", requiredStates))
        {
            m_EditRecipe.RequiredStates = EditorWidgets::SplitList(requiredStates);
            changed = true;
        }

        std::string blockedStates = EditorWidgets::JoinList(m_EditRecipe.BlockedStates);
        if (EditorWidgets::InputString("Blocked States", blockedStates))
        {
            m_EditRecipe.BlockedStates = EditorWidgets::SplitList(blockedStates);
            changed = true;
        }

        std::string requiredTags = EditorWidgets::JoinList(m_EditRecipe.RequiredTags);
        if (EditorWidgets::InputString("Required Tags", requiredTags))
        {
            m_EditRecipe.RequiredTags = EditorWidgets::SplitList(requiredTags);
            changed = true;
        }

        std::string blockedTags = EditorWidgets::JoinList(m_EditRecipe.BlockedTags);
        if (EditorWidgets::InputString("Blocked Tags", blockedTags))
        {
            m_EditRecipe.BlockedTags = EditorWidgets::SplitList(blockedTags);
            changed = true;
        }

        std::string resourceCost = JoinResourceCost(m_EditRecipe.ResourceCost);
        if (EditorWidgets::InputString("Resource Cost", resourceCost))
        {
            m_EditRecipe.ResourceCost = ParseResourceCost(resourceCost);
            changed = true;
        }
        ImGui::TextDisabled("Resource format: mana=12, sword=1");

        changed |= DrawParamsEditor();

        DrawEffectEditor();

        if (changed)
            m_EditDirty = true;

        ImGui::Separator();
        bool cancelClicked = false;
        if (EditorWidgets::DirtySaveBar(m_EditDirty, m_SaveStatus, "Save YAML", "Cancel", &cancelClicked))
        {
            if (SaveEditedRecipe())
            {
                const std::string savedId = m_EditRecipe.Id;
                const bool reloaded = ReloadActionSources();
                m_SelectedActionId = savedId;
                m_EditMode = false;
                m_EditDirty = false;
                m_SaveStatus = reloaded
                    ? "Saved and reloaded " + savedId
                    : "Saved " + savedId + ", but no YAML recipes were reloaded.";
            }
        }
        if (cancelClicked)
        {
            m_EditMode = false;
            m_EditDirty = false;
            m_SaveStatus = "Edit cancelled.";
        }
    }

    bool WAOActionEditorPanel::DrawParamsEditor()
    {
        bool changed = false;
        SectionHeader("Params");
        ImGui::TextDisabled("Typed key/value parameters used by action resolvers.");

        std::vector<std::pair<std::string, std::string>> params(m_EditRecipe.Params.begin(), m_EditRecipe.Params.end());
        std::sort(params.begin(), params.end(), [](const auto& left, const auto& right)
        {
            return left.first < right.first;
        });

        std::string removeKey;
        if (ImGui::BeginTable("##WAOParamsEditor", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Pick", ImGuiTableColumnFlags_WidthFixed, 82.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableHeadersRow();

            for (const auto& [originalKey, originalValue] : params)
            {
                ImGui::PushID(originalKey.c_str());
                std::string key = originalKey;
                std::string value = originalValue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(-1.0f);
                if (EditorWidgets::InputString("##key", key, 128) && !key.empty() && key != originalKey)
                {
                    m_EditRecipe.Params.erase(originalKey);
                    m_EditRecipe.Params[key] = value;
                    changed = true;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                if (EditorWidgets::InputString("##value", value, 256))
                {
                    m_EditRecipe.Params[key] = value;
                    changed = true;
                }

                ImGui::TableSetColumnIndex(2);
                if (DrawStringChoice("Key", key, CommonParamKeys()) && !key.empty())
                {
                    m_EditRecipe.Params.erase(originalKey);
                    m_EditRecipe.Params[key] = value;
                    changed = true;
                }
                ImGui::SameLine();
                if (DrawStringChoice("Value", value, CommonParamValues(key)))
                {
                    m_EditRecipe.Params[key] = value;
                    changed = true;
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("Remove"))
                    removeKey = key;

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (!removeKey.empty())
        {
            m_EditRecipe.Params.erase(removeKey);
            changed = true;
        }

        if (ImGui::SmallButton("Add Param"))
        {
            std::string key = "targetRule";
            for (int i = 2; m_EditRecipe.Params.find(key) != m_EditRecipe.Params.end(); ++i)
                key = "param" + std::to_string(i);
            m_EditRecipe.Params[key] = key == "targetRule" ? "EnemySingle" : "";
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Target Rule"))
        {
            m_EditRecipe.Params["targetRule"] = "EnemySingle";
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Category"))
        {
            m_EditRecipe.Params["category"] = "Skill";
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Magic Flag"))
        {
            m_EditRecipe.Params["magic"] = "true";
            changed = true;
        }

        return changed;
    }

    bool WAOActionEditorPanel::SaveEditedRecipe()
    {
        const std::string relativePath = RecipeSourcePath(m_EditRecipe.Id);
        if (relativePath.empty())
        {
            m_SaveStatus = "No YAML source mapping for " + m_EditRecipe.Id;
            return false;
        }

        const std::filesystem::path path = EditorWidgets::ResolveProjectAsset(relativePath);
        if (path.empty() || !std::filesystem::is_regular_file(path))
        {
            m_SaveStatus = "Missing YAML source: " + relativePath;
            return false;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(path.string());
            bool written = false;

            YAML::Node actions = root["actions"];
            if (actions && actions.IsSequence())
            {
                for (YAML::Node action : actions)
                {
                    if (!action || !action["id"])
                        continue;

                    if (action["id"].as<std::string>() == m_EditRecipe.Id)
                    {
                        written = WriteEditableRecipeFields(action, m_EditRecipe);
                        break;
                    }
                }
            }
            else if (root["id"] && root["id"].as<std::string>() == m_EditRecipe.Id)
            {
                written = WriteEditableRecipeFields(root, m_EditRecipe);
            }

            if (!written)
            {
                m_SaveStatus = "Recipe was not found in " + relativePath;
                return false;
            }

            if (!EditorWidgets::WriteFileText(path, std::string(YAML::Dump(root))))
            {
                m_SaveStatus = "Failed to write " + path.string();
                return false;
            }
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            m_SaveStatus = std::string("YAML error: ") + exception.what();
            return false;
        }
        catch (const std::exception& exception)
        {
            m_SaveStatus = std::string("Save failed: ") + exception.what();
            return false;
        }
    }

    void WAOActionEditorPanel::DrawEffectEditor()
    {
        SectionHeader("Effects");

        if (ImGui::Button("Add Damage"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::Damage));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Heal"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::Heal));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add State"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::AddState));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Signal"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::EmitSignal));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("More"))
            ImGui::OpenPopup("##WAOEffectTemplatePopup");
        if (ImGui::BeginPopup("##WAOEffectTemplatePopup"))
        {
            for (WAO::EffectType type : EditableEffectTypes())
            {
                if (ImGui::Selectable(EffectTypeName(type)))
                {
                    m_EditRecipe.Effects.push_back(MakeEffectTemplate(type));
                    m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
                    m_EditDirty = true;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        const bool canRemove = m_SelectedEffectIndex >= 0 && m_SelectedEffectIndex < static_cast<int>(m_EditRecipe.Effects.size());
        if (!canRemove)
            ImGui::BeginDisabled();
        if (ImGui::Button("Remove Selected") && canRemove)
        {
            m_EditRecipe.Effects.erase(m_EditRecipe.Effects.begin() + m_SelectedEffectIndex);
            m_SelectedEffectIndex = m_EditRecipe.Effects.empty()
                ? -1
                : std::min(m_SelectedEffectIndex, static_cast<int>(m_EditRecipe.Effects.size()) - 1);
            m_EditDirty = true;
        }
        if (!canRemove)
            ImGui::EndDisabled();

        if (m_EditRecipe.Effects.empty())
        {
            ImGui::TextDisabled("No effects. Add one to make this action drive gameplay.");
            return;
        }

        ImGui::BeginChild("##WAOEffectList", ImVec2(0.0f, 118.0f), true);
        for (int i = 0; i < static_cast<int>(m_EditRecipe.Effects.size()); ++i)
        {
            const WAO::EffectSpec& effect = m_EditRecipe.Effects[static_cast<size_t>(i)];
            std::string label = std::to_string(i + 1) + ". " + EffectTypeName(effect.Type) + " -> " + EffectTargetText(effect);
            label = EditorWidgets::LabelWithId(label, "wao_effect:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), i == m_SelectedEffectIndex))
                m_SelectedEffectIndex = i;
        }
        ImGui::EndChild();

        if (m_SelectedEffectIndex < 0 || m_SelectedEffectIndex >= static_cast<int>(m_EditRecipe.Effects.size()))
            return;

        WAO::EffectSpec& effect = m_EditRecipe.Effects[static_cast<size_t>(m_SelectedEffectIndex)];
        bool changed = false;

        if (ImGui::BeginCombo("Effect Type", EffectTypeName(effect.Type)))
        {
            for (WAO::EffectType type : EditableEffectTypes())
            {
                const bool selected = effect.Type == type;
                if (ImGui::Selectable(EffectTypeName(type), selected))
                {
                    if (effect.Type != type)
                    {
                        effect = MakeEffectTemplate(type);
                        changed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool usesAttribute =
            effect.Type == WAO::EffectType::Damage
            || effect.Type == WAO::EffectType::Heal
            || effect.Type == WAO::EffectType::ModifyAttribute
            || effect.Type == WAO::EffectType::ConsumeResource;
        const bool usesState =
            effect.Type == WAO::EffectType::AddState
            || effect.Type == WAO::EffectType::RemoveState;
        const bool usesSignal = effect.Type == WAO::EffectType::EmitSignal;
        const bool usesValue =
            usesAttribute
            || effect.Type == WAO::EffectType::Launch;

        if (usesAttribute)
            changed |= EditorWidgets::InputString("Attribute / Resource Id", effect.AttributeId, 128);
        if (usesState)
            changed |= EditorWidgets::InputString("State Id", effect.StateId, 128);
        if (usesSignal)
            changed |= EditorWidgets::InputString("Signal Id", effect.SignalId, 128);
        if (usesValue)
            changed |= ImGui::DragFloat("Value", &effect.Value, 0.05f, -100000.0f, 100000.0f, "%.3f");

        int turns = effect.Turns;
        if (effect.DurationPolicy == WAO::EffectDurationPolicy::Turns && ImGui::InputInt("Turns", &turns))
        {
            effect.Turns = std::max(0, turns);
            changed = true;
        }

        if (effect.DurationPolicy == WAO::EffectDurationPolicy::Seconds)
            changed |= ImGui::DragFloat("Seconds", &effect.Seconds, 0.05f, 0.0f, 3600.0f, "%.3f");
        if (ImGui::BeginCombo("Duration Policy", DurationPolicyName(effect.DurationPolicy)))
        {
            for (WAO::EffectDurationPolicy policy : EditableDurationPolicies())
            {
                const bool selected = effect.DurationPolicy == policy;
                if (ImGui::Selectable(DurationPolicyName(policy), selected))
                {
                    effect.DurationPolicy = policy;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (changed)
            m_EditDirty = true;
    }

    void WAOActionEditorPanel::DrawEffectsTable()
    {
        const WAO::ActionRecipe* recipe = FindSelectedRecipe(m_SelectedActionId);
        if (!recipe)
            return;

        if (recipe->Effects.empty())
        {
            ImGui::TextDisabled("No gameplay effects in this recipe.");
            return;
        }

        if (ImGui::BeginTable("##WAOEffects", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Duration");
            ImGui::TableSetupColumn("Signal");
            ImGui::TableHeadersRow();

            for (const WAO::EffectSpec& effect : recipe->Effects)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(EffectTypeName(effect.Type));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(EffectTargetText(effect).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", effect.Value);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(FormatDuration(effect).c_str());
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(effect.SignalId.empty() ? "-" : effect.SignalId.c_str());
            }

            ImGui::EndTable();
        }
    }

    void WAOActionEditorPanel::DrawValidationPanel()
    {
        const WAO::ActionRecipe* runtimeRecipe = FindSelectedRecipe(m_SelectedActionId);
        const WAO::ActionRecipe* recipe = (m_EditMode && m_EditingActionId == m_SelectedActionId) ? &m_EditRecipe : runtimeRecipe;
        if (!recipe)
            return;

        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        if (recipe->Id.empty())
            errors.push_back("Action id is empty.");
        if (RecipeSourcePath(recipe->Id).empty())
            warnings.push_back("No YAML source mapping. Saving is disabled for this id prefix.");
        if (recipe->DisplayName.empty())
            warnings.push_back("Display name is empty.");
        if (recipe->Duration < 0.0f || recipe->Startup < 0.0f || recipe->Recovery < 0.0f || recipe->HitTime < 0.0f)
            errors.push_back("Timing values must not be negative.");
        if (recipe->Duration > 0.0f && recipe->HitTime > recipe->Duration)
            warnings.push_back("Hit time is later than duration.");
        if (recipe->CancelEnd > 0.0f && recipe->CancelEnd < recipe->CancelStart)
            errors.push_back("Cancel end is earlier than cancel start.");
        if (!recipe->IconPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->IconPath))
            warnings.push_back("Icon path is missing: " + recipe->IconPath);
        if (!recipe->SoundPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->SoundPath))
            warnings.push_back("SFX path is missing: " + recipe->SoundPath);
        if (!recipe->EffectPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->EffectPath))
            warnings.push_back("VFX path is missing: " + recipe->EffectPath);

        for (const auto& [id, cost] : recipe->ResourceCost)
        {
            if (id.empty())
                errors.push_back("Resource cost has an empty id.");
            if (cost < 0.0f)
                errors.push_back("Resource cost is negative: " + id);
        }

        for (const auto& [id, value] : recipe->Params)
        {
            if (id.empty())
                errors.push_back("Recipe params contain an empty key.");
            if (value.empty())
                warnings.push_back("Recipe param has an empty value: " + id);

            const std::vector<std::string> choices = CommonParamValues(id);
            if (!choices.empty() && std::find(choices.begin(), choices.end(), value) == choices.end())
                warnings.push_back("Recipe param " + id + " uses a value outside the common schema: " + value);

            if (id == "defensePierce" || id == "range" || id == "radius" || id == "knockback" || id == "hitStop" || id == "cameraShake")
            {
                try
                {
                    (void)std::stof(value);
                }
                catch (...)
                {
                    errors.push_back("Recipe param " + id + " must be numeric.");
                }
            }
        }

        for (size_t i = 0; i < recipe->Effects.size(); ++i)
        {
            const WAO::EffectSpec& effect = recipe->Effects[i];
            const std::string prefix = "Effect " + std::to_string(i + 1) + ": ";
            if (effect.Type == WAO::EffectType::None)
                errors.push_back(prefix + "type is None.");
            if ((effect.Type == WAO::EffectType::ModifyAttribute || effect.Type == WAO::EffectType::Damage || effect.Type == WAO::EffectType::Heal)
                && effect.AttributeId.empty())
                warnings.push_back(prefix + "attribute id is empty.");
            else if ((effect.Type == WAO::EffectType::ModifyAttribute || effect.Type == WAO::EffectType::Damage || effect.Type == WAO::EffectType::Heal)
                && !IsKnownAttribute(effect.AttributeId))
                warnings.push_back(prefix + "attribute id '" + effect.AttributeId + "' is not a known attribute.");
            if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && effect.StateId.empty())
                warnings.push_back(prefix + "state id is empty.");
            else if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && WAO::FindStateDefinition(effect.StateId) == nullptr)
                warnings.push_back(prefix + "state id '" + effect.StateId + "' is not in StateRegistry.");
            if (effect.Type == WAO::EffectType::EmitSignal && effect.SignalId.empty())
                warnings.push_back(prefix + "signal id is empty.");
            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Seconds && effect.Seconds <= 0.0f)
                warnings.push_back(prefix + "seconds policy needs Seconds > 0.");
            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Turns && effect.Turns <= 0)
                warnings.push_back(prefix + "turns policy needs Turns > 0.");
        }

        if (errors.empty() && warnings.empty())
        {
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f), "Validation passed.");
            return;
        }

        if (!errors.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Errors");
            for (const std::string& message : errors)
                ImGui::BulletText("%s", message.c_str());
        }

        if (!warnings.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "Warnings");
            for (const std::string& message : warnings)
                ImGui::BulletText("%s", message.c_str());
        }
    }

    void WAOActionEditorPanel::DrawPreviewPanel()
    {
        const WAO::ActionRecipe* runtimeRecipe = FindSelectedRecipe(m_SelectedActionId);
        const WAO::ActionRecipe* recipe = (m_EditMode && m_EditingActionId == m_SelectedActionId) ? &m_EditRecipe : runtimeRecipe;
        if (!recipe)
            return;

        LabelValue("Action", recipe->Id);
        LabelValue("Animation", recipe->AnimationId);
        EditorWidgets::DrawLabeledPathTools("Icon", recipe->IconPath);
        EditorWidgets::DrawLabeledPathTools("SFX", recipe->SoundPath);
        EditorWidgets::DrawLabeledPathTools("VFX", recipe->EffectPath);

        SectionHeader("Timing Preview");
        const float total = std::max({ 0.1f, recipe->Duration, recipe->Startup + recipe->Recovery, recipe->HitTime + recipe->Recovery, recipe->CancelEnd });
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = std::max(260.0f, ImGui::GetContentRegionAvail().x - 8.0f);
        const float height = 46.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 end = ImVec2(origin.x + width, origin.y + height);
        drawList->AddRectFilled(origin, end, IM_COL32(22, 28, 34, 255), 4.0f);

        auto drawSegment = [&](float start, float stop, ImU32 color)
        {
            start = std::clamp(start, 0.0f, total);
            stop = std::clamp(stop, 0.0f, total);
            if (stop <= start)
                return;
            const float x0 = origin.x + (start / total) * width;
            const float x1 = origin.x + (stop / total) * width;
            drawList->AddRectFilled(ImVec2(x0, origin.y + 8.0f), ImVec2(x1, origin.y + height - 8.0f), color, 3.0f);
        };

        drawSegment(0.0f, recipe->Startup, IM_COL32(70, 110, 180, 255));
        drawSegment(recipe->Startup, std::max(recipe->Startup, recipe->HitTime), IM_COL32(80, 160, 120, 255));
        drawSegment(std::max(recipe->HitTime, recipe->Startup), std::max(recipe->Duration, recipe->HitTime), IM_COL32(170, 125, 70, 255));
        drawSegment(recipe->CancelStart, recipe->CancelEnd, IM_COL32(190, 210, 80, 210));

        const float hitX = origin.x + (std::clamp(recipe->HitTime, 0.0f, total) / total) * width;
        drawList->AddLine(ImVec2(hitX, origin.y + 4.0f), ImVec2(hitX, origin.y + height - 4.0f), IM_COL32(255, 90, 90, 255), 2.0f);
        ImGui::Dummy(ImVec2(width, height + 4.0f));
        ImGui::TextDisabled("Blue startup, green active lead-in, orange recovery/body, yellow cancel window, red hit frame.");

        SectionHeader("Gameplay Output");
        ImGui::Text("Effects: %d", static_cast<int>(recipe->Effects.size()));
        ImGui::Text("Signals: %s", recipe->Signals.empty() ? "-" : EditorWidgets::JoinList(recipe->Signals).c_str());
        ImGui::Text("Cost: %s", recipe->ResourceCost.empty() ? "-" : JoinResourceCost(recipe->ResourceCost).c_str());
        ImGui::Text("Params: %s", recipe->Params.empty() ? "-" : JoinParams(recipe->Params).c_str());

        SectionHeader("Sandbox");
        ImGui::TextDisabled("Execute this recipe against a synthetic runtime. No Scene/Play mode required.");
        if (ImGui::Button("Run in Sandbox"))
            RunSandbox(*recipe);
        if (m_SandboxRan)
            DrawSandboxResult();
    }

    void WAOActionEditorPanel::RunSandbox(const WAO::ActionRecipe& recipe)
    {
        WAO::ActionRuntime runtime;
        // Seed a baseline so Damage/Heal/Modify land on visible numbers.
        runtime.Attributes.Set("Health", 100.0f);
        runtime.Attributes.Set("hp", 100.0f);
        runtime.Attributes.Set("atk", 10.0f);
        runtime.Attributes.Set("mana", 50.0f);
        for (const auto& [id, cost] : recipe.ResourceCost)
            runtime.Resources[id] = cost + 10.0f; // allow the cost check to pass
        runtime.Tags = recipe.RequiredTags;

        WAO::ActionIntent intent;
        intent.Actor = 1;
        intent.ActionId = recipe.Id;

        m_SandboxBefore.clear();
        for (const auto& [id, value] : runtime.Attributes.Values)
            m_SandboxBefore[id] = value;
        m_SandboxBefore["mana"] = 50.0f;

        m_SandboxResult = WAO::Execute(intent, recipe, runtime);
        m_SandboxRan = true;

        m_SandboxAfter.clear();
        for (const auto& [id, value] : runtime.Attributes.Values)
            m_SandboxAfter[id] = value;

        const bool affordable = WAO::CanAfford(recipe, runtime);
        if (!m_SandboxResult.Success)
            m_SandboxStatus = affordable ? "Execution failed (see ledger below)." : "Cannot afford resource cost.";
        else
            m_SandboxStatus = "Executed.";
    }

    void WAOActionEditorPanel::DrawSandboxResult()
    {
        ImGui::Separator();
        ImGui::TextColored(m_SandboxResult.Success ? ImVec4(0.35f, 0.90f, 0.45f, 1.0f) : ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
            "%s", m_SandboxStatus.c_str());
        if (!m_SandboxResult.Success)
            ImGui::TextWrapped("Note: sandbox uses a synthetic runtime; resolver-side logic (module handlers) is bypassed.");

        if (!m_SandboxBefore.empty())
        {
            ImGui::TextDisabled("Attribute Deltas");
            for (const auto& [id, before] : m_SandboxBefore)
            {
                const float after = m_SandboxAfter.count(id) > 0 ? m_SandboxAfter.at(id) : before;
                const float delta = after - before;
                if (std::abs(delta) > 0.0001f)
                {
                    const ImVec4 color = delta < 0.0f ? ImVec4(1.0f, 0.45f, 0.40f, 1.0f) : ImVec4(0.45f, 0.90f, 0.45f, 1.0f);
                    ImGui::TextColored(color, "  %s: %.2f -> %.2f (%+.2f)", id.c_str(), before, after, delta);
                }
            }
        }

        const auto& entries = m_SandboxResult.Ledger.Entries();
        if (!entries.empty())
        {
            ImGui::TextDisabled("Ledger");
            for (const auto& entry : entries)
            {
                std::string label = EffectTypeName(entry.Type);
                if (!entry.Detail.empty())
                    label += " - " + entry.Detail;
                ImGui::BulletText("%s%s (%.2f)", label.c_str(), entry.Applied ? "" : " [blocked]", entry.Value);
            }
        }
    }

    bool WAOActionEditorPanel::ReloadActionSources()
    {
        ReloadActionSetDefinitions();
        if (FindActionSetByKey(m_NewActionSetKey) == nullptr)
            m_NewActionSetKey = FirstActionSetKey();
        size_t loaded = WAO::ActionAssetLoader::ReloadManifest(
            AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
        if (loaded == 0)
        {
            loaded = WAO::ActionAssetLoader::ReloadDirectory(
                AssetAliasRegistry::Path("wao.action_directory", "assets/gameplay/actions"));
        }
        m_SaveStatus = "Reloaded " + std::to_string(loaded) + " YAML action recipe(s).";
        return loaded > 0;
    }

    void WAOActionEditorPanel::LoadActionSetEditor()
    {
        ReloadActionSetDefinitions();
        m_ActionSetsLoaded = true;
        m_ActionSetsDirty = false;
        if (m_SelectedActionSetKey.empty() && !ActionSets().empty())
            m_SelectedActionSetKey = ActionSets().front().Key;
        m_ActionSetsStatus = "Loaded action set manifest.";
    }

    bool WAOActionEditorPanel::SaveActionSetEditor()
    {
        const std::filesystem::path path = ActionSetsManifestPath();
        if (path.empty())
        {
            m_ActionSetsStatus = "Cannot resolve action set manifest path.";
            return false;
        }

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            m_ActionSetsStatus = "Cannot create manifest directory: " + error.message();
            return false;
        }

        YAML::Emitter out;
        out << ActionSetsToYaml(ActionSets());
        if (!out.good() || !EditorWidgets::WriteFileText(path, std::string(out.c_str())))
        {
            m_ActionSetsStatus = "Failed to write action set manifest.";
            return false;
        }

        m_ActionSetsDirty = false;
        ReloadActionSources();
        m_ActionSetsStatus = "Saved action set manifest and reloaded YAML actions.";
        return true;
    }

    void WAOActionEditorPanel::DrawActionSetsPanel()
    {
        if (!m_ActionSetsLoaded)
            LoadActionSetEditor();

        std::vector<ActionSetDefinition>& sets = MutableActionSets();
        EditorWidgets::SectionHeader("Action Set Manifest", "Manage YAML files that contain WAO action recipes.");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_ActionSetsDirty,
            true,
            AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"),
            m_ActionSetsStatus
        });

        if (ImGui::Button("Reload Sets"))
            LoadActionSetEditor();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_ActionSetsDirty);
        if (ImGui::Button("Save Sets"))
            SaveActionSetEditor();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Add Set"))
        {
            ActionSetDefinition set;
            set.Key = "new";
            for (int i = 2; FindActionSetByKey(set.Key) != nullptr; ++i)
                set.Key = "new" + std::to_string(i);
            set.Label = "New Action Set";
            set.Path = "assets/gameplay/actions/" + set.Key + "_actions.yaml";
            sets.push_back(set);
            m_SelectedActionSetKey = set.Key;
            m_ActionSetsDirty = true;
        }

        ImGui::Separator();
        if (sets.empty())
        {
            EditorWidgets::EmptyState("No action sets.", "Add a set to create a manifest entry for WAO recipe YAML.");
            return;
        }

        int selectedIndex = -1;
        for (int i = 0; i < static_cast<int>(sets.size()); ++i)
        {
            if (sets[static_cast<size_t>(i)].Key == m_SelectedActionSetKey)
            {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex < 0)
        {
            selectedIndex = 0;
            m_SelectedActionSetKey = sets.front().Key;
        }

        const float listWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.34f);
        ImGui::BeginChild("##WAOActionSetsList", ImVec2(listWidth, 0.0f), true);
        for (int i = 0; i < static_cast<int>(sets.size()); ++i)
        {
            const ActionSetDefinition& set = sets[static_cast<size_t>(i)];
            const std::string label = EditorWidgets::LabelWithId(
                set.Label.empty() ? set.Key : set.Label,
                "wao_action_set_manifest:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), i == selectedIndex))
            {
                selectedIndex = i;
                m_SelectedActionSetKey = set.Key;
            }
            ImGui::TextDisabled("%s", set.Path.c_str());
            ImGui::Spacing();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##WAOActionSetDetails", ImVec2(0.0f, 0.0f), true);
        ActionSetDefinition& set = sets[static_cast<size_t>(selectedIndex)];
        std::string key = set.Key;
        if (EditorWidgets::InputString("Key", key, 128) && !key.empty())
        {
            const bool duplicate = std::find_if(sets.begin(), sets.end(), [&](const ActionSetDefinition& current)
            {
                return &current != &set && current.Key == key;
            }) != sets.end();
            if (duplicate)
            {
                m_ActionSetsStatus = "Action set key already exists.";
            }
            else
            {
                set.Key = key;
                m_SelectedActionSetKey = key;
                m_ActionSetsDirty = true;
            }
        }

        if (EditorWidgets::InputString("Label", set.Label, 256))
            m_ActionSetsDirty = true;
        if (EditorWidgets::DrawAssetReferenceField("YAML Path", set.Path, EditorWidgets::AssetReferenceKind::Script, 512))
            m_ActionSetsDirty = true;
        if (!set.Path.empty() && !EditorWidgets::ProjectAssetExists(set.Path))
            EditorWidgets::InlineStatus("YAML path does not exist yet. New actions can create it later.", EditorWidgets::StatusKind::Warning);

        ImGui::Separator();
        ImGui::BeginDisabled(selectedIndex <= 0);
        if (ImGui::Button("Move Up"))
        {
            std::swap(sets[static_cast<size_t>(selectedIndex)], sets[static_cast<size_t>(selectedIndex - 1)]);
            --selectedIndex;
            m_ActionSetsDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(selectedIndex + 1 >= static_cast<int>(sets.size()));
        if (ImGui::Button("Move Down"))
        {
            std::swap(sets[static_cast<size_t>(selectedIndex)], sets[static_cast<size_t>(selectedIndex + 1)]);
            ++selectedIndex;
            m_ActionSetsDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Duplicate Set"))
        {
            ActionSetDefinition clone = set;
            clone.Key += "_copy";
            for (int i = 2; FindActionSetByKey(clone.Key) != nullptr; ++i)
                clone.Key = set.Key + "_copy" + std::to_string(i);
            clone.Label += " Copy";
            sets.push_back(std::move(clone));
            m_SelectedActionSetKey = sets.back().Key;
            m_ActionSetsDirty = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(sets.size() <= 1);
        if (ImGui::Button("Delete Set"))
        {
            sets.erase(sets.begin() + selectedIndex);
            selectedIndex = std::min(selectedIndex, static_cast<int>(sets.size()) - 1);
            m_SelectedActionSetKey = selectedIndex >= 0 ? sets[static_cast<size_t>(selectedIndex)].Key : std::string{};
            m_ActionSetsDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
    }

    void WAOActionEditorPanel::DrawDebugLedger()
    {
        const std::vector<WAO::ActionDebugRecord> records = WAO::ActionDebugHistory::Recent();
        if (records.empty())
        {
            ImGui::TextDisabled("No runtime WAO records yet. Enter Play mode and trigger combat actions.");
            return;
        }

        ImGui::BeginChild("##WAOLedgerList", ImVec2(0.0f, 190.0f), true);
        if (ImGui::BeginTable("##WAOLedgerTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableHeadersRow();

            for (auto it = records.rbegin(); it != records.rend(); ++it)
            {
                const WAO::ActionDebugRecord& record = *it;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool selected = record.Sequence == m_SelectedRecordSequence;
                std::string id = std::to_string(record.Sequence);
                const std::string label = EditorWidgets::LabelWithId(id, "wao_ledger:" + id);
                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                    m_SelectedRecordSequence = record.Sequence;
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(record.Intent.ActionId.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(record.Intent.Source.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(record.Success ? ImVec4(0.35f, 0.90f, 0.45f, 1.0f) : ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
                    "%s",
                    record.Success ? "OK" : "Failed");
            }

            ImGui::EndTable();
        }
        ImGui::EndChild();

        const WAO::ActionDebugRecord* selectedRecord = nullptr;
        if (m_SelectedRecordSequence == 0 && !records.empty())
            m_SelectedRecordSequence = records.back().Sequence;
        for (const WAO::ActionDebugRecord& record : records)
        {
            if (record.Sequence == m_SelectedRecordSequence)
            {
                selectedRecord = &record;
                break;
            }
        }

        if (!selectedRecord)
            return;

        SectionHeader("Record Detail");
        LabelValue("Action", selectedRecord->Intent.ActionId);
        LabelValue("Source", selectedRecord->Intent.Source);
        LabelValue("Input", selectedRecord->Intent.InputId);
        LabelValue("Detail", selectedRecord->Detail);

        if (selectedRecord->Entries.empty())
        {
            ImGui::TextDisabled("No ledger entries.");
            return;
        }

        if (ImGui::BeginTable("##WAOLedgerEntries", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Detail");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Applied");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            for (const WAO::EffectLedgerEntry& entry : selectedRecord->Entries)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(EffectTypeName(entry.Type));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.Detail.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", entry.Value);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(entry.Applied ? "Yes" : "No");
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(entry.ActionId.c_str());
            }
            ImGui::EndTable();
        }
    }

} // namespace Wheatear
