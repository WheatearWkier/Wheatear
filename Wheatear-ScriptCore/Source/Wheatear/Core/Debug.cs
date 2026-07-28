namespace Wheatear
{
    public static class Debug
    {
        public static void Log(string message) => InternalCalls.Log_Info(message);
        public static void LogWarning(string message) => InternalCalls.Log_Warn(message);
        public static void LogError(string message) => InternalCalls.Log_Error(message);
    }
}
