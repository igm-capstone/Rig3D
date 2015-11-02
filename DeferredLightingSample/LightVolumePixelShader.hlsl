struct Pixel
{
	float4 position			: SV_POSITION;
	float4 lightColor		: COLOR;
	float3 lightPosition	: POSITIONT;
	float2 uv				: TEXCOORD;
};

Texture2D positionMap	: register(t0);
Texture2D normalMap		: register(t1);

SamplerState samplerState : register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float3 position = positionMap.Sample(samplerState, pixel.uv).xyz;
	float3 normal	= normalize(normalMap.Sample(samplerState, pixel.uv).xyz);
	float3 lightDirection = pixel.lightPosition - position;
	float  magnitude = length(lightDirection);
	lightDirection /= magnitude;
	
	float3 lightAttenuation = { 0.0f, 1.0f, 1.0f };
	float attenuation = saturate(1.0f - magnitude / 2.45f); // 0.1f / dot(lightAttenuation, float3(1.0f, magnitude, magnitude * magnitude));
	float nDotL = saturate(dot(normal, lightDirection));
	return nDotL * pixel.lightColor * attenuation;
}