#include "pch_dx12.hpp"
#include "occlusion_dx12.hpp"

#include "occlusion_helpers_dx12.hpp"
#include "frustum.hpp"

using namespace DirectX;

struct IndirectCommand
{
    unsigned int IndexCountPerInstance;
    unsigned int InstanceCount;
    unsigned int StartIndexLocation;
    int BaseVertexLocation;
    unsigned int StartInstanceLocation;
};

void OcclusionCulling::Initialize(std::shared_ptr<std::array<VertexPosColor, 8>> vertexBuffer,
                                       std::shared_ptr<std::array<WORD, 36>> indexBuffer,
                                       std::shared_ptr<std::vector<InstanceData>> instanceData,
                                       std::shared_ptr<Heap> aabbHeap,
                                       std::shared_ptr<GpuResource> aabbBuffer,
                                       std::shared_ptr<RenderTarget> renderTarget,
                                       const int numObjects)
{
    assert(!m_initialized && "The culling class is already initialized");

    m_vertexBuffer = vertexBuffer;
    m_numVertices = static_cast<uint32_t>(vertexBuffer->size());
    m_indexBuffer = indexBuffer;
    m_numIndices = static_cast<uint32_t>(indexBuffer->size());
    m_instanceDataBuffer = instanceData;
    m_numInstances = static_cast<uint32_t>(instanceData->size());
    m_aabbHeap = aabbHeap;
    m_aabbBuffer = aabbBuffer;
    m_renderTarget = renderTarget;
    m_numObjects = numObjects;

    m_device = Engine.m_device;

    m_width = m_device->GetWidth();
    m_height = m_device->GetHeight();
    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
    m_scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

    m_FrustumPlanes = std::make_shared<FrustumPlanes>();

    // Init PSOs
    InitPSOs();

    AttachRenderTargets();

    InitViews();

    PopulateResources();

    m_initialized = true;
}

void OcclusionCulling::Update(XMMATRIX& cameraVP) { ExtractPlanes(m_FrustumPlanes->planes, cameraVP, false); }

void OcclusionCulling::Render(XMMATRIX& mainCameraVP, XMMATRIX* debugCameraVP)
{
    if (m_renderCulling)
    {
        HzbCulling(mainCameraVP, debugCameraVP);
    }
    else
    {
        VisualizeMipMaps(&mainCameraVP);
    }
}

void OcclusionCulling::InitPSOs()
{
    InitDepthPSO();
    InitDrawPSO();
    InitVisualizeMipsPSO();
    InitCullingPSO();
    InitFirstPrefixPSO();
    InitSecondPrefixPSO();
    InitRecursivePrefixPSO();
    InitFillIndirectBufferPSO();
    InitIndirectDrawPSO();
    InitIndirectDepthPSO();
}

void OcclusionCulling::AttachRenderTargets()
{
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
                                                  m_width,
                                                  m_height,
                                                  1,
                                                  1,
                                                  sampleDesc.Count,
                                                  sampleDesc.Quality,
                                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = colorDesc.Format;
    colorClearValue.Color[0] = 0.4f;
    colorClearValue.Color[1] = 0.6f;
    colorClearValue.Color[2] = 0.9f;
    colorClearValue.Color[3] = 1.0f;

    auto colorTexture = m_device->CreateTexture(colorDesc, &colorClearValue);
    colorTexture->SetName(L"Color Render Target");

    //////////////////////////////////////////

    UINT16 numMips = static_cast<UINT16>(std::log2(std::max(m_width, m_height))) + 1;

    // Create a depth buffer.
    auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS,
                                                  m_width,
                                                  m_height,
                                                  1,
                                                  numMips,
                                                  sampleDesc.Count,
                                                  sampleDesc.Quality,
                                                  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil = {1.0f, 0};

    auto depthTexture = m_device->CreateTexture(depthDesc, &depthClearValue);
    depthTexture->SetName(L"Depth Render Target");

    // Attach the textures to the render target.
    m_renderTarget->AttachTexture(AttachmentPoint::DepthStencil, depthTexture);
    m_renderTarget->AttachTexture(AttachmentPoint::Color0, colorTexture);
}

