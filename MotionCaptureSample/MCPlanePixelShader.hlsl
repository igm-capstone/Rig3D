struct Pixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD;
};

Texture2D		checkerboard : register(t0);
SamplerState	samplerState : register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	return checkerboard.Sample(samplerState, pixel.uv);
}