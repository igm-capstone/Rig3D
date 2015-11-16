struct Vertex
{
	float3		position		: POSITION;
	float2		uv				: TEXCOORD;
};

struct Pixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
};

cbuffer transform : register(b0)
{
	matrix view;
	matrix projection;
}

cbuffer transform : register(b1)
{
	matrix world;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(world, view), projection);

	Pixel pixel;
	pixel.position = mul(float4(vertex.position, 1.0f), clip);
	pixel.uv = vertex.uv;

	return pixel;
}