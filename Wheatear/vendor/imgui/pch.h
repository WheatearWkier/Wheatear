#pragma once

// Precompiled header for Dear ImGui: public API header plus stable std headers.
//
// IMGUI_DEFINE_MATH_OPERATORS must be defined before imgui.h is parsed;
// imgui_internal.h #errors otherwise. The imgui TUs define it themselves, but
// the PCH reaches imgui.h first, so it is defined here too.
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>
