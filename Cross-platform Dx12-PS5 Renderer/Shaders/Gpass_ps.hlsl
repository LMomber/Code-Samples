Texture2D<float4> albedo_texture : register(t0);
Texture2D<float4> metallic_roughness_texture : register(t1);
Texture2D<float4> emissive_texture : register(t2);
Texture2D<float4> occlusion_texture : register(t3);
Texture2D<float4> normal_texture : register(t4);
Texture2D<float4> shadow_depth_texture : register(t5);

SamplerState linearSampler : register(s0);

cbuffer material : register(b2)
{
    float4 Diffuse;
    float3 Emission;

    float Metallic;
    float Roughness;

    bool diffuseMap;
    bool normalMap;
    bool metallicMap;
    bool roughnessMap;
    bool emissionMap;
};

cbuffer shadowCBuffer : register(b3)
{
    matrix shadowVP;
    float3 Direction;
}

struct PS_Output
{
    float4 albedoRoughness : SV_Target0;
    float4 normalMetallic : SV_Target1;
    float4 emission : SV_Target2;
};

struct PS_Input
{
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
    float4 position  : SV_Position;
};

float3 UnpackNormal(const float3 n)
{
    return n * 2.0f - 1.0f;
}

float3 ApplyNormalMap(const float3x3 tbn, Texture2D map, const float2 uv)
{
    const float3 normalTs = UnpackNormal(normal_texture.Sample(linearSampler, uv).xyz);
    return normalize(mul(normalTs, tbn));
}

PS_Output main(PS_Input IN)
{
    PS_Output OUT;

    float3 normal = normalize(IN.normal);
    if (normalMap)
    {
        const float3x3 tbn = float3x3(normalize(IN.tangent),
		                              normalize(IN.bitangent),
		                              normal);
        normal = ApplyNormalMap(tbn, normal_texture, IN.uv);
    }
    normal = normal * 0.5f + 0.5f;

    float3 albedo = diffuseMap ? albedo_texture.Sample(linearSampler, IN.uv).rgb : 1;
    albedo *= Diffuse.rgb;

    float3 metallicRoughness = metallic_roughness_texture.Sample(linearSampler, IN.uv).rgb;
    float metallic = metallicMap ? metallicRoughness.b : 1;
    metallic *= Metallic;
    float roughness = roughnessMap ? metallicRoughness.g : 1;
    roughness *= Roughness;

    OUT.albedoRoughness = float4(albedo.rgb, roughness);
    OUT.normalMetallic = float4(normal.rgb, metallic);

    float3 emission = emissionMap ? emissive_texture.Sample(linearSampler, IN.uv).rgb : 1;
    emission *= Emission;
    OUT.emission = float4(emission, 1.0);

    return OUT;
}