

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D Albedo		: register(t0);
Texture2D LightBuffer	: register(t1);
SamplerState Sampler	: register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	float3 pixelIndex = float3(input.position.xy, 0);
	float3 totalColor = /*Albedo.Load(pixelIndex).rgb **/ LightBuffer.Load(pixelIndex).rgb;
	return float4(totalColor, 1);
}