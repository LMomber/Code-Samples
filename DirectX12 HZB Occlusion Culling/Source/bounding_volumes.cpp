#include "bounding_volumes.hpp"

void AABB::Expand(const AABB& other) 
{
    min.x = std::min(min.x, other.min.x);
    min.y = std::min(min.y, other.min.y);
    min.z = std::min(min.z, other.min.z);

    max.x = std::max(max.x, other.max.x);
    max.y = std::max(max.y, other.max.y);
    max.z = std::max(max.z, other.max.z);
}

AABB TransformAABB(const AABB& aabb, const DirectX::XMMATRIX& worldMatrix) 
{
    assert((aabb.min.x <= aabb.max.x) && (aabb.min.y <= aabb.max.y) && (aabb.min.z <= aabb.max.z) &&
           "The min should be <= max on all axes");

    DirectX::XMFLOAT3 corners[8] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
    };

    AABB transformedAABB;
    transformedAABB.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    transformedAABB.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 8; ++i)
    {
        const DirectX::XMVECTOR cornerVec = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corners[i]), worldMatrix);

        DirectX::XMFLOAT3 transformedCorner;
        DirectX::XMStoreFloat3(&transformedCorner, cornerVec);

        transformedAABB.min.x = std::min(transformedAABB.min.x, transformedCorner.x);
        transformedAABB.min.y = std::min(transformedAABB.min.y, transformedCorner.y);
        transformedAABB.min.z = std::min(transformedAABB.min.z, transformedCorner.z);

        transformedAABB.max.x = std::max(transformedAABB.max.x, transformedCorner.x);
        transformedAABB.max.y = std::max(transformedAABB.max.y, transformedCorner.y);
        transformedAABB.max.z = std::max(transformedAABB.max.z, transformedCorner.z);
    }

    return transformedAABB;
}

std::shared_ptr<BVHNode> BuildBVH(std::vector<IndexedAABB>& objects, int start, int end) 
{
    std::shared_ptr<BVHNode> node = std::make_shared<BVHNode>();

    // Make te overarching boudning box
    IndexedAABB bounds = objects[start];
    for (int i = start + 1; i < end; ++i)
    {
        bounds.aabb.Expand(objects[i].aabb);
    }
    node->bounds = bounds.aabb;

    int objectCount = end - start;

    // Leaf node
    if (objectCount == 1)
    {
        node->objectIndex = start;
        return node;
    }

    // Check the longest axis
    DirectX::XMFLOAT3 size = {bounds.aabb.max.x - bounds.aabb.min.x,
                              bounds.aabb.max.y - bounds.aabb.min.y,
                              bounds.aabb.max.z - bounds.aabb.min.z};
    int axis = 0;  // x-axis
    if (size.y > size.x && size.y > size.z)
        axis = 1;  // y-axis
    else if (size.z > size.x)
        axis = 2;  // z-axis

    // Sort objects based on the center of their AABB along the longest axis
    auto compare = [axis](const IndexedAABB& a, const IndexedAABB& b)
    {
        float centerA = 0.f;
        float centerB = 0.f;

        switch (axis)
        {
            case 0:
                centerA = (a.aabb.min.x + a.aabb.max.x) / 2.0f;
                centerB = (b.aabb.min.x + b.aabb.max.x) / 2.0f;
                break;
            case 1:
                centerA = (a.aabb.min.y + a.aabb.max.y) / 2.0f;
                centerB = (b.aabb.min.y + b.aabb.max.y) / 2.0f;
                break;
            case 2:
                centerA = (a.aabb.min.z + a.aabb.max.z) / 2.0f;
                centerB = (b.aabb.min.z + b.aabb.max.z) / 2.0f;
                break;
        }
        return centerA < centerB;
    };
    std::sort(objects.begin() + start, objects.begin() + end, compare);

    int mid = start + objectCount / 2;

    node->left = BuildBVH(objects, start, mid);
    node->right = BuildBVH(objects, mid, end);

    return node;
}
