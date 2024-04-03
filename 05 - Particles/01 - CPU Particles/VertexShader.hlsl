
#include "ShaderStructs.hlsli"


cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix worldInvTrans;
	matrix view;
	matrix projection;

	matrix shadowView;
	matrix shadowProjection;
}


// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// Calculate screen position of this vertex
	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	// Pass other data through
	output.uv = input.uv;
	output.normal = normalize(mul((float3x3)worldInvTrans, input.normal));
	output.tangent = normalize(mul((float3x3)worldInvTrans, input.tangent));
	output.worldPos = mul(world, float4(input.localPosition, 1.0f)).xyz;

	// Calculate where this vertex is from the light's point of view
	matrix shadowWVP = mul(shadowProjection, mul(shadowView, world));
	output.posForShadow = mul(shadowWVP, float4(input.localPosition, 1.0f));

	return output;
}