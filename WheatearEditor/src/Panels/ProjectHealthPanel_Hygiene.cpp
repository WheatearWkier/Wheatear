#include "wepch.h"
#include "ProjectHealthPanel.h"
#include "ProjectHealthPanelInternal.h"

#include "Assets/AssetRegistry.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace Wheatear {

    using namespace ProjectHealthPanelInternal;

    void ProjectHealthPanel::BuildHygieneCleanupPlan()
    {
        m_HygieneActions.clear();

        const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
        const std::unordered_set<std::string> packedAssets = BuildPathSet(m_Report.IncludedAssets);
        const std::unordered_set<std::string> unusedAssets = BuildPathSet(m_Report.UnusedAssets);
        const std::unordered_map<std::string, std::string> aliasByTarget = BuildAliasByTarget();
        std::unordered_set<std::string> plannedArchiveAssets;

        auto makeArchiveDestination = [&](const std::filesystem::path& asset)
        {
            std::error_code error;
            std::filesystem::path destination = ArchiveDestinationFor(projectRoot, asset);
            std::filesystem::path relative = std::filesystem::relative(destination, projectRoot, error);
            return error ? destination : relative;
        };

        auto assetSize = [&](const std::filesystem::path& asset)
        {
            std::error_code error;
            const uintmax_t size = std::filesystem::file_size(projectRoot / asset, error);
            return error ? static_cast<uintmax_t>(0) : size;
        };

        for (const AssetDuplicateGroup& group : m_HygieneReport.DuplicateGroups)
        {
            if (group.Assets.size() < 2)
                continue;

            std::filesystem::path keep = group.Assets.front();
            for (const std::filesystem::path& asset : group.Assets)
            {
                if (packedAssets.count(NormalizedPathKey(asset)) != 0)
                {
                    keep = asset;
                    break;
                }
            }

            for (const std::filesystem::path& asset : group.Assets)
            {
                const std::string normalized = NormalizedPathKey(asset);
                if (asset == keep || IsArchivedAssetPath(asset))
                    continue;

                const bool unused = unusedAssets.count(normalized) != 0;
                const bool aliased = aliasByTarget.find(normalized) != aliasByTarget.end();

                AssetHygieneAction action;
                action.Type = unused && !aliased
                    ? AssetHygieneActionType::ArchiveDuplicateAsset
                    : AssetHygieneActionType::ReviewDuplicateAsset;
                action.CanApply = action.Type == AssetHygieneActionType::ArchiveDuplicateAsset;
                action.Selected = action.CanApply;
                action.Title = action.CanApply ? "Archive unused duplicate asset" : "Review duplicate before changing references";
                action.Detail = "Keep " + keep.generic_string() + "; duplicate " + asset.generic_string()
                    + (aliased ? " is an alias target." : ".");
                action.SourceAsset = asset;
                action.DestinationAsset = makeArchiveDestination(asset);
                action.SizeBytes = group.SizeBytes;
                plannedArchiveAssets.insert(normalized);
                m_HygieneActions.push_back(std::move(action));
            }
        }

        for (const std::filesystem::path& asset : m_Report.UnusedAssets)
        {
            const std::string normalized = NormalizedPathKey(asset);
            if (plannedArchiveAssets.count(normalized) != 0
                || aliasByTarget.find(normalized) != aliasByTarget.end()
                || IsArchivedAssetPath(asset)
                || !IsBinaryHygieneAsset(asset))
                continue;

            AssetHygieneAction action;
            action.Type = AssetHygieneActionType::ArchiveUnusedAsset;
            action.CanApply = true;
            action.Selected = true;
            action.Title = "Archive unused binary asset";
            action.Detail = "Not included in the current package closure and not targeted by an alias.";
            action.SourceAsset = asset;
            action.DestinationAsset = makeArchiveDestination(asset);
            action.SizeBytes = assetSize(asset);
            m_HygieneActions.push_back(std::move(action));
        }

        std::unordered_set<std::string> replacementKeys;
        for (const HardcodedAssetPathRecord& record : m_HygieneReport.HardcodedAssetPaths)
        {
            const std::string normalized = AssetDependencyScanner::NormalizeAssetReference(record.Reference);
            auto alias = aliasByTarget.find(normalized);
            if (alias == aliasByTarget.end() || alias->second.empty() || alias->second == record.Reference)
                continue;

            const std::string key = record.SourceFile.generic_string() + "|" + record.Reference + "|" + alias->second;
            if (!replacementKeys.insert(key).second)
                continue;

            AssetHygieneAction action;
            action.Type = AssetHygieneActionType::ReplacePathWithAlias;
            action.CanApply = true;
            action.Selected = false;
            action.Title = "Replace hardcoded asset path with alias";
            action.Detail = record.SourceFile.generic_string() + ": " + record.Reference + " -> " + alias->second;
            action.SourceFile = record.SourceFile;
            action.SearchText = record.Reference;
            action.ReplacementText = alias->second;
            m_HygieneActions.push_back(std::move(action));
        }

        const size_t selectedCount = std::count_if(m_HygieneActions.begin(), m_HygieneActions.end(),
            [](const AssetHygieneAction& action) { return action.Selected && action.CanApply; });
        m_HygieneActionStatus = "Generated " + std::to_string(m_HygieneActions.size())
            + " hygiene action(s), " + std::to_string(selectedCount) + " selected.";
    }
    bool ProjectHealthPanel::ApplySelectedHygieneActions()
    {
        const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
        size_t applied = 0;
        size_t failed = 0;
        std::string firstError;

        for (AssetHygieneAction& action : m_HygieneActions)
        {
            if (!action.Selected || !action.CanApply)
                continue;

            std::string error;
            bool ok = false;
            switch (action.Type)
            {
            case AssetHygieneActionType::ArchiveUnusedAsset:
            case AssetHygieneActionType::ArchiveDuplicateAsset:
                ok = MoveAssetToArchive(projectRoot, action, &error);
                break;
            case AssetHygieneActionType::ReplacePathWithAlias:
                ok = ReplaceTextInProjectFile(projectRoot, action, &error);
                break;
            case AssetHygieneActionType::ReviewDuplicateAsset:
                ok = false;
                error = "Review-only duplicate action cannot be applied.";
                break;
            }

            if (ok)
            {
                ++applied;
                action.Selected = false;
            }
            else
            {
                ++failed;
                if (firstError.empty())
                    firstError = error;
            }
        }

        Refresh();
        BuildHygieneCleanupPlan();
        m_HygieneActionStatus = "Applied " + std::to_string(applied) + " hygiene action(s).";
        if (failed > 0)
            m_HygieneActionStatus += " " + std::to_string(failed) + " failed. " + firstError;
        return failed == 0;
    }
    void ProjectHealthPanel::DrawAssetHygiene()
    {
        SummaryLine("Duplicate Groups", m_HygieneReport.DuplicateGroups.size());
        SummaryLine("Duplicate Extra Assets", m_HygieneReport.DuplicateExtraAssetCount);
        SummaryLine("Duplicate Extra Size", FormatBytes(m_HygieneReport.DuplicateExtraBytes));
        SummaryLine("Alias Issues", m_HygieneReport.AliasIssues.size());
        SummaryLine("Hardcoded Asset Paths", m_HygieneReport.HardcodedAssetPaths.size());
        SummaryLine(EditorLocale::Text("Unused Assets", "未使用资源"), m_Report.UnusedAssets.size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!ImGui::BeginTabBar("AssetHygieneTabs"))
            return;

        if (ImGui::BeginTabItem(EditorLocale::Text("Cleanup Plan", "清理计划")))
        {
            DrawHygieneCleanupPlan();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(EditorLocale::Text("Duplicates", "重复资源")))
        {
            if (m_HygieneReport.DuplicateGroups.empty())
            {
                EditorWidgets::EmptyState("No duplicate binary asset hashes found.", "Packable binary assets with matching size and content hash are unique.");
            }
            else
            {
                for (size_t i = 0; i < m_HygieneReport.DuplicateGroups.size(); ++i)
                {
                    const AssetDuplicateGroup& group = m_HygieneReport.DuplicateGroups[i];
                    const std::string label = "Group " + std::to_string(i + 1)
                        + "  |  " + std::to_string(group.Assets.size()) + " files"
                        + "  |  " + FormatBytes(group.SizeBytes)
                        + " each  |  " + group.Hash;
                    if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        for (const auto& asset : group.Assets)
                            ImGui::BulletText("%s", asset.generic_string().c_str());
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(EditorLocale::Text("Largest Files", "最大文件")))
        {
            if (m_HygieneReport.LargestAssets.empty())
            {
                ImGui::TextDisabled("No packable binary assets found.");
            }
            else
            {
                const size_t maxRows = std::min<size_t>(m_HygieneReport.LargestAssets.size(), 100);
                if (m_HygieneReport.LargestAssets.size() > maxRows)
                    ImGui::TextDisabled("Showing largest %zu of %zu packable binary assets.", maxRows, m_HygieneReport.LargestAssets.size());

                if (ImGui::BeginTable("AssetHygieneLargeFileTable", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                    ImVec2(0, 420)))
                {
                    ImGui::TableSetupColumn("Size");
                    ImGui::TableSetupColumn(EditorLocale::Text("Packed", "已打包"));
                    ImGui::TableSetupColumn("Unused");
                    ImGui::TableSetupColumn(EditorLocale::Text("Asset", "资源"));
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < maxRows; ++i)
                    {
                        const AssetSizeRecord& record = m_HygieneReport.LargestAssets[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(FormatBytes(record.SizeBytes).c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(record.Packed ? "yes" : "no");
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(record.Unused ? "yes" : "no");
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextWrapped("%s", record.Asset.generic_string().c_str());
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(EditorLocale::Text("Aliases", "别名")))
        {
            if (m_HygieneReport.AliasIssues.empty())
            {
                EditorWidgets::EmptyState("No alias target issues found.", "All loaded content manifest aliases resolve into the current project.");
            }
            else if (ImGui::BeginTable("AssetHygieneAliasIssueTable", 3,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                ImVec2(0, 420)))
            {
                ImGui::TableSetupColumn(EditorLocale::Text("Alias", "别名"));
                ImGui::TableSetupColumn(EditorLocale::Text("Target", "目标"));
                ImGui::TableSetupColumn(EditorLocale::Text("Issue", "问题"));
                ImGui::TableHeadersRow();

                for (const AssetAliasIssue& issue : m_HygieneReport.AliasIssues)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", issue.Alias.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", issue.Target.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextWrapped("%s", issue.Issue.c_str());
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(EditorLocale::Text("Source Paths", "源码路径")))
        {
            if (m_HygieneReport.HardcodedAssetPaths.empty())
            {
                EditorWidgets::EmptyState("No hardcoded asset path references found.", "Source and tool files did not contain literal assets/... paths.");
            }
            else
            {
                const size_t maxRows = std::min<size_t>(m_HygieneReport.HardcodedAssetPaths.size(), 500);
                if (m_HygieneReport.HardcodedAssetPaths.size() > maxRows)
                    ImGui::TextDisabled("Showing first %zu of %zu hardcoded asset path references.", maxRows, m_HygieneReport.HardcodedAssetPaths.size());

                if (ImGui::BeginTable("AssetHygieneSourcePathTable", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                    ImVec2(0, 420)))
                {
                    ImGui::TableSetupColumn(EditorLocale::Text("Source File", "源文件"));
                    ImGui::TableSetupColumn("Reference");
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < maxRows; ++i)
                    {
                        const HardcodedAssetPathRecord& record = m_HygieneReport.HardcodedAssetPaths[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextWrapped("%s", record.SourceFile.generic_string().c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextWrapped("%s", record.Reference.c_str());
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    void ProjectHealthPanel::DrawHygieneCleanupPlan()
    {
        if (ImGui::Button(EditorLocale::Text("Generate Cleanup Plan", "生成清理计划")))
            BuildHygieneCleanupPlan();

        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Select Safe Archive Actions", "选择安全归档项")))
        {
            for (AssetHygieneAction& action : m_HygieneActions)
            {
                action.Selected = action.CanApply
                    && (action.Type == AssetHygieneActionType::ArchiveUnusedAsset
                        || action.Type == AssetHygieneActionType::ArchiveDuplicateAsset);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Clear Selection", "清空选择")))
        {
            for (AssetHygieneAction& action : m_HygieneActions)
                action.Selected = false;
        }

        const size_t selectedCount = std::count_if(m_HygieneActions.begin(), m_HygieneActions.end(),
            [](const AssetHygieneAction& action) { return action.Selected && action.CanApply; });
        uintmax_t selectedBytes = 0;
        for (const AssetHygieneAction& action : m_HygieneActions)
        {
            if (action.Selected
                && action.CanApply
                && (action.Type == AssetHygieneActionType::ArchiveUnusedAsset
                    || action.Type == AssetHygieneActionType::ArchiveDuplicateAsset))
                selectedBytes += action.SizeBytes;
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(selectedCount == 0);
        if (ImGui::Button(EditorLocale::Text("Apply Selected", "应用选中项")))
            ImGui::OpenPopup("Apply Hygiene Actions?");
        ImGui::EndDisabled();

        if (ImGui::BeginPopupModal("Apply Hygiene Actions?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Apply %zu selected action(s)? Archive actions move files into assets/.wheatear/archive. Alias actions edit text files in place.", selectedCount);
            ImGui::Spacing();
            if (ImGui::Button("Apply"))
            {
                ApplySelectedHygieneActions();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        EditorWidgets::InlineStatus(m_HygieneActionStatus.empty()
                ? "Generate a cleanup plan from the latest scan before applying changes."
                : m_HygieneActionStatus,
            m_HygieneActionStatus.find("failed") != std::string::npos ? EditorWidgets::StatusKind::Error : EditorWidgets::StatusKind::Info);
        SummaryLine("Plan Actions", m_HygieneActions.size());
        SummaryLine("Selected Actions", selectedCount);
        SummaryLine("Selected Archive Size", FormatBytes(selectedBytes));

        if (m_HygieneActions.empty())
        {
            EditorWidgets::EmptyState("No cleanup plan generated.", "Click Generate Cleanup Plan after refreshing Project Health.");
            return;
        }

        if (ImGui::BeginTable("AssetHygieneCleanupPlanTable", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 440)))
        {
            ImGui::TableSetupColumn("Apply", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 88.0f);
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Destination / Replacement");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_HygieneActions.size(); ++i)
            {
                AssetHygieneAction& action = m_HygieneActions[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::BeginDisabled(!action.CanApply);
                ImGui::Checkbox("##selected", &action.Selected);
                ImGui::EndDisabled();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(HygieneActionTypeName(action.Type));

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(action.SizeBytes > 0 ? FormatBytes(action.SizeBytes).c_str() : "-");

                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped("%s", action.Title.c_str());
                if (!action.Detail.empty())
                    ImGui::TextDisabled("%s", action.Detail.c_str());

                ImGui::TableSetColumnIndex(4);
                if (!action.SourceAsset.empty())
                    ImGui::TextWrapped("%s", action.SourceAsset.generic_string().c_str());
                else
                    ImGui::TextWrapped("%s", action.SourceFile.generic_string().c_str());

                ImGui::TableSetColumnIndex(5);
                if (!action.DestinationAsset.empty())
                    ImGui::TextWrapped("%s", action.DestinationAsset.generic_string().c_str());
                else
                    ImGui::TextWrapped("%s -> %s", action.SearchText.c_str(), action.ReplacementText.c_str());

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
} // namespace Wheatear
