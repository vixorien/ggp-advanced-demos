
cbuffer perFrame : register(b0)
{
	matrix View;
	matrix Projection;
	float3 CameraPosition;
}

cbuffer perLight : register(b1)
{
	matrix World;
}

// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

// Defines the output data of our vertex shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float3 viewRay		: TEXCOORD0;
};

// The entry point for our vertex shader
VertexToPixel main(VertexShaderInput input)
{
	// Set up output
	VertexToPixel output;

	// Calculate output position
	matrix wvp = mul(Projection, mul(View, World));
	output.position = mul(wvp, float4(input.position, 1.0f));

	// Calculate the view ray from the camera through this vertex
	// which we need to reconstruct world position from depth in pixel shader
	output.viewRay = mul(World, float4(input.position, 1.0f)).xyz - CameraPosition;

	return output;
}