struct Vertex
{
	float3 mPosition	: POSITION;
	float2 mUV			: TEXCOORD;
};

struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float2 mUV			: TEXCOORD;
};

Pixel main(Vertex vertex)
{
	Pixel pixel;
	pixel.mPositionH = float4(vertex.mPosition, 1.0f);
	pixel.mUV = vertex.mUV;

	return pixel;
}