cbuffer cameraVS : register(b0)
{
    matrix VP;
    matrix V;
    matrix invV;
    matrix invP;
    float3 cameraPos;
};

cbuffer decalVS : register(b1)
{
    matrix M;
    matrix InvM;
    int  albedoMap;
    int  normalMap;
    int useAlbedoStrength;
    float albedoStrength;
    float normalStrength;
};

struct VS_Input
{
    float3 Position : POSITION;
};

struct VS_Output
{
    float4 Position : SV_Position;
};

VS_Output main(VS_Input IN)
{
    VS_Output OUT;

    // Transform to world space
    float4 worldPos = mul(M, float4(IN.Position, 1.0));

    // Convert to clip space
    OUT.Position = mul(VP, worldPos);

    return OUT; 
}
