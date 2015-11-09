struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

float4 main(Pixel pixel) : SV_TARGET
{
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}