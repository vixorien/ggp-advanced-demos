
// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// Matrices for projection in pixel shader
	matrix view;
	matrix projection;
	matrix prevView;
	matrix prevProjection;

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

matrix RemoveTranslation(matrix m)
{
	m._14 = 0;
	m._24 = 0;
	m._34 = 0;
	return m;
}

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

	// Calculate current and previous projections

	// Multiply the view (without translation) and the projection
	matrix vp = mul(projection, RemoveTranslation(view));
	float4 currentScreenPos = mul(vp, float4(input.sampleDir, 1.0f));

	// Calculate prev frame's position
	matrix prevVP = mul(prevProjection, RemoveTranslation(prevView));
	float4 prevScreenPos = mul(prevVP, float4(input.sampleDir, 1.0f));

	// Calculate velocity
	float2 thisPos = currentScreenPos.xy / currentScreenPos.w;
	float2 prevPos = prevScreenPos.xy / prevScreenPos.w;
	float2 velocity = thisPos - prevPos;
	velocity.y *= -1;

	// Scale to pixels and clamp to max
	velocity *= ScreenSize / 2;
	float magnitude = length(velocity);
	if (magnitude > MotionBlurMax)
	{
		velocity = normalize(velocity) * MotionBlurMax;
	}
	else if (magnitude < 0.0001f)
	{
		velocity = float2(0, 0);
	}

	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	output.color = skyTexture.Sample(samplerOptions, input.sampleDir);
	output.velocity = velocity;

	return output;
}