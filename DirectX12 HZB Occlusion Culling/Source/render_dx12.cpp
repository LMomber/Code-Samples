#include "pch_dx12.hpp"
#include "render_dx12.hpp"

#include "bounding_volumes.hpp"
#include "frustum.hpp"
#include "tools/random.hpp"

#include "rendering/render_components.hpp"

#include "core/engine.hpp"
#include "core/transform.hpp"
#include "core/ecs.hpp"

#include "occlusion_dx12.hpp"
#include "occlusion_helpers_dx12.hpp"
#include "render_helpers.hpp"

using namespace DirectX;

#include <algorithm>   // For std::min, std::max, and std::clamp.
#include <functional>  // For std::bind
#include <string>      // For std::wstring
#include <array>
#include <vector>

struct IndirectCommand
{
    unsigned int IndexCountPerInstance;
    unsigned int InstanceCount;
    unsigned int StartIndexLocation;
    int BaseVertexLocation;
    unsigned int StartInstanceLocation;
};

Renderer::Renderer()
    : m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
      m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(1920), static_cast<float>(1080))),
      m_Width(1920),
      m_Height(1080),
      m_VSync(false)
{
    LoadContent();
}

Renderer::~Renderer() {}

bool Renderer::LoadContent()
{
    // Create the DX12 device.
    m_Device = Engine.m_device;

    // Create a swap chain.
    m_SwapChain = m_Device->CreateSwapChain(DXGI_FORMAT_R8G8B8A8_UNORM);
    m_SwapChain->SetVSync(m_VSync);

    m_RenderTarget = std::make_shared<RenderTarget>();

    InitCameras();
    InitCubes();
    InitView();

    PopulateResources();

    OcclusionCulling::GetInstance().Initialize(std::make_shared<std::array<VertexPosColor, 8>>(render::vertexData),
                                               std::make_shared<std::array<WORD, 36>>(render::indexData),
                                               std::make_shared<std::vector<InstanceData>>(render::instanceData),
                                               std::make_shared<Heap>(m_aabbHeap),
                                               std::make_shared<GpuResource>(m_aabbBuffer),
                                               m_RenderTarget,
                                               render::numInstances);

    return true;
}

void Renderer::UnloadContent()
{
    m_RenderTarget->Reset();

    m_SwapChain.reset();
    m_Device.reset();
}

Entity Renderer::GetCameraEntity() const
{
    if (m_mode == Mode::MODE_DEFAULT) return m_cameraEntities[0];
    if (m_mode == Mode::MODE_DEBUG) return m_cameraEntities[1];

    Log().Critical("Invalid camera mode");

    return Entity();
}

void Renderer::SwitchMode()
{
    if (m_mode == Mode::MODE_DEFAULT)
    {
        m_mode = Mode::MODE_DEBUG;

        auto& debugCameraTransform = Engine.ECS().Registry.get<Transform>(m_cameraEntities[1]);
        auto& mainCameraTransform = Engine.ECS().Registry.get<Transform>(m_cameraEntities[0]);

        debugCameraTransform = mainCameraTransform;
    }
    else if (m_mode == Mode::MODE_DEBUG)
    {
        m_mode = Mode::MODE_DEFAULT;
    }
    else
    {
        Log().Critical("Invalid camera mode");
    }
}

void Renderer::InitCameras()
{
    // Update the projection matrix.
    float aspectRatio = Engine.Device().GetWidth() / static_cast<float>(Engine.Device().GetHeight());
    m_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), aspectRatio, 0.1f, 1000.0f);

    glm::vec3 translation = {0, 0, 0};
    auto registryView = Engine.ECS().Registry.view<Transform, Camera>().each();

    if (registryView.begin() == registryView.end())
    {
        Log().Critical("No camera entity found in the scene. Create a camera before creating the renderer");
    }

    auto cameraIndex = registryView.begin();
    cameraIndex++;

    // Check if there is more than 1 camera in the view
    if (cameraIndex != registryView.end())
    {
        Log().Critical("More than 1 camera entity found in the scene. Only 1 camera is supported for now");
    }

    // Store the main camera entity
    for (const auto& [e, cameraTransform, camera] : registryView)
    {
        m_cameraEntities.push_back(e);
        translation = cameraTransform.GetTranslation();
    }

    // Create a debug camera
    auto ent = Engine.ECS().CreateEntity();
    auto& transform = Engine.ECS().CreateComponent<Transform>(ent);
    transform.SetTranslation(translation);
    Engine.ECS().CreateComponent<Camera>(ent);

    // Store the debug camera entity
    m_cameraEntities.push_back(ent);
}

