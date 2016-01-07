struct SamplePixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mColor		: COLOR;
};

float4 main(SamplePixel pixel) : SV_TARGET
{
	return float4(pixel.mColor, 1.0f);
}