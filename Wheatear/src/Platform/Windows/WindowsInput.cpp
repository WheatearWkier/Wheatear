#include "wtpch.h"
#include "Wheatear/Input/Input.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"
#include <GLFW/glfw3.h>

namespace Wheatear {

	namespace {

		struct MouseInputBounds
		{
			bool Enabled = false;
			float MinX = 0.0f;
			float MinY = 0.0f;
			float MaxX = 0.0f;
			float MaxY = 0.0f;
		};

		static MouseInputBounds s_MouseInputBounds;

		static GLFWwindow* GetNativeWindow()
		{
			return static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		}

		static std::pair<float, float> GetRawMousePosition(GLFWwindow* window)
		{
			double xpos = 0.0;
			double ypos = 0.0;
			glfwGetCursorPos(window, &xpos, &ypos);
			return { static_cast<float>(xpos), static_cast<float>(ypos) };
		}

		static bool MouseWithinInputBounds(GLFWwindow* window)
		{
			if (!s_MouseInputBounds.Enabled)
				return true;

			const auto [x, y] = GetRawMousePosition(window);
			return x >= s_MouseInputBounds.MinX && x <= s_MouseInputBounds.MaxX
				&& y >= s_MouseInputBounds.MinY && y <= s_MouseInputBounds.MaxY;
		}

	}

	bool Input::IsKeyPressed(int keycode)
	{
		auto window = GetNativeWindow();
		auto state = glfwGetKey(window, keycode);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(int button)
	{
		auto window = GetNativeWindow();
		if (!MouseWithinInputBounds(window))
			return false;
		auto state = glfwGetMouseButton(window, button);
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		auto window = GetNativeWindow();
		return GetRawMousePosition(window);
	}

	float Input::GetMouseX()
	{
		auto [xpos, ypos] = GetMousePosition();
		return xpos;
	}

	float Input::GetMouseY()
	{
		auto [xpos, ypos] = GetMousePosition();
		return ypos;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		GLFWwindow* window = GetNativeWindow();

		switch (mode)
		{
		case CursorMode::Normal:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;

		case CursorMode::Hidden:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			break;

		case CursorMode::Locked:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			break;
		}
	}

	void Input::SetMouseInputBounds(float minX, float minY, float maxX, float maxY)
	{
		s_MouseInputBounds.Enabled = true;
		s_MouseInputBounds.MinX = minX;
		s_MouseInputBounds.MinY = minY;
		s_MouseInputBounds.MaxX = maxX;
		s_MouseInputBounds.MaxY = maxY;
	}

	void Input::ClearMouseInputBounds()
	{
		s_MouseInputBounds = {};
	}

	bool Input::IsMouseWithinInputBounds()
	{
		auto window = GetNativeWindow();
		return MouseWithinInputBounds(window);
	}

}
