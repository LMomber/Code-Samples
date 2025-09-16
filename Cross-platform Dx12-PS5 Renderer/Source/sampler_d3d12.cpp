#include "backends/d3d12/sampler_d3d12.hpp"

#include "backends/d3d12/descriptor_allocator_d3d12.hpp"
#include "backends/d3d12/device_d3d12.hpp"
#include "backends/d3d12/view_d3d12.hpp"
#include "core/engine.hpp"

namespace ff
{
inline D3D12_SAMPLER_DESC CreateDefaultSamplerDesc()
{
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;  // Linear filtering for minification, magnification, and mipmapping.
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // Wrap addressing mode for the U coordinate.
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // Wrap addressing mode for the V coordinate.
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // Wrap addressing mode for the W coordinate.
    samplerDesc.MipLODBias = 0.0f;                            // No bias for mip-level-of-detail.
    samplerDesc.MaxAnisotropy = 1;                            // Anisotropic filtering is off.
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;  // No comparison filtering.
    samplerDesc.BorderColor[0] = 0.0f;                        // Border color (only used if Address mode is BORDER).
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = 0.0f;               // Minimum mip level.
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;  // Maximum mip level.

    return samplerDesc;
}

D3D12Sampler::D3D12Sampler(const D3D12_SAMPLER_DESC* desc)
{
    const D3D12_SAMPLER_DESC* samplerDesc = desc;
    auto sampler = CreateDefaultSamplerDesc();
    if (!desc) { samplerDesc = &sampler; }

    m_view = new D3D12SamplerView(*samplerDesc);
}
D3D12Sampler::~D3D12Sampler() {}

D3D12Sampler* SamplerCache::GetOrCreate(const D3D12_SAMPLER_DESC* desc)
{
    uint64_t hash = 0;

    // If nullptr, the m_map uses the default sampler stored in hash == 0
    if (desc)
    {
        SumHash(hash, desc->Filter);
        SumHash(hash, desc->AddressU);
        SumHash(hash, desc->AddressV);
        SumHash(hash, desc->AddressW);
        SumHash(hash, desc->MipLODBias);
        SumHash(hash, desc->MaxAnisotropy);
        SumHash(hash, desc->ComparisonFunc);
        SumHash(hash, desc->BorderColor[0]);
        SumHash(hash, desc->BorderColor[1]);
        SumHash(hash, desc->BorderColor[2]);
        SumHash(hash, desc->BorderColor[3]);
        SumHash(hash, desc->MinLOD);
        SumHash(hash, desc->MaxLOD);
    }

    if (const auto shader = m_map.find(hash); shader == m_map.end())
    {
        m_map[hash] = std::unique_ptr<D3D12Sampler>(new D3D12Sampler(desc));
        bee::Log::Info("Created A New Sampler");
    }

    return m_map[hash].get();
}
}  // namespace ff
