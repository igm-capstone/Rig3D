struct Vertex
{
	float3 position : POSITION;
	float3 normal	: NORMAL;
	float2 uv		: TEXCOORD;
};

struct Pixel
{
	float4 position		: SV_POSITION;
	float3 texCoord		: TEXCOORD;
};

cbuffer Transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

Pixel main( Vertex vertex )
{
	float4x4 clip = mul(mul(world, view), projection);

	Pixel pixel;
	pixel.position = mul(float4(vertex.position, 1.0f), clip);
	pixel.position.z = pixel.position.w;
	pixel.texCoord = vertex.position;
	return pixel;
}