void OcclusionCulling::InitViews()
{
    {
        auto& resource = m_instanceData.GetResource();
        CreateStructuredBuffer(resource, m_numObjects, sizeof(InstanceData), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"instance data resource");
        m_instanceData.SetSRV(m_srvHeap.CreateSRV(resource, m_numObjects, sizeof(InstanceData)));  // 0
        m_instanceData.SetUAV(m_uavHeap.CreateUAV(resource, m_numObjects, sizeof(InstanceData)));
    }

    {
        auto& resource = m_visibility.GetResource();
        CreateStructuredBuffer(resource, m_numObjects, sizeof(unsigned int), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"visibility resource");
        m_visibility.SetSRV(m_srvHeap.CreateSRV(resource, m_numObjects, sizeof(unsigned int)));  // 1
        m_visibility.SetUAV(m_uavHeap.CreateUAV(resource, m_numObjects, sizeof(unsigned int)));
    }

    {
        auto& resource = m_scanResult.GetResource();
        CreateStructuredBuffer(resource, m_numObjects, sizeof(unsigned int), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"scan result resource");
        m_scanResult.SetSRV(m_srvHeap.CreateSRV(resource, m_numObjects, sizeof(unsigned int)));  // 2
        m_scanResult.SetUAV(m_uavHeap.CreateUAV(resource, m_numObjects, sizeof(unsigned int)));
    }

    {
        auto& resource = m_count.GetResource();
        CreateStructuredBuffer(resource, m_numObjects, sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"count resource");
        m_count.SetSRV(m_srvHeap.CreateSRV(resource, 1, sizeof(uint32_t)));  // 3
        m_count.SetUAV(m_uavHeap.CreateUAV(resource, 1, sizeof(uint32_t)));
    }

    {
        auto& resource = m_matrixIndex.GetResource();
        CreateStructuredBuffer(resource, m_numObjects, sizeof(unsigned int), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"WorldMatrixIndex resource");
        m_matrixIndex.SetSRV(m_srvHeap.CreateSRV(resource, m_numObjects, sizeof(unsigned int)));  // 4
        m_matrixIndex.SetUAV(m_uavHeap.CreateUAV(resource, m_numObjects, sizeof(unsigned int)));
    }

    {
        // Max objects to draw is for now 600.000. This because I'm using 1 descriptor table of UAVs, that is limited to 32
        // (using 2 already for other buffers) descriptors. 30 * 20.000 = 600.000
        auto& resource = m_indirectArgs.GetResource();
        CreateStructuredBuffer(resource, 100000, sizeof(IndirectCommand), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        resource->SetName(L"indirect args buffer");
        m_indirectArgs.SetSRV(m_srvHeap.CreateSRV(resource, 20000, sizeof(IndirectCommand)));  // 5
        m_indirectArgs.SetUAV(m_uavHeap.CreateUAV(resource, 20000, sizeof(IndirectCommand)));
    }

    // amount is equal to log256(n) == log(n)/log(256)
    m_numWorkGroupBuffers = static_cast<int>(std::ceil(std::log(m_numObjects) / std::log(256)));
    int amountofSums = m_numObjects / 256;
    for (int i = 0; i < m_numWorkGroupBuffers; i++)
    {
        ComPtr<ID3D12Resource> groupSumBuffer;  // 6 + i
        CreateStructuredBuffer(groupSumBuffer, amountofSums, sizeof(unsigned int), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_groupSums.push_back(groupSumBuffer);

        m_groupSums[i].SetUAV(m_uavHeap.CreateUAV(groupSumBuffer, amountofSums, sizeof(unsigned int)));

        amountofSums = static_cast<int>(std::ceil(static_cast<double>(amountofSums) / 256.f));
    }
}

void OcclusionCulling::PopulateResources()
{
    m_constantData.numObjects = m_numObjects;

    auto& commandQueue = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
    auto commandList = commandQueue.GetCommandList();

    ComPtr<ID3D12Resource> uploadBuffer1;
    auto instanceResource = m_instanceData.GetResource();
    PopulateBuffer(commandList,
                   instanceResource,
                   uploadBuffer1,
                   m_instanceDataBuffer.get()->data(),
                   m_numObjects,
                   sizeof(InstanceData));

    std::vector<unsigned int> visibilityData(m_numObjects, 1);
    ComPtr<ID3D12Resource> uploadBuffer2;
    auto& visibilityResource = m_visibility.GetResource();
    PopulateBuffer(commandList, visibilityResource, uploadBuffer2, visibilityData.data(), m_numObjects, sizeof(unsigned int));

    auto fence = commandQueue.ExecuteCommandList(commandList);
    commandQueue.WaitForFenceValue(fence);
}

void OcclusionCulling::CreateStructuredBuffer(ComPtr<ID3D12Resource>& resource,
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
    m_device->GetD3D12Device()->CreateCommittedResource(&heapProperties,  // Default heap for GPU access
                                                        D3D12_HEAP_FLAG_NONE,
                                                        &instanceBufferDesc,
                                                        D3D12_RESOURCE_STATE_COMMON,  // Initial state
                                                        nullptr,
                                                        IID_PPV_ARGS(&resource));

    ResourceStateTracker::AddGlobalResourceState(resource.Get(), D3D12_RESOURCE_STATE_COMMON);
}

void OcclusionCulling::InitDepthPSO()
{
    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/depth_vs.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/depth_ps.cso", &pixelShaderBlob));

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    // A single 32-bit constant root parameter that is used by the vertex shader.
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix
    rootParameters[1].InitAsShaderResourceView(0, 0);                                               // Instance Data

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    m_depthPass.rs = m_device->CreateRootSignature(rootSignatureDescription.Desc_1_1);
    m_depthPass.rs->GetD3D12RootSignature()->SetName(L"Depth RS");

    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    // Create a color buffer with sRGB for gamma correction.
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;  // Solid fill mode (no wireframe)
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;   // Enable backface culling
    rasterizerDesc.FrontCounterClockwise = FALSE;     // Clockwise winding is front-facing
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;  // Enable depth clipping
    rasterizerDesc.MultisampleEnable = TRUE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    pipelineStateStream.pRootSignature = m_depthPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.DepthStencil = {depthStencilDesc};
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.Rasterizer = {CD3DX12_RASTERIZER_DESC(rasterizerDesc)};
    pipelineStateStream.SampleDesc = sampleDesc;

    m_depthPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_depthPass.pso->GetD3D12PipelineState()->SetName(L"Depth PSO");
}

void OcclusionCulling::InitDrawPSO()
{
    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/draw_objects_vs.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/draw_objects_ps.cso", &pixelShaderBlob));

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    // A single 32-bit constant root parameter that is used by the vertex shader.
    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix
    rootParameters[1].InitAsShaderResourceView(0, 0);                                               // Instance Data
    rootParameters[2].InitAsShaderResourceView(1, 0);                                               // Cull Results

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    m_drawPass.rs = m_device->CreateRootSignature(rootSignatureDescription.Desc_1_1);
    m_drawPass.rs->GetD3D12RootSignature()->SetName(L"Draw Objects RS");

    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    // Create a color buffer with sRGB for gamma correction.
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;  // Solid fill mode (no wireframe)
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;   // Enable backface culling
    rasterizerDesc.FrontCounterClockwise = FALSE;     // Clockwise winding is front-facing
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;  // Enable depth clipping
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    pipelineStateStream.pRootSignature = m_drawPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.DepthStencil = {depthStencilDesc};
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.Rasterizer = {CD3DX12_RASTERIZER_DESC(rasterizerDesc)};
    pipelineStateStream.SampleDesc = sampleDesc;

    m_drawPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_drawPass.pso->GetD3D12PipelineState()->SetName(L"Draw Objects PSO");
}

void OcclusionCulling::InitVisualizeMipsPSO()
{
    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/visualize_mips_vs.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/visualize_mips_ps.cso", &pixelShaderBlob));

    CD3DX12_DESCRIPTOR_RANGE1 textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &textureRange);
    rootParameters[1].InitAsConstants(1, 0);

    CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0,                                 // ShaderRegister
                                                    D3D12_FILTER_MIN_MAG_MIP_POINT,    // Filter
                                                    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // AddressU
                                                    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // AddressV
                                                    D3D12_TEXTURE_ADDRESS_MODE_CLAMP   // AddressW
    );
    CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = {linearRepeatSampler};

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters),
                               rootParameters,
                               _countof(staticSamplers),
                               staticSamplers,
                               D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    m_visualizeMipsPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_visualizeMipsPass.rs->GetD3D12RootSignature()->SetName(L"Vizualize Mips RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(
        m_device->GetD3D12Device()->CreateRootSignature(0,
                                                        signature->GetBufferPointer(),
                                                        signature->GetBufferSize(),
                                                        IID_PPV_ARGS(&m_visualizeMipsPass.rs->GetD3D12RootSignature())));

    // Create a color buffer with sRGB for gamma correction.
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // Check the best multisample quality level that can be used for the given back buffer format.
    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;  // Solid fill mode (no wireframe)
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;  // Clockwise winding is front-facing
    rasterizerDesc.MultisampleEnable = TRUE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pipelineStateStream.pRootSignature = m_visualizeMipsPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.Rasterizer = {CD3DX12_RASTERIZER_DESC(rasterizerDesc)};
    pipelineStateStream.SampleDesc = sampleDesc;

    m_visualizeMipsPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_visualizeMipsPass.pso->GetD3D12PipelineState()->SetName(L"Vizualize Mips PSO");
}

