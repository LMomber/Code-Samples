#include "backends/d3d12/buffer_d3d12.hpp"
#include "backends/d3d12/commandqueue_d3d12.hpp"
#include "backends/d3d12/commandlist_d3d12.hpp"
#include "backends/d3d12/descriptor_allocator_d3d12.hpp"
#include "backends/d3d12/device_d3d12.hpp"
#include "backends/d3d12/fence_d3d12.hpp"
#include "backends/d3d12/pso_d3d12.hpp"
#include "backends/d3d12/shader_d3d12.hpp"
#include "backends/d3d12/texture_firefly_d3d12.hpp"
#include "platform/firefly/rendering/render_firefly.hpp"
#include "core/ecs.hpp"
#include "backends/d3d12/upload_buffer_d3d12.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
#include "DirectXMath.h"
#include "rendergraph/blackboard.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/render_components.hpp"
#include "tools/log.hpp"
#include "common.hpp"
#include <array>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

using namespace bee;
using namespace glm;

class bee::DebugRenderer::Impl
{
public:
    Impl();
    ~Impl();
    bool AddLine(const vec3& from, const vec3& to, const vec4& color);
    void Render();

    static int const m_maxLines = 32760;
    int m_linesCount = 0;

    std::array<glm::vec3, m_maxLines * 2> m_vertexPosition;
    std::array<glm::vec4, m_maxLines * 2> m_vertexColor;

    std::unique_ptr<ff::Buffer> m_positions;
    std::unique_ptr<ff::Buffer> m_color;

    ff::UploadBuffer* m_uploadBuffer;
};

bee::DebugRenderer::DebugRenderer()
{
    m_categoryFlags = DebugCategory::General | DebugCategory::Gameplay | DebugCategory::Physics | DebugCategory::Rendering |
                      DebugCategory::AINavigation | DebugCategory::AIDecision | DebugCategory::Editor;

    m_impl = std::make_unique<Impl>();
}

DebugRenderer::~DebugRenderer() = default;

void DebugRenderer::Render() { m_impl->Render(); }

void DebugRenderer::AddLine(DebugCategory::Enum category, const vec3& from, const vec3& to, const vec4& color)
{
    if (!(m_categoryFlags & category)) return;
    m_impl->AddLine(from, to, color);
}

bee::DebugRenderer::Impl::Impl()
{
    ff::BufferDesc posBufferDesc;
    posBufferDesc.DebugName = "Debug Positions";
    posBufferDesc.Format = ff::DataFormat::Float3;
    posBufferDesc.TypeFlags = ff::BufferDesc::Write;
    posBufferDesc.ViewFlags = ff::BufferDesc::VertexBuffer;
    posBufferDesc.NumElements = m_maxLines * 2;
    m_positions = std::make_unique<ff::Buffer>(posBufferDesc);

    ff::BufferDesc colorBufferDesc;
    colorBufferDesc.DebugName = "Debug Colors";
    colorBufferDesc.Format = ff::DataFormat::Float4;
    colorBufferDesc.TypeFlags = ff::BufferDesc::Write;
    colorBufferDesc.ViewFlags = ff::BufferDesc::VertexBuffer;
    colorBufferDesc.NumElements = m_maxLines * 2;
    m_color = std::make_unique<ff::Buffer>(colorBufferDesc);

    m_uploadBuffer = new ff::UploadBuffer();
}

bee::DebugRenderer::Impl::~Impl()
{
    /*
    delete[] m_vertexArray;
    glDeleteVertexArrays(1, &m_linesVAO);
    glDeleteBuffers(1, &m_linesVBO);
    */
    ;
}

bool bee::DebugRenderer::Impl::AddLine(const vec3& from, const vec3& to, const vec4& color)
{
    if (m_linesCount < m_maxLines)
    {
        m_vertexPosition[m_linesCount * 2] = from;
        m_vertexPosition[m_linesCount * 2 + 1] = to;
        m_vertexColor[m_linesCount * 2] = color;
        m_vertexColor[m_linesCount * 2 + 1] = color;
        ++m_linesCount;
        return true;
    }
    return false;
}

void bee::DebugRenderer::Impl::Render()
{
    if (m_linesCount > 0)
    {
        m_uploadBuffer->Reset();
        const std::string textureName = "Composite ScreenTexture";
        ff::Texture* OutputTexture = bee::Engine.ECS().GetSystem<ff::FireflyRenderer>().CompositeTextureConverted;

        ff::CommandList& commandList = bee::Engine.Device().GetTemporaryCommandList(ff::ListType::Direct);

        ff::PipelineSettings settings;
        settings.ShaderState.InputLayout = {
            ff::InputParameter("POSITION", ff::DataFormat::Float3, 0),
            ff::InputParameter("COLOR", ff::DataFormat::Float4, 1),
        };
        const auto shaderPath = "shaders/debug";
        settings.ShaderState.VertexShader = ff::ShaderCache::GetOrCreate(shaderPath, ff::ShaderType::Vertex, "VS_main");
        settings.ShaderState.PixelShader = ff::ShaderCache::GetOrCreate(shaderPath, ff::ShaderType::Pixel, "PS_main");
        settings.RasterizerState.TopologyType = ff::TopologyType::Line;

        commandList.BeginRender({OutputTexture},
                                {ff::ClearOperation::Store},
                                nullptr,
                                ff::DepthStencilClearOperation::Store,
                                "Debug Lines");

        commandList.SetPipelineSettings(settings);

        auto entt = Engine.ECS().Registry.view<Transform, Camera>().front();
        auto& camera = Engine.ECS().Registry.get<Camera>(entt);
        auto& cameraTransform = Engine.ECS().Registry.get<Transform>(entt);

        const glm::mat4 cameraWorld = cameraTransform.World();
        const glm::mat4 view = glm::inverse(cameraWorld);
        glm::mat4 vp = camera.Projection * view;

        const uint32_t components = m_linesCount * 2;
        m_positions->Write(m_vertexPosition.data(), components * sizeof(glm::vec3));
        m_color->Write(m_vertexColor.data(), components * sizeof(glm::vec4));

        
        commandList.SetBufferData("LineBuffer", m_uploadBuffer, &vp, sizeof(glm::mat4));

        const std::vector<ff::Buffer*> vertexBuffers = {m_positions.get(), m_color.get()};
        commandList.SetVertexBuffers(vertexBuffers);
        commandList.DrawInstanced(m_linesCount * 2, 1, 0, 0);
        commandList.EndRender();

        auto queue = Engine.Device().Queue(ff::QueueType::Direct);

        queue->Execute(commandList);
    }

    m_linesCount = 0;
}
