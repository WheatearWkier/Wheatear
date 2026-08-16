#include "wtpch.h"
#include "WindowsWindow.h"

#include "Wheatear/Assets/AssetPath.h"

#include "Wheatear/Events/KeyEvent.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Events/ApplicationEvent.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include "stb_image.h"

#include <filesystem>
#include <stdexcept>

namespace Wheatear {

    static uint32_t s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        WT_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }


    Window* Window::Create(const WindowProps& props)
    {
        return new WindowsWindow(props);
    }


    WindowsWindow::WindowsWindow(const WindowProps& props)
    {
        WT_PROFILE_FUNCTION();
        Init(props);
    }

    WindowsWindow::~WindowsWindow()
    {
        WT_PROFILE_FUNCTION();
        Shutdown();
    }


    void WindowsWindow::Init(const WindowProps& props)
    {
        WT_PROFILE_FUNCTION();

        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        WT_CORE_INFO("Creating window '{0}' ({1} x {2})",
            props.Title, props.Width, props.Height);

        if (s_GLFWWindowCount == 0)
        {
            WT_PROFILE_SCOPE("glfwInit");
            const int success = glfwInit();
            if (!success)
            {
                // Release builds compile the assert away; without this the app
                // would keep running on an uninitialized GLFW.
                WT_CORE_ERROR("WindowsWindow: failed to initialize GLFW");
                throw std::runtime_error("Failed to initialize GLFW");
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        {
            WT_PROFILE_SCOPE("glfwCreateWindow");
            m_Window = glfwCreateWindow(
                static_cast<int>(props.Width),
                static_cast<int>(props.Height),
                props.Title.c_str(),
                nullptr, nullptr
            );
            if (!m_Window)
            {
                WT_CORE_ERROR("WindowsWindow: failed to create window '{0}' ({1} x {2})",
                    props.Title, props.Width, props.Height);
                throw std::runtime_error("Failed to create GLFW window");
            }
            s_GLFWWindowCount++;

            if (!props.IconPath.empty())
            {
                const std::filesystem::path iconPath = AssetPath::ResolveResource(props.IconPath);
                GLFWimage icon;
                icon.pixels = stbi_load(iconPath.string().c_str(),
                    &icon.width, &icon.height, nullptr, 4);
                if (icon.pixels)
                {
                    glfwSetWindowIcon(m_Window, 1, &icon);
                    stbi_image_free(icon.pixels);
                }
            }
        }

        m_Context = CreateScope<OpenGLContext>(m_Window);
        m_Context->Init();

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);

        SetupCallbacks();
    }


    void WindowsWindow::SetupCallbacks()
    {

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                data.Width = static_cast<uint32_t>(width);
                data.Height = static_cast<uint32_t>(height);

                WindowResizeEvent event(width, height);
                data.EventCallback(event);
            });

        glfwSetWindowCloseCallback(m_Window,
            [](GLFWwindow* window)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                WindowCloseEvent event;
                data.EventCallback(event);
            });

        glfwSetKeyCallback(m_Window,
            [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                switch (action)
                {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
                }
            });

        glfwSetCharCallback(m_Window,
            [](GLFWwindow* window, unsigned int codepoint)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                KeyTypedEvent event(codepoint);
                data.EventCallback(event);
            });

        glfwSetMouseButtonCallback(m_Window,
            [](GLFWwindow* window, int button, int action, int /*mods*/)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                switch (action)
                {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                }
            });

        glfwSetScrollCallback(m_Window,
            [](GLFWwindow* window, double xOffset, double yOffset)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                MouseScrolledEvent event(
                    static_cast<float>(xOffset),
                    static_cast<float>(yOffset)
                );
                data.EventCallback(event);
            });

        glfwSetCursorPosCallback(m_Window,
            [](GLFWwindow* window, double xPos, double yPos)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                MouseMovedEvent event(
                    static_cast<float>(xPos),
                    static_cast<float>(yPos)
                );
                data.EventCallback(event);
            });
    }


    void WindowsWindow::Shutdown()
    {
        WT_PROFILE_FUNCTION();

        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        if (s_GLFWWindowCount == 0)
            glfwTerminate();
    }


    void WindowsWindow::OnUpdate()
    {
        WT_PROFILE_FUNCTION();

        m_Context->SwapBuffers();
    }

    //  VSync

    void WindowsWindow::SetVSync(bool enabled)
    {
        WT_PROFILE_FUNCTION();

        glfwSwapInterval(enabled ? 1 : 0);
        m_Data.VSync = enabled;
    }

    bool WindowsWindow::IsVSync() const
    {
        return m_Data.VSync;
    }

    void WindowsWindow::SetFullscreen(bool enabled)
    {
        WT_PROFILE_FUNCTION();

        if (!m_Window || m_Data.Fullscreen == enabled)
            return;

        if (enabled)
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
            if (!monitor || !mode)
            {
                WT_CORE_WARN("WindowsWindow: could not enter fullscreen; primary monitor unavailable.");
                return;
            }

            glfwGetWindowPos(m_Window, &m_Data.WindowedX, &m_Data.WindowedY);
            m_Data.WindowedWidth = m_Data.Width;
            m_Data.WindowedHeight = m_Data.Height;
            m_Data.Fullscreen = true;
            m_Data.Width = static_cast<uint32_t>(mode->width);
            m_Data.Height = static_cast<uint32_t>(mode->height);
            glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            return;
        }

        const uint32_t restoreWidth = m_Data.WindowedWidth > 0 ? m_Data.WindowedWidth : 1280;
        const uint32_t restoreHeight = m_Data.WindowedHeight > 0 ? m_Data.WindowedHeight : 720;
        m_Data.Fullscreen = false;
        m_Data.Width = restoreWidth;
        m_Data.Height = restoreHeight;
        glfwSetWindowMonitor(m_Window,
            nullptr,
            m_Data.WindowedX,
            m_Data.WindowedY,
            static_cast<int>(restoreWidth),
            static_cast<int>(restoreHeight),
            0);
    }

} // namespace Wheatear
