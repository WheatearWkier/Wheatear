#include "wtpch.h"
#include "WindowsWindow.h"

#include "Wheatear/Assets/AssetPath.h"

#include "Wheatear/Events/KeyEvent.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Events/ApplicationEvent.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include "stb_image.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>

#include <filesystem>
#include <stdexcept>

#pragma comment(lib, "dwmapi.lib")

namespace Wheatear {

    static uint32_t s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        WT_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    namespace {

        WNDPROC s_GlfwWndProc = nullptr;
        bool s_BorderlessCaption = false;

        LRESULT CALLBACK BorderlessWndProc(HWND hwnd, UINT message,
            WPARAM wParam, LPARAM lParam)
        {
            if (s_BorderlessCaption && message == WM_NCHITTEST
                && !IsZoomed(hwnd))
            {
                POINT cursor{};
                if (GetCursorPos(&cursor))
                {
                    RECT rect{};
                    GetWindowRect(hwnd, &rect);
                    constexpr int border = 6;
                    const bool left = cursor.x < rect.left + border;
                    const bool right = cursor.x >= rect.right - border;
                    const bool top = cursor.y < rect.top + border;
                    const bool bottom = cursor.y >= rect.bottom - border;

                    if (top && left) return HTTOPLEFT;
                    if (top && right) return HTTOPRIGHT;
                    if (bottom && left) return HTBOTTOMLEFT;
                    if (bottom && right) return HTBOTTOMRIGHT;
                    if (left) return HTLEFT;
                    if (right) return HTRIGHT;
                    if (top) return HTTOP;
                    if (bottom) return HTBOTTOM;
                    return HTCLIENT;
                }
            }

            if (s_GlfwWndProc)
                return CallWindowProc(s_GlfwWndProc, hwnd, message, wParam, lParam);
            return DefWindowProc(hwnd, message, wParam, lParam);
        }

    } // namespace


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

            // Match the native title bar to the dark editor. Attribute 20 is
            // the Win10 20H1+ value; 19 is the older preview spelling.
            if (HWND hwnd = glfwGetWin32Window(m_Window))
            {
                BOOL darkTitleBar = TRUE;
                if (FAILED(DwmSetWindowAttribute(hwnd, 20,
                        &darkTitleBar, sizeof(darkTitleBar))))
                {
                    DwmSetWindowAttribute(hwnd, 19, &darkTitleBar,
                        sizeof(darkTitleBar));
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

    void WindowsWindow::SetDecorated(bool decorated)
    {
        if (!m_Window)
            return;

        glfwSetWindowAttrib(m_Window, GLFW_DECORATED,
            decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, GLFW_TRUE);
        glfwSetWindowSizeLimits(m_Window, 900, 560,
            GLFW_DONT_CARE, GLFW_DONT_CARE);
        s_BorderlessCaption = !decorated;

        HWND hwnd = glfwGetWin32Window(m_Window);
        if (!hwnd)
            return;

        // A borderless WINDOW still needs WS_THICKFRAME for WM_NCHITTEST to
        // start an interactive resize. Keep min/max/sysmenu bits so restore,
        // snap, and the custom title-bar buttons continue to work.
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        if (!decorated && !s_GlfwWndProc)
        {
            s_GlfwWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd,
                GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BorderlessWndProc)));
        }
    }

    void WindowsWindow::BeginWindowDrag()
    {
        if (!m_Window)
            return;
        HWND hwnd = glfwGetWin32Window(m_Window);
        if (!hwnd)
            return;
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }

    void WindowsWindow::MinimizeWindow()
    {
        if (m_Window)
            glfwIconifyWindow(m_Window);
    }

    void WindowsWindow::MaximizeWindow()
    {
        if (m_Window && !glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED))
            glfwMaximizeWindow(m_Window);
    }

    void WindowsWindow::ToggleMaximizeWindow()
    {
        if (!m_Window)
            return;
        if (glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED))
            glfwRestoreWindow(m_Window);
        else
            glfwMaximizeWindow(m_Window);
    }

    bool WindowsWindow::IsWindowMaximized() const
    {
        return m_Window && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
    }

    void WindowsWindow::CloseWindow()
    {
        if (m_Window)
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

} // namespace Wheatear
