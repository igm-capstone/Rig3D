struct Vertex
{
	float3 position	: POSITION;
	float2 uv		: TEXCOORD;
};

struct Pixel
{
	float4 position	: SV_POSITION;
	float2 uv		: TEXCOORD;
};

Pixel main(Vertex vertex)
{
	Pixel pixel;
	pixel.position = float4(vertex.position, 1.0f);
	pixel.uv = vertex.uv;

	return pixel;
}