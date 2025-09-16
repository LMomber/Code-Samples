struct IndirectCommand
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
};

RWStructuredBuffer<uint> visibilityBuffer : register(u1);
RWStructuredBuffer<uint> scanResult       : register(u2);
RWStructuredBuffer<uint> count            : register(u3);
RWStructuredBuffer<uint> matrixIndexBuffer: register(u4);
RWStructuredBuffer<IndirectCommand> indirectDrawBuffer : register(u5);

cbuffer constants : register(b0) { uint numInstances; };

[numthreads(256, 1, 1)] 
void main(uint3 threadID: SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
    uint i = threadID.x;
    uint globalIndex = groupID.x * 256 + i;

    if (globalIndex < numInstances && visibilityBuffer[globalIndex])
    {
        IndirectCommand arg;
        arg.BaseVertexLocation = 0;
        arg.IndexCountPerInstance = 36;
        arg.InstanceCount = count[0];
        arg.StartIndexLocation = 0;
        arg.StartInstanceLocation = 0;

        indirectDrawBuffer[0] = arg;
        matrixIndexBuffer[scanResult[globalIndex]] = globalIndex;
    }
}