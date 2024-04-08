

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
	float4 colorTint	: COLOR;
};

// Textures and such
Texture2D Particle			: register(t0);
SamplerState BasicSampler	: register(s0);

// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	// Return the texture sample
	return Particle.Sample(BasicSampler, input.uv) * input.colorTint;
}