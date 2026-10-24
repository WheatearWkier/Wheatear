#include "wtpch.h"
#include "ContentValidation.h"

#include "Wheatear/Gameplay/Behavior/BehaviorAnalysis.h"
#include "Wheatear/Gameplay/Behavior/BehaviorBuiltinPrimitives.h"
#include "Wheatear/Gameplay/Behavior/BehaviorCompiler.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelScript.h"
#include "Wheatear/Scripting/EventScript.h"
#include "Wheatear/Utils/StringUtils.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_set>

namespace Wheatear {

    namespace {

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        void AddIssue(ContentValidationReport& report,
            const std::filesystem::path& asset,
            int line,
            ContentIssueSeverity severity,
            const std::string& message)
        {
            report.Issues.push_back({ asset, line, severity, message });
        }

        void ValidateEventScript(const std::filesystem::path& path,
            ContentValidationReport& report)
        {
            const EventScript script = EventScript::FromFile(path);
            for (const EventScriptIssue& issue : script.Validate())
            {
                AddIssue(report, path, issue.SourceLine,
                    issue.Error ? ContentIssueSeverity::Error
                                : ContentIssueSeverity::Warning,
                    issue.Message);
            }
        }

        void ValidateVisualNovel(const std::filesystem::path& path,
            ContentValidationReport& report)
        {
            const VisualNovelScript script = VisualNovelScript::FromFile(path);
            for (const VisualNovelIssue& issue : script.GetIssues())
            {
                AddIssue(report, path, issue.SourceLine,
                    issue.Error ? ContentIssueSeverity::Error
                                : ContentIssueSeverity::Warning,
                    issue.Message);
            }
        }

        void ValidateBehavior(const std::filesystem::path& path,
            const YAML::Node& root,
            ContentValidationReport& report)
        {
            // The editor/CLI may validate before gameplay modules have run.
            static std::once_flag builtinsOnce;
            std::call_once(builtinsOnce, []()
            {
                WAO::RegisterBuiltinBehaviorPrimitives();
            });

            WAO::BehaviorProgram program;
            std::string error;
            if (!WAO::BehaviorCompiler::Compile(root, path, &program, &error))
            {
                AddIssue(report, path, 0, ContentIssueSeverity::Error, error);
                return;
            }

            for (const WAO::BehaviorAnalysisIssue& issue
                : WAO::AnalyzeProgram(program))
            {
                AddIssue(report, path, 0,
                    issue.Error ? ContentIssueSeverity::Error
                                : ContentIssueSeverity::Warning,
                    "behavior '" + issue.BehaviorId + "' node '" + issue.NodeId
                        + "': " + issue.Message);
            }
        }

        bool IsKnownEffectType(const std::string& type)
        {
            static const std::unordered_set<std::string> known = {
                "damage", "heal", "modifyattribute", "modify_attribute",
                "addstate", "add_state", "removestate", "remove_state",
                "startcooldown", "start_cooldown", "consumeresource",
                "consume_resource", "launch", "hitstun", "hit_stun",
                "emitsignal", "emit_signal", "signal"
            };
            const std::string normalized = ToLowerAscii(
                Wheatear::StringUtils::Trim(type));
            return known.count(normalized) > 0;
        }

        void ValidateActionYaml(const std::filesystem::path& path,
            const YAML::Node& root,
            ContentValidationReport& report)
        {
            const YAML::Node actions = root["actions"];
            if (!actions || !actions.IsSequence())
            {
                AddIssue(report, path, 0, ContentIssueSeverity::Warning,
                    "action manifest has no 'actions' sequence");
                return;
            }

            std::unordered_set<std::string> ids;
            int index = 0;
            for (const YAML::Node& action : actions)
            {
                ++index;
                const YAML::Node idNode = action["id"] ? action["id"] : action["Id"];
                const std::string id = idNode && idNode.IsScalar()
                    ? idNode.as<std::string>()
                    : std::string();
                if (id.empty())
                {
                    AddIssue(report, path, 0, ContentIssueSeverity::Error,
                        "action #" + std::to_string(index) + " has no id");
                }
                else if (!ids.insert(id).second)
                {
                    AddIssue(report, path, 0, ContentIssueSeverity::Error,
                        "duplicate action id '" + id + "'");
                }

                const YAML::Node effects = action["effects"];
                if (!effects)
                    continue;
                if (!effects.IsSequence())
                {
                    AddIssue(report, path, 0, ContentIssueSeverity::Error,
                        "action '" + id + "' has a non-sequence 'effects' value");
                    continue;
                }

                int effectIndex = 0;
                for (const YAML::Node& effect : effects)
                {
                    ++effectIndex;
                    const YAML::Node typeNode = effect["type"] ? effect["type"] : effect["Type"];
                    const std::string type = typeNode && typeNode.IsScalar()
                        ? typeNode.as<std::string>()
                        : std::string();
                    const YAML::Node customNode = effect["customType"]
                        ? effect["customType"]
                        : effect["custom"];
                    const std::string customType = customNode && customNode.IsScalar()
                        ? customNode.as<std::string>()
                        : std::string();

                    const std::string normalizedType = ToLowerAscii(type);
                    const bool explicitNone = normalizedType.empty()
                        || normalizedType == "none" || normalizedType == "custom";
                    if (!customType.empty())
                    {
                        // CustomType takes precedence at runtime; Type is only
                        // a fallback and may legitimately be "none".
                    }
                    else if (!explicitNone && !IsKnownEffectType(type))
                    {
                        AddIssue(report, path, 0, ContentIssueSeverity::Error,
                            "action '" + id + "' effect #" + std::to_string(effectIndex)
                                + " has unknown type '" + type + "'");
                    }
                    if (explicitNone && customType.empty())
                    {
                        AddIssue(report, path, 0, ContentIssueSeverity::Warning,
                            "action '" + id + "' effect #" + std::to_string(effectIndex)
                                + " has neither type nor customType");
                    }

                    const YAML::Node formulaNode = effect["formula"];
                    if (formulaNode && formulaNode.IsScalar())
                    {
                        std::string error;
                        const std::string formula = formulaNode.as<std::string>();
                        if (!Logic::Validate(formula, &error))
                        {
                            AddIssue(report, path, 0, ContentIssueSeverity::Error,
                                "action '" + id + "' effect #" + std::to_string(effectIndex)
                                    + " has invalid formula '" + formula + "': " + error);
                        }
                    }
                }
            }
        }

