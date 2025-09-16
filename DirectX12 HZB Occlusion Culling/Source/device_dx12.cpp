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

#include "pch_dx12.hpp"
#include "device_dx12.hpp"

#if defined(_DEBUG)
#include "dxgidebug.h"
#endif

using namespace bee;

#pragma region Class adapters for std::make_shared

class MakeUnorderedAccessView : public UnorderedAccessView
{
public:
    MakeUnorderedAccessView(Device& device,
                            const std::shared_ptr<DX12Resource>& resource,
                            const std::shared_ptr<DX12Resource>& counterResource,
                            const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
        : UnorderedAccessView(device, resource, counterResource, uav)
    {
    }

    virtual ~MakeUnorderedAccessView() {}
};

class MakeShaderResourceView : public ShaderResourceView
{
public:
    MakeShaderResourceView(Device& device,
                           const std::shared_ptr<DX12Resource>& resource,
                           const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
        : ShaderResourceView(device, resource, srv)
    {
    }

    virtual ~MakeShaderResourceView() {}
};

class MakeConstantBufferView : public ConstantBufferView
{
public:
    MakeConstantBufferView(Device& device, const std::shared_ptr<ConstantBuffer>& constantBuffer, size_t offset)
        : ConstantBufferView(device, constantBuffer, offset)
    {
    }

    virtual ~MakeConstantBufferView() {}
};

class MakePipelineStateObject : public PipelineStateObject
{
public:
    MakePipelineStateObject(Device& device, const D3D12_PIPELINE_STATE_STREAM_DESC& desc)
        : PipelineStateObject(device, desc)
    {
    }

    virtual ~MakePipelineStateObject() {}
};
class MakeRootSignature : public RootSignature
{
public:
    MakeRootSignature(Device& device, const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc)
        : RootSignature(device, rootSignatureDesc)
    {
    }

    virtual ~MakeRootSignature() {}
};

class MakeTexture : public DX12Texture
{
public:
    MakeTexture(Device& device, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue)
        : DX12Texture(device, resourceDesc, clearValue)
    {
    }

    MakeTexture(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const D3D12_CLEAR_VALUE* clearValue)
        : DX12Texture(device, resource, clearValue)
    {
    }

    virtual ~MakeTexture() {}
};

class MakeStructuredBuffer : public StructuredBuffer
{
public:
    MakeStructuredBuffer(Device& device, size_t numElements, size_t elementSize)
        : StructuredBuffer(device, numElements, elementSize)
    {
    }

    MakeStructuredBuffer(Device& device, ComPtr<ID3D12Resource> resource, size_t numElements, size_t elementSize)
        : StructuredBuffer(device, resource, numElements, elementSize)
    {
    }

    virtual ~MakeStructuredBuffer() {}
};

class MakeVertexBuffer : public VertexBuffer
{
public:
    MakeVertexBuffer(Device& device, size_t numVertices, size_t vertexStride)
        : VertexBuffer(device, numVertices, vertexStride)
    {
    }

    MakeVertexBuffer(Device& device, ComPtr<ID3D12Resource> resource, size_t numVertices, size_t vertexStride)
        : VertexBuffer(device, resource, numVertices, vertexStride)
    {
    }

    virtual ~MakeVertexBuffer() {}
};

class MakeIndexBuffer : public IndexBuffer
{
public:
    MakeIndexBuffer(Device& device, size_t numIndices, DXGI_FORMAT indexFormat)
        : IndexBuffer(device, numIndices, indexFormat)
    {
    }

    MakeIndexBuffer(Device& device,
                    Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                    size_t numIndices,
                    DXGI_FORMAT indexFormat)
        : IndexBuffer(device, resource, numIndices, indexFormat)
    {
    }

    virtual ~MakeIndexBuffer() = default;
};

class MakeConstantBuffer : public ConstantBuffer
{
public:
    MakeConstantBuffer(Device& device, ComPtr<ID3D12Resource> resource) : ConstantBuffer(device, resource) {}

