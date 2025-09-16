#include "frustum.hpp"

void NormalizePlane(Plane& plane) 
{
    float mag;
    mag = sqrt(plane.a * plane.a + plane.b * plane.b + plane.c * plane.c);
    plane.a = plane.a / mag;
    plane.b = plane.b / mag;
    plane.c = plane.c / mag;
    plane.d = plane.d / mag;
}

void ExtractPlanes(Plane* p_planes, const XMMATRIX& cameraMatrix, bool normalize) 
{
    assert(p_planes != nullptr && "p_planes can't be null.");

    const XMVECTOR row1 = cameraMatrix.r[0];
    const XMVECTOR row2 = cameraMatrix.r[1];
    const XMVECTOR row3 = cameraMatrix.r[2];
    const XMVECTOR row4 = cameraMatrix.r[3];

    // Left clipping plane
    p_planes[0].a = XMVectorGetW(row1) + XMVectorGetX(row1);
    p_planes[0].b = XMVectorGetW(row2) + XMVectorGetX(row2);
    p_planes[0].c = XMVectorGetW(row3) + XMVectorGetX(row3);
    p_planes[0].d = XMVectorGetW(row4) + XMVectorGetX(row4);
    // Right clipping plane
    p_planes[1].a = XMVectorGetW(row1) - XMVectorGetX(row1);
    p_planes[1].b = XMVectorGetW(row2) - XMVectorGetX(row2);
    p_planes[1].c = XMVectorGetW(row3) - XMVectorGetX(row3);
    p_planes[1].d = XMVectorGetW(row4) - XMVectorGetX(row4);
    // Top clipping plane
    p_planes[2].a = XMVectorGetW(row1) - XMVectorGetY(row1);
    p_planes[2].b = XMVectorGetW(row2) - XMVectorGetY(row2);
    p_planes[2].c = XMVectorGetW(row3) - XMVectorGetY(row3);
    p_planes[2].d = XMVectorGetW(row4) - XMVectorGetY(row4);
    // Bottom clipping plane
    p_planes[3].a = XMVectorGetW(row1) + XMVectorGetY(row1);
    p_planes[3].b = XMVectorGetW(row2) + XMVectorGetY(row2);
    p_planes[3].c = XMVectorGetW(row3) + XMVectorGetY(row3);
    p_planes[3].d = XMVectorGetW(row4) + XMVectorGetY(row4);
    // Near clipping plane
    p_planes[4].a = XMVectorGetZ(row1);
    p_planes[4].b = XMVectorGetZ(row2);
    p_planes[4].c = XMVectorGetZ(row3);
    p_planes[4].d = XMVectorGetZ(row4);
    // Far clipping plane
    p_planes[5].a = XMVectorGetW(row1) + XMVectorGetZ(row1);
    p_planes[5].b = XMVectorGetW(row2) + XMVectorGetZ(row2);
    p_planes[5].c = XMVectorGetW(row3) + XMVectorGetZ(row3);
    p_planes[5].d = XMVectorGetW(row4) + XMVectorGetZ(row4);

    // Normalize the plane equations, if requested
    if (normalize == true)
    {
        NormalizePlane(p_planes[0]);
        NormalizePlane(p_planes[1]);
        NormalizePlane(p_planes[2]);
        NormalizePlane(p_planes[3]);
        NormalizePlane(p_planes[4]);
        NormalizePlane(p_planes[5]);
    }
}

IntersectionType PlaneAABBIntersect(AABB& B, Plane& plane) 
{
    XMVECTOR planeNormal = XMVectorSet(plane.a, plane.b, plane.c, 0.f);

    XMVECTOR positiveVertex =
        XMVectorSet(plane.a >= 0 ? B.max.x : B.min.x, plane.b >= 0 ? B.max.y : B.min.y, plane.c >= 0 ? B.max.z : B.min.z, 0.0f);

    XMVECTOR negativeVertex =
        XMVectorSet(plane.a >= 0 ? B.min.x : B.max.x, plane.b >= 0 ? B.min.y : B.max.y, plane.c >= 0 ? B.min.z : B.max.z, 0.0f);

    float s1 = XMVectorGetX(XMVector3Dot(positiveVertex, planeNormal)) + plane.d;
    float s2 = XMVectorGetX(XMVector3Dot(negativeVertex, planeNormal)) + plane.d;

    if (s1 < 0)
    {
        return OUTSIDE;
    }

    if (s2 < 0)
    {
        return INTERSECT;
    }

    return INSIDE;
}