void Renderer::InitCubes()
{
    render::instanceData.resize(render::numInstances);

    const float scale = 1.f;
    AABB cubeAABB{};
    cubeAABB.min = {-scale, -scale, -scale};
    cubeAABB.max = {scale, scale, scale};

    xorshift32_state xss;
    xss.a = 1431;
    const int modulus = 200000;
    for (int i = 0; i < render::numInstances; i++)
    {
        // make a uniform matrix
        render::instanceData[i].WorldMatrix =
            DirectX::XMMatrixTranslation(static_cast<float>(xorshift32(&xss) % modulus) * 0.001f,
                                                                  static_cast<float>(xorshift32(&xss) % modulus) * 0.001f,
                                                                  static_cast<float>(xorshift32(&xss) % modulus) * 0.001f);
        render::instanceData[i].WorldMatrix =
            DirectX::XMMatrixMultiply(render::instanceData[i].WorldMatrix, DirectX::XMMatrixScaling(scale, scale, scale));

        m_objects.push_back(TransformAABB(cubeAABB, render::instanceData[i].WorldMatrix));
    }
}

void Renderer::InitView()
{
    auto& resource = m_aabbBuffer.GetResource();
    CreateStructuredBuffer(resource, render::numInstances, sizeof(AABB));
    resource->SetName(L"aabb resource");
    m_aabbBuffer.SetSRV(m_aabbHeap.CreateSRV(resource, render::numInstances, sizeof(AABB)));  // 0 
}

void Renderer::PopulateResources()
{
    auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList = commandQueue.GetCommandList();

    ComPtr<ID3D12Resource> uploadBuffer;
    auto& aabbResource = m_aabbBuffer.GetResource();
    PopulateBuffer(commandList,
                   aabbResource, uploadBuffer, m_objects.data(), static_cast<UINT>(m_objects.size()), sizeof(AABB));

    const auto fence = commandQueue.ExecuteCommandList(commandList);
    commandQueue.WaitForFenceValue(fence);
}

void Renderer::Render()
{
    XMMATRIX cameraVP = GetCameraVP(Mode::MODE_DEFAULT);
    XMMATRIX debugCameraVP;
    XMMATRIX* vpPointer = nullptr;

    if (m_mode == Mode::MODE_DEBUG)
    {
        debugCameraVP = GetCameraVP(Mode::MODE_DEBUG);
        vpPointer = &debugCameraVP;
    }

    OcclusionCulling::GetInstance().Render(cameraVP, vpPointer);

    m_SwapChain->Present(m_RenderTarget->GetTexture(AttachmentPoint::Color0));
}

void Renderer::Update(float)
{
    // Always use main camera for culling
    XMMATRIX vpMatrix = GetCameraVP(Mode::MODE_DEFAULT);

    // Add occlusion update
    OcclusionCulling::GetInstance().Update(vpMatrix);
}

XMMATRIX Renderer::GetCameraVP(Mode mode)
{
    auto& cameraTransform = Engine.ECS().Registry.get<Transform>(m_cameraEntities[mode]);

    // Convert Transform data to DirectXMath types
    const XMVECTOR cameraPosition = XMVectorSet(cameraTransform.GetTranslation().x,
                                                cameraTransform.GetTranslation().y,
                                                cameraTransform.GetTranslation().z,
                                                1.0f);

    const XMVECTOR cameraRotation = XMVectorSet(cameraTransform.GetRotation().x,
                                                cameraTransform.GetRotation().y,
                                                cameraTransform.GetRotation().z,
                                                cameraTransform.GetRotation().w);  // Quaternion (x, y, z, w)

    // Compute forward vector
    const XMVECTOR localForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(localForward, cameraRotation));

    // Compute focus point
    const XMVECTOR focusPoint = XMVectorAdd(cameraPosition, forward);

    // Compute right and up vectors
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, worldUp));
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(right, forward));

    // Create view matrix
    XMMATRIX cameraViewMatrix = XMMatrixLookAtLH(cameraPosition, focusPoint, up);

    // Compute the view-projection matrix
    return XMMatrixMultiply(cameraViewMatrix, m_ProjectionMatrix);
}

