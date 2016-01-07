struct RigidBody
{
	float3 position;
	float3 velocity;
	float3 forces;
	float inverseMass;
};

struct Plane
{
	float3 normal;
	float  distance;
};

Texture2D heightTexture : register(t0);
Texture2D normalTexture : register(t1);
SamplerState samplerState : register(s0);

RWStructuredBuffer<RigidBody> rigidBodies : register(u0);

cbuffer terrain : register(b0)
{
	float width;
	float depth;
	float widthPatchCount;
	float depthPatchCount;
};

float CalculatePlaneSphereImpulse(RigidBody rigidBody, Plane plane)
{
	float3 vRel = rigidBody.velocity;
	float numerator = (1.0f + 1.0f) * dot(-vRel, plane.normal);
	float denominator = dot((rigidBody.inverseMass) * plane.normal, plane.normal);
	return numerator / denominator;
}

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	RigidBody rb = rigidBodies[(DTid.x * 1) + DTid.y];
	float u = (rb.position.x + (width * 0.5f)) / width;
	float v = (rb.position.z + (depth * 0.5f)) / depth;

	float scale = 1.0f;
	float radius = 0.5f;

	float2 uv = float2(u, 1.0f - v);
	float3 normal = normalTexture.SampleLevel(samplerState, uv, 0).xyz;
	float height = heightTexture.SampleLevel(samplerState, uv, 0).r * scale;
	float3 planePos = float3(rb.position.x, height, rb.position.z);



	float3 p0 = rb.position + float3(radius, 0.0f, radius);			// +xz
	float3 p1 = rb.position + float3(-radius, 0.0f, -radius);		// -xz
	float3 p2 = rb.position + float3(-radius, 0.0f, radius);		// -x+z
	float3 p3 = rb.position + float3(radius, 0.0f, -radius);		// +x-z

	float2 uv0 = float2((p0.x + (width * 0.5f)) / width, 1.0f - ((p0.z + (depth * 0.5f)) / depth));
	float2 uv1 = float2((p1.x + (width * 0.5f)) / width, 1.0f - ((p1.z + (depth * 0.5f)) / depth));
	float2 uv2 = float2((p2.x + (width * 0.5f)) / width, 1.0f - ((p2.z + (depth * 0.5f)) / depth));
	float2 uv3 = float2((p3.x + (width * 0.5f)) / width, 1.0f - ((p3.z + (depth * 0.5f)) / depth));

	p0.y = heightTexture.SampleLevel(samplerState, uv0, 0).r * scale;
	p1.y = heightTexture.SampleLevel(samplerState, uv1, 0).r * scale;
	p2.y = heightTexture.SampleLevel(samplerState, uv2, 0).r * scale;
	p3.y = heightTexture.SampleLevel(samplerState, uv3, 0).r * scale;

	float3 n0 = cross(p1 - p0, p2 - p0);
	float3 n1 = cross(p3 - p0, p2 - p0);
	
	//float3x3 TBN = float3x3
	//	(
	//		float3(1.0f, 0.0f, 0.0f),
	//		float3(0.0f, 0.0f, 1.0f),
	//		float3(0.0f, 1.0f, 0.0f)
	//	);

	//normal = normal * 2.0f - 1.0f;
	//normal = mul(normal, TBN);

	Plane plane;
	plane.normal = normalize((n0 + n1) * 0.5f);
	//plane.normal = float3(0.0f, 1.0f, 0.0f);// normalize(normal);
	//plane.normal =  normalize(normal);
	plane.distance = dot(planePos, plane.normal);

	float d = dot(plane.normal, rb.position) - plane.distance;

	// Test for plane negative half space
	if (d <= radius)
	{
		rb.position += plane.normal * (radius - d);

	//	if (dot(rb.velocity, plane.normal) == 0.0f)

		float3 poi = rb.position - (plane.normal * d);
		if (length(poi - planePos) < radius)
		{
			float k = CalculatePlaneSphereImpulse(rb, plane);
			rb.velocity += k * plane.normal * rb.inverseMass;
		}
		
	}

	rigidBodies[(DTid.x * 1) + DTid.y] = rb;
}