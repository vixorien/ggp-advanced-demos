
// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// Motion blur data
	int MotionBlurMax;
	float2 ScreenSize;

	float pad;
};

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
	float4 screenPosCurrent : SCREEN_POS0;
	float4 screenPosPrev	: SCREEN_POS1;
};

struct PS_Output
{
	float4 color		: SV_TARGET0;
	float2 velocity		: SV_TARGET1;
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

	// Calculate velocity
	float2 thisPos = input.screenPosCurrent.xy / input.screenPosCurrent.w;
	float2 prevPos = input.screenPosPrev.xy / input.screenPosPrev.w;
	float2 velocity = thisPos - prevPos;
	velocity.y *= -1;

	// Scale to pixels and clamp to max
	velocity *= ScreenSize;
	float magnitude = length(velocity);
	if (magnitude > MotionBlurMax)
	{
		velocity = normalize(velocity) * MotionBlurMax;
	}

	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	output.color = skyTexture.Sample(samplerOptions, input.sampleDir);
	output.velocity = velocity;

	return output;
}