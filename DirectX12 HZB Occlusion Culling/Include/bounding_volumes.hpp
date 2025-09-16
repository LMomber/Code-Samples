#pragma once

#include "pch_dx12.hpp"

#include <vector>
#include <algorithm>

struct VertexData
{
    float x, y, z;
};

struct AABB
{
    DirectX::XMFLOAT3 min;
    DirectX::XMFLOAT3 max;

    void Expand(const AABB& other);
};

struct IndexedAABB
{
    IndexedAABB(const AABB& aabb) : aabb(aabb) {}

    AABB aabb;
    int index = -1;
};

struct BVHNode
{
    AABB bounds;
    std::shared_ptr<BVHNode> left = nullptr; 
    std::shared_ptr<BVHNode> right = nullptr; 
    int objectIndex = -1;

    bool IsLeaf() const { return left == nullptr && right == nullptr; }
};

AABB TransformAABB(const AABB& aabb, const DirectX::XMMATRIX& worldMatrix);

std::shared_ptr<BVHNode> BuildBVH(std::vector<IndexedAABB>& objects, int start, int end);