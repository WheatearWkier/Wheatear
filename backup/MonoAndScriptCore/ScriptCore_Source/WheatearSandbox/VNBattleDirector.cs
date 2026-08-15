using Wheatear;

namespace WheatearSandbox
{
    public class VNBattleDirector : Entity
    {
        private ArcadeCombatLevelComponent? _level;
        private UITextComponent? _directorText;
        public string DirectorTextEntityName = "Battle_ScriptDirectorText";

        public override void OnCreate()
        {
            _level = GetComponent<ArcadeCombatLevelComponent>();

            Entity? textEntity = Scene.FindEntityByName(DirectorTextEntityName);
            _directorText = textEntity?.GetComponent<UITextComponent>();

            SetDirectorText("C# 导演脚本已启动。原生战斗系统负责战斗，脚本负责节奏提示。");
        }

        public override void OnUpdate(float ts)
        {
            if (_level == null)
                return;

            if (_level.Victory)
            {
                SetDirectorText("C# 导演：检测到胜利，原生结算流程会返回剧情。");
                return;
            }

            if (_level.Defeat)
            {
                SetDirectorText("C# 导演：检测到失败，可以接重试、读档或剧情分支。");
                return;
            }

            if (_level.IsPaused)
            {
                SetDirectorText("C# 导演：游戏已暂停，原生战斗冻结，脚本 UI 仍可响应。");
                return;
            }

            if (!_level.BossIntroStarted)
                SetDirectorText("C# 导演：移动到发光点，触发 Boss 登场。");
            else if (!_level.BossIntroFinished)
                SetDirectorText("C# 导演：Boss 登场演出中，玩家控制已锁定。");
            else
                SetDirectorText("C# 导演：战斗进行中。利用掩体，切换武器，击败 Boss。");
        }

        private void SetDirectorText(string text)
        {
            if (_directorText != null)
                _directorText.Text = text;
        }
    }
}
