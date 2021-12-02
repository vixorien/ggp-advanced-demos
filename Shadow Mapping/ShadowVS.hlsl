
// Constant Buffer for external (C++) data
cbuffer perFrame : register(b0)
{
	matrix view;
	matrix projection;
}

cbuffer perObject : register(b1)
{
	matrix world;
};

struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

// VStoPS struct for shadow map creation
struct VertexToPixel_Shadow
{
	float4 screenPosition	: SV_POSITION;
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel_Shadow main(VertexShaderInput input)
{
	// Set up output
	VertexToPixel_Shadow output;

	// Calculate output position
	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	return output;
}