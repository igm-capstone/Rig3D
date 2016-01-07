static const float PI = 3.1415926535f;

struct SH
{
	float4 coefficients[9];
};

Texture2DArray textureCube : register(t0);
SamplerState samplerState : register(s0);

RWStructuredBuffer<SH> SHCoefficients : register (u0);

void sh_eval_basis_2(float3 dir, out float coefficients[9])
{

	float x = dir.x;
	float y = dir.y;
	float z = dir.z;

	float z2 = z*z;

	/* m=0 */

	// l=0
	float p_0_0 = 0.282094791773878140;
	coefficients[0] = p_0_0; // l=0,m=0
				  // l=1
	float p_1_0 = 0.488602511902919920*z;
	coefficients[2] = p_1_0; // l=1,m=0
				  // l=2
	float p_2_0 = 0.946174695757560080*z2 + -0.315391565252520050;
	coefficients[6] = p_2_0; // l=2,m=0


				  /* m=1 */

	float s1 = y;
	float c1 = x;

	// l=1
	float p_1_1 = -0.488602511902919920;
	coefficients[1] = p_1_1*s1; // l=1,m=-1
	coefficients[3] = p_1_1*c1; // l=1,m=+1
					 // l=2
	float p_2_1 = -1.092548430592079200*z;
	coefficients[5] = p_2_1*s1; // l=2,m=-1
	coefficients[7] = p_2_1*c1; // l=2,m=+1


					 /* m=2 */

	float s2 = x*s1 + y*c1;
	float c2 = x*c1 - y*s1;

	// l=2
	float p_2_2 = 0.546274215296039590;
	coefficients[4] = p_2_2*s2; // l=2,m=-2
	coefficients[8] = p_2_2*c2; // l=2,m=+2
}

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	uint width, height, elements;
	textureCube.GetDimensions(width, height, elements);

	SH sh4 = SHCoefficients[DTid.x];

	float size = float(width);
	float pixelSize = 1.0f / size;

	float B = -1.0f + 1.0f / size;
	float S = 2.0f * (1.0f - 1.0f / size) / (size - 1.0f);

	float w = 0.0f;

	float sh[9];

	[loop]
	for (uint x = 0; x < width; x++)
	{
		[loop]
		for (uint y = 0; y < height; y++)
		{
			float v = y * S + B;


		//	float4 texel = textureCube.Load(x, y, DTid.x, 0);
			float4 texel = textureCube.SampleLevel(samplerState, float3(x, y, DTid.x), 0);

			float u = x * S + B;

			float3 direction = float3(0.0f, 0.0f, 0.0f);
			[call]
			switch (DTid.x)
			{
				case 0:	// +x
				{
					direction.z = 1.0f - (2.0f * float(x) + 1.0f) * pixelSize;
					direction.y = 1.0f - (2.0f * float(y) + 1.0f) * pixelSize;
					direction.x = 1.0f;
					break;
				}
				case 1:	// -x
				{
					direction.z = -1.0f + (2.0f * float(x) + 1.0f) * pixelSize;
					direction.y =  1.0f - (2.0f * float(y) + 1.0f) * pixelSize;
					direction.x = -1.0f;
					break;
				}
				case 2: // +y
				{
					direction.z = -1.0f + (2.0f * float(y) + 1.0f) * pixelSize;
					direction.y =  1.0f;
					direction.x = -1.0f + (2.0f * float(x) + 1.0f) * pixelSize;
					break;
				}
				case 3:	// -y
				{
					direction.z =  1.0f - (2.0f * float(y) + 1.0f) * pixelSize;
					direction.y = -1.0f;
					direction.x = -1.0f + (2.0f * float(x) + 1.0f) * pixelSize;
					break;
				}
				case 4: // +z
				{
					direction.z =  1.0f;
					direction.y =  1.0f - (2.0f * float(y) + 1.0f) * pixelSize;
					direction.x = -1.0f + (2.0f * float(x) + 1.0f) * pixelSize;
					break;
				}
				case 5:	// -z
				{
					direction.z = -1.0f;
					direction.y =  1.0f - (2.0f * float(y) + 1.0f) * pixelSize;
					direction.x =  1.0f - (2.0f * float(x) + 1.0f) * pixelSize;
					break;
				}
				default:
					direction.z = direction.y = direction.x = 0.0f;
					break;
			}

			direction = normalize(direction);

			sh_eval_basis_2(direction, sh);

			float diffSolidAngle = 4.0f / ((1.0f + u * u + v * v) * sqrt(1.0f + u * u + v * v));
			w += diffSolidAngle;

			float4 t[9];

			for (int i = 0; i < 9; i++)
			{
				t[i].x = sh[i] * texel.x * diffSolidAngle;
				t[i].y = sh[i] * texel.y * diffSolidAngle;
				t[i].z = sh[i] * texel.z * diffSolidAngle;
				sh4.coefficients[i] += t[i];
			}
		}
	}

	float normProj = (4.0f * PI) / w;

	for (int i = 0; i < 9; i++)
	{
		sh4.coefficients[i] *= normProj;
	}

	SHCoefficients[DTid.x] = sh4;
}