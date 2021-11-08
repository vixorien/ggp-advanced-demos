
cbuffer externalData : register(b0)
{
	int ssaoEnabled;
	int ssaoOutputOnly;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};


Texture2D SceneColorsNoAmbient	: register(t0);
Texture2D Ambient				: register(t1);
Texture2D SSAOBlur				: register(t2);
SamplerState BasicSampler		: register(s0);


float4 main(VertexToPixel input) : SV_TARGET
{
	// Sample all three
	float3 sceneColors = SceneColorsNoAmbient.Sample(BasicSampler, input.uv).rgb;
	float3 ambient = Ambient.Sample(BasicSampler, input.uv).rgb;
	float ao = SSAOBlur.Sample(BasicSampler, input.uv).r;

	// Early out for no SSAO
	if (!ssaoEnabled)
		ao = 1.0f;

	// Early out for SSAO only
	if (ssaoOutputOnly)
		return float4(ao.rrr, 1);

	// Final combine
	return float4(pow(ambient * ao + sceneColors, 1.0f / 2.2f), 1);
}