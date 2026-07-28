#pragma once

#include "Wheatear/Renderer/GraphicsContext.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

struct GLFWwindow;

namespace Wheatear {

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);
		virtual void Init() override;
		virtual void SwapBuffers() override;
	private:
		GLFWwindow* m_WindowHandle;
	};

}