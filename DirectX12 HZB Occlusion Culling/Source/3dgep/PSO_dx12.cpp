#include "pch_dx12.hpp"
#include "PSO_dx12.hpp"

using namespace bee;

PipelineStateObject::PipelineStateObject(Device& device, const D3D12_PIPELINE_STATE_STREAM_DESC& desc) : m_Device(device)
{
    auto d3d12Device = device.GetD3D12Device();

    ThrowIfFailed(d3d12Device->CreatePipelineState(&desc, IID_PPV_ARGS(&m_d3d12PipelineState)));
}