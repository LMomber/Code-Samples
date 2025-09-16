#pragma once

#include "core/ecs.hpp"

#include "render_target_dx12.hpp"

#include "descriptor_allocation_dx12.hpp"

#include "heap_dx12.hpp"
#include "gpu_resource_dx12.hpp"

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

#include <cstdint>  // For uint32_t
#include <memory>   // For std::unique_ptr and std::smart_ptr
#include <string>   // For std::wstring
#include <vector>   // For std::vector

struct AABB;
struct FrustumPlanes;

enum Mode
{
    MODE_DEFAULT = 0,
    MODE_DEBUG = 1
};

class CommandList;
class Device;
class SwapChain;
class OcclusionCulling;

class Renderer : public System
{
public:
    Renderer();
    virtual ~Renderer();

    void Render();
    void Update(float);

    bool LoadContent();
    void UnloadContent();

    bee::Entity GetCameraEntity() const;
    void SwitchMode();

    void InitCameras();
    void InitCubes();
    void InitView();

    void PopulateResources();

private:
    friend class InputHandler;

    DirectX::XMMATRIX m_ProjectionMatrix = {};

    std::vector<AABB> m_objects;

    std::vector<Entity> m_cameraEntities;  // [0] is main camera, [1] is debug camera

    Mode m_mode = MODE_DEFAULT;

    DirectX::XMMATRIX GetCameraVP(Mode mode);

    void CreateStructuredBuffer(ComPtr<ID3D12Resource>& resource,
                                UINT numElements,
                                UINT elementSize,
                                D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    void InitDescriptorHeap(ComPtr<ID3D12DescriptorHeap>& heap,
                            UINT numDescriptors = 1,
                            D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                            D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    void CreateSRV(ComPtr<ID3D12Resource>& resource,
                   ComPtr<ID3D12DescriptorHeap>& heap,
                   UINT numElements,
                   UINT elementSize,
                   D3D12_CPU_DESCRIPTOR_HANDLE* handle = nullptr,
                   UINT64 firstElement = 0);
    void CreateUAV(ComPtr<ID3D12Resource>& resource,
                   ComPtr<ID3D12DescriptorHeap>& heap,
                   UINT numElements,
                   UINT elementSize,
                   D3D12_CPU_DESCRIPTOR_HANDLE* handle = nullptr,
                   UINT64 firstElement = 0);
    void PopulateBuffer(std::shared_ptr<CommandList>& commandList,
                        ComPtr<ID3D12Resource>& resource,
                        ComPtr<ID3D12Resource>& uploadBuffer,
                        void* data,
                        UINT numElements,
                        UINT elementSize);

    float m_FoV = 60.f;

    // DX12 Device.
    std::shared_ptr<bee::Device> m_Device;
    std::shared_ptr<bee::SwapChain> m_SwapChain;

    // Render target
    std::shared_ptr<bee::RenderTarget> m_RenderTarget;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    int m_Width;
    int m_Height;
    bool m_VSync;

    Heap m_aabbHeap{L"aabb Heap", 32};
    GpuResource m_aabbBuffer;

    UINT m_handleIncrementSize = 0;
};