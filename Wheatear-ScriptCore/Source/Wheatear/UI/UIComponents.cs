namespace Wheatear
{
    public class UICanvasComponent : Component
    {
        public bool Visible
        {
            get => InternalCalls.UICanvasComponent_GetVisible(Entity.ID);
            set => InternalCalls.UICanvasComponent_SetVisible(Entity.ID, value);
        }
    }

    public class UIWidgetComponent : Component
    {
        public bool Visible
        {
            get => InternalCalls.UIWidgetComponent_GetVisible(Entity.ID);
            set => InternalCalls.UIWidgetComponent_SetVisible(Entity.ID, value);
        }

        public Vector2 Position
        {
            get { InternalCalls.UIWidgetComponent_GetPosition(Entity.ID, out Vector2 value); return value; }
            set => InternalCalls.UIWidgetComponent_SetPosition(Entity.ID, ref value);
        }

        public Vector2 Size
        {
            get { InternalCalls.UIWidgetComponent_GetSize(Entity.ID, out Vector2 value); return value; }
            set => InternalCalls.UIWidgetComponent_SetSize(Entity.ID, ref value);
        }
    }

    public class UIImageComponent : Component
    {
        public Vector4 Color
        {
            get { InternalCalls.UIImageComponent_GetColor(Entity.ID, out Vector4 value); return value; }
            set => InternalCalls.UIImageComponent_SetColor(Entity.ID, ref value);
        }
    }

    public class UITextComponent : Component
    {
        public string Text
        {
            get => InternalCalls.UITextComponent_GetText(Entity.ID);
            set => InternalCalls.UITextComponent_SetText(Entity.ID, value);
        }

        public Vector4 Color
        {
            get { InternalCalls.UITextComponent_GetColor(Entity.ID, out Vector4 value); return value; }
            set => InternalCalls.UITextComponent_SetColor(Entity.ID, ref value);
        }

        public float FontSize
        {
            get => InternalCalls.UITextComponent_GetFontSize(Entity.ID);
            set => InternalCalls.UITextComponent_SetFontSize(Entity.ID, value);
        }

        public string FontPath
        {
            get => InternalCalls.UITextComponent_GetFontPath(Entity.ID);
            set => InternalCalls.UITextComponent_SetFontPath(Entity.ID, value);
        }
    }

    public class UIProgressBarComponent : Component
    {
        public float Value
        {
            get => InternalCalls.UIProgressBarComponent_GetValue(Entity.ID);
            set => InternalCalls.UIProgressBarComponent_SetValue(Entity.ID, value);
        }

        public float MaxValue
        {
            get => InternalCalls.UIProgressBarComponent_GetMaxValue(Entity.ID);
            set => InternalCalls.UIProgressBarComponent_SetMaxValue(Entity.ID, value);
        }

        public float Normalized => MaxValue > 0.0f ? Mathf.Clamp01(Value / MaxValue) : 0.0f;

        public Vector4 ForegroundColor
        {
            get { InternalCalls.UIProgressBarComponent_GetForegroundColor(Entity.ID, out Vector4 value); return value; }
            set => InternalCalls.UIProgressBarComponent_SetForegroundColor(Entity.ID, ref value);
        }
    }

    public class UIButtonComponent : Component
    {
        public bool IsHovered => InternalCalls.UIButtonComponent_GetIsHovered(Entity.ID);
        public bool IsPressed => InternalCalls.UIButtonComponent_GetIsPressed(Entity.ID);

        public string OnClickFunction
        {
            get => InternalCalls.UIButtonComponent_GetOnClickFunction(Entity.ID);
            set => InternalCalls.UIButtonComponent_SetOnClickFunction(Entity.ID, value);
        }
    }
}
