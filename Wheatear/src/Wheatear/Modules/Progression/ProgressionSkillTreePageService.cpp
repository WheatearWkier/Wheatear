#include "wtpch.h"
#include "ProgressionSkillTreePageService.h"

#include "GameProgress.h"
#include "ProgressionContent.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear::ProgressionSkillTreePageService {

    using SceneQueries::FindEntityByName;

    static constexpr const char* kMagicSwordCoreNodeId = "magic_sword_core";
    static constexpr const char* kMagicSwordLv2NodeId = "magic_sword_lv2";

    struct SkillTreeVisualNode
    {
        std::string Id;
        std::string ParentId;
        glm::vec2 Position = { 0.5f, 0.5f };
    };

    constexpr float kPi = 3.14159265359f;
    struct SkillTreeBranchLayout
    {
        const char* Prefix;
        float BaseDegrees;
        float CurveDegrees;
    };

    constexpr std::array<SkillTreeBranchLayout, 5> kBranchLayouts = {{
        { "ME", -90.0f, 50.0f },
        { "MA", -18.0f, 52.0f },
        { "FU", 54.0f, 52.0f },
        { "MO", 126.0f, 52.0f },
        { "LI", 198.0f, 50.0f },
    }};

    static std::string SkillSafeTag(const std::string& id)
    {
        std::string safe = id;
        for (char& c : safe)
        {
            if (c == '-' || c == ':')
                c = '_';
            else
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return safe;
    }

    static std::string SkillTreeBranchIconPath(const std::string& nodeId)
    {
        const size_t separator = nodeId.find('-');
        if (separator == std::string::npos)
            return {};

        std::string branch = nodeId.substr(0, separator);
        std::transform(branch.begin(), branch.end(), branch.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

        if (branch == "me" || branch == "ma" || branch == "fu" || branch == "mo" || branch == "li")
            return AssetAliasRegistry::Path("progression.skill_tree.root") + "skill_" + branch + ".png";

        return {};
    }

    static int SkillNodeIndex(const std::string& nodeId)
    {
        const size_t separator = nodeId.find('-');
        if (separator == std::string::npos || separator + 1 >= nodeId.size())
            return 0;

        try
        {
            return std::max(std::stoi(nodeId.substr(separator + 1)), 0);
        }
        catch (...)
        {
            return 0;
        }
    }

    static std::vector<const ProgressionContent::SkillNodeDefinition*> CollectBranchNodes(const char* prefix)
    {
        std::vector<const ProgressionContent::SkillNodeDefinition*> nodes;
        const std::string branchPrefix = std::string(prefix) + "-";
        for (const auto& node : ProgressionContent::Get().SkillNodes)
        {
            if (node.Id.rfind(branchPrefix, 0) == 0)
                nodes.push_back(&node);
        }

        std::sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b)
        {
            const int aIndex = SkillNodeIndex(a->Id);
            const int bIndex = SkillNodeIndex(b->Id);
            if (aIndex != bIndex)
                return aIndex < bIndex;
            return a->Id < b->Id;
        });
        return nodes;
    }

    static std::vector<SkillTreeVisualNode> BuildFallbackSkillTreeVisualNodes()
    {
        std::vector<SkillTreeVisualNode> result;
        const auto* core = ProgressionContent::FindSkillNode(kMagicSwordCoreNodeId);
        if (!core)
            return result;

        result.push_back({ core->Id, "", { 0.50f, 0.50f } });
        if (const auto* lv2 = ProgressionContent::FindSkillNode(kMagicSwordLv2NodeId))
            result.push_back({ lv2->Id, core->Id, { 0.50f, 0.36f } });

        for (const auto& layout : kBranchLayouts)
        {
            const auto branchNodes = CollectBranchNodes(layout.Prefix);
            std::string parent = core->Id;
            const size_t count = branchNodes.size();

            for (size_t i = 0; i < count; ++i)
            {
                const auto& node = *branchNodes[i];
                const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
                const float radius = 0.11f + 0.045f * static_cast<float>(i + 1);
                const float angle = (layout.BaseDegrees + layout.CurveDegrees * t) * kPi / 180.0f;
                const float ringOffset = (i % 3 == 0 ? 0.014f : (i % 3 == 1 ? -0.006f : 0.006f));
                const glm::vec2 position = {
                    0.50f + std::cos(angle) * (radius + ringOffset),
                    0.50f + std::sin(angle) * (radius + ringOffset)
                };

                result.push_back({ node.Id, parent, position });
                parent = node.Id;
            }
        }

        return result;
    }

    static std::vector<SkillTreeVisualNode> GetSkillTreeVisualNodes()
    {
        const auto fallbackNodes = BuildFallbackSkillTreeVisualNodes();
        std::unordered_map<std::string, SkillTreeVisualNode> fallbackById;
        fallbackById.reserve(fallbackNodes.size());
        for (const auto& node : fallbackNodes)
            fallbackById[node.Id] = node;

        std::unordered_map<std::string, const ProgressionContent::SkillNodeDefinition*> definitionsById;
        const auto& definitions = ProgressionContent::Get().SkillNodes;
        definitionsById.reserve(definitions.size());
        for (const auto& definition : definitions)
        {
            if (!definition.Id.empty())
                definitionsById[definition.Id] = &definition;
        }

        auto buildNode = [&](const ProgressionContent::SkillNodeDefinition& definition)
        {
            SkillTreeVisualNode visual;
            visual.Id = definition.Id;
            if (auto fallback = fallbackById.find(definition.Id); fallback != fallbackById.end())
            {
                visual.ParentId = fallback->second.ParentId;
                visual.Position = fallback->second.Position;
            }
            else
            {
                visual.Position = { 0.50f, 0.50f };
            }

            if (definition.HasParentId)
                visual.ParentId = definition.ParentId;
            if (definition.HasPosition)
                visual.Position = { definition.PositionX, definition.PositionY };
            return visual;
        };

        std::vector<SkillTreeVisualNode> result;
        result.reserve(definitions.size());
        std::unordered_set<std::string> emitted;

        for (const auto& fallback : fallbackNodes)
        {
            if (auto definition = definitionsById.find(fallback.Id); definition != definitionsById.end())
            {
                result.push_back(buildNode(*definition->second));
                emitted.insert(fallback.Id);
            }
        }

        for (const auto& definition : definitions)
        {
            if (definition.Id.empty() || emitted.find(definition.Id) != emitted.end())
                continue;

            result.push_back(buildNode(definition));
        }

        return result;
    }

    static std::string SkillTreeIconPath(const std::string& nodeId)
    {
        if (nodeId == kMagicSwordCoreNodeId || nodeId == kMagicSwordLv2NodeId)
            return AssetAliasRegistry::Path("progression.skill_tree.magic_sword_core");
        if (std::string branchIcon = SkillTreeBranchIconPath(nodeId); !branchIcon.empty())
            return branchIcon;
        return AssetAliasRegistry::Path("progression.skill_tree.root")
            + "skill_" + SkillSafeTag(nodeId) + ".png";
    }

    static void ApplySkillTreeNodeContent(UISkillTreeNodeView& node)
    {
        if (const auto* definition = ProgressionContent::FindSkillNode(node.Id))
        {
            node.Branch = definition->Branch;
            node.UnlockChapter = definition->UnlockChapter;
            return;
        }

        node.Branch.clear();
        node.UnlockChapter = 1;
    }

    static bool SkillTreeViewNeedsRebuild(const UISkillTreeViewComponent& tree,
        const std::vector<SkillTreeVisualNode>& visualNodes)
    {
        if (tree.Nodes.size() != visualNodes.size())
            return true;

        for (size_t i = 0; i < visualNodes.size(); ++i)
        {
            if (tree.Nodes[i].Id != visualNodes[i].Id
                || tree.Nodes[i].ParentId != visualNodes[i].ParentId
                || std::abs(tree.Nodes[i].Position.x - visualNodes[i].Position.x) > 0.0005f
                || std::abs(tree.Nodes[i].Position.y - visualNodes[i].Position.y) > 0.0005f)
                return true;
        }
        return false;
    }

    static void PopulateSkillTreeViewNodes(UISkillTreeViewComponent& tree,
        const std::vector<SkillTreeVisualNode>& visualNodes)
    {
        tree.Nodes.clear();
        tree.Nodes.reserve(visualNodes.size());
        for (const auto& visualNode : visualNodes)
        {
            UISkillTreeNodeView node;
            node.Id = visualNode.Id;
            node.ParentId = visualNode.ParentId;
            node.Position = visualNode.Position;
            node.IconPath = SkillTreeIconPath(visualNode.Id);
            ApplySkillTreeNodeContent(node);
            tree.Nodes.push_back(node);
        }
    }

    bool SyncView(Scene* scene)
    {
        Entity treeEntity = FindEntityByName(scene, "SkillTree_View");
        if (!treeEntity || !treeEntity.HasComponent<UIWidgetComponent>())
            return false;

        if (!treeEntity.HasComponent<UISkillTreeViewComponent>())
            return false;

        auto& tree = treeEntity.GetComponent<UISkillTreeViewComponent>();

        if (treeEntity.HasComponent<UIPanelComponent>())
            treeEntity.GetComponent<UIPanelComponent>().ClipChildren = true;

        const auto visualNodes = GetSkillTreeVisualNodes();
        if (visualNodes.empty())
            return false;

        if (SkillTreeViewNeedsRebuild(tree, visualNodes))
            PopulateSkillTreeViewNodes(tree, visualNodes);

        auto& state = GameProgress::GetState();
        if (!tree.RuntimeDragging)
        {
            tree.Pan = { state.SkillTreePanX, state.SkillTreePanY };
            tree.ClampPan();
        }

        tree.SelectedNodeId = state.SelectedSkillNodeId;
        tree.CommandPrefix = "progression:select_skill_node:";

        for (auto& node : tree.Nodes)
        {
            node.IconPath = SkillTreeIconPath(node.Id);
            ApplySkillTreeNodeContent(node);
            node.Learned = node.Id == kMagicSwordCoreNodeId
                || (node.Id == kMagicSwordLv2NodeId && state.MagicSwordLevel >= 2)
                || state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
            node.Available = node.UnlockChapter <= state.CurrentChapter;
            node.Locked = !node.Learned;
            node.Selected = node.Id == state.SelectedSkillNodeId;
        }

        return true;
    }
} // namespace Wheatear::ProgressionSkillTreePageService
