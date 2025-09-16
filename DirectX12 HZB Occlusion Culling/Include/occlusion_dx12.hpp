#pragma once

#include "render_target_dx12.hpp"

#include "descriptor_allocation_dx12.hpp"
#include "buffers_dx12.hpp"

#include "heap_dx12.hpp"
#include "gpu_resource_dx12.hpp"

#include <wrl.h>
using namespace Microsoft::WRL;

#include <DirectXMath.h>
using namespace DirectX;

struct FrustumPlanes;

class CommandList;
class SwapChain;
class RootSignature;
class PipelineStateObject;
struct VertexPosColor;
struct InstanceData;

struct ConstantData
{
    unsigned int maxHzbMip;
    unsigned int numObjects;
};

struct RenderPass
{
    std::shared_ptr<bee::PipelineStateObject> pso;
    std::shared_ptr<bee::RootSignature> rs;
};

class OcclusionCulling
{
public:
    OcclusionCulling(const OcclusionCulling&) = delete;
    OcclusionCulling& operator=(const OcclusionCulling&) = delete;

    static OcclusionCulling& GetInstance()
    {
        static OcclusionCulling instance;
        return instance;
    }

    void Initialize(std::shared_ptr<std::array<VertexPosColor, 8>> vertexBuffer,
                    std::shared_ptr<std::array<WORD, 36>> indexBuffer,
                    std::shared_ptr<std::vector<InstanceData>> instanceData,
                    std::shared_ptr<Heap> aabbHeap,
                    std::shared_ptr<GpuResource> aabbBuffer,
                    std::shared_ptr<bee::RenderTarget> renderTarget,
                    const int numObjects);

    void Update(XMMATRIX& vpMatrix);
    void Render(XMMATRIX& mainCameraVP, XMMATRIX* debugCameraVP = nullptr);

    void ToggleFrustumCulling() { m_doFrustumCulling = !m_doFrustumCulling; }
    void ToggleHzbCulling() { m_doHzbCulling = !m_doHzbCulling; }
    void ToggleRenderCulling() { m_renderCulling = !m_renderCulling; }

    void SetMipToDisplay(unsigned int mip) { m_mipToDisplay = mip; }
    void IncrementMipToDisplay();
    void DecrementMipToDisplay();

private:
    OcclusionCulling() {};

    void InitPSOs();
    void AttachRenderTargets();
    void InitViews();
    void PopulateResources();

    void CreateStructuredBuffer(ComPtr<ID3D12Resource>& resource,
                                UINT numElements,
                                UINT elementSize,
                                D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    void HzbCulling(XMMATRIX& mainCameraVP, XMMATRIX* debugCameraVP);
    void VisualizeMipMaps(XMMATRIX* cameraVP);

    void InitDepthPSO();
    void InitDrawPSO();
    void InitVisualizeMipsPSO();
    void InitCullingPSO();
    void InitFirstPrefixPSO();
    void InitSecondPrefixPSO();
    void InitRecursivePrefixPSO();
    void InitFillIndirectBufferPSO();
    void InitIndirectDrawPSO();
    void InitIndirectDepthPSO();

    void FirstFrameDepthPass(std::shared_ptr<CommandList> commandList, XMMATRIX& vpMatrix);
    void FirstFrameDrawPass(XMMATRIX* cameraVP);
    void GenerateMipsPass(std::shared_ptr<bee::DX12Texture>& texture);
    void CullingPass(std::shared_ptr<bee::DX12Texture>& texture, XMMATRIX& vpMatrix, UINT16 numMips);
    void PrefixSumPass(UINT numElements);
    void FillIndirectPass();
    void IndirectDrawPass(XMMATRIX* cameraVP);
    void IndirectDepthPass(std::shared_ptr<CommandList> commandList, XMMATRIX& vpMatrix);

    void PopulateBuffer(std::shared_ptr<CommandList>& commandList,
                        ComPtr<ID3D12Resource>& resource,
                        ComPtr<ID3D12Resource>& uploadBuffer,
                        void* data,
                        UINT numElements,
                        UINT elementSize);

    std::shared_ptr<bee::Device> m_device;
    std::shared_ptr<bee::RenderTarget> m_renderTarget;

    // First-Frame
    RenderPass m_depthPass;
    RenderPass m_drawPass;

    // Debug
    RenderPass m_visualizeMipsPass;

    // Culling
    RenderPass m_generateMipsPass;
    RenderPass m_cullingPass;

    // Prefix Sum
    RenderPass m_firstPrefixPass;
    RenderPass m_secondPrefixPass;
    RenderPass m_recursivePrefixPass;

    // Execute Indirect
    RenderPass m_fillIndirectBufferPass;
    RenderPass m_indirectDrawPass;
    RenderPass m_indirectDepthPass;

    GpuResource m_instanceData;
    GpuResource m_vp;
    GpuResource m_visibility;
    GpuResource m_scanResult;
    GpuResource m_count;
    GpuResource m_matrixIndex;
    GpuResource m_indirectArgs;
    std::vector<GpuResource> m_groupSums;

    ComPtr<ID3D12CommandSignature> m_commandSignature;

    Heap m_srvHeap{L"srv Heap", 32};
    Heap m_uavHeap{L"uav Heap", 32};

    std::shared_ptr<Heap> m_aabbHeap = nullptr;
    std::shared_ptr<GpuResource> m_aabbBuffer = nullptr;

    ConstantData m_constantData;

    std::shared_ptr<std::array<VertexPosColor, 8>> m_vertexBuffer = nullptr;
    std::shared_ptr<std::array<WORD, 36>> m_indexBuffer = nullptr;
    std::shared_ptr<std::vector<InstanceData>> m_instanceDataBuffer = nullptr;

    std::shared_ptr<FrustumPlanes> m_FrustumPlanes = nullptr;

    uint32_t m_numVertices = 0;
    uint32_t m_numIndices = 0;
    uint32_t m_numInstances = 0;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    int m_width = 0;
    int m_height = 0;
    int m_numObjects = 0;
    int m_numWorkGroupBuffers = 0;
    unsigned int m_mipToDisplay = 0;

    bool m_doFrustumCulling = true;
    bool m_doHzbCulling = true;
    bool m_renderCulling = true;
    bool m_isFirstFrame = true;
    bool m_initialized = false;
};