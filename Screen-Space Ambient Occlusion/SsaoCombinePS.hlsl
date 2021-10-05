
cbuffer externalData : register(b0)
{
	int ssaoEnabled;
	float2 pixelSize;
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

	// Early out for no SSAO
	if (!ssaoEnabled) 
		return float4(sceneColors + ambient, 1);

	// Sample AO and combine
	float ao = SSAOBlur.Sample(BasicSampler, input.uv).r;
	return float4(ambient * ao + sceneColors, 1);
}