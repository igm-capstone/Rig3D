
static const float PI = 3.14159265f;

struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
	float2 material		: TEXCOORD;	// x - gloss, y - metal
};

TextureCube textureCube : register(t0);
SamplerState samplerState : register(s0);

cbuffer BRDF_Inputs : register(b0)
{
	float3	L;	// Light Direction
	float3	P;	// Camera Position
}

float sDot(float3 A, float3 B)
{
	return saturate(dot(A, B));
}

float SphericalGaussian(float3 V, float3 H)
{
	float VoH = sDot(V, H);
	float c0 = -5.55473f;
	float c1 = 6.98316f;
	return ((c0 * VoH) - c1) * VoH;
}

float4 D(float3 H, float3 N, float alpha)
{
	//			     alpha^2
	//------------------------------------
	// PI((N o H)^2 * (alpha^2 - 1) + 1)^2

	float alphaSquared = alpha * alpha;
	
	float NoH = sDot(N, H);
	float inner = ((NoH * NoH) * (alphaSquared - 1.0f) + 1.0f);
	return (alphaSquared) / (PI * (inner * inner));
}

float F(float3 V, float3 H, float F0)
{
	float exponent = SphericalGaussian(V, H);
	return F0 + (1.0f - F0) * pow(2.0f, exponent);
}

float G1(float3 N, float3 V, float k)
{
	float NoV = sDot(N, V);
	return NoV / (NoV * (1.0f - k) + k);
}

float4 G(float3 L, float3 V, float3 N, float alpha)
{
	float alphaRemap = (alpha + 1.0f) * 0.5f;
	float k = ((alphaRemap + 1.0f) * (alphaRemap + 1.0f)) / 8.0f;
	return G1(N, L, k) * G1(N, V, k);
}

float4 main(Pixel pixel) : SV_TARGET
{
	float3 V = normalize(P - pixel.positionT);
	float3 N = normalize(pixel.normal);
	float3 H = normalize((-L + V) * 0.5f);
	float NoL = dot(N, -L);
	float NoV = dot(N, V);

	// Direct Diffuse = N dot L / PI
	float4 diffuse = saturate(NoL) / PI;

	// Direct Specular = Cook Torrance Microfacet 
	// - Roughness
	// - Metalness

	return diffuse + ((D(H, N, pixel.material.x) * F(V, H, pixel.material.y) * G(-L, V, N, pixel.material.x)) / 4.0f;

	// Indirect Diffuse = Spherical Harmonics  = Another Cubemap
		// 


	// Indirect Specular = Create cubemap
		// - Environment BRDF
		// - Roughness
		// - Metalness
}