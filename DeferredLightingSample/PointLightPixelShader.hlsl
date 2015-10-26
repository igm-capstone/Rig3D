struct Pixel
{
	float4 position	: SV_POSITION;
	float4 color	: COLOR;
};

float4 main(Pixel pixel) : SV_TARGET
{
	return pixel.color;
}