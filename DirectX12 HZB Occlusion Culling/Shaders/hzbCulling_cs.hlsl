#define FLT_MAX 3.402823466e+38F

static const uint OC_HIDDEN = 0;
static const uint OC_VISIBLE = 1;
static const uint OC_ERROR = 2;

cbuffer Constants : register(b0)
{
    uint maxHzbMip;
    uint numObjects;
};

struct CameraVP
{
    matrix VP;
};

struct Frustum
{
    float4 planes[6];
};

struct AABB
{
    float3 min;
    float3 max;
};

ConstantBuffer<CameraVP> cameraCB : register(b1);
ConstantBuffer<Frustum> frustum : register(b2);

Texture2D hzb : register(t0);
StructuredBuffer<AABB> aabbBuffer : register(t1);

RWStructuredBuffer<uint> visibilityBuffer : register(u0);

SamplerState pointSampler : register(s0);

#define OUTSIDE 0
#define INSIDE 1

int PlaneAABBIntersect(AABB B, float4 plane)
{
    float3 mask = step(0.0, plane.xyz);  // 1.0 where plane.xyz >= 0, 0.0 where plane.xyz < 0

    // Since mask is always either 0 or 1, the lerp acts as a ternary operator.
	// This is a minor optimization, since there is no branching.
    float3 positiveVertex = lerp(B.min, B.max, mask);

    // Whether the box is inside or intersecting is only important when using a BVH on the CPU. 
	// On the GPU, we just want to know of its outside or not.
    float s1 = dot(positiveVertex, plane.xyz) + plane.w;

    return (s1 >= 0) * INSIDE;
}

int FrustumAABBIntersect(AABB B)
{
    int insideCounter = 0;

    [unroll] 
    for (int i = 0; i < 6; ++i)
    {
        int result = PlaneAABBIntersect(B, frustum.planes[i]);

        int outsideMask = (result == OUTSIDE);
        if (outsideMask) return OUTSIDE;

        insideCounter += 1;
    }

    return step(6, insideCounter);
}

void CalculateMinMax(float3 aabb_min, float3 aabb_max, out float2 min_xy, out float2 max_xy, out float min_z)
{
    float3 corners[8] =
    {
        float3(aabb_min.x, aabb_min.y, aabb_min.z),
        float3(aabb_min.x, aabb_min.y, aabb_max.z),
        float3(aabb_min.x, aabb_max.y, aabb_min.z),
        float3(aabb_min.x, aabb_max.y, aabb_max.z),
        float3(aabb_max.x, aabb_min.y, aabb_min.z),
        float3(aabb_max.x, aabb_min.y, aabb_max.z),
        float3(aabb_max.x, aabb_max.y, aabb_min.z),
        float3(aabb_max.x, aabb_max.y, aabb_max.z)
    };

    min_xy = float2(FLT_MAX, FLT_MAX);
    max_xy = float2(-FLT_MAX, -FLT_MAX);
    min_z = 1.0f;

    for (int i = 0; i < 8; ++i)
    {
        float4 world_space = float4(corners[i], 1.0f);
        float4 clip_space = mul(cameraCB.VP, world_space);

        float3 ndc_space = clip_space.xyz / clip_space.w;
        
        float3 uv_space = float3((ndc_space.x + 1) / 2, (1 - ndc_space.y) / 2, (ndc_space.z - 0.9) * 10);

        min_xy = min(min_xy, uv_space.xy);
        max_xy = max(max_xy, uv_space.xy);
        min_z = min(min_z, uv_space.z);
    }
}

[numthreads(64, 1, 1)]
void main(int3 uid : SV_DispatchThreadID)
{
    uint index = uid.x;

    if (index >= numObjects || FrustumAABBIntersect(aabbBuffer[index]) == OUTSIDE)
    {
        visibilityBuffer[index] = OC_HIDDEN;
        return;
    }
    
    AABB aabb = aabbBuffer[index];
    
    float3 aabb_min = aabb.min;
    float3 aabb_max = aabb.max;
    float4 aabb_size = float4(aabb_max - aabb_min, 0);
    
    float2 min_xy = float2(1.f, 1.f);
    float2 max_xy = float2(0.f, 0.f);
    float min_z = 1;

    CalculateMinMax(aabb_min, aabb_max, min_xy, max_xy, min_z);
    
    uint baseWidth, baseHeight;
    hzb.GetDimensions(baseWidth, baseHeight);
    
    float2 boxWidth = float2(max_xy - min_xy);
    float2 texelSize = boxWidth * float2(baseWidth, baseHeight);
    float maxDimension = max(texelSize.x, texelSize.y);
    
    float mip = floor(log2(maxDimension));
    mip = min(mip, maxHzbMip);

    float4 box = float4(min_xy, max_xy);
    
    float4 sample1 = hzb.SampleLevel(pointSampler, box.xy, mip);
    float4 sample2 = hzb.SampleLevel(pointSampler, box.zy, mip);
    float4 sample3 = hzb.SampleLevel(pointSampler, box.xw, mip);
    float4 sample4 = hzb.SampleLevel(pointSampler, box.zw, mip);

    float max_z = max(max(max(sample1.x, sample2.x), sample3.x), sample4.x);
    
    visibilityBuffer[index] = min_z <= max_z + 0.001f;
}