#include "systems/particles/particle_components.hpp"

#include "core/engine.hpp"
#include "core/ecs.hpp"
#include "core/transform.hpp"

#include "rendering/render_components.hpp"

#include "systems/particles/emitter.hpp"

ParticleComponent::ParticleComponent(EmitterComponent& emitter,
                                          glm::vec3 pos,
                                          glm::vec4 color,
                                          glm::vec3 vel,
                                          glm::vec2 scale,
                                          float lifetime,
                                          entt::entity handle)
{
    m_billboard = &Engine.ECS().Registry.emplace<ParticleBillboard>(handle, pos, scale);
    m_maxLife = lifetime;
    m_scaleDown = emitter.m_scaleDown;
    m_initialScale = scale;

    m_color = color;
    m_velocity = vel;
    m_handle = handle;
    m_parent = emitter.m_handle;
}

void TrailComponent::AddSegment()
{
    m_segments.emplace_back(m_particleTransform, m_particleColor, m_particleScale, m_lifetime);
}
