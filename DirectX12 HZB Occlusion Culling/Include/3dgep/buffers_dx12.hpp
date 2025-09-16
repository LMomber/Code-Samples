#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

/**
 *  @file Buffer.h
 *  @date October 22, 2018
 *  @author Jeremiah van Oosten
 *
 *  @brief Abstract base class for buffer DX12Resources.
 */

#include "descriptor_allocation_dx12.hpp"
#include "core/device.hpp"

#include <dx12Headers/include/directx/d3dx12.h>

#include <d3d12.h>
#include <wrl.h>

#include <string>
#include <memory>

namespace bee
{

class DX12Resource
{
public:
    /**
     * Get the Device that was used to create this resource.
     */
    Device& GetDevice() const { return m_Device; }

    // Get access to the underlying D3D12 resource
    Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Resource() const { return m_d3d12Resource; }

    D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const
    {
        D3D12_RESOURCE_DESC resDesc = {};
        if (m_d3d12Resource)
        {
            resDesc = m_d3d12Resource->GetDesc();
        }

        return resDesc;
    }

    /**
     * Set the name of the resource. Useful for debugging purposes.
     */
    void SetName(const std::wstring& name);
    const std::wstring& GetName() const { return m_ResourceName; }

    /**
     * Check if the resource format supports a specific feature.
     */
    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;

protected:
    //    friend class CommandList;

    // Resource creation should go through the device.
    DX12Resource(bee::Device& device, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue = nullptr);
    DX12Resource(bee::Device& device,
                 Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                 const D3D12_CLEAR_VALUE* clearValue = nullptr);

    virtual ~DX12Resource() = default;

    // The device that is used to create this resource.
    bee::Device& m_Device;

    // The underlying D3D12 resource.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3d12Resource;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT m_FormatSupport;
    std::unique_ptr<D3D12_CLEAR_VALUE> m_d3d12ClearValue;
    std::wstring m_ResourceName;

private:
    // Check the format support and populate the m_FormatSupport structure.
    void CheckFeatureSupport();
};

/////////

class Buffer : public DX12Resource
{
public:
protected:
    Buffer(bee::Device& device, const D3D12_RESOURCE_DESC& resDesc);
    Buffer(bee::Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
};

class ByteAddressBuffer : public Buffer
{
public:
    size_t GetBufferSize() const { return m_BufferSize; }

protected:
    ByteAddressBuffer(bee::Device& device, const D3D12_RESOURCE_DESC& resDesc);
    ByteAddressBuffer(bee::Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
    virtual ~ByteAddressBuffer() = default;

private:
    size_t m_BufferSize;
};

/////////

class ConstantBuffer : public Buffer
{
public:
    size_t GetSizeInBytes() const { return m_SizeInBytes; }

protected:
    ConstantBuffer(bee::Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
    virtual ~ConstantBuffer();

private:
    size_t m_SizeInBytes;
};

/////////

class IndexBuffer : public Buffer
{
public:
    /**
     * Get the index buffer view for biding to the Input Assembler stage.
     */
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_IndexBufferView; }

    size_t GetNumIndices() const { return m_NumIndices; }

    DXGI_FORMAT GetIndexFormat() const { return m_IndexFormat; }

protected:
    IndexBuffer(bee::Device& device, size_t numIndices, DXGI_FORMAT indexFormat);
    IndexBuffer(bee::Device& device,
                Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                size_t numIndices,
                DXGI_FORMAT indexFormat);
    virtual ~IndexBuffer() = default;

    void CreateIndexBufferView();

private:
    size_t m_NumIndices;
    DXGI_FORMAT m_IndexFormat;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
};

/////////

class VertexBuffer : public Buffer
{
public:
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_VertexBufferView; }

    size_t GetNumVertices() const { return m_NumVertices; }

    size_t GetVertexStride() const { return m_VertexStride; }

protected:
    VertexBuffer(bee::Device& device, size_t numVertices, size_t vertexStride);
    VertexBuffer(bee::Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource, size_t numVertices, size_t vertexStride);
    virtual ~VertexBuffer();

    void CreateVertexBufferView();

private:
    size_t m_NumVertices;
    size_t m_VertexStride;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
};

/////////

class StructuredBuffer : public Buffer
{
public:
    /**
     * Get the number of elements contained in this buffer.
     */
    virtual size_t GetNumElements() const { return m_NumElements; }

    /**
     * Get the size in bytes of each element in this buffer.
     */
    virtual size_t GetElementSize() const { return m_ElementSize; }

    std::shared_ptr<ByteAddressBuffer> GetCounterBuffer() const { return m_CounterBuffer; }

protected:
    StructuredBuffer(bee::Device& device, size_t numElements, size_t elementSize);
    StructuredBuffer(bee::Device& device,
                     Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                     size_t numElements,
                     size_t elementSize);

    virtual ~StructuredBuffer() = default;

private:
    size_t m_NumElements;
    size_t m_ElementSize;

    // A buffer to store the internal counter for the structured buffer.
    std::shared_ptr<ByteAddressBuffer> m_CounterBuffer;
};
}  // namespace bee