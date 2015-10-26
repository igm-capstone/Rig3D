struct Pixel
{
	float4 mPositionH	: SV_POSITION;
	float2 mUV			: TEXCOORD;
};

cbuffer blurParameters : register(b0)
{
	float2 uvOffsets[12];	// 4 * 2 * 12 = 96
}

Texture2D sceneTexture		: register(t0);
Texture2D depthTexture		: register(t1);
SamplerState sceneSampler	: register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	float zOverW = depthTexture.Sample(sceneSampler, pixel.mUV).r;
	if (zOverW < 1.0f) {
		float weights[12] = { 0.000003, 0.000229, 0.005977, 0.060598, 0.24173, 0.382925, 0.382925, 0.24173,	0.060598, 0.005977,	0.000229, 0.000003 };

		float4 pixelColor = float4(0, 0, 0, 0);

		for (int i = 0; i < 12; i++) {
			pixelColor += sceneTexture.Sample(sceneSampler, pixel.mUV.xy + uvOffsets[i]) * weights[i];
		}

		return pixelColor;
	}
	else {
		return sceneTexture.Sample(sceneSampler, pixel.mUV.xy);
	}
	

}