#include "systems/particles/particle_system.hpp"

#include "entt/entt.hpp"

#include "core/engine.hpp"
#include "core/ecs.hpp"
#include "core/transform.hpp"

#include "systems/particles/emitter.hpp"
#include "systems/particles/particle_components.hpp"
#include "rendering/render_components.hpp"

#include <glm/gtc/type_ptr.hpp>

EmitterComponent& ParticleSystem::SpawnEmitter(glm::vec3 pos, glm::vec3 rot)
{
    auto& registry = Engine.ECS().Registry;
    entt::entity entity = registry.create();
    EmitterComponent& emitter = registry.emplace<EmitterComponent>(entity, entity);

    Transform& transform = registry.emplace<Transform>(entity);
    transform.SetTranslation(pos);

    glm::vec3 from = glm::vec3(0.0f, 0.0f, 1.0f);  // Reference forward
    glm::vec3 to = glm::normalize(rot);            // Your target direction

    glm::quat rotationQuat = glm::rotation(from, to);
    transform.SetRotation(rotationQuat);

    return emitter;
}

EmitterComponent& ParticleSystem::SpawnEmitter(Entity entity, glm::vec3 pos, glm::vec3 rot)
{
    auto& registry = Engine.ECS().Registry;
    EmitterComponent& emitter = registry.emplace<EmitterComponent>(entity, entity);

    Transform& transform = registry.emplace<Transform>(entity);
    transform.SetTranslation(pos);

    glm::vec3 from = glm::vec3(0.0f, 0.0f, 1.0f);  // Reference forward
    glm::vec3 to = glm::normalize(rot);            // Your target direction

    glm::quat rotationQuat = glm::rotation(from, to);
    transform.SetRotation(rotationQuat);

    return emitter;
}

void ParticleSystem::SpawnDefaultBurst(glm::vec3 pos, int amountOfParticles)
{
    glm::vec3 dir = glm::vec3(0, 1, 0);
    auto& emitter = SpawnEmitter(pos, dir)
                        .SetConeAngle(360.f)
                        .SetScale(glm::vec2(0.12f, 0.02f))
                        .SetMinMaxVelocity(glm::vec2(2.f, 4.f))
                        .SetStartColor(glm::vec4(0.94f, 0.76f, 0.18f, 1.0f))
                        .SetParticleLifetime(0.3f);

    emitter.BurstAndDelete(amountOfParticles);
}

#ifdef INSPECTOR
void ParticleSystem::OnEntity(entt::entity entt)
{
    auto pEmitter = Engine.ECS().Registry.try_get<EmitterComponent>(entt);
    if (pEmitter)
    {
        if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            InspectorDisc(entt);
            ImGui::DragFloat("Cone Angle", &pEmitter->m_coneAngle, 1.f, 0.f, 360.0f);
            InspectorEmissionRate(pEmitter);
            ImGui::DragInt("Pool Max", &pEmitter->m_poolMax, 1.f, 0, 10000);
        }

        if (ImGui::CollapsingHeader("Particle Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            InspectorVelocity(pEmitter);
            InspectorScale(pEmitter);
            InspectorLifetime(pEmitter);

            ImGui::Checkbox("Scale Down", &pEmitter->m_scaleDown);

            ImGui::ColorEdit4("Color", glm::value_ptr(pEmitter->m_startColor));

            InspectorEndColor(entt);
            InspectorBloom(entt);

            if (!InspectorEndLocation(entt))
            {
                InspectorMass(entt);
            }

            InspectorTrail(entt);
        }
    }
}

