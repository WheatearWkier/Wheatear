#pragma once

#include "Wheatear/Core/Window.h"
#include "Wheatear/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Wheatear {

    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& props);
        virtual ~WindowsWindow();

        void OnUpdate() override;

        uint32_t GetWidth()  const override { return m_Data.Width; }
        uint32_t GetHeight() const override { return m_Data.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override
        {
            m_Data.EventCallback = callback;
        }

        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

        void* GetNativeWindow() const override { return m_Window; }

    private:
        void Init(const WindowProps& props);
        void Shutdown();

        void SetupCallbacks();

    private:
        GLFWwindow* m_Window = nullptr;
        Scope<GraphicsContext>   m_Context;

        struct WindowData
        {
            std::string     Title;
            uint32_t        Width = 0;
            uint32_t        Height = 0;
            bool            VSync = false;
            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
    };

} // namespace Wheatear