    virtual ~MakeConstantBuffer() = default;
};

class MakeByteAddressBuffer : public ByteAddressBuffer
{
public:
    MakeByteAddressBuffer(Device& device, const D3D12_RESOURCE_DESC& desc) : ByteAddressBuffer(device, desc) {}

    MakeByteAddressBuffer(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
        : ByteAddressBuffer(device, resource)
    {
    }

    virtual ~MakeByteAddressBuffer() = default;
};

class MakeDescriptorAllocator : public DescriptorAllocator
{
public:
    MakeDescriptorAllocator(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerHeap = 256)
        : DescriptorAllocator(device, type, numDescriptorsPerHeap)
    {
    }

    virtual ~MakeDescriptorAllocator() {}
};

class MakeSwapChain : public SwapChain
{
public:
    MakeSwapChain(Device& device, HWND hWnd, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM)
        : SwapChain(device, hWnd, backBufferFormat)
    {
    }

    virtual ~MakeSwapChain() {}
};

class MakeCommandQueue : public CommandQueue
{
public:
    MakeCommandQueue(Device& device, D3D12_COMMAND_LIST_TYPE type) : CommandQueue(device, type) {}

    virtual ~MakeCommandQueue() {}
};

class MakeDevice : public Device
{
public:
    MakeDevice(std::shared_ptr<Adapter> adapter) : Device(adapter) {}

    virtual ~MakeDevice() {}
};
#pragma endregion

static void ErrorCallback(int error, const char* description) { Log::Error("Error {0}, {1}\n", error, description); }

uint64_t Device::GetFrameCount() const { return 0; }

void Device::EnableDebugLayer()
{
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
}

#if defined(_DEBUG)
void Device::ReportLiveObjects()
{
    IDXGIDebug1* dxgiDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    dxgiDebug->Release();
}
#endif

std::shared_ptr<Device> Device::Create(std::shared_ptr<Adapter> adapter) { return std::make_shared<MakeDevice>(adapter); }

std::wstring Device::GetDescription() const { return m_Adapter->GetDescription(); }

Device::Device(std::shared_ptr<Adapter> adapter) : m_Adapter(adapter)
{
    m_hInstance = GetModuleHandle(nullptr);

    if (!m_hInstance)
    {
        Log::Critical("Window :: Failed to retriece HINSTANCE \n");
        return;
    }

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window
    // to achieve 100% scaling while still allowing non-client window content to
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if defined(_DEBUG)
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

   if (!m_Adapter)
    {
        m_Adapter = Adapter::Create();
        assert(m_Adapter);
    }

    auto dxgiAdapter = m_Adapter->GetDXGIAdapter();

    ThrowIfFailed(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device)));
    
     // Enable debug messages (only works if the debug layer has already been enabled).
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(m_d3d12Device.As(&pInfoQueue)))
    {
        // Suppress whole categories of messages
        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,  // I'm really not sure how to avoid this
                                                                           // message.

            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,  // This warning occurs when using capture frame while graphics
                                                     // debugging.

            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,  // This warning occurs when using capture frame while graphics
                                                       // debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }

    m_DirectCommandQueue = std::make_unique<MakeCommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_ComputeCommandQueue = std::make_unique<MakeCommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_CopyCommandQueue = std::make_unique<MakeCommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_COPY);

     // Create descriptor allocators
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DescriptorAllocators[i] =
            std::make_unique<MakeDescriptorAllocator>(*this, static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
    }

    // Check features.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE,
                                                      &featureData,
                                                      sizeof(D3D12_FEATURE_DATA_ROOT_SIGNATURE))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
        m_HighestRootSignatureVersion = featureData.HighestVersion;
    }

    if (!glfwInit())
    {
        Log::Critical("GLFW init failed");
        assert(false);
        exit(EXIT_FAILURE);
    }

    Log::Info("GLFW version {}.{}.{}", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION);

    glfwSetErrorCallback(ErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_monitor = glfwGetPrimaryMonitor();

    m_width = 1920;
    m_height = 1080;

    m_window = glfwCreateWindow(m_width, m_height, "BEE", nullptr, nullptr);

    if (!m_window)
    {
        glfwTerminate();
        assert(false);
        exit(EXIT_FAILURE);
    }

    m_hwnd = glfwGetWin32Window(m_window);
}

