#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear {

	class FileDialogs
	{
	public:
		//These return empty strings if cancelled
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);

		// Multi-select variant (Windows Explorer style); empty when cancelled.
		WHEATEAR_API static std::vector<std::string> OpenFiles(const char* filter);
	private:

	};
}