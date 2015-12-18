struct Vertex
{
	float3		position		: POSITION;
	float3		normal			: NORMAL;
	float2		uv				: TEXCOORD;
	float4x4	world			: WORLD;
	uint		instanceID		: SV_InstanceID;
};

struct GVertex
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
	float2 uv			: TEXCOORD;
};

// Offset by half extents of terrain
// Normalize position with inverted Y
// Divide by number of patches per dimension

cbuffer transform : register(b0)
{
	matrix view;
	matrix projection;
}

cbuffer terrain : register (b1)
{
	float width;
	float depth;
	float widthPatchCount;
	float depthPatchCount;
}

Texture2D heightTexture : register(t0);
SamplerState samplerState: register(s0);

GVertex main(Vertex vertex)
{
	float3 position = float3(vertex.world[3][0], vertex.world[3][1], vertex.world[3][2]);
	position.x -= width;
	position.z -= depth;
	position = normalize(position);
	
	uint xid = vertex.instanceID / uint(widthPatchCount);
	uint zid = vertex.instanceID % uint(widthPatchCount);
	float2 id = float2(zid, xid);

	float x = (vertex.uv.x / widthPatchCount) + id.x * 1 / widthPatchCount;
	float z = (vertex.uv.y / depthPatchCount) + id.y * 1 / depthPatchCount;
	float2 uv = float2(x, z);

	float y = heightTexture.SampleLevel(samplerState, uv, 0).r * 10.0f;
	float4 vertexPosition = float4(vertex.position.x, y, vertex.position.z, 1.0f);

	matrix clip = mul(mul(vertex.world, view), projection);

	GVertex gVertex;
	gVertex.position = mul(vertexPosition, clip);

	float c = widthPatchCount * depthPatchCount;
	gVertex.positionT = mul(vertexPosition, vertex.world).xyz;
	gVertex.normal = mul(float4(vertex.normal, 0.0f), vertex.world).xyz;
	gVertex.uv = uv;

	return gVertex;
}