struct ViewProjection
{
    matrix VP;
};

struct InstanceData
{
    matrix M;
};

struct IndirectCommand
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

ConstantBuffer<ViewProjection> ViewProjectionCB : register(b0);
StructuredBuffer<InstanceData> instanceBuffer   : register(t0);
StructuredBuffer<uint> matrixIndexBuffer        : register(t4);
StructuredBuffer<IndirectCommand> indirectDrawBuffer : register(t5);

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

    int maxCount = indirectDrawBuffer[0].InstanceCount;

    OUT.Position = mul(mul(ViewProjectionCB.VP, instanceBuffer[matrixIndexBuffer[instanceId]].M), float4(IN.Position, 1.0f));
    OUT.Color = float4(IN.Color, 1.0f);

    return OUT;
}