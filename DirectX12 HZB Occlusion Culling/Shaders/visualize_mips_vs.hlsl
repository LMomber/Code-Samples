struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput
{
    float2 texCoord : TEXCOORD0;
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}