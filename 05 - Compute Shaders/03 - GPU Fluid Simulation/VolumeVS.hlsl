
// Data that changes at most once per frame
cbuffer externalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
};


// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

// Out of the vertex shader (and eventually input to the PS)
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float3 uvw				: TEXCOORD;
	float3 worldPos			: POSITION;
};

VertexToPixel main(VertexShaderInput input)
{
	// Set up output
	VertexToPixel output;

	// Calculate output position
	matrix worldViewProj = mul(projection, mul(view, world));
	output.screenPosition = mul(worldViewProj, float4(input.position, 1.0f));

	// Calculate the world position of this vertex (to be used
	// in the pixel shader when we do point/spot lights)
	output.worldPos = mul(world, float4(input.position, 1.0f)).xyz;

	// Calc uvw from position using the sign (+/-/0) of each component
	// and clamping to 0-1.  Then flip the Y to match texture coords.
	output.uvw = saturate(sign(input.position));
	output.uvw.y = 1.0f - output.uvw.y;

	return output;
}