struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float3 mTangent		: TANGENT;
	float3 mBitangent	: BINORMAL;
	float3 mNormal		: NORMAL;
	float2 mUV			: TEXCOORD;
};

Texture2D texture2d			: register(t0);
SamplerState samplerState	: register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float3 lightDirection = normalize(float3(1.0f, -1.0f, 1.0f));
	float nDotL = dot(normalize(pixel.mNormal), -lightDirection);
	float4 color = texture2d.Sample(samplerState, pixel.mUV.xy);
	return color * nDotL;
}