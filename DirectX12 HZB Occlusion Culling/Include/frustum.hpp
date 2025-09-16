#pragma once

#include "pch_dx12.hpp"

#include "bounding_volumes.hpp"

#include <memory>
#include <array>

using namespace DirectX;

// From paper: Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix
// Source: https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf

enum IntersectionType
{
    OUTSIDE,
    INTERSECT,
    INSIDE
};

struct Plane
{
    float a, b, c, d;
};

struct FrustumPlanes
{
    Plane planes[6];
};

void NormalizePlane(Plane& plane);

void ExtractPlanes(Plane* p_planes, const XMMATRIX& comboMatrix, bool normalize);

IntersectionType PlaneAABBIntersect(AABB& B, Plane& plane);

IntersectionType FrustumAABBIntersect(AABB& B, Plane* planes);

void AddAllChildren(std::vector<int>& array, std::shared_ptr<BVHNode>& node);

void FrustumBVHIntersect(std::vector<int>& array, std::shared_ptr<BVHNode>& bvh, FrustumPlanes& frustum);

XMFLOAT3 IntersectionPoint(const Plane& a, const Plane& b, const Plane& c);

void GetFrustumCorners(std::array<XMFLOAT3, 8>& corners, FrustumPlanes frustum);

std::array<uint16_t, 24> GetFrustumWireframeIndices();