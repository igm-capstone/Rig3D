struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mPositionW	: POSITIONT;
	float3 mTangent		: TANGENT;
	float3 mBitangent	: BINORMAL;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
};

struct PS_OUT
{
	float4  position: SV_TARGET0;
	float4  depth	: SV_TARGET1;
	float4	color	: SV_TARGET2;
	float4	normal	: SV_TARGET3;
};

cbuffer Color : register(b0)
{
	float4 color;
}

PS_OUT main(Pixel pixel)
{
	PS_OUT output;
	output.position = float4(pixel.mPositionW, 1.0f);
	output.depth	= float4(pixel.mPositionH.z / pixel.mPositionH.w, 0.0f, 0.0f, 0.0);
	output.color	= color;
	output.normal	= normalize(float4(pixel.mNormal, 0.0f));
	return output;
}