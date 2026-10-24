#include "wepch.h"
#include "WAOActionEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "WAOActionEditorPanelInternal.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    using namespace WAOActionEditorInternal;


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

        if (m_EditMode && !ImGui::GetIO().WantTextInput)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                RedoRecipe();
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                UndoRecipe();
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
                RedoRecipe();
        }

        const bool wasOpen = m_Open;
        if (!EditorFloatingWindow::Begin("WAO Action Editor", &m_Open, 0, { 1220.0f, 760.0f }))
        {
            EditorFloatingWindow::End();
            if (wasOpen && !m_Open)
            {
                if (m_EditDirty)
                {
                    m_Open = true;
                    m_ClosePrompt = true;
                }
                else if (m_ActionSetsDirty)
                {
                    m_Open = true;
                    m_SaveStatus = "Save action set changes before closing.";
                }
            }
            DrawClosePrompt();
            return;
        }

        if (m_EditMode
            && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::GetIO().KeyCtrl
            && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (SaveEditedRecipe())
            {
                const std::string savedId = m_EditRecipe.Id;
                const bool reloaded = ReloadActionSources();
                m_SelectedActionId = savedId;
                m_EditMode = false;
                m_EditDirty = false;
                m_SavedRecipeFingerprint = RecipeFingerprint(m_EditRecipe);
                m_SaveStatus = reloaded
                    ? "Saved and reloaded " + savedId
                    : "Saved " + savedId + ", but no YAML recipes were reloaded.";
            }
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
        if (ImGui::Button(EditorLocale::Text("Manage Action Sets", "管理动作集")))
            m_ShowActionSetsPanel = !m_ShowActionSetsPanel;
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

        if (m_ShowActionSetsPanel)
        {
            ImGui::Separator();
            DrawActionSetsPanel();
        }

        EditorFloatingWindow::End();

        if (wasOpen && !m_Open)
        {
            if (m_EditDirty)
            {
                m_Open = true;
                m_ClosePrompt = true;
            }
            else if (m_ActionSetsDirty)
            {
                m_Open = true;
                m_SaveStatus = "Save action set changes before closing.";
            }
        }
        DrawClosePrompt();
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
        if (m_EditMode && m_EditDirty && m_SelectedActionId != m_EditingActionId)
        {
            m_SelectedActionId = m_EditingActionId;
            m_SaveStatus = "Save or cancel the current recipe before switching.";
        }

        const bool hasRecipe = FindSelectedRecipe(m_SelectedActionId) != nullptr;

        if (ImGui::BeginTabBar("##WAOActionTabs"))
        {
            if (hasRecipe && ImGui::BeginTabItem(EditorLocale::Text("Recipe", "配方")))
            {
                DrawRecipeOverview();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem(EditorLocale::Text("Effects", "效果")))
            {
                DrawEffectsTable();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem(EditorLocale::Text("Validation", "校验")))
            {
                DrawValidationPanel();
                ImGui::EndTabItem();
            }
            if (hasRecipe && ImGui::BeginTabItem(EditorLocale::Text("Preview", "预览")))
            {
                DrawPreviewPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Ledger", "运行记录")))
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
            if (m_EditDirty)
            {
                // Keep the edit buffer; do not silently discard unsaved changes
                // when the list selection changes.
                m_SelectedActionId = m_EditingActionId;
                m_SaveStatus = "Save or cancel the current recipe before switching.";
                return;
            }
            m_EditMode = false;
            m_EditDirty = false;
            m_EditingActionId.clear();
        }

        if (m_EditMode)
        {
            DrawRecipeEditor();
            return;
        }

        if (ImGui::Button(EditorLocale::Text("Edit Recipe", "编辑配方")))
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

        SectionHeader("Behavior");
        LabelValue("Behavior Id", recipe->BehaviorId);
        EditorWidgets::DrawLabeledPathTools("Behavior File", recipe->BehaviorFile);
        LabelValue("Behavior Entry", recipe->BehaviorEntry);
        LabelValue("Behavior Version", static_cast<float>(recipe->BehaviorVersion));

        SectionHeader("Resources");
        if (recipe->ResourceCost.empty())
            ImGui::TextDisabled(EditorLocale::Text("No resource cost.", "无资源消耗。"));
        else
        {
            for (const auto& [id, cost] : recipe->ResourceCost)
                LabelValue(id.c_str(), cost);
        }

        SectionHeader("Params");
        if (recipe->Params.empty())
            ImGui::TextDisabled(EditorLocale::Text("No recipe params.", "无配方参数。"));
        else
        {
            for (const auto& [id, value] : recipe->Params)
                LabelValue(id.c_str(), value);
        }

        SectionHeader("Tags");
        if (recipe->Tags.empty())
            ImGui::TextDisabled(EditorLocale::Text("No tags.", "无标签。"));
        else
        {
            for (const std::string& tag : recipe->Tags)
            {
                ImGui::BulletText("%s", tag.c_str());
            }
        }

        SectionHeader("Signals");
        if (recipe->Signals.empty())
            ImGui::TextDisabled(EditorLocale::Text("No signals.", "无信号。"));
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
            if (ImGui::Button(EditorLocale::Text("Rename", "重命名")))
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
            if (ImGui::Button(EditorLocale::Text("Cancel", "取消")))
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
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_EffectsStructureChanged = false;

        m_EffectOrigins.clear();
        m_NextEffectId = 1;
        for (WAO::EffectSpec& effect : m_EditRecipe.Effects)
            effect.EditorId = m_NextEffectId++;

        const std::string relativePath = RecipeSourcePath(recipe.Id);
        if (!relativePath.empty())
        {
            const std::filesystem::path path =
                EditorWidgets::ResolveProjectAsset(relativePath);
            if (std::filesystem::is_regular_file(path))
            {
                try
                {
                    YAML::Node root = YAML::LoadFile(path.string());
                    YAML::Node action = root;
                    if (root["actions"] && root["actions"].IsSequence())
                    {
                        for (YAML::Node candidate : root["actions"])
                        {
                            if (candidate["id"]
                                && candidate["id"].as<std::string>() == recipe.Id)
                            {
                                action = candidate;
                                break;
                            }
                        }
                    }
                    const YAML::Node effects = action["effects"];
                    if (effects && effects.IsSequence())
                    {
                        for (size_t i = 0; i < effects.size()
                            && i < m_EditRecipe.Effects.size(); ++i)
                        {
                            m_EffectOrigins[m_EditRecipe.Effects[i].EditorId] =
                                YAML::Dump(effects[i]);
                        }
                    }
                }
                catch (const YAML::Exception&)
                {
                    // Missing/invalid source falls back to clean effect nodes.
                }
            }
        }

        m_LastRecipeFingerprint = RecipeFingerprint(recipe);
        m_SavedRecipeFingerprint = m_LastRecipeFingerprint;
    }




    void WAOActionEditorPanel::DrawRecipeEditor()
    {
        if (m_EditingActionId.empty())
        {
            m_EditMode = false;
            return;
        }

        const WAO::ActionRecipe previousRecipe = m_EditRecipe;
        const int previousEffect = m_SelectedEffectIndex;

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

        SectionHeader("Behavior");
        changed |= EditorWidgets::InputString("Behavior Id", m_EditRecipe.BehaviorId);
        changed |= EditorWidgets::DrawAssetReferenceField("Behavior File",
            m_EditRecipe.BehaviorFile,
            EditorWidgets::AssetReferenceKind::Any);
        changed |= EditorWidgets::InputString("Behavior Entry", m_EditRecipe.BehaviorEntry);
        int behaviorVersion = static_cast<int>(m_EditRecipe.BehaviorVersion);
        if (ImGui::DragInt("Behavior Version", &behaviorVersion, 1.0f, 0, 100000))
        {
            m_EditRecipe.BehaviorVersion = static_cast<uint32_t>(std::max(0, behaviorVersion));
            changed = true;
        }

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
        ImGui::BeginDisabled(m_UndoStack.empty());
        if (ImGui::Button(EditorLocale::Text("Undo", "撤销")))
            UndoRecipe();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_RedoStack.empty());
        if (ImGui::Button(EditorLocale::Text("Redo", "重做")))
            RedoRecipe();
        ImGui::EndDisabled();
        ImGui::SameLine();
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
                m_SavedRecipeFingerprint = RecipeFingerprint(m_EditRecipe);
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

        if (m_EditMode && !m_SuppressUndoCapture)
        {
            const std::string current = RecipeFingerprint(m_EditRecipe);
            if (current != m_LastRecipeFingerprint)
            {
                PushRecipeUndo(previousRecipe, previousEffect);
                m_LastRecipeFingerprint = current;
                m_EditDirty = true;
            }
        }
    }

    std::string WAOActionEditorPanel::RecipeFingerprint(
        const WAO::ActionRecipe& recipe) const
    {
        return YAML::Dump(RecipeToYaml(recipe));
    }

    void WAOActionEditorPanel::PushRecipeUndo(const WAO::ActionRecipe& recipe,
        int selectedEffect)
    {
        m_UndoStack.push_back({ recipe, selectedEffect });
        if (m_UndoStack.size() > 64)
            m_UndoStack.erase(m_UndoStack.begin());
        m_RedoStack.clear();
    }

    void WAOActionEditorPanel::UndoRecipe()
    {
        if (m_UndoStack.empty())
            return;

        RecipeSnapshot snapshot = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        m_RedoStack.push_back({ m_EditRecipe, m_SelectedEffectIndex });
        m_SuppressUndoCapture = true;
        m_EditRecipe = std::move(snapshot.Recipe);
        m_SelectedEffectIndex = snapshot.SelectedEffectIndex;
        m_LastRecipeFingerprint = RecipeFingerprint(m_EditRecipe);
        m_SuppressUndoCapture = false;
        m_EditDirty = m_LastRecipeFingerprint != m_SavedRecipeFingerprint;
        m_SaveStatus = "Undo.";
    }

    void WAOActionEditorPanel::RedoRecipe()
    {
        if (m_RedoStack.empty())
            return;

        RecipeSnapshot snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        m_UndoStack.push_back({ m_EditRecipe, m_SelectedEffectIndex });
        m_SuppressUndoCapture = true;
        m_EditRecipe = std::move(snapshot.Recipe);
        m_SelectedEffectIndex = snapshot.SelectedEffectIndex;
        m_LastRecipeFingerprint = RecipeFingerprint(m_EditRecipe);
        m_SuppressUndoCapture = false;
        m_EditDirty = m_LastRecipeFingerprint != m_SavedRecipeFingerprint;
        m_SaveStatus = "Redo.";
    }

    void WAOActionEditorPanel::DrawClosePrompt()
    {
        if (!m_ClosePrompt)
            return;

        const char* popupId = "Unsaved WAO Recipe##WAOUnsaved";
        if (!ImGui::IsPopupOpen(popupId))
            ImGui::OpenPopup(popupId);

        if (ImGui::BeginPopupModal(popupId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(EditorLocale::Text(
                "This recipe has unsaved changes.",
                "当前配方有未保存的修改。"));
            ImGui::Spacing();
            if (ImGui::Button(EditorLocale::Text("Save", "保存")))
            {
                if (SaveEditedRecipe())
                {
                    const std::string savedId = m_EditRecipe.Id;
                    const bool reloaded = ReloadActionSources();
                    m_SelectedActionId = savedId;
                    m_EditMode = false;
                    m_EditDirty = false;
                    m_SavedRecipeFingerprint = RecipeFingerprint(m_EditRecipe);
                    m_ClosePrompt = false;
                    m_Open = false;
                    if (!reloaded)
                        m_SaveStatus = "Saved " + savedId + ", but no YAML recipes were reloaded.";
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Discard", "放弃")))
            {
                m_EditMode = false;
                m_EditDirty = false;
                m_ClosePrompt = false;
                m_Open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Cancel", "取消")))
            {
                m_ClosePrompt = false;
                ImGui::CloseCurrentPopup();
            }
            if (!m_SaveStatus.empty())
            {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", m_SaveStatus.c_str());
            }
            ImGui::EndPopup();
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
                if (EditorWidgets::InputString("##key", key, 128) && key != originalKey)
                {
                    if (key.empty())
                    {
                        m_SaveStatus = "Param key cannot be empty.";
                        key = originalKey;
                    }
                    else if (m_EditRecipe.Params.count(key) > 0 && key != originalKey)
                    {
                        m_SaveStatus = "Param '" + key + "' already exists.";
                        key = originalKey;
                    }
                    else
                    {
                        m_EditRecipe.Params.erase(originalKey);
                        m_EditRecipe.Params[key] = value;
                        changed = true;
                    }
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                if (EditorWidgets::InputString("##value", value, 256))
                {
                    if (!key.empty())
                        m_EditRecipe.Params[key] = value;
                    changed = true;
                }

                ImGui::TableSetColumnIndex(2);
                if (DrawStringChoice("Key", key, CommonParamKeys()) && !key.empty()
                    && key != originalKey
                    && m_EditRecipe.Params.count(key) == 0)
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
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        CollectValidationMessages(m_EditRecipe, errors, warnings);
        (void)warnings;
        if (!errors.empty())
        {
            m_SaveStatus = "Validation failed: " + errors.front();
            return false;
        }

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
                        written = WriteEditableRecipeFields(action, m_EditRecipe, &m_EffectOrigins);
                        break;
                    }
                }
            }
            else if (root["id"] && root["id"].as<std::string>() == m_EditRecipe.Id)
            {
                written = WriteEditableRecipeFields(root, m_EditRecipe, &m_EffectOrigins);
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
