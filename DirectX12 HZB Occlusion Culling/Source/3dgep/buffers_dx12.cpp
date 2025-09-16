#include "pch_dx12.hpp"
#include "buffers_dx12.hpp"

using namespace bee;

DX12Resource::DX12Resource(Device& device, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue)
    : m_Device(device)
{
    auto d3d12Device = m_Device.GetD3D12Device();

    if (clearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
    }
    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(d3d12Device->CreateCommittedResource(&heapProperties,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &resourceDesc,
                                                       D3D12_RESOURCE_STATE_COMMON,
                                                       m_d3d12ClearValue.get(),
                                                       IID_PPV_ARGS(&m_d3d12Resource)));

    ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

    CheckFeatureSupport();
}

DX12Resource::DX12Resource(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const D3D12_CLEAR_VALUE* clearValue)
    : m_Device(device), m_d3d12Resource(resource)
{
    if (clearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
    }
    CheckFeatureSupport();
}

void DX12Resource::SetName(const std::wstring& name)
{
    m_ResourceName = name;
    if (m_d3d12Resource && !m_ResourceName.empty())
    {
        m_d3d12Resource->SetName(m_ResourceName.c_str());
    }
}

bool DX12Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const
{
    return (m_FormatSupport.Support1 & formatSupport) != 0;
}

bool DX12Resource::CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const
{
    return (m_FormatSupport.Support2 & formatSupport) != 0;
}

void DX12Resource::CheckFeatureSupport()
{
    auto d3d12Device = m_Device.GetD3D12Device();

    auto desc = m_d3d12Resource->GetDesc();
    m_FormatSupport.Format = desc.Format;
    ThrowIfFailed(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
                                                   &m_FormatSupport,
                                                   sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

/////////

Buffer::Buffer(Device& device, const D3D12_RESOURCE_DESC& resDesc) : DX12Resource(device, resDesc) {}

Buffer::Buffer(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource) : DX12Resource(device, resource) {}

/////////

ByteAddressBuffer::ByteAddressBuffer(Device& device, const D3D12_RESOURCE_DESC& resDesc) : Buffer(device, resDesc) {}

ByteAddressBuffer::ByteAddressBuffer(Device& device, ComPtr<ID3D12Resource> resource) : Buffer(device, resource) {}

/////////

ConstantBuffer::ConstantBuffer(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource) : Buffer(device, resource)
{
    m_SizeInBytes = GetD3D12ResourceDesc().Width;
}

ConstantBuffer::~ConstantBuffer() {}

/////////

IndexBuffer::IndexBuffer(Device& device, size_t numIndices, DXGI_FORMAT indexFormat)
    : Buffer(device, CD3DX12_RESOURCE_DESC::Buffer(numIndices * (indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4))),
      m_NumIndices(numIndices),
      m_IndexFormat(indexFormat),
      m_IndexBufferView{}
{
    assert(indexFormat == DXGI_FORMAT_R16_UINT || indexFormat == DXGI_FORMAT_R32_UINT);
    CreateIndexBufferView();
}

IndexBuffer::IndexBuffer(Device& device,
                         Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                         size_t numIndices,
                         DXGI_FORMAT indexFormat)
    : Buffer(device, resource), m_NumIndices(numIndices), m_IndexFormat(indexFormat), m_IndexBufferView{}
{
    assert(indexFormat == DXGI_FORMAT_R16_UINT || indexFormat == DXGI_FORMAT_R32_UINT);
    CreateIndexBufferView();
}

void IndexBuffer::CreateIndexBufferView()
{
    UINT bufferSize = static_cast<UINT>(m_NumIndices) * (m_IndexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4);

    m_IndexBufferView.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
    m_IndexBufferView.SizeInBytes = bufferSize;
    m_IndexBufferView.Format = m_IndexFormat;
}

/////////

VertexBuffer::VertexBuffer(Device& device, size_t numVertices, size_t vertexStride)
    : Buffer(device, CD3DX12_RESOURCE_DESC::Buffer(numVertices * vertexStride)),
      m_NumVertices(numVertices),
      m_VertexStride(vertexStride),
      m_VertexBufferView{}
{
    CreateVertexBufferView();
}

VertexBuffer::VertexBuffer(Device& device,
                           Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                           size_t numVertices,
                           size_t vertexStride)
    : Buffer(device, resource), m_NumVertices(numVertices), m_VertexStride(vertexStride), m_VertexBufferView{}
{
    CreateVertexBufferView();
}

VertexBuffer::~VertexBuffer() {}

void VertexBuffer::CreateVertexBufferView()
{
    m_VertexBufferView.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
    m_VertexBufferView.SizeInBytes = static_cast<UINT>(m_NumVertices * m_VertexStride);
    m_VertexBufferView.StrideInBytes = static_cast<UINT>(m_VertexStride);
}

/////////

StructuredBuffer::StructuredBuffer(Device& device, size_t numElements, size_t elementSize)
    : Buffer(device, CD3DX12_RESOURCE_DESC::Buffer(numElements * elementSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)),
      m_NumElements(numElements),
      m_ElementSize(elementSize)
{
    m_CounterBuffer = m_Device.CreateByteAddressBuffer(4);
}

StructuredBuffer::StructuredBuffer(Device& device,
                                   Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                   size_t numElements,
                                   size_t elementSize)
    : Buffer(device, resource), m_NumElements(numElements), m_ElementSize(elementSize)
{
    m_CounterBuffer = m_Device.CreateByteAddressBuffer(4);
}