
struct SH
{
	float3 coefficients[9];
};

Texture2DArray textureCube : register(t0);

RWStructuredBuffer<SH> SHCoefficients : register (u0);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint width, height;
	textureCube.GetDimensions(width, height);

	SH sh = SHCoefficients[DTid.x];

	for (uint x = 0; x < width; x++)
	{
		for (uint y = 0; y < height; y++)
		{
			float4 texel = textureCube.Load(x, y, DTid);
			sh.coefficients[0] += 0.282095f;	// l = 0, m = 0
			sh.coefficients[2] += 0.488603f * 
		}
	}

}