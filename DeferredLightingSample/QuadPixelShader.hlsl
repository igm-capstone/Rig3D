#define MAX_LIGHTS 10

struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float2 mUV			: TEXCOORD;
};

struct PointLight 
{
	float4	color;
	float3	position;
	float	range;
};

cbuffer lights	: register(b0)
{
	PointLight		pointLights[MAX_LIGHTS];
	float4			ambientLight;
}

Texture2D positionMap	: register(t0);
Texture2D depthMap		: register(t1);
Texture2D diffuseMap	: register(t2);
Texture2D normalMap		: register(t3);

SamplerState samplerState : register(s0);


float4 CalculatePointLightColor(PointLight pointLight, float4 diffuseColor, float3 position, float3 normal, float depth)
{
	float3 lightDirection			= pointLight.position - position;
	float  magnitude				= length(lightDirection);
	lightDirection					/= magnitude;

	float3 lightAttenuation		= { 0.0f, 1.0f, 0.0f };
	float attenuation			= 1.0f / dot(lightAttenuation, float3(1.0f, magnitude, magnitude * magnitude));
	float nDotL					= saturate(dot(normal, lightDirection));
	return nDotL * diffuseColor * pointLight.color * attenuation;
}

float4 main(Pixel pixel) : SV_TARGET
{
	float3 position = positionMap.Sample(samplerState, pixel.mUV).xyz;
	float depth		= depthMap.Sample(samplerState, pixel.mUV).r;
	float4 color	= diffuseMap.Sample(samplerState, pixel.mUV);
	float3 normal = normalize(normalMap.Sample(samplerState, pixel.mUV).xyz);

	float4 diffuseColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	[unroll]
	for (int i = 0; i < MAX_LIGHTS; i++)
	{
		diffuseColor += CalculatePointLightColor(pointLights[i], color, position, normal, depth);
	}

	return ambientLight + diffuseColor;
}