namespace Wheatear
{
    public static class Input
    {
        public static bool IsKeyDown(KeyCode keycode) => InternalCalls.Input_IsKeyDown((int)keycode);
        public static bool IsKeyPressed(KeyCode keycode) => IsKeyDown(keycode);

        public static bool IsMouseButtonDown(MouseButton button)
            => InternalCalls.Input_IsMouseButtonDown((int)button);

        public static Vector2 MousePosition
        {
            get { InternalCalls.Input_GetMousePosition(out Vector2 value); return value; }
        }

        public static float GetAxisRaw(string axisName)
        {
            switch (axisName)
            {
                case "Horizontal":
                    return ReadAxis(KeyCode.A, KeyCode.D, KeyCode.Left, KeyCode.Right);
                case "Vertical":
                    return ReadAxis(KeyCode.S, KeyCode.W, KeyCode.Down, KeyCode.Up);
                default:
                    return 0.0f;
            }
        }

        private static float ReadAxis(KeyCode negativeA, KeyCode positiveA, KeyCode negativeB, KeyCode positiveB)
        {
            float value = 0.0f;
            if (IsKeyDown(negativeA) || IsKeyDown(negativeB)) value -= 1.0f;
            if (IsKeyDown(positiveA) || IsKeyDown(positiveB)) value += 1.0f;
            return value;
        }
    }
}
