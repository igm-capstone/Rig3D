struct Pixel
{
	float4 position	: SV_POSITION;
	float3 normal	: NORMAL;
	float3 uv		: TEXCOORD;
};

Texture2DArray diffuseTextureArray : register(t0);
SamplerState samplerState : register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float3 lightDirection = {0.0f, -1.0f, 0.0};
	float3 normal = normalize(pixel.normal);
	float4 color = diffuseTextureArray.Sample(samplerState, pixel.uv, 0);
	return color * saturate(dot(-lightDirection, normal));
}