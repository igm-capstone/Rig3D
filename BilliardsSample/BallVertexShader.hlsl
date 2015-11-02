struct Vertex
{
	float3		position	: POSITION;
	float3		normal		: NORMAL;
	float2		uv			: TEXCOORD;
	float4x4	world		: WORLD;
	uint		instanceID	: SV_InstanceID;
};

struct Pixel
{
	float4 position		: SV_POSITION;
	float3 normal		: NORMAL;
	float3 uv			: TEXCOORD;
};

cbuffer transform : register(b0)
{
	matrix view;
	matrix projection;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(vertex.world, view), projection);

	Pixel pixel;
	pixel.position = mul(float4(vertex.position, 1.0f), clip);
	pixel.normal = mul(float4(vertex.normal, 0.0f), vertex.world).xyz;
	pixel.uv = float3(vertex.uv, 0.0f + vertex.instanceID);
	return pixel;
}