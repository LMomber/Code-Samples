struct ViewProjection
{
    matrix VP;
};

struct InstanceData
{
    matrix M;
};

ConstantBuffer<ViewProjection> ViewProjectionCB : register(b0);

StructuredBuffer<InstanceData> instanceBuffer : register(t0);

struct VertexInput
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct VertexShaderOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

VertexShaderOutput main(uint instanceId : SV_InstanceID, VertexInput IN)
{
    VertexShaderOutput OUT;

    OUT.Position = mul(mul(ViewProjectionCB.VP, instanceBuffer[instanceId].M), float4(IN.Position, 1.0f));
    OUT.Color = float4(IN.Color, 1.0f);

    return OUT;
}