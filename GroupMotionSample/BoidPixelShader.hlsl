struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

cbuffer color : register(b0)
{
	float4 color;
}

float4 main(Pixel pixel) : SV_TARGET
{
	float4 ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
	float3 lightDirection = float3(0.0f, -1.0f, 1.0f);

	return ambient + (color * saturate(dot(normalize(pixel.normal), -normalize(lightDirection))));
}