void OcclusionCulling::InitCullingPSO()
{
    // Load the vertex shader.
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/hzbCulling_cs.cso", &computeShaderBlob));

    // int amountOfObjects = 10000;
    CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[1];
    descriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // SRV at t0 : HZB texture

    CD3DX12_ROOT_PARAMETER1 rootParameters[6];
    rootParameters[0].InitAsConstants(2, 0);                           // constant data
    rootParameters[1].InitAsConstants(sizeof(XMMATRIX) / 4, 1);        // VP matrix
    rootParameters[2].InitAsConstants((sizeof(XMFLOAT4) * 6) / 4, 2);  // Frustum planes
    rootParameters[3].InitAsDescriptorTable(1, descriptorRanges);
    rootParameters[4].InitAsShaderResourceView(1, 0);
    rootParameters[5].InitAsUnorderedAccessView(0, 0);

    CD3DX12_STATIC_SAMPLER_DESC pointSampler = CD3DX12_STATIC_SAMPLER_DESC(0,
                                                                           D3D12_FILTER_MIN_MAG_MIP_POINT,
                                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                                           0.f,
                                                                           1,
                                                                           D3D12_COMPARISON_FUNC_ALWAYS,
                                                                           D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                                                                           0.f,
                                                                           D3D12_FLOAT32_MAX);

    CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = {pointSampler};

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(staticSamplers), staticSamplers);

    m_cullingPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_cullingPass.rs->GetD3D12RootSignature()->SetName(L"HZB Culling RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(m_device->GetD3D12Device()->CreateRootSignature(0,
                                                                  signature->GetBufferPointer(),
                                                                  signature->GetBufferSize(),
                                                                  IID_PPV_ARGS(&m_cullingPass.rs->GetD3D12RootSignature())));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_cullingPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    m_cullingPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_cullingPass.pso->GetD3D12PipelineState()->SetName(L"HZB Culling PSO");
}

void OcclusionCulling::InitFirstPrefixPSO()
{
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/firstPassPrefixSum_cs.cso", &computeShaderBlob));

    CD3DX12_ROOT_PARAMETER1 rootParameters[4];
    rootParameters[0].InitAsUnorderedAccessView(0, 0);  // uav
    rootParameters[1].InitAsUnorderedAccessView(1, 0);  // uav
    rootParameters[2].InitAsUnorderedAccessView(2, 0);  // uav
    rootParameters[3].InitAsConstants(1, 0);            // m_numInstances

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters);

    m_firstPrefixPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_firstPrefixPass.rs->GetD3D12RootSignature()->SetName(L"First Pass RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(
        m_device->GetD3D12Device()->CreateRootSignature(0,
                                                        signature->GetBufferPointer(),
                                                        signature->GetBufferSize(),
                                                        IID_PPV_ARGS(&m_firstPrefixPass.rs->GetD3D12RootSignature())));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_firstPrefixPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    m_firstPrefixPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_firstPrefixPass.pso->GetD3D12PipelineState()->SetName(L"First Pass PSO");
}

