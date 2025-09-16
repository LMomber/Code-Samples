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

float4 main(uint instanceId : SV_InstanceID, float3 Position : POSITION) : SV_Position
{
    return mul(mul(ViewProjectionCB.VP, instanceBuffer[instanceId].M), float4(Position, 1.0f));
}         
                                                                                                                                                                      