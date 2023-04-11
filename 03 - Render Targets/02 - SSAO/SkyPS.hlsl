

// Texture-related resources
TextureCube skyTexture		: register(t0);
SamplerState samplerOptions : register(s0);


// Struct representing the data we expect to receive from earlier pipeline stages
// - Should match the output of our corresponding vertex shader
// - The name of the struct itself is unimportant
// - The variable names don't have to match other shaders (just the semantics)
// - Each variable must have a semantic, which defines its usage
struct VertexToPixel
{
	float4 position		: SV_POSITION;	// XYZW position (System Value Position)
	float3 sampleDir	: DIRECTION;
};


// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// 
// - Input is the data coming down the pipeline (defined by the struct)
// - Output is a single color (float4)
// - Has a special semantic (SV_TARGET), which means 
//    "put the output of this into the current render target"
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	return skyTexture.Sample(samplerOptions, input.sampleDir);
}