Device::~Device() {}

CommandQueue& Device::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
    CommandQueue* commandQueue = nullptr;
    switch (type)
    {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            commandQueue = m_DirectCommandQueue.get();
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            commandQueue = m_ComputeCommandQueue.get();
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            commandQueue = m_CopyCommandQueue.get();
            break;
        default:
            assert(false && "Invalid command queue type.");
    }

    return *commandQueue;
}

void Device::Flush()
{
    m_DirectCommandQueue->Flush();
    m_ComputeCommandQueue->Flush();
    m_CopyCommandQueue->Flush();
}

DescriptorAllocation Device::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
    return m_DescriptorAllocators[type]->Allocate(numDescriptors);
}

void Device::ReleaseStaleDescriptors()
{
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DescriptorAllocators[i]->ReleaseStaleDescriptors();
    }
}

std::shared_ptr<SwapChain> Device::CreateSwapChain(DXGI_FORMAT backBufferFormat)
{
    std::shared_ptr<SwapChain> swapChain;
    swapChain = std::make_shared<MakeSwapChain>(*this, m_hwnd, backBufferFormat);

    return swapChain;
}

std::shared_ptr<ConstantBuffer> Device::CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
    std::shared_ptr<ConstantBuffer> constantBuffer = std::make_shared<MakeConstantBuffer>(*this, resource);

    return constantBuffer;
}

std::shared_ptr<ByteAddressBuffer> Device::CreateByteAddressBuffer(size_t bufferSize)
{
    // Align-up to 4-bytes
    bufferSize = Math::AlignUp(bufferSize, 4);

    std::shared_ptr<ByteAddressBuffer> buffer = std::make_shared<MakeByteAddressBuffer>(
        *this,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));

    return buffer;
}

std::shared_ptr<ByteAddressBuffer> Device::CreateByteAddressBuffer(ComPtr<ID3D12Resource> resource)
{
    std::shared_ptr<ByteAddressBuffer> buffer = std::make_shared<MakeByteAddressBuffer>(*this, resource);

    return buffer;
}

std::shared_ptr<StructuredBuffer> Device::CreateStructuredBuffer(size_t numElements, size_t elementSize)
{
    std::shared_ptr<StructuredBuffer> structuredBuffer =
        std::make_shared<MakeStructuredBuffer>(*this, numElements, elementSize);

    return structuredBuffer;
}

std::shared_ptr<StructuredBuffer> Device::CreateStructuredBuffer(ComPtr<ID3D12Resource> resource,
                                                                 size_t numElements,
                                                                 size_t elementSize)
{
    std::shared_ptr<StructuredBuffer> structuredBuffer =
        std::make_shared<MakeStructuredBuffer>(*this, resource, numElements, elementSize);

    return structuredBuffer;
}

std::shared_ptr<IndexBuffer> Device::CreateIndexBuffer(size_t numIndices, DXGI_FORMAT indexFormat)
{
    std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<MakeIndexBuffer>(*this, numIndices, indexFormat);

    return indexBuffer;
}

std::shared_ptr<IndexBuffer> Device::CreateIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                                         size_t numIndices,
                                                                         DXGI_FORMAT indexFormat)
{
    std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<MakeIndexBuffer>(*this, resource, numIndices, indexFormat);

    return indexBuffer;
}

std::shared_ptr<VertexBuffer> Device::CreateVertexBuffer(size_t numVertices, size_t vertexStride)
{
    std::shared_ptr<VertexBuffer> vertexBuffer = std::make_shared<MakeVertexBuffer>(*this, numVertices, vertexStride);

    return vertexBuffer;
}