void OcclusionCulling::InitSecondPrefixPSO()
{
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/secondPassPrefixSum_cs.cso", &computeShaderBlob));

    CD3DX12_ROOT_PARAMETER1 rootParameters[4];
    rootParameters[0].InitAsUnorderedAccessView(0, 0);  // uav (u0)
    rootParameters[1].InitAsUnorderedAccessView(1, 0);  // uav (u1)
    rootParameters[2].InitAsUnorderedAccessView(2, 0);  // uav (u2)
    rootParameters[3].InitAsConstants(1, 0);            // m_numInstances

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters);

    m_secondPrefixPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_secondPrefixPass.rs->GetD3D12RootSignature()->SetName(L"Second Pass RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(
        m_device->GetD3D12Device()->CreateRootSignature(0,
                                                        signature->GetBufferPointer(),
                                                        signature->GetBufferSize(),
                                                        IID_PPV_ARGS(&m_secondPrefixPass.rs->GetD3D12RootSignature())));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_secondPrefixPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    m_secondPrefixPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_secondPrefixPass.pso->GetD3D12PipelineState()->SetName(L"Second Pass PSO");
}

void OcclusionCulling::InitRecursivePrefixPSO()
{
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/recursivePrefixSum_cs.cso", &computeShaderBlob));

    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsUnorderedAccessView(0, 0);  // uav
    rootParameters[1].InitAsUnorderedAccessView(1, 0);  // uav
    rootParameters[2].InitAsConstants(1, 0);            // m_numInstances

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters);

    m_recursivePrefixPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_recursivePrefixPass.rs->GetD3D12RootSignature()->SetName(L"Recursive Presum RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(
        m_device->GetD3D12Device()->CreateRootSignature(0,
                                                        signature->GetBufferPointer(),
                                                        signature->GetBufferSize(),
                                                        IID_PPV_ARGS(&m_recursivePrefixPass.rs->GetD3D12RootSignature())));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_recursivePrefixPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    m_recursivePrefixPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_recursivePrefixPass.pso->GetD3D12PipelineState()->SetName(L"Recursive Presum PSO");
}

void OcclusionCulling::InitFillIndirectBufferPSO()
{
    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/fill_indirect_draw_buffer_cs.cso", &computeShaderBlob));

    CD3DX12_DESCRIPTOR_RANGE1 uavRanges[1];
    uavRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                      5,
                      1,
                      0,
                      D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
                      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);  // 5 UAVs at t1

    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(_countof(uavRanges), uavRanges);
    rootParameters[1].InitAsConstants(1, 0);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters);

    m_fillIndirectBufferPass.rs = m_device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
    m_fillIndirectBufferPass.rs->GetD3D12RootSignature()->SetName(L"Fill Indirect Buffer RS");

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(
        m_device->GetD3D12Device()->CreateRootSignature(0,
                                                        signature->GetBufferPointer(),
                                                        signature->GetBufferSize(),
                                                        IID_PPV_ARGS(&m_fillIndirectBufferPass.rs->GetD3D12RootSignature())));

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_fillIndirectBufferPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    m_fillIndirectBufferPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_fillIndirectBufferPass.pso->GetD3D12PipelineState()->SetName(L"Fill Indirect Buffer PSO");
}

void OcclusionCulling::InitIndirectDrawPSO()
{
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/indirect_draw_vs.cso", &vertexShaderBlob));

    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/draw_objects_ps.cso", &pixelShaderBlob));

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_DESCRIPTOR_RANGE1 srvRanges[6]{};
    srvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    srvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    srvRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    srvRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    srvRanges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    srvRanges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_DESCRIPTOR_RANGE1 cbvRanges[1]{};
    cbvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(_countof(srvRanges), srvRanges);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    m_indirectDrawPass.rs = m_device->CreateRootSignature(rootSignatureDescription.Desc_1_1);
    m_indirectDrawPass.rs->GetD3D12RootSignature()->SetName(L"Execute Indirect RS");

    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    pipelineStateStream.pRootSignature = m_indirectDrawPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.DepthStencil = {depthStencilDesc};
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.Rasterizer = {CD3DX12_RASTERIZER_DESC(rasterizerDesc)};
    pipelineStateStream.SampleDesc = sampleDesc;

    m_indirectDrawPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_indirectDrawPass.pso->GetD3D12PipelineState()->SetName(L"Execute Indirect PSO");

    ////////////////////////////////

    D3D12_INDIRECT_ARGUMENT_DESC args[1] = {};

    args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC cmdSignatureDesc = {};
    cmdSignatureDesc.ByteStride = sizeof(IndirectCommand);
    cmdSignatureDesc.NumArgumentDescs = _countof(args);
    cmdSignatureDesc.pArgumentDescs = args;
    cmdSignatureDesc.NodeMask = 0;

    m_device->GetD3D12Device()->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&m_commandSignature));
}

void OcclusionCulling::InitIndirectDepthPSO()
{
    // Load the vertex shader.
    ComPtr<ID3DBlob> vertexShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/indirect_depth_vs.cso", &vertexShaderBlob));

    // Load the pixel shader.
    ComPtr<ID3DBlob> pixelShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/depth_ps.cso", &pixelShaderBlob));

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    // A single 32-bit constant root parameter that is used by the vertex shader.
    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);  // VP matrix
    rootParameters[1].InitAsShaderResourceView(0, 0);                                               // Instance Data
    rootParameters[2].InitAsShaderResourceView(1, 0);                                               // matrix index buffer

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    m_indirectDepthPass.rs = m_device->CreateRootSignature(rootSignatureDescription.Desc_1_1);
    m_indirectDepthPass.rs->GetD3D12RootSignature()->SetName(L"Indirect Depth RS");

    // Create the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    // Create a color buffer with sRGB for gamma correction.
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = backBufferFormat;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;  // Solid fill mode (no wireframe)
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;   // Enable backface culling
    rasterizerDesc.FrontCounterClockwise = FALSE;     // Clockwise winding is front-facing
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;  // Enable depth clipping
    rasterizerDesc.MultisampleEnable = TRUE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;

    CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    pipelineStateStream.pRootSignature = m_indirectDepthPass.rs->GetD3D12RootSignature().Get();
    pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineStateStream.DepthStencil = {depthStencilDesc};
    pipelineStateStream.RTVFormats = rtvFormats;
    pipelineStateStream.Rasterizer = {CD3DX12_RASTERIZER_DESC(rasterizerDesc)};
    pipelineStateStream.SampleDesc = sampleDesc;

    m_indirectDepthPass.pso = m_device->CreatePipelineStateObject(pipelineStateStream);
    m_indirectDepthPass.pso->GetD3D12PipelineState()->SetName(L"Indirect Depth PSO");
}

