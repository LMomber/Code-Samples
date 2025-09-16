#include "renderpipeline/gbuffer.hpp"

#include "animation/skeleton.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
#include "disable_depth.hpp"
#include "firefly_commandlist.hpp"
#include "firefly_render_components.hpp"
#include "platform/firefly/core/device_firefly.hpp"
#include "platform/firefly/rendering/render_firefly.hpp"
#include "rendergraph/rendergraph_builder.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/image.hpp"
#include "rendering/mesh.hpp"
#include "rendering/render_components.hpp"
#include "renderpipeline/shadow.hpp"
#include "sampler_firefly.hpp"
#include "shader_firefly.hpp"

#ifdef BEE_GRAPHICS_DIRECTX12
#include "rendering/debug_render.hpp"
#endif
#include <random>

struct ModelCBuffer
{
    glm::mat4 m_model;
    glm::mat4 m_invTransMatrix;
};

struct CameraCBuffer
{
    glm::mat4 m_vp;
};

struct MaterialCBuffer
{
    glm::vec4 m_diffuse;
    glm::vec3 m_emission;

    float m_metallic;
    float m_roughness;

    int m_diffuseMap;
    int m_normalMap;
    int m_metallicMap;
    int m_roughnessMap;
    int m_emissionMap;
};

struct ShadowCBuffer
{
    glm::mat4 m_vp;
    glm::vec3 m_direction;
};

void ff::GBufferRenderFeature::Add(FireflyRenderer& renderer, GraphBuilder& builder)
{
    AddGBufferPass(renderer, builder);
    AddDecalPass(renderer, builder);
    AddSkeletalPass(renderer, builder);
    AddOverlayPass(renderer, builder);

    AddSSAOPass(renderer, builder);
    AddSSAOVerticalBlur(renderer, builder);
    AddSSAOHorizontalBlur(renderer, builder);
}

