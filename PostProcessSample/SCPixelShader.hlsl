struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mTangent		: TANGENT;
	float3 mBitangent	: BINORMAL;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
};

cbuffer color : register(b0)
{
	float4 color;
}

float4 main(Pixel pixel) : SV_TARGET
{
	return color;
}