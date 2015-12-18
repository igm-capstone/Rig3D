struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
	float2 uv			: TEXCOORD;
};

Texture2D normalTexture : register(t0);
SamplerState samplerState : register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	//pixel.normal = normalize(pixel.normal);


	float3x3 TBN = float3x3
	(
		float3(1.0f, 0.0f, 0.0f),
		float3(0.0f, 0.0f, 1.0f),
		float3(0.0f, 1.0f, 0.0f)
	);

	float4 ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
	float3 lightDirection = float3(1.0f, -1.0f, 1.0f);
	float3 normal = normalTexture.Sample(samplerState, pixel.uv).xyz;

	normal = normal * 2.0f - 1.0f;
	normal = mul(normal, TBN);
	//return float4(normal, 1.0f);

	float4 color = float4(0.0f, 0.6f, 0.2f, 1.0f);

	return ambient + (color * saturate(dot(normalize(normal), -normalize(lightDirection))));
}