struct Vertex
{
	float3 position	: POSITION;
	float3 normal	: NORMAL;
	float2 uv		: TEXCOORD;
};

struct Pixel
{
	float4 position			: SV_POSITION;
	float4 lightColor		: COLOR;
	float3 lightPosition	: POSITIONT;
	float2 uv				: TEXCOORD;
};

cbuffer transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

cbuffer traits : register(b1)
{
	float4 color;
	float3 position;
}

Pixel main(Vertex vertex)
{
	matrix clip = mul(mul(world, view), projection);

	Pixel pixel;
	pixel.position = mul(float4(vertex.position, 1.0f), clip);
	pixel.lightColor = color;
	pixel.lightPosition = position;
	pixel.uv = float2((pixel.position.x / pixel.position.w + 1.0f) * 0.5f, (1.0f - pixel.position.y / pixel.position.w) * 0.5f);

	return pixel;
}