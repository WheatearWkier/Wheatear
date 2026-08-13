#include "wepch.h"
#include "WAOActionEditorPanel.h"
#include "WAOActionEditorPanelInternal.h"

#include "Editor/EditorLocale.h"
#include "Editor/GameplayEditorShell.h"
#include "Editor/GameplayEditorShell.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"

#include <imgui/imgui.h>

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    using namespace WAOActionEditorInternal;

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
} // namespace Wheatear