IntersectionType FrustumAABBIntersect(AABB& B, Plane* planes) 
{
    int insideCounter = 0;
    for (int i = 0; i < 6; ++i)
    {
        IntersectionType result = PlaneAABBIntersect(B, planes[i]);
        if (result == OUTSIDE)
        {
            return OUTSIDE;
        }
        if (result == INSIDE)
        {
            insideCounter++;
        }
    }

    if (insideCounter == 6)
    {
        return INSIDE;
    }
    else
        return INTERSECT;
}

void AddAllChildren(std::vector<int>& array, std::shared_ptr<BVHNode>& node) 
{
    if (node->objectIndex != -1)
    {
        array.push_back(node->objectIndex);
    }
    else
    {
        AddAllChildren(array, node->left);
        AddAllChildren(array, node->right);
    }
}

void FrustumBVHIntersect(std::vector<int>& array, std::shared_ptr<BVHNode>& bvh, FrustumPlanes& frustum) 
{
    IntersectionType intersect = FrustumAABBIntersect(bvh->bounds, frustum.planes);
    if (intersect == INSIDE)
    {
        AddAllChildren(array, bvh);
    }
    if (intersect == INTERSECT)
    {
        if (bvh->objectIndex != -1)
        {
            array.push_back(bvh->objectIndex);
        }
        else
        {
            FrustumBVHIntersect(array, bvh->left, frustum);
            FrustumBVHIntersect(array, bvh->right, frustum);
        }
    }

    // If intersect == OUTSIDE, discard all
    return;
}

XMFLOAT3 IntersectionPoint(const Plane& a, const Plane& b, const Plane& c) 
{
    // Formula from: https://stackoverflow.com/questions/28822211/how-to-draw-a-frustum-in-opengl

    // Plane normals
    XMVECTOR N1 = XMVectorSet(a.a, a.b, a.c, 0.0f);
    XMVECTOR N2 = XMVectorSet(b.a, b.b, b.c, 0.0f);
    XMVECTOR N3 = XMVectorSet(c.a, c.b, c.c, 0.0f);

    XMVECTOR crossN2N3 = XMVector3Cross(N2, N3);
    XMVECTOR crossN3N1 = XMVector3Cross(N3, N1);
    XMVECTOR crossN1N2 = XMVector3Cross(N1, N2);

    float f = -XMVectorGetX(XMVector3Dot(N1, crossN2N3));

    XMVECTOR v1 = XMVectorScale(crossN2N3, a.d);
    XMVECTOR v2 = XMVectorScale(crossN3N1, b.d);
    XMVECTOR v3 = XMVectorScale(crossN1N2, c.d);

    XMVECTOR vec = XMVectorAdd(XMVectorAdd(v1, v2), v3);
    vec = XMVectorScale(vec, 1.0f / f);

    XMFLOAT3 result;
    XMStoreFloat3(&result, vec);

    return result;
}

void GetFrustumCorners(std::array<XMFLOAT3, 8>& corners, FrustumPlanes frustum) 
{
    corners[0] = IntersectionPoint(frustum.planes[0], frustum.planes[2], frustum.planes[4]);  // left  | top    | near
    corners[1] = IntersectionPoint(frustum.planes[0], frustum.planes[2], frustum.planes[5]);  // left  | top    | far
    corners[2] = IntersectionPoint(frustum.planes[0], frustum.planes[3], frustum.planes[4]);  // left  | bottom | near
    corners[3] = IntersectionPoint(frustum.planes[0], frustum.planes[3], frustum.planes[5]);  // left  | bottom | far
    corners[4] = IntersectionPoint(frustum.planes[1], frustum.planes[2], frustum.planes[4]);  // right | top    | near
    corners[5] = IntersectionPoint(frustum.planes[1], frustum.planes[2], frustum.planes[5]);  // right | top    | far
    corners[6] = IntersectionPoint(frustum.planes[1], frustum.planes[3], frustum.planes[4]);  // right | bottom | near
    corners[7] = IntersectionPoint(frustum.planes[1], frustum.planes[3], frustum.planes[5]);  // right | bottom | far
}

std::array<uint16_t, 24> GetFrustumWireframeIndices() 
{
    return {
        0, 1,  // Near top-left to far top-left
        1, 3,  // Far top-left to far bottom-left
        3, 2,  // Far bottom-left to near bottom-left
        2, 0,  // Near bottom-left to near top-left

        4, 5,  // Near top-right to far top-right
        5, 7,  // Far top-right to far bottom-right
        7, 6,  // Far bottom-right to near bottom-right
        6, 4,  // Near bottom-right to near top-right

        0, 4,  // Near top-left to near top-right
        1, 5,  // Far top-left to far top-right
        2, 6,  // Near bottom-left to near bottom-right
        3, 7   // Far bottom-left to far bottom-right
    };
}
