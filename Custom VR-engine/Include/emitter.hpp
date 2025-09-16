#pragma once

#include <glm/glm.hpp>
#include "entt/entt.hpp"

#include "rendering/render_components.hpp"

#include <imgui/IconsFontAwesome6.h>
#include <memory>

struct MassComponent;
struct EndColorComponent;
struct EndLocationComponent;
struct TrailComponent;
struct ValueNoiseComponent;
struct DiscComponent;
struct BloomComponent;
struct EmitterComponent
{
    EmitterComponent() {}
    EmitterComponent(entt::entity entity);
    ~EmitterComponent() = default;

    void EmitParticle();
    bool IsAlive() const;

    void SetPlay(bool play);

    void PlayForDuration(float timeInSeconds);

    void Burst(int amountOfParticles);
    void BurstAndDelete(int amountOfParticles);

    EmitterComponent& AddMass(float mass);
    EmitterComponent& AddEndColor(glm::vec4 color);
    EmitterComponent& AddEndLocation(glm::vec3 pos);
    EmitterComponent& AddTrail(unsigned int intervalCount, unsigned int particlesPerSegment, float lifetime, bool fadeIn, bool scaleDown);
    EmitterComponent& AddNoise(int octaves, float amplitude, float frequency, float gain, float lacunarity, float scale);
    EmitterComponent& AddDisc(float radius);
    EmitterComponent& AddBloom(float strength);

    EmitterComponent& SetMinMaxVelocity(glm::vec2 vel);
    EmitterComponent& SetParticleLifetime(float lifetime);
    EmitterComponent& SetPoolMax(int poolMax);
    EmitterComponent& SetConeAngle(float coneAngle);
    EmitterComponent& SetEmissionRate(float emissionRate);
    EmitterComponent& SetScale(glm::vec2 scale);
    EmitterComponent& SetStartColor(glm::vec4 color);

    glm::vec2 GetMinMaxVelocity() const;
    float GetParticleLifetime() const;
    int GetPoolMax() const;
    float GetConeAngle() const;
    float GetEmissionRate() const;
    glm::vec2 GetScale() const;
    glm::vec4 GetStartColor() const;

    MassComponent* GetMass() const;
    EndColorComponent* GetEndColor() const;
    EndLocationComponent* GetEndLocation() const;
    TrailComponent* GetTrail() const;
    ValueNoiseComponent* GetNoise() const;
    DiscComponent* GetDisc() const;
    BloomComponent* GetBloom() const;

    EmitterComponent& ToggleScaleDown();
    template <class Archive>
    void serialize(Archive& ar)
    {
        ar( CEREAL_NVP(m_startColor),
            CEREAL_NVP(m_velocity),
            CEREAL_NVP(m_emissionRate),
            CEREAL_NVP(m_particleLifetime),
            CEREAL_NVP(m_lifetime),
            CEREAL_NVP(m_coneAngle),
            CEREAL_NVP(m_minScale),
            CEREAL_NVP(m_maxScale),
            CEREAL_NVP(m_sizeMultiplier),
            CEREAL_NVP(m_scaleDown),
            CEREAL_NVP(m_useConstantVelocity),
            CEREAL_NVP(m_useConstantEmissionRate),
            CEREAL_NVP(m_useConstantParticleScale),
            CEREAL_NVP(m_useConstantParticleLifetime),
            CEREAL_NVP(m_poolMax));
    }

private:   
    friend class ParticleSystem;
    friend struct ParticleComponent;
    friend struct EmitterComponent;

    void ConstructComponents(Transform& emitterTransform);

    static glm::vec3 RandomVelocity(EmitterComponent& emitter, glm::vec2 bounds);

    glm::vec4 m_startColor = glm::vec4(1.f);
    glm::vec2 m_velocity = {1.f, 2.f};  // x = minimum velocity, y = maximum velocity
    glm::vec2 m_emissionRate = glm::vec2(1.f);
    glm::vec2 m_particleLifetime = glm::vec2(0.8f);
    float m_lifetime = 0.f;  // 0.f == Infinite duration
    float m_coneAngle = 30.f;
    glm::vec2 m_minScale = glm::vec2(1.f, 1.f);
    glm::vec2 m_maxScale = glm::vec2(1.f, 1.f);
    float m_sizeMultiplier = 1.0f;
    bool m_scaleDown = false;

    bool m_useConstantVelocity = true;
    bool m_useConstantEmissionRate = true;
    bool m_useConstantParticleScale = true;
    bool m_useConstantParticleLifetime = true;

    int m_poolMax = 1000;
    
    // Do not serialize
    float m_timeElapsed = 0.f;
    float m_frames = 0.f;
    int m_pool = 0;
    bool m_shouldDie = false;

    bool m_play = true;
    bool m_checkDuration = false;
    float m_timeStamp = 0;
    float m_playDuration = 0;

    entt::entity m_handle = entt::null;
    //
};

struct EmittersManager
{
    void AddEmitterEntity(std::string name, Entity entity);
    Entity GetEmitterEntity(std::string name);    

    private:
    std::unordered_map<std::string,Entity> m_emitters;
};
