#pragma once

#define VIEW_MATRIX_LOCATION 0
#define PROJECTION_MATRIX_LOCATION 1
#define VIEW_POSITION_LOCATION 2
#define LIGHT_POSITION_LOCATION 3
#define LIGHT_COLOR_LOCATION 4

#define POSITION_LOCATION 0
#define NORMAL_INDEX 1
#define MODEL_LOCATION 2
#define ALBEDO_LOCATION 6
#define ROUGHNESS_LOCATION 7
#define FRESNEL_FACTOR__LOCATION 8
#define TRANSPOSE_INVERSE_MODEL_LOCATION 9

#define SHADER(...) "#version 420 core\n#extension GL_ARB_explicit_uniform_location : require\n" STRINGIFY(__VA_ARGS__)

internal const char* const vs =
"#version 420 core\n"
"#extension GL_ARB_explicit_uniform_location : require\n"
#include "shaders/shader.vs"
"";

internal const char* const  fs =
"#version 420 core\n"
"#extension GL_ARB_explicit_uniform_location : require\n"
#include "shaders/shader.fs"
"";
