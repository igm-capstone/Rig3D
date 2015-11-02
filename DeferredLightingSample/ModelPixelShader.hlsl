struct Pixel
{
	float4 position		: SV_POSITION;
	float3 positionT	: POSITIONT;
	float3 normal		: NORMAL;
};

struct PS_OUT
{
	float4  position: SV_TARGET0;
	float4  depth	: SV_TARGET1;
	float4	color	: SV_TARGET2;
	float4	normal	: SV_TARGET3;
};

cbuffer Color : register(b0)
{
	float4 color;
}

PS_OUT main(Pixel pixel)
{
	PS_OUT output;
	output.position = float4(pixel.positionT, 1.0f);
	output.depth	= float4(pixel.position.z / pixel.position.w, 0.0f, 0.0f, 0.0);
	output.color	= color;
	output.normal	= normalize(float4(pixel.normal, 0.0f));
	return output;
}