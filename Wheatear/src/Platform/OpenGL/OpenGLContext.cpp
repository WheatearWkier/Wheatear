#include "wtpch.h"
#include "OpenGLContext.h"

namespace Wheatear {

	Wheatear::OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		WT_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void Wheatear::OpenGLContext::Init()
	{
		WT_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		WT_CORE_ASSERT(status, "Failed to initialize Glad!");

		WT_CORE_INFO("OpenGL Info:");
		WT_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		WT_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		WT_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
	}

	void Wheatear::OpenGLContext::SwapBuffers()
	{
		WT_PROFILE_FUNCTION();

		glfwSwapBuffers(m_WindowHandle);
	}

}

