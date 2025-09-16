#pragma once

#include "core/ecs.hpp"

#include "inspector/inspectable.hpp"

#include "systems/particles/particle_system_helpers.hpp"

#include <memory>

#include <glm/glm.hpp>

struct EmitterComponent;
struct ParticleComponent;
struct ParticleBillboard;
struct TrailComponent;
struct Texture;
class ParticleSystem : public System, public IEntityInspector
{
public:
    // Spawns a particles until the user deletes the emitter
    EmitterComponent& SpawnEmitter(glm::vec3 pos, glm::vec3 rot);
    EmitterComponent& SpawnEmitter(Entity entity, glm::vec3 pos, glm::vec3 rot);


    void SpawnDefaultBurst(glm::vec3 pos, int amountOfParticles);

#ifdef INSPECTOR
    void OnEntity(entt::entity entt) override;
    void InspectorMass(entt::entity entt);
    void InspectorEndColor(entt::entity entt);
    bool InspectorEndLocation(entt::entity entt);
    void InspectorEmissionRate(EmitterComponent* emitter);
    void InspectorScale(EmitterComponent* emitter);
    void InspectorVelocity(EmitterComponent* emitter);
    void InspectorLifetime(EmitterComponent* emitter);
    void InspectorTrail(entt::entity entt);
    void InspectorNoise(entt::entity entt);
    void InspectorDisc(entt::entity entt);
    void InspectorBloom(entt::entity entt);
#endif

private:
    friend class EngineClass;

    void Update(float dt);

    void EmitParticles(float dt);
    void UpdateParticles(float dt);
    void UpdateSegments(float dt);
    void UpdateTrailData(ParticleComponent& particle, ParticleBillboard& billboard);
    void RemoveDeadSegments(TrailComponent& trail);
    void RemoveDeadParticles();
    void RemoveDeadEmitters();
};