#pragma once

#include "occlusion_helpers_dx12.hpp"

#include "d3d12.h"

#include <array>
#include <vector>

namespace render
{
static std::array<VertexPosColor, 8> vertexData = {{
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}},  // 0
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},   // 1
    {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}},    // 2
    {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},   // 3
    {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},   // 4
    {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},    // 5
    {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},     // 6
    {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}     // 7
}};

const int numInstances = 10000000;
static std::vector<InstanceData> instanceData;

static std::array<WORD, 36> indexData = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0,
                                               3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7};
}  // namespace render