        void ValidateYamlDocument(const std::filesystem::path& path,
            ContentValidationReport& report)
        {
            YAML::Node root;
            try
            {
                root = YAML::LoadFile(path.string());
            }
            catch (const YAML::Exception& exception)
            {
                AddIssue(report, path, exception.mark.line + 1,
                    ContentIssueSeverity::Error,
                    std::string("YAML parse error: ") + exception.msg);
                return;
            }

            try
            {
                if (!root.IsMap())
                {
                    AddIssue(report, path, 0, ContentIssueSeverity::Error,
                        "YAML root must be a map");
                    return;
                }

                if (root["behaviors"])
                {
                    ValidateBehavior(path, root, report);
                }
                else if (ToLowerAscii(path.extension().string()) == ".wtb")
                {
                    AddIssue(report, path, 0, ContentIssueSeverity::Error,
                        "behavior file has no 'behaviors' sequence");
                }
                else if (root["actions"]
                    && path.generic_string().find("/gameplay/") != std::string::npos)
                {
                    ValidateActionYaml(path, root, report);
                }
            }
            catch (const YAML::Exception& exception)
            {
                AddIssue(report, path, exception.mark.line + 1,
                    ContentIssueSeverity::Error,
                    std::string("YAML structure error: ") + exception.msg);
            }
        }

    } // namespace

    bool ContentValidationReport::HasErrors() const
    {
        return std::any_of(Issues.begin(), Issues.end(),
            [](const ContentIssue& issue)
            {
                return issue.Severity == ContentIssueSeverity::Error;
            });
    }

    size_t ContentValidationReport::ErrorCount() const
    {
        return static_cast<size_t>(std::count_if(Issues.begin(), Issues.end(),
            [](const ContentIssue& issue)
            {
                return issue.Severity == ContentIssueSeverity::Error;
            }));
    }

    size_t ContentValidationReport::WarningCount() const
    {
        return static_cast<size_t>(std::count_if(Issues.begin(), Issues.end(),
            [](const ContentIssue& issue)
            {
                return issue.Severity == ContentIssueSeverity::Warning;
            }));
    }

    ContentValidationReport ContentValidator::ValidateFile(
        const std::filesystem::path& path)
    {
        ContentValidationReport report;
        if (!std::filesystem::exists(path))
        {
            AddIssue(report, path, 0, ContentIssueSeverity::Error,
                "asset does not exist");
            return report;
        }
        if (!std::filesystem::is_regular_file(path))
            return report;

        ++report.FilesChecked;
        const std::string extension = ToLowerAscii(path.extension().string());
        if (extension == ".wts")
            ValidateEventScript(path, report);
        else if (extension == ".vn")
            ValidateVisualNovel(path, report);
        else if (extension == ".wtb" || extension == ".yaml" || extension == ".yml")
            ValidateYamlDocument(path, report);
        return report;
    }

    ContentValidationReport ContentValidator::ValidateDirectory(
        const std::filesystem::path& path)
    {
        ContentValidationReport report;
        if (!std::filesystem::exists(path))
        {
            AddIssue(report, path, 0, ContentIssueSeverity::Error,
                "content directory does not exist");
            return report;
        }
        if (!std::filesystem::is_directory(path))
        {
            AddIssue(report, path, 0, ContentIssueSeverity::Error,
                "content validation target is not a directory");
            return report;
        }

        std::error_code error;
        std::unordered_map<std::string, std::filesystem::path> actionIds;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
            path, std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (error)
                break;
            if (!entry.is_regular_file(error))
                continue;

            const std::string extension = ToLowerAscii(entry.path().extension().string());
            if (extension != ".wts" && extension != ".vn"
                && extension != ".wtb" && extension != ".yaml"
                && extension != ".yml")
                continue;

            const ContentValidationReport fileReport = ValidateFile(entry.path());
            report.FilesChecked += fileReport.FilesChecked;
            report.Issues.insert(report.Issues.end(),
                fileReport.Issues.begin(), fileReport.Issues.end());

            if ((extension == ".yaml" || extension == ".yml")
                && entry.path().generic_string().find("/gameplay/") != std::string::npos)
            {
                try
                {
                    const YAML::Node root = YAML::LoadFile(entry.path().string());
                    const YAML::Node actions = root.IsMap() ? root["actions"] : YAML::Node();
                    if (actions && actions.IsSequence())
                    {
                        for (const YAML::Node& action : actions)
                        {
                            if (!action || !action.IsMap() || !action["id"])
                                continue;
                            const std::string id = action["id"].as<std::string>();
                            if (id.empty())
                                continue;
                            const auto [it, inserted] = actionIds.emplace(id, entry.path());
                            if (!inserted && it->second != entry.path())
                            {
                                AddIssue(report, entry.path(), 0,
                                    ContentIssueSeverity::Error,
                                    "duplicate action id '" + id + "' also defined in "
                                        + it->second.generic_string());
                            }
                        }
                    }
                }
                catch (const YAML::Exception&)
                {
                    // The file-level validator already reports the parse error.
                }
            }
        }

        return report;
    }

} // namespace Wheatear