void ParticleSystem::InspectorMass(entt::entity entt)
{
    auto pMass = Engine.ECS().Registry.try_get<MassComponent>(entt);
    bool hasMass = pMass;

    if (hasMass)
    {
        if (ImGui::Checkbox("Mass", &hasMass))
        {
            Engine.ECS().Registry.remove<MassComponent>(entt);
            pMass = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Mass", &hasMass))
        {
            pMass = &Engine.ECS().Registry.emplace<MassComponent>(entt);
        }
    }

    if (pMass) ImGui::DragFloat("Particle Mass", &pMass->m_mass, 0.01f, 0.01f, 5.0f);
}

void ParticleSystem::InspectorEndColor(entt::entity entt)
{
    auto pEndColor = Engine.ECS().Registry.try_get<EndColorComponent>(entt);
    bool hasEndColor = pEndColor;

    if (hasEndColor)
    {
        if (ImGui::Checkbox("Use second color", &hasEndColor))
        {
            Engine.ECS().Registry.remove<EndColorComponent>(entt);
            pEndColor = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Use second color", &hasEndColor))
        {
            pEndColor = &Engine.ECS().Registry.emplace<EndColorComponent>(entt);
        }
    }

    if (pEndColor) ImGui::ColorEdit4("Second Color", glm::value_ptr(pEndColor->m_color));
}

bool ParticleSystem::InspectorEndLocation(entt::entity entt)
{
    auto pEndLocation = Engine.ECS().Registry.try_get<EndLocationComponent>(entt);
    bool hasEndLocation = pEndLocation;

    if (hasEndLocation)
    {
        if (ImGui::Checkbox("Use End Location", &hasEndLocation))
        {
            Engine.ECS().Registry.remove<EndLocationComponent>(entt);
            pEndLocation = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Use End Location", &hasEndLocation))
        {
            pEndLocation = &Engine.ECS().Registry.emplace<EndLocationComponent>(entt);
        }
    }

    if (pEndLocation) ImGui::InputFloat3("End Location", glm::value_ptr(pEndLocation->m_endPos));

    return pEndLocation;
}

void ParticleSystem::InspectorScale(EmitterComponent* emitter)
{
    int isRandomScale = static_cast<int>(!emitter->m_useConstantParticleScale);
    if (ImGui::Combo("Size Type", &isRandomScale, "Constant\0Random between 2 values\0"))
    {
        emitter->m_useConstantParticleScale = !static_cast<bool>(isRandomScale);
    }

    if (isRandomScale)
    {
        ImGui::DragFloat2("Min Size", glm::value_ptr(emitter->m_minScale), 0.1f, 0.1f, 2.f);
        ImGui::DragFloat2("Max Size", glm::value_ptr(emitter->m_maxScale), 0.1f, 0.1f, 2.f);
    }
    else
    {
        ImGui::DragFloat2("Size", glm::value_ptr(emitter->m_minScale), 0.1f, 0.1f, 2.f);
    }

    ImGui::DragFloat("Size Multiplier", &emitter->m_sizeMultiplier, 0.1f, 0.1f, 5.f);
}

void ParticleSystem::InspectorLifetime(EmitterComponent* emitter)
{
    int isRandomLifetime = static_cast<int>(!emitter->m_useConstantParticleLifetime);
    if (ImGui::Combo("Lifetime", &isRandomLifetime, "Constant\0Random between 2 values\0"))
    {
        emitter->m_useConstantParticleLifetime = !static_cast<bool>(isRandomLifetime);
    }

    if (isRandomLifetime)
    {
        ImGui::DragFloat2("Particle Lifetime", glm::value_ptr(emitter->m_particleLifetime), 0.1f, 0.1f, 4.0f);
    }
    else
    {
        ImGui::DragFloat("Particle Lifetime", &emitter->m_particleLifetime.x, 0.1f, 0.1f, 4.0f);
    }
}
void ParticleSystem::InspectorTrail(entt::entity entt)
{
    auto pTrail = Engine.ECS().Registry.try_get<TrailComponent>(entt);
    bool hasTrail = pTrail;

    if (hasTrail)
    {
        if (ImGui::Checkbox("Trail", &hasTrail))
        {
            Engine.ECS().Registry.remove<TrailComponent>(entt);
            pTrail = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Trail", &hasTrail))
        {
            pTrail = &Engine.ECS().Registry.emplace<TrailComponent>(entt);
        }
    }

    if (pTrail)
    {
        if (ImGui::CollapsingHeader("Trail Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // intervals of 4, so that it's SIMD friendly in case it's needed.
            int count = pTrail->m_segmentCount;
            if (ImGui::DragInt("Segment Count", &count, 4, 0, 64))
            {
                pTrail->m_segmentCount = count;
            }

            int segmentCount = pTrail->m_particlesPerSegment;
            if (ImGui::DragInt("Particles Per Segment", &segmentCount, 4, 0, 64))
            {
                pTrail->m_particlesPerSegment = segmentCount;
            }

            ImGui::DragFloat("Trail Lifetime", &pTrail->m_maxLife, 0.1f, 0.1f, 3.0f);
            ImGui::Checkbox("Fade In Trail", &pTrail->m_fadeIn);
            ImGui::Checkbox("Scale Down Trail", &pTrail->m_scaleDown);

            InspectorNoise(entt);
        }
    }
}

void ParticleSystem::InspectorNoise(entt::entity entt)
{
    auto pNoise = Engine.ECS().Registry.try_get<ValueNoiseComponent>(entt);
    bool hasNoise = pNoise;

    if (hasNoise)
    {
        if (ImGui::Checkbox("Trail Noise", &hasNoise))
        {
            Engine.ECS().Registry.remove<ValueNoiseComponent>(entt);
            pNoise = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Trail Noise", &hasNoise))
        {
            pNoise = &Engine.ECS().Registry.emplace<ValueNoiseComponent>(entt);
        }
    }

    if (pNoise)
    {
        if (ImGui::CollapsingHeader("Trail Noise Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragInt("Octaves", &pNoise->m_parameters.octaves, 1, 1, 20);
            ImGui::DragFloat("Amplitude", &pNoise->m_parameters.amplitude, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Frequency", &pNoise->m_parameters.frequency, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Gain", &pNoise->m_parameters.gain, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Lacunarity", &pNoise->m_parameters.lacunarity, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Scale", &pNoise->m_parameters.scale, 0.1f, 0.0f, 10.0f);
        }
    }
}

void ParticleSystem::InspectorDisc(entt::entity entt)
{
    auto pDisc = Engine.ECS().Registry.try_get<DiscComponent>(entt);
    bool hasDisc = pDisc;

    if (hasDisc)
    {
        if (ImGui::Checkbox("Use Disc", &hasDisc))
        {
            Engine.ECS().Registry.remove<DiscComponent>(entt);
            pDisc = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Use Disc", &hasDisc))
        {
            pDisc = &Engine.ECS().Registry.emplace<DiscComponent>(entt);
        }
    }

    if (pDisc)
    {
        ImGui::DragFloat("Disc Radius", &pDisc->m_radius, 0.1f, 0.0f, 5.0f);
    }
}

void ParticleSystem::InspectorBloom(entt::entity entt)
{
    auto pBloom = Engine.ECS().Registry.try_get<BloomComponent>(entt);
    bool hasBloom = pBloom;

    if (hasBloom)
    {
        if (ImGui::Checkbox("Use Bloom", &hasBloom))
        {
            Engine.ECS().Registry.remove<BloomComponent>(entt);
            pBloom = nullptr;
        }
    }
    else
    {
        if (ImGui::Checkbox("Use Bloom", &hasBloom))
        {
            pBloom = &Engine.ECS().Registry.emplace<BloomComponent>(entt);
        }
    }

    if (pBloom)
    {
        ImGui::DragFloat("Bloom Strength", &pBloom->m_strength, 0.01f, 0.0f, 4.0f);
    }
}

void ParticleSystem::InspectorEmissionRate(EmitterComponent* emitter)
{
    int isRandomEmission = static_cast<int>(!emitter->m_useConstantEmissionRate);
    if (ImGui::Combo("Emission Type", &isRandomEmission, "Constant\0Random between 2 values\0"))
    {
        emitter->m_useConstantEmissionRate = !static_cast<bool>(isRandomEmission);
    }

    if (isRandomEmission)
    {
        ImGui::DragFloat2("Min/Max Emission Rate", glm::value_ptr(emitter->m_emissionRate), 0.5f, 0.f, 30.f);
    }
    else
    {
        ImGui::DragFloat("Emission Rate", &emitter->m_emissionRate.x, 0.5f, 0.f, 30.f);
    }
}

void ParticleSystem::InspectorVelocity(EmitterComponent* emitter)
{
    int isRandomVelocity = static_cast<int>(!emitter->m_useConstantVelocity);
    if (ImGui::Combo("Velocity Setting", &isRandomVelocity, "Constant\0Random between 2 values\0"))
    {
        emitter->m_useConstantVelocity = !static_cast<bool>(isRandomVelocity);
    }

    if (isRandomVelocity)
    {
        ImGui::DragFloat2("Min/Max Velocity", glm::value_ptr(emitter->m_velocity), 0.1f, 0.1f, 5.f);
    }
    else
    {
        ImGui::DragFloat("Velocity", &emitter->m_velocity.x, 0.1f, 0.1f, 5.f);
    }
}
#endif

void ParticleSystem::Update(float dt)
{
    EmitParticles(dt);

    UpdateParticles(dt);
    UpdateSegments(dt);

    RemoveDeadParticles();
    RemoveDeadEmitters();
}

void ParticleSystem::EmitParticles(float dt)
{
    auto& registry = Engine.ECS().Registry;

    // Emit particles
    auto view = registry.view<EmitterComponent>();
    for (auto&& [entity, emitter] : view.each())
    {
        if (emitter.m_handle == entt::null) emitter.m_handle = entity;
        if (emitter.m_play == false) continue;

        const float value =
            emitter.m_useConstantEmissionRate ? 1.f / emitter.m_emissionRate.x : 1.f / GetRandomValue(emitter.m_emissionRate);

        emitter.m_frames += 1.f * dt;

        while (emitter.m_frames >= value)
        {
            emitter.EmitParticle();

            emitter.m_frames -= value;
        }

        if (emitter.m_checkDuration && ((emitter.m_timeElapsed - emitter.m_timeStamp) > emitter.m_playDuration))
        {
            emitter.m_play = false;
            emitter.m_checkDuration = false;
        }
        emitter.m_timeElapsed += dt;
    }
}

void ParticleSystem::UpdateParticles(float dt)
{
    auto& registry = Engine.ECS().Registry;

    // Update particles
    auto particle_view = registry.view<ParticleComponent, ParticleBillboard>();
    for (const auto& [entity, particle, billboard] : particle_view.each())
    {
        if (registry.all_of<EndLocationComponent>(entity))
        {
            float time = particle.m_lifetime / particle.m_maxLife;
            auto& endLocation = registry.get<EndLocationComponent>(entity);
            glm::vec3 n1 = endLocation.m_startPos;
            glm::vec3 n3 = endLocation.m_endPos;
            glm::vec3 n2 = glm::vec3(n3.x, n1.y - 0.5f, n3.z);

            float invTime = 1 - time;

            // Bezier curve to the end location
            glm::vec3 pos = (invTime * invTime) * n1 + 2 * invTime * time * n2 + time * time * n3;
            billboard.m_transform->SetTranslation(pos);
        }
        else
        {
            billboard.m_transform->SetTranslation(billboard.m_transform->GetTranslation() + particle.m_velocity * dt);

            if (registry.all_of<MassComponent>(entity))
            {
                // Maybe make mass a vec3?
                const float mass = registry.get<MassComponent>(entity).m_mass;
                particle.m_velocity += glm::vec3(0, -9.81f * mass * dt, 0);
            }
        }

        if (registry.all_of<TrailComponent>(entity))
        {
            UpdateTrailData(particle, billboard);
        }

        particle.m_lifetime += dt;
    }
}

// Split trail particles up in 2 groups; The first group contains the trails where the leading particle is still active,
// while the second group holds the trails where the leading particle is dead.
void ParticleSystem::UpdateSegments(float dt)
{
    // Group with the leading particle alive:
    auto& registry = Engine.ECS().Registry;
    for (const auto& [entity, trail, particle] : registry.view<TrailComponent, ParticleComponent>().each())
    {
        trail.m_lifetime += dt;

        // I want a trail particle to spawn every (maxlifetime / max segment count) seconds
        const auto intervalLifetime = trail.m_maxLife / trail.m_segmentCount;
        const auto amountOfIntervals = trail.m_segments.size();

        if ((trail.m_totalAmountOfSegments + 1) * intervalLifetime < trail.m_lifetime)
        {
            assert(amountOfIntervals <= trail.m_segmentCount &&
                   "Amount of intervals should not be greater than the set maximum.");

            trail.m_totalAmountOfSegments++;
            if (amountOfIntervals == trail.m_segmentCount)
            {
                // Pop front
                trail.m_segments.erase(trail.m_segments.begin());
            }

            trail.AddSegment();
        }

        RemoveDeadSegments(trail);
    }

    // Group with the leading particle dead:
    for (const auto& [entity, trail] : registry.view<TrailComponent>(entt::exclude<EmitterComponent, ParticleComponent>).each())
    {
        trail.m_lifetime += dt;

        RemoveDeadSegments(trail);

        // If all segments are gone destroy the trail
        if (trail.m_segments.empty())
        {
            Engine.ECS().Registry.destroy(entity);
        }
    }
}

void ParticleSystem::UpdateTrailData(ParticleComponent& particle, ParticleBillboard& billboard)
{
    auto& trail = Engine.ECS().Registry.get<TrailComponent>(particle.m_handle);

    bool useFade = false;
    float time = particle.m_lifetime / particle.m_maxLife;
    if (time <= 0.5)
    {
        if (trail.m_fadeIn)
        {
            particle.m_billboard->m_size = glm::mix(glm::vec2(0.f, 0.f), particle.m_initialScale, time * 2);
            useFade = true;
        }
    }
    else
    {
        if (trail.m_scaleDown)
        {
            particle.m_billboard->m_size = glm::mix(particle.m_initialScale, glm::vec2(0.f, 0.f), (time * 2) - 1);
            useFade = true;
        }
    }

    glm::vec2 scale;
    scale = useFade ? particle.m_billboard->m_size : particle.m_initialScale;
    trail.m_particleScale = scale;

    glm::vec4 color = particle.m_color;
    if (Engine.ECS().Registry.all_of<EndColorComponent>(particle.m_handle))
    {
        glm::vec4 endColor = Engine.ECS().Registry.get<EndColorComponent>(particle.m_handle).m_color;
        color = glm::mix(color, endColor, time);
    }

    if (Engine.ECS().Registry.all_of<BloomComponent>(particle.m_handle))
    {
        float strength = Engine.ECS().Registry.get<BloomComponent>(particle.m_handle).m_strength;
        color += (color * strength);
    }

    trail.m_particleColor = color;
    trail.m_particleTransform = Transform(billboard.m_transform->GetTranslation(),
                                          billboard.m_transform->GetScale(),
                                          billboard.m_transform->GetRotation());
}

void ParticleSystem::RemoveDeadSegments(TrailComponent& trail)
{
    while (!trail.m_segments.empty())
    {
        if (!trail.m_segments[0].IsAlive(trail.m_maxLife, trail.m_lifetime))
        {
            trail.m_segments.erase(trail.m_segments.begin());
        }
        else
        {
            // First segment is still alive; all following are too.
            break;
        }
    }
}

void ParticleSystem::RemoveDeadParticles()
{
    auto& registry = Engine.ECS().Registry;

    // Get a view of all emitters, then loop over the pools to see which particles died.
    // Remove the particles from both the registry as the pool.
    auto view = registry.view<ParticleComponent>();
    for (const auto& [entity, particle] : view.each())
    {
        if (!particle.IsAlive())
        {
            if (registry.all_of<EmitterComponent>(particle.m_parent))
            {
                registry.get<EmitterComponent>(particle.m_parent).m_pool--;
            }

            if (registry.try_get<TrailComponent>(particle.m_handle))
            {
                registry.remove<ParticleComponent>(particle.m_handle);
            }
            else
            {
                registry.destroy(entity);
            }
        }
    }
}

void ParticleSystem::RemoveDeadEmitters()
{
    auto& registry = Engine.ECS().Registry;

    auto view = registry.view<EmitterComponent>();
    for (auto entity : view)
    {
        auto& emitter = view.get<EmitterComponent>(entity);

        if (!emitter.IsAlive())
        {
            registry.destroy(entity);
            emitter.~EmitterComponent();
        }
    }
}
