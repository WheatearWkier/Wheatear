namespace Wheatear
{
    public class ArcadeCombatLevelComponent : Component
    {
        public bool IsPaused => InternalCalls.ArcadeCombatLevelComponent_GetPaused(Entity.ID);
        public bool BossIntroStarted => InternalCalls.ArcadeCombatLevelComponent_GetBossIntroStarted(Entity.ID);
        public bool BossIntroFinished => InternalCalls.ArcadeCombatLevelComponent_GetBossIntroFinished(Entity.ID);
        public bool Victory => InternalCalls.ArcadeCombatLevelComponent_GetVictory(Entity.ID);
        public bool Defeat => InternalCalls.ArcadeCombatLevelComponent_GetDefeat(Entity.ID);

        public void RequestSceneCommand(string command)
        {
            InternalCalls.ArcadeCombatLevelComponent_RequestSceneCommand(Entity.ID, command);
        }
    }
}
