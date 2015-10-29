struct ParticlePixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
	float4 color		: COLOR;
};

Texture2D particleTexture	: register(t0);
SamplerState samplerState	: register(s0);

float4 main(ParticlePixel pixel) : SV_TARGET
{
	float4 textureColor;

	textureColor = particleTexture.Sample(samplerState, pixel.uv);

	return textureColor *pixel.color;
}