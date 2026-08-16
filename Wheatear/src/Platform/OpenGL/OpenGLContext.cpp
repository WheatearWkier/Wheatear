#include "wtpch.h"
#include "OpenGLContext.h"

#include <stdexcept>

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
		if (!status)
		{
			// Release builds compile the assert away, so a failed loader would
			// otherwise call null function pointers below. Fail loudly instead.
			WT_CORE_ERROR("OpenGLContext: failed to initialize GLAD (no GL driver?)");
			throw std::runtime_error("Failed to initialize GLAD (no GL driver?)");
		}

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

