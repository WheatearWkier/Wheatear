namespace Wheatear
{
    public static class Time
    {
        public static float DeltaTime => InternalCalls.Time_GetDeltaTime();
        public static float ElapsedTime => InternalCalls.Time_GetElapsedTime();
        public static ulong FrameCount => InternalCalls.Time_GetFrameCount();
    }
}
