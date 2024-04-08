
struct VertexToPixel
{
	float4 screenPosition		: SV_POSITION;
	float2 uv           : TEXCOORD0;
	float3 normal		: NORMAL;
};

struct PS_Output
{
	float4 colorNoAmbient	: SV_TARGET0;
	float4 ambientColor		: SV_TARGET1;
	float4 normals			: SV_TARGET2;
	float depths			: SV_TARGET3;
};

Texture2D Pixels			: register(t0);
SamplerState BasicSampler	: register(s0);


PS_Output main(VertexToPixel input)
{
	// Multiple render target output
	PS_Output output;
	output.colorNoAmbient = Pixels.Sample(BasicSampler, input.uv);
	output.ambientColor = float4(0, 0, 0, 1);
	output.normals = float4(input.normal * 0.5f + 0.5f, 1);
	output.depths = input.screenPosition.z;
	return output;
}