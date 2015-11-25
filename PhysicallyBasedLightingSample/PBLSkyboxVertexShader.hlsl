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
	float4 cameraPos;
}

Pixel main( Vertex vertex )
{
	float4x4 clip = mul(mul(world, view), projection);
	float4 vertexPosition = float4(vertex.position, 1.0f);
	float4 vertexPositionW = mul(vertexPosition, world);

	Pixel pixel;
	pixel.position = mul(vertexPosition, clip);
	pixel.texCoord = vertex.position;
	return pixel;
}