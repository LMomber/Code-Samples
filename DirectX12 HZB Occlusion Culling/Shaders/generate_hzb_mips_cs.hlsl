#define BLOCK_SIZE 16

#define FLT_MAX 3.402823466e+38F

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

cbuffer GenerateMipsCB : register(b0)
{
    uint SrcMipLevel; // Texture level of source mip
    uint SrcDimension;
    float2 TexelSize; // 1.0 / OutMip1.Dimensions
}

// Source mip map.
Texture2D<float> SrcMip : register(t0);

// Write up to 4 mip map levels.
RWTexture2D<float> OutMip : register(u0);

// Linear clamp sampler.
SamplerState pointSampler : register(s0);

#define GenerateMips_RootSignature \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 6), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 1) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_POINT)"


[RootSignature(GenerateMips_RootSignature)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    float Src = -FLT_MAX;
    
    float2 UV1 = TexelSize * (IN.DispatchThreadID.xy + float2(0.25, 0.25));
    float2 Off = TexelSize * 0.5;
            
    float2 offset1 = float2(0.0, 0.0);
    float2 offset2 = float2(Off.x, 0.0);
    float2 offset3 = float2(0.0, Off.y);
    float2 offset4 = float2(Off.x, Off.y);
            
    Src = max(max(max(SrcMip.SampleLevel(pointSampler, UV1 + offset1, SrcMipLevel),
            max(SrcMip.SampleLevel(pointSampler, UV1 + offset2, SrcMipLevel),
                max(SrcMip.SampleLevel(pointSampler, UV1 + offset3, SrcMipLevel),
                    SrcMip.SampleLevel(pointSampler, UV1 + offset4, SrcMipLevel)))),
            max(SrcMip.SampleLevel(pointSampler, UV1 + offset1 + float2(Off.x, 0.0), SrcMipLevel),
                max(SrcMip.SampleLevel(pointSampler, UV1 + offset2 + float2(Off.x, 0.0), SrcMipLevel),
                    max(SrcMip.SampleLevel(pointSampler, UV1 + offset3 + float2(Off.x, 0.0), SrcMipLevel),
                        SrcMip.SampleLevel(pointSampler, UV1 + offset4 + float2(Off.x, 0.0), SrcMipLevel))))),
            
          max(max(SrcMip.SampleLevel(pointSampler, UV1 + offset1 + float2(0.0, Off.y), SrcMipLevel),
            max(SrcMip.SampleLevel(pointSampler, UV1 + offset2 + float2(0.0, Off.y), SrcMipLevel),
                max(SrcMip.SampleLevel(pointSampler, UV1 + offset3 + float2(0.0, Off.y), SrcMipLevel),
                    SrcMip.SampleLevel(pointSampler, UV1 + offset4 + float2(0.0, Off.y), SrcMipLevel)))),
            max(SrcMip.SampleLevel(pointSampler, UV1 + offset1 + float2(Off.x, Off.y), SrcMipLevel),
                max(SrcMip.SampleLevel(pointSampler, UV1 + offset2 + float2(Off.x, Off.y), SrcMipLevel),
                    max(SrcMip.SampleLevel(pointSampler, UV1 + offset3 + float2(Off.x, Off.y), SrcMipLevel),
                        SrcMip.SampleLevel(pointSampler, UV1 + offset4 + float2(Off.x, Off.y), SrcMipLevel))))));

    OutMip[IN.DispatchThreadID.xy] = Src;
}

