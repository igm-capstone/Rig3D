struct Vertex
{
	float4 mTangent		: TANGENT;
	float3 mPosition	: POSITION;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
};

struct Pixel
{
	float4 position	: SV_POSITION;
	float4 color	: COLOR;
};

cbuffer transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

cbuffer Color : register(b1)
{
	float4 color;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(world, view), projection);

	Pixel pixel;
	pixel.position = mul(float4(vertex.mPosition, 1.0f), clip);
	pixel.color = color;

	return pixel;
}