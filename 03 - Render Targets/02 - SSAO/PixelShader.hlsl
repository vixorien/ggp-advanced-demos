
#include "Lighting.hlsli"

// How many lights could we handle?
#define MAX_LIGHTS 128

// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// An array of light data
	Light lights[MAX_LIGHTS];

	// The amount of lights THIS FRAME
	int lightCount;

	// Needed for specular (reflection) calculation
	float3 cameraPosition;
};

// Data that can change per material
cbuffer perMaterial : register(b1)
{
	// Surface color
	float3 colorTint;

	// UV adjustments
	float2 uvScale;
	float2 uvOffset;
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

// MRT Setup
struct PS_Output
{
	float4 colorDirect		: SV_TARGET0;
	float4 colorIndirect	: SV_TARGET1;
	float4 normals			: SV_TARGET2;
	float depths : SV_TARGET3;
};

// Texture-related variables
Texture2D Albedo			: register(t0);
Texture2D NormalMap			: register(t1);
Texture2D RoughnessMap		: register(t2);
SamplerState BasicSampler		: register(s0);


// Entry point for this pixel shader
PS_Output main(VertexToPixel input)
{
	// Always re-normalize interpolated direction vectors
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Apply the uv adjustments
	input.uv = input.uv * uvScale + uvOffset;

	// Normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);
	
	// Treating roughness as a pseduo-spec map here
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float specPower = max(256.0f * (1.0f - roughness), 0.01f); // Ensure we never hit 0
	
	// Gamma correct the texture back to linear space and apply the color tint
	float4 surfaceColor = Albedo.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2) * colorTint;

	// Total color for this pixel
	float3 totalColor = float3(0,0,0);

	// Loop through all lights this frame
	for(int i = 0; i < lightCount; i++)
	{
		// Which kind of light?
		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalColor += DirLight(lights[i], input.normal, input.worldPos, cameraPosition, specPower, surfaceColor.rgb);
			break;

		case LIGHT_TYPE_POINT:
			totalColor += PointLight(lights[i], input.normal, input.worldPos, cameraPosition, specPower, surfaceColor.rgb);
			break;

		case LIGHT_TYPE_SPOT:
			totalColor += SpotLight(lights[i], input.normal, input.worldPos, cameraPosition, specPower, surfaceColor.rgb);
			break;
		}
	}

	// Multiple render target output
	PS_Output output;
	output.colorDirect		= float4(totalColor, 1); // No gamma correction yet!
	output.colorIndirect	= float4(0, 0, 0, 1); // No ambient at the moment
	output.normals			= float4(input.normal * 0.5f + 0.5f, 1);
	output.depths			= input.screenPosition.z;
	return output;
}