RWStructuredBuffer<uint> scanResult : register(u0);
RWStructuredBuffer<uint> groupSums  : register(u1); 
RWStructuredBuffer<uint> count      : register(u2);

cbuffer constants : register(b0) { uint numInstances; };

[numthreads(256, 1, 1)] void main(uint3 threadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
    uint threadGroupIndex = threadID.x;
    uint globalIndex = groupID.x * 256 + threadGroupIndex;

    uint offset = (groupID.x > 0) ? groupSums[groupID.x] : 0;

    scanResult[globalIndex] += (globalIndex < numInstances) ? offset : 0;

    if ((globalIndex+1) == numInstances) 
    {
        count[0] = offset;
    }
}