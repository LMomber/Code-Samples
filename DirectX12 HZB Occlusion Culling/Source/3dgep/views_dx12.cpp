#include "pch_dx12.hpp"
#include "views_dx12.hpp"

using namespace bee;

D3D12_CPU_DESCRIPTOR_HANDLE bee::ConstantBufferView::GetDescriptorHandle() { return m_Descriptor->GetDescriptorHandle(); }

ConstantBufferView::ConstantBufferView(Device& device, const std::shared_ptr<ConstantBuffer>& constantBuffer, size_t offset)
    : m_Device(device), m_ConstantBuffer(constantBuffer)
{
    assert(constantBuffer);

    auto d3d12Device = m_Device.GetD3D12Device();
    auto d3d12Resource = m_ConstantBuffer->GetD3D12Resource();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
    cbv.BufferLocation = d3d12Resource->GetGPUVirtualAddress() + offset;
    cbv.SizeInBytes = static_cast<UINT>(Math::AlignUp(m_ConstantBuffer->GetSizeInBytes(),
                                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));  // Constant buffers must be aligned for
                                                                                      // hardware requirements.

    *m_Descriptor = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    d3d12Device->CreateConstantBufferView(&cbv, m_Descriptor->GetDescriptorHandle());
}

/////////

D3D12_CPU_DESCRIPTOR_HANDLE bee::ShaderResourceView::GetDescriptorHandle() const { return m_Descriptor->GetDescriptorHandle(); }

ShaderResourceView::ShaderResourceView(Device& device,
                                       const std::shared_ptr<DX12Resource>& resource,
                                       const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
    : m_Device(device), m_Resource(resource)
{
    assert(resource || srv);

    auto d3d12Resource = m_Resource ? m_Resource->GetD3D12Resource() : nullptr;
    auto d3d12Device = m_Device.GetD3D12Device();

    m_Descriptor = std::make_shared<DescriptorAllocation>(m_Device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    d3d12Device->CreateShaderResourceView(d3d12Resource.Get(), srv, m_Descriptor->GetDescriptorHandle());
}

/////////

D3D12_CPU_DESCRIPTOR_HANDLE bee::UnorderedAccessView::GetDescriptorHandle() const
{
    return m_Descriptor->GetDescriptorHandle();
}

UnorderedAccessView::UnorderedAccessView(Device& device,
                                         const std::shared_ptr<DX12Resource>& resource,
                                         const std::shared_ptr<DX12Resource>& counterResource,
                                         const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
    : m_Device(device), m_Resource(resource), m_CounterResource(counterResource)
{
    assert(m_Resource || uav);

    auto d3d12Device = m_Device.GetD3D12Device();
    auto d3d12Resource = m_Resource ? m_Resource->GetD3D12Resource() : nullptr;
    auto d3d12CounterResource = m_CounterResource ? m_CounterResource->GetD3D12Resource() : nullptr;

    if (m_Resource)
    {
        auto d3d12ResourceDesc = m_Resource->GetD3D12ResourceDesc();

        // Resource must be created with the D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS flag.
        assert((d3d12ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0);
    }

    m_Descriptor = std::make_shared<DescriptorAllocation>(m_Device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    d3d12Device->CreateUnorderedAccessView(d3d12Resource.Get(),
                                           d3d12CounterResource.Get(),
                                           uav,
                                           m_Descriptor->GetDescriptorHandle());
}