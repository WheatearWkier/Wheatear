#include "wepch.h"
#include "WAOActionEditorPanel.h"

#include "Build/PlayerPackager.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
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

        std::string ActionModuleKey(const WAO::ActionRecipe& recipe)
        {
            const size_t split = recipe.Id.find('.');
            if (split != std::string::npos && split > 0)
                return recipe.Id.substr(0, split);
            return "misc";
        }

        const char* ActionModuleLabel(const std::string& key)
        {
            if (key == "arcade") return "Arcade Combat";
            if (key == "side") return "Side Combat";
            if (key == "turn") return "Turn Combat";
            if (key == "tactical") return "Tactical Combat";
            return "Misc";
        }

        std::string RecipeSourcePath(const std::string& actionId)
        {
            if (actionId.rfind("arcade.", 0) == 0)
                return "assets/gameplay/actions/00_arcade_actions.yaml";
            if (actionId.rfind("side.", 0) == 0)
                return "assets/gameplay/actions/10_side_combat_actions.yaml";
            if (actionId.rfind("turn.", 0) == 0)
                return "assets/gameplay/actions/20_turn_combat_actions.yaml";
            if (actionId.rfind("tactical.", 0) == 0)
                return "assets/gameplay/actions/30_tactical_combat_actions.yaml";
            return {};
        }

        std::string TuningSourcePath(const std::string& actionId)
        {
            if (actionId.rfind("side.", 0) == 0)
                return "assets/vertical_slice/data/side_combat_tuning.yaml";
            return {};
        }

        std::filesystem::path ResolveProjectAsset(const std::string& relativePath)
        {
            if (relativePath.empty())
                return {};
            return AssetPath::GetProjectRoot() / std::filesystem::path(relativePath);
        }

        bool ProjectAssetExists(const std::string& relativePath)
        {
            const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
            return !resolved.empty() && std::filesystem::exists(resolved);
        }

        void CopyProjectAssetPath(const std::string& relativePath)
        {
            const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
            const std::string text = resolved.empty() ? relativePath : resolved.string();
            ImGui::SetClipboardText(text.c_str());
        }

        void OpenProjectAssetFolder(const std::string& relativePath)
        {
            const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
            if (resolved.empty())
                return;

            const std::filesystem::path directory = std::filesystem::is_directory(resolved)
                ? resolved
                : resolved.parent_path();
            PlayerPackager::OpenDirectory(directory);
        }

        void DrawPathTools(const char* id, const std::string& relativePath)
        {
            if (relativePath.empty())
            {
                ImGui::TextDisabled("-");
                return;
            }

            const bool exists = ProjectAssetExists(relativePath);
            ImGui::TextWrapped("%s", relativePath.c_str());
            ImGui::SameLine();
            ImGui::PushID(id);
            if (exists)
            {
                if (ImGui::SmallButton("Open Folder"))
                    OpenProjectAssetFolder(relativePath);
                ImGui::SameLine();
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Missing");
                ImGui::SameLine();
            }
            if (ImGui::SmallButton("Copy Path"))
                CopyProjectAssetPath(relativePath);
            ImGui::PopID();
        }

        void LabelPathTools(const char* label, const std::string& relativePath)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(150.0f);
            DrawPathTools(label, relativePath);
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

        std::vector<WAO::ActionRecipe> SortedActions()
        {
            std::vector<WAO::ActionRecipe> actions = WAO::ActionDatabase::All();
            std::sort(actions.begin(),
                actions.end(),
                [](const WAO::ActionRecipe& a, const WAO::ActionRecipe& b)
                {
                    return a.Id < b.Id;
                });
            return actions;
        }

        const WAO::ActionRecipe* FindSelectedRecipe(const std::string& id)
        {
            return id.empty() ? nullptr : WAO::ActionDatabase::Find(id);
        }

        bool EditStringField(const char* label, std::string& value, size_t capacity = 512)
        {
            std::vector<char> buffer(capacity + 1, '\0');
            std::strncpy(buffer.data(), value.c_str(), capacity);
            buffer[capacity] = '\0';
            if (!ImGui::InputText(label, buffer.data(), buffer.size()))
                return false;

            value = buffer.data();
            return true;
        }

        bool EditMultilineField(const char* label, std::string& value, const ImVec2& size, size_t capacity = 1024)
        {
            std::vector<char> buffer(capacity + 1, '\0');
            std::strncpy(buffer.data(), value.c_str(), capacity);
            buffer[capacity] = '\0';
            if (!ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size))
                return false;

            value = buffer.data();
            return true;
        }

        std::vector<std::string> SplitList(const std::string& text)
        {
            std::vector<std::string> values;
            std::string current;
            auto flush = [&]()
            {
                const size_t start = current.find_first_not_of(" \t\r\n");
                const size_t end = current.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos)
                    values.push_back(current.substr(start, end - start + 1));
                current.clear();
            };

            for (char c : text)
            {
                if (c == ',' || c == ';' || c == '|')
                    flush();
                else
                    current.push_back(c);
            }
            flush();
            return values;
        }

        std::string JoinList(const std::vector<std::string>& values)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << values[i];
            }
            return stream.str();
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
            node["icon"] = recipe.IconPath;
            node["animation"] = recipe.AnimationId;
            node["sound"] = recipe.SoundPath;
            node["effect"] = recipe.EffectPath;
            node["cooldown"] = recipe.Cooldown;
            node["duration"] = recipe.Duration;
            node["startup"] = recipe.Startup;
            node["hitTime"] = recipe.HitTime;
            node["recovery"] = recipe.Recovery;
            node["cancelStart"] = recipe.CancelStart;
            node["cancelEnd"] = recipe.CancelEnd;
            node["movementScale"] = recipe.MovementScale;
            WriteStringSequence(node, "tags", recipe.Tags);
            WriteStringSequence(node, "signals", recipe.Signals);
            WriteResourceCost(node, recipe.ResourceCost);
            WriteEffects(node, recipe.Effects);
            return true;
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
            ImGui::Separator();
            ImGui::TextDisabled("%s", label);
        }

    } // namespace

    void WAOActionEditorPanel::Open(const EditorToolContext&)
    {
        m_Open = true;
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

        if (!ImGui::Begin("WAO Action Debugger", &m_Open))
        {
            ImGui::End();
            return;
        }

        const auto actions = SortedActions();
        ImGui::Text("Actions: %d", static_cast<int>(actions.size()));
        ImGui::SameLine();
        if (ImGui::Button("Clear Ledger"))
        {
            WAO::ActionDebugHistory::Clear();
            m_SelectedRecordSequence = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload YAML"))
            ReloadActionSources();
        ImGui::SameLine();
        ImGui::TextDisabled("Runtime recipes update as gameplay starts actions.");
        if (!m_SaveStatus.empty())
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", m_SaveStatus.c_str());

        ImGui::Separator();
        const float leftWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.30f);
        ImGui::BeginChild("##wao_action_list", ImVec2(leftWidth, 0.0f), true);
        DrawActionList();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##wao_action_detail", ImVec2(0.0f, 0.0f), true);
        DrawActionDetails();
        ImGui::EndChild();

        ImGui::End();
    }

    void WAOActionEditorPanel::DrawActionList()
    {
        ImGui::InputTextWithHint("##WAOFilter", "Filter id / tag / text", m_Filter, sizeof(m_Filter));
        ImGui::Checkbox("Group by module", &m_GroupByModule);
        ImGui::Separator();

        const std::vector<WAO::ActionRecipe> actions = SortedActions();
        auto drawRecipe = [this](const WAO::ActionRecipe& recipe)
        {
            if (!MatchesFilter(recipe, m_Filter))
                return;

            const bool selected = recipe.Id == m_SelectedActionId;
            if (ImGui::Selectable(recipe.Id.c_str(), selected))
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
            const std::string header = std::string(ActionModuleLabel(module)) + " (" + std::to_string(recipes.size()) + ")";
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
        if (!FindSelectedRecipe(m_SelectedActionId))
        {
            ImGui::TextDisabled("Select an action recipe.");
            DrawDebugLedger();
            return;
        }

        if (ImGui::BeginTabBar("##WAOActionTabs"))
        {
            if (ImGui::BeginTabItem("Recipe"))
            {
                DrawRecipeOverview();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Effects"))
            {
                DrawEffectsTable();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Validation"))
            {
                DrawValidationPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Preview"))
            {
                DrawPreviewPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ledger"))
            {
                DrawDebugLedger();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
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

        LabelValue("Id", recipe->Id);
        LabelValue("Name", recipe->DisplayName);
        LabelValue("Description", recipe->Description);
        LabelValue("Module", ActionModuleLabel(ActionModuleKey(*recipe)));

        SectionHeader("Authoring");
        LabelPathTools("Recipe YAML", RecipeSourcePath(recipe->Id));
        LabelPathTools("Tuning", TuningSourcePath(recipe->Id));
        LabelPathTools("Icon", recipe->IconPath);
        LabelValue("Animation", recipe->AnimationId);
        LabelPathTools("SFX", recipe->SoundPath);
        LabelPathTools("VFX", recipe->EffectPath);

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
        changed |= EditStringField("Name", m_EditRecipe.DisplayName);
        changed |= EditMultilineField("Description", m_EditRecipe.Description, ImVec2(0.0f, 76.0f), 1536);
        changed |= EditStringField("Icon Path", m_EditRecipe.IconPath);
        changed |= EditStringField("Animation Id", m_EditRecipe.AnimationId);
        changed |= EditStringField("SFX Path", m_EditRecipe.SoundPath);
        changed |= EditStringField("VFX Path", m_EditRecipe.EffectPath);

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
        std::string tags = JoinList(m_EditRecipe.Tags);
        if (EditStringField("Tags", tags))
        {
            m_EditRecipe.Tags = SplitList(tags);
            changed = true;
        }

        std::string signals = JoinList(m_EditRecipe.Signals);
        if (EditStringField("Signals", signals))
        {
            m_EditRecipe.Signals = SplitList(signals);
            changed = true;
        }

        std::string resourceCost = JoinResourceCost(m_EditRecipe.ResourceCost);
        if (EditStringField("Resource Cost", resourceCost))
        {
            m_EditRecipe.ResourceCost = ParseResourceCost(resourceCost);
            changed = true;
        }
        ImGui::TextDisabled("Resource format: mana=12, sword=1");

        DrawEffectEditor();

        if (changed)
            m_EditDirty = true;

        ImGui::Separator();
        if (ImGui::Button("Save YAML"))
        {
            if (SaveEditedRecipe())
            {
                WAO::ActionDatabase::Register(m_EditRecipe);
                m_EditMode = false;
                m_EditDirty = false;
                m_SaveStatus = "Saved " + m_EditRecipe.Id;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_EditMode = false;
            m_EditDirty = false;
            m_SaveStatus = "Edit cancelled.";
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_EditDirty ? "Unsaved changes" : "No changes");

        if (!m_SaveStatus.empty())
            ImGui::TextWrapped("%s", m_SaveStatus.c_str());
    }

    bool WAOActionEditorPanel::SaveEditedRecipe()
    {
        const std::string relativePath = RecipeSourcePath(m_EditRecipe.Id);
        if (relativePath.empty())
        {
            m_SaveStatus = "No YAML source mapping for " + m_EditRecipe.Id;
            return false;
        }

        const std::filesystem::path path = ResolveProjectAsset(relativePath);
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

            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                m_SaveStatus = "Failed to write " + path.string();
                return false;
            }

            output << root;
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

        if (ImGui::Button("Add Effect"))
        {
            WAO::EffectSpec effect;
            effect.Type = WAO::EffectType::Damage;
            effect.AttributeId = "hp";
            effect.Value = 1.0f;
            m_EditRecipe.Effects.push_back(effect);
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
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
                    effect.Type = type;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        changed |= EditStringField("Attribute Id", effect.AttributeId, 128);
        changed |= EditStringField("State Id", effect.StateId, 128);
        changed |= EditStringField("Signal Id", effect.SignalId, 128);
        changed |= ImGui::DragFloat("Value", &effect.Value, 0.05f, -100000.0f, 100000.0f, "%.3f");

        int turns = effect.Turns;
        if (ImGui::InputInt("Turns", &turns))
        {
            effect.Turns = std::max(0, turns);
            changed = true;
        }

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
        if (!recipe->IconPath.empty() && !ProjectAssetExists(recipe->IconPath))
            warnings.push_back("Icon path is missing: " + recipe->IconPath);
        if (!recipe->SoundPath.empty() && !ProjectAssetExists(recipe->SoundPath))
            warnings.push_back("SFX path is missing: " + recipe->SoundPath);
        if (!recipe->EffectPath.empty() && !ProjectAssetExists(recipe->EffectPath))
            warnings.push_back("VFX path is missing: " + recipe->EffectPath);

        for (const auto& [id, cost] : recipe->ResourceCost)
        {
            if (id.empty())
                errors.push_back("Resource cost has an empty id.");
            if (cost < 0.0f)
                errors.push_back("Resource cost is negative: " + id);
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
            if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && effect.StateId.empty())
                warnings.push_back(prefix + "state id is empty.");
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
        LabelPathTools("Icon", recipe->IconPath);
        LabelPathTools("SFX", recipe->SoundPath);
        LabelPathTools("VFX", recipe->EffectPath);

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
        ImGui::Text("Signals: %s", recipe->Signals.empty() ? "-" : JoinList(recipe->Signals).c_str());
        ImGui::Text("Cost: %s", recipe->ResourceCost.empty() ? "-" : JoinResourceCost(recipe->ResourceCost).c_str());
    }

    bool WAOActionEditorPanel::ReloadActionSources()
    {
        const size_t loaded = WAO::ActionAssetLoader::LoadDirectory("assets/gameplay/actions");
        m_SaveStatus = "Reloaded " + std::to_string(loaded) + " YAML action recipe(s).";
        return loaded > 0;
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
                if (ImGui::Selectable(id.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
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
