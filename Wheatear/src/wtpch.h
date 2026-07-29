#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <stdint.h>

#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>

//#include <filesystem>

#include "Wheatear/Core/Log.h"
#include "Wheatear/Debug/Instrumentor.h"

#ifdef WT_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <Windows.h>
#endif // WT_PLATFORM_WINDOWS
