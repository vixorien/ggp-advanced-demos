
#include "Lighting.hlsli"

// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// Nothing necessary for GBuffer creation!
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float4 Color;
};


// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION; // The world position of this PIXEL
};

struct GBuffer
{
	float4 albedo			: SV_TARGET0;
	float4 normals			: SV_TARGET2;
	float  depth			: SV_TARGET3;
	float4 metalRough		: SV_TARGET4;
};

// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
Texture2D MetalTexture			: register(t3);

// Samplers
SamplerState BasicSampler		: register(s0);

// Entry point for this pixel shader
GBuffer main(VertexToPixel input)
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Sample various textures
	input.normal = NormalMapping(NormalTexture, BasicSampler, input.uv, input.normal, input.tangent);
	float roughness = RoughnessTexture.Sample(BasicSampler, input.uv).r;
	float metal = MetalTexture.Sample(BasicSampler, input.uv).r;

	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * Color.rgb;
	
	// Multiple render target output
	GBuffer gbuffer;
	gbuffer.albedo	= surfaceColor;
	gbuffer.normals		= float4(input.normal * 0.5f + 0.5f, 1);
	gbuffer.depth		= input.screenPosition.z;
	gbuffer.metalRough	= float4(metal, roughness, 0, 1);
	return gbuffer;

}