void OcclusionCulling::FirstFrameDepthPass(std::shared_ptr<CommandList> commandList, XMMATRIX& vpMatrix)
{
    assert(commandList && "commandlist can't be null.");
    {
        // Clear the render targets.
        {
            commandList->ClearDepthStencilTexture(m_renderTarget->GetTexture(AttachmentPoint::DepthStencil),
                                                  D3D12_CLEAR_FLAG_DEPTH);
        }

        commandList->SetPipelineState(m_depthPass.pso);
        commandList->SetGraphicsRootSignature(m_depthPass.rs);

        {
            // Bind the descriptor heap
            ID3D12DescriptorHeap* ppHeaps[] = {m_srvHeap.GetD3D12Heap().Get()};
            commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            auto adress = m_instanceData.GetResource()->GetGPUVirtualAddress();
            commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(1, adress);
        }

        commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetDynamicVertexBuffer(0, m_numVertices, sizeof(VertexPosColor), m_vertexBuffer.get());
        commandList->SetDynamicIndexBuffer(m_numIndices, DXGI_FORMAT_R16_UINT, m_indexBuffer.get());

        commandList->SetViewport(m_viewport);
        commandList->SetScissorRect(m_scissorRect);

        commandList->SetRenderTarget(*m_renderTarget);

        commandList->SetGraphics32BitConstants(0, sizeof(XMMATRIX) / 4, &vpMatrix);

        // Draw all objects in shader, but use visibility buffer to cull objects. (look into dispatch?)
        commandList->DrawIndexed(m_numIndices, m_numInstances, 0, 0, 0);
    }
}

