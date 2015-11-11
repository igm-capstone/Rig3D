struct Vertex
{
	float3		position		: POSITION;
	float3		normal			: NORMAL;
	float2		uv				: TEXCOORD;
	float4x4	world			: WORLD;
	uint		instanceID		: SV_InstanceID;
};

struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

cbuffer transform : register(b0)
{
	matrix view;
	matrix projection;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(vertex.world, view), projection);
	float4 vertexPosition = float4(vertex.position, 1.0f);

	Pixel pixel;
	pixel.position = mul(vertexPosition, clip);
	pixel.positionT = mul(vertexPosition, vertex.world).xyz;
	pixel.normal = mul(float4(vertex.normal, 0.0f), vertex.world).xyz;

	return pixel;
}