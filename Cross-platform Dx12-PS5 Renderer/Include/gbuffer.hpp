#pragma once
#include "platform/firefly/rendering/render_firefly.hpp"
#include "platform/firefly/rendering/texture_firefly.hpp"

#include <memory>

namespace ff
{
struct GBufferPassParams
{
    Texture* FallbackTexture;
    Texture* AlbedoTexture;
    Texture* EmissionTexture;
    Texture* NormalMetallicTexture;
    Texture* shadowMapTexture;
    Texture* DepthStencilTexture;
};

struct DecalAlbedoPassParams
{
    Texture* FallbackTexture;
    Texture* AlbedoTexture;
    Texture* NormalMetallicTexture;
    Texture* DepthStencilTexture;
};
struct DecalAlbedoNormalPassParams
{
    Texture* FallbackTexture;
    Texture* AlbedoTexture;
    Texture* NormalMetallicTexture;
    Texture* DepthStencilTexture;
};
struct DecalAlbedoAlphaNormalPassParams
{
    Texture* FallbackTexture;
    Texture* AlbedoTexture;
    Texture* NormalMetallicTexture;
    Texture* DepthStencilTexture;
};
struct DecalNormalPassParams
{
    Texture* FallbackTexture;
    Texture* AlbedoTexture;
    Texture* NormalMetallicTexture;
    Texture* DepthStencilTexture;
};

struct GBufferSettings
{
    bool RenderSkeletons = true;
};

class GBufferRenderFeature : public RenderFeature
{
public:
    virtual void Add(FireflyRenderer& renderer, GraphBuilder& builder) override;

    void AddGBufferPass(FireflyRenderer& renderer, GraphBuilder& builder);
    void AddDecalPass(FireflyRenderer& renderer, GraphBuilder& builder);

    GBufferSettings Settings;

    Buffer* m_cubeVertexBuffer;
    Buffer* m_cubeIndexBuffer;

    Texture* m_copyTexture = nullptr;
};
}  // namespace ff