void ff::GBufferRenderFeature::AddGBufferPass(FireflyRenderer& renderer, GraphBuilder& builder)
{
    GBufferPassParams* params = builder.AllocParameters<GBufferPassParams>();

    {
        // Create a dummy texture
        TextureDesc desc;
        desc.Width = 1;
        desc.Height = 1;
        desc.Format = TextureFormat::R8G8B8A8_UNORM;
        desc.DebugName = "Dummy Emissive Texture";
        desc.ViewFlags = TextureDesc::ShaderResource;
        desc.TypeFlags = TextureDesc::GpuOnly;
        params->FallbackTexture = builder.CreateTransientTexture("EmptyTexture", desc);
    }

    // Albedo texture.
    {
        TextureDesc desc;
        desc.Width = bee::Engine.Device().GetWidth();
        desc.Height = bee::Engine.Device().GetHeight();
        desc.Format = TextureFormat::R8G8B8A8_UNORM;
        desc.DebugName = "Albedo/Roughness Texture";
        desc.ViewFlags = TextureDesc::RenderTargettable | TextureDesc::ShaderResource;
        desc.TypeFlags = TextureDesc::GpuOnly;
        params->AlbedoTexture = builder.CreateTransientTexture("Albedo/Roughness", desc);
    }

    // Normal/Metallic texture.
    {
        TextureDesc desc;
        desc.Width = bee::Engine.Device().GetWidth();
        desc.Height = bee::Engine.Device().GetHeight();
        desc.Format = TextureFormat::R8G8B8A8_UNORM;
        desc.DebugName = "Normal/Metallic Texture";
        desc.ViewFlags = TextureDesc::RenderTargettable | TextureDesc::ShaderResource | TextureDesc::UnorderedAccess;
        desc.TypeFlags = TextureDesc::GpuOnly;
        params->NormalMetallicTexture = builder.CreateTransientTexture("Normal/Metallic", desc);
    }
    // Emission texture.
    {
        TextureDesc desc;
        desc.Width = bee::Engine.Device().GetWidth();
        desc.Height = bee::Engine.Device().GetHeight();
        desc.Format = TextureFormat::R11G11B10_FLOAT;
        desc.DebugName = "Emission Texture";
        desc.ViewFlags = TextureDesc::RenderTargettable | TextureDesc::ShaderResource;
        desc.TypeFlags = TextureDesc::GpuOnly;
        params->EmissionTexture = builder.CreateTransientTexture("Emission", desc);
    }
    // DepthStencil texture.
    {
        TextureDesc desc;
        desc.Width = bee::Engine.Device().GetWidth();
        desc.Height = bee::Engine.Device().GetHeight();
        desc.Format = TextureFormat::D24S8;
        desc.DebugName = "DepthStencil Texture Overlay";
        desc.ViewFlags = TextureDesc::DepthStencilable | TextureDesc::ShaderResource;
        desc.TypeFlags = TextureDesc::GpuOnly;
        params->DepthStencilTexture = builder.CreateTransientTexture("DepthStencil", desc);
    }

    builder.AddPass<GBufferPassParams>("GbufferPass",
                                       [&renderer](GBufferPassParams& params, CommandList& commandList)
    {
        commandList.BeginRender({params.AlbedoTexture, params.NormalMetallicTexture, params.EmissionTexture},
                                {ClearOperation::Clear, ClearOperation::Clear, ClearOperation::Clear},
                                params.DepthStencilTexture,
                                DepthStencilClearOperation::Clear,
                                "RenderPass -> GBufferPass");

        PipelineSettings settings;
        settings.ShaderState.VertexShader = ShaderCache::GetOrCreate("shaders/Gpass_vs", ShaderType::Vertex);
        settings.ShaderState.PixelShader = ShaderCache::GetOrCreate("shaders/Gpass_ps", ShaderType::Pixel);
        settings.ShaderState.InputLayout = {InputParameter("POSITION", DataFormat::Float3, 0),
                                            InputParameter("NORMAL", DataFormat::Float3, 1),
                                            InputParameter("TANGENT", DataFormat::Float3, 2),
                                            InputParameter("TEXCOORD", DataFormat::Float2, 3)};
        settings.DepthStencilState.DepthEnable = true;

        commandList.SetPipelineSettings(settings);

#if defined(BEE_GRAPHICS_DIRECTX12)
        D3D12_SAMPLER_DESC linearSamplerDesc = {};
        linearSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        linearSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.MipLODBias = 0.0f;
        linearSamplerDesc.MaxAnisotropy = 16;
        linearSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        linearSamplerDesc.BorderColor[0] = 0.0f;
        linearSamplerDesc.BorderColor[1] = 0.0f;
        linearSamplerDesc.BorderColor[2] = 0.0f;
        linearSamplerDesc.BorderColor[3] = 0.0f;
        linearSamplerDesc.MinLOD = 0.0f;
        linearSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        commandList.SetSamplerState("linearSampler", ff::SamplerCache::GetOrCreate(&linearSamplerDesc)->View());
#endif

        for (const auto& [e, cameraTransform, camera] : bee::Engine.ECS().Registry.view<bee::Transform, bee::Camera>().each())
        {
            const glm::mat4 cameraWorld = cameraTransform.World();
            const glm::mat4 view = glm::inverse(cameraWorld);

            CameraCBuffer cameraCBuffer;
            cameraCBuffer.m_vp = camera.Projection * view;
            commandList.SetBufferData("camera", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));

            for (const auto& [e2, meshRenderer, transform] :
                 bee::Engine.ECS().Registry.view<bee::MeshRenderer, bee::Transform>(entt::exclude<TopLayer>).each())
            {
                // auto mesh = renderer.Mesh.get();
                auto material = meshRenderer.Material.get();
                auto mesh = meshRenderer.Mesh.get();

                // Bind textures.
                if (!material->BaseColorTexture) { commandList.SetTextureSRV("albedo_texture", *params.FallbackTexture); }
                else { commandList.SetTextureSRV("albedo_texture", material->BaseColorTexture->Image->GetTexture()); }

                if (!material->MetallicRoughnessTexture)
                {
                    commandList.SetTextureSRV("metallic_roughness_texture", *params.FallbackTexture);
                }
                else
                {
                    commandList.SetTextureSRV("metallic_roughness_texture",
                                              material->MetallicRoughnessTexture->Image->GetTexture());
                }

                if (!material->EmissiveTexture) { commandList.SetTextureSRV("emissive_texture", *params.FallbackTexture); }
                else { commandList.SetTextureSRV("emissive_texture", material->EmissiveTexture->Image->GetTexture()); }

                if (!material->NormalTexture) { commandList.SetTextureSRV("normal_texture", *params.FallbackTexture); }
                else { commandList.SetTextureSRV("normal_texture", material->NormalTexture->Image->GetTexture()); }

                glm::vec3 emission = material->EmissiveFactor;

                MaterialCBuffer materialCBuffer;
                materialCBuffer.m_diffuse = material->BaseColorFactor;
                materialCBuffer.m_metallic = material->MetallicFactor;
                materialCBuffer.m_roughness = material->RoughnessFactor;
                materialCBuffer.m_emission = {emission.x, emission.y, emission.z};
                materialCBuffer.m_diffuseMap = material->UseBaseTexture;
                materialCBuffer.m_normalMap = material->UseNormalTexture;
                materialCBuffer.m_metallicMap = material->UseMetallicRoughnessTexture;
                materialCBuffer.m_roughnessMap = material->UseMetallicRoughnessTexture;
                materialCBuffer.m_emissionMap = material->UseEmissiveTexture;

                commandList.SetBufferData("material",
                                          renderer.RenderUploadBuffer.get(),
                                          &materialCBuffer,
                                          sizeof(MaterialCBuffer));

                ModelCBuffer modelCBuffer;
                modelCBuffer.m_model = transform.World();
                modelCBuffer.m_invTransMatrix = glm::transpose(glm::inverse(modelCBuffer.m_model));

                commandList.SetBufferData("modelCBuffer",
                                          renderer.RenderUploadBuffer.get(),
                                          &modelCBuffer,
                                          sizeof(ModelCBuffer));

                auto shadowView = bee::Engine.ECS().Registry.view<ShadowCamera>();
                ShadowCamera* shadowData = nullptr;

                for (auto entity : shadowView) { shadowData = &shadowView.get<ShadowCamera>(entity); }

                ShadowCBuffer shadowBuffer;

                if (shadowData)
                {
                    shadowBuffer.m_vp = shadowData->Projection * shadowData->View;
                    shadowBuffer.m_direction = shadowData->Direction;
                }
                else
                {
                    shadowBuffer.m_vp = glm::mat4{1.0f};
                    shadowBuffer.m_direction = glm::vec3{0.0f};
                }

                commandList.SetVertexBuffers({mesh->m_positionBuffer.get(),
                                              mesh->m_normalBuffer.get(),
                                              mesh->m_tangentBuffer.get(),
                                              mesh->m_texCoordsBuffer.get()});

                commandList.DrawIndexedInstanced(*mesh->m_indexBuffer, 1, 0, 0, 0);
            }
        }

        commandList.EndRender();
    });
    renderer.CompositeTexture = params->AlbedoTexture;
}

