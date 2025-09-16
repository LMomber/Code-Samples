#pragma once

#include "d3d12.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include <memory>

#include <DirectXMath.h>
using namespace DirectX;

class GpuResource
{
public:
    GpuResource() = default;
    GpuResource(ComPtr<ID3D12Resource> resource) : m_resource(resource) {}

    void SetResource(ComPtr<ID3D12Resource> resource) { m_resource = resource; }
    void SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE srv) { m_srv = std::make_shared<D3D12_CPU_DESCRIPTOR_HANDLE>(srv); }
    void SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE uav) { m_uav = std::make_shared<D3D12_CPU_DESCRIPTOR_HANDLE>(uav); }
    void SetCBV(D3D12_CPU_DESCRIPTOR_HANDLE cbv) { m_cbv = std::make_shared<D3D12_CPU_DESCRIPTOR_HANDLE>(cbv); }
    void SetDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv) { m_dsv = std::make_shared<D3D12_CPU_DESCRIPTOR_HANDLE>(dsv); }
    void SetRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv) { m_rtv = std::make_shared<D3D12_CPU_DESCRIPTOR_HANDLE>(rtv); }

    ComPtr<ID3D12Resource>& GetResource()
    {
        //assert(m_resource != nullptr);
        return m_resource;
    }

    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> GetSRV() const
    {
        assert(m_srv != nullptr);
        return m_srv;
    }

    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> GetUAV() const
    {
        assert(m_uav != nullptr);
        return m_uav;
    }

    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> GetCBV() const
    {
        assert(m_cbv != nullptr);
        return m_cbv;
    }

    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> GetDSV() const
    {
        assert(m_dsv != nullptr);
        return m_dsv;
    }

    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> GetRTV() const
    {
        assert(m_rtv != nullptr);
        return m_rtv;
    }

private:
    ComPtr<ID3D12Resource> m_resource = nullptr;
    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> m_srv = nullptr;     
    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> m_uav = nullptr;     
    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> m_cbv = nullptr;     
    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> m_dsv = nullptr;
    std::shared_ptr<D3D12_CPU_DESCRIPTOR_HANDLE> m_rtv = nullptr;
};