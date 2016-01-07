struct ParticleVertex
{
	float4 position		: POSITION;
	float4 color		: COLOR;
	float2 uv			: TEXCOORD0;
};

struct ParticlePixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
	float4 color		: COLOR;
};

cbuffer transform : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

ParticlePixel main(ParticleVertex vertex)
{
	ParticlePixel pixel;

	pixel.position = mul(vertex.position, world);
	pixel.position = mul(pixel.position, view);
	pixel.position = mul(pixel.position, projection);

	pixel.uv = vertex.uv;
	pixel.color = vertex.color;

	return pixel;
}