void ff::GBufferRenderFeature::AddDecalPass(FireflyRenderer& renderer, GraphBuilder& builder)
{
    static const std::array<glm::vec3, 8> cubeVertices = {{
        {-1.0f, -1.0f, -1.0f},  // 0
        {-1.0f, 1.0f, -1.0f},   // 1
        {1.0f, 1.0f, -1.0f},    // 2
        {1.0f, -1.0f, -1.0f},   // 3
        {-1.0f, -1.0f, 1.0f},   // 4
        {-1.0f, 1.0f, 1.0f},    // 5
        {1.0f, 1.0f, 1.0f},     // 6
        {1.0f, -1.0f, 1.0f}     // 7
    }};

    static const std::array<int, 36> cubeIndices = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0,
                                                    3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7};

    struct CameraCBuffer
    {
        glm::mat4 m_VP;
        glm::mat4 m_V;
        glm::mat4 m_invV;
        glm::mat4 m_invP;
        glm::vec3 m_cameraPos;
    };

    struct DecalCBuffer
    {
        glm::mat4 m_M;
        glm::mat4 m_invM;
        int m_albedoMap;
        int m_normalMap;
        int m_useAlbedoStrength;
        float m_albedoStrength;
    };

    DecalAlbedoPassParams* albedoParams = builder.AllocParameters<DecalAlbedoPassParams>();
    GBufferPassParams* gBufferParams = builder.GetPassData<DecalAlbedoPassParams, GBufferPassParams>();
    albedoParams->FallbackTexture = gBufferParams->FallbackTexture;
    albedoParams->AlbedoTexture = gBufferParams->AlbedoTexture;
    albedoParams->NormalMetallicTexture = gBufferParams->NormalMetallicTexture;
    albedoParams->DepthStencilTexture = gBufferParams->DepthStencilTexture;

    static bool runOnce = true;
    if (runOnce)
    {
        BufferDesc desc;

        m_cubeVertexBuffer = new Buffer(
            desc.AsVertexBuffer(cubeVertices.data(), static_cast<uint32_t>(cubeVertices.size()), DataFormat::Float3));
        m_cubeIndexBuffer =
            new Buffer(desc.AsIndexBuffer(cubeIndices.data(), static_cast<uint32_t>(cubeIndices.size()), DataFormat::Uint));
        runOnce = false;
    }

    {
        TextureDesc texDesc;
        texDesc.Width = gBufferParams->NormalMetallicTexture->Width();
        texDesc.Height = gBufferParams->NormalMetallicTexture->Height();
        texDesc.Format = TextureFormat::R8G8B8A8_UNORM;
        texDesc.DebugName = "copy texture";
        texDesc.ViewFlags = TextureDesc::ShaderResource;
        texDesc.TypeFlags = TextureDesc::GpuOnly;
        texDesc.GenerateMipMaps = true;
        m_copyTexture = builder.CreateTransientTexture("CopyTexture", texDesc);
    }

    DecalAlbedoNormalPassParams* albedoNormalParams = builder.AllocParameters<DecalAlbedoNormalPassParams>();
    builder.GetPassData<DecalAlbedoNormalPassParams, DecalAlbedoPassParams>();
    albedoNormalParams->FallbackTexture = gBufferParams->FallbackTexture;
    albedoNormalParams->AlbedoTexture = gBufferParams->AlbedoTexture;
    albedoNormalParams->NormalMetallicTexture = gBufferParams->NormalMetallicTexture;
    albedoNormalParams->DepthStencilTexture = gBufferParams->DepthStencilTexture;


    DecalNormalPassParams* normalParams = builder.AllocParameters<DecalNormalPassParams>();
    builder.GetPassData<DecalNormalPassParams, DecalAlbedoNormalPassParams>();
    normalParams->FallbackTexture = gBufferParams->FallbackTexture;
    normalParams->AlbedoTexture = gBufferParams->AlbedoTexture;
    normalParams->NormalMetallicTexture = gBufferParams->NormalMetallicTexture;
    normalParams->DepthStencilTexture = gBufferParams->DepthStencilTexture;

    builder.AddPass<DecalAlbedoPassParams>("DecalAlbedoPass",
                                           [this, &renderer](const DecalAlbedoPassParams& params, CommandList& commandList)
    {
        commandList.BeginRender({params.AlbedoTexture, params.NormalMetallicTexture},
                                {ClearOperation::Store, ClearOperation::Store},
                                nullptr,
                                DepthStencilClearOperation::Store,
                                "Decal Albedo Pass");

        commandList.CopyResource(*params.NormalMetallicTexture, *m_copyTexture);

        const auto* const vertexShader = "shaders/decal_vs";
        const auto* const pixelShader = "shaders/decal_ps";

        PipelineSettings settings;
        settings.ShaderState.VertexShader = ShaderCache::GetOrCreate(vertexShader, ShaderType::Vertex);
        settings.ShaderState.PixelShader = ShaderCache::GetOrCreate(pixelShader, ShaderType::Pixel);
        settings.ShaderState.InputLayout = {InputParameter("POSITION", DataFormat::Float3, 0)};

        settings.RasterizerState.CullingMode = CullingMode::Back;
        settings.RasterizerState.BlendDesc.AlphaToCoverageEnable = false;
        settings.RasterizerState.BlendDesc.IndependentBlendEnable = true;

        settings.RasterizerState.BlendDesc.RenderTarget[0].Enabled = true;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlend = BlendMode::SrcAlpha;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlend = BlendMode::InvSrcAlpha;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOpAlpha = BlendEquation::Add;

        // Removes metallic and leaves normals intact
        settings.RasterizerState.BlendDesc.RenderTarget[1].Enabled = true;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlend = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlend = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlendAlpha = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOpAlpha = BlendEquation::Add;

        commandList.SetPipelineSettings(settings);

        auto entt = bee::Engine.ECS().Registry.view<bee::Transform, bee::Camera>().front();
        auto& camera = bee::Engine.ECS().Registry.get<bee::Camera>(entt);
        auto& cameraTransform = bee::Engine.ECS().Registry.get<bee::Transform>(entt);

        const glm::mat4 cameraWorld = cameraTransform.World();
        const glm::mat4 view = glm::inverse(cameraWorld);

        CameraCBuffer cameraCBuffer;
        cameraCBuffer.m_VP = camera.Projection * view;
        cameraCBuffer.m_V = view;
        cameraCBuffer.m_invV = glm::inverse(view);
        cameraCBuffer.m_invP = glm::inverse(camera.Projection);
        cameraCBuffer.m_cameraPos = cameraTransform.GetTranslation();

#ifdef BEE_GRAPHICS_DIRECTX12
        D3D12_SAMPLER_DESC linearSamplerDesc = {};
        linearSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        linearSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.MipLODBias = 0.0f;
        linearSamplerDesc.MaxAnisotropy = 1;
        linearSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        linearSamplerDesc.BorderColor[0] = 0.0f;
        linearSamplerDesc.BorderColor[1] = 0.0f;
        linearSamplerDesc.BorderColor[2] = 0.0f;
        linearSamplerDesc.BorderColor[3] = 0.0f;
        linearSamplerDesc.MinLOD = 0.0f;
        linearSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_SAMPLER_DESC pointSamplerDesc = {};
        pointSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        pointSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.MipLODBias = 0.0f;
        pointSamplerDesc.MaxAnisotropy = 1;
        pointSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        pointSamplerDesc.BorderColor[0] = 0.0f;
        pointSamplerDesc.BorderColor[1] = 0.0f;
        pointSamplerDesc.BorderColor[2] = 0.0f;
        pointSamplerDesc.BorderColor[3] = 0.0f;
        pointSamplerDesc.MinLOD = 0.0f;
        pointSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        commandList.SetSamplerState("linearSampler", ff::SamplerCache::GetOrCreate(&linearSamplerDesc)->View());
        commandList.SetSamplerState("pointSampler", ff::SamplerCache::GetOrCreate(&pointSamplerDesc)->View());
#endif

        for (const auto& [e2, decal, transform] : bee::Engine.ECS().Registry.view<Decal, bee::Transform>().each())
        {
            if (decal.IsAlbedoUsed() && !decal.IsNormalUsed())
            {
                DecalCBuffer decalCBuffer;
                decalCBuffer.m_M = transform.World();
                decalCBuffer.m_invM = glm::inverse(decalCBuffer.m_M);
                decalCBuffer.m_albedoMap = decal.IsAlbedoUsed();
                decalCBuffer.m_normalMap = decal.IsNormalUsed();
                decalCBuffer.m_useAlbedoStrength = decal.IsAlbedoStrengthUsed();
                decalCBuffer.m_albedoStrength = decal.GetAlbedoStrength();

                commandList.SetBufferData("cameraVS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalVS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));
                commandList.SetBufferData("cameraPS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalPS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));

                if (decal.IsAlbedoUsed())
                {
                    commandList.SetTextureSRV("albedoTexture", decal.GetAlbedoTexture()->GetTexture());
                }
                else { commandList.SetTextureSRV("albedoTexture", *params.FallbackTexture); }

                if (decal.IsNormalUsed())
                {
                    commandList.SetTextureSRV("normalTexture", decal.GetNormalTexture()->GetTexture());
                    commandList.SetTextureSRV("normalBuffer", *m_copyTexture);
                }
                else
                {
                    commandList.SetTextureSRV("normalTexture", *params.FallbackTexture);
                    commandList.SetTextureSRV("normalBuffer", *params.FallbackTexture);
                }
                commandList.SetTextureSRV("depthTexture", *params.DepthStencilTexture);

                commandList.SetVertexBuffer(*m_cubeVertexBuffer);

                commandList.DrawIndexedInstanced(*m_cubeIndexBuffer, 1, 0, 0, 0);

//#ifdef BEE_GRAPHICS_DIRECTX12
//                bee::Engine.DebugRenderer().AddCube(bee::DebugCategory::Rendering,
//                                                    transform.GetTranslation(),
//                                                    transform.GetScale().x * 2,
//                                                    glm::vec4(1.f, 0.f, 0.f, 1.f));
//#endif
            }
        }

        commandList.EndRender();
    });

    builder.AddPass<DecalAlbedoNormalPassParams>(
        "DecalAlbedoNormalPass",
        [this, &renderer](const DecalAlbedoNormalPassParams& params, CommandList& commandList)
    {
        commandList.BeginRender({params.AlbedoTexture, params.NormalMetallicTexture},
                                {ClearOperation::Store, ClearOperation::Store},
                                nullptr,
                                DepthStencilClearOperation::Store,
                                "Decal Albedo/Normal Pass");

        const auto* const vertexShader = "shaders/decal_vs";
        const auto* const pixelShader = "shaders/decal_ps";

        PipelineSettings settings;
        settings.ShaderState.VertexShader = ShaderCache::GetOrCreate(vertexShader, ShaderType::Vertex);
        settings.ShaderState.PixelShader = ShaderCache::GetOrCreate(pixelShader, ShaderType::Pixel);
        settings.ShaderState.InputLayout = {InputParameter("POSITION", DataFormat::Float3, 0)};

        settings.RasterizerState.CullingMode = CullingMode::Back;
        settings.RasterizerState.BlendDesc.AlphaToCoverageEnable = false;
        settings.RasterizerState.BlendDesc.IndependentBlendEnable = true;

        settings.RasterizerState.BlendDesc.RenderTarget[0].Enabled = true;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlend = BlendMode::SrcAlpha;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlend = BlendMode::InvSrcAlpha;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlendAlpha = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOpAlpha = BlendEquation::Add;

        // Both normal blending & albedo
        settings.RasterizerState.BlendDesc.RenderTarget[1].Enabled = false;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlend = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlend = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlendAlpha = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOpAlpha = BlendEquation::Add;



        commandList.SetPipelineSettings(settings);

        auto entt = bee::Engine.ECS().Registry.view<bee::Transform, bee::Camera>().front();
        auto& camera = bee::Engine.ECS().Registry.get<bee::Camera>(entt);
        auto& cameraTransform = bee::Engine.ECS().Registry.get<bee::Transform>(entt);

        const glm::mat4 cameraWorld = cameraTransform.World();
        const glm::mat4 view = glm::inverse(cameraWorld);

        CameraCBuffer cameraCBuffer;
        cameraCBuffer.m_VP = camera.Projection * view;
        cameraCBuffer.m_invV = glm::inverse(view);
        cameraCBuffer.m_invP = glm::inverse(camera.Projection);
        cameraCBuffer.m_cameraPos = cameraTransform.GetTranslation();

#ifdef BEE_GRAPHICS_DIRECTX12
        D3D12_SAMPLER_DESC linearSamplerDesc = {};
        linearSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        linearSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.MipLODBias = 0.0f;
        linearSamplerDesc.MaxAnisotropy = 1;
        linearSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        linearSamplerDesc.BorderColor[0] = 0.0f;
        linearSamplerDesc.BorderColor[1] = 0.0f;
        linearSamplerDesc.BorderColor[2] = 0.0f;
        linearSamplerDesc.BorderColor[3] = 0.0f;
        linearSamplerDesc.MinLOD = 0.0f;
        linearSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_SAMPLER_DESC pointSamplerDesc = {};
        pointSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        pointSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.MipLODBias = 0.0f;
        pointSamplerDesc.MaxAnisotropy = 1;
        pointSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        pointSamplerDesc.BorderColor[0] = 0.0f;
        pointSamplerDesc.BorderColor[1] = 0.0f;
        pointSamplerDesc.BorderColor[2] = 0.0f;
        pointSamplerDesc.BorderColor[3] = 0.0f;
        pointSamplerDesc.MinLOD = 0.0f;
        pointSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        commandList.SetSamplerState("linearSampler", ff::SamplerCache::GetOrCreate(&linearSamplerDesc)->View());
        commandList.SetSamplerState("pointSampler", ff::SamplerCache::GetOrCreate(&pointSamplerDesc)->View());
#endif

        for (const auto& [e2, decal, transform] : bee::Engine.ECS().Registry.view<Decal, bee::Transform>().each())
        {
            if (decal.IsAlbedoUsed() && decal.IsNormalUsed())
            {
                DecalCBuffer decalCBuffer;
                decalCBuffer.m_M = transform.World();
                decalCBuffer.m_invM = glm::inverse(decalCBuffer.m_M);
                decalCBuffer.m_albedoMap = decal.IsAlbedoUsed();
                decalCBuffer.m_normalMap = decal.IsNormalUsed();
                decalCBuffer.m_useAlbedoStrength = decal.IsAlbedoStrengthUsed();
                decalCBuffer.m_albedoStrength = decal.GetAlbedoStrength();

                commandList.SetBufferData("cameraVS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalVS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));
                commandList.SetBufferData("cameraPS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalPS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));

                if (decal.IsAlbedoUsed())
                {
                    commandList.SetTextureSRV("albedoTexture", decal.GetAlbedoTexture()->GetTexture());
                }
                else { commandList.SetTextureSRV("albedoTexture", *params.FallbackTexture); }

                if (decal.IsNormalUsed())
                {
                    commandList.SetTextureSRV("normalTexture", decal.GetNormalTexture()->GetTexture());
                    commandList.SetTextureSRV("normalBuffer", *m_copyTexture);
                }
                else
                {
                    commandList.SetTextureSRV("normalTexture", *params.FallbackTexture);
                    commandList.SetTextureSRV("normalBuffer", *params.FallbackTexture);
                }
                commandList.SetTextureSRV("depthTexture", *params.DepthStencilTexture);

                commandList.SetVertexBuffer(*m_cubeVertexBuffer);

                commandList.DrawIndexedInstanced(*m_cubeIndexBuffer, 1, 0, 0, 0);

                // Scale * 2 because the cube is [-1, 1]
                /*bee::Engine.DebugRenderer().AddCube(bee::DebugCategory::Rendering,
                                                    transform.GetTranslation(),
                                                    transform.GetScale().x * 2,
                                                    glm::vec4(1.f, 0.f, 0.f, 1.f));*/
            }
        }

        commandList.EndRender();
    });

    builder.AddPass<DecalNormalPassParams>("DecalNormalPass",
                                           [this, &renderer](const DecalNormalPassParams& params, CommandList& commandList)
    {
        commandList.BeginRender({params.AlbedoTexture, params.NormalMetallicTexture},
                                {ClearOperation::Store, ClearOperation::Store},
                                nullptr,
                                DepthStencilClearOperation::Store,
                                "Decal Normal Pass");

        const auto* const vertexShader = "shaders/decal_vs";
        const auto* const pixelShader = "shaders/decal_ps";

        PipelineSettings settings;
        settings.ShaderState.VertexShader = ShaderCache::GetOrCreate(vertexShader, ShaderType::Vertex);
        settings.ShaderState.PixelShader = ShaderCache::GetOrCreate(pixelShader, ShaderType::Pixel);
        settings.ShaderState.InputLayout = {InputParameter("POSITION", DataFormat::Float3, 0)};

        settings.RasterizerState.CullingMode = CullingMode::Back;
        settings.RasterizerState.BlendDesc.AlphaToCoverageEnable = false;
        settings.RasterizerState.BlendDesc.IndependentBlendEnable = true;

        settings.RasterizerState.BlendDesc.RenderTarget[0].Enabled = true;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlend = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlend = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[0].DstBlendAlpha = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOpAlpha = BlendEquation::Add;

        // Only normal blending, no albedo
        settings.RasterizerState.BlendDesc.RenderTarget[1].Enabled = true;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlend = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlend = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOp = BlendEquation::Add;
        settings.RasterizerState.BlendDesc.RenderTarget[1].SrcBlendAlpha = BlendMode::Zero;
        settings.RasterizerState.BlendDesc.RenderTarget[1].DstBlendAlpha = BlendMode::One;
        settings.RasterizerState.BlendDesc.RenderTarget[1].BlendOpAlpha = BlendEquation::Add;

        commandList.SetPipelineSettings(settings);

        auto entt = bee::Engine.ECS().Registry.view<bee::Transform, bee::Camera>().front();
        auto& camera = bee::Engine.ECS().Registry.get<bee::Camera>(entt);
        auto& cameraTransform = bee::Engine.ECS().Registry.get<bee::Transform>(entt);

        const glm::mat4 cameraWorld = cameraTransform.World();
        const glm::mat4 view = glm::inverse(cameraWorld);

        CameraCBuffer cameraCBuffer;
        cameraCBuffer.m_VP = camera.Projection * view;
        cameraCBuffer.m_V = view;
        cameraCBuffer.m_invV = glm::inverse(view);
        cameraCBuffer.m_invP = glm::inverse(camera.Projection);
        cameraCBuffer.m_cameraPos = cameraTransform.GetTranslation();

#ifdef BEE_GRAPHICS_DIRECTX12
        D3D12_SAMPLER_DESC linearSamplerDesc = {};
        linearSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        linearSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearSamplerDesc.MipLODBias = 0.0f;
        linearSamplerDesc.MaxAnisotropy = 1;
        linearSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        linearSamplerDesc.BorderColor[0] = 0.0f;
        linearSamplerDesc.BorderColor[1] = 0.0f;
        linearSamplerDesc.BorderColor[2] = 0.0f;
        linearSamplerDesc.BorderColor[3] = 0.0f;
        linearSamplerDesc.MinLOD = 0.0f;
        linearSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_SAMPLER_DESC pointSamplerDesc = {};
        pointSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        pointSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        pointSamplerDesc.MipLODBias = 0.0f;
        pointSamplerDesc.MaxAnisotropy = 1;
        pointSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        pointSamplerDesc.BorderColor[0] = 0.0f;
        pointSamplerDesc.BorderColor[1] = 0.0f;
        pointSamplerDesc.BorderColor[2] = 0.0f;
        pointSamplerDesc.BorderColor[3] = 0.0f;
        pointSamplerDesc.MinLOD = 0.0f;
        pointSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        commandList.SetSamplerState("linearSampler", ff::SamplerCache::GetOrCreate(&linearSamplerDesc)->View());
        commandList.SetSamplerState("pointSampler", ff::SamplerCache::GetOrCreate(&pointSamplerDesc)->View());
#endif

        for (const auto& [e2, decal, transform] : bee::Engine.ECS().Registry.view<Decal, bee::Transform>().each())
        {
            if (!decal.IsAlbedoUsed() && decal.IsNormalUsed())
            {
                DecalCBuffer decalCBuffer;
                decalCBuffer.m_M = transform.World();
                decalCBuffer.m_invM = glm::inverse(decalCBuffer.m_M);
                decalCBuffer.m_albedoMap = decal.IsAlbedoUsed();
                decalCBuffer.m_normalMap = decal.IsNormalUsed();
                decalCBuffer.m_useAlbedoStrength = decal.IsAlbedoStrengthUsed();
                decalCBuffer.m_albedoStrength = decal.GetAlbedoStrength();

                commandList.SetBufferData("cameraVS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalVS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));
                commandList.SetBufferData("cameraPS", renderer.RenderUploadBuffer.get(), &cameraCBuffer, sizeof(CameraCBuffer));
                commandList.SetBufferData("decalPS", renderer.RenderUploadBuffer.get(), &decalCBuffer, sizeof(DecalCBuffer));

                if (decal.IsAlbedoUsed())
                {
                    commandList.SetTextureSRV("albedoTexture", decal.GetAlbedoTexture()->GetTexture());
                }
                else { commandList.SetTextureSRV("albedoTexture", *params.FallbackTexture); }

                if (decal.IsNormalUsed())
                {
                    commandList.SetTextureSRV("normalTexture", decal.GetNormalTexture()->GetTexture());
                    commandList.SetTextureSRV("normalBuffer", *m_copyTexture);
                }
                else
                {
                    commandList.SetTextureSRV("normalTexture", *params.FallbackTexture);
                    commandList.SetTextureSRV("normalBuffer", *params.FallbackTexture);
                }
                commandList.SetTextureSRV("depthTexture", *params.DepthStencilTexture);

                commandList.SetVertexBuffer(*m_cubeVertexBuffer);

                commandList.DrawIndexedInstanced(*m_cubeIndexBuffer, 1, 0, 0, 0);

               /* bee::Engine.DebugRenderer().AddCube(bee::DebugCategory::Rendering,
                                                    transform.GetTranslation(),
                                                    transform.GetScale().x * 2,
                                                    glm::vec4(1.f, 0.f, 0.f, 1.f));*/
            }
        }

        commandList.EndRender();
    });
}