void OcclusionCulling::FirstFrameDrawPass(XMMATRIX* cameraVP)
{
    auto& commandQueueDirect = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueueDirect.GetCommandList();

    // Clear the render targets.
    {
        FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};

        commandList->ClearTexture(m_renderTarget->GetTexture(AttachmentPoint::Color0), clearColor);
        commandList->ClearDepthStencilTexture(m_renderTarget->GetTexture(AttachmentPoint::DepthStencil),
                                              D3D12_CLEAR_FLAG_DEPTH);
    }

    commandList->SetPipelineState(m_drawPass.pso);
    commandList->SetGraphicsRootSignature(m_drawPass.rs);

    commandList->SetViewport(m_viewport);
    commandList->SetScissorRect(m_scissorRect);
    commandList->SetRenderTarget(*m_renderTarget);

    commandList->SetGraphics32BitConstants(0, sizeof(XMMATRIX) / 4, cameraVP);

    commandList->TransitionBarrier(m_visibility.GetResource(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    {
        // Bind the descriptor heap
        ID3D12DescriptorHeap* ppHeaps[] = {m_srvHeap.GetD3D12Heap().Get()};
        commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        auto adress = m_instanceData.GetResource()->GetGPUVirtualAddress();
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(1, adress);
    }

    {
        // Bind the descriptor heap
        ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
        commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(
            2,
            m_visibility.GetResource()->GetGPUVirtualAddress());
    }

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetDynamicVertexBuffer(0, m_numVertices, sizeof(VertexPosColor), m_vertexBuffer.get());
    commandList->SetDynamicIndexBuffer(m_numIndices, DXGI_FORMAT_R16_UINT, m_indexBuffer.get());

    // Draw all objects in shader, but use visibility buffer to cull objects. (look into dispatch?)
    commandList->DrawIndexed(m_numIndices, m_numInstances, 0, 0, 0);

    commandQueueDirect.ExecuteCommandList(commandList);

    auto fenceValue = commandQueueDirect.Signal();
    commandQueueDirect.WaitForFenceValue(fenceValue);
}

void OcclusionCulling::HzbCulling(XMMATRIX& mainCameraVP, XMMATRIX* debugCameraVP)
{
    if (!debugCameraVP) debugCameraVP = &mainCameraVP;

    auto& commandQueueDirect = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueueDirect.GetCommandList();

    if (m_isFirstFrame || !m_doHzbCulling)
    {
        FirstFrameDepthPass(commandList, mainCameraVP);
        m_isFirstFrame = false;
    }
    else
    {
        IndirectDepthPass(commandList, mainCameraVP);
    }

    auto depthTexture = m_renderTarget->GetTexture(AttachmentPoint::DepthStencil);

    UINT16 numMips = static_cast<UINT16>(std::log2(std::max(m_width, m_height))) + 1;
    m_constantData.maxHzbMip = numMips;

    auto srvUavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS,
                                                   m_width,
                                                   m_height,
                                                   1,
                                                   numMips,
                                                   1,
                                                   0,
                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto depthTextureSRV = m_device->CreateTexture(srvUavDesc);
    depthTextureSRV->SetName(L"Depth Texture SRV");

    commandList->CopyResource(depthTextureSRV, depthTexture);

    commandList->TransitionBarrier(depthTextureSRV, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    commandQueueDirect.ExecuteCommandList(commandList);

    // Fence to make sure the depth buffer is correctly drawn before generating mips
    auto fenceValue = commandQueueDirect.Signal();
    commandQueueDirect.WaitForFenceValue(fenceValue);

    if (m_doHzbCulling)
    {
        GenerateMipsPass(depthTextureSRV);

        CullingPass(depthTextureSRV, mainCameraVP, numMips);

        PrefixSumPass(m_numInstances);

        FillIndirectPass();

        IndirectDrawPass(debugCameraVP);
    }
    else
    {
        FirstFrameDrawPass(debugCameraVP);
    }
}

void OcclusionCulling::VisualizeMipMaps(XMMATRIX* cameraVP)
{
    auto& commandQueueDirect = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueueDirect.GetCommandList();

    // Clear the render targets.
    {
        FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};

        commandList->ClearTexture(m_renderTarget->GetTexture(AttachmentPoint::Color0), clearColor);
        commandList->ClearDepthStencilTexture(m_renderTarget->GetTexture(AttachmentPoint::DepthStencil),
                                              D3D12_CLEAR_FLAG_DEPTH);
    }

    commandList->SetPipelineState(m_depthPass.pso);
    commandList->SetGraphicsRootSignature(m_depthPass.rs);

    {
        // Bind the descriptor heap
        ID3D12DescriptorHeap* ppHeaps[] = {m_srvHeap.GetD3D12Heap().Get()};
        commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        auto adress = m_instanceData.GetResource()->GetGPUVirtualAddress();
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(1, adress);
    }

    {
        ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
        commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(
            2,
            m_visibility.GetResource()->GetGPUVirtualAddress());
    }

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetDynamicVertexBuffer(0, m_numVertices, sizeof(VertexPosColor), m_vertexBuffer.get());
    commandList->SetDynamicIndexBuffer(m_numIndices, DXGI_FORMAT_R16_UINT, m_indexBuffer.get());

    commandList->SetViewport(m_viewport);
    commandList->SetScissorRect(m_scissorRect);

    commandList->SetRenderTarget(*m_renderTarget);

    ////////////////////////////////////////

    commandList->SetGraphics32BitConstants(0, sizeof(XMMATRIX) / 4, cameraVP);

    // Draw all objects in shader, but use visibility buffer to cull objects. (look into dispatch?)
    commandList->DrawIndexed(m_numIndices, m_numInstances, 0, 0, 0);

    auto depthTexture = m_renderTarget->GetTexture(AttachmentPoint::DepthStencil);

    UINT16 numMips = static_cast<UINT16>(std::log2(std::max(m_width, m_height))) + 1;

    auto srvUavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS,
                                                   m_width,
                                                   m_height,
                                                   1,
                                                   numMips,
                                                   1,
                                                   0,
                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto depthTextureSRV = m_device->CreateTexture(srvUavDesc);
    depthTextureSRV->SetName(L"Depth Texture SRV");

    commandList->CopyResource(depthTextureSRV, depthTexture);

    commandList->TransitionBarrier(depthTextureSRV, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    commandQueueDirect.ExecuteCommandList(commandList);

    // Fence to make sure the depth buffer is correctly drawn before generating mips
    auto fenceValue = commandQueueDirect.Signal();
    commandQueueDirect.WaitForFenceValue(fenceValue);

    auto& commandQueueCompute = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    auto commandListCompute = commandQueueCompute.GetCommandList();

    commandListCompute->GenerateMips(depthTextureSRV);

    commandQueueCompute.ExecuteCommandList(commandListCompute);

    fenceValue = commandQueueCompute.Signal();
    commandQueueCompute.WaitForFenceValue(fenceValue);

    ///////////////////////

    commandList = commandQueueDirect.GetCommandList();

    commandList->SetPipelineState(m_visualizeMipsPass.pso);
    commandList->SetGraphicsRootSignature(m_visualizeMipsPass.rs);
    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetViewport(m_viewport);
    commandList->SetScissorRect(m_scissorRect);

    commandList->SetRenderTarget(*m_renderTarget);

    commandList->TransitionBarrier(depthTextureSRV, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    commandList->SetShaderResourceView(0, 0, depthTextureSRV, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, 0, numMips);

    commandList->SetGraphics32BitConstants(1, sizeof(unsigned int) / 4, &m_mipToDisplay);

    std::vector<Vertex> v = {fullscreenTriangle[0], fullscreenTriangle[1], fullscreenTriangle[2]};
    commandList->SetDynamicVertexBuffer(0, v);

    commandList->Draw(3);

    commandQueueDirect.ExecuteCommandList(commandList);

    // Fence to make sure the depth buffer is correctly drawn before generating mips
    fenceValue = commandQueueDirect.Signal();
    commandQueueDirect.WaitForFenceValue(fenceValue);
}

void OcclusionCulling::GenerateMipsPass(std::shared_ptr<DX12Texture>& texture)
{
    auto& commandQueueCompute = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    auto commandListCompute = commandQueueCompute.GetCommandList();

    commandListCompute->GenerateMips(texture);

    commandQueueCompute.ExecuteCommandList(commandListCompute);
}

void OcclusionCulling::CullingPass(std::shared_ptr<DX12Texture>& texture, XMMATRIX& vpMatrix, UINT16 numMips)
{
    auto& commandQueueCompute = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    auto commandListCompute = commandQueueCompute.GetCommandList();

    commandListCompute->TransitionBarrier(texture->GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST);

    commandListCompute->SetPipelineState(m_cullingPass.pso);
    commandListCompute->SetComputeRootSignature(m_cullingPass.rs);

    commandListCompute->SetCompute32BitConstants(0, 2, &m_constantData);

    // Always use the main camera for culling (Mode::MODE_DEFAULT)
    commandListCompute->SetCompute32BitConstants(1, sizeof(XMMATRIX) / 4, &vpMatrix);

    commandListCompute->SetCompute32BitConstants(2, (sizeof(XMFLOAT4) * 6) / 4, &m_FrustumPlanes->planes);

    // commandListCompute->TransitionBarrier(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandListCompute->SetShaderResourceView(3, 0, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, numMips);

    {
        ID3D12DescriptorHeap* descriptorHeapsSRV[] = {m_aabbHeap->GetD3D12Heap().Get()};
        commandListCompute->GetD3D12CommandList()->SetDescriptorHeaps(_countof(descriptorHeapsSRV), descriptorHeapsSRV);
        commandListCompute->GetD3D12CommandList()->SetComputeRootShaderResourceView(
            4,
            m_aabbBuffer->GetResource()->GetGPUVirtualAddress());

        ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
        commandListCompute->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
        commandListCompute->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
            5,
            m_visibility.GetResource()->GetGPUVirtualAddress());
    }

    int threadsPerGroup = 64;                                                  // Assuming 16x16 threads per group
    int numGroups = (m_numInstances + threadsPerGroup - 1) / threadsPerGroup;  // Ceiling division

    commandListCompute->Dispatch(numGroups);

    commandListCompute->UAVBarrier(m_visibility.GetResource());

    commandQueueCompute.ExecuteCommandList(commandListCompute);
}

void OcclusionCulling::PrefixSumPass(UINT numElements)
{
    assert(numElements > 0 && "numElements must be > 0.");

    auto& computeQueue = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);

    const int threadsPerGroup = 256;
    int numIndices = numElements;
    int numGroups = (numElements + threadsPerGroup - 1) / threadsPerGroup;

    std::vector<int> groupBufferSizes;

    for (int i = 0; i < m_numWorkGroupBuffers; i++)
    {
        assert(i < m_numWorkGroupBuffers);

        groupBufferSizes.push_back(numGroups);
        auto commandList = computeQueue.GetCommandList();

        if (i == 0)
        {
            commandList->SetPipelineState(m_firstPrefixPass.pso);
            commandList->SetComputeRootSignature(m_firstPrefixPass.rs);
            commandList->SetCompute32BitConstants(3, 1, &numIndices);
            {
                ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    0,
                    m_visibility.GetResource()->GetGPUVirtualAddress());

                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    1,
                    m_scanResult.GetResource()->GetGPUVirtualAddress());

                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    2,
                    m_groupSums[i].GetResource()->GetGPUVirtualAddress());
            }

            commandList->Dispatch(numGroups);
            commandList->UAVBarrier(m_scanResult.GetResource());
            commandList->UAVBarrier(m_groupSums[i].GetResource());
        }
        else
        {
            commandList->SetPipelineState(m_recursivePrefixPass.pso);
            commandList->SetComputeRootSignature(m_recursivePrefixPass.rs);
            commandList->SetCompute32BitConstants(2, 1, &numIndices);
            {
                ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    0,
                    m_groupSums[i - 1].GetResource()->GetGPUVirtualAddress());
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    1,
                    m_groupSums[i].GetResource()->GetGPUVirtualAddress());
            }

            commandList->Dispatch(numGroups);
            commandList->UAVBarrier(m_groupSums[i - 1].GetResource());
            commandList->UAVBarrier(m_groupSums[i].GetResource());
        }

        computeQueue.ExecuteCommandList(commandList);

        numIndices = numGroups;
        numGroups = static_cast<int>(std::ceil(static_cast<double>(numGroups) / 256.f));
    }

    for (int i = m_numWorkGroupBuffers - 1; i >= 0; i--)
    {
        auto commandList = computeQueue.GetCommandList();
        commandList->SetPipelineState(m_secondPrefixPass.pso);
        commandList->SetComputeRootSignature(m_secondPrefixPass.rs);

        if (i > 0)
        {
            commandList->SetCompute32BitConstants(3, 1, &groupBufferSizes[i - 1]);
            {
                ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    0,
                    m_groupSums[i - 1].GetResource()->GetGPUVirtualAddress());
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    1,
                    m_groupSums[i].GetResource()->GetGPUVirtualAddress());
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    2,
                    m_count.GetResource()->GetGPUVirtualAddress());
            }
            commandList->Dispatch(groupBufferSizes[i]);
            commandList->UAVBarrier(m_groupSums[i - 1].GetResource());
            commandList->UAVBarrier(m_groupSums[i].GetResource());
        }
        else
        {
            commandList->SetCompute32BitConstants(3, 1, &numElements);
            {
                ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    0,
                    m_scanResult.GetResource()->GetGPUVirtualAddress());
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    1,
                    m_groupSums[i].GetResource()->GetGPUVirtualAddress());
                commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);
                commandList->GetD3D12CommandList()->SetComputeRootUnorderedAccessView(
                    2,
                    m_count.GetResource()->GetGPUVirtualAddress());
            }
            commandList->Dispatch(groupBufferSizes[i]);
            commandList->UAVBarrier(m_scanResult.GetResource());
            commandList->UAVBarrier(m_groupSums[i].GetResource());
        }
        computeQueue.ExecuteCommandList(commandList);
    }
}

