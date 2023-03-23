
cbuffer ExternalData : register(b0)
{
	float chromaticAberrationOffset;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D Pixels			: register(t0);
SamplerState BasicSampler	: register(s0);


float4 main(VertexToPixel input) : SV_TARGET
{
	float r = Pixels.Sample(BasicSampler, input.uv + float2(chromaticAberrationOffset, 0)).r;
	float g = Pixels.Sample(BasicSampler, input.uv).g;
	float b = Pixels.Sample(BasicSampler, input.uv + float2(0, chromaticAberrationOffset)).b;

	return float4(r, g, b, 1);
}