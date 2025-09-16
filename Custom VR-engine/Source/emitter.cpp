#include "systems/particles/emitter.hpp"

#include "core/engine.hpp"
#include "core/ecs.hpp"
#include "core/transform.hpp"

#include "systems/particles/particle_components.hpp"
#include "systems/particles/particle_system_helpers.hpp"

#include "imgui/imgui.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

EmitterComponent::EmitterComponent(entt::entity entity) : m_handle(entity) {}

void EmitterComponent::EmitParticle()
{
    if (m_handle == entt::null) return;
    auto& registry = Engine.ECS().Registry;
    auto& transform = registry.get<Transform>(m_handle);

    if (m_pool < m_poolMax)
    {
        ConstructComponents(transform);
    }
}

bool EmitterComponent::IsAlive() const { return m_lifetime == 0.f || m_timeElapsed < m_lifetime; }

void EmitterComponent::SetPlay(bool play)
{
    m_play = play;
    m_checkDuration = false;
}

void EmitterComponent::PlayForDuration(float timeInSeconds)
{
    m_play = true;
    m_checkDuration = true;
    m_timeStamp = m_timeElapsed;
    m_playDuration = timeInSeconds;
}

void EmitterComponent::Burst(int amountOfParticles)
{
    assert(amountOfParticles < 10000 && "I mean.. Come on...");

    auto& registry = Engine.ECS().Registry;
    auto& transform = registry.get<Transform>(m_handle);

    for (int i = 0; i < amountOfParticles; i++)
    {
        ConstructComponents(transform);
    }
}

void EmitterComponent::BurstAndDelete(int amountOfParticles)
{
    Burst(amountOfParticles);

    m_lifetime = m_particleLifetime.y;
    m_shouldDie = true;
}

EmitterComponent& EmitterComponent::AddMass(float mass)
{
    Engine.ECS().Registry.emplace_or_replace<MassComponent>(m_handle, mass);
    return *this;
}

EmitterComponent& EmitterComponent::AddEndColor(glm::vec4 color)
{
    Engine.ECS().Registry.emplace_or_replace<EndColorComponent>(m_handle, color);
    return *this;
}

EmitterComponent& EmitterComponent::AddEndLocation(glm::vec3 endPos)
{
    Engine.ECS().Registry.emplace_or_replace<EndLocationComponent>(m_handle, endPos);
    return *this;
}

EmitterComponent& EmitterComponent::AddTrail(unsigned int intervalCount,
                                                  unsigned int particlesPerSegment,
                                                  float lifetime,
                                                  bool fadeIn,
                                                  bool scaleDown)
{
    Engine.ECS()
        .Registry.emplace_or_replace<TrailComponent>(m_handle, intervalCount, particlesPerSegment, lifetime, fadeIn, scaleDown);
    return *this;
}

EmitterComponent& EmitterComponent::AddNoise(int octaves,
                                                  float amplitude,
                                                  float frequency,
                                                  float gain,
                                                  float lacunarity,
                                                  float scale)
{
    Engine.ECS()
        .Registry.emplace_or_replace<ValueNoiseComponent>(m_handle, octaves, lacunarity, gain, amplitude, frequency, scale);
    return *this;
}

EmitterComponent& EmitterComponent::AddDisc(float radius)
{
    Engine.ECS().Registry.emplace_or_replace<DiscComponent>(m_handle, radius);
    return *this;
}

EmitterComponent& EmitterComponent::AddBloom(float strength)
{
    Engine.ECS().Registry.emplace_or_replace<BloomComponent>(m_handle, strength);
    return *this;
}

EmitterComponent& EmitterComponent::SetMinMaxVelocity(glm::vec2 vel)
{
    m_velocity = vel;
    return *this;
}

EmitterComponent& EmitterComponent::SetParticleLifetime(float lifetime)
{
    m_particleLifetime.x = lifetime;
    return *this;
}

EmitterComponent& EmitterComponent::SetPoolMax(int poolMax)
{
    m_poolMax = poolMax;
    return *this;
}

EmitterComponent& EmitterComponent::SetConeAngle(float coneAngle)
{
    m_coneAngle = coneAngle;
    return *this;
}