void OcclusionCulling::FillIndirectPass()
{
    auto& commandQueueCompute = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    auto commandListCompute = commandQueueCompute.GetCommandList();

    commandListCompute->SetPipelineState(m_fillIndirectBufferPass.pso);
    commandListCompute->SetComputeRootSignature(m_fillIndirectBufferPass.rs);
    commandListCompute->SetCompute32BitConstants(1, 1, &m_numInstances);

    {
        ID3D12DescriptorHeap* uavHeap[] = {m_uavHeap.GetD3D12Heap().Get()};
        commandListCompute->GetD3D12CommandList()->SetDescriptorHeaps(_countof(uavHeap), uavHeap);

        auto handle = m_uavHeap.GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart();
        auto incrementSize =
            m_device->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        handle.ptr += incrementSize;
        commandListCompute->GetD3D12CommandList()->SetComputeRootDescriptorTable(0, handle);
    }

    commandListCompute->Dispatch(static_cast<int>(m_numInstances / 256) + 1);
    commandListCompute->UAVBarrier(m_indirectArgs.GetResource());
    auto fenceValue = commandQueueCompute.ExecuteCommandList(commandListCompute);
    commandQueueCompute.WaitForFenceValue(fenceValue);
}

void OcclusionCulling::IndirectDrawPass(XMMATRIX* cameraVP)
{
    auto& commandQueueDirect = m_device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueueDirect.GetCommandList();

    // Clear the render targets.
    {
        FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};

        commandList->ClearTexture(m_renderTarget->GetTexture(AttachmentPoint::Color0), clearColor);
        commandList->ClearDepthStencilTexture(m_renderTarget->GetTexture(AttachmentPoint::DepthStencil),
                                              D3D12_CLEAR_FLAG_DEPTH);
    }

    commandList->SetPipelineState(m_indirectDrawPass.pso);
    commandList->SetGraphicsRootSignature(m_indirectDrawPass.rs);

    commandList->SetViewport(m_viewport);
    commandList->SetScissorRect(m_scissorRect);
    commandList->SetRenderTarget(*m_renderTarget);

    commandList->SetGraphics32BitConstants(0, sizeof(XMMATRIX) / 4, cameraVP);

    commandList->TransitionBarrier(m_indirectArgs.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    ID3D12DescriptorHeap* srvHeaps[] = {m_srvHeap.GetD3D12Heap().Get()};
    commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(srvHeaps), srvHeaps);
    commandList->GetD3D12CommandList()->SetGraphicsRootDescriptorTable(
        1,
        m_srvHeap.GetD3D12Heap()->GetGPUDescriptorHandleForHeapStart());

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetDynamicVertexBuffer(0, m_numVertices, sizeof(VertexPosColor), m_vertexBuffer.get());
    commandList->SetDynamicIndexBuffer(m_numIndices, DXGI_FORMAT_R16_UINT, m_indexBuffer.get());

    commandList->GetD3D12CommandList()
        ->ExecuteIndirect(m_commandSignature.Get(), 1, m_indirectArgs.GetResource().Get(), 0, m_count.GetResource().Get(), 0);

    commandQueueDirect.ExecuteCommandList(commandList);

    auto fenceValue = commandQueueDirect.Signal();
    commandQueueDirect.WaitForFenceValue(fenceValue);
}

