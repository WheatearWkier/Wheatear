#include "wepch.h"
#include "ProjectHealthPanel.h"

#include "Assets/AssetRegistry.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "ProjectHealthPanelInternal.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Config/PlayerConfig.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Scene/EntityReference.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <tuple>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    using namespace ProjectHealthPanelInternal;

    void ProjectHealthPanel::Open(const EditorToolContext&)
    {
        m_Open = true;
        if (m_StartupScene.empty())
        {
            // Project-level startup scene lives in assets/game/player.config;
            // fall back to the engine default when the project has none.
            const std::filesystem::path projectConfig =
                AssetPath::GetProjectRoot() / "assets" / "game" / "player.config";
            const RuntimePlayerConfig config = LoadRuntimePlayerConfig(projectConfig);
            m_StartupScene = config.StartupScene.generic_string();
        }
        if (!m_AliasManifestDocument.IsLoaded())
            LoadAliasManifest();
        Refresh();
    }

    void ProjectHealthPanel::SaveStartupScene()
    {
        const std::filesystem::path configPath =
            AssetPath::GetProjectRoot() / "assets" / "game" / "player.config";
        const RuntimePlayerConfig config{ m_StartupScene };
        if (SaveRuntimePlayerConfig(configPath, config, EngineInfo::EditorName))
        {
            m_Status = "Startup scene saved to " + configPath.generic_string();
            Refresh();
        }
        else
        {
            m_Status = "Failed to save startup scene config: " + configPath.generic_string();
        }
    }

    void ProjectHealthPanel::LoadAliasManifest()
    {
        m_AliasManifestDocument.SetSourcePath(m_AliasManifestPath);
        m_AliasManifestDocument.Load();
        m_AliasStatus = m_AliasManifestDocument.GetStatus();
        m_SelectedAlias.clear();
    }

    void ProjectHealthPanel::SaveAliasManifest()
    {
        if (m_AliasManifestDocument.Save(AliasManifestHeader))
        {
            AssetAliasRegistry::Reload(m_AliasManifestPath);
            m_AliasStatus = "Alias manifest saved and registry reloaded.";
            Refresh();
        }
        else
        {
            m_AliasStatus = m_AliasManifestDocument.GetStatus();
        }
    }

    void ProjectHealthPanel::Refresh()
    {
        AssetDependencyScanOptions options;
        options.ProjectRoot = AssetPath::GetProjectRoot();
        options.BuiltinRoot = AssetPath::GetEngineRoot();
        options.StartupAsset = m_StartupScene;
        options.IncludeBuiltinAssets = true;
        options.IncludeUnusedAssets = m_IncludeUnusedAssets;

        m_Report = AssetDependencyScanner::BuildReport(options);
        m_SourceReport = ProjectSourceScanner::BuildReport(options.ProjectRoot);
        AssetRegistry::Get().Scan(options.ProjectRoot);
        AssetRegistry::Get().WriteRegistry();
        m_HygieneReport = BuildAssetHygieneReport(options.ProjectRoot, m_Report);
        ScanEntityBindings();
        const size_t sourceSyncIssues = m_SourceReport.MissingFromProject.size()
            + m_SourceReport.StaleProjectEntries.size();
        m_Status = "Last scan: " + std::to_string(m_Report.IncludedAssets.size()) + " assets, "
            + std::to_string(m_Report.MissingReferences.size()) + " missing references, "
            + std::to_string(m_Report.MissingSceneTransitions.size()) + " missing scene transition(s), "
            + std::to_string(sourceSyncIssues) + " source sync issue(s), "
            + std::to_string(m_HygieneReport.DuplicateGroups.size()) + " duplicate group(s), "
            + std::to_string(m_HygieneReport.AliasIssues.size()) + " alias issue(s), "
            + std::to_string(m_MissingEntityBindings.size()) + " dangling entity binding(s).";
    }

    void ProjectHealthPanel::ScanEntityBindings()
    {
        m_MissingEntityBindings.clear();

        const std::filesystem::path scenesDir =
            AssetPath::GetProjectRoot() / "assets" / "scenes";
        if (!std::filesystem::is_directory(scenesDir))
            return;

        for (const auto& entry : std::filesystem::directory_iterator(scenesDir))
        {
            if (!entry.is_regular_file())
                continue;
            const std::filesystem::path path = entry.path();
            if (path.extension() != ".wt")
                continue;

            YAML::Node root;
            try
            {
                root = YAML::LoadFile(path.string());
            }
            catch (...)
            {
                continue;
            }
            if (!root.IsMap())
                continue;

            const YAML::Node entities = root["Entities"];
            if (!entities.IsSequence())
                continue;

            // Pass 1: collect entity tags and UUIDs of this scene.
            std::unordered_set<std::string> tags;
            std::unordered_set<uint64_t> uuids;
            for (const YAML::Node& entity : entities)
            {
                if (!entity.IsMap())
                    continue;
                const YAML::Node tagNode = entity["TagComponent"];
                if (tagNode && tagNode.IsMap())
                {
                    const std::string tag = tagNode["Tag"].as<std::string>("");
                    if (!tag.empty())
                        tags.insert(tag);
                }
                const uint64_t uuid = entity["Entity"].as<uint64_t>(0);
                if (uuid != 0)
                    uuids.insert(uuid);
            }

            // Pass 2: every *EntityName string field (or "@UUID" selector)
            // must resolve to a tag / UUID of the same scene.
            const std::string relativePath =
                AssetPath::ToProjectRelative(path).generic_string();
            for (const YAML::Node& entity : entities)
            {
                if (!entity.IsMap())
                    continue;
                for (auto componentIt = entity.begin(); componentIt != entity.end(); ++componentIt)
                {
                    const std::string componentKey = componentIt->first.as<std::string>("");
                    const YAML::Node component = componentIt->second;
                    if (!component.IsMap())
                        continue;

                    for (auto fieldIt = component.begin(); fieldIt != component.end(); ++fieldIt)
                    {
                        const std::string fieldKey = fieldIt->first.as<std::string>("");
                        const YAML::Node fieldValue = fieldIt->second;
                        if (!fieldValue.IsScalar())
                            continue;

                        const std::string value = fieldValue.as<std::string>("");
                        if (value.empty())
                            continue;

                        // Binding-like fields: *EntityName members, or any
                        // scalar carrying an "@UUID" selector. Skip runtime
                        // state, prefixes and spawn names (those reference
                        // entities created at runtime, not authored ones).
                        const bool looksLikeBinding =
                            fieldKey.find("EntityName") != std::string::npos
                            || (!value.empty() && value.front() == '@');
                        if (!looksLikeBinding)
                            continue;
                        if (fieldKey.rfind("Runtime", 0) == 0)
                            continue;
                        if (fieldKey.find("Prefix") != std::string::npos
                            || fieldKey.find("Spawn") != std::string::npos)
                            continue;

                        bool resolved = false;
                        if (value.front() == '@')
                        {
                            const uint64_t uuid = EntityReferences::ParseUUIDSelector(value);
                            resolved = uuid != 0 && uuids.count(uuid) != 0;
                        }
                        else
                        {
                            resolved = tags.count(value) != 0;
                        }

                        if (!resolved)
                        {
                            m_MissingEntityBindings.push_back({
                                value,
                                relativePath + " :: " + componentKey + "." + fieldKey
                            });
                        }
                    }
                }
            }
        }
    }



    void ProjectHealthPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!EditorFloatingWindow::Begin(EditorLocale::Text("Project Health", "项目健康检查"), &m_Open, 0, { 1120.0f, 720.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }

        const size_t issueCount = m_Report.MissingReferences.size()
            + m_Report.MissingSceneTransitions.size()
            + m_SourceReport.MissingFromProject.size()
            + m_SourceReport.StaleProjectEntries.size()
            + m_HygieneReport.DuplicateGroups.size()
            + m_HygieneReport.AliasIssues.size()
            + m_MissingEntityBindings.size();
        EditorWidgets::PanelHeader(EditorLocale::Text("Project Health", "项目健康检查"), "Package dependency, asset hygiene, registry, and source/project sync validation.");
        EditorWidgets::StatusBadge(issueCount == 0 ? "Ready" : "Needs Attention",
            issueCount == 0 ? EditorWidgets::StatusKind::Success : EditorWidgets::StatusKind::Error);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(m_Report.IncludedAssets.size()) + " packed assets").c_str(), EditorWidgets::StatusKind::Info);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(m_Report.Warnings.size()) + " warning(s)").c_str(),
            m_Report.Warnings.empty() ? EditorWidgets::StatusKind::Neutral : EditorWidgets::StatusKind::Warning);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(m_HygieneReport.DuplicateGroups.size()) + " duplicate group(s)").c_str(),
            m_HygieneReport.DuplicateGroups.empty() ? EditorWidgets::StatusKind::Neutral : EditorWidgets::StatusKind::Warning);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(m_HygieneReport.HardcodedAssetPaths.size()) + " hardcoded path(s)").c_str(),
            m_HygieneReport.HardcodedAssetPaths.empty() ? EditorWidgets::StatusKind::Neutral : EditorWidgets::StatusKind::Warning);
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton(EditorLocale::Text("Project Health", "项目健康检查"));

        EditorWidgets::SectionHeader(EditorLocale::Text("Scan Scope", "扫描范围"), "The startup scene defines the package dependency closure.");
        EditorWidgets::InputString(EditorLocale::Text("Startup Scene", "启动场景"), m_StartupScene, 384);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Save Startup Scene", "保存启动场景")))
            SaveStartupScene();
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Scan Unused Assets", "扫描未使用资源"), &m_IncludeUnusedAssets);

        if (ImGui::Button(EditorLocale::Text("Refresh", "刷新")))
            Refresh();
        ImGui::SameLine();
        EditorWidgets::InlineStatus(m_Status, issueCount == 0 ? EditorWidgets::StatusKind::Success : EditorWidgets::StatusKind::Warning);

        EditorWidgets::SectionHeader(EditorLocale::Text("Summary", "摘要"));
        DrawSummary();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ProjectHealthTabs"))
        {
            if (ImGui::BeginTabItem(EditorLocale::Text("Missing", "缺失引用")))
            {
                DrawMissingReferences();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Asset Registry", "资源注册表")))
            {
                DrawAssetRegistry();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Packed Assets", "打包资源")))
            {
                DrawAssetList("PackedAssetTable", m_Report.IncludedAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Scene Links", "场景链接")))
            {
                DrawSceneTransitions();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Entity Bindings", "实体绑定")))
            {
                DrawEntityBindings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Unused Assets", "未使用资源")))
            {
                DrawAssetList("UnusedAssetTable", m_Report.UnusedAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Parsed Text", "解析文本")))
            {
                DrawAssetList("ParsedTextAssetTable", m_Report.ParsedTextAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Warnings", "警告")))
            {
                if (m_Report.Warnings.empty())
                {
                    EditorWidgets::EmptyState(EditorLocale::Text("No warnings.", "无警告。"), "The current scan did not find legacy commands or package risk notes.");
                }
                else
                {
                    for (const std::string& warning : m_Report.Warnings)
                        ImGui::BulletText("%s", warning.c_str());
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Asset Hygiene", "资源卫生")))
            {
                DrawAssetHygiene();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Alias Manifest", "别名清单")))
            {
                DrawAliasManifestEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Source Sync", "源码同步")))
            {
                DrawSourceSync();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        EditorFloatingWindow::End();
    }

    void ProjectHealthPanel::DrawSummary() const
    {
        SummaryLine(EditorLocale::Text("Packed Assets", "打包资源"), m_Report.IncludedAssets.size());
        SummaryLine("Packed Source Size", FormatBytes(m_Report.IncludedBytes));
        SummaryLine("Packable Assets", m_Report.PackableAssetCount);
        SummaryLine("Packable Source Size", FormatBytes(m_Report.PackableBytes));
        SummaryLine(EditorLocale::Text("Unused Assets", "未使用资源"), m_Report.UnusedAssets.size());
        SummaryLine("Parsed Text Assets", m_Report.ParsedTextAssets.size());
        SummaryLine("Scene Transitions", m_Report.SceneTransitions.size());
        SummaryLine("Missing Scene Transitions", m_Report.MissingSceneTransitions.size());
        SummaryLine(EditorLocale::Text("Missing References", "缺失引用"), m_Report.MissingReferences.size());
        SummaryLine("Asset Registry Assets", AssetRegistry::Get().GetAssetCount());
        SummaryLine("Asset Registry References", AssetRegistry::Get().GetReferenceCount());
        SummaryLine(EditorLocale::Text("Warnings", "警告"), m_Report.Warnings.size());
        SummaryLine("Duplicate Groups", m_HygieneReport.DuplicateGroups.size());
        SummaryLine("Duplicate Extra Assets", m_HygieneReport.DuplicateExtraAssetCount);
        SummaryLine("Alias Issues", m_HygieneReport.AliasIssues.size());
        SummaryLine("Hardcoded Asset Paths", m_HygieneReport.HardcodedAssetPaths.size());
        SummaryLine("Source Files", m_SourceReport.ScannedSourceFiles);
        SummaryLine("Project Entries", m_SourceReport.ScannedProjectEntries);
        SummaryLine("Source Sync Issues",
            m_SourceReport.MissingFromProject.size() + m_SourceReport.StaleProjectEntries.size());
    }

    void ProjectHealthPanel::DrawAssetRegistry() const
    {
        const auto& registry = AssetRegistry::Get();
        SummaryLine("Registered Assets", registry.GetAssetCount());
        SummaryLine("Reference Edges", registry.GetReferenceCount());

        ImGui::Spacing();
        if (ImGui::BeginTable("AssetRegistryTable", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 420)))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("UUID");
            ImGui::TableSetupColumn(EditorLocale::Text("Asset", "资源"));
            ImGui::TableSetupColumn(EditorLocale::Text("Refs", "引用数"));
            ImGui::TableSetupColumn(EditorLocale::Text("Used By", "被引用"));
            ImGui::TableHeadersRow();

            const auto& assets = registry.GetAssets();
            const size_t rowCount = std::min<size_t>(assets.size(), 1000);
            for (size_t i = 0; i < rowCount; ++i)
            {
                const EditorAssetMetadata& metadata = assets[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(AssetRegistry::KindToString(metadata.Kind).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%llu", static_cast<unsigned long long>(static_cast<uint64_t>(metadata.ID)));
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", metadata.RelativePath.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%zu", metadata.References.size());
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%zu", metadata.ReferencedBy.size());
            }

            ImGui::EndTable();
        }
    }

    void ProjectHealthPanel::DrawMissingReferences() const
    {
        if (m_Report.MissingReferences.empty())
        {
            EditorWidgets::EmptyState(EditorLocale::Text("No missing asset references found.", "未发现缺失的资源引用。"), "All scanned scene, YAML, script, and manifest references resolve.");
            return;
        }

        if (ImGui::BeginTable("MissingReferenceTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Reference");
            ImGui::TableSetupColumn("Source");
            ImGui::TableHeadersRow();

            for (const AssetReferenceRecord& record : m_Report.MissingReferences)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", record.Reference.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", record.SourceAsset.empty() ? "(startup/builtin)" : record.SourceAsset.c_str());
            }

            ImGui::EndTable();
        }
    }

    void ProjectHealthPanel::DrawEntityBindings() const
    {
        if (m_MissingEntityBindings.empty())
        {
            EditorWidgets::EmptyState(
                EditorLocale::Text("No dangling entity bindings found.", "未发现悬空实体绑定。"),
                "Every *EntityName field and @UUID selector in the scene files resolves to an entity of the same scene.");
            return;
        }

        if (ImGui::BeginTable("EntityBindingTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(EditorLocale::Text("Bound Value", "绑定值"));
            ImGui::TableSetupColumn(EditorLocale::Text("Source", "来源"));
            ImGui::TableHeadersRow();

            for (const AssetReferenceRecord& record : m_MissingEntityBindings)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", record.Reference.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", record.SourceAsset.c_str());
            }

            ImGui::EndTable();
        }
    }

    void ProjectHealthPanel::DrawSceneTransitions() const
    {
        if (!m_Report.MissingSceneTransitions.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
                "Missing scene transition target(s) found.");
            if (ImGui::BeginTable("MissingSceneTransitionTable", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Scene");
                ImGui::TableSetupColumn("Source");
                ImGui::TableHeadersRow();

                for (const AssetReferenceRecord& record : m_Report.MissingSceneTransitions)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", record.Reference.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", record.SourceAsset.empty() ? "(startup/builtin)" : record.SourceAsset.c_str());
                }

                ImGui::EndTable();
            }
            ImGui::Separator();
        }

        if (m_Report.SceneTransitions.empty())
        {
            EditorWidgets::EmptyState("No scene transition commands found.", "Parsed text assets did not declare scene/newgame/loadgame targets.");
            return;
        }

        if (ImGui::BeginTable("SceneTransitionTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Scene");
            ImGui::TableSetupColumn("Source");
            ImGui::TableHeadersRow();

            for (const AssetReferenceRecord& record : m_Report.SceneTransitions)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", record.Reference.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", record.SourceAsset.empty() ? "(startup/builtin)" : record.SourceAsset.c_str());
            }

            ImGui::EndTable();
        }
    }



    void ProjectHealthPanel::DrawAliasManifestEditor()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Alias Manifest", "别名清单"), "Edit content_manifest.yaml aliases used by runtime asset lookup.");

        ImGui::SetNextItemWidth(420.0f);
        EditorWidgets::InputString("Manifest", m_AliasManifestPath, 512);
        ImGui::SameLine();
        if (ImGui::Button("Load Manifest"))
            LoadAliasManifest();
        ImGui::SameLine();
        if (ImGui::Button("Save Manifest"))
            SaveAliasManifest();

        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_AliasManifestDocument.IsDirty(),
            m_AliasManifestDocument.IsParseValid(),
            m_AliasManifestPath,
            m_AliasStatus.empty() ? m_AliasManifestDocument.GetStatus() : m_AliasStatus
        });

        if (!m_AliasManifestDocument.IsParseValid())
        {
            EditorWidgets::InlineStatus("Manifest YAML parse failed. Fix the file before alias editing.", EditorWidgets::StatusKind::Error);
            return;
        }

        YAML::Node root = m_AliasManifestDocument.Root();
        std::vector<AliasManifestRow> rows = BuildAliasRows(root);
        if (m_SelectedAlias.empty() && !rows.empty())
            m_SelectedAlias = rows.front().Alias;

        const float listWidth = std::max(360.0f, ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::BeginChild("##AliasManifestRows", ImVec2(listWidth, 0.0f), true);
        if (rows.empty())
        {
            EditorWidgets::EmptyState("No aliases found.", "Add an alias below to start building the manifest.");
        }
        else if (ImGui::BeginTable("AliasManifestTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 420)))
        {
            ImGui::TableSetupColumn(EditorLocale::Text("Alias", "别名"));
            ImGui::TableSetupColumn(EditorLocale::Text("Target", "目标"));
            ImGui::TableHeadersRow();

            for (const AliasManifestRow& row : rows)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const std::string label = EditorWidgets::LabelWithId(row.Alias, "alias_manifest:" + row.Alias);
                if (ImGui::Selectable(label.c_str(), row.Alias == m_SelectedAlias, ImGuiSelectableFlags_SpanAllColumns))
                    m_SelectedAlias = row.Alias;
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", row.Target.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        EditorWidgets::SectionHeader(EditorLocale::Text("Add Alias", "添加别名"));
        EditorWidgets::InputString(EditorLocale::Text("New Alias", "新别名"), m_NewAliasName, 256);
        EditorWidgets::DrawAssetReferenceField("New Target",
            m_NewAliasTarget,
            EditorWidgets::AssetReferenceKind::Any,
            512);
        const bool canAdd = !m_NewAliasName.empty() && !m_NewAliasTarget.empty();
        ImGui::BeginDisabled(!canAdd);
        if (ImGui::Button(EditorLocale::Text("Add Alias", "添加别名")) && canAdd)
        {
            if (SetAliasTarget(root, m_NewAliasName, m_NewAliasTarget))
            {
                m_SelectedAlias = m_NewAliasName;
                m_NewAliasName.clear();
                m_NewAliasTarget = "assets/";
                m_AliasManifestDocument.MarkDirty();
                m_AliasStatus = "Alias added.";
            }
        }
        ImGui::EndDisabled();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##AliasManifestDetails", ImVec2(0.0f, 0.0f), true);
        EditorWidgets::SectionHeader(EditorLocale::Text("Alias Details", "别名详情"), "Rename aliases or retarget them to project assets.");

        auto selected = std::find_if(rows.begin(), rows.end(), [&](const AliasManifestRow& row)
        {
            return row.Alias == m_SelectedAlias;
        });

        if (selected == rows.end())
        {
            EditorWidgets::EmptyState("Select an alias to edit.");
            ImGui::EndChild();
            return;
        }

        std::string alias = selected->Alias;
        std::string target = selected->Target;
        if (EditorWidgets::InputString(EditorLocale::Text("Alias", "别名"), alias, 256)
            && !alias.empty()
            && alias != selected->Alias)
        {
            RemoveAliasTarget(root, selected->Alias);
            SetAliasTarget(root, alias, target);
            m_SelectedAlias = alias;
            m_AliasManifestDocument.MarkDirty();
            m_AliasStatus = "Alias renamed.";
        }

        if (EditorWidgets::DrawAssetReferenceField(EditorLocale::Text("Target", "目标"),
            target,
            EditorWidgets::AssetReferenceKind::Any,
            512))
        {
            SetAliasTarget(root, m_SelectedAlias, target);
            m_AliasManifestDocument.MarkDirty();
            m_AliasStatus = "Alias target changed.";
        }

        const std::filesystem::path resolvedTarget = EditorWidgets::ResolveProjectAsset(target);
        if (!target.empty())
        {
            const bool exists = !resolvedTarget.empty() && std::filesystem::exists(resolvedTarget);
            EditorWidgets::StatusBadge(exists ? "Target exists" : "Target missing",
                exists ? EditorWidgets::StatusKind::Success : EditorWidgets::StatusKind::Error);
            if (!resolvedTarget.empty())
                ImGui::TextDisabled("%s", resolvedTarget.generic_string().c_str());
        }

        ImGui::Separator();
        if (ImGui::Button(EditorLocale::Text("Delete Alias", "删除别名")))
        {
            if (RemoveAliasTarget(root, m_SelectedAlias))
            {
                m_SelectedAlias.clear();
                m_AliasManifestDocument.MarkDirty();
                m_AliasStatus = "Alias deleted.";
            }
        }
        ImGui::EndChild();
    }

    void ProjectHealthPanel::DrawAssetList(const char* tableId,
        const std::vector<std::filesystem::path>& assets,
        size_t maxRows) const
    {
        if (assets.empty())
        {
            ImGui::TextDisabled("No assets in this list.");
            return;
        }

        if (assets.size() > maxRows)
            ImGui::TextDisabled("Showing first %zu of %zu rows.", maxRows, assets.size());

        if (ImGui::BeginTable(tableId, 1,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(EditorLocale::Text("Asset", "资源"));
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min(maxRows, assets.size());
            for (size_t i = 0; i < rowCount; ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const std::string path = assets[i].generic_string();
                ImGui::TextWrapped("%s", path.c_str());
            }

            ImGui::EndTable();
        }
    }

    void ProjectHealthPanel::DrawSourceSync() const
    {
        if (m_SourceReport.Healthy())
        {
            ImGui::TextDisabled("Source trees and Visual Studio project files are in sync.");
            return;
        }

        if (ImGui::BeginTable("ProjectSourceSyncTable", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Project");
            ImGui::TableSetupColumn("File");
            ImGui::TableHeadersRow();

            for (const ProjectSourceRecord& record : m_SourceReport.MissingFromProject)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Missing From Project");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(record.ProjectName.c_str());
                ImGui::TableSetColumnIndex(2);
                const std::string file = record.File.generic_string();
                ImGui::TextWrapped("%s", file.c_str());
            }

            for (const ProjectSourceRecord& record : m_SourceReport.StaleProjectEntries)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Stale Project Entry");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(record.ProjectName.c_str());
                ImGui::TableSetColumnIndex(2);
                const std::string file = record.File.generic_string();
                ImGui::TextWrapped("%s", file.c_str());
            }

            ImGui::EndTable();
        }
    }

} // namespace Wheatear
