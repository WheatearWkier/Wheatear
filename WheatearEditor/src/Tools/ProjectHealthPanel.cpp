#include "wepch.h"
#include "ProjectHealthPanel.h"

#include "Assets/AssetRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include <imgui/imgui.h>

#include <array>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Wheatear {

    namespace {

        static bool InputString(const char* label, std::string& value, size_t capacity)
        {
            std::vector<char> buffer(capacity, 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buffer.data(), buffer.size()))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

        static std::string FormatBytes(uintmax_t bytes)
        {
            const char* units[] = { "B", "KB", "MB", "GB" };
            double value = static_cast<double>(bytes);
            int unit = 0;
            while (value >= 1024.0 && unit < 3)
            {
                value /= 1024.0;
                ++unit;
            }

            std::ostringstream out;
            out << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << " " << units[unit];
            return out.str();
        }

        static void SummaryLine(const char* label, const std::string& value)
        {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(180.0f);
            ImGui::TextUnformatted(value.c_str());
        }

        static void SummaryLine(const char* label, size_t value)
        {
            SummaryLine(label, std::to_string(value));
        }

    } // namespace

    void ProjectHealthPanel::Open(const EditorToolContext&)
    {
        m_Open = true;
        if (m_StartupScene.empty())
            m_StartupScene = EngineInfo::DefaultStartupScene;
        Refresh();
    }

    void ProjectHealthPanel::Refresh()
    {
        AssetDependencyScanOptions options;
        options.ProjectRoot = AssetPath::GetProjectRoot();
        options.StartupAsset = m_StartupScene;
        options.EnableScripts = m_EnableScripts;
        options.IncludeBuiltinAssets = true;
        options.IncludeUnusedAssets = m_IncludeUnusedAssets;

        m_Report = AssetDependencyScanner::BuildReport(options);
        m_SourceReport = ProjectSourceScanner::BuildReport(options.ProjectRoot);
        AssetRegistry::Get().Scan(options.ProjectRoot);
        AssetRegistry::Get().WriteRegistry();
        const size_t sourceSyncIssues = m_SourceReport.MissingFromProject.size()
            + m_SourceReport.StaleProjectEntries.size();
        m_Status = "Last scan: " + std::to_string(m_Report.IncludedAssets.size()) + " assets, "
            + std::to_string(m_Report.MissingReferences.size()) + " missing references, "
            + std::to_string(m_Report.MissingSceneTransitions.size()) + " missing scene transition(s), "
            + std::to_string(sourceSyncIssues) + " source sync issue(s).";
    }

    void ProjectHealthPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!ImGui::Begin("Project Health", &m_Open))
        {
            ImGui::End();
            return;
        }

        ImGui::TextWrapped("Project-wide validation for package dependencies, missing references, and unused assets.");
        InputString("Startup Scene", m_StartupScene, 384);
        ImGui::Checkbox("Enable C# Script Assets", &m_EnableScripts);
        ImGui::SameLine();
        ImGui::Checkbox("Scan Unused Assets", &m_IncludeUnusedAssets);

        if (ImGui::Button("Refresh"))
            Refresh();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_Status.c_str());

        ImGui::Separator();
        DrawSummary();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ProjectHealthTabs"))
        {
            if (ImGui::BeginTabItem("Missing"))
            {
                DrawMissingReferences();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Asset Registry"))
            {
                DrawAssetRegistry();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Packed Assets"))
            {
                DrawAssetList("PackedAssetTable", m_Report.IncludedAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene Links"))
            {
                DrawSceneTransitions();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Unused Assets"))
            {
                DrawAssetList("UnusedAssetTable", m_Report.UnusedAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Parsed Text"))
            {
                DrawAssetList("ParsedTextAssetTable", m_Report.ParsedTextAssets);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Warnings"))
            {
                if (m_Report.Warnings.empty())
                {
                    ImGui::TextDisabled("No warnings.");
                }
                else
                {
                    for (const std::string& warning : m_Report.Warnings)
                        ImGui::BulletText("%s", warning.c_str());
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Source Sync"))
            {
                DrawSourceSync();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void ProjectHealthPanel::DrawSummary() const
    {
        SummaryLine("Packed Assets", m_Report.IncludedAssets.size());
        SummaryLine("Packed Source Size", FormatBytes(m_Report.IncludedBytes));
        SummaryLine("Packable Assets", m_Report.PackableAssetCount);
        SummaryLine("Packable Source Size", FormatBytes(m_Report.PackableBytes));
        SummaryLine("Unused Assets", m_Report.UnusedAssets.size());
        SummaryLine("Parsed Text Assets", m_Report.ParsedTextAssets.size());
        SummaryLine("Scene Transitions", m_Report.SceneTransitions.size());
        SummaryLine("Missing Scene Transitions", m_Report.MissingSceneTransitions.size());
        SummaryLine("Missing References", m_Report.MissingReferences.size());
        SummaryLine("Asset Registry Assets", AssetRegistry::Get().GetAssetCount());
        SummaryLine("Asset Registry References", AssetRegistry::Get().GetReferenceCount());
        SummaryLine("Warnings", m_Report.Warnings.size());
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
            ImGui::TableSetupColumn("Asset");
            ImGui::TableSetupColumn("Refs");
            ImGui::TableSetupColumn("Used By");
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
            ImGui::TextDisabled("No missing asset references found.");
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
            ImGui::TextDisabled("No scene transition commands found in parsed text assets.");
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
            ImGui::TableSetupColumn("Asset");
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
