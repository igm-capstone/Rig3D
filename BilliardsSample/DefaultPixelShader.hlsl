struct Pixel
{
	float4 position		: SV_POSITION;
	float3 normal		: NORMAL;
	float2 uv			: TEXCOORD;
};

Texture2D diffuseTexture	: register(t0);
SamplerState samplerState	: register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float3 lightDirection = { 0.0f, -1.0f, 0.0 };
	float3 normal = normalize(pixel.normal);
	float4 color = diffuseTexture.Sample(samplerState, pixel.uv);
	return color * saturate(dot(-lightDirection, normal));
}