RWStructuredBuffer<uint> inputGroupSums  : register(u0); 
RWStructuredBuffer<uint> outputGroupSums     : register(u1); 

cbuffer constants : register(b0) { uint numInstances; };

groupshared uint temp[256];

[numthreads(256, 1, 1)] void main(uint3 threadID
                                                : SV_GroupThreadID, uint3 groupID
                                                : SV_GroupID)
{
    uint i = threadID.x;
    uint globalIndex = groupID.x * 256 + i;

    temp[i] = (globalIndex < numInstances) ? inputGroupSums[globalIndex] : 0;
    GroupMemoryBarrierWithGroupSync();

    // Upsweep
    for (uint stride = 1; stride < 256; stride *= 2)
    {
        uint index = (i + 1) * stride * 2 - 1;
        if (index < 256)
        {
            temp[index] += temp[index - stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }


    if (i == 255)
    {
        outputGroupSums[groupID.x] = temp[255];
        temp[255] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // Downsweep
    for (uint stride = 128; stride > 0; stride /= 2)
    {
        uint index = (i + 1) * stride * 2 - 1;
        if (index < 256)
        {
            uint tempValue = temp[index - stride];
            temp[index - stride] = temp[index];
            temp[index] += tempValue;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (globalIndex < numInstances)
    {
        inputGroupSums[globalIndex] = temp[i];
    }
}