EmitterComponent& EmitterComponent::SetEmissionRate(float emissionRate)
{
    m_emissionRate.x = emissionRate;
    return *this;
}

EmitterComponent& EmitterComponent::SetScale(glm::vec2 scale)
{
    m_minScale = scale;
    return *this;
}

EmitterComponent& EmitterComponent::SetStartColor(glm::vec4 color)
{
    m_startColor = color;
    return *this;
}

glm::vec2 EmitterComponent::GetMinMaxVelocity() const { return m_velocity; }

float EmitterComponent::GetParticleLifetime() const { return m_particleLifetime.x; }

int EmitterComponent::GetPoolMax() const { return m_poolMax; }

float EmitterComponent::GetConeAngle() const { return m_coneAngle; }

float EmitterComponent::GetEmissionRate() const { return m_emissionRate.x; }

glm::vec2 EmitterComponent::GetScale() const { return m_minScale; }

glm::vec4 EmitterComponent::GetStartColor() const { return m_startColor; }

EndColorComponent* EmitterComponent::GetEndColor() const
{
    return Engine.ECS().Registry.try_get<EndColorComponent>(m_handle);
}

EndLocationComponent* EmitterComponent::GetEndLocation() const
{
    return Engine.ECS().Registry.try_get<EndLocationComponent>(m_handle);
}

TrailComponent* EmitterComponent::GetTrail() const { return Engine.ECS().Registry.try_get<TrailComponent>(m_handle); }

ValueNoiseComponent* EmitterComponent::GetNoise() const
{
    return Engine.ECS().Registry.try_get<ValueNoiseComponent>(m_handle);
}

DiscComponent* EmitterComponent::GetDisc() const { return Engine.ECS().Registry.try_get<DiscComponent>(m_handle); }

BloomComponent* EmitterComponent::GetBloom() const { return Engine.ECS().Registry.try_get<BloomComponent>(m_handle); }

MassComponent* EmitterComponent::GetMass() const { return Engine.ECS().Registry.try_get<MassComponent>(m_handle); }

EmitterComponent& EmitterComponent::ToggleScaleDown()
{
    m_scaleDown = true;
    return *this;
}

