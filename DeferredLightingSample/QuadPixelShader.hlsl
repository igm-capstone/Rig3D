struct Pixel
{
	float4 position	: SV_POSITION;
	float2 uv		: TEXCOORD;
};

cbuffer ambientLight	: register(b0)
{
	float4	ambientLightColor;
}

Texture2D diffuseMap	: register(t0);
Texture2D lightMap		: register(t1);
Texture2D depthMap		: register(t2);

SamplerState samplerState : register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float4 albedo = lightMap.Sample(samplerState, pixel.uv);
	float4 diffuse = diffuseMap.Sample(samplerState, pixel.uv);
	int depth = (int)(depthMap.Sample(samplerState, pixel.uv).r);
	float4 clearColor = { 0.2, 0.5f, 0.6f, 1.0f };
	return (clearColor * depth) + (ambientLightColor * !depth) + (albedo * diffuse);
}