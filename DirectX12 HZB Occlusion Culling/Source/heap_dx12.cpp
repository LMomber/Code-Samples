#include "pch_dx12.hpp"
#include "heap_dx12.hpp"

Heap::Heap(LPCWSTR name, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = flags;

    ThrowIfFailed(Engine.Device().GetD3D12Device()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

    m_heap->SetName(name);
    m_currentHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_handleIncrementSize =
        Engine.Device().GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

D3D12_CPU_DESCRIPTOR_HANDLE Heap::CreateSRV(ComPtr<ID3D12Resource>& resource,
                                                 UINT numElements,
                                                 UINT elementSize,
                                                 UINT64 firstElement)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;  // Typeless format for structured buffers
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = elementSize;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    Engine.Device().GetD3D12Device()->CreateShaderResourceView(resource.Get(), &srvDesc, m_currentHandle);

    auto handle = m_currentHandle;
    m_currentHandle.ptr += m_handleIncrementSize;

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Heap::CreateUAV(ComPtr<ID3D12Resource>& resource,
                                                 UINT numElements,
                                                 UINT elementSize,
                                                 UINT64 firstElement)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;  // Typeless format for structured buffers
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = numElements;          // Number of elements in the buffer
    uavDesc.Buffer.StructureByteStride = elementSize;  // Size of each structure
    uavDesc.Buffer.CounterOffsetInBytes = 0;           // No counter buffer
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    Engine.Device().GetD3D12Device()->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, m_currentHandle);

    auto handle = m_currentHandle;
    m_currentHandle.ptr += m_handleIncrementSize;

    return handle;
}

// Works only for the first CBV in the heap, since it doesn't increment the handle
D3D12_CPU_DESCRIPTOR_HANDLE Heap::CreateCBV(ComPtr<ID3D12Resource>& resource, UINT sizeInBytes)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeInBytes;  // Must be 256-byte aligned

    Engine.Device().GetD3D12Device()->CreateConstantBufferView(&cbvDesc, m_currentHandle);
    
    auto handle = m_currentHandle;
    m_currentHandle.ptr += m_handleIncrementSize;

    return handle;
}