void EmitterComponent::ConstructComponents(Transform& emitterTransform)
{
    auto& registry = Engine.ECS().Registry;

    m_pool++;
    entt::entity particle = registry.create();
    glm::vec3 velocity;
    velocity = m_useConstantVelocity ? RandomVelocity(*this, glm::vec2(m_velocity.x, m_velocity.x))
                                     : RandomVelocity(*this, m_velocity);
    glm::vec2 scale = m_useConstantParticleScale ? m_minScale : GetRandomValue(m_minScale, m_maxScale);
    float lifetime = m_useConstantParticleLifetime ? m_particleLifetime.x : GetRandomValue(m_particleLifetime);

    glm::vec3 startPos = emitterTransform.GetWorldTranslation();

    // Calculate a random point on a disc to emitter from
    if (registry.all_of<DiscComponent>(m_handle))
    {
        auto& disc = Engine.ECS().Registry.get<DiscComponent>(m_handle);

        float randNum1 = static_cast<float>(std::rand()) / RAND_MAX;
        float randNum2 = static_cast<float>(std::rand()) / RAND_MAX;
        float r = sqrt(randNum1 * 1.0f) * disc.m_radius;
        float theta = randNum2 * 2.0f * glm::pi<float>();
        float x = r * cos(theta);
        float y = r * sin(theta);

        glm::vec3 localOffset = glm::vec3(x, y, 0.0f);

        // if there's an end component, orient the disc towards that point
        if (registry.all_of<EndLocationComponent>(m_handle))
        {
            auto& component = registry.get<EndLocationComponent>(m_handle);

            // ChatGPT, I was low on time
            //
            // Build rotation matrix so disc faces m_endPos
            glm::vec3 emitterPos = glm::vec3(emitterTransform.World()[3]);  // translation from mat4
            glm::vec3 forward = glm::normalize(component.m_endPos - emitterPos);
            glm::vec3 up = glm::vec3(0, 0, 1);  // arbitrary up (Z-up)

            // If forward and up are colinear, fix up
            if (glm::abs(glm::dot(forward, up)) > 0.99f) up = glm::vec3(0, 1, 0);  // fallback up

            glm::vec3 right = glm::normalize(glm::cross(up, forward));
            glm::vec3 realUp = glm::normalize(glm::cross(forward, right));
            glm::mat3 rotationMatrix = glm::mat3(right, realUp, forward);

            // Convert to mat4 and set position
            glm::mat4 orient = glm::mat4(rotationMatrix);
            orient[3] = emitterTransform.World()[3];  // keep translation
            //

            startPos = glm::vec3(orient * glm::vec4(localOffset, 1.0f));
        }
        else
        {
            startPos = glm::vec3(emitterTransform.World() * glm::vec4(localOffset, 1.0f));
        }
    }

    registry.emplace<ParticleComponent>(particle,
                                        *this,
                                        startPos,
                                        m_startColor,
                                        velocity,
                                        scale * m_sizeMultiplier,
                                        lifetime,
                                        particle);

    // Mass component and End location component do not work together.
    if (registry.all_of<EndLocationComponent>(m_handle))
    {
        auto& component = registry.get<EndLocationComponent>(m_handle);
        registry.emplace<EndLocationComponent>(particle, startPos, component.m_endPos);
    }
    else
    {
        if (registry.all_of<MassComponent>(m_handle))
        {
            auto& component = registry.get<MassComponent>(m_handle);
            registry.emplace<MassComponent>(particle, component.m_mass);
        }
    }

    if (registry.all_of<EndColorComponent>(m_handle))
    {
        auto& component = registry.get<EndColorComponent>(m_handle);
        registry.emplace<EndColorComponent>(particle, component.m_color);
    }

    if (registry.all_of<TrailComponent>(m_handle))
    {
        auto& component = registry.get<TrailComponent>(m_handle);
        registry.emplace<TrailComponent>(particle,
                                         component.m_segmentCount,
                                         component.m_particlesPerSegment,
                                         component.m_maxLife,
                                         component.m_fadeIn,
                                         component.m_scaleDown);
    }

    if (registry.all_of<ValueNoiseComponent>(m_handle))
    {
        auto& component = registry.get<ValueNoiseComponent>(m_handle);
        registry.emplace<ValueNoiseComponent>(particle,
                                              component.m_parameters.octaves,
                                              component.m_parameters.lacunarity,
                                              component.m_parameters.gain,
                                              component.m_parameters.amplitude,
                                              component.m_parameters.frequency,
                                              component.m_parameters.scale);
    }

    if (registry.all_of<BloomComponent>(m_handle))
    {
        auto& component = registry.get<BloomComponent>(m_handle);
        registry.emplace<BloomComponent>(particle, component.m_strength);
    }
}

glm::vec3 EmitterComponent::RandomVelocity(EmitterComponent& emitter, glm::vec2 bounds)
{
    float randmaxInverse = 1.f / RAND_MAX;

    float azimuth = glm::radians(((static_cast<float>(rand()) * randmaxInverse)) * 360.f);
    float inclination = glm::radians(((static_cast<float>(rand()) * randmaxInverse)) * emitter.m_coneAngle);

    float r = (static_cast<float>(rand()) * randmaxInverse) * bounds.y;
    if (r < bounds.x) r = bounds.x;

    glm::vec4 localVelocity =
        glm::vec4(r * sin(inclination) * cos(azimuth), r * sin(inclination) * sin(azimuth), r * cos(inclination), 0.0f);

    auto& transformComponent = Engine.ECS().Registry.get<Transform>(emitter.m_handle);
    glm::quat quaternion = transformComponent.GetWorldRotation();  // glm::quat(glm::radians(emitter.m_orientation));
    glm::mat4 model = glm::mat4_cast(quaternion);

    return model * localVelocity;
}

// Emitter manager
void EmittersManager::AddEmitterEntity(std::string name, Entity entity) { m_emitters[name] = entity; };

Entity EmittersManager::GetEmitterEntity(std::string name)
{
    auto it = m_emitters.find(name);
    if (it != m_emitters.end())
        return m_emitters[name];
    else
        return entt::null;
}
