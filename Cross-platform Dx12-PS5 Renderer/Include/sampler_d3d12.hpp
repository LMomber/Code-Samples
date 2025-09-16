#pragma once
#include "common_d3d12.hpp"

namespace ff
{
class D3D12SamplerView;
class D3D12Sampler : private NonCopyable
{
public:
    D3D12Sampler(const D3D12_SAMPLER_DESC* desc = nullptr);
    ~D3D12Sampler();

    const D3D12SamplerView* View() const { return m_view; }

private:
    uint32_t m_viewIndex;
    D3D12SamplerView* m_view;
    uint64_t m_hash;
    inline static std::unordered_map<uint64_t, D3D12SamplerView*> m_samplerViews;
};

class SamplerCache
{
public:
    static D3D12Sampler* GetOrCreate(const D3D12_SAMPLER_DESC* desc);

private:
    inline static std::unordered_map<uint64_t, std::unique_ptr<D3D12Sampler>> m_map;
};
}  // namespace ff