void OcclusionCulling::IndirectDepthPass(std::shared_ptr<CommandList> commandList, XMMATRIX& vpMatrix)
{
    assert(commandList && "commandlist can't be null.");

    // Clear the render targets.
    {
        commandList->ClearDepthStencilTexture(m_renderTarget->GetTexture(AttachmentPoint::DepthStencil),
                                              D3D12_CLEAR_FLAG_DEPTH);
    }

    // commandList->SetPipelineState(m_PipelineState);
    commandList->SetPipelineState(m_indirectDepthPass.pso);
    commandList->SetGraphicsRootSignature(m_indirectDepthPass.rs);

    {
        // Bind the descriptor heap
        ID3D12DescriptorHeap* ppHeaps[] = {m_srvHeap.GetD3D12Heap().Get()};
        commandList->GetD3D12CommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        auto adress = m_instanceData.GetResource()->GetGPUVirtualAddress();
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(1, adress);

        adress = m_matrixIndex.GetResource()->GetGPUVirtualAddress();
        commandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(2, adress);
    }

    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetDynamicVertexBuffer(0, m_numVertices, sizeof(VertexPosColor), m_vertexBuffer.get());
    commandList->SetDynamicIndexBuffer(m_numIndices, DXGI_FORMAT_R16_UINT, m_indexBuffer.get());

    commandList->SetViewport(m_viewport);
    commandList->SetScissorRect(m_scissorRect);

    commandList->SetRenderTarget(*m_renderTarget);

    commandList->SetGraphics32BitConstants(0, sizeof(XMMATRIX) / 4, &vpMatrix);

    commandList->GetD3D12CommandList()
        ->ExecuteIndirect(m_commandSignature.Get(), 1, m_indirectArgs.GetResource().Get(), 0, m_count.GetResource().Get(), 0);
}

void OcclusionCulling::IncrementMipToDisplay()
{
    UINT16 numMips = static_cast<UINT16>(std::log2(std::max(m_width, m_height))) + 1;
    if (m_mipToDisplay < numMips)
    {
        m_mipToDisplay++;
    }
}

void OcclusionCulling::DecrementMipToDisplay()
{
    if (m_mipToDisplay > 0)
    {
        m_mipToDisplay--;
    }
}

void OcclusionCulling::PopulateBuffer(std::shared_ptr<CommandList>& commandList,
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
        m_device->GetD3D12Device()->CreateCommittedResource(&uploadHeapProps,
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
