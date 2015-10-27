struct SampleVertex
{
	float3 mPosition	: POSITION;
	float3 mColor		: COLOR;
};

struct SamplePixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mColor		: COLOR;
};

cbuffer transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

SamplePixel main(SampleVertex vertex)
{
	matrix clip = mul(mul(world, view), projection);

	SamplePixel pixel;
	pixel.mPositionH = mul(float4(vertex.mPosition, 1.0f), clip);
	pixel.mColor = vertex.mColor;

	return pixel;
}