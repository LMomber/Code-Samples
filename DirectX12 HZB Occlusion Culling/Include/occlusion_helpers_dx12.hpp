#pragma once

#include <memory>

#include <DirectXMath.h>
using namespace DirectX;

class RootSignature;
class PipelineStateObject;

// For visualizing the mipmaps.
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
};

static Vertex fullscreenTriangle[3] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},  // Bottom-left
    {{3.0f, -1.0f, 0.0f}, {2.0f, 1.0f}},   // Bottom-right (past right edge)
    {{-1.0f, 3.0f, 0.0f}, {0.0f, -1.0f}}   // Top-left (past top edge)
};
//

struct VertexPosColor
{
    XMFLOAT3 Position = {0.f, 0.f, 0.f};
    XMFLOAT3 Color = {1.f, 1.f, 1.f};
};

struct InstanceData
{
    XMMATRIX WorldMatrix;
};