std::shared_ptr<VertexBuffer> Device::CreateVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                                           size_t numVertices,
                                                                           size_t vertexStride)
{
    std::shared_ptr<VertexBuffer> vertexBuffer = std::make_shared<MakeVertexBuffer>(*this, resource, numVertices, vertexStride);

    return vertexBuffer;
}

std::shared_ptr<DX12Texture> Device::CreateTexture(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue)
{
    std::shared_ptr<DX12Texture> texture = std::make_shared<MakeTexture>(*this, resourceDesc, clearValue);

    return texture;
}

std::shared_ptr<DX12Texture> Device::CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                   const D3D12_CLEAR_VALUE* clearValue)
{
    std::shared_ptr<DX12Texture> texture = std::make_shared<MakeTexture>(*this, resource, clearValue);

    return texture;
}

std::shared_ptr<RootSignature> Device::CreateRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC1& rootSignatureDesc)
{
    std::shared_ptr<RootSignature> rootSignature = std::make_shared<MakeRootSignature>(*this, rootSignatureDesc);

    return rootSignature;
}

std::shared_ptr<PipelineStateObject> Device::DoCreatePipelineStateObject(
    const D3D12_PIPELINE_STATE_STREAM_DESC& pipelineStateStreamDesc)
{
    std::shared_ptr<PipelineStateObject> pipelineStateObject =
        std::make_shared<MakePipelineStateObject>(*this, pipelineStateStreamDesc);

    return pipelineStateObject;
}

std::shared_ptr<ConstantBufferView> Device::CreateConstantBufferView(const std::shared_ptr<ConstantBuffer>& constantBuffer,
                                                                     size_t offset)
{
    std::shared_ptr<ConstantBufferView> constantBufferView =
        std::make_shared<MakeConstantBufferView>(*this, constantBuffer, offset);

    return constantBufferView;
}

std::shared_ptr<ShaderResourceView> Device::CreateShaderResourceView(const std::shared_ptr<DX12Resource>& resource,
                                                                     const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
    std::shared_ptr<ShaderResourceView> shaderResourceView = std::make_shared<MakeShaderResourceView>(*this, resource, srv);

    return shaderResourceView;
}

std::shared_ptr<UnorderedAccessView> Device::CreateUnorderedAccessView(const std::shared_ptr<DX12Resource>& resource,
                                                                       const std::shared_ptr<DX12Resource>& counterResource,
                                                                       const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
{
    std::shared_ptr<UnorderedAccessView> unorderedAccessView =
        std::make_shared<MakeUnorderedAccessView>(*this, resource, counterResource, uav);

    return unorderedAccessView;
}

DXGI_SAMPLE_DESC Device::GetMultisampleQualityLevels(DXGI_FORMAT format,
                                                     UINT numSamples,
                                                     D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags) const
{
    DXGI_SAMPLE_DESC sampleDesc = {1, 0};

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
    qualityLevels.Format = format;
    qualityLevels.SampleCount = 1;
    qualityLevels.Flags = flags;
    qualityLevels.NumQualityLevels = 0;

    while (qualityLevels.SampleCount <= numSamples &&
           SUCCEEDED(m_d3d12Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                        &qualityLevels,
                                                        sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))) &&
           qualityLevels.NumQualityLevels > 0)
    {
        // That works...
        sampleDesc.Count = qualityLevels.SampleCount;
        sampleDesc.Quality = qualityLevels.NumQualityLevels - 1;

        // But can we do better?
        qualityLevels.SampleCount *= 2;
    }

    return sampleDesc;
}

int Device::GetWidth() const { return m_width; }

int Device::GetHeight() const { return m_height; }

GLFWwindow* Device::GetWindow() const { return m_window; }

HWND Device::GetHwnd() const { return m_hwnd; }

GLFWmonitor* Device::GetMonitor() const { return m_monitor; }

float Device::GetMonitorUIScale() const
{
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetMonitorContentScale(m_monitor, &xscale, &yscale);
    return xscale;
}

bool Device::ShouldClose() { return glfwWindowShouldClose(m_window); }