void Renderer::CreateStructuredBuffer(ComPtr<ID3D12Resource>& resource,
                                           UINT numElements,
                                           UINT elementSize,
                                           D3D12_RESOURCE_FLAGS flags)
{
    D3D12_RESOURCE_DESC instanceBufferDesc = {};
    instanceBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    instanceBufferDesc.Width = elementSize * numElements;  // Total buffer size
    instanceBufferDesc.Height = 1;
    instanceBufferDesc.DepthOrArraySize = 1;
    instanceBufferDesc.MipLevels = 1;
    instanceBufferDesc.Format = DXGI_FORMAT_UNKNOWN;  // Structured buffers use a typeless format
    instanceBufferDesc.SampleDesc.Count = 1;
    instanceBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    instanceBufferDesc.Flags = flags;

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    m_Device->GetD3D12Device()->CreateCommittedResource(&heapProperties,  // Default heap for GPU access
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &instanceBufferDesc,
                                                        D3D12_RESOURCE_STATE_COMMON,  // Initial state
                                                        nullptr,
                                                        IID_PPV_ARGS(&resource));

    ResourceStateTracker::AddGlobalResourceState(resource.Get(), D3D12_RESOURCE_STATE_COMMON);
}

void Renderer::InitDescriptorHeap(ComPtr<ID3D12DescriptorHeap>& heap,
                                       UINT numDescriptors,
                                       D3D12_DESCRIPTOR_HEAP_TYPE type,
                                       D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = flags;

    ThrowIfFailed(m_Device->GetD3D12Device()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));
}

void Renderer::CreateSRV(ComPtr<ID3D12Resource>& resource,
                              ComPtr<ID3D12DescriptorHeap>& heap,
                              UINT numElements,
                              UINT elementSize,
                              D3D12_CPU_DESCRIPTOR_HANDLE* handle,
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

    if (handle)
    {
        m_Device->GetD3D12Device()->CreateShaderResourceView(resource.Get(), &srvDesc, *handle);
        handle->ptr += m_handleIncrementSize;
    }
    else
    {
        auto newHandle = heap->GetCPUDescriptorHandleForHeapStart();
        m_Device->GetD3D12Device()->CreateShaderResourceView(resource.Get(), &srvDesc, newHandle);
    }
}

void Renderer::CreateUAV(ComPtr<ID3D12Resource>& resource,
                              ComPtr<ID3D12DescriptorHeap>& heap,
                              UINT numElements,
                              UINT elementSize,
                              D3D12_CPU_DESCRIPTOR_HANDLE* handle,
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

    if (handle)
    {
        m_Device->GetD3D12Device()->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, *handle);
        handle->ptr += m_handleIncrementSize;
    }
    else
    {
        auto newHandle = heap->GetCPUDescriptorHandleForHeapStart();
        m_Device->GetD3D12Device()->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, newHandle);
    }
}

void Renderer::PopulateBuffer(std::shared_ptr<CommandList>& commandList,
                                   ComPtr<ID3D12Resource>& resource,
                                   ComPtr<ID3D12Resource>& uploadBuffer,
                                   void* data,
                                   UINT numElements,
                                   UINT elementSize)
{
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = elementSize * numElements;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        m_Device->GetD3D12Device()->CreateCommittedResource(&uploadHeapProps,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &uploadDesc,
                                                            D3D12_RESOURCE_STATE_GENERIC_READ,  // Must be in a readable state
                                                            nullptr,
                                                            IID_PPV_ARGS(&uploadBuffer)));

    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, data, elementSize * numElements);  // Copy your data here
    uploadBuffer->Unmap(0, nullptr);

    commandList->GetD3D12CommandList()->CopyBufferRegion(resource.Get(),              // Destination resource
                                                         0,                           // Destination offset
                                                         uploadBuffer.Get(),          // Source resource
                                                         0,                           // Source offset
                                                         elementSize * numElements);  // Size of the data
}