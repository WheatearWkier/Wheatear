#include "wepch.h"
#include "ProgressionContentEditorPanel.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "ProgressionContentEditorPanelInternal.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Modules/Progression/ProgressionContent.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Wheatear {

    using namespace ProgressionContentEditorInternal;


    void ProgressionContentEditorPanel::Open(const std::string& manifestPath)
    {
        m_Open = true;
        if (!manifestPath.empty() && manifestPath != m_ManifestPath)
        {
            m_ManifestPath = manifestPath;
            m_ManifestLoaded = false;
            m_SelectedAssetLoaded = false;
        }
    }

    void ProgressionContentEditorPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!m_ManifestLoaded)
            LoadManifest();

        if (EditorFloatingWindow::Begin("Progression Content Editor", &m_Open, 0, { 1240.0f, 780.0f }))
        {
            EditorFloatingWindow::DrawToggleButton("Progression Content Editor");
            DrawToolbar();
            ImGui::Separator();

            ImGui::BeginChild("##ProgressionFileList", { 250.0f, 0.0f }, true);
            DrawFileList();
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##ProgressionEditor", { 0.0f, 0.0f }, true);
            DrawSelectedAsset();
            ImGui::EndChild();
        }
        EditorFloatingWindow::End();
    }

    void ProgressionContentEditorPanel::LoadManifest()
    {
        m_ManifestDocument.SetSourcePath(m_ManifestPath);
        m_ManifestDocument.Load();
        m_ManifestLoaded = true;
        RefreshEntriesFromManifest();

        const bool selectedStillValid = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const AssetEntry& entry)
        {
            return entry.Key == m_SelectedKey;
        }) != m_Entries.end();

        if (!selectedStillValid)
            m_SelectedKey = m_Entries.empty() ? std::string{} : m_Entries.front().Key;

        LoadSelectedAsset();
        m_Status = m_ManifestDocument.GetStatus();
    }

    void ProgressionContentEditorPanel::SaveManifest()
    {
        if (m_ManifestDocument.Save(DefaultHeader))
        {
            RefreshEntriesFromManifest();
            ProgressionContent::Reload();
            m_Status = "Manifest saved and progression content reloaded.";
        }
        else
        {
            m_Status = m_ManifestDocument.GetStatus();
        }
    }

    void ProgressionContentEditorPanel::RefreshEntriesFromManifest()
    {
        m_Entries.clear();
        if (!m_ManifestDocument.IsParseValid())
            return;

        YAML::Node files = m_ManifestDocument.Root()["files"];
        if (!files || !files.IsMap())
            return;

        for (const std::string& key : MapKeysInOrder(files))
        {
            AssetEntry entry;
            entry.Key = key;
            entry.Label = HumanizeKey(key);
            entry.Path = ScalarText(files[key]);
            if (!entry.Key.empty() && !entry.Path.empty())
                m_Entries.push_back(std::move(entry));
        }
    }

    void ProgressionContentEditorPanel::SelectEntry(const std::string& key)
    {
        if (key == m_SelectedKey && m_SelectedAssetLoaded)
            return;

        m_SelectedKey = key;
        m_SelectedAssetLoaded = false;
        LoadSelectedAsset();
    }

    void ProgressionContentEditorPanel::LoadSelectedAsset()
    {
        m_SelectedSkillNodeId.clear();
        m_SelectedContentRecordId.clear();
        m_SelectedPath.clear();
        for (const AssetEntry& entry : m_Entries)
        {
            if (entry.Key == m_SelectedKey)
            {
                m_SelectedPath = entry.Path;
                break;
            }
        }

        if (m_SelectedPath.empty())
        {
            m_SelectedAssetLoaded = false;
            return;
        }

        m_SelectedDocument.SetSourcePath(m_SelectedPath);
        m_SelectedDocument.Load();
        m_SelectedAssetLoaded = true;
        m_Status = m_SelectedDocument.GetStatus();
    }

    void ProgressionContentEditorPanel::SaveSelectedAsset()
    {
        if (!m_SelectedAssetLoaded)
            return;

        if (m_SelectedDocument.Save(DefaultHeader))
        {
            ProgressionContent::Reload();
            m_Status = "Saved " + m_SelectedKey + " and reloaded progression content.";
        }
        else
            m_Status = m_SelectedDocument.GetStatus();
    }

    void ProgressionContentEditorPanel::DrawToolbar()
    {
        EditorWidgets::PanelHeader(
            EditorLocale::Text("Progression Content", "成长内容"),
            EditorLocale::Text("Manifest-driven structured editing for progression YAML assets.", "由清单驱动的成长数据结构化编辑。"));

        ImGui::PushItemWidth(520.0f);
        if (EditorWidgets::InputString("Manifest", m_ManifestPath, 512))
            m_ManifestLoaded = false;
        ImGui::PopItemWidth();

        if (ImGui::Button("Reload Manifest"))
            LoadManifest();
        ImGui::SameLine();
        bool reloadManifest = false;
        if (EditorWidgets::DirtySaveBar(m_ManifestDocument.IsDirty(), m_ManifestDocument.GetStatus(), "Save Manifest", "Discard Manifest", &reloadManifest))
            SaveManifest();
        if (reloadManifest)
            LoadManifest();

        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_SelectedDocument.IsDirty(),
            m_ManifestDocument.IsParseValid() && m_SelectedDocument.IsParseValid(),
            m_SelectedPath.empty() ? m_ManifestPath : m_SelectedPath,
            m_Status
        });
    }

    void ProgressionContentEditorPanel::DrawFileList()
    {
        EditorWidgets::SectionHeader(
            EditorLocale::Text("Files", "文件"),
            EditorLocale::Text("Loaded from the progression manifest.", "从成长清单中载入。"));
        if (m_Entries.empty())
        {
            EditorWidgets::EmptyState(
                EditorLocale::Text("No progression files found.", "没有找到成长文件。"),
                EditorLocale::Text("Check the manifest files map.", "检查清单里的 files 映射。"));
            return;
        }

        for (const AssetEntry& entry : m_Entries)
        {
            ImGui::PushID(entry.Key.c_str());
            const bool selected = entry.Key == m_SelectedKey;
            if (ImGui::Selectable(entry.Label.c_str(), selected))
                SelectEntry(entry.Key);
            ImGui::TextDisabled("%s", entry.Path.c_str());
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    void ProgressionContentEditorPanel::DrawSelectedAsset()
    {
        if (m_SelectedKey.empty())
        {
            EditorWidgets::EmptyState("No file selected.");
            return;
        }

        if (!m_SelectedAssetLoaded)
            LoadSelectedAsset();

        ImGui::Text("Selected: %s", m_SelectedKey.c_str());
        ImGui::TextDisabled("%s", m_SelectedPath.c_str());
        if (!m_SelectedDocument.GetResolvedPath().empty())
            ImGui::TextDisabled("%s", m_SelectedDocument.GetResolvedPath().generic_string().c_str());

        if (ImGui::Button("Reload Asset"))
            LoadSelectedAsset();
        ImGui::SameLine();
        bool reloadAsset = false;
        if (EditorWidgets::DirtySaveBar(m_SelectedDocument.IsDirty(), m_SelectedDocument.GetStatus(), "Save Asset", "Discard Asset", &reloadAsset))
            SaveSelectedAsset();
        if (reloadAsset)
            LoadSelectedAsset();

        const bool isSkillNodes = m_SelectedKey == "skillNodes"
            || m_SelectedPath.find("skill_nodes.yaml") != std::string::npos;
        const bool hasContentTab = isSkillNodes
            || RootKeyForSelectedAsset(m_SelectedKey) != nullptr
            || m_SelectedKey == "defaults"
            || m_SelectedKey == "upgrades"
            || m_SelectedKey == "dungeonRewardSummary";

        if (ImGui::BeginTabBar("##ProgressionContentTabs"))
        {
            if (hasContentTab && ImGui::BeginTabItem("Content"))
            {
                DrawContentTab();
                ImGui::EndTabItem();
            }

            if (isSkillNodes && ImGui::BeginTabItem(EditorLocale::Text("Skill Tree", "技能树")))
            {
                DrawSkillTreeTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Validation"))
            {
                DrawValidationTab();
                ImGui::EndTabItem();
            }

            if (EditorGameplayShell::BeginAdvancedTab("Advanced YAML"))
            {
                EditorWidgets::InlineStatus("Advanced structured YAML editing. Prefer the Content tab for normal authoring.", EditorWidgets::StatusKind::Warning);
                if (!m_SelectedDocument.IsParseValid())
                    EditorWidgets::InlineStatus("YAML parse failed. Fix the source file before structured editing.", EditorWidgets::StatusKind::Error);
                else if (DrawYamlNode(m_SelectedDocument.Root(), "asset:" + m_SelectedKey))
                    m_SelectedDocument.MarkDirty();
                ImGui::EndTabItem();
            }

            if (EditorGameplayShell::BeginRawPreviewTab("Advanced Raw"))
            {
                EditorWidgets::InlineStatus("Read-only raw YAML preview. This is here for verification, not normal content entry.", EditorWidgets::StatusKind::Info);
                m_SelectedDocument.RefreshRawPreview();
                EditorGameplayShell::DrawRawPreview(m_SelectedDocument.GetRawPreview(), "##ProgressionAssetRawPreview");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Manifest"))
            {
                DrawManifestTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    void ProgressionContentEditorPanel::DrawSkillTreeTab()
    {
        if (!m_SelectedDocument.IsParseValid())
        {
            EditorWidgets::InlineStatus("YAML parse failed. Fix the skill tree file before graph editing.", EditorWidgets::StatusKind::Error);
            DrawRawPreview(m_SelectedDocument.GetRawPreview(), "##SkillTreeRawPreview");
            return;
        }

        YAML::Node root = m_SelectedDocument.Root();
        YAML::Node skillNodes = root["skillNodes"];
        if (!skillNodes || !skillNodes.IsSequence())
        {
            EditorWidgets::InlineStatus("This asset has no skillNodes list yet.", EditorWidgets::StatusKind::Warning);
            if (ImGui::Button("Create skillNodes List"))
            {
                root["skillNodes"] = YAML::Node(YAML::NodeType::Sequence);
                m_SelectedDocument.MarkDirty();
                m_Status = "Created skillNodes list.";
            }
            return;
        }

        std::vector<SkillGraphNode> graphNodes = BuildSkillGraphNodes(skillNodes);
        if (m_SelectedSkillNodeId.empty() && !graphNodes.empty())
            m_SelectedSkillNodeId = graphNodes.front().Id;

        const int selectedIndex = FindSkillGraphIndex(graphNodes, m_SelectedSkillNodeId);
        const SkillGraphNode* selectedNode = selectedIndex >= 0 ? &graphNodes[static_cast<size_t>(selectedIndex)] : nullptr;

        std::unordered_map<std::string, int> idCounts;
        int syntheticCount = 0;
        for (const SkillGraphNode& node : graphNodes)
        {
            ++idCounts[node.Id];
            if (IsSyntheticSkillId(node.Id))
                ++syntheticCount;
        }

        if (graphNodes.empty())
        {
            EditorWidgets::EmptyState("No skill nodes found.", "Add a skill node to start building the tree.");
            if (ImGui::Button("Add Skill"))
            {
                YAML::Node newNode(YAML::NodeType::Map);
                newNode["id"] = "NEW_SKILL";
                newNode["name"] = "New Skill";
                newNode["branch"] = "Custom";
                newNode["input"] = "";
                newNode["comboRole"] = "";
                newNode["requirement"] = "";
                newNode["description"] = "";
                newNode["unlockChapter"] = 1;
                WriteSkillNodePosition(newNode, 0.5f, 0.5f);
                skillNodes.push_back(newNode);
                m_SelectedSkillNodeId = "NEW_SKILL";
                m_SelectedDocument.MarkDirty();
                m_Status = "Added skill node.";
            }
            return;
        }

        if (syntheticCount > 0)
            EditorWidgets::InlineStatus("Some nodes are missing ids. They are shown as temporary graph entries until fixed.", EditorWidgets::StatusKind::Warning);

        int duplicateIdCount = 0;
        for (const auto& entry : idCounts)
        {
            if (entry.second > 1)
                ++duplicateIdCount;
        }
        if (duplicateIdCount > 0)
            EditorWidgets::InlineStatus("Duplicate skill node ids detected. The graph can still be edited, but the scene sync will be ambiguous.", EditorWidgets::StatusKind::Error);

        ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float inspectorWidth = std::clamp(avail.x * 0.33f, 320.0f, 420.0f);
        const float canvasWidth = std::max(380.0f, avail.x - inspectorWidth - style.ItemSpacing.x);
        const ImGuiWindowFlags canvasFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::BeginChild("##SkillTreeCanvas", ImVec2(canvasWidth, 0.0f), true, canvasFlags);
        {
            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            canvasSize.x = std::max(canvasSize.x, 360.0f);
            canvasSize.y = std::max(canvasSize.y, 420.0f);
            const ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(14, 20, 24, 248), 4.0f);
            const float gridStep = 40.0f;
            for (float x = std::fmod(canvasMin.x, gridStep); x < canvasSize.x; x += gridStep)
            {
                const float lineX = canvasMin.x + x;
                drawList->AddLine({ lineX, canvasMin.y }, { lineX, canvasMax.y }, IM_COL32(42, 52, 58, 180), 1.0f);
            }
            for (float y = std::fmod(canvasMin.y, gridStep); y < canvasSize.y; y += gridStep)
            {
                const float lineY = canvasMin.y + y;
                drawList->AddLine({ canvasMin.x, lineY }, { canvasMax.x, lineY }, IM_COL32(42, 52, 58, 180), 1.0f);
            }

            ImGui::SetCursorScreenPos(canvasMin);
            ImGui::InvisibleButton("##SkillTreeSurface", canvasSize,
                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

            std::unordered_map<std::string, ImVec2> centers;
            centers.reserve(graphNodes.size());
            for (const SkillGraphNode& node : graphNodes)
                centers.emplace(node.Id, SkillGraphToScreen(canvasMin, canvasSize, node.Position));

            drawList->PushClipRect(canvasMin, canvasMax, true);
            for (const SkillGraphNode& node : graphNodes)
            {
                if (node.ParentId.empty() || node.ParentId == node.Id)
                    continue;

                const auto parentIt = centers.find(node.ParentId);
                const auto childIt = centers.find(node.Id);
                if (parentIt == centers.end() || childIt == centers.end())
                    continue;

                const ImVec2 from = parentIt->second;
                const ImVec2 to = childIt->second;
                const ImVec2 c1 = { from.x, (from.y + to.y) * 0.5f };
                const ImVec2 c2 = { to.x, (from.y + to.y) * 0.5f };
                drawList->AddBezierCubic(from, c1, c2, to, IM_COL32(112, 188, 176, 165), 2.0f, 24);
            }

            const ImVec2 nodeSize = { 176.0f, 60.0f };
            for (auto& node : graphNodes)
            {
                const ImVec2 center = SkillGraphToScreen(canvasMin, canvasSize, node.Position);
                const ImVec2 min = { center.x - nodeSize.x * 0.5f, center.y - nodeSize.y * 0.5f };
                const ImVec2 max = { min.x + nodeSize.x, min.y + nodeSize.y };
                const bool selected = node.Id == m_SelectedSkillNodeId;
                const bool duplicateId = idCounts[node.Id] > 1;

                const ImU32 fill = selected
                    ? IM_COL32(35, 108, 110, 245)
                    : (duplicateId ? IM_COL32(92, 58, 58, 236) : IM_COL32(26, 41, 46, 236));
                const ImU32 border = selected
                    ? IM_COL32(255, 214, 92, 255)
                    : (duplicateId ? IM_COL32(242, 146, 118, 255) : IM_COL32(92, 118, 121, 225));

                drawList->AddRectFilled(min, max, fill, 8.0f);
                drawList->AddRect(min, max, border, 8.0f, 0, selected ? 2.4f : 1.2f);
                drawList->AddText({ min.x + 10.0f, min.y + 8.0f }, IM_COL32(236, 246, 244, 255), Shorten(node.Id, 24).c_str());
                drawList->AddText({ min.x + 10.0f, min.y + 28.0f }, IM_COL32(184, 205, 199, 255),
                    Shorten(node.Name.empty() ? node.Branch : node.Name, 28).c_str());
                if (!node.Branch.empty())
                {
                    drawList->AddText({ min.x + 10.0f, min.y + 44.0f }, IM_COL32(132, 154, 149, 255), Shorten(node.Branch, 22).c_str());
                }
                if (node.HasExplicitPosition)
                    drawList->AddCircleFilled({ max.x - 12.0f, min.y + 12.0f }, 4.0f, IM_COL32(122, 219, 187, 255));

                ImGui::SetCursorScreenPos(min);
                ImGui::PushID(static_cast<int>(node.Index));
                ImGui::InvisibleButton("##SkillNodeHit", nodeSize, ImGuiButtonFlags_MouseButtonLeft);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    m_SelectedSkillNodeId = node.Id;
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const ImVec2 dragged = { mouse.x - nodeSize.x * 0.5f, mouse.y - nodeSize.y * 0.5f };
                    const ImVec2 normalized = SkillGraphFromScreen(canvasMin, canvasSize, dragged);
                    WriteSkillNodePosition(skillNodes[node.Index], normalized.x, normalized.y);
                    node.Position = normalized;
                    node.HasExplicitPosition = true;
                    m_SelectedSkillNodeId = node.Id;
                    m_SelectedDocument.MarkDirty();
                }
                ImGui::PopID();
            }
            drawList->PopClipRect();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##SkillTreeInspector", ImVec2(0.0f, 0.0f), true);
        {
            EditorWidgets::SectionHeader("Skill Node Inspector", "Edit node identity, parent links, and baked layout coordinates.");

            const bool hasSelection = selectedNode != nullptr;
            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::Button("Duplicate Selected") && hasSelection)
            {
                const std::string baseId = selectedNode->Id.empty() || IsSyntheticSkillId(selectedNode->Id)
                    ? "NEW_SKILL"
                    : selectedNode->Id + "_copy";
                YAML::Node clone = YAML::Clone(skillNodes[selectedNode->Index]);
                const std::string newId = MakeUniqueSkillNodeId(skillNodes, baseId);
                clone["id"] = newId;
                WriteSkillNodePosition(clone, std::clamp(selectedNode->Position.x + 0.05f, 0.0f, 1.0f),
                    std::clamp(selectedNode->Position.y + 0.05f, 0.0f, 1.0f));
                skillNodes.push_back(clone);
                m_SelectedSkillNodeId = newId;
                m_SelectedDocument.MarkDirty();
                m_Status = "Duplicated skill node.";
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Selected") && hasSelection)
            {
                skillNodes.remove(selectedNode->Index);
                m_SelectedSkillNodeId.clear();
                m_SelectedDocument.MarkDirty();
                m_Status = "Deleted skill node.";
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Add Skill"))
            {
                YAML::Node newNode(YAML::NodeType::Map);
                const std::string baseId = hasSelection && !selectedNode->Id.empty() && !IsSyntheticSkillId(selectedNode->Id)
                    ? selectedNode->Id + "_child"
                    : "NEW_SKILL";
                const std::string newId = MakeUniqueSkillNodeId(skillNodes, baseId);
                newNode["id"] = newId;
                newNode["name"] = "New Skill";
                newNode["branch"] = hasSelection ? selectedNode->Branch : "Custom";
                newNode["input"] = "";
                newNode["comboRole"] = "";
                newNode["requirement"] = "";
                newNode["description"] = "";
                newNode["unlockChapter"] = hasSelection ? selectedNode->UnlockChapter : 1;
                if (hasSelection && !selectedNode->Id.empty() && !IsSyntheticSkillId(selectedNode->Id))
                    newNode["parentId"] = selectedNode->Id;

                const ImVec2 seed = hasSelection ? selectedNode->Position : ImVec2{ 0.5f, 0.5f };
                WriteSkillNodePosition(newNode, std::clamp(seed.x + 0.06f, 0.0f, 1.0f), std::clamp(seed.y + 0.04f, 0.0f, 1.0f));
                skillNodes.push_back(newNode);
                m_SelectedSkillNodeId = newId;
                m_SelectedDocument.MarkDirty();
                m_Status = "Added skill node.";
            }

            ImGui::Separator();

            if (!hasSelection)
            {
                EditorWidgets::EmptyState("Select a node on the canvas to edit its fields.");
            }
            else
            {
                YAML::Node selectedYamlNode = skillNodes[selectedNode->Index];
                const std::string originalId = selectedNode->Id;

                ImGui::PushID(static_cast<int>(selectedNode->Index));
                if (DrawSkillTextField(selectedYamlNode, "Id", "id", 128))
                {
                    const std::string newId = ScalarText(selectedYamlNode["id"]);
                    if (!newId.empty() && newId != originalId)
                    {
                        for (size_t i = 0; i < skillNodes.size(); ++i)
                        {
                            YAML::Node child = skillNodes[i];
                            if (ScalarText(child["parentId"]) == originalId)
                                child["parentId"] = newId;
                        }
                        m_SelectedSkillNodeId = newId;
                    }
                    m_SelectedDocument.MarkDirty();
                }
                if (DrawSkillTextField(selectedYamlNode, "Name", "name", 128))
                    m_SelectedDocument.MarkDirty();
                if (DrawChoiceField(selectedYamlNode, "Branch", "branch", SkillBranchChoices(), 128))
                    m_SelectedDocument.MarkDirty();
                if (DrawSkillTextField(selectedYamlNode, "Input", "input", 128))
                    m_SelectedDocument.MarkDirty();
                if (DrawSkillTextField(selectedYamlNode, "Combo Role", "comboRole", 256))
                    m_SelectedDocument.MarkDirty();
                if (DrawRequirementField(selectedYamlNode, EditorLocale::Text("Requirement", "需求"), "requirement"))
                    m_SelectedDocument.MarkDirty();
                if (DrawSkillMultilineField(selectedYamlNode, EditorLocale::Text("Description", "描述"), "description"))
                    m_SelectedDocument.MarkDirty();

                int unlockChapter = selectedYamlNode["unlockChapter"].as<int>(selectedNode->UnlockChapter);
                if (ImGui::DragInt(EditorLocale::Text("Unlock Chapter", "解锁章节"), &unlockChapter, 1.0f, 0, 99))
                {
                    selectedYamlNode["unlockChapter"] = unlockChapter;
                    m_SelectedDocument.MarkDirty();
                }

                std::string parentId = ScalarText(selectedYamlNode["parentId"]);
                const char* parentPreview = parentId.empty() ? "(root)" : parentId.c_str();
                if (ImGui::BeginCombo("Parent", parentPreview))
                {
                    if (ImGui::Selectable("(root)", parentId.empty()))
                    {
                        selectedYamlNode["parentId"] = "";
                        m_SelectedSkillNodeId = ScalarText(selectedYamlNode["id"]);
                        m_SelectedDocument.MarkDirty();
                    }

                    for (const SkillGraphNode& candidate : graphNodes)
                    {
                        if (candidate.Index == selectedNode->Index || candidate.Id.empty() || IsSyntheticSkillId(candidate.Id))
                            continue;

                        ImGui::PushID(static_cast<int>(candidate.Index));
                        const bool candidateSelected = parentId == candidate.Id;
                        if (ImGui::Selectable(candidate.Id.c_str(), candidateSelected))
                        {
                            selectedYamlNode["parentId"] = candidate.Id;
                            m_SelectedSkillNodeId = ScalarText(selectedYamlNode["id"]);
                            m_SelectedDocument.MarkDirty();
                            parentId = candidate.Id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }

                ImVec2 position = selectedNode->Position;
                if (ImGui::DragFloat2("Position", &position.x, 0.002f, 0.0f, 1.0f, "%.3f"))
                {
                    WriteSkillNodePosition(selectedYamlNode, position.x, position.y);
                    m_SelectedDocument.MarkDirty();
                }

                if (ImGui::Button("Bake Layout To YAML"))
                {
                    for (const SkillGraphNode& node : graphNodes)
                    {
                        YAML::Node item = skillNodes[node.Index];
                        item["parentId"] = node.ParentId;
                        WriteSkillNodePosition(item, node.Position.x, node.Position.y);
                    }
                    m_SelectedDocument.MarkDirty();
                    m_Status = "Baked skill tree layout into YAML.";
                }

                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }













} // namespace Wheatear
