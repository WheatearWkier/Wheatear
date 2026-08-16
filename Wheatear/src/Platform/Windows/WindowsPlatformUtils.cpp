#include "wtpch.h"
#include "Wheatear/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"

#include <filesystem>
#include <vector>

namespace Wheatear {

	std::string FileDialogs::OpenFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();

	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };

		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);

		ofn.hwndOwner = glfwGetWin32Window(
			(GLFWwindow*)Application::Get().GetWindow().GetNativeWindow()
		);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.lpstrDefExt = "wheatear";
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetSaveFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::vector<std::string> FileDialogs::OpenFiles(const char* filter)
	{
		std::vector<std::string> result;

		// Explorer-style multi-select: one big buffer, NUL-separated entries;
		// the first entry is the directory when more than one file was chosen.
		std::vector<char> buffer(1u << 16, 0);
		OPENFILENAMEA ofn;
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(
			(GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = buffer.data();
		ofn.nMaxFile = static_cast<DWORD>(buffer.size());
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR
			| OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		if (GetOpenFileNameA(&ofn) != TRUE)
			return result;

		const char* cursor = buffer.data();
		const std::string first(cursor);
		if (first.empty())
			return result;
		cursor += first.size() + 1;
		if (*cursor == '\0')
		{
			// Single selection: the buffer holds the full path.
			result.push_back(first);
			return result;
		}

		const std::filesystem::path directory = first;
		while (*cursor)
		{
			const std::string name(cursor);
			result.push_back((directory / name).generic_string());
			cursor += name.size() + 1;
		}
		return result;
	}

}