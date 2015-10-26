struct Vertex
{
	float4 mTangent		: TANGENT;
	float3 mPosition	: POSITION;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
};

struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mTangent		: TANGENT;
	float3 mBitangent	: BINORMAL;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
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

	Pixel pixel;
	pixel.mPositionH	= mul(float4(vertex.mPosition, 1.0f), clip);
	pixel.mNormal		= mul(vertex.mNormal, (float3x3)world);
	pixel.mTangent		= vertex.mTangent.xyz;
	pixel.mBitangent	= cross(vertex.mNormal, vertex.mTangent.xyz) * vertex.mTangent.w;
	pixel.mUV			= vertex.mUV;

	return pixel;
}