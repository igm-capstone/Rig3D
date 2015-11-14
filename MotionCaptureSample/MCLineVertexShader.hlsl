struct Vertex
{
	float3 position : POSITION;
};

cbuffer transform : register(b0)
{
	matrix view;
	matrix projection;
}

float4 main( Vertex vertex ) : SV_POSITION
{
	matrix clip = mul(view, projection);

	return mul(float4(vertex.position, 1.0f), clip);
}