#pragma once

#include "d3d12.h"

#include <wrl.h>
using namespace Microsoft::WRL;

class Heap
{
public:
    Heap(LPCWSTR name,
         UINT numDescriptors = 1,
         D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
         D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    ComPtr<ID3D12DescriptorHeap> GetD3D12Heap() const { return m_heap; }

    D3D12_CPU_DESCRIPTOR_HANDLE CreateSRV(ComPtr<ID3D12Resource>& resource,
                                          UINT numElements,
                                          UINT elementSize,
                                          UINT64 firstElement = 0);
    D3D12_CPU_DESCRIPTOR_HANDLE CreateUAV(ComPtr<ID3D12Resource>& resource,
                                          UINT numElements,
                                          UINT elementSize,
                                          UINT64 firstElement = 0);
    D3D12_CPU_DESCRIPTOR_HANDLE CreateCBV(ComPtr<ID3D12Resource>& resource,
                                          UINT sizeInBytes);

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;

    D3D12_CPU_DESCRIPTOR_HANDLE m_currentHandle;
    UINT m_handleIncrementSize = 0;
};