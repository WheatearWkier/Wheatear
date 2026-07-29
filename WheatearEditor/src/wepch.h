#pragma once

// Editor-wide precompiled header. Keep this to stable, frequently used headers
// so feature panels can compile quickly without pulling in module-specific code.
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include "wtpch.h"

#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <system_error>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
