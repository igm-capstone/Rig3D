struct Vertex
{
	float3 position		: POSITION;
	float3 normal		: NORMAL;
	float2 uv			: TEXCOORD;
};

struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

cbuffer transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(world, view), projection);
	float4 vertexPosition = float4(vertex.position, 1.0f);

	Pixel pixel;
	pixel.position = mul(vertexPosition, clip);
	pixel.positionT = mul(vertexPosition, world).xyz;
	pixel.normal = mul(vertex.normal, (float3x3)world);

	return pixel;
}