#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/KeyCodes.h"
//#include "Wheatear/Core/MouseCodes.h"

namespace Wheatear {

	enum class CursorMode
	{
		Normal = 0,
		Hidden,
		Locked
	};

	class Input
	{
	public:
		static bool IsKeyPressed(int keycode);
		static bool IsMouseButtonPressed(int button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		static void SetMouseInputBounds(float minX, float minY, float maxX, float maxY);
		static void ClearMouseInputBounds();
		static bool IsMouseWithinInputBounds();

		static void SetCursorMode(CursorMode mode);
	};

}
