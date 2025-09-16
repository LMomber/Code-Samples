#pragma once

#include "core/transform.hpp"
#include "platform/opengl/render_components_gl.hpp"

#include <glm/glm.hpp>
#include "entt/entt.hpp"

struct ParticleBillboard;
struct EmitterComponent;
struct TrailComponent;
struct ParticleComponent
{
    ParticleComponent(EmitterComponent& emitter,
                      glm::vec3 pos,
                      glm::vec4 color,
                      glm::vec3 vel,
                      glm::vec2 scale,
                      float lifetime,
                      entt::entity handle);

    glm::vec3 m_velocity = {0, 0, 0};
    float m_maxLife = 0.f;
    glm::vec4 m_color;
    glm::vec2 m_initialScale;
    float m_lifetime = 0.f;

    entt::entity m_handle;
    entt::entity m_parent;

    bool m_scaleDown = false;

    ParticleBillboard* m_billboard = nullptr;

    bool IsAlive() const { return m_lifetime < m_maxLife; }
};

struct MassComponent
{
    MassComponent() {};
    MassComponent(float mass) : m_mass(mass) {}

    float m_mass = 0.f;
};

struct EndColorComponent
{
    EndColorComponent() {};
    EndColorComponent(glm::vec4 color) : m_color(color) {}

    glm::vec4 m_color = glm::vec4(0);
};

// A particle trail consists of AmountOfSegments * ParticlesPerSegment particles.
// The interval at which a segment is added depends on the trail's max lifetime.
// Interval = maxLifetime / amountOfSegments.
struct TrailComponent
{
    TrailComponent() {};
    TrailComponent(unsigned int intervalCount, unsigned int particlesPerSegment, float maxLife, bool fadeIn, bool scaleDown)
        : m_segmentCount(intervalCount),
          m_particlesPerSegment(particlesPerSegment),
          m_maxLife(maxLife),
          m_fadeIn(fadeIn),
          m_scaleDown(scaleDown) {};

    void AddSegment();

    unsigned int m_totalAmountOfSegments = 0;
    unsigned int m_segmentCount = 2;
    unsigned int m_particlesPerSegment = 4;

    float m_maxLife = 0.1f;
    float m_lifetime = 0.f;
    float m_emitterEmissionRate = 1.f;
    bool m_fadeIn = false;
    bool m_scaleDown = false;

    // Leading particle's data; Updated every frame
    Transform m_particleTransform;
    glm::vec4 m_particleColor;
    glm::vec2 m_particleScale;

    struct Segment
    {
        Transform m_world;  // 64
        glm::vec4 m_color;       // 16
        glm::vec2 m_size;        // 8
        float m_startTimeStamp;  // 4

        bool IsAlive(float trailMaxLife, float currentLifetime) const
        {
            return currentLifetime < (m_startTimeStamp + trailMaxLife);
        }

        Segment(Transform world, glm::vec4 color, glm::vec2 size, float startTimeStamp)
            : m_world(world), m_color(color), m_size(size), m_startTimeStamp(startTimeStamp){};
    };

    std::vector<Segment> m_segments;
};

struct EndLocationComponent
{
    EndLocationComponent() {};
    EndLocationComponent(glm::vec3 endPos) : m_endPos(endPos) {};
    EndLocationComponent(glm::vec3 startPos, glm::vec3 endPos) : m_startPos(startPos), m_endPos(endPos) {};

    glm::vec3 m_startPos{};
    glm::vec3 m_endPos{};
};

struct ValueNoiseComponent 
{
    ValueNoiseComponent() {};
    ValueNoiseComponent(int octaves, float lacunarity, float gain, float amplitude, float frequency, float scale)
    {
        m_parameters.amplitude = amplitude;
        m_parameters.frequency = frequency;
        m_parameters.gain = gain;
        m_parameters.lacunarity = lacunarity;
        m_parameters.octaves = octaves;
        m_parameters.scale = scale;
    };

    noise_parameters m_parameters;
};

struct DiscComponent
{
    DiscComponent() {};
    DiscComponent(float radius) : m_radius(radius) {};

    float m_radius = 1.f;
};

struct BloomComponent
{
    BloomComponent() {};
    BloomComponent(float strength) : m_strength(strength) {};

    float m_strength = 0.f;
};