
#include "ShaderStructs.hlsli"

// Texture-related resources
TextureCube SkyTexture		: register(t0);
SamplerState BasicSampler	: register(s0);

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel_Sky input) : SV_TARGET
{
	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	return SkyTexture.Sample(BasicSampler, input.sampleDir);
}