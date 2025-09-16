Texture2D mipSRV : register(t0);
SamplerState samplerState : register(s0);

cbuffer MipLevel : register(b0)
{
    uint mipLevel; 
};

struct PSInput
{
    float2 texCoord : TEXCOORD0; 
};

float4 main(PSInput input) : SV_Target
{
    uint baseWidth, baseHeight, numMipLevels;
    mipSRV.GetDimensions(baseWidth, baseHeight);

    float2 mipResolution = float2(baseWidth, baseHeight) / pow(2.0, mipLevel);
    float2 scaledCoord = input.texCoord * mipResolution;
    float2 snappedCoord = ceil(scaledCoord);
    float2 finalCoord = snappedCoord / mipResolution;

    float redShade =  mipSRV.SampleLevel(samplerState, finalCoord, mipLevel);
    
    return float4(redShade, redShade, redShade, 1);
}
