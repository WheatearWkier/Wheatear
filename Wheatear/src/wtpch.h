#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif

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

// Heavy, stable headers parsed once here instead of in every TU. GLM and entt
// are used by ~54 and ~31 translation units respectively; keeping them out of
// the PCH made each of those files re-parse them in full.
#include "entt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>

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
