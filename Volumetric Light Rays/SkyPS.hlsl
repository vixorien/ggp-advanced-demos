
cbuffer externalData : register(b0)
{
	float3 sunDirection;
	float falloffExponent;
	float3 sunColor;
	int useSkyboxColor;
}

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

struct PS_Output
{
	float4 colorNoAmbient	: SV_TARGET0;
	float4 skyAndOccluders	: SV_TARGET4;
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
PS_Output main(VertexToPixel input)
{
	PS_Output output;

	// Calculate the falloff from the sun direction
	float falloff = saturate(dot(normalize(sunDirection), normalize(input.sampleDir)));
	falloff = pow(falloff, falloffExponent);

	// Colors
	float3 skyColor = skyTexture.Sample(samplerOptions, input.sampleDir).rgb;
	float3 lightColor = lerp(sunColor, skyColor, useSkyboxColor);

	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	output.colorNoAmbient = float4(skyColor,1);
	output.skyAndOccluders = float4(falloff * lightColor, 1);
	return output;
}