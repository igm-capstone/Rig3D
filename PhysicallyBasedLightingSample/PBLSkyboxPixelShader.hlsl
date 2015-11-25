struct Pixel
{
	float4 position		: SV_POSITION;
	float3 texCoord		: TEXCOORD;
};

TextureCube		textureCube		: register(t0);
SamplerState	samplerState	: register(s0);

float4 main(Pixel pixel) : SV_TARGET
{
	return textureCube.Sample(samplerState, normalize(pixel.texCoord));
}