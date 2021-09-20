
#include "Lighting.hlsli"

// How many lights could we handle?
#define MAX_LIGHTS 128

// Data that can change per material
cbuffer perMaterial : register(b0)
{
	// Surface color
	float4 Color;
};

// Data that only changes once per frame
cbuffer perFrame : register(b1)
{
	// An array of light data
	Light Lights[MAX_LIGHTS];

	// The amount of lights THIS FRAME
	int LightCount;

	// Needed for specular (reflection) calculation
	float3 CameraPosition;
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


// Texture-related variables
Texture2D AlbedoTexture			: register(t0);
Texture2D NormalTexture			: register(t1);
Texture2D RoughnessTexture		: register(t2);
Texture2D MetalTexture			: register(t3);
SamplerState BasicSampler		: register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
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

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Total color for this pixel
	float3 totalColor = float3(0,0,0);

	// Loop through all lights this frame
	for(int i = 0; i < LightCount; i++)
	{
		// Which kind of light?
		switch (Lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalColor += DirLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_POINT:
			totalColor += PointLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_SPOT:
			totalColor += SpotLightPBR(Lights[i], input.normal, input.worldPos, CameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;
		}
	}

	// Gamma correction
	return float4(pow(totalColor, 1.0f / 2.2f), 1);
}