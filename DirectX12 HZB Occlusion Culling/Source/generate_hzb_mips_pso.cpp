#include "pch_dx12.hpp"
#include "generate_hzb_mips_pso.hpp"

GenerateHzbMipsPSO::GenerateHzbMipsPSO(Device& device)
{
    auto d3d12Device = device.GetD3D12Device();

    ComPtr<ID3DBlob> computeShaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(L"../bee/compiledShaders/generate_hzb_mips_cs.cso", &computeShaderBlob));

    CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                     1,
                                     0,
                                     0,
                                     D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                     1,
                                     0,
                                     0,
                                     D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[GenerateHzbMips::NumRootParameters];
    rootParameters[GenerateHzbMips::GenerateMipsCB].InitAsConstants(sizeof(GenerateHzbMipsCB) / 4, 0);
    rootParameters[GenerateHzbMips::SrcMip].InitAsDescriptorTable(1, &srcMip);
    rootParameters[GenerateHzbMips::OutMip].InitAsDescriptorTable(1, &outMip);

    CD3DX12_STATIC_SAMPLER_DESC pointSampler(0,
                                             D3D12_FILTER_MIN_MAG_MIP_POINT,
                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(GenerateHzbMips::NumRootParameters,
                                                            rootParameters,
                                                            1,
                                                            &pointSampler);

    m_RootSignature =
        std::make_shared<RootSignature>(device, rootSignatureDesc.Desc_1_1);  // This is the line that causes the error?

    // Create the PSO for GenerateHzbMips shader.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(PipelineStateStream), &pipelineStateStream};

    m_PipelineState = std::make_shared<PipelineStateObject>(device, pipelineStateStreamDesc);

    // Create some default texture UAV's to pad any unused UAV's during mip map generation.
    m_DefaultUAV = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;

    d3d12Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_DefaultUAV.GetDescriptorHandle());
}