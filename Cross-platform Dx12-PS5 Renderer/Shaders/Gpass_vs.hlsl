cbuffer camera : register(b0)
{
    matrix VP;
}

cbuffer modelCBuffer : register(b1)
{
    matrix model;
    matrix invTransMatrix;
};

struct VS_Input
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float2 uv        : TEXCOORD;
};

struct VS_Output
{
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
    float4 position  : SV_Position;
};

VS_Output main(VS_Input IN)
{
    VS_Output OUT;

    float4 position = mul(model, float4(IN.position, 1.0));
    OUT.position = mul(VP, position);
    // OUT.position.z = 0.5 * OUT.position.z + 0.5;
    OUT.normal = mul((float3x3)invTransMatrix, IN.normal);
    OUT.tangent = mul((float3x3)invTransMatrix, IN.tangent);
    OUT.bitangent = normalize(cross(OUT.normal, OUT.tangent));
    OUT.uv = IN.uv;

    return OUT;
}