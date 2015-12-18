struct GVertex
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
	float2 uv			: TEXCOORD;
};

[maxvertexcount(3)]
void main(
	triangle GVertex input[3] : SV_POSITION, 
	inout TriangleStream< GVertex > output
)
{
	input[0].normal = normalize(cross(input[1].positionT.xyz - input[0].positionT.xyz, input[2].positionT.xyz - input[0].positionT.xyz));
	input[1].normal = normalize(cross(input[2].positionT.xyz - input[1].positionT.xyz, input[0].positionT.xyz - input[1].positionT.xyz));
	input[2].normal = normalize(cross(input[0].positionT.xyz - input[2].positionT.xyz, input[1].positionT.xyz - input[2].positionT.xyz));

	for (uint i = 0; i < 3; i++)
	{
		output.Append(input[i]);
	}

	output.RestartStrip();
}