#pragma once

// Changed and added upon:

/*
 *  Copyright(c) 2020 Jeremiah van Oosten
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
 *  @file Device.h
 *  @date October 9, 2020
 *  @author Jeremiah van Oosten
 *
 *  @brief A wrapper for the D3D12Device.
 */

#include "views_dx12.hpp"

#include "core/engine.hpp"

#include <dx12Headers/include/directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class Adapter;
class CommandQueue;
class CommandList;
class DescriptorAllocation;
class DescriptorAllocator;
class PipelineStateObject;
class RenderTarget;
class RootSignature;
class Scene;
class SwapChain;
class DX12Texture;
class Renderer;

class Device
{
public:
    int GetWidth() const;
    int GetHeight() const;

    GLFWwindow* GetWindow() const;
    HWND GetHwnd() const;
    GLFWmonitor* GetMonitor() const;
    float GetMonitorUIScale() const;
    bool ShouldClose();
    void BeginFrame() {}
    void EndFrame() {}
    void Update(){};

    uint64_t GetFrameCount() const;

    static void EnableDebugLayer();

    static void ReportLiveObjects();

    static std::shared_ptr<Device> Create(std::shared_ptr<Adapter> adapter = nullptr);

    std::wstring GetDescription() const;

    bee::DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors = 1);

    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
    {
        return m_d3d12Device->GetDescriptorHandleIncrementSize(type);
    }

    std::shared_ptr<SwapChain> CreateSwapChain(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM);

    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
    std::shared_ptr<ByteAddressBuffer> CreateByteAddressBuffer(size_t bufferSize);
    std::shared_ptr<ByteAddressBuffer> CreateByteAddressBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource);
    std::shared_ptr<StructuredBuffer> CreateStructuredBuffer(size_t numElements, size_t elementSize);
    std::shared_ptr<StructuredBuffer> CreateStructuredBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                             size_t numElements,
                                                             size_t elementSize);
    std::shared_ptr<DX12Texture> CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc,
                                               const D3D12_CLEAR_VALUE* clearValue = nullptr);
    std::shared_ptr<DX12Texture> CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                           const D3D12_CLEAR_VALUE* clearValue = nullptr);
    std::shared_ptr<IndexBuffer> CreateIndexBuffer(size_t numIndices, DXGI_FORMAT indexFormat);
    std::shared_ptr<IndexBuffer> CreateIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                   size_t numIndices,
                                                   DXGI_FORMAT indexFormat);
    std::shared_ptr<VertexBuffer> CreateVertexBuffer(size_t numVertices, size_t vertexStride);
    std::shared_ptr<VertexBuffer> CreateVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                     size_t numVertices,
                                                     size_t vertexStride);
    std::shared_ptr<RootSignature> CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc);

    template <class PipelineStateStream>
    std::shared_ptr<PipelineStateObject> CreatePipelineStateObject(PipelineStateStream& pipelineStateStream)
    {
        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(PipelineStateStream), &pipelineStateStream};

        return DoCreatePipelineStateObject(pipelineStateStreamDesc);
    }

    std::shared_ptr<ConstantBufferView> CreateConstantBufferView(const std::shared_ptr<ConstantBuffer>& constantBuffer,
                                                                 size_t offset = 0);
    std::shared_ptr<ShaderResourceView> CreateShaderResourceView(const std::shared_ptr<DX12Resource>& resource,
                                                                 const D3D12_SHADER_RESOURCE_VIEW_DESC* srv = nullptr);
    std::shared_ptr<UnorderedAccessView> CreateUnorderedAccessView(
        const std::shared_ptr<DX12Resource>& resource,
        const std::shared_ptr<DX12Resource>& counterResource = nullptr,
                                                                   const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav = nullptr);

    /**
     * Flush all command queues.
     */
    void Flush();

    /**
     * Release stale descriptors. This should only be called with a completed frame counter.
     */
    void ReleaseStaleDescriptors();

    std::shared_ptr<Adapter> GetAdapter() const { return m_Adapter; }

    CommandQueue& GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

    Microsoft::WRL::ComPtr<ID3D12Device2> GetD3D12Device() const { return m_d3d12Device; }

    D3D_ROOT_SIGNATURE_VERSION GetHighestRootSignatureVersion() const { return m_HighestRootSignatureVersion; }

    /**
     * Check if the requested multisample quality is supported for the given format.
     */
    DXGI_SAMPLE_DESC GetMultisampleQualityLevels(
        DXGI_FORMAT format,
        UINT numSamples = D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT,
        D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE) const;

protected:
    friend class EngineClass;

    explicit Device(std::shared_ptr<Adapter> adapter);
    virtual ~Device();

    std::shared_ptr<PipelineStateObject> DoCreatePipelineStateObject(
        const D3D12_PIPELINE_STATE_STREAM_DESC& pipelineStateStreamDesc);

private:
    HINSTANCE m_hInstance = {};
    HWND m_hwnd = {};

    GLFWwindow* m_window = nullptr;
    GLFWmonitor* m_monitor = nullptr;
    bool m_vsync = true;
    bool m_fullscreen = false;
    int m_width = -1;
    int m_height = -1;

    Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;

    std::shared_ptr<Adapter> m_Adapter;

    std::unique_ptr<CommandQueue> m_DirectCommandQueue;
    std::unique_ptr<CommandQueue> m_ComputeCommandQueue;
    std::unique_ptr<CommandQueue> m_CopyCommandQueue;

    std::unique_ptr<DescriptorAllocator> m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    D3D_ROOT_SIGNATURE_VERSION m_HighestRootSignatureVersion;
};