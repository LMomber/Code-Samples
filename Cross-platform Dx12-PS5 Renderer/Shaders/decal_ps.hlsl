Texture2D<float> depthTexture  : register(t0);
Texture2D<float4> albedoTexture : register(t1);
Texture2D<float4> normalTexture : register(t2);
Texture2D<float4> normalBuffer : register(t3);

cbuffer cameraPS : register(b2)
{
    matrix VP;
    matrix V;
    matrix invV;
    matrix invP;
    float3 cameraPos;
};

cbuffer decalPS : register(b3)
{
    matrix M;
    matrix InvM;
    int  albedoMap;
    int  normalMap;
    int useAlbedoStrength;
    float albedoStrength;
    float normalStrength;
};

SamplerState pointSampler  : register(s0);
SamplerState linearSampler : register(s1);

struct PS_Input
{
    float4 position : SV_Position;
};

struct PS_Output
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
};

float3 ScreenSpaceUVToNDC(float2 uv, float zNDC)
{
    float3 positionNDC;
    positionNDC.xy = (uv * 2) - 1;
    positionNDC.y *= -1;
    positionNDC.z = zNDC;
    return positionNDC;
}

float4 RestorePositionVS(float3 positionNDC, matrix inverseProjection)
{
    float4 positionVS = mul(inverseProjection, float4(positionNDC, 1.0));
    positionVS /= positionVS.w;
    return positionVS;
}

float3 RestorePositionWS(float3 positionNDC, matrix inverseProjection, matrix inverseView)
{
    float4 positionVS = RestorePositionVS(positionNDC, inverseProjection);
    return mul(inverseView, positionVS).xyz;
}

float3 ExtractScale(float4x4 mat)
{
    float3 scale;
    scale.x = length(mat[0].xyz);
    scale.y = length(mat[1].xyz);
    scale.z = length(mat[2].xyz);
    return scale;
}

float3x3 cotangent_frame( float3 N, float3 p, float2 uv )
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx( p );
    float3 dp2 = ddy( p );
    float2 duv1 = ddx( uv );
    float2 duv2 = ddy( uv );
 
    // solve the linear system
    float3 dp2perp = cross( dp2, N );
    float3 dp1perp = cross( N, dp1 );
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
    return float3x3( T * invmax, B * invmax, N );
}

PS_Output main(PS_Input IN)
{
    PS_Output OUT;

    const float2 screenSpaceCoord = float2(IN.position.x, IN.position.y);

    uint2 textureDimensions;
    depthTexture.GetDimensions(textureDimensions.x, textureDimensions.y);

    const float2 ndcCoord = (screenSpaceCoord / textureDimensions) * 2.0 - 1.0;
    const float2 uvCoord = (ndcCoord + 1.0) * 0.5;

    const float depth = depthTexture.Sample(pointSampler, uvCoord);

    clip(depth - IN.position.z);

    const float3 positionNDC = ScreenSpaceUVToNDC(uvCoord, depth);
    const float3 positionWS = RestorePositionWS(positionNDC, invP, invV);

    // Bring it to the object position of the decal
    const float4 objectPosition = mul(InvM, float4(positionWS, 1));

    // Check if it's inside the bounds of the decal box
    clip(ExtractScale(M) - abs(objectPosition.xyz));
    
    float2 decalTexCoord = (objectPosition.xy * 0.5) + 0.5;
    decalTexCoord.y *= -1;

    OUT.albedo = albedoMap != 0 ? albedoTexture.Sample(linearSampler, decalTexCoord) : float4(0,0,0,0);
    OUT.albedo.a = useAlbedoStrength ? albedoStrength : OUT.albedo.a;

    // Sample the tangent space normal and remap to [-1,1]
    float3 sampledNormalTS = normalTexture.Sample(linearSampler, decalTexCoord).xyz * 2.0 - 1.0;

    // Compute world space normal from the normal buffer and map to [-1, 1]
    float3 N_world = normalize(normalBuffer.Sample(linearSampler, uvCoord).xyz * 2.0 - 1.0);

    N_world.y *= -1;

    // Compute view vector in world space
    float3 V_WS = cameraPos - positionWS;

    // Compute the TBN matrix using the cotangent_frame function
    float3x3 TBN = cotangent_frame(N_world, V_WS, decalTexCoord);

    OUT.normal.xyz = (normalize(mul(TBN, sampledNormalTS)) + 1) * 0.5f;
    OUT.normal.a = 0;

    return OUT;
}