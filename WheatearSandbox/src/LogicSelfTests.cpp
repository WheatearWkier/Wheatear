#include "Wheatear/Assets/ContentValidation.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"
#include "Wheatear/Gameplay/Action/EffectFormula.h"
#include "Wheatear/Logic/LogicCommandRegistry.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelRuntime.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelScript.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scripting/EventScript.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

    int g_Failures = 0;

    void Check(bool condition, const char* name)
    {
        if (condition)
        {
            std::cout << "[PASS] " << name << '\n';
            return;
        }

        std::cerr << "[FAIL] " << name << '\n';
        ++g_Failures;
    }

    void CheckNear(double actual, double expected, const char* name)
    {
        Check(std::abs(actual - expected) <= 0.0001, name);
    }

    void CheckText(const std::string& actual, const std::string& expected,
        const char* name)
    {
        if (actual == expected)
        {
            std::cout << "[PASS] " << name << '\n';
            return;
        }

        std::cerr << "[FAIL] " << name << " (expected '" << expected
            << "', got '" << actual << "')\n";
        ++g_Failures;
    }

} // namespace

int RunLogicSelfTests()
{
    using namespace Wheatear::Logic;

    // Legacy .wts / .vn condition syntax normalizes into the shared grammar.
    CheckText(NormalizeCondition("always"), "1", "normalize always");
    CheckText(NormalizeCondition("never"), "0", "normalize never");
    CheckText(NormalizeCondition("flag CH01"), "flag:CH01", "normalize flag");
    CheckText(NormalizeCondition("not flag CH01"), "not (flag:CH01)",
        "normalize not flag");
    CheckText(NormalizeCondition("material iron >= 3"), "material:iron >= 3",
        "normalize material comparison");
    CheckText(NormalizeCondition("material iron"), "material:iron > 0",
        "normalize material presence");
    CheckText(NormalizeCondition("chapter >= 2"), "chapter >= 2",
        "normalize leaves expressions untouched");
    CheckText(NormalizeCondition("flag A and flag B"), "flag:A and flag:B",
        "normalize multiple predicates");
    CheckText(NormalizeCondition("always and flag A"), "1 and flag:A",
        "normalize always without dropping the rest");

    // Arithmetic, comparisons, boolean logic and functions.
    EvalContext context;
    context.Resolve = [](const std::string& name)
    {
        if (name == "atk") return 12.0;
        if (name == "flag:CH01") return 1.0;
        if (name == "flag:CH02") return 0.0;
        return 0.0;
    };

    double value = 0.0;
    std::string error;
    Check(Evaluate("2 + 3 * 4", context, &value, &error), "evaluate precedence");
    CheckNear(value, 14.0, "precedence result");
    Check(Evaluate("(2 + 3) * 4", context, &value, &error), "evaluate parentheses");
    CheckNear(value, 20.0, "parentheses result");
    Check(Evaluate("clamp(atk * 2, 0, 20)", context, &value, &error),
        "evaluate function chain");
    CheckNear(value, 20.0, "clamp result");
    Check(Evaluate("if(flag:CH01, 5, 7)", context, &value, &error),
        "evaluate conditional function");
    CheckNear(value, 5.0, "conditional result");

    bool boolean = false;
    Check(EvaluateBool("flag:CH01 and not flag:CH02", context, &boolean, &error),
        "evaluate boolean expression");
    Check(boolean, "boolean result");
    Check(EvaluateBool("flag:CH02 or atk > 10", context, &boolean, &error),
        "evaluate or comparison");
    Check(boolean, "or comparison result");

    // Malformed input must be rejected instead of silently accepted.
    Check(!Evaluate("1 +", context, &value, &error), "reject trailing operator");
    Check(!Evaluate("(1 + 2", context, &value, &error), "reject missing paren");
    Check(!Evaluate("1 $ 2", context, &value, &error), "reject invalid character");
    Check(!Evaluate("1.2.3", context, &value, &error), "reject malformed number");
    Check(!Evaluate(".", context, &value, &error), "reject bare decimal point");
    Check(!Validate("min(1, 2", &error), "validate rejects missing paren");
    Check(Validate("max(1, 2) + atk", &error), "validate accepts expression");

    // Local > scene > persistent > gameplay predicate.
    VariableScope scope;
    scope.SetPersistent("chapter", 1.0);
    scope.SetPersistent("shared", 1.0);
    scope.SetScene("chapter", 2.0);
    scope.SetScene("shared", 2.0);
    scope.SetLocal("chapter", 3.0);
    const EvalContext layered = scope.MakeContext();
    Check(Evaluate("chapter", layered, &value, &error), "scope evaluate");
    CheckNear(value, 3.0, "local scope wins");
    Check(Evaluate("shared", layered, &value, &error), "scope evaluate scene");
    CheckNear(value, 2.0, "scene scope wins over persistent");
    Check(Evaluate("flag:CH01", layered, &value, &error), "scope gameplay fallback");
    CheckNear(value, 0.0, "gameplay fallback resolves safely");

    // WAO effect formulas share the same evaluator.
    Wheatear::WAO::AttributeStore attributes;
    attributes.Set("atk", 10.0f);
    attributes.Set("def", 3.0f);
    CheckNear(Wheatear::WAO::EvaluateEffectFormula("atk * 1.5 - def", attributes, 0.0f),
        12.0, "wao formula evaluates");
    CheckNear(Wheatear::WAO::EvaluateEffectFormula("bad +", attributes, 4.0f),
        4.0, "wao formula keeps fallback on error");
    Check(Wheatear::WAO::ValidateEffectFormula("max(atk, def)", &error),
        "wao formula validates");
    Check(!Wheatear::WAO::ValidateEffectFormula("max(atk", &error),
        "wao formula rejects malformed input");

    // Command catalog validation is shared by .wts validation and tools.
    Check(Wheatear::CommandBus::ValidateCommand("scene:assets/scenes/Main.wt"),
        "built-in scene command validates");
    Check(Wheatear::CommandBus::ValidateCommand("vn:start"),
        "gameplay vn command validates");
    Check(!Wheatear::CommandBus::ValidateCommand("bogus:value", &error),
        "unknown command prefix is rejected");
    Check(!error.empty(), "unknown command reports an error");

    {
        CommandDescriptor descriptor;
        descriptor.Name = "arcade:";
        descriptor.Category = "Custom";
        CommandRegistry::Register(descriptor);
        Check(Wheatear::CommandBus::ValidateCommand("arcade:start"),
            "custom prefix with trailing colon validates");
        Check(Wheatear::CommandBus::ValidateCommand("ARCADE:start"),
            "command prefix matching is case-insensitive");
    }

    // .wts structural validation catches failures that used to be silent.
    {
        const Wheatear::EventScript script = Wheatear::EventScript::FromString(
            "event first:\n"
            "  scene:assets/scenes/Main.wt\n"
            "  if flag:CH01\n"
            "  bogus:command\n"
            "end\n"
            "\n"
            "event first:\n"
            "  wait 1\n"
            "end\n");
        const auto issues = script.Validate();
        const auto hasError = [&issues](const std::string& needle)
        {
            return std::any_of(issues.begin(), issues.end(),
                [&needle](const Wheatear::EventScriptIssue& issue)
                {
                    return issue.Error
                        && issue.Message.find(needle) != std::string::npos;
                });
        };
        Check(hasError("duplicate event"), "wts validation detects duplicate events");
        Check(hasError("missing a matching endif"), "wts validation detects missing endif");
        Check(hasError("Unknown command prefix"), "wts validation detects unknown command");

        const Wheatear::EventScript emptyEvent = Wheatear::EventScript::FromString(
            "event on_start:\n"
            "  event:\n"
            "end\n");
        bool emptyEventError = false;
        for (const Wheatear::EventScriptIssue& issue : emptyEvent.Validate())
            emptyEventError |= issue.Error
                && issue.Message.find("non-empty event name") != std::string::npos;
        Check(emptyEventError, "wts validation rejects empty event call");
    }

    // .vn validation catches dangling targets and accepts multi-token gates.
    {
        const Wheatear::VisualNovelScript script = Wheatear::VisualNovelScript::FromString(
            "@label start\n"
            "@choice prompt:Pick | Go -> ending if gold >= 5 | Bad -> missing\n"
            "@if flag:CH01 -> nowhere\n"
            "Mira: Hello.\n");
        const auto& issues = script.GetIssues();
        const auto hasError = [&issues](const std::string& needle)
        {
            return std::any_of(issues.begin(), issues.end(),
                [&needle](const Wheatear::VisualNovelIssue& issue)
                {
                    return issue.Error
                        && issue.Message.find(needle) != std::string::npos;
                });
        };
        Check(hasError("'missing'"), "vn validation detects dangling choice target");
        Check(hasError("'nowhere'"), "vn validation detects dangling jump target");
        Check(issues.size() >= 2, "vn validation reports line-located issues");

        const auto& lines = script.GetLines();
        const auto choice = std::find_if(lines.begin(), lines.end(),
            [](const Wheatear::VisualNovelLine& line)
            {
                return line.Type == Wheatear::VisualNovelLineType::Choice;
            });
        Check(choice != lines.end(), "vn choice parsed");
        if (choice != lines.end())
        {
            Check(choice->Choices.size() == 2, "vn choice options parsed");
            if (choice->Choices.size() == 2)
            {
                Check(choice->Choices[0].TargetLabel == "ending",
                    "vn multi-token gate keeps label");
                Check(choice->Choices[0].RequiredCondition == "gold >= 5",
                    "vn multi-token gate keeps full condition");
            }
        }
    }

    // Quoted choice text may contain separators and escaped newlines.
    {
        const Wheatear::VisualNovelScript script = Wheatear::VisualNovelScript::FromString(
            "@label start\n"
            "@choice prompt:\"Pick | one\" | \"say | hi\" -> \"ending\" | \"line\\nbreak\" -> ending\n"
            "@label ending\n"
            "Mira: done\n");
        const auto& lines = script.GetLines();
        Check(lines.size() >= 2 && lines[0].Type == Wheatear::VisualNovelLineType::Choice,
            "vn quoted choice parsed");
        if (!lines.empty() && lines[0].Type == Wheatear::VisualNovelLineType::Choice)
        {
            Check(lines[0].Text == "Pick | one", "vn quoted prompt keeps separator");
            Check(lines[0].Choices.size() == 2, "vn quoted options parsed");
            if (lines[0].Choices.size() == 2)
            {
                Check(lines[0].Choices[0].Text == "say | hi",
                    "vn quoted option keeps separator");
                Check(lines[0].Choices[0].TargetLabel == "ending",
                    "vn quoted option target");
                Check(lines[0].Choices[1].Text == "line\nbreak",
                    "vn escaped newline restored");
            }
        }
        Check(script.GetIssues().empty(), "vn quoted choice validates");
    }

    // Apostrophes must not be mistaken for quote delimiters.
    {
        const Wheatear::VisualNovelScript script = Wheatear::VisualNovelScript::FromString(
            "@label start\n"
            "@choice Don't go -> leave | Go -> start\n"
            "@label leave\n"
            "Mira: bye\n");
        const auto& lines = script.GetLines();
        Check(!lines.empty() && lines[0].Type == Wheatear::VisualNovelLineType::Choice,
            "vn apostrophe choice parsed");
        if (!lines.empty() && lines[0].Type == Wheatear::VisualNovelLineType::Choice)
        {
            Check(lines[0].Choices.size() == 2, "vn apostrophe keeps both options");
            if (lines[0].Choices.size() == 2)
                Check(lines[0].Choices[0].Text == "Don't go",
                    "vn apostrophe stays in choice text");
        }
    }

    {
        const Wheatear::VisualNovelScript script =
            Wheatear::VisualNovelScript::FromString("@choice prompt:Pick\n");
        bool found = false;
        for (const Wheatear::VisualNovelIssue& issue : script.GetIssues())
            found |= issue.Error && issue.Message.find("prompt") != std::string::npos;
        Check(found, "vn prompt without separator is rejected");
    }

    // @set uses the shared expression language.
    {
        Wheatear::VisualNovelRuntime runtime;
        runtime.SetScript(Wheatear::VisualNovelScript::FromString(
            "@set gold = 2\n"
            "@set gold = gold * 3 + 4\n"
            "Mira: ready\n"));
        CheckNear(runtime.GetVariable("gold"), 10.0, "vn set evaluates expression");
    }

    // Nested .wts events use a per-component stack instead of overwriting the
    // parent frame, and a self-recursive event is rejected.
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "wheatear_event_stack_test.wts";
        {
            std::ofstream file(path, std::ios::trunc);
            file << "event parent:\n"
                    "  event:child\n"
                    "  wait 0.1\n"
                    "end\n"
                    "event child:\n"
                    "  wait 0.05\n"
                    "  event:child\n"
                    "end\n";
        }

        Wheatear::CommandBus::ClearQueuedCommands();
        Wheatear::Scene scene;
        auto entity = scene.CreateEntity("Flow");
        auto& component = entity.AddComponent<Wheatear::EventScriptComponent>();
        component.ScriptPath = path.string();
        component.StartEvent = "parent";
        component.RunOnStart = true;
        component.RunOnce = false;

        Wheatear::EventScriptSystem system;
        system.OnRuntimeStart(&scene);
        size_t maxDepth = 0;
        for (int i = 0; i < 120; ++i)
        {
            system.OnUpdateRuntime(&scene, Wheatear::Timestep(1.0f / 60.0f));
            maxDepth = std::max(maxDepth, component.RuntimeFrames.size());
        }

        Check(maxDepth >= 2, "wts nested event pushes a stack frame");
        Check(maxDepth <= 3, "wts recursive self event is bounded");
        Check(component.RuntimeFrames.empty(), "wts event stack unwinds");
        Check(component.RuntimeCompleted, "wts event completes after stack unwinds");

        Wheatear::CommandBus::ClearQueuedCommands();
        std::filesystem::remove(path);
    }

    // VNSTATE 2 stores a label + offset so save games survive script edits.
    {
        const std::filesystem::path savePath =
            std::filesystem::temp_directory_path() / "wheatear_vn_v2_test.vnstate";
        const Wheatear::VisualNovelScript script = Wheatear::VisualNovelScript::FromString(
            "@label start\n"
            "@choice prompt:Pick | Go -> start | Stay -> start\n"
            "Mira: first\n"
            "Mira: second\n");

        Wheatear::VisualNovelRuntime saved;
        saved.SetScript(script);
        saved.SkipWaitingChoice();
        Check(saved.SaveState(savePath), "vn save v2 writes");

        Wheatear::VisualNovelRuntime loaded;
        loaded.SetScript(script);
        Check(loaded.LoadState(savePath), "vn save v2 loads");
        Check(loaded.GetCurrentLineIndex() == 1,
            "vn save v2 restores label-relative position");
        std::filesystem::remove(savePath);
    }

    // Hot reload resumes at the current offset instead of replaying @set from
    // the label start.
    {
        const Wheatear::VisualNovelScript script = Wheatear::VisualNovelScript::FromString(
            "@label start\n"
            "@set gold = gold + 5\n"
            "Mira: first\n"
            "Mira: second\n");
        Wheatear::VisualNovelRuntime runtime;
        runtime.SetScript(script);
        runtime.Advance();
        runtime.Advance();
        CheckNear(runtime.GetVariable("gold"), 5.0, "vn hot reload starts at expected value");
        runtime.SetScriptPreservingPosition(script);
        CheckNear(runtime.GetVariable("gold"), 5.0,
            "vn hot reload does not replay @set");
    }

    // Content validation must report malformed/scalar YAML instead of throwing.
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "wheatear_scalar_yaml_test.yaml";
        {
            std::ofstream file(path, std::ios::trunc);
            file << "hello\n";
        }
        bool threw = false;
        bool reported = false;
        try
        {
            const Wheatear::ContentValidationReport report =
                Wheatear::ContentValidator::ValidateFile(path);
            reported = report.HasErrors();
        }
        catch (...)
        {
            threw = true;
        }
        Check(!threw, "content validation does not throw on scalar YAML");
        Check(reported, "content validation reports scalar YAML root");
        std::filesystem::remove(path);
    }

    // Sibling event requests are dispatched in author order (one per frame per
    // component) instead of LIFO on the stack.
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "wheatear_event_order_test.wts";
        {
            std::ofstream file(path, std::ios::trunc);
            file << "event a:\n  wait 5\nend\n"
                    "event b:\n  wait 5\nend\n";
        }

        Wheatear::CommandBus::ClearQueuedCommands();
        Wheatear::Scene scene;
        auto entity = scene.CreateEntity("Flow");
        auto& component = entity.AddComponent<Wheatear::EventScriptComponent>();
        component.ScriptPath = path.string();
        component.RunOnStart = false;
        component.RunOnce = false;

        Wheatear::CommandBus::QueueEventCommand({ 0, "a" });
        Wheatear::CommandBus::QueueEventCommand({ 0, "b" });

        Wheatear::EventScriptSystem system;
        system.OnUpdateRuntime(&scene, Wheatear::Timestep(1.0f / 60.0f));
        Check(component.RuntimeFrames.size() == 1
                && component.RuntimeFrames.back().EventName == "a",
            "wts sibling events dispatch in order");
        Wheatear::CommandBus::ClearQueuedCommands();
        std::filesystem::remove(path);
    }

    // Child events can return a value to the suspended parent.
    {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "wheatear_event_return_test.wts";
        {
            std::ofstream file(path, std::ios::trunc);
            file << "event parent:\n"
                    "  event:child\n"
                    "  if event_result == 7\n"
                    "    wait 0.01\n"
                    "  endif\n"
                    "end\n"
                    "event child:\n"
                    "  event:return:7\n"
                    "end\n";
        }

        Wheatear::CommandBus::ClearQueuedCommands();
        Wheatear::Scene scene;
        auto entity = scene.CreateEntity("Flow");
        auto& component = entity.AddComponent<Wheatear::EventScriptComponent>();
        component.ScriptPath = path.string();
        component.StartEvent = "parent";
        component.RunOnStart = true;
        component.RunOnce = false;

        Wheatear::EventScriptSystem system;
        system.OnRuntimeStart(&scene);
        for (int i = 0; i < 120; ++i)
            system.OnUpdateRuntime(&scene, Wheatear::Timestep(1.0f / 60.0f));

        Check(component.RuntimeEventResult == 7.0,
            "wts child event returns a value");
        Check(component.RuntimeFrames.empty() && component.RuntimeCompleted,
            "wts parent resumes and completes after child return");
        Wheatear::CommandBus::ClearQueuedCommands();
        std::filesystem::remove(path);
    }

    if (g_Failures == 0)
    {
        std::cout << "Logic self-tests passed.\n";
        return 0;
    }

    std::cerr << g_Failures << " logic self-test(s) failed.\n";
    return 1;
}
