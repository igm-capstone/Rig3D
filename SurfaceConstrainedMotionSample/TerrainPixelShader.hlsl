struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

// Offset by half extents of terrain
// Normalize position with inverted Y
// Divide by number of patches per dimension

float4 main(Pixel pixel) : SV_TARGET
{
	return float4(pixel.positionT, 1.0f);

	float4 ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
	float3 lightDirection = float3(-1.0f, -1.0f, 0.0f);
	float4 color = float4(0.0f, 0.6f, 0.2f, 1.0f);

	return ambient + (color * saturate(dot(normalize(pixel.normal), -normalize(lightDirection))));
}